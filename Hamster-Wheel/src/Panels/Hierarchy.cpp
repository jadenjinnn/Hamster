#include "Hierarchy.h"
#include "../Panel.h"
#include "../Theme.h"
#include "../Components/Components.h"
#include "IconsFontAwesome6.h"

#include <Core/Components.h>

#include <imgui.h>
#include <cctype>
#include <cstring>
#include <string>

static Panel g_HierarchyPanel = {"Hierarchy", "...##hi"};

Hierarchy::Hierarchy(Hamster::EventDispatcher *dispatcher,
                     std::shared_ptr<Hamster::Scene> scene)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)) {
    m_ActiveSceneSub = m_Dispatcher->Subscribe(
        Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(Hierarchy::OnActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));
}

Hierarchy::~Hierarchy() {
    m_Dispatcher->Unsubscribe(Hamster::ActiveSceneChanged, m_ActiveSceneSub);
}

void Hierarchy::OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
    m_SelectedEntity = entt::null;
}

void Hierarchy::Render() {
    g_HierarchyPanel.DrawHeader();
    g_HierarchyPanel.BeginContent();

    // Search bar
    float avail = ImGui::GetContentRegionAvail().x - kScrollGap;
    ImGui::PushItemWidth(avail);
    ImGui::InputTextWithHint("##HierarchySearch",
                             ICON_FA_MAGNIFYING_GLASS "  Search entities...",
                             m_SearchBuffer, sizeof(m_SearchBuffer));
    ImGui::PopItemWidth();

    // Force exactly 16px from the search bar's frame bottom to the first
    // row's text. The panel's ItemSpacing.y already advanced cursor past
    // the search bar; back out that spacing and add 16, minus the
    // FramePadding.y we push around TreeNodeEx so the *text* lands at +16.
    {
        float curY = ImGui::GetCursorPosY();
        float searchBottomY = curY - ImGui::GetStyle().ItemSpacing.y;
        ImGui::SetCursorPosY(searchBottomY + 16.0f - 6.0f);
    }

    if (!m_Scene) {
        ImGui::TextDisabled("No scene loaded");
        g_HierarchyPanel.EndContent();
        return;
    }

    bool filterActive = m_SearchBuffer[0] != '\0';

    if (filterActive) {
        // Fall back to flat list when filter is active so deep children are
        // findable. Reorder / drag is disabled in this mode.
        const auto view = m_Scene->GetRegistry().view<Hamster::Name, Hamster::ID>();
        view.each([&](auto entity, auto &name, auto &id) {
            std::string lower = name.name;
            std::string filter = m_SearchBuffer;
            for (auto &ch : lower)  ch = static_cast<char>(std::tolower(ch));
            for (auto &ch : filter) ch = static_cast<char>(std::tolower(ch));
            if (lower.find(filter) == std::string::npos) return;

            ImGui::PushID(entt::to_integral(entity));
            bool isSelected = (entity == m_SelectedEntity);
            if (ImGui::Selectable(name.name.c_str(), isSelected)) {
                m_SelectedEntity = entity;
            }
            ImGui::PopID();
        });
    } else {
        // Recursive tree render starting at root.
        auto topLevel = m_Scene->GetTopLevelEntities();
        for (size_t i = 0; i < topLevel.size(); i++) {
            RenderSiblingDropGap(Hamster::UUID::GetNil(),
                                 static_cast<uint32_t>(i));
            RenderEntityRecursive(topLevel[i], 0);
        }
        RenderSiblingDropGap(Hamster::UUID::GetNil(),
                             static_cast<uint32_t>(topLevel.size()));
    }

    ImGui::Dummy({0, 8});

    // Drop-to-root is covered by the trailing sibling-gap drop target rendered
    // after the last top-level entity (see Render's top-level loop).

    {
        const char *label = ICON_FA_PLUS "  Add Entity";
        ImVec2 textSz = ImGui::CalcTextSize(label);
        float padX = 14.0f, padY = 6.0f;
        float btnW = textSz.x + padX * 2;
        float btnH = textSz.y + padY * 2;

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btnW) * 0.5f);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##addentity", {btnW, btnH});
        bool hovered = ImGui::IsItemHovered();
        bool held    = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked();

        ImDrawList *dl = ImGui::GetWindowDrawList();
        if (hovered || held) {
            ImU32 bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive
                                                : ImGuiCol_ButtonHovered);
            dl->AddRectFilled(pos, {pos.x + btnW, pos.y + btnH}, bg, 6.0f);
        }
        dl->AddText({pos.x + padX, pos.y + padY},
                    ImGui::ColorConvertFloat4ToU32(kAccent), label);

        if (clicked) m_Scene->CreateEntity();
    }

    g_HierarchyPanel.EndContent();
}

void Hierarchy::RenderEntityRecursive(Hamster::UUID uuid, int depth) {
    auto entity = m_Scene->GetEntity(uuid);
    if (!m_Scene->EntityHasComponent<Hamster::Name>(uuid)) return;
    auto &name = m_Scene->GetEntityComponent<Hamster::Name>(uuid);

    const auto &kids = m_Scene->GetChildren(uuid);
    bool hasChildren = !kids.empty();
    bool isSelected = (entity == m_SelectedEntity);
    bool collapsed = m_Collapsed.count(uuid) > 0;

    ImGui::PushID(entt::to_integral(entity));

    // ── Row geometry ─────────────────────────────────────────────────────
    const float kIndent     = 14.0f;  // per nesting level
    const float kTriW       = 16.0f;  // collapse triangle area
    const float kPadX       = 8.0f;   // internal horizontal padding inside bg
    const float kPadY       = 4.0f;   // internal vertical   padding inside bg
    const float kRowGap     = 0.0f;   // visual gap between rows
    const float kSideMargin = 4.0f;   // gap from row bg to panel edges (so
                                      // rounded corners are visible)

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float availW = ImGui::GetContentRegionAvail().x;
    float bgX0   = cursor.x + kSideMargin;
    float bgX1   = cursor.x + availW - kSideMargin;
    float textH  = ImGui::GetTextLineHeight();
    float rowH   = textH + kPadY * 2.0f;

    ImDrawList *dl = ImGui::GetWindowDrawList();

    // ── Whole-row invisible hit area (selection + drag/drop + context) ──
    ImVec2 rowPos = {cursor.x, cursor.y};
    ImGui::SetCursorScreenPos(rowPos);
    ImGui::InvisibleButton("##row", {availW, rowH});
    bool rowHovered = ImGui::IsItemHovered();
    bool rowClicked = ImGui::IsItemClicked();

    if (ImGui::BeginDragDropSource()) {
        Hamster::UUID payload = uuid;
        ImGui::SetDragDropPayload("HIER_UUID", &payload, sizeof(Hamster::UUID));
        ImGui::Text("%s", name.name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        // Suppress ImGui's default yellow highlight; draw a thin grey border
        // ourselves to indicate "drop here = reparent under this entity".
        const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
            "HIER_UUID", ImGuiDragDropFlags_AcceptNoDrawDefaultRect |
                             ImGuiDragDropFlags_AcceptBeforeDelivery);
        if (payload) {
            dl->AddRect({cursor.x + kSideMargin, cursor.y},
                        {cursor.x + availW - kSideMargin,
                         cursor.y + textH + kPadY * 2.0f},
                        ImGui::ColorConvertFloat4ToU32(kTextDim), 4.0f, 0, 1.0f);
            if (payload->IsDelivery()) {
                Hamster::UUID dropped = *(const Hamster::UUID *)payload->Data;
                m_Scene->SetParent(dropped, uuid);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::BeginPopupContextItem("##hier_ctx")) {
        if (ImGui::MenuItem(ICON_FA_TRASH "  Delete")) {
            if (entity == m_SelectedEntity) m_SelectedEntity = entt::null;
            m_Scene->DestroyEntity(uuid);
        }
        ImGui::EndPopup();
    }

    // ── Background ───────────────────────────────────────────────────────
    if (isSelected || rowHovered) {
        ImU32 bgCol = isSelected
                          ? ImGui::ColorConvertFloat4ToU32(kAccent)
                          : ImGui::ColorConvertFloat4ToU32(kSurfaceHov);
        dl->AddRectFilled({bgX0, rowPos.y}, {bgX1, rowPos.y + rowH}, bgCol, 4.0f);
    }

    // ── Triangle (if has children) — overlaid InvisibleButton for click ──
    float triCenterX = bgX0 + kPadX + static_cast<float>(depth) * kIndent + kTriW * 0.5f;
    float triY       = rowPos.y + kPadY;
    if (hasChildren) {
        ImGui::SetCursorScreenPos({triCenterX - kTriW * 0.5f, triY - 2.0f});
        ImGui::InvisibleButton("##tri", {kTriW, textH + 4.0f});
        bool triClicked = ImGui::IsItemClicked();
        bool triHovered = ImGui::IsItemHovered();
        const char *tri = collapsed ? ICON_FA_CARET_RIGHT : ICON_FA_CARET_DOWN;
        ImVec2 triSize = ImGui::CalcTextSize(tri);
        ImU32 triCol = ImGui::ColorConvertFloat4ToU32(
            isSelected ? kText : (triHovered ? kText : kTextDim));
        dl->AddText({triCenterX - triSize.x * 0.5f, triY}, triCol, tri);

        if (triClicked) {
            if (collapsed) m_Collapsed.erase(uuid);
            else           m_Collapsed.insert(uuid);
        }
        // Triangle click consumed — don't also select.
        if (triClicked) rowClicked = false;
    }

    // ── Name text ────────────────────────────────────────────────────────
    float textX = bgX0 + kPadX + static_cast<float>(depth) * kIndent + kTriW + 4.0f;
    ImU32 textCol = isSelected ? IM_COL32(255, 255, 255, 255)
                               : ImGui::ColorConvertFloat4ToU32(kText);
    dl->AddText({textX, rowPos.y + kPadY}, textCol, name.name.c_str());

    if (rowClicked) m_SelectedEntity = entity;

    // ── Advance cursor for the next row ─────────────────────────────────
    ImGui::SetCursorScreenPos({cursor.x, rowPos.y + rowH + kRowGap});

    ImGui::PopID();

    if (hasChildren && !collapsed) {
        // Copy — recursion may mutate the children vector via drop.
        std::vector<Hamster::UUID> kidsCopy = kids;
        for (size_t i = 0; i < kidsCopy.size(); i++) {
            RenderSiblingDropGap(uuid, static_cast<uint32_t>(i));
            RenderEntityRecursive(kidsCopy[i], depth + 1);
        }
        RenderSiblingDropGap(uuid, static_cast<uint32_t>(kidsCopy.size()));
    }
}

void Hierarchy::RenderSiblingDropGap(Hamster::UUID parent,
                                     uint32_t insertIndex) {
    // Thin (4 px) drop target between rows. Dropping here reorders the
    // dragged entity to insertIndex among parent's children. We restore the
    // cursor manually so the gap doesn't inflate the row spacing via
    // ItemSpacing.y.
    ImGui::PushID("##gap");
    ImGui::PushID(insertIndex);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    const float kGapH = 2.0f;
    ImGui::InvisibleButton("##gap", {w, kGapH});
    if (ImGui::BeginDragDropTarget()) {
        // Suppress ImGui's default yellow highlight rect — we draw our own
        // accent line below as the visual cue.
        const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
            "HIER_UUID", ImGuiDragDropFlags_AcceptNoDrawDefaultRect |
                             ImGuiDragDropFlags_AcceptBeforeDelivery);
        if (payload) {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddLine({pos.x, pos.y + kGapH * 0.5f},
                        {pos.x + w, pos.y + kGapH * 0.5f},
                        ImGui::ColorConvertFloat4ToU32(kAccent), 2.0f);
            if (payload->IsDelivery()) {
                Hamster::UUID dropped = *(const Hamster::UUID *)payload->Data;
                if (m_Scene->SetParent(dropped, parent)) {
                    m_Scene->ReorderSibling(dropped, insertIndex);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    // Reclaim the ItemSpacing.y that ImGui added after the invisible button —
    // the gap's vertical footprint should be exactly kGapH.
    ImGui::SetCursorScreenPos({pos.x, pos.y + kGapH});
    ImGui::PopID();
    ImGui::PopID();
}
