#include "Panel.h"
#include "Theme.h"
#include "IconsFontAwesome6.h"

void Panel::DrawDotsButton() const {
    float winW = ImGui::GetWindowWidth();
    ImVec2 btnSz = ImGui::CalcTextSize("...");
    float btnY = (kHeaderH - btnSz.y) * 0.5f - 2.0f;
    ImGui::SetCursorPos({winW - kPadX - btnSz.x, btnY});
    ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1,1,1,0.06f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {1,1,1,0.10f});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::Button(imguiId, btnSz);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

void Panel::DrawTabbedHeader(const char *tabs[], int count, int *selectedTab) const {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float winW = ImGui::GetWindowWidth();

    dl->AddRectFilled(wp, {wp.x + winW, wp.y + kHeaderH},
                      ImGui::ColorConvertFloat4ToU32(kHeader),
                      6.0f, ImDrawFlags_RoundCornersTop);

    bool windowActive = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImGui::SetCursorPos({kPadX, 0});
    ImFont *hf = g_HeaderFont ? g_HeaderFont : g_BoldFont;

    for (int i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine(0, 20);

        if (hf) ImGui::PushFont(hf);
        float textY = (kHeaderH - ImGui::CalcTextSize(tabs[i]).y) * 0.5f;
        ImGui::SetCursorPosY(textY);

        bool active = (*selectedTab == i);
        ImGui::PushStyleColor(ImGuiCol_Text, (active && windowActive) ? kText : kTextDim);
        ImGui::PushStyleColor(ImGuiCol_Button, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0,0,0,0});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0,0,0,0});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        // Strip frame padding so the button's text sits at the cursor Y —
        // matches DrawHeader's plain Text vertical alignment.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        if (ImGui::Button(tabs[i])) *selectedTab = i;
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        if (active && windowActive) {
            float lineY = wp.y + kHeaderH - 2.0f;
            ImVec2 bMin = ImGui::GetItemRectMin();
            ImVec2 bMax = ImGui::GetItemRectMax();
            dl->AddLine({bMin.x, lineY}, {bMax.x, lineY},
                        ImGui::ColorConvertFloat4ToU32(kAccent), 2.0f);
        }
        if (hf) ImGui::PopFont();
    }

    DrawDotsButton();
}

void Panel::DrawHeader() const {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float winW = ImGui::GetWindowWidth();

    dl->AddRectFilled(wp, {wp.x + winW, wp.y + kHeaderH},
                      ImGui::ColorConvertFloat4ToU32(kHeader),
                      6.0f, ImDrawFlags_RoundCornersTop);

    ImFont *hf = g_HeaderFont ? g_HeaderFont : g_BoldFont;
    if (hf) ImGui::PushFont(hf);
    float textH = ImGui::GetTextLineHeight();
    float textY = (kHeaderH - textH) * 0.5f;
    ImGui::SetCursorPos({kPadX, textY});

    bool active = selected || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::PushStyleColor(ImGuiCol_Text, active ? kText : kTextDim);
    ImGui::Text("%s", name);
    ImGui::PopStyleColor();

    if (active) {
        ImVec2 tMin = ImGui::GetItemRectMin();
        ImVec2 tMax = ImGui::GetItemRectMax();
        float lineY = wp.y + kHeaderH - 2.0f;
        dl->AddLine({tMin.x, lineY}, {tMax.x, lineY},
                    ImGui::ColorConvertFloat4ToU32(kAccent), 2.0f);
    }

    if (hf) ImGui::PopFont();

    DrawDotsButton();
}

void Panel::BeginContent() const {
    if (scrollable) {
        float winW = ImGui::GetWindowWidth();
        ImGui::SetCursorPos({kPadX, kHeaderH});
        float contentH = ImGui::GetWindowSize().y - kHeaderH;
        float childW = winW - kPadX;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {kPadX, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 6});
        ImGui::BeginChild("##scroll", {childW, contentH}, false);
        ImGui::Dummy({0, kContentGap});
    } else {
        ImGui::SetCursorPosY(kHeaderH + kContentGap);
    }
}

void Panel::EndContent() const {
    if (scrollable) {
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
    }
}
