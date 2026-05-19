#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "Events/Event.h"
#include "Events/WindowEvents.h"
#include "LayerStack.h"
#include "Log.h"
#include "Window.h"

#include "Gui/ImGuiLayer.h"
#include "Renderer/Texture.h"
#include "Scene.h"
#include "Utils/InputManager.h"

namespace Hamster {
    class AssetManager;
    class Renderer;

    class Application {
    public:
        Application();
        explicit Application(const WindowProps &props);

        ~Application();

        void Run();

        void Close(WindowCloseEvent &e);

        // glm::mat4 GetProjectionMatrix();

        std::shared_ptr<EventDispatcher> GetEventDispatcher() { return m_Dispatcher; }

        static Application &GetApplicationInstance() { return *s_Instance; }

        static void ResizeWindow(WindowResizeEvent &e);

        void PauseSimulation();

        void ResumeSimulation();

        bool IsSimulationPaused();

        GLFWwindow *GetWindow() { return m_Window->GetGLFWWindowPointer(); }

        // Popout play-window lifecycle. Stage 4 of the
        // project-resolution-and-play-window feature. OpenPlayWindow creates
        // a non-resizable GLFW window that shares the editor's GL context;
        // ClosePlayWindow destroys it. Both are idempotent. Stages 5–7 layer
        // rendering, input routing, and deferred-destroy on top of this.
        void OpenPlayWindow(int width, int height, const std::string &title);
        void ClosePlayWindow();
        GLFWwindow *GetPlayWindow() const { return m_PlayWindow; }
        bool IsPlayWindowOpen() const { return m_PlayWindow != nullptr; }

        void PushLayer(Layer *layer);

        void PopLayer(Layer *layer);

        // [[nodiscard]] int GetViewportHeight() const { return m_ViewportHeight; }
        // [[nodiscard]] int GetViewportWidth() const { return m_ViewportWidth; }
        //
        // void SetViewportHeight(const int height) { m_ViewportHeight = height; }
        // void SetViewportWidth(const int width) { m_ViewportWidth = width; }

        static glm::vec3 IdToColour(int id);

        static int ColourToId(glm::vec3 colour);

        void AddScene(std::shared_ptr<Scene> scene);

        void RemoveScene(UUID uuid);

        void RemoveAllScenes();

        void StopActiveScene();

        void SetSceneActive(UUID uuid);

        std::shared_ptr<Scene> GetScene(UUID uuid);

        std::shared_ptr<Scene> GetActiveScene();

        void AppendToMainThreadQueue(const std::function<void()> &func);

        void ExecuteMainThread();

        AssetManager *GetAssetManager() { return m_AssetManager.get(); }

        Renderer *GetRenderer() { return m_Renderer.get(); }

        static std::string GetExecutablePath();

    private:
        static Application *s_Instance;

        bool m_Running = false;
        bool m_IsSimulationPaused = true;

        // int m_ViewportWidth = 1920;
        // int m_ViewportHeight = 1080;

        // glm::mat4 m_Projection;

        std::shared_ptr<EventDispatcher> m_Dispatcher;
        LayerStack m_LayerStack;

        ImGuiLayer *m_ImGuiLayer = nullptr;

        std::unique_ptr<Window> m_Window;

        // Popout play window — null when not open. Owns its own GLFW handle
        // (shares the editor's GL context). Destroyed via ClosePlayWindow.
        GLFWwindow *m_PlayWindow = nullptr;

        std::unique_ptr<InputManager> m_InputManager;
        std::unique_ptr<AssetManager> m_AssetManager;
        std::unique_ptr<Renderer> m_Renderer;

        std::unordered_map<UUID, std::shared_ptr<Scene> > m_Scenes;
        std::shared_ptr<Scene> m_ActiveScene = nullptr;

        WindowProps m_WindowProps;

        std::vector<std::function<void()> > m_MainThreadQueue;
        std::mutex m_MainThreadMutex;

        std::vector<Layer *> m_LayersPendingPush;
        std::vector<Layer *> m_LayersPendingPop;
    };
} // namespace Hamster
