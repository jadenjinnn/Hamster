#include "Components.h"
#include "../Theme.h"

#include <cstdio>

void SectionSeparator(float spacingBefore, float spacingAfter) {
    ImGui::Dummy({0, spacingBefore});
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    dl->AddLine({cp.x, cp.y}, {cp.x + w, cp.y}, IM_COL32(255, 255, 255, 25), 1.0f);
    ImGui::Dummy({0, spacingAfter});
}

void DrawGradientBar(ImVec2 p0, ImVec2 p1, ImU32 left, ImU32 right, float rounding) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilledMultiColor(p0, p1, left, right, right, left);
}

void SectionHeader(const char *label) {
    if (g_BoldFont) ImGui::PushFont(g_BoldFont);
    ImGui::Text("%s", label);
    if (g_BoldFont) ImGui::PopFont();
    ImGui::Dummy({0, 2});
}

bool HButton(const char *label, float width) {
    ImGui::PushStyleColor(ImGuiCol_Button, kHeader);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceAct);
    bool clicked = ImGui::Button(label, {width, 0});
    ImGui::PopStyleColor(3);
    return clicked;
}

bool HToolbarButton(const char *label) {
    ImGui::PushStyleColor(ImGuiCol_Button, kSurface);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kSurfaceHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kSurfaceAct);
    bool clicked = ImGui::Button(label);
    ImGui::PopStyleColor(3);
    return clicked;
}

bool HBeginCombo(const char *id, const char *preview, float width) {
    ImGui::SetNextItemWidth(width);
    // Distinct surface for the popup so it doesn't blend with the panel bg.
    ImGui::PushStyleColor(ImGuiCol_PopupBg, kSurface);
    // Inset the hover bg from the popup border via WindowPadding.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    bool open = ImGui::BeginCombo(id, preview);
    if (!open) {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
    return open;
}

void HEndCombo() {
    ImGui::EndCombo();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

bool HBeginStyledPopup(const char *id) {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, kSurface);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
    bool open = ImGui::BeginPopup(id);
    if (!open) {
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
    return open;
}

void HEndStyledPopup() {
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

bool HComboItem(const char *label, bool selected) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 textSz = ImGui::CalcTextSize(label);
    float padX = 6.0f;
    float rowH = textSz.y + 12.0f;
    // Auto-sizing popups can report 0 content width on the first frame,
    // which would assert inside InvisibleButton. Fall back to text width.
    float fullW = ImGui::GetContentRegionAvail().x;
    float minW  = textSz.x + padX * 2;
    if (fullW < minW) fullW = minW;
    ImVec2 cursor = ImGui::GetCursorScreenPos();

    // Popup window bounds — bg extends to these even though the row sits inside WindowPadding.
    ImVec2 winPos = ImGui::GetWindowPos();
    float bgX0 = winPos.x;
    float bgX1 = winPos.x + ImGui::GetWindowSize().x;

    ImGui::PushID(label);
    ImGui::InvisibleButton("##itm", ImVec2(fullW, rowH));
    bool hovered = ImGui::IsItemHovered();
    bool held    = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    if (selected || hovered) {
        ImU32 bg = ImGui::GetColorU32(
            held    ? ImGuiCol_HeaderActive  :
            hovered ? ImGuiCol_HeaderHovered :
                      ImGuiCol_Header);
        dl->AddRectFilled({bgX0, cursor.y}, {bgX1, cursor.y + rowH}, bg);
    }

    dl->AddText({cursor.x + padX, cursor.y + (rowH - textSz.y) * 0.5f},
                ImGui::GetColorU32(ImGuiCol_Text), label);

    if (clicked) ImGui::CloseCurrentPopup();
    return clicked;
}

void HCombo(const char *label, const char *id, int *val, const char *items[], int count) {
    ImGui::Text("%s", label);
    ImGui::SameLine(kLabelCol);
    const char *preview = (*val >= 0 && *val < count) ? items[*val] : "";
    if (HBeginCombo(id, preview)) {
        for (int i = 0; i < count; ++i) {
            if (HComboItem(items[i], i == *val)) *val = i;
        }
        HEndCombo();
    }
}

void HDragFloat(const char *label, const char *id, float *val,
                float speed, float lo, float hi) {
    ImGui::Text("%s", label);
    ImGui::SameLine(kLabelCol);
    ImGui::SetNextItemWidth(-kScrollGap);
    ImGui::DragFloat(id, val, speed, lo, hi);
}

void HCheckbox(const char *label, const char *id, bool *val) {
    ImGui::Text("%s", label);
    ImGui::SameLine(kLabelCol);
    ImGui::Checkbox(id, val);
}

void AxisDotInput(const char *letter, const char *inputId, float *val,
                  ImU32 dotColor, float totalW) {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    float dotR = 5.0f;
    float rounding = ImGui::GetStyle().FrameRounding;
    ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(kBorder);

    dl->AddRectFilled(cp, {cp.x + totalW, cp.y + h},
                      ImGui::ColorConvertFloat4ToU32(kSurface), rounding);
    dl->AddRect(cp, {cp.x + totalW, cp.y + h}, borderCol, rounding);

    float leftPad = 8.0f;
    dl->AddCircleFilled({cp.x + leftPad + dotR, cp.y + h * 0.5f}, dotR, dotColor);

    ImVec2 letterSz = ImGui::CalcTextSize(letter);
    dl->AddText({cp.x + leftPad + dotR * 2 + 4, cp.y + (h - letterSz.y) * 0.5f},
                ImGui::ColorConvertFloat4ToU32(kTextDim), letter);

    char valBuf[32];
    std::snprintf(valBuf, sizeof(valBuf), "%.1f", *val);
    ImVec2 valSz = ImGui::CalcTextSize(valBuf);
    float rightPad = 8.0f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, {0,0,0,0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, {0,0,0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushItemWidth(totalW);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
        {totalW - valSz.x - rightPad, ImGui::GetStyle().FramePadding.y});
    ImGui::InputFloat(inputId, val, 0, 0, "%.1f");
    ImGui::PopStyleVar(2);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 8);
}
