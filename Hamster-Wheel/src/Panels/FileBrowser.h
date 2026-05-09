//
// Created by Jaden on 02/09/2024.
//

#ifndef FILEBROWSER_H
#define FILEBROWSER_H
#include <Gui/Panel.h>

#include <memory>

#include <Renderer/Texture.h>
class FileBrowser : public Hamster::Panel {
public:
  explicit FileBrowser(Hamster::EventDispatcher *dispatcher,
                       std::shared_ptr<Hamster::Scene> scene);

  void Render() override;

private:
  std::unique_ptr<Hamster::Texture> m_FolderIcon;
  std::unique_ptr<Hamster::Texture> m_FileIcon;

  float m_IconSize = 64.0f;
  float m_Padding = 16.0f;
};

#endif // FILEBROWSER_H
