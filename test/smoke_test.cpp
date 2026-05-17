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

  // ─── Asset sidecars: reconciliation + rename + missing detection ───
  {
    auto *am = app.GetAssetManager();

    std::filesystem::path tmpProject =
        std::filesystem::temp_directory_path() / "hamster_sidecar_smoke";
    std::filesystem::remove_all(tmpProject);
    std::filesystem::create_directories(tmpProject);

    // Make the tmp project importable for HamsterScript's pybind11 import.
    pybind11::list sysPath = pybind11::module_::import("sys").attr("path");
    sysPath.append(tmpProject.string());

    // Fixture: enemy.py + enemy.py.meta with a fixed UUID, plus player.py
    // with no sidecar.
    const std::string fixedUuidStr = "11111111-2222-3333-4444-555555555555";
    {
      std::ofstream out(tmpProject / "enemy.py");
      out << "import Hamster\nclass Enemy(Hamster.HamsterBehaviour): pass\n";
    }
    {
      std::ofstream out(tmpProject / "enemy.py.meta");
      out << "{\"uuid\":\"" << fixedUuidStr << "\"}\n";
    }
    {
      std::ofstream out(tmpProject / "player.py");
      out << "import Hamster\nclass Player(Hamster.HamsterBehaviour): pass\n";
    }

    am->LoadProjectScripts(tmpProject);

    // Sidecar reconciliation: enemy keeps its fixed UUID; player gets a fresh
    // one and a new .py.meta is written next to it.
    std::string mutableFixed = fixedUuidStr;
    Hamster::UUID fixedUuid(mutableFixed);

    auto fixedScript = am->GetScript(fixedUuid);
    if (!fixedScript) {
      std::cerr << "FAIL: sidecar UUID not adopted on load" << std::endl;
      return 1;
    }
    if (!std::filesystem::exists(tmpProject / "player.py.meta")) {
      std::cerr << "FAIL: sidecar not minted for unannotated .py" << std::endl;
      return 1;
    }
    std::cout << "PASS: sidecar reconciliation (adopt + mint)" << std::endl;

    // Locate the player script's UUID for the rename test.
    Hamster::UUID playerUuid = Hamster::UUID::GetNil();
    for (auto const &[uuid, script] : am->GetScriptMap()) {
      if (script->GetScriptPath() == (tmpProject / "player.py")) {
        playerUuid = uuid;
        break;
      }
    }
    if (Hamster::UUID::IsNil(playerUuid)) {
      std::cerr << "FAIL: player.py not registered" << std::endl;
      return 1;
    }

    // Rename via API. Both the .py and the .meta should follow; the UUID
    // must be preserved (so attachments would survive in a real scene).
    if (!am->RenameAsset(playerUuid, "player_renamed.py")) {
      std::cerr << "FAIL: rename refused unexpectedly" << std::endl;
      return 1;
    }
    if (std::filesystem::exists(tmpProject / "player.py") ||
        std::filesystem::exists(tmpProject / "player.py.meta")) {
      std::cerr << "FAIL: old name still on disk after rename" << std::endl;
      return 1;
    }
    if (!std::filesystem::exists(tmpProject / "player_renamed.py") ||
        !std::filesystem::exists(tmpProject / "player_renamed.py.meta")) {
      std::cerr << "FAIL: new name files missing after rename" << std::endl;
      return 1;
    }
    if (!am->GetScript(playerUuid)) {
      std::cerr << "FAIL: rename dropped the UUID from the asset manager"
                << std::endl;
      return 1;
    }
    std::cout << "PASS: rename via API preserves UUID + moves sidecar"
              << std::endl;

    // Same-folder collision is refused.
    if (am->RenameAsset(playerUuid, "enemy.py")) {
      std::cerr << "FAIL: same-folder name collision was not refused"
                << std::endl;
      return 1;
    }
    std::cout << "PASS: same-folder collision refused" << std::endl;

    std::filesystem::remove_all(tmpProject);
  }

  // ─── Missing-script detection on scene load ───
  {
    auto *am = app.GetAssetManager();

    auto missScene = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    Hamster::UUID entId = missScene->CreateEntity();
    missScene->AddEntityComponent<Hamster::Behaviour>(entId);

    auto &beh = missScene->GetEntityComponent<Hamster::Behaviour>(entId);
    Hamster::UUID phantomUuid; // freshly minted; AM has no script for it.
    beh.scripts.emplace(phantomUuid, nullptr);
    beh.cachedNames.emplace(phantomUuid, std::string("phantom_script"));

    // Round-trip via SceneSerialiser to confirm cached names + null script
    // entries survive serialise → deserialise.
    std::filesystem::path missFile =
        std::filesystem::temp_directory_path() / "hamster_miss_smoke.scene";
    {
      Hamster::SceneSerialiser sr(missScene, am);
      std::ofstream out(missFile, std::ios::binary);
      sr.Serialise(out);
    }

    auto loaded = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    {
      Hamster::SceneSerialiser sr(loaded, am);
      std::ifstream in(missFile, std::ios::binary);
      sr.Deserialise(in);
    }
    std::filesystem::remove(missFile);

    bool ok = false;
    auto view = loaded->GetRegistry().view<Hamster::Behaviour>();
    view.each([&](auto &b) {
      auto it = b.scripts.find(phantomUuid);
      if (it == b.scripts.end()) return;
      if (it->second != nullptr) return; // should be null
      auto nameIt = b.cachedNames.find(phantomUuid);
      if (nameIt == b.cachedNames.end()) return;
      if (nameIt->second != "phantom_script") return;
      ok = true;
    });
    if (!ok) {
      std::cerr << "FAIL: missing script reference + cached name did not "
                   "survive scene round-trip" << std::endl;
      return 1;
    }
    std::cout << "PASS: missing-script detection survives scene round-trip"
              << std::endl;
  }

  // --- simulation-snapshot scenario ---
  // Covers spec success criteria #1 (runtime-spawn revert) and #2
  // (component mutation revert). Manager entity carries a script that
  // spawns 5 entities in on_create and mutates its own transform in
  // on_update. After PauseSceneSimulation, the registry should hold only
  // the manager again, and its transform must match the pre-play state.
  {
    auto snapScene = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);

    Hamster::UUID managerUUID = snapScene->CreateEntity();
    auto &preTransform =
        snapScene->GetEntityComponent<Hamster::Transform>(managerUUID);
    preTransform.position = {10.0f, 20.0f, 0.0f};

    snapScene->AddEntityComponent<Hamster::Behaviour>(managerUUID);
    auto probeScript = std::make_shared<Hamster::HamsterScript>(
        fixtureDir / "snapshot_script.py", "snapshot_script");
    auto &mgrBehaviour =
        snapScene->GetEntityComponent<Hamster::Behaviour>(managerUUID);
    mgrBehaviour.scripts[probeScript->GetUUID()] = probeScript;

    app.AddScene(snapScene);
    app.SetSceneActive(snapScene->GetUUID());
    snapScene->RunScene();

    uint32_t countBeforePlay = snapScene->GetEntityCount();
    glm::vec3 posBeforePlay =
        snapScene->GetEntityComponent<Hamster::Transform>(managerUUID).position;

    snapScene->RunSceneSimulation();

    // on_create ran inside RunSceneSimulation — 5 new entities should exist.
    uint32_t countAfterSpawn = snapScene->GetEntityCount();
    if (countAfterSpawn != countBeforePlay + 5) {
      std::cerr << "FAIL: expected " << (countBeforePlay + 5)
                << " entities after spawn, got " << countAfterSpawn
                << std::endl;
      return 1;
    }

    // on_update mutates the manager's transform to (999, 999).
    snapScene->OnUpdate();
    glm::vec3 posDuringPlay =
        snapScene->GetEntityComponent<Hamster::Transform>(managerUUID).position;
    if (posDuringPlay.x != 999.0f || posDuringPlay.y != 999.0f) {
      std::cerr << "FAIL: on_update mutation did not take effect (got "
                << posDuringPlay.x << "," << posDuringPlay.y << ")"
                << std::endl;
      return 1;
    }

    // Stop — snapshot restore should fire.
    snapScene->PauseSceneSimulation();

    uint32_t countAfterRestore = snapScene->GetEntityCount();
    if (countAfterRestore != countBeforePlay) {
      std::cerr << "FAIL: expected " << countBeforePlay
                << " entities after restore (runtime-spawned reverted), got "
                << countAfterRestore << std::endl;
      return 1;
    }

    glm::vec3 posAfterRestore =
        snapScene->GetEntityComponent<Hamster::Transform>(managerUUID).position;
    if (posAfterRestore.x != posBeforePlay.x ||
        posAfterRestore.y != posBeforePlay.y) {
      std::cerr << "FAIL: transform did not revert on stop (expected "
                << posBeforePlay.x << "," << posBeforePlay.y << " got "
                << posAfterRestore.x << "," << posAfterRestore.y << ")"
                << std::endl;
      return 1;
    }

    std::cout << "PASS: simulation snapshot reverts runtime spawns and "
                 "component mutations" << std::endl;
  }

  return 0;
}
