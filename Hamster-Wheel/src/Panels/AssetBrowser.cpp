#include "AssetBrowser.h"
#include "../Theme.h"
#include "../Components/Components.h"
#include "IconsFontAwesome6.h"
#include "SpritesheetEditor.h"

#include <cstring>

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
                           Hamster::AssetManager *assetManager,
                           SpritesheetEditor *spritesheetEditor)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)),
      m_AssetManager(assetManager),
      m_SpritesheetEditor(spritesheetEditor) {
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
        if (HComboItem(ICON_FA_TABLE_CELLS "  Import Spritesheet", false)) {
            const char *filterPattern = {"*.png"};
            const char *path = tinyfd_openFileDialog(
                "Select spritesheet", "", 1, &filterPattern, "PNG Files", 1);
            if (path) {
                // Import as a regular texture first (sync path so we can
                // resolve the new UUID immediately), then open the slice
                // editor on it. The .png.sheet sidecar is created when the
                // user clicks Save inside the editor.
                auto texture = m_AssetManager->AddTexture(path);
                if (texture && m_SpritesheetEditor) {
                    m_SpritesheetEditor->Open(texture->GetUUID(),
                                               m_AssetManager);
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
    // the card's label flips to an InputText next frame. `isTextureCard`
    // flag adds the spritesheet "Edit Slices..." item only for texture
    // assets (script cards don't slice).
    auto contextMenu = [&](Hamster::UUID uuid,
                            const std::filesystem::path &assetPath,
                            bool isTextureCard = false) {
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
            if (isTextureCard && m_SpritesheetEditor) {
                if (HComboItem(ICON_FA_TABLE_CELLS "  Edit Slices...", false)) {
                    m_SpritesheetEditor->Open(uuid, m_AssetManager);
                }
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

    // Texture assets. Textures with sub-sprite children (i.e. a .png.sheet
    // sidecar registered N sub-sprites under the same UUID) get a small
    // expand caret on the card. Clicking the caret flips the texture's
    // entry in m_ExpandedSheets and a mini-card grid renders inline below.
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

        // Gather this texture's sub-sprites — by parent UUID, stable order.
        std::vector<std::shared_ptr<Hamster::SubSprite>> children;
        for (auto &kv : m_AssetManager->GetSubSpriteMap()) {
            auto &ss = kv.second;
            if (ss &&
                ss->parentTextureUUID.GetUUID() == mUUID.GetUUID()) {
                children.push_back(ss);
            }
        }
        const bool isSheet = !children.empty();

        ImVec2 cardPos = ImGui::GetCursorScreenPos();
        DrawAssetCard(id.c_str(), ICON_FA_IMAGE,
                      texture->GetName().c_str(), texture.get(), cardW, cardH);
        contextMenu(mUUID, texPath, /*isTextureCard=*/true);

        // Caret overlay (top-right corner) for sheet cards. ImGui's
        // InvisibleButton would steal hover from the underlying card; use a
        // small SmallButton positioned via SetCursorScreenPos within a
        // BeginGroup() so it sits on top of the card chrome.
        if (isSheet) {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            const float caretW = 18.0f;
            const float caretH = 18.0f;
            ImVec2 ca{cardPos.x + cardW - caretW - 4.0f,
                      cardPos.y + 4.0f};
            ImVec2 cb{ca.x + caretW, ca.y + caretH};
            // Background pill for hit-feedback.
            dl->AddRectFilled(ca, cb,
                              ImGui::ColorConvertFloat4ToU32(kSurfaceHov),
                              4.0f);
            bool expanded = m_ExpandedSheets.count(mUUID) > 0;
            const char *glyph = expanded ? ICON_FA_CHEVRON_UP
                                         : ICON_FA_CHEVRON_DOWN;
            ImVec2 gSz = ImGui::CalcTextSize(glyph);
            dl->AddText({ca.x + (caretW - gSz.x) * 0.5f,
                          ca.y + (caretH - gSz.y) * 0.5f},
                         ImGui::ColorConvertFloat4ToU32(kText), glyph);

            // Hit-test the caret area. Use ImGui::IsMouseHoveringRect +
            // IsMouseClicked rather than InvisibleButton so we don't push
            // an ID that conflicts with the underlying card.
            const ImVec2 mp = ImGui::GetMousePos();
            if (mp.x >= ca.x && mp.x <= cb.x && mp.y >= ca.y && mp.y <= cb.y &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                ImGui::IsWindowHovered()) {
                if (expanded) m_ExpandedSheets.erase(mUUID);
                else m_ExpandedSheets.insert(mUUID);
            }
        }

        nextRow();

        // Mini-cards for the expanded sheet — rendered inline in the same
        // grid as a stop-gap. Future work: a flushed-row block with
        // dedicated sub-grid sizing per the spec's "card grows downward".
        if (isSheet && m_ExpandedSheets.count(mUUID) > 0) {
            const float miniW = std::max(60.0f, cardW * 0.55f);
            const float miniH = miniW;
            float texW = static_cast<float>(texture->GetWidth());
            float texH = static_cast<float>(texture->GetHeight());

            for (size_t i = 0; i < children.size(); ++i) {
                const auto &ss = children[i];
                std::string mid =
                    "sub_" + ss->uuid.GetUUIDString();

                ImGui::PushID(mid.c_str());
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##mini", {miniW, miniH});
                bool hovered = ImGui::IsItemHovered();
                bool clicked = ImGui::IsItemClicked();

                bool selected = m_SelectedSubSprites.count(ss->uuid) > 0;

                ImDrawList *dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(pos, {pos.x + miniW, pos.y + miniH},
                                  ImGui::ColorConvertFloat4ToU32(
                                      hovered ? kSurfaceHov : kSurface),
                                  4.0f);
                if (selected) {
                    dl->AddRect(pos, {pos.x + miniW, pos.y + miniH},
                                IM_COL32(255, 200, 80, 255), 4.0f, 0, 2.0f);
                }

                // Clipped texture region — show only this sub-sprite.
                if (texW > 0 && texH > 0) {
                    ImVec2 uv0{ss->pixelRect.x / texW,
                                ss->pixelRect.y / texH};
                    ImVec2 uv1{(ss->pixelRect.x + ss->pixelRect.z) / texW,
                                (ss->pixelRect.y + ss->pixelRect.w) / texH};
                    // Fit the sub-region into the mini-card preserving aspect.
                    float rw = static_cast<float>(ss->pixelRect.z);
                    float rh = static_cast<float>(ss->pixelRect.w);
                    float labelH = 18.0f;
                    float drawAreaW = miniW - 8.0f;
                    float drawAreaH = miniH - labelH - 4.0f;
                    float scale = std::min(drawAreaW / rw, drawAreaH / rh);
                    if (scale <= 0.0f) scale = 1.0f;
                    float dw = rw * scale;
                    float dh = rh * scale;
                    ImVec2 i0{pos.x + (miniW - dw) * 0.5f,
                              pos.y + (drawAreaH - dh) * 0.5f + 2.0f};
                    ImVec2 i1{i0.x + dw, i0.y + dh};
                    dl->AddImage(reinterpret_cast<ImTextureID>(
                                     static_cast<intptr_t>(
                                         texture->GetTextureId())),
                                 i0, i1, uv0, uv1);
                }
                // Name label (truncate if too wide).
                ImVec2 lSz = ImGui::CalcTextSize(ss->name.c_str());
                float maxLW = miniW - 8.0f;
                if (lSz.x > maxLW) {
                    std::string trimmed = ss->name;
                    while (trimmed.size() > 1 &&
                           ImGui::CalcTextSize((trimmed + "...").c_str()).x >
                               maxLW) {
                        trimmed.pop_back();
                    }
                    trimmed += "...";
                    ImVec2 tSz = ImGui::CalcTextSize(trimmed.c_str());
                    dl->AddText({pos.x + (miniW - tSz.x) * 0.5f,
                                  pos.y + miniH - 16.0f},
                                 ImGui::ColorConvertFloat4ToU32(kText),
                                 trimmed.c_str());
                } else {
                    dl->AddText({pos.x + (miniW - lSz.x) * 0.5f,
                                  pos.y + miniH - 16.0f},
                                 ImGui::ColorConvertFloat4ToU32(kText),
                                 ss->name.c_str());
                }

                // Multi-select: Ctrl-click toggles; plain click sets to
                // exactly this one.
                if (clicked) {
                    const bool ctrl = ImGui::GetIO().KeyCtrl;
                    if (ctrl) {
                        if (selected) m_SelectedSubSprites.erase(ss->uuid);
                        else m_SelectedSubSprites.insert(ss->uuid);
                    } else {
                        m_SelectedSubSprites.clear();
                        m_SelectedSubSprites.insert(ss->uuid);
                    }
                }

                // Drag source — bundle either this UUID alone (if not in
                // the selection set) or the whole selection. Payload format:
                //   uint32 count + count * 16 bytes (boost::uuids::uuid).
                if (ImGui::BeginDragDropSource()) {
                    std::vector<Hamster::UUID> packed;
                    if (m_SelectedSubSprites.count(ss->uuid) > 0) {
                        packed.assign(m_SelectedSubSprites.begin(),
                                       m_SelectedSubSprites.end());
                    } else {
                        packed.push_back(ss->uuid);
                    }
                    std::vector<unsigned char> buf;
                    uint32_t n = static_cast<uint32_t>(packed.size());
                    buf.resize(sizeof(n) + n * sizeof(boost::uuids::uuid));
                    std::memcpy(buf.data(), &n, sizeof(n));
                    for (uint32_t k = 0; k < n; ++k) {
                        auto raw = packed[k].GetUUID();
                        std::memcpy(buf.data() + sizeof(n) +
                                        k * sizeof(boost::uuids::uuid),
                                    &raw, sizeof(boost::uuids::uuid));
                    }
                    ImGui::SetDragDropPayload("HAMSTER_SUBSPRITE_UUIDS",
                                               buf.data(),
                                               buf.size());
                    ImGui::Text("%u sub-sprite%s", n, n == 1 ? "" : "s");
                    ImGui::EndDragDropSource();
                }

                ImGui::PopID();
                nextRow();
            }
        }
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
