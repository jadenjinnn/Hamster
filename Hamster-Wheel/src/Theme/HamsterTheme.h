#ifndef HAMSTER_THEME_H
#define HAMSTER_THEME_H

#include <imgui.h>
#include <string>

namespace HamsterTheme {

void Apply(ImGuiIO &io, const std::string &resourcePath);

} // namespace HamsterTheme

#endif // HAMSTER_THEME_H
