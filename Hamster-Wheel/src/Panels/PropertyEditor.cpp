// Created by Jaden on 24/08/2024.
//

#include "PropertyEditor.h"

#include <cstdlib>

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <Scripting/Scripting.h>

#include <box2d/box2d.h>
// #include "Scripting/Scripting.h"

void PropertyEditor::Render() {
  if (!ImGui::Begin("Property Editor", &m_WindowOpen) ||
      Hamster::UUID::IsNil(m_SelectedEntity)) {
    ImGui::End();
    return;
  }

  ImGui::Text("%s", m_SelectedEntity.GetUUIDString().c_str());

  if (m_Transform != nullptr) {
    ImGui::SeparatorText("Transform");

    ImGui::PushItemWidth(80);
    ImGui::Text("Position");
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(230, 57, 70));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(216, 77, 89));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(220, 40, 52));

    ImGui::Button("X##P1", ImVec2(20, 20));

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::InputFloat("##X##P2", &m_Transform->position.x);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(81, 152, 114));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          (ImVec4)ImColor(97, 193, 142));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(50, 145, 95));

    ImGui::Button("Y##P1", ImVec2(20, 20));

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::InputFloat("##Y##P2", &m_Transform->position.y);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(72, 190, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          (ImVec4)ImColor(63, 165, 221));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(55, 138, 184));

    ImGui::Button("Z##P1", ImVec2(20, 20));

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::InputFloat("##Z##P2", &m_Transform->position.z);

    ImGui::Text("Scale");
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(230, 57, 70));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(216, 77, 89));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(220, 40, 52));

    ImGui::Button("X##S1", ImVec2(20, 20));

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::InputFloat("##X##S2", &m_Transform->size.x);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(81, 152, 114));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          (ImVec4)ImColor(97, 193, 142));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(50, 145, 95));

    ImGui::Button("Y##S1", ImVec2(20, 20));

    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    ImGui::InputFloat("##Y##S2", &m_Transform->size.y);

    ImGui::Text("Rotation");
    ImGui::SameLine();

    ImGui::InputFloat("##X#R1", &m_Transform->rotation);
  }

  if (m_Sprite != nullptr) {
    ImGui::SeparatorText("Sprite");

    ImGui::PushItemWidth(80);

    if (m_Sprite->texture != nullptr) {
      ImGui::Text("Current Sprite: ");
      ImGui::SameLine();
      ImGui::Text("%s", m_Sprite->texture->GetName().c_str());
    }

    if (ImGui::Button("Select Sprite")) {
      ImGui::OpenPopup("Select Asset");
    }

    if (ImGui::BeginPopup("Select Asset")) {
      for (const auto &[uuid, texture] :
           m_AssetManager->GetTextureMap()) {
        std::string buttonText =
            texture->GetName() + "##" + texture->GetUUID().GetUUIDString();

        if (ImGui::Selectable(buttonText.c_str())) {
          m_Sprite->texture = texture;
        }
      }

      ImGui::EndPopup();
    }
  }

  if (m_Behaviour != nullptr) {
    ImGui::SeparatorText("Scripts");

    ImGui::PushItemWidth(80);

    Hamster::UUID removeScriptUUID = Hamster::UUID::GetNil();

    for (auto &[uuid, script] : m_Behaviour->scripts) {
      if (ImGui::Button(script->GetName().c_str())) {
        OpenFile(script->GetScriptPath());
      };

      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::BeginPopup("Script Actions");
      }

      if (ImGui::BeginPopupContextItem()) {
        if (ImGui::Selectable("Remove")) {
          removeScriptUUID = uuid;
        }

        if (ImGui::Selectable("Rename")) {
          m_RenameModal = std::make_shared<RenameModal>(script->GetName());
          m_RenameModalOpen = true;
        }

        ImGui::EndPopup();
      }
    }

    if (!Hamster::UUID::IsNil(removeScriptUUID)) {
      m_Behaviour->scripts.erase(removeScriptUUID);
    }

    if (ImGui::Button("Add Script")) {
      ImGui::OpenPopup("Add Script");
    }

    if (ImGui::BeginPopup("Add Script")) {
      for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
        if (m_Behaviour->scripts.count(uuid) == 0) {
          if (ImGui::Selectable(script->GetName().c_str())) {
            m_Behaviour->scripts.emplace(script->GetUUID(), script);
          }
        }
      }

      ImGui::EndPopup();
    }

    if (m_RenameModalOpen) {
      ImGui::OpenPopup("Rename Window");
      m_RenameModalOpen = false;
    }

    if (m_RenameModal) {
      m_RenameModal->Render();
    }
  }

  if (m_Rigidbody != nullptr) {
    ImGui::SeparatorText("Rigidbody");

    ImGui::Text("Is Static: ");
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, (ImVec4)ImColor(230, 57, 70));
    ImGui::Checkbox("##dynamicbodycheckbox", &m_Rigidbody->isStatic);

    ImGui::PopStyleColor();
  }

  if (ImGui::Button("Add Component")) {
    ImGui::OpenPopup("Add Component");
  }

  if (ImGui::BeginPopup("Add Component")) {
    ImGui::SeparatorText("Components");

    if (!m_Scene->EntityHasComponent<Hamster::Rigidbody>(m_SelectedEntity)) {
      if (ImGui::Selectable("Rigidbody")) {
        m_Scene->AddEntityComponent<Hamster::Rigidbody>(m_SelectedEntity);
      }
    }

    if (!m_Scene->EntityHasComponent<Hamster::Sprite>(m_SelectedEntity)) {
      if (ImGui::Selectable("Sprite")) {
        m_Scene->AddEntityComponent<Hamster::Sprite>(m_SelectedEntity);
      }
    }

    // if (!m_Scene->EntityHasComponent<Hamster::Script>(m_SelectedEntity)) {
    if (ImGui::Selectable(("Script"))) {
      m_Scene->AddEntityComponent<Hamster::Behaviour>(m_SelectedEntity);
    }

    // }

    ImGui::EndPopup();
  }

  ImGui::End();
}

void PropertyEditor::SetSelectedEntity(Hamster::UUID uuid) {
  m_SelectedEntity = uuid;

  if (!Hamster::UUID::IsNil(m_SelectedEntity)) {
    if (m_Scene->EntityHasComponent<Hamster::Transform>(m_SelectedEntity)) {
      m_Transform =
          &m_Scene->GetEntityComponent<Hamster::Transform>(m_SelectedEntity);
    } else {
      m_Transform = nullptr;
    }

    if (m_Scene->EntityHasComponent<Hamster::Sprite>(m_SelectedEntity)) {
      m_Sprite =
          &m_Scene->GetEntityComponent<Hamster::Sprite>(m_SelectedEntity);
    } else {
      m_Sprite = nullptr;
    }

    // if (m_Scene->EntityHasComponent<Hamster::Script>(m_SelectedEntity)) {
    //   m_Script =
    //   &m_Scene->GetEntityComponent<Hamster::Script>(m_SelectedEntity);
    // }

    if (m_Scene->EntityHasComponent<Hamster::Behaviour>(m_SelectedEntity)) {
      m_Behaviour =
          &m_Scene->GetEntityComponent<Hamster::Behaviour>(m_SelectedEntity);
    } else {
      m_Behaviour = nullptr;
    }

    if (m_Scene->EntityHasComponent<Hamster::Rigidbody>(m_SelectedEntity)) {
      m_Rigidbody =
          &m_Scene->GetEntityComponent<Hamster::Rigidbody>(m_SelectedEntity);
    } else {
      m_Rigidbody = nullptr;
    }
  }
}

void PropertyEditor::OpenFile(std::filesystem::path path) {
#ifdef _WIN32
  // Windows
  std::string command = "start " + path.string();
  system(command.c_str());
#elif __APPLE__
  // macOS
  std::string command = "open " + path.string();
  system(command.c_str());
#elif __linux__
  // Linux
  std::string command = "xdg-open " + path.string();
  system(command.c_str());
#endif
}
