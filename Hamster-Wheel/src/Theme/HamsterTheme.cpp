#include "HamsterTheme.h"
#include "IconsFontAwesome6.h"

#include <filesystem>
#include <iostream>

namespace HamsterTheme {

// --- Palette ---
// Figma-inspired muted dark. Low-contrast backgrounds, subtle borders, sparse accent.
static constexpr ImVec4 kBg          = {0.114f, 0.114f, 0.129f, 1.0f};   // #1d1d21
static constexpr ImVec4 kBgChild     = {0.131f, 0.131f, 0.149f, 1.0f};   // #212126
static constexpr ImVec4 kSurface     = {0.161f, 0.161f, 0.180f, 1.0f};   // #29292e
static constexpr ImVec4 kSurfaceHov  = {0.196f, 0.196f, 0.216f, 1.0f};   // #323237
static constexpr ImVec4 kSurfaceAct  = {0.220f, 0.220f, 0.243f, 1.0f};   // #38383e
static constexpr ImVec4 kBorder      = {0.220f, 0.220f, 0.243f, 1.0f};   // #38383e
static constexpr ImVec4 kText        = {0.933f, 0.933f, 0.949f, 1.0f};   // #eeeeee
static constexpr ImVec4 kTextDim     = {0.533f, 0.533f, 0.573f, 1.0f};   // #888892
static constexpr ImVec4 kAccent      = {0.345f, 0.529f, 0.969f, 1.0f};   // #5887f7  — blue
static constexpr ImVec4 kAccentHov   = {0.435f, 0.600f, 1.000f, 1.0f};
static constexpr ImVec4 kAccentAct   = {0.278f, 0.455f, 0.878f, 1.0f};
static constexpr ImVec4 kScrollbar   = {0.0f, 0.0f, 0.0f, 0.0f};
static constexpr ImVec4 kScrollGrab  = {0.300f, 0.300f, 0.330f, 0.60f};
static constexpr ImVec4 kScrollHov   = {0.350f, 0.350f, 0.380f, 0.75f};
static constexpr ImVec4 kScrollAct   = {0.400f, 0.400f, 0.430f, 0.85f};

static void ApplyColors() {
    ImVec4 *c = ImGui::GetStyle().Colors;

    c[ImGuiCol_Text]                  = kText;
    c[ImGuiCol_TextDisabled]          = kTextDim;
    c[ImGuiCol_WindowBg]              = kBg;
    c[ImGuiCol_ChildBg]               = kBgChild;
    c[ImGuiCol_PopupBg]               = kBgChild;
    c[ImGuiCol_Border]                = kBorder;
    c[ImGuiCol_BorderShadow]          = {0, 0, 0, 0};

    c[ImGuiCol_FrameBg]               = kSurface;
    c[ImGuiCol_FrameBgHovered]        = kSurfaceHov;
    c[ImGuiCol_FrameBgActive]         = kSurfaceAct;

    c[ImGuiCol_TitleBg]               = kBg;
    c[ImGuiCol_TitleBgActive]         = kBg;
    c[ImGuiCol_TitleBgCollapsed]      = kBg;
    c[ImGuiCol_MenuBarBg]             = kBg;

    c[ImGuiCol_ScrollbarBg]           = kScrollbar;
    c[ImGuiCol_ScrollbarGrab]         = kScrollGrab;
    c[ImGuiCol_ScrollbarGrabHovered]  = kScrollHov;
    c[ImGuiCol_ScrollbarGrabActive]   = kScrollAct;

    c[ImGuiCol_CheckMark]             = kAccent;
    c[ImGuiCol_SliderGrab]            = kAccent;
    c[ImGuiCol_SliderGrabActive]      = kAccentAct;

    c[ImGuiCol_Button]                = kSurface;
    c[ImGuiCol_ButtonHovered]         = kSurfaceHov;
    c[ImGuiCol_ButtonActive]          = kSurfaceAct;

    c[ImGuiCol_Header]                = kSurface;
    c[ImGuiCol_HeaderHovered]         = kSurfaceHov;
    c[ImGuiCol_HeaderActive]          = kSurfaceAct;

    c[ImGuiCol_Separator]             = kBorder;
    c[ImGuiCol_SeparatorHovered]      = kAccent;
    c[ImGuiCol_SeparatorActive]       = kAccentAct;

    c[ImGuiCol_ResizeGrip]            = kSurface;
    c[ImGuiCol_ResizeGripHovered]     = kAccent;
    c[ImGuiCol_ResizeGripActive]      = kAccentAct;

    c[ImGuiCol_Tab]                       = kBg;
    c[ImGuiCol_TabHovered]                = kSurfaceAct;
    c[ImGuiCol_TabSelected]               = kSurface;
    c[ImGuiCol_TabSelectedOverline]       = {0, 0, 0, 0};
    c[ImGuiCol_TabDimmed]                 = kBg;
    c[ImGuiCol_TabDimmedSelected]         = kSurface;
    c[ImGuiCol_TabDimmedSelectedOverline] = {0, 0, 0, 0};

    c[ImGuiCol_DockingPreview]        = kAccent;
    c[ImGuiCol_DockingEmptyBg]        = kBg;

    c[ImGuiCol_PlotLines]             = kTextDim;
    c[ImGuiCol_PlotLinesHovered]      = kAccent;
    c[ImGuiCol_PlotHistogram]         = kAccent;
    c[ImGuiCol_PlotHistogramHovered]  = kAccentHov;

    c[ImGuiCol_TextSelectedBg]        = {kAccent.x, kAccent.y, kAccent.z, 0.30f};
    c[ImGuiCol_DragDropTarget]        = kAccent;
    c[ImGuiCol_NavHighlight]          = kAccent;
    c[ImGuiCol_NavWindowingHighlight] = {1, 1, 1, 0.70f};
    c[ImGuiCol_NavWindowingDimBg]     = {0.2f, 0.2f, 0.2f, 0.20f};
    c[ImGuiCol_ModalWindowDimBg]      = {0, 0, 0, 0.55f};

    c[ImGuiCol_TableHeaderBg]         = kSurface;
    c[ImGuiCol_TableBorderStrong]     = kBorder;
    c[ImGuiCol_TableBorderLight]      = {kBorder.x, kBorder.y, kBorder.z, 0.5f};
    c[ImGuiCol_TableRowBg]            = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt]         = {1, 1, 1, 0.02f};
}

static void ApplyStyle() {
    ImGuiStyle &s = ImGui::GetStyle();

    s.WindowPadding     = {12, 12};
    s.FramePadding      = {8, 4};
    s.CellPadding       = {8, 4};
    s.ItemSpacing       = {8, 4};
    s.ItemInnerSpacing  = {4, 4};
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 6.0f;

    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    s.DockingSeparatorSize = 1.0f;

    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 99.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 6.0f;
}

static ImFont *s_BoldFont = nullptr;
static ImFont *s_HeaderFont = nullptr;
static ImFont *s_TitleFont = nullptr;

static void MergeIcons(ImGuiIO &io, const std::string &iconPath, float size) {
    if (!std::filesystem::exists(iconPath)) return;
    static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    ImFontConfig iconCfg;
    iconCfg.MergeMode = true;
    iconCfg.PixelSnapH = true;
    iconCfg.GlyphMinAdvanceX = size;
    io.Fonts->AddFontFromFileTTF(iconPath.c_str(), size - 1.0f, &iconCfg, iconRanges);
}

static void LoadFonts(ImGuiIO &io, const std::string &resourcePath) {
    std::string fontsDir = resourcePath + "/Fonts/";
    std::string interRegular = fontsDir + "Inter-Regular.ttf";
    std::string interBold = fontsDir + "Inter-Bold.ttf";
    std::string iconPath = fontsDir + "fa-solid-900.ttf";

    // Regular 16px (default font)
    if (std::filesystem::exists(interRegular)) {
        io.Fonts->AddFontFromFileTTF(interRegular.c_str(), 16.0f);
    } else {
        std::cerr << "[HamsterTheme] Inter-Regular.ttf not found, falling back to default\n";
        io.Fonts->AddFontDefault();
    }
    MergeIcons(io, iconPath, 16.0f);

    // Bold 16px — for section headers, labels
    if (std::filesystem::exists(interBold)) {
        s_BoldFont = io.Fonts->AddFontFromFileTTF(interBold.c_str(), 16.0f);
        MergeIcons(io, iconPath, 16.0f);
    }

    // Bold 18px — for panel/component headers
    if (std::filesystem::exists(interBold)) {
        s_HeaderFont = io.Fonts->AddFontFromFileTTF(interBold.c_str(), 18.0f);
        MergeIcons(io, iconPath, 18.0f);
    }

    // Bold 28px — for page titles (project hub "Projects" heading)
    if (std::filesystem::exists(interBold)) {
        s_TitleFont = io.Fonts->AddFontFromFileTTF(interBold.c_str(), 28.0f);
        MergeIcons(io, iconPath, 28.0f);
    }
}

void Apply(ImGuiIO &io, const std::string &resourcePath) {
    ApplyColors();
    ApplyStyle();
    LoadFonts(io, resourcePath);
}

ImFont *GetBoldFont() { return s_BoldFont; }
ImFont *GetHeaderFont() { return s_HeaderFont; }
ImFont *GetTitleFont() { return s_TitleFont; }

} // namespace HamsterTheme
