#include "SpritesheetEditor.h"

#include <algorithm>
#include <filesystem>
#include <unordered_set>

#include <imgui.h>

#include <Utils/SheetSidecar.h>

namespace {
constexpr ImU32 kRegionFill   = IM_COL32( 80, 180, 255,  50);
constexpr ImU32 kRegionBorder = IM_COL32( 80, 180, 255, 255);
constexpr ImU32 kPendingFill  = IM_COL32(255, 200,  80,  60);
constexpr ImU32 kPendingBdr   = IM_COL32(255, 200,  80, 255);
}

void SpritesheetEditor::Open(Hamster::UUID textureUUID,
                             Hamster::AssetManager *am) {
    m_TextureUUID = textureUUID;
    m_AssetManager = am;
    m_Open = true;
    m_Drawing = false;
    m_Working.clear();

    if (!am || Hamster::UUID::IsNil(textureUUID)) return;

    // Seed the working list from any sub-sprites already registered under
    // this parent texture (these come from the .sheet sidecar on project
    // open, or from a previous slice session in this editor process).
    for (const auto &[uuid, ss] : am->GetSubSpriteMap()) {
        if (ss &&
            ss->parentTextureUUID.GetUUID() == textureUUID.GetUUID()) {
            Hamster::SubSpriteEntry e;
            e.uuid = ss->uuid;
            e.name = ss->name;
            e.pixelRect = ss->pixelRect;
            m_Working.push_back(e);
        }
    }
}

void SpritesheetEditor::Close() {
    m_Open = false;
    m_Drawing = false;
    m_Working.clear();
}

void SpritesheetEditor::Render() {
    if (!m_Open) return;
    if (!m_AssetManager || Hamster::UUID::IsNil(m_TextureUUID)) return;

    auto tex = m_AssetManager->GetTexture(m_TextureUUID);
    if (!tex) {
        Close();
        return;
    }

    ImGui::SetNextWindowSize({640, 560}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Spritesheet Editor", &m_Open)) {
        ImGui::End();
        if (!m_Open) Close();
        return;
    }

    // Header — texture name + size + zoom slider.
    ImGui::Text("Sheet: %s   %d x %d", tex->GetName().c_str(),
                tex->GetWidth(), tex->GetHeight());
    ImGui::SliderFloat("Zoom", &m_Zoom, 1.0f, 16.0f, "%.0fx");

    ImGui::Separator();

    // Image preview region. ImGui::Image renders the texture at
    // (width*zoom, height*zoom) screen pixels. Cursor screen pos lets us
    // map back to source pixels for click + drag.
    const float imgW = static_cast<float>(tex->GetWidth())  * m_Zoom;
    const float imgH = static_cast<float>(tex->GetHeight()) * m_Zoom;
    ImVec2 imgPos = ImGui::GetCursorScreenPos();
    ImGui::Image(reinterpret_cast<ImTextureID>(
                     static_cast<intptr_t>(tex->GetTextureId())),
                 {imgW, imgH});

    ImDrawList *dl = ImGui::GetWindowDrawList();

    auto pixelFromScreen = [&](ImVec2 sp) -> glm::ivec2 {
        return {
            static_cast<int>((sp.x - imgPos.x) / m_Zoom),
            static_cast<int>((sp.y - imgPos.y) / m_Zoom),
        };
    };
    auto screenFromPixel = [&](glm::ivec2 p) -> ImVec2 {
        return {imgPos.x + p.x * m_Zoom, imgPos.y + p.y * m_Zoom};
    };

    // Draw existing regions (working list).
    for (const auto &e : m_Working) {
        ImVec2 tl = screenFromPixel({e.pixelRect.x, e.pixelRect.y});
        ImVec2 br = screenFromPixel({e.pixelRect.x + e.pixelRect.z,
                                     e.pixelRect.y + e.pixelRect.w});
        dl->AddRectFilled(tl, br, kRegionFill);
        dl->AddRect(tl, br, kRegionBorder, 0.0f, 0, 1.5f);
        // Label
        dl->AddText({tl.x + 2.0f, tl.y + 2.0f}, kRegionBorder, e.name.c_str());
    }

    // Mouse interaction on the image. Capture only when the mouse is over
    // the image rect to avoid stealing clicks from header / list items.
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const glm::ivec2 pixel = pixelFromScreen(mouse);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_Drawing = true;
        m_DrawStartPixel = pixel;
        m_DrawCurrentPixel = pixel;
    }
    if (m_Drawing && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        m_DrawCurrentPixel = pixel;

        // Render pending rect overlay.
        glm::ivec2 a = m_DrawStartPixel;
        glm::ivec2 b = m_DrawCurrentPixel;
        glm::ivec2 tl{std::min(a.x, b.x), std::min(a.y, b.y)};
        glm::ivec2 br{std::max(a.x, b.x), std::max(a.y, b.y)};
        ImVec2 stl = screenFromPixel(tl);
        ImVec2 sbr = screenFromPixel(br);
        dl->AddRectFilled(stl, sbr, kPendingFill);
        dl->AddRect(stl, sbr, kPendingBdr, 0.0f, 0, 1.5f);
    }
    if (m_Drawing && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        glm::ivec2 a = m_DrawStartPixel;
        glm::ivec2 b = m_DrawCurrentPixel;
        glm::ivec2 tl{std::min(a.x, b.x), std::min(a.y, b.y)};
        glm::ivec2 br{std::max(a.x, b.x), std::max(a.y, b.y)};
        // Clamp to texture bounds.
        tl.x = std::max(0, std::min(tl.x, tex->GetWidth()));
        tl.y = std::max(0, std::min(tl.y, tex->GetHeight()));
        br.x = std::max(0, std::min(br.x, tex->GetWidth()));
        br.y = std::max(0, std::min(br.y, tex->GetHeight()));
        glm::ivec4 rect{tl.x, tl.y, br.x - tl.x, br.y - tl.y};
        if (rect.z > 0 && rect.w > 0) {
            std::string stem =
                std::filesystem::path(tex->GetTexturePath()).stem().string();
            if (stem.empty()) stem = tex->GetName();
            // Auto-name: <stem>_<index> — index = current count of working
            // entries (skipping over names that already exist to keep them
            // unique).
            std::string autoName;
            for (size_t i = m_Working.size();; ++i) {
                autoName = stem + "_" + std::to_string(i);
                bool clash = false;
                for (const auto &e : m_Working) {
                    if (e.name == autoName) {
                        clash = true;
                        break;
                    }
                }
                if (!clash) break;
            }
            Hamster::SubSpriteEntry e;
            e.uuid = Hamster::UUID();  // fresh
            e.name = autoName;
            e.pixelRect = rect;
            m_Working.push_back(e);
        }
        m_Drawing = false;
    }

    ImGui::Separator();

    // Buttons row at the bottom — Save commits the diff, Cancel discards.
    if (ImGui::Button("Save")) {
        if (m_AssetManager) {
            // Build a set of UUIDs we want to keep after this save.
            std::unordered_set<std::string> keep;
            for (auto &e : m_Working) {
                keep.insert(e.uuid.GetUUIDString());
            }
            // Drop any existing sub-sprites under this parent that are no
            // longer in the working list.
            std::vector<Hamster::UUID> toRemove;
            for (auto &kv : m_AssetManager->GetSubSpriteMap()) {
                auto &ss = kv.second;
                if (ss && ss->parentTextureUUID.GetUUID() ==
                              m_TextureUUID.GetUUID() &&
                    keep.find(ss->uuid.GetUUIDString()) == keep.end()) {
                    toRemove.push_back(ss->uuid);
                }
            }
            for (const auto &u : toRemove) {
                m_AssetManager->RemoveSubSprite(u);
            }
            // Add new entries (or update existing — for stage 3 only new
            // rects come through this path; stage 4 will handle modifies).
            for (const auto &e : m_Working) {
                if (!m_AssetManager->GetSubSprite(e.uuid)) {
                    m_AssetManager->AddSubSprite(e.uuid, m_TextureUUID,
                                                  e.pixelRect, e.name);
                }
            }
            // Persist to disk.
            if (tex && !tex->GetTexturePath().empty()) {
                Hamster::SheetSidecar::Write(tex->GetTexturePath(), m_Working);
            }
        }
        Close();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        Close();
    }

    ImGui::End();
    if (!m_Open) Close();
}
