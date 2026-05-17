//
// Created by Jaden on 31/08/2024.
//

#include "HamsterPCH.h"

#include "Project.h"

#include "Application.h"
#include "Components.h"
#include "ProjectSerialiser.h"
#include "Scene.h"
#include "SceneSerialiser.h"
#include "Utils/AssetManager.h"
#include "Utils/ProjectWatcher.h"

namespace Hamster {
    Project::Project(const ProjectConfig &config) : m_Config(config) {
        std::filesystem::create_directory(config.ProjectDirectory);

        std::filesystem::current_path(config.ProjectDirectory);
    }

    Project::~Project() = default;

    void Project::StartWatcher(Application *app) {
        AssetManager *assetManager = app->GetAssetManager();

        auto enqueue = [app](std::function<void()> fn) {
            app->AppendToMainThreadQueue(fn);
        };

        auto onEvents = [assetManager, app](std::vector<FileEvent> events) {
            assetManager->HandleFileEvents(events);
            // External delete / rename can drop a script from the AssetManager
            // while Behaviour components still hold a live shared_ptr to it.
            // The missing-script UI and the simulation guard both key off
            // a null shared_ptr, so null any entry whose UUID is no longer
            // registered. This also catches renames where the .meta did NOT
            // move with the file (a fresh UUID surfaces, old goes missing).
            if (auto scene = app->GetActiveScene()) {
                auto view = scene->GetRegistry().view<Behaviour>();
                view.each([assetManager](auto &beh) {
                    for (auto &[uuid, script] : beh.scripts) {
                        if (script && !assetManager->GetScript(uuid)) {
                            script.reset();
                        }
                    }
                });
            }
        };

        m_Watcher = std::make_unique<ProjectWatcher>(
            m_Config.ProjectDirectory, std::move(enqueue), std::move(onEvents));
    }

    bool Project::New(ProjectConfig &config, Application *app) {
        if (std::filesystem::exists(config.ProjectDirectory) &&
            std::filesystem::is_directory(config.ProjectDirectory)) {
            std::runtime_error("Project directory already exists");

            return false;
        }

        auto *assetManager = app->GetAssetManager();

        if (s_ActiveProject != nullptr) {
            Hamster::Project::SaveCurrentProject(assetManager);
        }

        app->StopActiveScene();

        app->RemoveAllScenes();

        // Drop any previously-loaded project's assets so the new project does
        // not inherit them. See bug 0004.
        assetManager->Clear();

        std::filesystem::create_directory(config.ProjectDirectory);
        std::filesystem::current_path(config.ProjectDirectory);

        std::filesystem::create_directory("Scenes");

        auto scene = std::make_shared<Scene>(app->GetEventDispatcher().get(), app);

        app->AddScene(scene);

        std::cout << scene->GetUUID().GetUUID() << std::endl;

        SceneSerialiser sceneSerialiser(scene, assetManager);

        std::filesystem::path scenePath = config.ProjectDirectory / scene->GetPath();

        std::cout << scenePath << std::endl;

        std::ofstream sceneOut(scenePath, std::ios::binary);
        sceneSerialiser.Serialise(sceneOut);
        sceneOut.close();

        config.StartScenePath = scenePath;

        app->SetSceneActive(scene->GetUUID());

        s_ActiveProject = std::make_shared<Project>(config);

        std::filesystem::path spriteFolderPath =
                Hamster::Application::GetExecutablePath() +
                "/../share/Resources/Hamster-Wheel/Resources/Sprites";

        std::shared_ptr<Hamster::Texture> square = assetManager->AddTexture(
            spriteFolderPath.string() + "/square.png");

        square->SetName("Square");

        std::shared_ptr<Hamster::Texture> triangle = assetManager->AddTexture(
            spriteFolderPath.string() + "/triangle.png");

        triangle->SetName("Triangle");

        std::shared_ptr<Hamster::Texture> circle = assetManager->AddTexture(
            spriteFolderPath.string() + "/circle.png");

        circle->SetName("Circle");

        SaveCurrentProject(assetManager);

        app->AddScene(scene);
        app->SetSceneActive(scene->GetUUID());

        ProjectOpenedEvent e(config.ProjectDirectory);

        app->GetEventDispatcher()
                ->Post<ProjectOpenedEvent>(e);

        s_ActiveProject->StartWatcher(app);

        return true;
    }

    bool Project::Open(std::filesystem::path projectPath, Application *app) {
        auto *assetManager = app->GetAssetManager();

        if (s_ActiveProject != nullptr) {
            Project::SaveCurrentProject(assetManager);
        }

        app->StopActiveScene();

        app->RemoveAllScenes();

        // Drop any previously-loaded project's assets so the new project does
        // not inherit them. See bug 0004.
        assetManager->Clear();

        std::ifstream projectFile(projectPath, std::ios::binary);

        ProjectConfig config = ProjectSerialiser::Deserialise(projectFile);

        s_ActiveProject = std::make_shared<Project>(config);

        assetManager->Deserialise(projectFile, config);

        projectFile.close();

        // Script identity now lives in .py.meta sidecars next to each script.
        // Animations are loaded from <projectDir>/Animations as self-
        // identifying .hanim files. Both happen after the blob is read
        // (which fills in textures) and before scene deserialisation (which
        // resolves asset UUIDs referenced by entity components).
        assetManager->LoadProjectScripts(config.ProjectDirectory);
        assetManager->LoadProjectAnimations(config.ProjectDirectory);

        auto scene = std::make_shared<Scene>(app->GetEventDispatcher().get(), app);
        SceneSerialiser sceneSerialiser(scene, assetManager);
        std::ifstream sceneIn(config.StartScenePath, std::ios::binary);
        sceneSerialiser.Deserialise(sceneIn);
        sceneIn.close();

        s_ActiveProject->SetStartScene(scene);

        app->AddScene(scene);

        app->SetSceneActive(scene->GetUUID());

        ProjectOpenedEvent e(projectPath);

        app->GetEventDispatcher()
                ->Post<ProjectOpenedEvent>(e);

        std::filesystem::current_path(config.ProjectDirectory);

        s_ActiveProject->StartWatcher(app);

        return true;
    }

    void Project::SaveCurrentProject(AssetManager *assetManager) {
        if (s_ActiveProject != nullptr) {
            ProjectSerialiser serialiser(s_ActiveProject);

            std::cout << s_ActiveProject->GetConfig().Name << std::endl;

            std::ofstream out(s_ActiveProject->GetConfig().Name + ".hamproj",
                              std::ios::binary);

            serialiser.Serialise(out);

            assetManager->Serialise(out);

            out.close();
        }
    }

    void Project::SetStartScene(std::shared_ptr<Scene> scene) {
        m_StartScene = std::move(scene);
    }

    ProjectConfig &Project::GetConfig() { return m_Config; }
} // namespace Hamster
