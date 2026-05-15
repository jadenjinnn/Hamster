#include <Hamster.h>
#include <Core/Application.h>
#include <imgui.h>

#include "Theme.h"
#include "ProjectHubLayer.h"

int main() {
    Hamster::WindowProps props(768.0f, 1376.0f, "Hamster",
                               /*borderless*/ true, /*maximized*/ true);
    auto *app = new Hamster::Application(props);

    const std::string resourcePath = Hamster::Application::GetExecutablePath() +
        "/../share/Resources/Hamster-Wheel/Resources";

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    LoadFonts(io, resourcePath);
    ApplyTheme();

    // Start in the project hub — selecting / opening a project swaps in the
    // EditorLayer via the ProjectOpened event.
    app->PushLayer(new ProjectHubLayer(app));
    app->Run();

    delete app;
    return 0;
}
