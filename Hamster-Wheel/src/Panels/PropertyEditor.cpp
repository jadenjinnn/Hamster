// Created by Jaden on 24/08/2024.
//

#include "PropertyEditor.h"

#include <cstdlib>

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <Scripting/Scripting.h>

#include <box2d/box2d.h>

#include "Theme/IconsFontAwesome6.h"

static constexpr ImVec4 kAxisRed       = {0.878f, 0.290f, 0.310f, 1.0f};
static constexpr ImVec4 kAxisRedHov    = {0.920f, 0.360f, 0.380f, 1.0f};
static constexpr ImVec4 kAxisRedAct    = {0.780f, 0.220f, 0.240f, 1.0f};
static constexpr ImVec4 kAxisGreen     = {0.337f, 0.576f, 0.439f, 1.0f};
static constexpr ImVec4 kAxisGreenHov  = {0.400f, 0.660f, 0.510f, 1.0f};
static constexpr ImVec4 kAxisGreenAct  = {0.260f, 0.490f, 0.360f, 1.0f};
static constexpr ImVec4 kAxisBlue      = {0.310f, 0.620f, 0.890f, 1.0f};
static constexpr ImVec4 kAxisBlueHov   = {0.380f, 0.690f, 0.940f, 1.0f};
static constexpr ImVec4 kAxisBlueAct   = {0.240f, 0.530f, 0.780f, 1.0f};

static void DrawAxisFloat(const char *axisLabel, const char *inputId, float *value,
                          const ImVec4 &col, const ImVec4 &hov, const ImVec4 &act,
                          float inputWidth) {
    ImGuiStyle &style = ImGui::GetStyle();
    float h = ImGui::GetFrameHeight();
    float rounding = style.FrameRounding;
    float savedSpacingX = style.ItemSpacing.x;
    ImDrawList *dl = ImGui::GetWindowDrawList();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, style.ItemSpacing.y});

    // Colored axis prefix — left-rounded only
    ImVec2 pp = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(axisLabel, {h, h});
    ImVec4 bg = ImGui::IsItemActive() ? act : (ImGui::IsItemHovered() ? hov : col);
    dl->AddRectFilled(pp, {pp.x + h, pp.y + h},
                      ImGui::ColorConvertFloat4ToU32(bg),
                      rounding, ImDrawFlags_RoundCornersLeft);
    char vis[2] = {axisLabel[0], '\0'};
    ImVec2 ts = ImGui::CalcTextSize(vis);
    dl->AddText({pp.x + (h - ts.x) * 0.5f, pp.y + (h - ts.y) * 0.5f},
                ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_Text]), vis);

    // Input — right-rounded only, drawn as custom bg + transparent InputFloat
    ImGui::SameLine();
    ImVec2 ip = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(ip, {ip.x + inputWidth, ip.y + h},
                      ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_FrameBg]),
                      rounding, ImDrawFlags_RoundCornersRight);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0, 0, 0, 0});
    ImGui::PushItemWidth(inputWidth);
    ImGui::InputFloat(inputId, value);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);

    ImGui::PopStyleVar();
    ImGui::SameLine(0, savedSpacingX);
}

static constexpr float kLabelColumnWidth = 72.0f;
static constexpr float kAxisInputWidth   = 60.0f;

void PropertyEditor::Render() {
  if (!ImGui::Begin("Property Editor", &m_WindowOpen) ||
      Hamster::UUID::IsNil(m_SelectedEntity)) {
    ImGui::End();
    return;
  }

  // Entity header — icon + editable name
  {
    float iconSize = 36.0f;
    ImGui::Image((ImTextureID)(intptr_t)m_EntityIcon->GetTextureId(),
                 {iconSize, iconSize},
                 {0, 0}, {1, 1},
                 ImVec4(0.345f, 0.529f, 0.969f, 1.0f));

    ImGui::SameLine();

    ImGui::BeginGroup();
    if (m_Name != nullptr) {
      ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
      ImGui::InputText("##EntityName", &m_Name->name[0], m_Name->name.capacity() + 1,
                        ImGuiInputTextFlags_CallbackResize,
                        [](ImGuiInputTextCallbackData *data) -> int {
                            auto *str = static_cast<std::string *>(data->UserData);
                            str->resize(data->BufTextLen);
                            data->Buf = &(*str)[0];
                            return 0;
                        }, &m_Name->name);
      ImGui::PopItemWidth();
    }
    ImGui::TextDisabled("%s", m_SelectedEntity.GetUUIDString().c_str());
    ImGui::EndGroup();
  }

  ImGui::Dummy({0, 8});

  if (m_Transform != nullptr) {
    ImGui::SeparatorText(ICON_FA_UP_DOWN_LEFT_RIGHT "  Transform");
    ImGui::Dummy({0, 4});

    // Position
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Position");
    ImGui::SameLine(kLabelColumnWidth);
    DrawAxisFloat("X##P", "##XP", &m_Transform->position.x, kAxisRed, kAxisRedHov, kAxisRedAct, kAxisInputWidth);
    DrawAxisFloat("Y##P", "##YP", &m_Transform->position.y, kAxisGreen, kAxisGreenHov, kAxisGreenAct, kAxisInputWidth);
    DrawAxisFloat("Z##P", "##ZP", &m_Transform->position.z, kAxisBlue, kAxisBlueHov, kAxisBlueAct, kAxisInputWidth);
    ImGui::NewLine();
    ImGui::Dummy({0, 4});

    // Scale
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scale");
    ImGui::SameLine(kLabelColumnWidth);
    DrawAxisFloat("X##S", "##XS", &m_Transform->size.x, kAxisRed, kAxisRedHov, kAxisRedAct, kAxisInputWidth);
    DrawAxisFloat("Y##S", "##YS", &m_Transform->size.y, kAxisGreen, kAxisGreenHov, kAxisGreenAct, kAxisInputWidth);
    ImGui::NewLine();
    ImGui::Dummy({0, 4});

    // Rotation
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Rotation");
    ImGui::SameLine(kLabelColumnWidth);
    ImGui::PushItemWidth(kAxisInputWidth + ImGui::GetFrameHeight());
    ImGui::InputFloat("##XR", &m_Transform->rotation);
    ImGui::PopItemWidth();
  }

  if (m_Sprite != nullptr) {
    ImGui::Dummy({0, 8});
    ImGui::SeparatorText(ICON_FA_IMAGE "  Sprite");
    ImGui::Dummy({0, 4});

    constexpr float kThumbSize = 96.0f;
    auto &colour = m_Sprite->colour;
    ImVec4 tint(colour.r, colour.g, colour.b, 1.0f);

    ImGui::BeginGroup();
    if (m_Sprite->texture != nullptr && m_Sprite->texture->GetTextureId() != 0) {
      ImGui::Image(
          (ImTextureID)(intptr_t)m_Sprite->texture->GetTextureId(),
          ImVec2(kThumbSize, kThumbSize),
          ImVec2(0, 0), ImVec2(1, 1),
          tint);
    } else {
      ImVec2 pos = ImGui::GetCursorScreenPos();
      ImGui::GetWindowDrawList()->AddRectFilled(
          pos, ImVec2(pos.x + kThumbSize, pos.y + kThumbSize),
          IM_COL32(40, 40, 40, 255));
      ImGui::GetWindowDrawList()->AddRect(
          pos, ImVec2(pos.x + kThumbSize, pos.y + kThumbSize),
          IM_COL32(80, 80, 80, 255));
      ImGui::Dummy(ImVec2(kThumbSize, kThumbSize));
    }
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    if (m_Sprite->texture != nullptr) {
      ImGui::Text("Name");
      ImGui::SameLine();
      ImGui::TextDisabled("%s", m_Sprite->texture->GetName().c_str());
      ImGui::Text("Size");
      ImGui::SameLine();
      ImGui::TextDisabled("%d x %d", m_Sprite->texture->GetWidth(), m_Sprite->texture->GetHeight());
    } else {
      ImGui::Text("Name");
      ImGui::SameLine();
      ImGui::TextDisabled("None");
      ImGui::Text("Size");
      ImGui::SameLine();
      ImGui::TextDisabled("- x -");
    }
    ImGui::Text("Tint");
    ImGui::SameLine();
    float col[3] = {colour.r, colour.g, colour.b};
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::ColorEdit3("##Tint", col, ImGuiColorEditFlags_NoLabel)) {
      colour = glm::vec3(col[0], col[1], col[2]);
    }
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::Dummy({0, 4});

    ImGui::PushItemWidth(80);

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
    ImGui::Dummy({0, 8});
    ImGui::SeparatorText(ICON_FA_CODE "  Scripts");
    ImGui::Dummy({0, 4});

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
    ImGui::Dummy({0, 8});
    ImGui::SeparatorText(ICON_FA_SHAPES "  Rigidbody");
    ImGui::Dummy({0, 4});

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Is Static");
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.345f, 0.529f, 0.969f, 1.0f));
    ImGui::Checkbox("##dynamicbodycheckbox", &m_Rigidbody->isStatic);

    ImGui::PopStyleColor();
  }

  ImGui::Dummy({0, 16});
  float btnWidth = ImGui::CalcTextSize("Add Component").x + ImGui::GetStyle().FramePadding.x * 2;
  ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - btnWidth) * 0.5f + ImGui::GetCursorPosX());
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
    if (m_Scene->EntityHasComponent<Hamster::Name>(m_SelectedEntity)) {
      m_Name =
          &m_Scene->GetEntityComponent<Hamster::Name>(m_SelectedEntity);
    } else {
      m_Name = nullptr;
    }

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
