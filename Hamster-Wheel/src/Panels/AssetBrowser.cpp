#include "AssetBrowser.h"
#include "../Theme.h"
#include "../Components/Components.h"
#include "IconsFontAwesome6.h"

#include <Core/Application.h>
#include <Core/Components.h>
#include <Core/Project.h>
#include <Renderer/Texture.h>
#include <Utils/AssetManager.h>
#include <tinyfiledialogs.h>

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <sstream>

AssetBrowser::AssetBrowser(Hamster::EventDispatcher *dispatcher,
                           std::shared_ptr<Hamster::Scene> scene,
                           Hamster::AssetManager *assetManager)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)),
      m_AssetManager(assetManager) {
    m_ActiveSceneSub = m_Dispatcher->Subscribe(
        Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(AssetBrowser::OnActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));
}

AssetBrowser::~AssetBrowser() {
    m_Dispatcher->Unsubscribe(Hamster::ActiveSceneChanged, m_ActiveSceneSub);
}

void AssetBrowser::OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
}

// Draws the card background + icon/texture preview area. Shared between the
// normal and edit-mode card paths so they stay visually identical.
static void DrawCardChrome(ImVec2 pos, float cardW, float cardH,
                           const char *iconText, Hamster::Texture *tex,
                           bool hovered) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    const float labelH = 30.0f;
    const float cardR  = 6.0f;

    dl->AddRectFilled(pos, {pos.x + cardW, pos.y + cardH},
                      ImGui::ColorConvertFloat4ToU32(hovered ? kSurfaceHov : kSurface),
                      cardR);

    if (tex && tex->GetTextureId() != 0) {
        ImVec2 r0 = pos;
        ImVec2 r1 = {pos.x + cardW, pos.y + cardH - labelH};
        float rectW = r1.x - r0.x;
        float rectH = r1.y - r0.y;

        // Checkerboard background; top-corner cells round to match the card.
        const float cellSz = 8.0f;
        int cols = (int)std::ceil(rectW / cellSz);
        int rows = (int)std::ceil(rectH / cellSz);
        for (int iy = 0; iy < rows; ++iy) {
            for (int ix = 0; ix < cols; ++ix) {
                ImU32 col = ((ix + iy) % 2 == 0) ? IM_COL32(60, 60, 65, 255)
                                                  : IM_COL32(40, 40, 45, 255);
                ImVec2 c0 = {r0.x + ix * cellSz, r0.y + iy * cellSz};
                ImVec2 c1 = {r0.x + std::min((ix + 1) * cellSz, rectW),
                             r0.y + std::min((iy + 1) * cellSz, rectH)};
                ImDrawFlags flags = 0;
                float r = 0.0f;
                if (iy == 0 && ix == 0)              { flags = ImDrawFlags_RoundCornersTopLeft;  r = cardR; }
                else if (iy == 0 && ix == cols - 1)  { flags = ImDrawFlags_RoundCornersTopRight; r = cardR; }
                else                                  { flags = ImDrawFlags_RoundCornersNone; }
                dl->AddRectFilled(c0, c1, col, r, flags);
            }
        }

        float texW = (float)tex->GetWidth();
        float texH = (float)tex->GetHeight();
        float scale = std::min(rectW / texW, rectH / texH);
        float drawW = texW * scale;
        float drawH = texH * scale;
        ImVec2 i0 = {r0.x + (rectW - drawW) * 0.5f, r0.y + (rectH - drawH) * 0.5f};
        ImVec2 i1 = {i0.x + drawW, i0.y + drawH};
        dl->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex->GetTextureId())),
                     i0, i1);
    } else if (iconText && g_IconLarge) {
        ImVec2 iSz = g_IconLarge->CalcTextSizeA(36.0f, FLT_MAX, 0, iconText);
        float iconX = pos.x + (cardW - iSz.x) * 0.5f;
        float iconY = pos.y + (cardH - labelH - iSz.y) * 0.5f + 4;
        dl->AddText(g_IconLarge, 36.0f, {iconX, iconY},
                    IM_COL32(120, 128, 145, 255), iconText);
    }
}

static void DrawAssetCard(const char *id, const char *iconText, const char *label,
                          Hamster::Texture *tex, float cardW, float cardH) {
    ImGui::PushID(id);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##card", {cardW, cardH});
    bool hovered = ImGui::IsItemHovered();

    DrawCardChrome(pos, cardW, cardH, iconText, tex, hovered);

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 nSz = ImGui::CalcTextSize(label);
    float maxW = cardW - 8;
    if (nSz.x > maxW) {
        std::string s = label;
        ImVec2 ell = ImGui::CalcTextSize("...");
        while (s.size() > 1 && ImGui::CalcTextSize(s.c_str()).x + ell.x > maxW)
            s.pop_back();
        s += "...";
        ImVec2 sSz = ImGui::CalcTextSize(s.c_str());
        dl->AddText({pos.x + (cardW - sSz.x) * 0.5f, pos.y + cardH - 20},
                    ImGui::ColorConvertFloat4ToU32(kText), s.c_str());
    } else {
        dl->AddText({pos.x + (cardW - nSz.x) * 0.5f, pos.y + cardH - 20},
                    ImGui::ColorConvertFloat4ToU32(kText), label);
    }

    ImGui::PopID();
}

// Edit-mode card: same chrome as DrawAssetCard, but the label area becomes
// an auto-focused InputText. Returns true once the edit is finished (commit
// or cancel); *outCommitted is set true for commit, false for cancel.
static bool DrawEditingCard(const char *id, const char *iconText,
                            Hamster::Texture *tex, float cardW, float cardH,
                            char *buf, size_t bufSize, bool *focus,
                            bool *outCommitted) {
    ImGui::PushID(id);
    ImGui::BeginGroup();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy({cardW, cardH});

    DrawCardChrome(pos, cardW, cardH, iconText, tex, /*hovered=*/true);

    ImGui::SetCursorScreenPos({pos.x + 4, pos.y + cardH - 26});
    ImGui::SetNextItemWidth(cardW - 8);
    if (*focus) {
        ImGui::SetKeyboardFocusHere();
        *focus = false;
    }
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
                              | ImGuiInputTextFlags_AutoSelectAll;
    bool enterCommit = ImGui::InputText("##inrn", buf, bufSize, flags);
    bool escaped = ImGui::IsItemActive() &&
                   ImGui::IsKeyPressed(ImGuiKey_Escape);
    bool deactivated = ImGui::IsItemDeactivated();
    ImGui::EndGroup();
    ImGui::PopID();

    if (enterCommit || deactivated) {
        *outCommitted = !escaped;
        return true;
    }
    return false;
}

static bool DrawAddCard(float cardW, float cardH) {
    ImGui::PushID("##add_asset_card");
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    bool clicked = ImGui::InvisibleButton("##add", {cardW, cardH});
    bool hovered = ImGui::IsItemHovered();

    // Accent-tinted bg so this card stands out from the others.
    ImVec4 baseBg = {kAccent.x, kAccent.y, kAccent.z, hovered ? 0.28f : 0.18f};
    dl->AddRectFilled(pos, {pos.x + cardW, pos.y + cardH},
                      ImGui::ColorConvertFloat4ToU32(baseBg), 6.0f);
    dl->AddRect(pos, {pos.x + cardW, pos.y + cardH},
                ImGui::ColorConvertFloat4ToU32(kAccent), 6.0f, 0, 1.0f);

    if (g_IconLarge) {
        const char *iconText = ICON_FA_PLUS;
        ImVec2 iSz = g_IconLarge->CalcTextSizeA(36.0f, FLT_MAX, 0, iconText);
        float iconX = pos.x + (cardW - iSz.x) * 0.5f;
        float iconY = pos.y + (cardH - 30.0f - iSz.y) * 0.5f + 4;
        dl->AddText(g_IconLarge, 36.0f, {iconX, iconY},
                    ImGui::ColorConvertFloat4ToU32(kAccent), iconText);
    }

    const char *label = "Add Asset";
    ImVec2 nSz = ImGui::CalcTextSize(label);
    dl->AddText({pos.x + (cardW - nSz.x) * 0.5f, pos.y + cardH - 20},
                ImGui::ColorConvertFloat4ToU32(kAccent), label);

    ImGui::PopID();
    return clicked;
}

void AssetBrowser::Render() {
    ImGui::Dummy({0, 4});

    float cardW = 110.0f;
    float cardH = 140.0f; // preview is square (cardW = cardH - labelH(30))
    float spacing = 12.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    int cardsPerRow = std::max(1, static_cast<int>((avail + spacing) / (cardW + spacing)));
    int colIdx = 0;

    auto nextRow = [&]() {
        colIdx++;
        if (colIdx < cardsPerRow) ImGui::SameLine(0, spacing);
        else colIdx = 0;
    };

    // Add Asset card (always first)
    if (DrawAddCard(cardW, cardH)) {
        ImGui::OpenPopup("##add_asset_popup");
    }
    if (HBeginStyledPopup("##add_asset_popup")) {
        if (HComboItem(ICON_FA_FILE_IMPORT "  Import Texture", false)) {
            const char *filterPattern = {"*.png"};
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
        if (HComboItem(ICON_FA_PLUS "  New Script", false)) {
            Hamster::UUID newId = m_AssetManager->AddDefaultScript();
            auto newScript = m_AssetManager->GetScript(newId);
            if (newScript) {
                std::filesystem::path p(newScript->GetScriptPath());
                std::string stem = p.stem().string();
                std::strncpy(m_InlineRenameBuf, stem.c_str(),
                             sizeof(m_InlineRenameBuf) - 1);
                m_InlineRenameBuf[sizeof(m_InlineRenameBuf) - 1] = '\0';
                m_InlineRenameExt = p.extension().string();
                m_InlineRenameUUID = newId;
                m_InlineRenameFocus = true;
            }
        }
        HEndStyledPopup();
    }
    nextRow();

    // Right-click context menu. Triggers inline rename instead of a modal —
    // the card's label flips to an InputText next frame.
    auto contextMenu = [&](Hamster::UUID uuid,
                            const std::filesystem::path &assetPath) {
        std::string popupId = "##ctx_" + uuid.GetUUIDString();
        if (HBeginStyledContextItem(popupId.c_str())) {
            if (HComboItem(ICON_FA_PEN "  Rename", false)) {
                std::string stem = assetPath.stem().string();
                std::strncpy(m_InlineRenameBuf, stem.c_str(),
                             sizeof(m_InlineRenameBuf) - 1);
                m_InlineRenameBuf[sizeof(m_InlineRenameBuf) - 1] = '\0';
                m_InlineRenameExt = assetPath.extension().string();
                m_InlineRenameUUID = uuid;
                m_InlineRenameFocus = true;
            }
            HEndStyledContextItem();
        }
    };

    // Commit handler shared by the texture + script edit-card paths.
    auto commitInlineRename = [&](Hamster::UUID uuid, bool committed) {
        if (committed) {
            std::string newStem = m_InlineRenameBuf;
            if (!newStem.empty()) {
                m_AssetManager->RenameAsset(uuid, newStem + m_InlineRenameExt);
            }
        }
        m_InlineRenameUUID = Hamster::UUID::GetNil();
        m_InlineRenameExt.clear();
    };

    // Texture assets
    for (const auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
        Hamster::UUID mUUID = uuid;
        std::string id = "tex_" + mUUID.GetUUIDString();
        std::filesystem::path texPath(texture->GetTexturePath());

        if (mUUID == m_InlineRenameUUID) {
            bool committed = false;
            if (DrawEditingCard(id.c_str(), ICON_FA_IMAGE, texture.get(),
                                cardW, cardH, m_InlineRenameBuf,
                                sizeof(m_InlineRenameBuf),
                                &m_InlineRenameFocus, &committed)) {
                commitInlineRename(mUUID, committed);
            }
            nextRow();
            continue;
        }

        DrawAssetCard(id.c_str(), ICON_FA_IMAGE,
                      texture->GetName().c_str(), texture.get(), cardW, cardH);
        contextMenu(mUUID, texPath);
        nextRow();
    }

    // ── Breadcrumb (only when navigating below the project root) ──
    std::filesystem::path projectDir;
    auto activeProject = Hamster::Project::GetCurrentProject();
    if (activeProject) projectDir = activeProject->GetConfig().ProjectDirectory;

    if (!m_CurrentFolder.empty() && !projectDir.empty()) {
        ImGui::Dummy({0, 2});
        // "Project" link returns to root.
        if (ImGui::SmallButton(ICON_FA_FOLDER_OPEN "  Project")) {
            m_CurrentFolder.clear();
        }
        std::filesystem::path crumb;
        for (auto const &segment : m_CurrentFolder) {
            crumb /= segment;
            ImGui::SameLine(0, 4);
            ImGui::TextDisabled("/");
            ImGui::SameLine(0, 4);
            std::string label = segment.string() + "##crumb_" + crumb.string();
            if (ImGui::SmallButton(label.c_str())) {
                m_CurrentFolder = crumb;
            }
        }
        ImGui::Dummy({0, 4});
        colIdx = 0; // breadcrumb broke our row layout — reset
    }

    // ── Folder cards (direct subdirectories of the current folder) ──
    std::filesystem::path activeFolder =
        projectDir.empty() ? std::filesystem::path{}
                            : (projectDir / m_CurrentFolder);

    if (!activeFolder.empty() && std::filesystem::is_directory(activeFolder)) {
        for (auto const &entry :
             std::filesystem::directory_iterator(activeFolder)) {
            if (!entry.is_directory()) continue;
            const std::string name = entry.path().filename().string();
            // Reserved subdirectories — managed by the engine, not user
            // browsing. Hide from the script section.
            if (name == "Animations" || name == "Scenes") continue;

            // Render a folder card; double-click to enter.
            ImGui::PushID(("folder_" + name).c_str());
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const float cardR = 6.0f;

            bool clicked = ImGui::InvisibleButton("##folder", {cardW, cardH});
            bool hovered = ImGui::IsItemHovered();
            dl->AddRectFilled(pos, {pos.x + cardW, pos.y + cardH},
                              ImGui::ColorConvertFloat4ToU32(
                                  hovered ? kSurfaceHov : kSurface),
                              cardR);

            if (g_IconLarge) {
                const char *icon = ICON_FA_FOLDER;
                ImVec2 iSz = g_IconLarge->CalcTextSizeA(36.0f, FLT_MAX, 0, icon);
                float iconX = pos.x + (cardW - iSz.x) * 0.5f;
                float iconY = pos.y + (cardH - 30.0f - iSz.y) * 0.5f + 4;
                dl->AddText(g_IconLarge, 36.0f, {iconX, iconY},
                            ImGui::ColorConvertFloat4ToU32(kAccent), icon);
            }
            ImVec2 nSz = ImGui::CalcTextSize(name.c_str());
            dl->AddText({pos.x + (cardW - nSz.x) * 0.5f, pos.y + cardH - 20},
                        ImGui::ColorConvertFloat4ToU32(kText), name.c_str());

            ImGui::PopID();

            if (clicked) {
                m_CurrentFolder /= name;
            }
            nextRow();
        }
    }

    // ── Script assets (filtered to the current folder) ──
    // projectDir may carry a trailing separator (registry stores it as the
    // user typed it), and lexically_normal() preserves the trailing slash.
    // Strip both sides to a canonical form before comparing.
    auto stripSep = [](std::filesystem::path p) {
        std::string s = p.lexically_normal().string();
        while (s.size() > 1 && (s.back() == '\\' || s.back() == '/'))
            s.pop_back();
        return std::filesystem::path(s);
    };
    std::filesystem::path normActive = stripSep(activeFolder);
    for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
        std::filesystem::path scriptPath(script->GetScriptPath());
        std::filesystem::path scriptParent = stripSep(scriptPath.parent_path());
        if (!normActive.empty() && scriptParent != normActive) {
            continue; // script is in a different folder than the current view
        }

        Hamster::UUID mUUID = uuid;
        std::string id = "scr_" + mUUID.GetUUIDString();

        if (mUUID == m_InlineRenameUUID) {
            bool committed = false;
            if (DrawEditingCard(id.c_str(), ICON_FA_FILE_CODE, nullptr,
                                cardW, cardH, m_InlineRenameBuf,
                                sizeof(m_InlineRenameBuf),
                                &m_InlineRenameFocus, &committed)) {
                commitInlineRename(mUUID, committed);
            }
            nextRow();
            continue;
        }

        DrawAssetCard(id.c_str(), ICON_FA_FILE_CODE,
                      script->GetName().c_str(), nullptr, cardW, cardH);
        contextMenu(mUUID, scriptPath);
        nextRow();
    }
}
