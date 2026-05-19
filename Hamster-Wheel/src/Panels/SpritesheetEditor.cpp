#include "SpritesheetEditor.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <unordered_set>

#include <imgui.h>

#include <Utils/SheetSidecar.h>

namespace {
constexpr ImU32 kRegionFill    = IM_COL32( 80, 180, 255,  35);
constexpr ImU32 kRegionBorder  = IM_COL32( 80, 180, 255, 220);
constexpr ImU32 kSelectedFill  = IM_COL32(255, 200,  80,  60);
constexpr ImU32 kSelectedBdr   = IM_COL32(255, 200,  80, 255);
constexpr ImU32 kPendingFill   = IM_COL32(255, 255, 255,  40);
constexpr ImU32 kPendingBdr    = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kHandleFill    = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kHandleBorder  = IM_COL32(255, 200,  80, 255);

constexpr float kHandlePx   = 10.0f;  // visual handle size (screen px)
constexpr float kHandleHit  = 12.0f;  // mouse-hit half-extent

glm::ivec4 NormaliseRect(glm::ivec2 a, glm::ivec2 b) {
    int x0 = std::min(a.x, b.x);
    int y0 = std::min(a.y, b.y);
    int x1 = std::max(a.x, b.x);
    int y1 = std::max(a.y, b.y);
    return {x0, y0, x1 - x0, y1 - y0};
}

// Returns the screen-space centre of handle index (0..7) for a rect drawn
// at zoom from imgPos. Handle order: TL=0, T=1, TR=2, R=3, BR=4, B=5,
// BL=6, L=7.
ImVec2 HandleCentre(ImVec2 imgPos, glm::ivec4 r, int handle, float zoom) {
    const float x0 = imgPos.x + r.x * zoom;
    const float y0 = imgPos.y + r.y * zoom;
    const float x1 = x0 + r.z * zoom;
    const float y1 = y0 + r.w * zoom;
    const float cx = (x0 + x1) * 0.5f;
    const float cy = (y0 + y1) * 0.5f;
    switch (handle) {
    case 0: return {x0, y0};
    case 1: return {cx, y0};
    case 2: return {x1, y0};
    case 3: return {x1, cy};
    case 4: return {x1, y1};
    case 5: return {cx, y1};
    case 6: return {x0, y1};
    case 7: return {x0, cy};
    }
    return {cx, cy};
}

// Apply a handle drag delta in pixel coords to the rect, returning the
// new rect. Same handle order as HandleCentre.
glm::ivec4 ApplyHandleDrag(glm::ivec4 base, int handle, glm::ivec2 dPx) {
    int x0 = base.x;
    int y0 = base.y;
    int x1 = base.x + base.z;
    int y1 = base.y + base.w;
    switch (handle) {
    case 0: x0 += dPx.x; y0 += dPx.y; break;
    case 1: y0 += dPx.y; break;
    case 2: x1 += dPx.x; y0 += dPx.y; break;
    case 3: x1 += dPx.x; break;
    case 4: x1 += dPx.x; y1 += dPx.y; break;
    case 5: y1 += dPx.y; break;
    case 6: x0 += dPx.x; y1 += dPx.y; break;
    case 7: x0 += dPx.x; break;
    }
    return NormaliseRect({x0, y0}, {x1, y1});
}

bool PointInRect(glm::ivec2 p, glm::ivec4 r) {
    return p.x >= r.x && p.x < r.x + r.z &&
           p.y >= r.y && p.y < r.y + r.w;
}
}

void SpritesheetEditor::Open(Hamster::UUID textureUUID,
                             Hamster::AssetManager *am) {
    m_TextureUUID = textureUUID;
    m_AssetManager = am;
    m_Open = true;
    m_DragMode = DragMode::None;
    m_DragHandle = -1;
    m_SelectedIndex = -1;
    m_LastError.clear();
    m_Working.clear();
    m_NameBuffers.clear();

    if (!am || Hamster::UUID::IsNil(textureUUID)) return;

    for (auto &kv : am->GetSubSpriteMap()) {
        auto &ss = kv.second;
        if (ss &&
            ss->parentTextureUUID.GetUUID() == textureUUID.GetUUID()) {
            Hamster::SubSpriteEntry e;
            e.uuid = ss->uuid;
            e.name = ss->name;
            e.pixelRect = ss->pixelRect;
            m_Working.push_back(e);
        }
    }
    m_NameBuffers.resize(m_Working.size());
    for (size_t i = 0; i < m_Working.size(); ++i) {
        std::strncpy(m_NameBuffers[i].data(), m_Working[i].name.c_str(),
                     m_NameBuffers[i].size() - 1);
        m_NameBuffers[i][m_NameBuffers[i].size() - 1] = '\0';
    }
}

void SpritesheetEditor::Close() {
    m_Open = false;
    m_DragMode = DragMode::None;
    m_DragHandle = -1;
    m_SelectedIndex = -1;
    m_Working.clear();
    m_NameBuffers.clear();
    m_LastError.clear();
}

void SpritesheetEditor::Render() {
    if (!m_Open) return;
    if (!m_AssetManager || Hamster::UUID::IsNil(m_TextureUUID)) return;

    auto tex = m_AssetManager->GetTexture(m_TextureUUID);
    if (!tex) {
        Close();
        return;
    }

    ImGui::SetNextWindowSize({820, 600}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Spritesheet Editor", &m_Open)) {
        ImGui::End();
        if (!m_Open) Close();
        return;
    }

    // Header row: sheet info + zoom.
    ImGui::Text("Sheet: %s   %d x %d", tex->GetName().c_str(),
                tex->GetWidth(), tex->GetHeight());
    ImGui::SameLine(0, 24.0f);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("Zoom", &m_Zoom, 1.0f, 16.0f, "%.0fx");
    ImGui::Separator();

    // Two-column layout: image + handles on the left, region list right.
    const float listW = 240.0f;
    const float gap   = 8.0f;
    const float imgRegionW =
        ImGui::GetContentRegionAvail().x - listW - gap;

    ImGui::BeginChild("##sheet_image",
                      {imgRegionW, ImGui::GetContentRegionAvail().y - 36.0f},
                      true, ImGuiWindowFlags_HorizontalScrollbar);

    const float imgW = static_cast<float>(tex->GetWidth())  * m_Zoom;
    const float imgH = static_cast<float>(tex->GetHeight()) * m_Zoom;
    ImVec2 imgPos = ImGui::GetCursorScreenPos();
    // InvisibleButton FIRST — owns the active-item state so clicks here
    // don't fall through to the window-background drag handler. The image
    // is then drawn at the same rect via the draw list, purely visual.
    ImGui::InvisibleButton("##sheet_canvas", {imgW, imgH});
    const bool imgHovered = ImGui::IsItemHovered();

    // Cursor-centered wheel zoom — pinch-equivalent on desktop. Reads
    // mouse-wheel delta only when the canvas is hovered so plain
    // vertical scroll inside the child still works elsewhere. Pre-zoom
    // pixel under the cursor is preserved across the zoom by
    // adjusting child-scroll relative to the new image size.
    if (imgHovered) {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            const ImVec2 mp = ImGui::GetMousePos();
            const float oldZoom = m_Zoom;
            float pxUnderCursorX = (mp.x - imgPos.x) / oldZoom;
            float pxUnderCursorY = (mp.y - imgPos.y) / oldZoom;
            // ~+/-12% per notch; clamped to slider range so the rest of
            // the UI stays consistent.
            float newZoom = oldZoom * (wheel > 0 ? 1.12f : 1.0f / 1.12f);
            newZoom = std::max(1.0f, std::min(16.0f, newZoom));
            if (newZoom != oldZoom) {
                m_Zoom = newZoom;
                // After the next frame's layout, the image will be at
                // imgPos + scroll. To keep pxUnderCursor under the mouse,
                // shift child-scroll by the delta of (px * (new - old)).
                float dx = pxUnderCursorX * (newZoom - oldZoom);
                float dy = pxUnderCursorY * (newZoom - oldZoom);
                ImGui::SetScrollX(ImGui::GetScrollX() + dx);
                ImGui::SetScrollY(ImGui::GetScrollY() + dy);
            }
        }
    }

    ImGui::GetWindowDrawList()->AddImage(
        reinterpret_cast<ImTextureID>(
            static_cast<intptr_t>(tex->GetTextureId())),
        imgPos, {imgPos.x + imgW, imgPos.y + imgH});

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

    // Draw all working rects (selection drawn on top of others).
    for (size_t i = 0; i < m_Working.size(); ++i) {
        const auto &r = m_Working[i].pixelRect;
        ImVec2 tl = screenFromPixel({r.x, r.y});
        ImVec2 br = screenFromPixel({r.x + r.z, r.y + r.w});
        bool sel = (static_cast<int>(i) == m_SelectedIndex);
        dl->AddRectFilled(tl, br, sel ? kSelectedFill : kRegionFill);
        dl->AddRect(tl, br, sel ? kSelectedBdr : kRegionBorder, 0.0f, 0,
                    sel ? 2.0f : 1.5f);
        dl->AddText({tl.x + 3.0f, tl.y + 2.0f},
                    sel ? kSelectedBdr : kRegionBorder,
                    m_Working[i].name.c_str());
    }

    // Selected rect handles.
    if (m_SelectedIndex >= 0 &&
        m_SelectedIndex < static_cast<int>(m_Working.size())) {
        const auto &r = m_Working[m_SelectedIndex].pixelRect;
        for (int h = 0; h < 8; ++h) {
            ImVec2 c = HandleCentre(imgPos, r, h, m_Zoom);
            ImVec2 a{c.x - kHandlePx * 0.5f, c.y - kHandlePx * 0.5f};
            ImVec2 b{c.x + kHandlePx * 0.5f, c.y + kHandlePx * 0.5f};
            dl->AddRectFilled(a, b, kHandleFill, 1.5f);
            dl->AddRect(a, b, kHandleBorder, 1.5f, 0, 1.5f);
        }
    }

    // Mouse interaction.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const glm::ivec2 mousePixel = pixelFromScreen(mouse);

    if (m_DragMode == DragMode::None && imgHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {

        // Hit-test handles of the selected rect first.
        int hitHandle = -1;
        if (m_SelectedIndex >= 0 &&
            m_SelectedIndex < static_cast<int>(m_Working.size())) {
            const auto &r = m_Working[m_SelectedIndex].pixelRect;
            for (int h = 0; h < 8; ++h) {
                ImVec2 c = HandleCentre(imgPos, r, h, m_Zoom);
                if (std::abs(mouse.x - c.x) <= kHandleHit &&
                    std::abs(mouse.y - c.y) <= kHandleHit) {
                    hitHandle = h;
                    break;
                }
            }
        }

        if (hitHandle != -1) {
            m_DragMode = DragMode::ResizeHandle;
            m_DragHandle = hitHandle;
            m_DragStartPixel = mousePixel;
            m_DragStartRect = m_Working[m_SelectedIndex].pixelRect;
        } else {
            // Hit-test rect bodies (topmost first — iterate in reverse).
            int hitIdx = -1;
            for (int i = static_cast<int>(m_Working.size()) - 1; i >= 0; --i) {
                if (PointInRect(mousePixel, m_Working[i].pixelRect)) {
                    hitIdx = i;
                    break;
                }
            }
            if (hitIdx != -1) {
                m_SelectedIndex = hitIdx;
                m_DragMode = DragMode::MoveBody;
                m_DragStartPixel = mousePixel;
                m_DragStartRect = m_Working[hitIdx].pixelRect;
            } else {
                // Background click — start drawing a new rect.
                m_SelectedIndex = -1;
                m_DragMode = DragMode::DrawNew;
                m_DragStartPixel = mousePixel;
                m_DragStartRect = {mousePixel.x, mousePixel.y, 0, 0};
            }
        }
    }

    if (m_DragMode != DragMode::None &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        glm::ivec2 d = mousePixel - m_DragStartPixel;
        if (m_DragMode == DragMode::DrawNew) {
            // Live overlay only — actual entry created on release.
            glm::ivec4 r = NormaliseRect(m_DragStartPixel, mousePixel);
            ImVec2 tl = screenFromPixel({r.x, r.y});
            ImVec2 br = screenFromPixel({r.x + r.z, r.y + r.w});
            dl->AddRectFilled(tl, br, kPendingFill);
            dl->AddRect(tl, br, kPendingBdr, 0.0f, 0, 1.5f);
        } else if (m_DragMode == DragMode::MoveBody &&
                   m_SelectedIndex >= 0) {
            auto &r = m_Working[m_SelectedIndex].pixelRect;
            r.x = m_DragStartRect.x + d.x;
            r.y = m_DragStartRect.y + d.y;
        } else if (m_DragMode == DragMode::ResizeHandle &&
                   m_SelectedIndex >= 0) {
            m_Working[m_SelectedIndex].pixelRect =
                ApplyHandleDrag(m_DragStartRect, m_DragHandle, d);
        }
    }

    if (m_DragMode != DragMode::None &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_DragMode == DragMode::DrawNew) {
            glm::ivec4 rect = NormaliseRect(m_DragStartPixel, mousePixel);
            rect.x = std::max(0, rect.x);
            rect.y = std::max(0, rect.y);
            if (rect.x + rect.z > tex->GetWidth())
                rect.z = tex->GetWidth() - rect.x;
            if (rect.y + rect.w > tex->GetHeight())
                rect.w = tex->GetHeight() - rect.y;
            if (rect.z > 0 && rect.w > 0) {
                std::string stem = std::filesystem::path(
                                       tex->GetTexturePath())
                                       .stem()
                                       .string();
                if (stem.empty()) stem = tex->GetName();
                // Find next free index for the auto-name.
                std::string autoName;
                for (size_t i = m_Working.size();; ++i) {
                    autoName = stem + "_" + std::to_string(i);
                    bool clash = false;
                    for (const auto &e : m_Working) {
                        if (e.name == autoName) { clash = true; break; }
                    }
                    if (!clash) break;
                }
                Hamster::SubSpriteEntry entry;
                entry.uuid = Hamster::UUID();
                entry.name = autoName;
                entry.pixelRect = rect;
                m_Working.push_back(entry);
                m_NameBuffers.emplace_back();
                std::strncpy(m_NameBuffers.back().data(), autoName.c_str(),
                             m_NameBuffers.back().size() - 1);
                m_NameBuffers.back()[m_NameBuffers.back().size() - 1] = '\0';
                m_SelectedIndex = static_cast<int>(m_Working.size()) - 1;
            }
        }
        m_DragMode = DragMode::None;
        m_DragHandle = -1;
    }

    // Delete shortcut for selected rect.
    if (m_SelectedIndex >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        m_Working.erase(m_Working.begin() + m_SelectedIndex);
        m_NameBuffers.erase(m_NameBuffers.begin() + m_SelectedIndex);
        m_SelectedIndex = -1;
    }

    ImGui::EndChild();

    // Region list pane.
    ImGui::SameLine(0, gap);
    ImGui::BeginChild("##region_list",
                      {listW, ImGui::GetContentRegionAvail().y - 36.0f},
                      true);
    ImGui::Text("Regions (%zu)", m_Working.size());
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(m_Working.size()); ++i) {
        ImGui::PushID(i);
        bool sel = (i == m_SelectedIndex);
        if (ImGui::Selectable(("##row" + std::to_string(i)).c_str(), sel,
                              0, {0, ImGui::GetTextLineHeight() + 6.0f})) {
            m_SelectedIndex = i;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-32.0f);
        if (ImGui::InputText("##name", m_NameBuffers[i].data(),
                              m_NameBuffers[i].size())) {
            m_Working[i].name = m_NameBuffers[i].data();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            m_Working.erase(m_Working.begin() + i);
            m_NameBuffers.erase(m_NameBuffers.begin() + i);
            if (m_SelectedIndex == i) m_SelectedIndex = -1;
            else if (m_SelectedIndex > i) --m_SelectedIndex;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    // Status / error + Save / Cancel buttons row.
    if (!m_LastError.empty()) {
        ImGui::TextColored({0.9f, 0.22f, 0.27f, 1.0f}, "%s",
                            m_LastError.c_str());
    }

    if (ImGui::Button("Save")) {
        m_LastError.clear();

        // Collision check: each entry's name must be unique across both the
        // working list AND the unified Texture+SubSprite namespace minus
        // sub-sprites currently registered under this same parent texture
        // (since those are about to be replaced).
        std::unordered_set<std::string> workingNames;
        for (const auto &e : m_Working) {
            if (e.name.empty()) {
                m_LastError = "empty sub-sprite name";
                break;
            }
            if (!workingNames.insert(e.name).second) {
                m_LastError = "duplicate sub-sprite name: " + e.name;
                break;
            }
            // Skip self when checking the global namespace.
            Hamster::UUID existing =
                m_AssetManager->FindAssetByName(e.name);
            if (!Hamster::UUID::IsNil(existing) &&
                existing.GetUUID() != e.uuid.GetUUID()) {
                // Is the existing match a sub-sprite under this same parent?
                auto victim = m_AssetManager->GetSubSprite(existing);
                bool sameParent =
                    victim &&
                    victim->parentTextureUUID.GetUUID() ==
                        m_TextureUUID.GetUUID();
                if (!sameParent) {
                    m_LastError =
                        "name '" + e.name +
                        "' already exists as a texture / other sheet";
                    break;
                }
            }
        }

        if (m_LastError.empty()) {
            // Commit. Drop existing sub-sprites no longer in working set.
            std::unordered_set<std::string> keep;
            for (auto &e : m_Working) {
                keep.insert(e.uuid.GetUUIDString());
            }
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

            // Add new entries; mutate existing in place.
            for (const auto &e : m_Working) {
                auto existing = m_AssetManager->GetSubSprite(e.uuid);
                if (existing) {
                    existing->pixelRect = e.pixelRect;
                    existing->name = e.name;  // collision-checked above
                } else {
                    m_AssetManager->AddSubSprite(e.uuid, m_TextureUUID,
                                                  e.pixelRect, e.name);
                }
            }

            if (tex && !tex->GetTexturePath().empty()) {
                Hamster::SheetSidecar::Write(tex->GetTexturePath(),
                                              m_Working);
            }
            Close();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        Close();
    }

    ImGui::End();
    if (!m_Open) Close();
}
