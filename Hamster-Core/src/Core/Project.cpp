//
// Created by Jaden on 31/08/2024.
//

#include "HamsterPCH.h"

#include "Project.h"

#include "Application.h"
#include "ProjectSerialiser.h"
#include "SceneSerialiser.h"
#include "Utils/AssetManager.h"

namespace Hamster {
    Project::Project(const ProjectConfig &config) : m_Config(config) {
        std::filesystem::create_directory(config.ProjectDirectory);

        std::filesystem::current_path(config.ProjectDirectory);
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


        return true;
    }

    bool Project::Open(std::filesystem::path projectPath, Application *app) {
        auto *assetManager = app->GetAssetManager();

        if (s_ActiveProject != nullptr) {
            Project::SaveCurrentProject(assetManager);
        }

        app->StopActiveScene();

        app->RemoveAllScenes();

        std::ifstream projectFile(projectPath, std::ios::binary);

        ProjectConfig config = ProjectSerialiser::Deserialise(projectFile);

        s_ActiveProject = std::make_shared<Project>(config);

        assetManager->Deserialise(projectFile, config);

        projectFile.close();

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
