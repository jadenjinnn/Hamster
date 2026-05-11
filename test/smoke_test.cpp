#include <filesystem>
#include <fstream>
#include <iostream>

#include <box2d/box2d.h>
#include <glad/glad.h>
#include <pybind11/embed.h>

#include "Core/Application.h"
#include "Core/Components.h"
#include "Core/Scene.h"
#include "Scripting/HamsterScript.h"

int main() {
  std::filesystem::path fixtureDir = SMOKE_TEST_FIXTURE_DIR;
  std::filesystem::path markerDir =
      std::filesystem::temp_directory_path() / "hamster_smoke";

  std::filesystem::create_directories(markerDir);
  std::filesystem::remove(markerDir / "smoke_create.ok");
  std::filesystem::remove(markerDir / "smoke_update.ok");

  // Python script reads this to know where to write marker files
  _putenv_s("HAMSTER_TEST_MARKER_DIR", markerDir.string().c_str());

  Hamster::Application app;

  // Add fixture dir to sys.path so Python can find smoke_script and Hamster.pyd
  pybind11::module_ sys = pybind11::module_::import("sys");
  pybind11::list path = sys.attr("path");
  path.append(fixtureDir.string());

  auto scene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);
  Hamster::UUID entityId = scene->CreateEntity();
  scene->AddEntityComponent<Hamster::Behaviour>(entityId);

  auto script = std::make_shared<Hamster::HamsterScript>(
      fixtureDir / "smoke_script.py", "smoke_script");

  auto &behaviour = scene->GetEntityComponent<Hamster::Behaviour>(entityId);
  behaviour.scripts[script->GetUUID()] = script;

  app.AddScene(scene);
  app.SetSceneActive(scene->GetUUID());
  scene->RunScene();

  // Instantiates Python objects, calls on_create
  scene->RunSceneSimulation();

  // Runs one frame: calls on_update on all Python behaviours
  scene->OnUpdate();

  bool createOk = std::filesystem::exists(markerDir / "smoke_create.ok");
  bool updateOk = std::filesystem::exists(markerDir / "smoke_update.ok");

  std::filesystem::remove_all(markerDir);

  if (!createOk || !updateOk) {
    std::cerr << "FAIL:";
    if (!createOk) std::cerr << " on_create did not fire";
    if (!updateOk) std::cerr << " on_update did not fire";
    std::cerr << std::endl;
    return 1;
  }

  std::cout << "PASS: on_create and on_update both fired" << std::endl;

  // --- Box2D physics test ---
  // Create a second scene with two entities: one dynamic (should fall), one static (floor)
  auto physScene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);

  Hamster::UUID dynamicId = physScene->CreateEntity();
  auto &dynTransform = physScene->GetEntityComponent<Hamster::Transform>(dynamicId);
  dynTransform.position = {0.0f, 0.0f, 0.0f};
  dynTransform.size = {50.0f, 50.0f};

  Hamster::Rigidbody dynRb;
  dynRb.bodyType = Hamster::BodyType::Dynamic;
  dynRb.density = 1.0f;
  physScene->AddEntityComponent<Hamster::Rigidbody>(dynamicId, dynRb);

  Hamster::UUID staticId = physScene->CreateEntity();
  auto &staticTransform = physScene->GetEntityComponent<Hamster::Transform>(staticId);
  staticTransform.position = {0.0f, 500.0f, 0.0f};
  staticTransform.size = {200.0f, 50.0f};

  Hamster::Rigidbody staticRb;
  staticRb.bodyType = Hamster::BodyType::Static;
  physScene->AddEntityComponent<Hamster::Rigidbody>(staticId, staticRb);

  app.AddScene(physScene);
  app.SetSceneActive(physScene->GetUUID());
  physScene->RunScene();
  physScene->RunSceneSimulation();

  float startY = physScene->GetEntityComponent<Hamster::Transform>(dynamicId).position.y;

  // Step a few frames — gravity should pull the dynamic entity down (+Y is down)
  for (int i = 0; i < 10; i++) {
    physScene->OnUpdate();
  }

  float endY = physScene->GetEntityComponent<Hamster::Transform>(dynamicId).position.y;

  if (endY <= startY) {
    std::cerr << "FAIL: dynamic entity did not fall (y: " << startY << " -> " << endY << ")" << std::endl;
    return 1;
  }

  std::cout << "PASS: Box2D gravity applied (y: " << startY << " -> " << endY << ")" << std::endl;

  // --- Python apply_force test ---
  // Create a third scene with a dynamic entity running force_script.py
  auto forceScene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);

  Hamster::UUID forceEntityId = forceScene->CreateEntity();
  auto &forceTransform = forceScene->GetEntityComponent<Hamster::Transform>(forceEntityId);
  forceTransform.position = {0.0f, 0.0f, 0.0f};
  forceTransform.size = {50.0f, 50.0f};

  Hamster::Rigidbody forceRb;
  forceRb.bodyType = Hamster::BodyType::Dynamic;
  forceRb.density = 1.0f;
  forceRb.gravityScale = 0.0f; // no gravity so we can isolate force effect
  forceScene->AddEntityComponent<Hamster::Rigidbody>(forceEntityId, forceRb);

  forceScene->AddEntityComponent<Hamster::Behaviour>(forceEntityId);
  auto forceScript = std::make_shared<Hamster::HamsterScript>(
      fixtureDir / "force_script.py", "force_script");
  auto &forceBehaviour = forceScene->GetEntityComponent<Hamster::Behaviour>(forceEntityId);
  forceBehaviour.scripts[forceScript->GetUUID()] = forceScript;

  app.AddScene(forceScene);
  app.SetSceneActive(forceScene->GetUUID());
  forceScene->RunScene();

  std::filesystem::remove(markerDir / "force_update.ok");
  std::filesystem::create_directories(markerDir);

  forceScene->RunSceneSimulation();

  // Step a few frames — force_script calls self.apply_force(500, 0) each frame
  for (int i = 0; i < 5; i++) {
    forceScene->OnUpdate();
  }

  bool forceOk = std::filesystem::exists(markerDir / "force_update.ok");
  float forceEndX = forceScene->GetEntityComponent<Hamster::Transform>(forceEntityId).position.x;

  std::filesystem::remove_all(markerDir);

  if (!forceOk) {
    std::cerr << "FAIL: force_script on_update did not fire (crashed in apply_force?)" << std::endl;
    return 1;
  }

  std::cout << "PASS: Python apply_force executed (x moved to " << forceEndX << ")" << std::endl;
  return 0;
}
