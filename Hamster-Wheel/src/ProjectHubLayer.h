#ifndef PROJECTHUBLAYER_H
#define PROJECTHUBLAYER_H

#include <Hamster.h>
#include <filesystem>

#include "ProjectRegistry.h"

class ProjectHubLayer : public Hamster::Layer {
public:
  explicit ProjectHubLayer(Hamster::Application *app);

  void OnImGuiUpdate() override;

private:
  void RenderTopBar(float width);
  void RenderHeader(float width);
  void RenderCardGrid(float width, float startY, float height);
  void RenderCreateModal();
  void RenderRenameModal();
  void RenderDeleteConfirmation();
  void RenderMissingProjectDialog();
  void OpenProjectDialog();
  void OpenProject(const std::filesystem::path &path);

  std::string FormatTimestamp(const std::string &iso) const;

  Hamster::Application *m_App;
  Hamster::EventDispatcher *m_Dispatcher;
  ProjectRegistry m_Registry;

  bool m_ShowCreateModal = false;
  char m_ProjectName[128] = "Untitled";
  char m_SearchBuffer[128] = {};
  std::filesystem::path m_ProjectDirectory;
  int m_SelectedTemplate = 0;
  bool m_NoDirectorySelected = false;
  bool m_DirectoryExists = false;

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
