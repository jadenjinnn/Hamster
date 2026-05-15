//
// Created by Jaden on 03/09/2024.
//

#include "AssetBrowser.h"

#include <imgui.h>

#include "Core/Application.h"
#include "Core/Components.h"
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

void AssetBrowser::StartRename(Hamster::UUID uuid) {
  m_RenamingUUID = uuid;
  m_RenameFocusPending = true;
}

static constexpr float kIconSize = 48.0f;
static constexpr float kTextAreaH = 32.0f;

static void DrawCard(const char *id, ImTextureID texId, const char *label,
                     float cardSize) {
    ImGui::PushID(id);
    ImGui::BeginGroup();

    float padSide = (cardSize - kIconSize) * 0.5f;
    float cardH = 8.0f + kIconSize + 4.0f + kTextAreaH + 4.0f;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 cardEnd = {cursor.x + cardSize, cursor.y + cardH};

    ImGui::InvisibleButton("##card", {cardSize, cardH});
    bool hovered = ImGui::IsItemHovered();

    dl->AddRect(cursor, cardEnd, IM_COL32(255, 255, 255, 30), 6.0f);
    if (hovered) {
        dl->AddRectFilled(cursor, cardEnd,
                          IM_COL32(255, 255, 255, 15), 6.0f);
    }

    ImVec2 iconPos = {cursor.x + padSide, cursor.y + 8.0f};
    dl->AddImage(texId, iconPos,
                 {iconPos.x + kIconSize, iconPos.y + kIconSize});

    float maxTextW = cardSize - 8.0f;
    float textY = cursor.y + 8.0f + kIconSize + 4.0f;
    float lineH = ImGui::GetTextLineHeight();
    std::string text = label;
    ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

    if (textSize.x <= maxTextW) {
        float textX = cursor.x + (cardSize - textSize.x) * 0.5f;
        dl->AddText({textX, textY}, IM_COL32(238, 238, 238, 255), text.c_str());
    } else {
        // Word-wrap to 2 lines max, truncate second line with ellipsis
        size_t breakAt = 0;
        for (size_t i = 1; i < text.size(); i++) {
            std::string sub = text.substr(0, i);
            if (ImGui::CalcTextSize(sub.c_str()).x > maxTextW) {
                breakAt = i - 1;
                break;
            }
        }
        if (breakAt == 0) breakAt = text.size();
        std::string line1 = text.substr(0, breakAt);
        std::string line2 = text.substr(breakAt);

        ImVec2 ellipsis = ImGui::CalcTextSize("...");
        while (line2.size() > 1 &&
               ImGui::CalcTextSize(line2.c_str()).x + ellipsis.x > maxTextW)
            line2.pop_back();
        if (ImGui::CalcTextSize((text.substr(breakAt)).c_str()).x > maxTextW)
            line2 += "...";

        float x1 = cursor.x + (cardSize - ImGui::CalcTextSize(line1.c_str()).x) * 0.5f;
        float x2 = cursor.x + (cardSize - ImGui::CalcTextSize(line2.c_str()).x) * 0.5f;
        dl->AddText({x1, textY}, IM_COL32(238, 238, 238, 255), line1.c_str());
        dl->AddText({x2, textY + lineH}, IM_COL32(238, 238, 238, 255), line2.c_str());
    }

    ImGui::EndGroup();
    ImGui::PopID();
}

static bool DrawCardRenameable(const char *id, ImTextureID texId,
                               float cardSize,
                               bool isRenaming, char *renameBuf,
                               size_t renameBufSize, bool focusPending,
                               bool &renameFinished) {
    ImGui::PushID(id);
    ImGui::BeginGroup();

    float padSide = (cardSize - kIconSize) * 0.5f;
    float cardH = 8.0f + kIconSize + 4.0f + kTextAreaH + 4.0f;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 cardEnd = {cursor.x + cardSize, cursor.y + cardH};

    ImGui::InvisibleButton("##card", {cardSize, cardH});
    bool hovered = ImGui::IsItemHovered();
    bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    dl->AddRect(cursor, cardEnd, IM_COL32(255, 255, 255, 30), 6.0f);

    if (hovered || isRenaming) {
        dl->AddRectFilled(cursor, cardEnd,
                          IM_COL32(255, 255, 255, 15), 6.0f);
    }

    ImVec2 iconPos = {cursor.x + padSide, cursor.y + 8.0f};
    dl->AddImage(texId, iconPos,
                 {iconPos.x + kIconSize, iconPos.y + kIconSize});

    float textY = cursor.y + 8.0f + kIconSize + 4.0f;

    if (isRenaming) {
        float inputW = cardSize - 8.0f;
        ImGui::SetCursorScreenPos({cursor.x + 4.0f, textY});
        ImGui::SetNextItemWidth(inputW);
        if (focusPending) {
            ImGui::SetKeyboardFocusHere();
        }
        if (ImGui::InputText("##rename", renameBuf, renameBufSize,
                             ImGuiInputTextFlags_EnterReturnsTrue |
                             ImGuiInputTextFlags_AutoSelectAll)) {
            renameFinished = true;
        }
        if (!focusPending && !ImGui::IsItemActive()) {
            renameFinished = true;
        }
    } else {
        float maxTextW = cardSize - 8.0f;
        float lineH = ImGui::GetTextLineHeight();
        std::string text = renameBuf;
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

        if (textSize.x <= maxTextW) {
            float textX = cursor.x + (cardSize - textSize.x) * 0.5f;
            dl->AddText({textX, textY}, IM_COL32(238, 238, 238, 255), text.c_str());
        } else {
            size_t breakAt = 0;
            for (size_t i = 1; i < text.size(); i++) {
                std::string sub = text.substr(0, i);
                if (ImGui::CalcTextSize(sub.c_str()).x > maxTextW) {
                    breakAt = i - 1;
                    break;
                }
            }
            if (breakAt == 0) breakAt = text.size();
            std::string line1 = text.substr(0, breakAt);
            std::string line2 = text.substr(breakAt);

            ImVec2 ellipsis = ImGui::CalcTextSize("...");
            while (line2.size() > 1 &&
                   ImGui::CalcTextSize(line2.c_str()).x + ellipsis.x > maxTextW)
                line2.pop_back();
            if (ImGui::CalcTextSize((text.substr(breakAt)).c_str()).x > maxTextW)
                line2 += "...";

            float x1 = cursor.x + (cardSize - ImGui::CalcTextSize(line1.c_str()).x) * 0.5f;
            float x2 = cursor.x + (cardSize - ImGui::CalcTextSize(line2.c_str()).x) * 0.5f;
            dl->AddText({x1, textY}, IM_COL32(238, 238, 238, 255), line1.c_str());
            dl->AddText({x2, textY + lineH}, IM_COL32(238, 238, 238, 255), line2.c_str());
        }
    }

    ImGui::EndGroup();
    ImGui::PopID();

    return rightClicked;
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
        Hamster::UUID newId = m_AssetManager->AddDefaultScript();
        StartRename(newId);
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

    bool isRenaming = !Hamster::UUID::IsNil(m_RenamingUUID) &&
                      uuid.GetUUID() == m_RenamingUUID.GetUUID();

    if (isRenaming && m_RenameFocusPending) {
      strncpy(m_RenameBuffer, texture->GetName().c_str(),
              sizeof(m_RenameBuffer) - 1);
      m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
    }

    char labelBuf[128];
    if (!isRenaming) {
      strncpy(labelBuf, texture->GetName().c_str(), sizeof(labelBuf) - 1);
      labelBuf[sizeof(labelBuf) - 1] = '\0';
    }

    bool renameFinished = false;
    bool rightClicked = DrawCardRenameable(
        id.c_str(), texId, m_CardSize,
        isRenaming,
        isRenaming ? m_RenameBuffer : labelBuf,
        isRenaming ? sizeof(m_RenameBuffer) : sizeof(labelBuf),
        isRenaming && m_RenameFocusPending,
        renameFinished);

    if (isRenaming) {
      m_RenameFocusPending = false;

      if (renameFinished) {
        if (strlen(m_RenameBuffer) > 0) {
          texture->SetName(m_RenameBuffer);
        }
        m_RenamingUUID = Hamster::UUID::GetNil();
      }
    }

    if (rightClicked && !isRenaming) {
      m_ContextMenuUUID = uuid;
      m_ContextMenuIsTexture = true;
      ImGui::OpenPopup("##TextureContextMenu");
    }

    ImGui::NextColumn();
  }

  // Script assets — context menu for rename
  for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
    std::string id = "scr_" + boost::uuids::to_string(uuid.GetUUID());
    bool isRenaming = !Hamster::UUID::IsNil(m_RenamingUUID) &&
                      uuid.GetUUID() == m_RenamingUUID.GetUUID();

    if (isRenaming && m_RenameFocusPending) {
      strncpy(m_RenameBuffer, script->GetName().c_str(),
              sizeof(m_RenameBuffer) - 1);
      m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
    }

    char labelBuf[128];
    if (!isRenaming) {
      strncpy(labelBuf, script->GetName().c_str(), sizeof(labelBuf) - 1);
      labelBuf[sizeof(labelBuf) - 1] = '\0';
    }

    bool renameFinished = false;
    bool rightClicked = DrawCardRenameable(
        id.c_str(),
        (ImTextureID)(intptr_t)m_PythonIcon->GetTextureId(),
        m_CardSize,
        isRenaming,
        isRenaming ? m_RenameBuffer : labelBuf,
        isRenaming ? sizeof(m_RenameBuffer) : sizeof(labelBuf),
        isRenaming && m_RenameFocusPending,
        renameFinished);

    if (isRenaming) {
      m_RenameFocusPending = false;

      if (renameFinished) {
        if (strlen(m_RenameBuffer) > 0) {
          script->SetName(m_RenameBuffer);
        }
        m_RenamingUUID = Hamster::UUID::GetNil();
      }
    }

    if (rightClicked && !isRenaming) {
      m_ContextMenuUUID = uuid;
      m_ContextMenuIsTexture = false;
      ImGui::OpenPopup("##ScriptContextMenu");
    }

    ImGui::NextColumn();
  }

  // Texture context menu
  if (ImGui::BeginPopup("##TextureContextMenu")) {
    if (ImGui::Selectable("Rename")) {
      StartRename(m_ContextMenuUUID);
    }
    if (ImGui::Selectable("Delete")) {
      m_AssetManager->RemoveTexture(m_ContextMenuUUID);
      m_ContextMenuUUID = Hamster::UUID::GetNil();
    }
    ImGui::EndPopup();
  }

  // Script context menu
  if (ImGui::BeginPopup("##ScriptContextMenu")) {
    if (ImGui::Selectable("Rename")) {
      StartRename(m_ContextMenuUUID);
    }
    if (ImGui::Selectable("Delete")) {
      if (m_Scene) {
        auto view = m_Scene->GetRegistry().view<Hamster::Behaviour>();
        for (auto entity : view) {
          auto &behaviour = view.get<Hamster::Behaviour>(entity);
          behaviour.scripts.erase(m_ContextMenuUUID);
        }
      }
      m_AssetManager->RemoveScript(m_ContextMenuUUID);
      m_ContextMenuUUID = Hamster::UUID::GetNil();
    }
    ImGui::EndPopup();
  }

  ImGui::Columns(1);

  ImGui::End();
}
