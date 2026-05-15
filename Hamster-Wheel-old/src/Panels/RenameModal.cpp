//
// Created by Jaden on 10/02/2025.
//

#include "RenameModal.h"

#include <imgui.h>
#include <iostream>

void RenameModal::Render() {
  if (ImGui::BeginPopupModal("Rename Window")) {
    ImGui::InputText("##New Name", m_NewName, sizeof(m_NewName));

    if (ImGui::Button("Rename")) {
      m_Name = m_NewName;

      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}
