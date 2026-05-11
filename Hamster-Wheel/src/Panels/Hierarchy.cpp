//
// Created by Jaden on 25/08/2024.
//

#include "Hierarchy.h"

#include "RenameModal.h"
#include "Theme/HamsterTheme.h"
#include "Theme/IconsFontAwesome6.h"

#include <cstring>
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

  // Search bar
  float avail = ImGui::GetContentRegionAvail().x;
  ImGui::PushItemWidth(avail);
  ImGui::InputTextWithHint("##HierarchySearch", ICON_FA_MAGNIFYING_GLASS "  Search entities...", m_SearchBuffer, sizeof(m_SearchBuffer));
  ImGui::PopItemWidth();
  ImGui::Dummy({0, 4});

  const auto view = m_Scene->GetRegistry().view<Hamster::Name>();

  view.each([&](auto entity, auto &name) {
    if (m_SearchBuffer[0] != '\0') {
      std::string lower = name.name;
      std::string filter = m_SearchBuffer;
      for (auto &ch : lower) ch = static_cast<char>(std::tolower(ch));
      for (auto &ch : filter) ch = static_cast<char>(std::tolower(ch));
      if (lower.find(filter) == std::string::npos) return;
    }

    ImGui::PushID(entt::to_integral(entity));

    bool isSelected = (entity == m_SelectedEntity);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx(name.name.c_str(), flags);

    if (ImGui::IsItemClicked()) {
      SetSelectedEntity(entity);
    }

    if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
      m_RenameModal = std::make_shared<RenameModal>(name.name);
      m_RenameModalOpen = true;
    }

    ImGui::PopID();
  });

  ImGui::Dummy({0, 8});

  if (ImGui::Button(ICON_FA_PLUS "  Add Entity")) {
    m_Scene->CreateEntity();
  }

  if (m_RenameModalOpen) {
    ImGui::OpenPopup("Rename Window");
    m_RenameModalOpen = false;
  }

  m_RenameModal->Render();

  ImGui::End();
}
