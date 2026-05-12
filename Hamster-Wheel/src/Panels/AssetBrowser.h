//
// Created by Jaden on 03/09/2024.
//

#ifndef ASSETBROWSER_H
#define ASSETBROWSER_H
#include <Gui/Panel.h>

#include <Core/UUID.h>
#include <Renderer/Texture.h>
#include <memory>

namespace Hamster { class AssetManager; }

class AssetBrowser : public Hamster::Panel {
public:
  AssetBrowser(Hamster::EventDispatcher *dispatcher,
               std::shared_ptr<Hamster::Scene> scene,
               Hamster::AssetManager *assetManager);

  void Render() override;

  void StartRename(Hamster::UUID uuid);

private:
  Hamster::AssetManager *m_AssetManager;
  std::unique_ptr<Hamster::Texture> m_PythonIcon;
  std::unique_ptr<Hamster::Texture> m_FolderIcon;
  std::unique_ptr<Hamster::Texture> m_FileIcon;

  float m_CardSize = 96.0f;
  float m_CardPadding = 12.0f;

  Hamster::UUID m_RenamingUUID = Hamster::UUID::GetNil();
  char m_RenameBuffer[128] = {};
  bool m_RenameFocusPending = false;

  Hamster::UUID m_ContextMenuUUID = Hamster::UUID::GetNil();
};

#endif // ASSETBROWSER_H
