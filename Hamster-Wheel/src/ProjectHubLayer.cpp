//
// Created by Jaden on 01/09/2024.
//

#include "ProjectHubLayer.h"

ProjectHubLayer::ProjectHubLayer(Hamster::Application *app)
    : m_App(app),
      m_Dispatcher(app->GetEventDispatcher().get()),
      m_ProjectSelector(std::make_unique<ProjectSelector>(m_Dispatcher)),
      m_ProjectCreator(std::make_unique<ProjectCreator>(m_Dispatcher)) {
}

void ProjectHubLayer::OnAttach() {
  m_Dispatcher->Subscribe(Hamster::ProjectOpened, [this](Hamster::Event &e) {
        m_EditorLayer = new EditorLayer(
            m_App,
            m_App->GetActiveScene());

        m_App->PushLayer(m_EditorLayer);
        m_App->PopLayer(this);
      });
}

void ProjectHubLayer::OnImGuiUpdate() {
  m_App->GetRenderer()->Clear();

  m_ProjectSelector->Render();
  m_ProjectCreator->Render();
}
