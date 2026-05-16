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
    ImGui::Dummy({0, 4});

    if (!m_Scene) {
        ImGui::TextDisabled("No scene loaded");
        g_HierarchyPanel.EndContent();
        return;
    }

    const auto view = m_Scene->GetRegistry().view<Hamster::Name>();

    view.each([&](auto entity, auto &name) {
        if (m_SearchBuffer[0] != '\0') {
            std::string lower = name.name;
            std::string filter = m_SearchBuffer;
            for (auto &ch : lower)  ch = static_cast<char>(std::tolower(ch));
            for (auto &ch : filter) ch = static_cast<char>(std::tolower(ch));
            if (lower.find(filter) == std::string::npos) return;
        }

        ImGui::PushID(entt::to_integral(entity));

        bool isSelected = (entity == m_SelectedEntity);
        float rowH = ImGui::GetTextLineHeight() + 8.0f;

        if (isSelected) {
            ImVec2 rp = ImGui::GetCursorScreenPos();
            rp.x = ImGui::GetWindowPos().x;
            rp.y -= (rowH - ImGui::GetTextLineHeight()) * 0.5f;
            float rw = ImGui::GetWindowSize().x;
            ImGui::GetWindowDrawList()->AddRectFilled(
                rp, {rp.x + rw, rp.y + rowH},
                ImGui::ColorConvertFloat4ToU32(kAccent));
        }

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            ImGui::Text("    %s", name.name.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::Text("    %s", name.name.c_str());
        }

        if (ImGui::IsItemClicked()) {
            m_SelectedEntity = entity;
        }

        ImGui::PopID();
    });

    ImGui::Dummy({0, 8});
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
