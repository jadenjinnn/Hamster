//
// Created by Jaden on 31/08/2024.
//

#ifndef PROJECT_H
#define PROJECT_H

#include <memory>

#include "Scene.h"

namespace Hamster {
class Application;
class AssetManager;
class Scene;
class ProjectWatcher;

struct ProjectConfig {
  std::string Name;
  std::filesystem::path ProjectDirectory;
  std::filesystem::path StartScenePath;
  // Target play-window resolution. Legacy projects without these fields in
  // their .hamproj fall back to 1280x720 on deserialise (see ProjectSerialiser).
  int32_t TargetWidth = 1280;
  int32_t TargetHeight = 720;
};

class Project {
public:
  Project(const ProjectConfig &config);
  ~Project();

  static bool New(ProjectConfig &config, Application *app);

  static bool Open(std::filesystem::path projectPath, Application *app);

  static void SaveCurrentProject(AssetManager *assetManager);

  void SetStartScene(std::shared_ptr<Scene> scene);

  // Starts the file-system watcher on the project directory. Must be called
  // after Open / New has finished populating the AssetManager. The watcher
  // dispatches add/remove/rename events into AssetManager::HandleFileEvents.
  void StartWatcher(Application *app);

  static std::shared_ptr<Project> GetCurrentProject() {
    return s_ActiveProject;
  }

  [[nodiscard]] ProjectConfig &GetConfig();

private:
  ProjectConfig m_Config;

  inline static std::shared_ptr<Project> s_ActiveProject = nullptr;

  std::shared_ptr<Scene> m_StartScene = nullptr;

  // Lifetime: born in StartWatcher, dies in ~Project. Member ordering is
  // intentional — the watcher's worker thread accesses AssetManager via the
  // callback, but Project doesn't own AssetManager (Application does), so
  // ordering inside Project doesn't matter; what matters is that the
  // watcher is destroyed before the AssetManager, which happens because
  // ~Application destroys layers (and thus ~Project) before ~AssetManager.
  std::unique_ptr<ProjectWatcher> m_Watcher;
};
} // namespace Hamster

#endif // PROJECT_H
