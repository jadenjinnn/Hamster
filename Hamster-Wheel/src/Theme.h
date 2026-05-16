#ifndef UIPROTO_THEME_H
#define UIPROTO_THEME_H

#include <imgui.h>
#include <string>

// ─── Color Palette ───────────────────────────────────────────────
inline constexpr ImVec4 kGap        = {0.039f, 0.043f, 0.059f, 1.0f};
inline constexpr ImVec4 kPanel      = {0.039f, 0.043f, 0.059f, 1.0f};
inline constexpr ImVec4 kBorder     = {0.160f, 0.170f, 0.210f, 1.0f};
inline constexpr ImVec4 kCanvas     = {0.055f, 0.063f, 0.086f, 1.0f};
inline constexpr ImVec4 kSurface    = {0.055f, 0.063f, 0.086f, 1.0f};
inline constexpr ImVec4 kSurfaceHov = {0.100f, 0.110f, 0.140f, 1.0f};
inline constexpr ImVec4 kSurfaceAct = {0.125f, 0.135f, 0.170f, 1.0f};
inline constexpr ImVec4 kText       = {0.894f, 0.902f, 0.922f, 1.0f};
inline constexpr ImVec4 kTextDim    = {0.541f, 0.565f, 0.612f, 1.0f};
inline constexpr ImVec4 kAccent     = {0.300f, 0.520f, 0.960f, 1.0f};
inline constexpr ImVec4 kAccentDim  = {0.300f, 0.520f, 0.960f, 0.30f};
inline constexpr ImVec4 kHeader     = {0.094f, 0.102f, 0.133f, 1.0f};
inline constexpr ImVec4 kGreen      = {0.133f, 0.773f, 0.369f, 1.0f};
inline constexpr ImVec4 kRed        = {0.850f, 0.250f, 0.250f, 1.0f};
inline constexpr ImVec4 kHubGreen    = {0.24f, 0.75f, 0.39f, 1.0f};
inline constexpr ImVec4 kHubGreenHov = {0.30f, 0.82f, 0.45f, 1.0f};
inline constexpr ImVec4 kHubGreenAct = {0.20f, 0.65f, 0.33f, 1.0f};

// ─── Fonts ───────────────────────────────────────────────────────
extern ImFont *g_Font;
extern ImFont *g_BoldFont;
extern ImFont *g_HeaderFont;
extern ImFont *g_IconLarge;

// ─── Functions ───────────────────────────────────────────────────
void ApplyTheme();
void LoadFonts(ImGuiIO &io, const std::string &resourcePath);

#endif // UIPROTO_THEME_H
