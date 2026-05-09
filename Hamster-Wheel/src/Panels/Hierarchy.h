//
// Created by Jaden on 25/08/2024.
//

#ifndef HIERARCHY_H
#define HIERARCHY_H

#include "RenameModal.h"

#include <entt/entt.hpp>

#include <Gui/Panel.h>
#include <Hamster.h>

class Hierarchy : public Hamster::Panel {
public:
  Hierarchy(Hamster::EventDispatcher *dispatcher,
            std::shared_ptr<Hamster::Scene> scene)
      : Hamster::Panel(dispatcher, scene, true) {};

  void SetSelectedEntity(entt::entity entity);

  entt::entity GetSelectedEntity() const;

  void Render() override;

private:
  entt::entity m_SelectedEntity = entt::null;
  std::shared_ptr<RenameModal> m_RenameModal;

  bool m_RenameModalOpen = false;
};

#endif // HIERARCHY_H
