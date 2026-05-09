//
// Created by Jaden on 03/09/2024.
//

#ifndef ASSETBROWSER_H
#define ASSETBROWSER_H
#include <Gui/Panel.h>

#include <Renderer/Texture.h>
#include <memory>

class AssetBrowser : public Hamster::Panel {
public:
  AssetBrowser(Hamster::EventDispatcher *dispatcher,
               std::shared_ptr<Hamster::Scene> scene);

  void Render() override;

private:
  std::unique_ptr<Hamster::Texture> m_PythonIcon;
};

#endif // ASSETBROWSER_H
