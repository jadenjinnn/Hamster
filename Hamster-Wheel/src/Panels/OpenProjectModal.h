#ifndef OPEN_PROJECT_MODAL_H
#define OPEN_PROJECT_MODAL_H

#include <Hamster.h>

class ProjectRegistry;

// In-editor counterpart to the hub's project picker: a modal that lists
// projects from the registry (recent-first), filterable by a search bar,
// for use from EditorLayer's File → Open Project menu.
class OpenProjectModal {
public:
  explicit OpenProjectModal(Hamster::Application *app);

  void Show() { m_OpenRequested = true; }
  void Render(ProjectRegistry *registry);

private:
  Hamster::Application *m_App;
  bool m_OpenRequested = false;
  bool m_IsOpen = false;
  char m_SearchBuffer[128] = {0};
};

#endif // OPEN_PROJECT_MODAL_H
