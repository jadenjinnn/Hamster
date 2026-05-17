#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include <box2d/box2d.h>
#include <glad/glad.h>
#include <pybind11/embed.h>

#include "Core/Application.h"
#include "Core/Components.h"
#include "Core/Scene.h"
#include "Core/SceneSerialiser.h"
#include "Scripting/HamsterScript.h"
#include "Utils/AssetManager.h"

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
  // Sleep briefly so glfwGetTime delta is non-zero between frames
  for (int i = 0; i < 10; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

  // --- Runtime entity creation/destruction test ---
  auto runtimeScene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);

  // Create a manager entity that will spawn entities via Python
  Hamster::UUID managerId = runtimeScene->CreateEntity();
  runtimeScene->AddEntityComponent<Hamster::Behaviour>(managerId);
  auto spawnScript = std::make_shared<Hamster::HamsterScript>(
      fixtureDir / "spawn_script.py", "spawn_script");
  auto &managerBehaviour = runtimeScene->GetEntityComponent<Hamster::Behaviour>(managerId);
  managerBehaviour.scripts[spawnScript->GetUUID()] = spawnScript;

  app.AddScene(runtimeScene);
  app.SetSceneActive(runtimeScene->GetUUID());
  runtimeScene->RunScene();

  std::filesystem::create_directories(markerDir);
  std::filesystem::remove(markerDir / "spawn_created.ok");
  std::filesystem::remove(markerDir / "spawn_destroyed.ok");

  runtimeScene->RunSceneSimulation();

  // on_create already ran — check entity was spawned
  bool spawnCreated = std::filesystem::exists(markerDir / "spawn_created.ok");
  uint32_t countAfterSpawn = runtimeScene->GetEntityCount();

  if (!spawnCreated) {
    std::cerr << "FAIL: spawn_script on_create did not run" << std::endl;
    return 1;
  }

  // Manager + spawned entity = 2
  if (countAfterSpawn < 2) {
    std::cerr << "FAIL: expected at least 2 entities after spawn, got " << countAfterSpawn << std::endl;
    return 1;
  }

  std::cout << "PASS: runtime entity created (count: " << countAfterSpawn << ")" << std::endl;

  // Frame 1: on_update runs, destroys the entity (deferred to end of frame)
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  runtimeScene->OnUpdate();

  bool spawnDestroyed = std::filesystem::exists(markerDir / "spawn_destroyed.ok");
  uint32_t countAfterDestroy = runtimeScene->GetEntityCount();

  if (!spawnDestroyed) {
    std::cerr << "FAIL: spawn_script on_update did not run" << std::endl;
    return 1;
  }

  // Back to 1 (just the manager)
  if (countAfterDestroy != 1) {
    std::cerr << "FAIL: expected 1 entity after destroy, got " << countAfterDestroy << std::endl;
    return 1;
  }

  std::cout << "PASS: runtime entity destroyed (count: " << countAfterDestroy << ")" << std::endl;

  std::filesystem::remove_all(markerDir);

  // --- Animation system test ---
  auto animScene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);

  auto *am = app.GetAssetManager();

  // Create two dummy textures with unique UUIDs (no GL data needed for pointer comparison)
  Hamster::UUID tex1UUID;
  Hamster::UUID tex2UUID;
  am->AddTexture(tex1UUID, "dummy1", "frame1");
  am->AddTexture(tex2UUID, "dummy2", "frame2");

  // Create animation data: 2 keyframes, frame1 at 0s, frame2 at 0.1s
  std::vector<Hamster::AnimationKeyframe> keyframes = {
      {0.0f, tex1UUID},
      {0.1f, tex2UUID}
  };
  Hamster::UUID animDataUUID = am->AddAnimation("TestWalk", keyframes);

  // Create entity with Sprite + Animation
  Hamster::UUID animEntityId = animScene->CreateEntity();
  animScene->AddEntityComponent<Hamster::Sprite>(animEntityId, glm::vec3(1.0f));
  auto &animEntitySprite = animScene->GetEntityComponent<Hamster::Sprite>(animEntityId);

  Hamster::Animation animComp;
  animComp.animations["Walk"] = animDataUUID;
  animComp.defaultAnimation = "";
  animComp.loop = false;
  animScene->AddEntityComponent<Hamster::Animation>(animEntityId, animComp);

  app.AddScene(animScene);
  app.SetSceneActive(animScene->GetUUID());
  animScene->RunScene();
  animScene->RunSceneSimulation();

  // Manually start the animation (simulating what Python's self.animate("Walk") does)
  auto &animC = animScene->GetEntityComponent<Hamster::Animation>(animEntityId);
  animC.currentAnimation = "Walk";
  animC.currentTime = 0.0f;
  animC.playing = true;
  animC.runtimeLoop = false;

  // Frame 1: should show frame1 texture (time starts at 0)
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  animScene->OnUpdate();

  auto &spriteAfter1 = animScene->GetEntityComponent<Hamster::Sprite>(animEntityId);
  // After first update, sprite should have a texture from the animation
  // (tex1 at time 0, or tex2 if enough time passed)

  // Step several frames to get past 0.1s
  for (int i = 0; i < 10; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    animScene->OnUpdate();
  }

  auto &animAfter = animScene->GetEntityComponent<Hamster::Animation>(animEntityId);
  auto &spriteAfter = animScene->GetEntityComponent<Hamster::Sprite>(animEntityId);

  // Non-looping animation should have stopped
  if (animAfter.playing) {
    std::cerr << "FAIL: non-looping animation still playing after duration elapsed" << std::endl;
    return 1;
  }

  // Sprite should be showing the last keyframe's texture (tex2)
  auto tex2FromAM = am->GetTexture(tex2UUID);
  if (spriteAfter.texture != tex2FromAM) {
    std::cerr << "FAIL: sprite texture not swapped to last keyframe" << std::endl;
    return 1;
  }

  // Check that AnimationCompleted event was posted (completedAnimations should have been populated)
  // Since we already ran OnScriptUpdate (which clears it), check that playing is false
  if (!animAfter.completedAnimations.empty()) {
    std::cerr << "FAIL: completedAnimations should be cleared after OnScriptUpdate" << std::endl;
    return 1;
  }

  std::cout << "PASS: animation played, stopped on last frame, sprite swapped correctly" << std::endl;

  // --- Entity hierarchy: parent/child + cycle refuse + cascade destroy ---
  {
    auto hScene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);

    Hamster::UUID a = hScene->CreateEntity();
    Hamster::UUID b = hScene->CreateEntity();
    Hamster::UUID c = hScene->CreateEntity();

    // Build A → B → C tree.
    if (!hScene->SetParent(b, a) || !hScene->SetParent(c, b)) {
      std::cerr << "FAIL: SetParent rejected a valid reparent" << std::endl;
      return 1;
    }
    auto &aKids = hScene->GetChildren(a);
    auto &bKids = hScene->GetChildren(b);
    if (aKids.size() != 1 || aKids[0] != b ||
        bKids.size() != 1 || bKids[0] != c) {
      std::cerr << "FAIL: hierarchy tree did not record A->B->C" << std::endl;
      return 1;
    }
    if (hScene->GetParent(b) != a || hScene->GetParent(c) != b) {
      std::cerr << "FAIL: GetParent disagrees with SetParent" << std::endl;
      return 1;
    }
    std::cout << "PASS: hierarchy built A->B->C" << std::endl;

    // Cycle refusal: A under C would create a cycle.
    if (hScene->SetParent(a, c)) {
      std::cerr << "FAIL: cycle-creating reparent was accepted" << std::endl;
      return 1;
    }
    // State unchanged.
    if (hScene->GetParent(a) != Hamster::UUID::GetNil() || aKids.size() != 1) {
      std::cerr << "FAIL: cycle refusal mutated state" << std::endl;
      return 1;
    }
    std::cout << "PASS: cycle-creating reparent refused" << std::endl;

    // Cascade destroy: removing A drops all three.
    uint32_t before = hScene->GetEntityCount();
    hScene->DestroyEntity(a);
    uint32_t after = hScene->GetEntityCount();
    if (before - after != 3) {
      std::cerr << "FAIL: cascade destroy removed " << (before - after)
                << " entities, expected 3" << std::endl;
      return 1;
    }
    std::cout << "PASS: cascade destroy removed full subtree" << std::endl;
  }

  // --- Entity hierarchy: Python API via hierarchy_script.py ---
  {
    auto hScene = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);
    Hamster::UUID id = hScene->CreateEntity();
    hScene->AddEntityComponent<Hamster::Behaviour>(id);
    auto script = std::make_shared<Hamster::HamsterScript>(
        fixtureDir / "hierarchy_script.py", "hierarchy_script");
    auto &beh = hScene->GetEntityComponent<Hamster::Behaviour>(id);
    beh.scripts[script->GetUUID()] = script;

    app.AddScene(hScene);
    app.SetSceneActive(hScene->GetUUID());
    hScene->RunScene();

    std::filesystem::create_directories(markerDir);
    std::filesystem::remove(markerDir / "hierarchy_ok.ok");

    hScene->RunSceneSimulation();

    bool hierarchyOk = std::filesystem::exists(markerDir / "hierarchy_ok.ok");
    std::filesystem::remove_all(markerDir);
    if (!hierarchyOk) {
      std::cerr << "FAIL: hierarchy_script did not write hierarchy_ok marker"
                << std::endl;
      return 1;
    }
    std::cout << "PASS: Python hierarchy API (parent/children/set_parent/create_entity(parent=))"
              << std::endl;
  }

  // --- Entity hierarchy: serialiser round-trip ---
  {
    auto src = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);
    Hamster::UUID a = src->CreateEntity();
    Hamster::UUID b = src->CreateEntity();
    Hamster::UUID c = src->CreateEntity();
    src->SetParent(b, a);
    src->SetParent(c, b);

    std::filesystem::path tmpFile =
        std::filesystem::temp_directory_path() / "hamster_smoke_hier.scene";
    {
      std::ofstream out(tmpFile, std::ios::binary);
      Hamster::SceneSerialiser writer(src, app.GetAssetManager());
      writer.Serialise(out);
    }

    auto dst = std::make_shared<Hamster::Scene>(app.GetEventDispatcher().get(), &app);
    {
      std::ifstream in(tmpFile, std::ios::binary);
      Hamster::SceneSerialiser reader(dst, app.GetAssetManager());
      reader.Deserialise(in);
    }
    std::filesystem::remove(tmpFile);

    if (dst->GetParent(b) != a || dst->GetParent(c) != b) {
      std::cerr << "FAIL: hierarchy did not survive serialise round-trip"
                << std::endl;
      return 1;
    }
    auto &kids = dst->GetChildren(a);
    if (kids.size() != 1 || kids[0] != b) {
      std::cerr << "FAIL: round-tripped children list wrong" << std::endl;
      return 1;
    }
    std::cout << "PASS: hierarchy survives serialise round-trip" << std::endl;
  }

  return 0;
}
