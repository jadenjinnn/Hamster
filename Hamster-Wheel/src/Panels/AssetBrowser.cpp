#include "AssetBrowser.h"
#include "../Theme.h"
#include "../Components/Components.h"
#include "IconsFontAwesome6.h"

#include <Core/Application.h>
#include <Core/Components.h>
#include <Renderer/Texture.h>
#include <Utils/AssetManager.h>
#include <tinyfiledialogs.h>

#include <imgui.h>
#include <algorithm>
#include <sstream>

AssetBrowser::AssetBrowser(Hamster::EventDispatcher *dispatcher,
                           std::shared_ptr<Hamster::Scene> scene,
                           Hamster::AssetManager *assetManager)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)),
      m_AssetManager(assetManager) {
    m_Dispatcher->Subscribe(
        Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(AssetBrowser::OnActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));
}

void AssetBrowser::OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
}

static void DrawAssetCard(const char *id, const char *iconText, const char *label,
                          Hamster::Texture *tex, float cardW, float cardH) {
    ImGui::PushID(id);
    ImGui::BeginGroup();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    const float labelH  = 30.0f;
    const float cardR   = 6.0f;

    bool hovered = ImGui::IsMouseHoveringRect(pos, {pos.x + cardW, pos.y + cardH});
    dl->AddRectFilled(pos, {pos.x + cardW, pos.y + cardH},
                      ImGui::ColorConvertFloat4ToU32(hovered ? kSurfaceHov : kSurface),
                      cardR);

    if (tex && tex->GetTextureId() != 0) {
        // Preview area: edge-to-edge horizontally, top of card to start of label area.
        ImVec2 r0 = pos;
        ImVec2 r1 = {pos.x + cardW, pos.y + cardH - labelH};
        float rectW = r1.x - r0.x;
        float rectH = r1.y - r0.y;

        // Checkerboard background. Cells at the top corners use matching rounding
        // so the pattern follows the card's curve and doesn't poke past it.
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

        // Fit image into the preview area preserving aspect ratio, centered.
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

    ImGui::Dummy({cardW, cardH});
    ImGui::EndGroup();
    ImGui::PopID();
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
            m_AssetManager->AddDefaultScript();
        }
        HEndStyledPopup();
    }
    nextRow();

    // Texture assets
    for (const auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
        Hamster::UUID mUUID = uuid;
        std::string id = "tex_" + mUUID.GetUUIDString();
        DrawAssetCard(id.c_str(), ICON_FA_IMAGE,
                      texture->GetName().c_str(), texture.get(), cardW, cardH);
        nextRow();
    }

    // Script assets
    for (const auto &[uuid, script] : m_AssetManager->GetScriptMap()) {
        Hamster::UUID mUUID = uuid;
        std::string id = "scr_" + mUUID.GetUUIDString();
        DrawAssetCard(id.c_str(), ICON_FA_FILE_CODE,
                      script->GetName().c_str(), nullptr, cardW, cardH);
        nextRow();
    }
}
