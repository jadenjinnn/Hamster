//
// Created by Jaden on 02/09/2024.
//

#include "FileBrowser.h"

#include <filesystem>

#include <glad/glad.h>
#include <imgui.h>

#include <Core/Application.h>
#include <Core/Project.h>

FileBrowser::FileBrowser(Hamster::EventDispatcher *dispatcher,
                         std::shared_ptr<Hamster::Scene> scene)
    : Panel(dispatcher, std::move(scene), true) {

  m_FolderIcon = std::make_unique<Hamster::Texture>(
      Hamster::Application::GetExecutablePath() +
      "/../share/Resources/Hamster-Wheel/Resources/Icons/folder.png");

  m_FileIcon = std::make_unique<Hamster::Texture>(
      Hamster::Application::GetExecutablePath() +
      "/../share/Resources/Hamster-Wheel/Resources/Icons/file.png");
}

static void DrawFileCard(const char *id, ImTextureID texId, const char *label,
                         float cardSize, float iconSize) {
    ImGui::PushID(id);
    ImGui::BeginGroup();

    float padSide = (cardSize - iconSize) * 0.5f;
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 cardEnd = {cursor.x + cardSize, cursor.y + cardSize + 20.0f};

    ImGui::InvisibleButton("##card", {cardSize, cardSize + 20.0f});
    bool hovered = ImGui::IsItemHovered();

    dl->AddRect(cursor, cardEnd, IM_COL32(255, 255, 255, 30), 6.0f);

    if (hovered) {
        dl->AddRectFilled(cursor, cardEnd,
                          IM_COL32(255, 255, 255, 15), 6.0f);
    }

    ImVec2 iconPos = {cursor.x + padSide, cursor.y + 8.0f};
    dl->AddImage(texId, iconPos,
                 {iconPos.x + iconSize, iconPos.y + iconSize});

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

void FileBrowser::Render() {
  if (!ImGui::Begin("File Browser", &m_WindowOpen)) {
    ImGui::End();
    return;
  }

  std::filesystem::path projectDir =
      Hamster::Project::GetCurrentProject()->GetConfig().ProjectDirectory;

  float browserWidth = ImGui::GetContentRegionAvail().x;
  float cellSize = m_IconSize + m_Padding + 32.0f;
  int columns = static_cast<int>(browserWidth / cellSize);
  if (columns < 1) columns = 1;

  ImGui::Columns(columns, "##FileGrid", false);

  for (auto &directory : std::filesystem::directory_iterator(projectDir)) {
    std::string label = directory.is_directory()
        ? std::filesystem::relative(directory.path(), projectDir).string()
        : directory.path().filename().string();

    ImTextureID icon = directory.is_directory()
        ? (ImTextureID)(intptr_t)m_FolderIcon->GetTextureId()
        : (ImTextureID)(intptr_t)m_FileIcon->GetTextureId();

    DrawFileCard(label.c_str(), icon, label.c_str(),
                 m_IconSize + 32.0f, m_IconSize);

    ImGui::NextColumn();
  }

  ImGui::Columns(1);
  ImGui::End();
}
