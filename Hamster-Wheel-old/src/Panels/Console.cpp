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
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.933f, 0.933f, 0.949f, 1.0f));
      break;
    case Hamster::Error:
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.878f, 0.325f, 0.345f, 1.0f));
      break;
    case Hamster::Warning:
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.941f, 0.725f, 0.290f, 1.0f));

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
