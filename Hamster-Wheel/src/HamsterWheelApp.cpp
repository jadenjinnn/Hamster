//
// Created by Jaden on 24/08/2024.
//

#include <Hamster.h>

#include <Core/Scene.h>
#include <Core/SceneSerialiser.h>
#include <Core/UUID.h>
#include <EditorLayer.h>
#include <Physics/Physics.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "ProjectHubLayer.h"

int main() {
  auto *app = new Hamster::Application();

  // pybind11::hash()

  std::filesystem::path filePath =
      Hamster::Application::GetExecutablePath() +
      "/../share/Resources/Hamster-Wheel/Resources/Fonts/segoe-ui-this/"
      "segoeuithis.ttf";

  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->AddFontFromFileTTF(filePath.string().c_str(), 18.0f);

  ProjectHubLayer *projectHubLayer = new ProjectHubLayer(
      app->GetEventDispatcher().get());

  app->PushLayer(projectHubLayer);

  // app->PushLayer(new EditorLayer(scene));

  // //
  // Hamster::UUID mario = scene->CreateEntity();
  // scene->AddEntityComponent<Hamster::Transform>(mario, glm::vec2(400.0f,
  // 0.0f), 0.0f, glm::vec2(300.0f, 300.0f));
  // scene->AddEntityComponent<Hamster::Sprite>(mario, "mario",
  // Hamster::AssetManager::GetTexture("mario"),
  //                                            glm::vec3(1.0f, 1.0f, 1.0f));
  // scene->AddEntityComponent<Hamster::Name>(mario, "Mario");
  // Hamster::Physics::CreateBody(scene->GetWorldId(), b2_dynamicBody,
  // scene->GetEntity(mario), scene->GetRegistry());

  std::cout << Hamster::Application::GetExecutablePath() << std::endl;

  app->Run();

  delete app;
}
