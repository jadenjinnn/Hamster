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

namespace Hamster { class AssetManager; }

class PropertyEditor : public Hamster::Panel {
public:
  PropertyEditor(Hamster::EventDispatcher *dispatcher,
                 std::shared_ptr<Hamster::Scene> scene,
                 Hamster::AssetManager *assetManager)
      : Hamster::Panel(dispatcher, scene, true),
        m_AssetManager(assetManager) {};

  void Render() override;

  void SetSelectedEntity(Hamster::UUID uuid);

private:
  void OpenFile(std::filesystem::path path);

  Hamster::UUID m_SelectedEntity = Hamster::UUID::GetNil();
  Hamster::Name *m_Name = nullptr;
  Hamster::Transform *m_Transform = nullptr;
  Hamster::Sprite *m_Sprite = nullptr;
  Hamster::Behaviour *m_Behaviour = nullptr;
  Hamster::Rigidbody *m_Rigidbody = nullptr;

  Hamster::AssetManager *m_AssetManager;

  std::shared_ptr<RenameModal> m_RenameModal;
  bool m_RenameModalOpen = false;
};

#endif // PROPERTYEDITOR_H
