#ifndef CREATE_PROJECT_MODAL_H
#define CREATE_PROJECT_MODAL_H

#include <Hamster.h>
#include <filesystem>

class ProjectRegistry;

// Shared modal used by both ProjectHubLayer and EditorLayer's File menu.
// Owns its own input state and error flags. Call Show() to request open;
// Render() each frame.
class CreateProjectModal {
public:
  explicit CreateProjectModal(Hamster::Application *app);

  void Show() { m_OpenRequested = true; }
  void Render(ProjectRegistry *registry);

private:
  Hamster::Application *m_App;

  bool m_OpenRequested = false;
  bool m_IsOpen = false;

  char m_ProjectName[128] = "Untitled";
  std::filesystem::path m_ProjectDirectory;
  int m_SelectedTemplate = 0;
  bool m_NoDirectorySelected = false;
  bool m_DirectoryExists = false;
};

#endif // CREATE_PROJECT_MODAL_H
