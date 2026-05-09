//
// Created by Jaden on 31/08/2024.
//

#ifndef PROJECT_H
#define PROJECT_H

#include "Scene.h"

namespace Hamster {
class Application;
class AssetManager;
class Scene;

struct ProjectConfig {
  std::string Name;
  std::filesystem::path ProjectDirectory;
  std::filesystem::path StartScenePath;
};

class Project {
public:
  Project(const ProjectConfig &config);

  static bool New(ProjectConfig &config, Application *app);

  static bool Open(std::filesystem::path projectPath, Application *app);

  static void SaveCurrentProject(AssetManager *assetManager);

  void SetStartScene(std::shared_ptr<Scene> scene);

  static std::shared_ptr<Project> GetCurrentProject() {
    return s_ActiveProject;
  }

  [[nodiscard]] ProjectConfig &GetConfig();

private:
  ProjectConfig m_Config;

  inline static std::shared_ptr<Project> s_ActiveProject = nullptr;

  std::shared_ptr<Scene> m_StartScene = nullptr;
};
} // namespace Hamster

#endif // PROJECT_H
