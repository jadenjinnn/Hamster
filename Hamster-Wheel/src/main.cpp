#include <Hamster.h>
#include <Core/Application.h>
#include <imgui.h>

#include "Theme.h"
#include "ProjectHubLayer.h"
#include "EditorLayer.h"

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

    // Start in the project hub. ProjectOpened swaps the current main layer
    // for a fresh EditorLayer — pop+delete the previous one so we never
    // accumulate stale layers on each project switch (bug 0002).
    Hamster::Layer *mainLayer = new ProjectHubLayer(app);
    app->PushLayer(mainLayer);

    app->GetEventDispatcher()->Subscribe(
        Hamster::ProjectOpened, [app, &mainLayer](Hamster::Event &) {
            app->PopLayer(mainLayer);
            mainLayer = new EditorLayer(app);
            app->PushLayer(mainLayer);
        });

    app->Run();

    delete app;
    return 0;
}
