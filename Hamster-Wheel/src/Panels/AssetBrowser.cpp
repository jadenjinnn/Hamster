//
// Created by Jaden on 03/09/2024.
//

#include "AssetBrowser.h"

#include <imgui.h>

#include "Core/Application.h"
#include "Theme/HamsterTheme.h"
#include "Utils/AssetManager.h"
#include <tinyfiledialogs.h>

#include <iostream>
#include <sstream>
#include <string>

AssetBrowser::AssetBrowser(Hamster::EventDispatcher *dispatcher,
                           std::shared_ptr<Hamster::Scene> scene,
                           Hamster::AssetManager *assetManager)
  : Hamster::Panel(dispatcher, scene), m_AssetManager(assetManager) {
  std::string iconsDir = Hamster::Application::GetExecutablePath() +
    "/../share/Resources/Hamster-Wheel/Resources/Icons/";

  m_PythonIcon = std::make_unique<Hamster::Texture>(iconsDir + "python.png");
  m_FolderIcon = std::make_unique<Hamster::Texture>(iconsDir + "folder.png");
  m_FileIcon   = std::make_unique<Hamster::Texture>(iconsDir + "file.png");
};

static void DrawCard(const char *id, ImTextureID texId, const char *label,
                     float cardSize, float iconSize) {
    ImGui::PushID(id);

    ImGui::BeginGroup();

    float padSide = (cardSize - iconSize) * 0.5f;

    // Card background
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 cardEnd = {cursor.x + cardSize, cursor.y + cardSize + 20.0f};

    // Invisible button for hover/click detection
    ImGui::InvisibleButton("##card", {cardSize, cardSize + 20.0f});
    bool hovered = ImGui::IsItemHovered();

    dl->AddRect(cursor, cardEnd, IM_COL32(255, 255, 255, 30), 6.0f);

    if (hovered) {
        dl->AddRectFilled(cursor, cardEnd,
                          IM_COL32(255, 255, 255, 15), 6.0f);
    }

    // Icon centered in card
    ImVec2 iconPos = {cursor.x + padSide, cursor.y + 8.0f};
    dl->AddImage(texId, iconPos,
                 {iconPos.x + iconSize, iconPos.y + iconSize});

    // Label centered below icon
    float maxTextW = cardSize - 8.0f;
    float textY = cursor.y + 8.0f + iconSize + 4.0f;
    std::string display = label;
    ImVec2 textSize = ImGui::CalcTextSize(display.c_str());
    if (textSize.x > maxTextW) {
        ImVec2 ellipsis = ImGui::CalcTextSize("...");
        while (display.size() > 1 && ImGui::CalcTextSize(display.c_str()).x + ellipsis.x > maxTextW)
            display.pop_back();
        display += "...";
        textSize = ImGui::CalcTextSize(display.c_str());
    }
    float textX = cursor.x + (cardSize - textSize.x) * 0.5f;
    dl->AddText({textX, textY}, IM_COL32(238, 238, 238, 255), display.c_str());

    ImGui::EndGroup();
    ImGui::PopID();
}

void AssetBrowser::Render() {
  if (!ImGui::Begin("Asset Browser", &m_WindowOpen, ImGuiWindowFlags_MenuBar)) {
    ImGui::End();
    return;
  }

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Import Asset")) {
        char const *filterPattern = {"*.png"};
        const char *path = tinyfd_openFileDialog(
          "Select asset", "", 1, &filterPattern, "PNG Files", 1);

        if (path) {
          std::stringstream pathSS(path);
          std::string item;
          while (std::getline(pathSS, item, '|')) {
            m_AssetManager->AddTextureAsync(item);
          }
        }
      }
      if (ImGui::MenuItem("New Script")) {
        m_AssetManager->AddDefaultScript();
      }
      ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
  }

  float browserWidth = ImGui::GetContentRegionAvail().x;
  float cellSize = m_CardSize + m_CardPadding;
  int columns = static_cast<int>(browserWidth / cellSize);
  if (columns < 1) columns = 1;

  ImGui::Columns(columns, "##AssetGrid", false);

  // Texture assets
  for (const auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
    std::string id = "tex_" + boost::uuids::to_string(uuid.GetUUID());
    ImTextureID texId = texture->GetTextureId() != 0
        ? (ImTextureID)(intptr_t)texture->GetTextureId()
        : (ImTextureID)(intptr_t)m_FileIcon->GetTextureId();

    DrawCard(id.c_str(), texId, texture->GetName().c_str(),
             m_CardSize, 64.0f);
    ImGui::NextColumn();
  }

  // Script assets
  for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
    std::string id = "scr_" + boost::uuids::to_string(uuid.GetUUID());
    DrawCard(id.c_str(),
             (ImTextureID)(intptr_t)m_PythonIcon->GetTextureId(),
             script->GetName().c_str(), m_CardSize, 64.0f);
    ImGui::NextColumn();
  }

  ImGui::Columns(1);

  ImGui::End();
}
