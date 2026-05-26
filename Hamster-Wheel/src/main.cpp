#include <Hamster.h>
#include <Core/Application.h>
#include <imgui.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "Theme.h"
#include "ProjectHubLayer.h"
#include "EditorLayer.h"

int main() {
    // The shipped build has no console (/SUBSYSTEM:WINDOWS), so route stdout/
    // stderr to %APPDATA%/Hamster/log.txt — otherwise all diagnostics vanish.
    // static: the file buffer must outlive every std::cout/cerr use, including
    // during static destruction at process exit.
    static std::ofstream s_LogFile;
    if (const char *appData = std::getenv("APPDATA")) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Hamster";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        s_LogFile.open((dir / "log.txt").string(), std::ios::trunc);
        if (s_LogFile.is_open()) {
            std::cout.rdbuf(s_LogFile.rdbuf());
            std::cerr.rdbuf(s_LogFile.rdbuf());
        }
    }

    Hamster::WindowProps props(768.0f, 1376.0f, "Hamster",
                               /*borderless*/ true, /*maximized*/ true);
    auto *app = new Hamster::Application(props);

    const std::string resourcePath = Hamster::Application::GetExecutablePath() +
        "/../share/Resources/Hamster-Wheel/Resources";

    ImGuiIO &io = ImGui::GetIO();
    // Keyboard nav left disabled — Alt activating menu nav while a modal
    // popup is open causes a delayed crash inside the modal's Render
    // a few frames after the keypress.
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
