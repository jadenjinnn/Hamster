#include "Console.h"

#include <imgui.h>

Console::Console(Hamster::EventDispatcher *dispatcher,
                 std::shared_ptr<Hamster::Scene> scene)
    : Panel(dispatcher, std::move(scene), true) {
  m_ClientLogger = m_Scene->GetClientLogger();
};

void Console::Render() {
  if (!ImGui::Begin("Console", &m_WindowOpen)) {
    ImGui::End();

    return;
  }

  size_t index = m_ClientLogger->GetTailIndex();
  size_t bufSize = m_ClientLogger->GetBufferSize();
  const std::vector<Hamster::LogEntry> &buffer = m_ClientLogger->GetBuffer();

  for (int i = 0; i < bufSize; i++) {
    std::string mes = buffer[index].message;
    Hamster::LogType type = buffer[index].type;

    switch (type) {
    case Hamster::Info:
      ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(248, 253, 255));

      break;
    case Hamster::Error:
      ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(230, 57, 70));

      break;
    case Hamster::Warning:
      ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(251, 177, 60));

      break;
    }

    if (!mes.empty()) {
      ImGui::TextUnformatted(mes.c_str());
    }

    ImGui::PopStyleColor();

    index = (index + 1) % bufSize;
  }

  ImGui::End();
}
