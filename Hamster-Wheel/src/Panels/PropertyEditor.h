//
// Created by Jaden on 24/08/2024.
//

#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H
#include "RenameModal.h"

#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <ostream>

#include <Gui/Panel.h>
#include <Hamster.h>
#include <Renderer/Texture.h>

class AssetBrowser;
class ColliderEditor;
namespace Hamster { class AssetManager; }

class PropertyEditor : public Hamster::Panel {
public:
  PropertyEditor(Hamster::EventDispatcher *dispatcher,
                 std::shared_ptr<Hamster::Scene> scene,
                 Hamster::AssetManager *assetManager,
                 ColliderEditor *colliderEditor)
      : Hamster::Panel(dispatcher, scene, true),
        m_AssetManager(assetManager),
        m_ColliderEditor(colliderEditor) {
    m_EntityIcon = std::make_unique<Hamster::Texture>(
        Hamster::Application::GetExecutablePath() +
        "/../share/Resources/Hamster-Wheel/Resources/Icons/hamster.png");
  };

  void Render() override;

  void SetSelectedEntity(Hamster::UUID uuid);
  void SetAssetBrowser(AssetBrowser *browser) { m_AssetBrowser = browser; }

private:
  void OpenFile(std::filesystem::path path);

  Hamster::UUID m_SelectedEntity = Hamster::UUID::GetNil();
  Hamster::Name *m_Name = nullptr;
  Hamster::Transform *m_Transform = nullptr;
  Hamster::Sprite *m_Sprite = nullptr;
  Hamster::Behaviour *m_Behaviour = nullptr;
  Hamster::Rigidbody *m_Rigidbody = nullptr;

  Hamster::AssetManager *m_AssetManager;
  AssetBrowser *m_AssetBrowser = nullptr;
  ColliderEditor *m_ColliderEditor = nullptr;
  std::unique_ptr<Hamster::Texture> m_EntityIcon;

  std::shared_ptr<RenameModal> m_RenameModal;
  bool m_RenameModalOpen = false;
};

#endif // PROPERTYEDITOR_H
