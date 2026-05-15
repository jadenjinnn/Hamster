#include "Theme.h"
#include "IconsFontAwesome6.h"

#include <filesystem>
#include <string>

ImFont *g_Font       = nullptr;
ImFont *g_BoldFont   = nullptr;
ImFont *g_HeaderFont = nullptr;
ImFont *g_IconLarge  = nullptr;

void ApplyTheme() {
    ImVec4 *c = ImGui::GetStyle().Colors;
    ImGuiStyle &s = ImGui::GetStyle();

    c[ImGuiCol_Text]                 = kText;
    c[ImGuiCol_TextDisabled]         = kTextDim;
    c[ImGuiCol_WindowBg]             = kPanel;
    c[ImGuiCol_ChildBg]              = {0,0,0,0};
    c[ImGuiCol_PopupBg]              = kPanel;
    c[ImGuiCol_Border]               = kBorder;
    c[ImGuiCol_BorderShadow]         = {0,0,0,0};
    c[ImGuiCol_FrameBg]              = kSurface;
    c[ImGuiCol_FrameBgHovered]       = kSurfaceHov;
    c[ImGuiCol_FrameBgActive]        = kSurfaceAct;
    c[ImGuiCol_TitleBg]              = kPanel;
    c[ImGuiCol_TitleBgActive]        = kPanel;
    c[ImGuiCol_TitleBgCollapsed]     = kPanel;
    c[ImGuiCol_MenuBarBg]            = kPanel;
    c[ImGuiCol_ScrollbarBg]          = {0,0,0,0};
    c[ImGuiCol_ScrollbarGrab]        = {0.30f,0.32f,0.36f,0.40f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.35f,0.37f,0.41f,0.60f};
    c[ImGuiCol_ScrollbarGrabActive]  = {0.40f,0.42f,0.46f,0.80f};
    c[ImGuiCol_CheckMark]            = kAccent;
    c[ImGuiCol_SliderGrab]           = kAccent;
    c[ImGuiCol_SliderGrabActive]     = kAccent;
    c[ImGuiCol_Button]               = kSurface;
    c[ImGuiCol_ButtonHovered]        = kSurfaceHov;
    c[ImGuiCol_ButtonActive]         = kSurfaceAct;
    c[ImGuiCol_Header]               = kSurface;
    c[ImGuiCol_HeaderHovered]        = kSurfaceHov;
    c[ImGuiCol_HeaderActive]         = kSurfaceAct;
    c[ImGuiCol_Separator]            = {0.20f,0.22f,0.26f,1.0f};
    c[ImGuiCol_Tab]                  = kPanel;
    c[ImGuiCol_TabHovered]           = kSurfaceHov;
    c[ImGuiCol_TabSelected]          = kPanel;
    c[ImGuiCol_TabSelectedOverline]  = kAccent;
    c[ImGuiCol_ModalWindowDimBg]     = {0,0,0,0.55f};

    s.WindowPadding     = {16, 32};
    s.FramePadding      = {8, 5};
    s.CellPadding       = {6, 3};
    s.ItemSpacing       = {8, 6};
    s.ItemInnerSpacing  = {4, 4};
    s.IndentSpacing     = 20.0f;
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 6.0f;

    s.WindowBorderSize  = 1.0f;
    s.ChildBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.FrameBorderSize   = 1.0f;

    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 99.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 0.0f;
}

void LoadFonts(ImGuiIO &io, const std::string &resourcePath) {
    std::string dir = resourcePath + "/Fonts/";
    std::string regular  = dir + "Inter-Regular.ttf";
    std::string semibold = dir + "Inter-SemiBold.ttf";
    std::string icons    = dir + "fa-solid-900.ttf";

    auto mergeIcons = [&](float sz) {
        if (!std::filesystem::exists(icons)) return;
        static const ImWchar ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.GlyphMinAdvanceX = sz;
        io.Fonts->AddFontFromFileTTF(icons.c_str(), sz - 1.0f, &cfg, ranges);
    };

    if (std::filesystem::exists(regular))
        g_Font = io.Fonts->AddFontFromFileTTF(regular.c_str(), 15.0f);
    else
        g_Font = io.Fonts->AddFontDefault();
    mergeIcons(15.0f);

    if (std::filesystem::exists(semibold)) {
        g_BoldFont = io.Fonts->AddFontFromFileTTF(semibold.c_str(), 15.0f);
        mergeIcons(15.0f);
        g_HeaderFont = io.Fonts->AddFontFromFileTTF(semibold.c_str(), 17.0f);
        mergeIcons(17.0f);
    }

    if (std::filesystem::exists(icons)) {
        static const ImWchar ranges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
        ImFontConfig cfg;
        cfg.PixelSnapH = true;
        cfg.GlyphMinAdvanceX = 36.0f;
        g_IconLarge = io.Fonts->AddFontFromFileTTF(icons.c_str(), 36.0f, &cfg, ranges);
    }
}
