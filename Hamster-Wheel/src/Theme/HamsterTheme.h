#ifndef HAMSTER_THEME_H
#define HAMSTER_THEME_H

#include <imgui.h>
#include <string>

namespace HamsterTheme {

void Apply(ImGuiIO &io, const std::string &resourcePath);

ImFont *GetBoldFont();
ImFont *GetHeaderFont();
ImFont *GetTitleFont();

} // namespace HamsterTheme

#endif // HAMSTER_THEME_H
