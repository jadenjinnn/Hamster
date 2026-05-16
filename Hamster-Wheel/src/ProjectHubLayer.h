#ifndef PROJECTHUBLAYER_H
#define PROJECTHUBLAYER_H

#include <Hamster.h>
#include <filesystem>
#include <memory>

#include "ProjectRegistry.h"
#include "Panels/CreateProjectModal.h"

class ProjectHubLayer : public Hamster::Layer {
public:
  explicit ProjectHubLayer(Hamster::Application *app);

  void OnImGuiUpdate() override;

private:
  void RenderTopBar(float width);
  void RenderHeader(float width);
  void RenderCardGrid(float width, float startY, float height);
  void RenderRenameModal();
  void RenderDeleteConfirmation();
  void RenderMissingProjectDialog();
  void OpenProjectDialog();
  void OpenProject(const std::filesystem::path &path);

  std::string FormatTimestamp(const std::string &iso) const;

  Hamster::Application *m_App;
  Hamster::EventDispatcher *m_Dispatcher;
  ProjectRegistry m_Registry;
  std::unique_ptr<CreateProjectModal> m_CreateModal;

  char m_SearchBuffer[128] = {};

  int m_ContextMenuIndex = -1;

  bool m_ShowRenameModal = false;
  int m_RenameIndex = -1;
  char m_RenameBuffer[128] = {};
  bool m_RenameError = false;

  bool m_ShowDeleteConfirm = false;
  int m_DeleteIndex = -1;

  bool m_ShowMissingDialog = false;
  int m_MissingIndex = -1;
};

#endif
