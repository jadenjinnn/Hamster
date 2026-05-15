#include "Console.h"
#include "../Theme.h"

#include <imgui.h>

Console::Console(Hamster::EventDispatcher *dispatcher,
                 std::shared_ptr<Hamster::Scene> scene)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)) {
    if (m_Scene) m_ClientLogger = m_Scene->GetClientLogger();
    m_Dispatcher->Subscribe(
        Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(Console::OnActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));
}

void Console::OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
    if (m_Scene) m_ClientLogger = m_Scene->GetClientLogger();
}

void Console::Render() {
    if (!m_ClientLogger) {
        ImGui::TextDisabled("No active scene");
        return;
    }

    size_t index = m_ClientLogger->GetTailIndex();
    size_t bufSize = m_ClientLogger->GetBufferSize();
    const std::vector<Hamster::LogEntry> &buffer = m_ClientLogger->GetBuffer();

    for (size_t i = 0; i < bufSize; i++) {
        const std::string &mes = buffer[index].message;
        Hamster::LogType type = buffer[index].type;

        switch (type) {
        case Hamster::Info:
            ImGui::PushStyleColor(ImGuiCol_Text, kText);
            break;
        case Hamster::Error:
            ImGui::PushStyleColor(ImGuiCol_Text, kRed);
            break;
        case Hamster::Warning:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.72f, 0.29f, 1.0f));
            break;
        }

        if (!mes.empty()) ImGui::TextUnformatted(mes.c_str());
        ImGui::PopStyleColor();

        index = (index + 1) % bufSize;
    }
}
