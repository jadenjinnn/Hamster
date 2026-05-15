#ifndef UIPROTO_PANEL_H
#define UIPROTO_PANEL_H

#include <imgui.h>

inline constexpr float kHeaderH    = 36.0f;
inline constexpr float kPadX       = 16.0f;
inline constexpr float kContentGap = 8.0f;

inline const ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoFocusOnAppearing;

struct Panel {
    const char *name;
    const char *imguiId;
    bool selected   = false;
    bool scrollable = false;

    void DrawDotsButton() const;
    void DrawTabbedHeader(const char *tabs[], int count, int *selectedTab) const;
    void DrawHeader() const;
    void BeginContent() const;
    void EndContent() const;
};

#endif // UIPROTO_PANEL_H
