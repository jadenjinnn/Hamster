//
// Created by Jaden on 25/08/2024.
//

#include "Hierarchy.h"

#include "RenameModal.h"

#include <utility>

entt::entity Hierarchy::GetSelectedEntity() const { return m_SelectedEntity; }

void Hierarchy::SetSelectedEntity(const entt::entity entity) {
  m_SelectedEntity = entity;

  if (entity != entt::null) {
    m_Renderer->DrawGuizmo(
        m_Scene->GetRegistry().get<Hamster::Transform>(m_SelectedEntity),
        Hamster::Translate, false);
  }
}

void Hierarchy::Render() {
  if (!ImGui::Begin("Hierarchy", &m_WindowOpen)) {
    ImGui::End();
    return;
  }

  const auto view = m_Scene->GetRegistry().view<Hamster::Name>();

  view.each([&](auto entity, auto &name) {
    ImGui::PushID(entt::to_integral(entity));

    if (entity == m_SelectedEntity) {
      ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    if (ImGui::TreeNode(name.name.c_str())) {
      ImGui::TreePop();
    }

    if (ImGui::IsItemClicked()) {
      SetSelectedEntity(entity);
    }

    if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
      m_RenameModal = std::make_shared<RenameModal>(name.name);
      m_RenameModalOpen = true;
    }

    ImGui::PopID();
  });

  ImGui::Separator();

  if (ImGui::Button("Add")) {
    m_Scene->CreateEntity();
  }

  if (m_RenameModalOpen) {
    ImGui::OpenPopup("Rename Window");
    m_RenameModalOpen = false;
  }

  m_RenameModal->Render();

  ImGui::End();
}
