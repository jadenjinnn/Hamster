//
// Created by Jaden on 25/08/2024.
//

#ifndef HIERARCHY_H
#define HIERARCHY_H

#include "RenameModal.h"

#include <entt/entt.hpp>

#include <Gui/Panel.h>
#include <Hamster.h>

namespace Hamster { class Renderer; }

class Hierarchy : public Hamster::Panel {
public:
  Hierarchy(Hamster::EventDispatcher *dispatcher,
            std::shared_ptr<Hamster::Scene> scene,
            Hamster::Renderer *renderer)
      : Hamster::Panel(dispatcher, scene, true),
        m_Renderer(renderer) {};

  void SetSelectedEntity(entt::entity entity);

  entt::entity GetSelectedEntity() const;

  void Render() override;

private:
  Hamster::Renderer *m_Renderer;
  entt::entity m_SelectedEntity = entt::null;
  std::shared_ptr<RenameModal> m_RenameModal;

  bool m_RenameModalOpen = false;
  char m_SearchBuffer[128] = {};
};

#endif // HIERARCHY_H
