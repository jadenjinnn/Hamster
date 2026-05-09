#include <filesystem>
#include <fstream>
#include <iostream>

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

  auto scene = std::make_shared<Hamster::Scene>();
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

  if (createOk && updateOk) {
    std::cout << "PASS: on_create and on_update both fired" << std::endl;
    return 0;
  }

  std::cerr << "FAIL:";
  if (!createOk) std::cerr << " on_create did not fire";
  if (!updateOk) std::cerr << " on_update did not fire";
  std::cerr << std::endl;
  return 1;
}
