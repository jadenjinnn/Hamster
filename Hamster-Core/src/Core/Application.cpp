#include "HamsterPCH.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <windows.h>

#include "Application.h"

#include "Base.h"
#include "Events/ApplicationEvents.h"
#include "Events/UIEvents.h"
#include "Layer.h"
#include "Project.h"
#include "Renderer/FramebufferTexture.h"
#include "Renderer/Renderer.h"
#include "Renderer/Shader.h"
#include "Scripting/Scripting.h"
#include "Utils/AssetManager.h"
#include "Utils/InputManager.h"

namespace Hamster {
    // Singleton was chosen due to need for all systems to access the Application
    // class + there should only ever be 1 instance of Application even when
    // Hamster-Wheel is running
    Application *Application::s_Instance = nullptr;

    Application::Application() : Application(WindowProps{}) {}

    Application::Application(const WindowProps &props) {
        std::cout << "Application created" << std::endl;

        m_WindowProps = props;

        s_Instance = this;

        Log::Init();

        // Similarly, only 1 window is needed, ImGui can handle "sub-windows"
        m_Window = std::make_unique<Window>(m_WindowProps);

        m_InputManager =
                std::make_unique<InputManager>(m_Window->GetGLFWWindowPointer());

        m_AssetManager = std::make_unique<AssetManager>([this](std::function<void()> fn) {
            AppendToMainThreadQueue(fn);
        });

        m_Renderer = std::make_unique<Renderer>(1920, 1080, m_AssetManager.get());

        m_Dispatcher = std::make_shared<EventDispatcher>();

        m_ImGuiLayer = new ImGuiLayer();
        m_ImGuiLayer->SetWindow(m_Window->GetGLFWWindowPointer());
        PushLayer(m_ImGuiLayer);

        m_Window->SetWindowEventDispatcher(m_Dispatcher.get());

        m_Dispatcher->Subscribe(
            WindowClose,
            FORWARD_CALLBACK_FUNCTION(Application::Close, WindowCloseEvent));

        glfwSetWindowCloseCallback(
            m_Window->GetGLFWWindowPointer(), [](GLFWwindow *windowGLFW) {
                auto *dispatcher = static_cast<EventDispatcher *>(
                    glfwGetWindowUserPointer(windowGLFW));

                WindowCloseEvent e;

                dispatcher->Post<WindowCloseEvent>(e);
            });

        // Use of framebuffer resize to support high DPI displays + Linux framebuffer
        // size and window size do not match unlike on Window, window resize callback
        // commented above incase needed for gui resizing
        m_Dispatcher->Subscribe(FramebufferResize,
                                [this](Event &e) {
                                    m_Renderer->SetViewport(dynamic_cast<FramebufferResizeEvent &>(e));
                                });

        glfwSetFramebufferSizeCallback(
            m_Window->GetGLFWWindowPointer(),
            [](GLFWwindow *windowGLFW, int width, int height) {
                auto *dispatcher = static_cast<EventDispatcher *>(
                    glfwGetWindowUserPointer(windowGLFW));

                FramebufferResizeEvent e(width, height);

                dispatcher->Post<FramebufferResizeEvent>(e);
            });

        Scripting::InitInterpreter(m_Dispatcher.get());

        // glfwMaximizeWindow runs inside Window's constructor — before this
        // dispatcher exists — so its framebuffer-resize event has no
        // subscribers. Re-post the current size now that everything is wired
        // up; otherwise the renderer keeps its placeholder 1920x1080 until
        // the user manually resizes the window.
        {
            int fbW, fbH;
            glfwGetFramebufferSize(m_Window->GetGLFWWindowPointer(), &fbW, &fbH);
            FramebufferResizeEvent e(fbW, fbH);
            m_Dispatcher->Post<FramebufferResizeEvent>(e);
        }

        m_Running = true;
    }

    Application::~Application() {
        // Close popout first — its handle becomes a dangling pointer once
        // glfwTerminate (in ~Window) runs.
        ClosePlayWindow();

        Project::SaveCurrentProject(m_AssetManager.get());

        for (auto const &[uuid, scene]: m_Scenes) {
            std::cout << "Currently saving scene with uuid: " << uuid.GetUUID()
                    << std::endl;
            Scene::SaveScene(scene);
        }

        // LayerStack owns its layers — pop+delete each one (including
        // m_ImGuiLayer, which is in the stack). PopLayer deletes via the
        // Layer* it was given, so the m_ImGuiLayer raw pointer is freed here.
        for (Layer *layer : m_LayersPendingPush) {
            delete layer;
        }
        m_LayersPendingPush.clear();
        m_LayersPendingPop.clear();

        while (m_LayerStack.begin() != m_LayerStack.end()) {
            m_LayerStack.PopLayer(*m_LayerStack.begin());
        }
        m_ImGuiLayer = nullptr;

        // Tear down everything that owns pybind11 handles BEFORE finalizing
        // the interpreter. Otherwise the implicit member-destruction at the
        // end of this destructor would decref Python objects on a dead
        // interpreter (HamsterScript::m_Module, Behaviour::pyObjects), which
        // is UB and crashes in practice once enough modules accumulate.
        m_Scenes.clear();
        // bug 0008: m_ActiveScene is a *second* shared_ptr to the active scene,
        // so clearing m_Scenes alone left it alive — it was then destroyed in
        // implicit member destruction, AFTER FinaliseInterpreter, decref'ing
        // its Behaviour::pyObjects on a dead interpreter (SIGSEGV). Drop it,
        // and close the static Project (which holds the start-scene + the file
        // watcher thread), before finalising Python and freeing the AssetManager.
        m_ActiveScene.reset();
        Project::Close();
        m_AssetManager.reset();

        Scripting::FinaliseInterpreter();

        std::cout << "Application destroyed" << std::endl;
    }

    void Application::Run() {
        std::cout << "Application running" << std::endl;

        // Variables left for deltatime calculations, will move later to scene so can
        // be passed to scripts
        float deltaTime = 0.0f;
        float lastFrame = 0.0f;

        // Only 2 shaders used, one for rendering sprites and one for rendering flat
        // colours used in selection

        // m_Projection =
        // glm::ortho(0.0f, static_cast<float>(m_ViewportWidth),
        // static_cast<float>(m_ViewportHeight), 0.0f, -1.0f, 1.0f);


        // NOTE: TEXTURE DOESNT LOAD IF ADDED DURING RUNNING BUT WORKS AFTER
        // RESTARTING APP

        while (m_Running) {
            // Delta time calculations
            auto currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;

            ExecuteMainThread();

            // Popout-close check at frame top. GLFW sets the should-close
            // flag on the popout when the user clicks its X; treating that
            // identically to Stop here keeps simulation state in lockstep
            // with window state. Must run BEFORE any render path that would
            // try to draw into the popout this frame (mitigates Risk #2 of
            // the project-resolution-and-play-window spec — never destroy
            // a window mid-frame).
            if (m_PlayWindow && glfwWindowShouldClose(m_PlayWindow)) {
                if (m_ActiveScene && !m_ActiveScene->IsSceneSimulationPaused()) {
                    m_ActiveScene->PauseSceneSimulation();
                }
                ClosePlayWindow();
            }

            // Run any deferred scene-state restore from the previous frame
            // (simulation-snapshot feature). Must happen between frames so
            // the registry clear+deserialise doesn't trip iterators that
            // were live when the previous frame called PauseSceneSimulation.
            if (m_ActiveScene) {
                m_ActiveScene->ProcessPendingRestore();
            }

            for (Layer *layer: m_LayersPendingPop) {
                {
                    m_LayerStack.PopLayer(layer);
                }
            }

            for (Layer *layer: m_LayersPendingPush) {
                m_LayerStack.PushLayer(layer);
            }

            m_LayersPendingPop.clear();
            m_LayersPendingPush.clear();

            // Normal layers updated before gui so gui can respond to changes
            for (Layer *layer: m_LayerStack) {
                layer->OnUpdate();
            }

            for (Layer *layer: m_LayersPendingPop) {
                m_LayerStack.PopLayer(layer);
            }

            for (Layer *layer: m_LayersPendingPush) {
                m_LayerStack.PushLayer(layer);
            }

            m_LayersPendingPop.clear();
            m_LayersPendingPush.clear();

            m_ImGuiLayer->Begin();

            for (Layer *layer: m_LayerStack) {
                layer->OnImGuiUpdate();
            }

            m_ImGuiLayer->End();

            if (m_ActiveScene != nullptr) {
                m_ActiveScene->OnUpdate();
            }

            m_Window->Update(m_Running);
        }
    }

    void Application::Close(WindowCloseEvent &e) {
        m_Running = false;

        // Persist scenes NOW — while the interpreter, AssetManager, and scenes
        // are all alive — instead of relying on the destructor (bug 0016).
        // The dtor saves too, but doing it here guarantees the write lands
        // before any teardown can go wrong.
        Project::SaveCurrentProject(m_AssetManager.get());
        for (auto const &[uuid, scene] : m_Scenes) {
            Scene::SaveScene(scene);
        }

        // Do NOT destroy the window here. It must outlive the destructor's
        // layer-pop, because ImGui/GL shutdown needs a live context — calling
        // glfwTerminate here ran that shutdown on a dead context (bug 0008).
    }

    void Application::OpenPlayWindow(int width, int height,
                                     const std::string &title) {
        if (m_PlayWindow) {
            // Idempotent — caller can hammer this without checking.
            return;
        }

        // The editor window's hints (borderless, 4.0 core) persist on the
        // shared context but the popout should use OS chrome and not be
        // resizable. Reset hints to a known baseline before create.
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

        // Share the editor's GL context so textures/shaders/FBOs created in
        // the editor are visible from the popout — no re-upload on Play.
        GLFWwindow *editor = m_Window ? m_Window->GetGLFWWindowPointer() : nullptr;
        m_PlayWindow =
            glfwCreateWindow(width, height, title.c_str(), nullptr, editor);

        // Restore defaults so any future glfwCreateWindow elsewhere isn't
        // surprised by our hints leaking out.
        glfwDefaultWindowHints();

        if (!m_PlayWindow) {
            std::cout << "Application: failed to create popout play window ("
                      << width << "x" << height << ")" << std::endl;
            return;
        }

        // Wire input — same key + mouse callbacks the editor window uses,
        // sharing the engine event dispatcher so popout key/mouse events
        // reach scripts via the existing KeyPressed / MouseButtonClicked
        // dispatch path. Editor and popout each carry the same dispatcher
        // pointer in their GLFW user-pointer.
        glfwSetWindowUserPointer(m_PlayWindow, m_Dispatcher.get());
        InputManager::AttachCallbacks(m_PlayWindow);

        // Override the mouse-button callback with a popout-specific one that
        // hit-tests UI buttons in popout coordinates and posts
        // ButtonClickedEvent on hit. Editor's UI hit-test runs through
        // ImGui::IsMouseClicked + panel-relative coords (EditorLayer); the
        // popout has no ImGui surface so it needs its own path. Uses the
        // Application singleton to reach Scene + Renderer because the
        // window's user-pointer is already taken by the EventDispatcher
        // (for the parent AttachCallbacks key callback).
        glfwSetMouseButtonCallback(m_PlayWindow, [](GLFWwindow *w, int button,
                                                     int action, int /*mods*/) {
            if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

            Application &app = Application::GetApplicationInstance();
            auto scene = app.GetActiveScene();
            Renderer *renderer = app.GetRenderer();
            if (!scene || !renderer) return;

            double mx, my;
            glfwGetCursorPos(w, &mx, &my);
            int winW = 0, winH = 0;
            glfwGetWindowSize(w, &winW, &winH);
            if (winW <= 0 || winH <= 0) return;

            auto uiView =
                scene->GetRegistry().view<Hamster::UIButton, Hamster::ID>();
            for (auto e : uiView) {
                auto &btn = uiView.get<Hamster::UIButton>(e);
                auto &id = uiView.get<Hamster::ID>(e);
                Hamster::UIRect r = renderer->ResolveUIButton(
                    btn, static_cast<float>(winW), static_cast<float>(winH));
                if (r.ContainsPoint(static_cast<float>(mx),
                                    static_cast<float>(my))) {
                    Hamster::ButtonClickedEvent be(id.uuid);
                    app.GetEventDispatcher()
                        ->Post<Hamster::ButtonClickedEvent>(be);
                    break;
                }
            }
        });

        // After creating the new window, GLFW makes its context current as a
        // side effect. Restore the editor's context so subsequent rendering
        // still targets it. Stage 5 will switch deliberately each frame.
        if (editor) glfwMakeContextCurrent(editor);
    }

    void Application::ClosePlayWindow() {
        if (!m_PlayWindow) return;
        glfwDestroyWindow(m_PlayWindow);
        m_PlayWindow = nullptr;

        // Defensive: re-current the editor context in case the destroyed
        // popout was the current one when this was called.
        if (m_Window) {
            glfwMakeContextCurrent(m_Window->GetGLFWWindowPointer());
        }
    }

    void Application::ResizeWindow(WindowResizeEvent &e) {
        // Renderer::SetViewport(e.GetWidth(), e.GetHeight());

        // m_Projection =

        // std::cout << "Window resized" << std::endl;
    }

    void Application::PushLayer(Layer *layer) {
        if (m_Running) { m_LayersPendingPush.push_back(layer); } else {
            m_LayerStack.PushLayer(layer);
        }
    }

    void Application::PopLayer(Layer *layer) {
        if (m_Running) { m_LayersPendingPop.push_back(layer); } else {
            m_LayerStack.PopLayer(layer);
        }
    }

    // glm::mat4 Application::GetProjectionMatrix() { return m_Projection; }

    glm::vec3 Application::IdToColour(int id) {
        // Used to convert an entity handle to a colour, 1 is added to the handle so
        // that 0 is an invalid id; OpenGL returns 0 as the colour for an invalid
        // pixel selection

        id += 1;

        int r = (id & 0x000000FF) >> 0;
        int g = (id & 0x0000FF00) >> 8;
        int b = (id & 0x00FF0000) >> 16;

        return {r / 255.0f, g / 255.0f, b / 255.0f};
    }

    int Application::ColourToId(const glm::vec3 colour) {
        // Converts colour back to an entity handle used in ECS, 1 is subtracted to
        // account for invalid id

        int id = static_cast<int>(colour.r) << 0 | static_cast<int>(colour.g) << 8 |
                 static_cast<int>(colour.b) << 16;

        return id - 1;
    }

    void Application::PauseSimulation() { m_IsSimulationPaused = true; }

    void Application::ResumeSimulation() { m_IsSimulationPaused = false; }

    bool Application::IsSimulationPaused() { return m_IsSimulationPaused; }

    void Application::AddScene(std::shared_ptr<Scene> scene) {
        m_Scenes[scene->GetUUID()] = scene;
    }

    void Application::RemoveScene(UUID uuid) { m_Scenes.erase(uuid); }

    void Application::RemoveAllScenes() {
        // Used for resetting application to load new project

        m_ActiveScene = nullptr;
        m_Scenes.clear();
    }

    void Application::StopActiveScene() {
        // Pauses running of active scene for loading in new scene or serialisation of
        // current scene

        if (m_ActiveScene != nullptr) {
            m_ActiveScene->PauseScene();
            m_ActiveScene->PauseSceneSimulation();
        }
    }

    void Application::SetSceneActive(UUID uuid) {
        // Sets a scene as active, each application can only have 1 active scene, the
        // active scene will be the scene that is rendered on screen

        if (m_ActiveScene != nullptr) {
            m_ActiveScene->PauseScene();
            m_ActiveScene->PauseSceneSimulation();
        }

        std::shared_ptr<Scene> scene = m_Scenes[uuid];

        if (scene) {
            m_ActiveScene = scene;

            ActiveSceneChangedEvent e(m_ActiveScene);

            m_Dispatcher->Post<ActiveSceneChangedEvent>(e);

            m_ActiveScene->RunScene();

            std::cout << scene->GetUUID().GetUUIDString() << std::endl;
        } else {
            std::cout << "Scene not found" << std::endl;

            m_ActiveScene = nullptr;
        }
    }

    std::shared_ptr<Scene> Application::GetScene(UUID uuid) {
        return m_Scenes[uuid];
    }

    std::shared_ptr<Scene> Application::GetActiveScene() { return m_ActiveScene; }

    void Application::AppendToMainThreadQueue(const std::function<void()> &func) {
        std::lock_guard<std::mutex> lock(m_MainThreadMutex);

        m_MainThreadQueue.push_back(func);
    }

    void Application::ExecuteMainThread() {
        std::lock_guard<std::mutex> lock(m_MainThreadMutex);

        for (auto &func: m_MainThreadQueue) {
            func();
        }

        m_MainThreadQueue.clear();
    }

    std::string Application::GetExecutablePath() {
        char path[FILENAME_MAX];
#ifdef _WIN32
        GetModuleFileNameA(nullptr, path, sizeof(path)); // For Windows
#else
  ssize_t count = readlink("/proc/self/exe", path, sizeof(path)); // For Linux
  if (count == -1)
    throw std::runtime_error("Failed to determine executable path");
  path[count] = '\0';
#endif
        return std::filesystem::path(path).parent_path().string();
    }
} // namespace Hamster
