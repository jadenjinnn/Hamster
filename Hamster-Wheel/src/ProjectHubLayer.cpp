//
// Created by Jaden on 01/09/2024.
//

#include "ProjectHubLayer.h"

void ProjectHubLayer::OnAttach() {
  Hamster::Application::GetApplicationInstance()
      .GetEventDispatcher()
      ->Subscribe(Hamster::ProjectOpened, [this](Hamster::Event &e) {
        m_EditorLayer = new EditorLayer(
            m_Dispatcher,
            Hamster::Application::GetApplicationInstance().GetActiveScene());

        Hamster::Application::GetApplicationInstance().PushLayer(m_EditorLayer);
        Hamster::Application::GetApplicationInstance().PopLayer(this);
      });
}

void ProjectHubLayer::OnImGuiUpdate() {
  Hamster::Renderer::Clear();

  m_ProjectSelector->Render();
  m_ProjectCreator->Render();
}
