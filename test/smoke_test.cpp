#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include <box2d/box2d.h>
#include <glad/glad.h>
#include <pybind11/embed.h>

#include "Core/Application.h"
#include "Core/Components.h"
#include "Core/Project.h"
#include "Core/ProjectSerialiser.h"
#include "Core/Scene.h"
#include "Core/SceneSerialiser.h"
#include "Events/UIEvents.h"
#include "Renderer/Renderer.h"
#include "Scripting/HamsterScript.h"
#include "Utils/AssetManager.h"
#include "Utils/SheetSidecar.h"
#include "Utils/SpatialIndex.h"

#include <sstream>

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

  // ─── bug 0014: scripts under Assets/Scripts/ must import by their dotted
  // module name. Only the project root is on sys.path (Scripting::AddPathToPy),
  // so a bare stem isn't importable — AddDefaultScript used to pass the bare
  // stem, which raised ModuleNotFoundError through the unguarded HamsterScript
  // ctor and crashed the editor. This guards the dotted-import mechanism the
  // fix depends on. ───
  {
    auto *am = app.GetAssetManager();

    std::filesystem::path tmpProject =
        std::filesystem::temp_directory_path() / "hamster_newscript_smoke";
    std::filesystem::remove_all(tmpProject);
    std::filesystem::create_directories(tmpProject / "Assets" / "Scripts");

    // Editor adds only the project ROOT to sys.path.
    pybind11::list sysPath = pybind11::module_::import("sys").attr("path");
    sysPath.append(tmpProject.string());

    const std::filesystem::path scriptFile =
        tmpProject / "Assets" / "Scripts" / "Untitled_Script.py";
    {
      std::ofstream out(scriptFile);
      out << "import Hamster\nclass test(Hamster.HamsterBehaviour):\n"
             "    def on_update(self, delta_time): pass\n";
    }

    // Premise: the bare stem is not importable from the project root. If this
    // ever stops failing the bug's trigger is gone — and so is the need for the
    // dotted name, so surface it rather than silently passing.
    bool bareImportFailed = false;
    try {
      pybind11::module_::import("Untitled_Script");
    } catch (pybind11::error_already_set &) {
      bareImportFailed = true;
    }
    if (!bareImportFailed) {
      std::cerr << "FAIL: bare-stem import unexpectedly succeeded (test premise)"
                << std::endl;
      return 1;
    }

    // Real path: LoadProjectScripts derives the dotted name and imports it.
    am->LoadProjectScripts(tmpProject);

    bool registered = false;
    for (auto const &[uuid, script] : am->GetScriptMap()) {
      if (script->GetScriptPath() == scriptFile) {
        registered = true;
        break;
      }
    }
    if (!registered) {
      std::cerr << "FAIL: Assets/Scripts script not loaded by dotted name"
                << std::endl;
      return 1;
    }
    std::cout << "PASS: Assets/Scripts script imports by dotted module name "
                 "(bug 0014)" << std::endl;

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

    // Stop — snapshot restore is deferred; ProcessPendingRestore simulates
    // the next frame's Application::Run prologue.
    snapScene->PauseSceneSimulation();
    snapScene->ProcessPendingRestore();

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

  // --- sprite-batching scenario ---
  // 4 sprites, 2 textures (A, A, B, B), all z = 0. One Scene::OnRender pass
  // should produce exactly 2 batches: one per unique texture. Asserts the
  // submit/flush API counts correctly. Pre-existing 15 scenarios cover the
  // per-sprite path indirectly (they go through DrawSprite for the pick
  // pass which we deliberately left unbatched).
  {
    auto batchScene = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    auto *am2 = app.GetAssetManager();

    Hamster::UUID texAUUID;
    Hamster::UUID texBUUID;
    am2->AddTexture(texAUUID, "batchA", "A");
    am2->AddTexture(texBUUID, "batchB", "B");
    auto texA = am2->GetTexture(texAUUID);
    auto texB = am2->GetTexture(texBUUID);

    // Two sprites per texture, sorted-equivalent z. The render group sorts
    // by transform.position.z, so identical z means batch order is
    // determined by EnTT iteration — both texA sprites come first or both
    // texB come first, never interleaved (we don't control the order, but
    // we know there are exactly 2 distinct textures → 2 flushes).
    for (int i = 0; i < 2; ++i) {
      Hamster::UUID e = batchScene->CreateEntity();
      auto &t = batchScene->GetEntityComponent<Hamster::Transform>(e);
      t.position = {static_cast<float>(i * 40), 0.0f, 0.0f};
      t.size = {32.0f, 32.0f};
      batchScene->AddEntityComponent<Hamster::Sprite>(e, texA, glm::vec3(1.0f));
    }
    for (int i = 0; i < 2; ++i) {
      Hamster::UUID e = batchScene->CreateEntity();
      auto &t = batchScene->GetEntityComponent<Hamster::Transform>(e);
      t.position = {static_cast<float>(100 + i * 40), 0.0f, 0.0f};
      t.size = {32.0f, 32.0f};
      batchScene->AddEntityComponent<Hamster::Sprite>(e, texB, glm::vec3(1.0f));
    }

    app.AddScene(batchScene);
    app.SetSceneActive(batchScene->GetUUID());

    // Drain any pre-existing GL error state from earlier scenarios so the
    // post-render check measures the batch path in isolation.
    while (glGetError() != GL_NO_ERROR) {}

    batchScene->OnRender(false);

    uint32_t drawCalls = app.GetRenderer()->GetLastFrameDrawCallCount();
    if (drawCalls != 2) {
      std::cerr << "FAIL: expected 2 draw calls for 4 sprites + 2 textures, "
                << "got " << drawCalls << std::endl;
      return 1;
    }
    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
      std::cerr << "FAIL: glGetError after batched render: 0x"
                << std::hex << glErr << std::endl;
      return 1;
    }
    std::cout << "PASS: sprite batching emitted " << drawCalls
              << " draw call(s) for 4 sprites / 2 textures" << std::endl;
  }

  // --- spatial-index scenario A: point query ---
  // 5 entries at known positions. Hit/miss/overlap-with-z-tiebreak.
  {
    using namespace Hamster;
    SpatialIndex idx;
    UUID a, b, c, d, e;
    std::vector<std::pair<UUID, AABB>> entries = {
      {a, AABB{{  0.0f,   0.0f}, { 10.0f,  10.0f}}},
      {b, AABB{{100.0f, 100.0f}, {200.0f, 200.0f}}},
      {c, AABB{{  0.0f,   0.0f}, {  5.0f,   5.0f}}},  // overlaps a
      {d, AABB{{500.0f, 500.0f}, {510.0f, 510.0f}}},
      {e, AABB{{ 50.0f,  50.0f}, { 60.0f,  60.0f}}},
    };
    idx.Rebuild(entries);

    auto zMap = [&](UUID u) -> float {
      if (u == a) return 1.0f;
      if (u == c) return 2.0f;
      return 0.0f;
    };

    UUID hit1 = idx.QueryPoint({105.0f, 150.0f}, zMap);
    if (hit1 != b) {
      std::cerr << "FAIL: point query: expected b at (105,150)" << std::endl;
      return 1;
    }
    UUID hit2 = idx.QueryPoint({1000.0f, 1000.0f}, zMap);
    if (!UUID::IsNil(hit2)) {
      std::cerr << "FAIL: point query: expected Nil at (1000,1000)"
                << std::endl;
      return 1;
    }
    // (3,3) hits both a and c; c has higher z → wins.
    UUID hit3 = idx.QueryPoint({3.0f, 3.0f}, zMap);
    if (hit3 != c) {
      std::cerr << "FAIL: point query: expected c (higher z) at (3,3)"
                << std::endl;
      return 1;
    }
    std::cout << "PASS: spatial index point query (hit, miss, z-tiebreak)"
              << std::endl;
  }

  // --- spatial-index scenario B: rect query ---
  {
    using namespace Hamster;
    SpatialIndex idx;
    UUID a, b, c, d, e;
    std::vector<std::pair<UUID, AABB>> entries = {
      {a, AABB{{  0.0f,   0.0f}, { 10.0f,  10.0f}}},
      {b, AABB{{100.0f, 100.0f}, {200.0f, 200.0f}}},
      {c, AABB{{  0.0f,   0.0f}, {  5.0f,   5.0f}}},
      {d, AABB{{500.0f, 500.0f}, {510.0f, 510.0f}}},
      {e, AABB{{ 50.0f,  50.0f}, { 60.0f,  60.0f}}},
    };
    idx.Rebuild(entries);

    // Rect covering a/c only (both at origin)
    auto r1 = idx.QueryRect(AABB{{-5.0f, -5.0f}, {6.0f, 6.0f}});
    bool hasA = false, hasC = false;
    for (UUID u : r1) {
      if (u == a) hasA = true;
      if (u == c) hasC = true;
    }
    if (!hasA || !hasC || r1.size() != 2) {
      std::cerr << "FAIL: rect query: expected {a,c} at origin rect, got "
                << r1.size() << " entries" << std::endl;
      return 1;
    }

    auto r2 = idx.QueryRect(AABB{{900.0f, 900.0f}, {1000.0f, 1000.0f}});
    if (!r2.empty()) {
      std::cerr << "FAIL: rect query: expected empty far away" << std::endl;
      return 1;
    }
    std::cout << "PASS: spatial index rect query (subset + empty)" << std::endl;
  }

  // --- spatial-index scenario C: rotated AABB end-to-end via Scene ---
  // One 45°-rotated 100×100 sprite at origin. Tight bbox should fit a
  // diagonal of length 100√2 ≈ 141, half ≈ 70.7. Centre of the sprite is
  // at (50, 50) (position + half-size).
  {
    auto rotScene = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    auto *amR = app.GetAssetManager();
    Hamster::UUID texUUID;
    amR->AddTexture(texUUID, "rotTex", "R");
    auto tex = amR->GetTexture(texUUID);

    Hamster::UUID eUUID = rotScene->CreateEntity();
    auto &t = rotScene->GetEntityComponent<Hamster::Transform>(eUUID);
    t.position = {0.0f, 0.0f, 0.0f};
    t.size = {100.0f, 100.0f};
    t.rotation = 45.0f;
    rotScene->AddEntityComponent<Hamster::Sprite>(eUUID, tex,
                                                  glm::vec3(1.0f));

    app.AddScene(rotScene);
    app.SetSceneActive(rotScene->GetUUID());

    while (glGetError() != GL_NO_ERROR) {}
    rotScene->OnRender(false);

    // Sprite centre is at (50, 50). Tight bbox should cover roughly
    // (50 - 70.7) .. (50 + 70.7), so points near the diagonal corners
    // are inside.
    auto zMap = [](Hamster::UUID) -> float { return 0.0f; };
    const auto &idx = rotScene->GetSpatialIndex();
    Hamster::UUID centre = idx.QueryPoint({50.0f, 50.0f}, zMap);
    if (centre != eUUID) {
      std::cerr << "FAIL: rotated AABB: centre point did not pick entity"
                << std::endl;
      return 1;
    }
    // Point well outside the tight bbox (say 200, 200) — should miss.
    Hamster::UUID miss = idx.QueryPoint({200.0f, 200.0f}, zMap);
    if (!Hamster::UUID::IsNil(miss)) {
      std::cerr << "FAIL: rotated AABB: distant point unexpectedly hit"
                << std::endl;
      return 1;
    }
    // Point at (50, 115) — just outside an axis-aligned 100×100 bbox but
    // INSIDE the rotated tight bbox (which extends ~70 units in each
    // direction from the centre).
    Hamster::UUID rotated = idx.QueryPoint({50.0f, 115.0f}, zMap);
    if (rotated != eUUID) {
      std::cerr << "FAIL: rotated AABB: point inside tight bbox did not "
                   "pick entity (rotation math wrong?)" << std::endl;
      return 1;
    }
    std::cout << "PASS: spatial index rotated-sprite AABB" << std::endl;
  }

  // --- spatial-index scenario D: viewport culling integration ---
  // 4 sprites at different positions; viewport covers only 2. After
  // OnRender, draw-call count should reflect only the visible 2 (one per
  // unique texture). Confirms RebuildSpatialIndex + QueryRect + the cull
  // loop in Scene::OnRender all line up.
  {
    auto cullScene = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    auto *amC = app.GetAssetManager();
    Hamster::UUID texXUUID, texYUUID;
    amC->AddTexture(texXUUID, "cullX", "X");
    amC->AddTexture(texYUUID, "cullY", "Y");
    auto texX = amC->GetTexture(texXUUID);
    auto texY = amC->GetTexture(texYUUID);

    auto place = [&](float px, float py,
                     std::shared_ptr<Hamster::Texture> tex) {
      Hamster::UUID id = cullScene->CreateEntity();
      auto &tr = cullScene->GetEntityComponent<Hamster::Transform>(id);
      tr.position = {px, py, 0.0f};
      tr.size = {32.0f, 32.0f};
      cullScene->AddEntityComponent<Hamster::Sprite>(id, tex,
                                                     glm::vec3(1.0f));
    };
    place(   0.0f,    0.0f, texX);    // inside default viewport
    place(  60.0f,    0.0f, texY);    // inside default viewport
    place(5000.0f, 5000.0f, texX);    // far outside — should be culled
    place(6000.0f, 6000.0f, texY);    // far outside — should be culled

    app.AddScene(cullScene);
    app.SetSceneActive(cullScene->GetUUID());

    while (glGetError() != GL_NO_ERROR) {}
    cullScene->OnRender(false);

    uint32_t drawCalls = app.GetRenderer()->GetLastFrameDrawCallCount();
    if (drawCalls != 2) {
      std::cerr << "FAIL: cull: expected 2 draw calls (2 visible / 2 unique "
                << "textures), got " << drawCalls << std::endl;
      return 1;
    }
    GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
      std::cerr << "FAIL: cull: glGetError after render: 0x"
                << std::hex << glErr << std::endl;
      return 1;
    }
    std::cout << "PASS: viewport culling — " << drawCalls
              << " draw call(s) for 2 visible of 4 total sprites"
              << std::endl;
  }


  // ─── game-ui UI-1: UIButton + UIText serialise round-trip ───
  {
    auto src = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    Hamster::UUID btnId = src->CreateEntity();
    Hamster::UIButton btn;
    btn.anchor = Hamster::UIAnchor::BottomRight;
    btn.offset = {20.0f, 20.0f};
    btn.size = {180.0f, 48.0f};
    btn.autoSize = true;
    btn.padding = 12.0f;
    btn.bgColour = {0.3f, 0.5f, 0.7f, 0.9f};
    btn.label = "Start";
    btn.textColour = {1.0f, 0.95f, 0.9f, 1.0f};
    btn.fontSize = 22.0f;
    btn.textAlign = Hamster::UITextAlign::Right;
    src->AddEntityComponent<Hamster::UIButton>(btnId, btn);

    Hamster::UUID txtId = src->CreateEntity();
    Hamster::UIText txt;
    txt.anchor = Hamster::UIAnchor::TopLeft;
    txt.offset = {15.0f, 25.0f};
    txt.text = "Score: 42";
    txt.textColour = {1.0f, 1.0f, 1.0f, 1.0f};
    txt.fontSize = 18.0f;
    txt.wrapWidth = 240.0f;
    src->AddEntityComponent<Hamster::UIText>(txtId, txt);

    std::filesystem::path tmpFile =
        std::filesystem::temp_directory_path() / "hamster_smoke_ui.scene";
    {
      std::ofstream out(tmpFile, std::ios::binary);
      Hamster::SceneSerialiser writer(src, app.GetAssetManager());
      writer.Serialise(out);
    }

    auto dst = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    {
      std::ifstream in(tmpFile, std::ios::binary);
      Hamster::SceneSerialiser reader(dst, app.GetAssetManager());
      reader.Deserialise(in);
    }
    std::filesystem::remove(tmpFile);

    if (!dst->EntityHasComponent<Hamster::UIButton>(btnId) ||
        !dst->EntityHasComponent<Hamster::UIText>(txtId)) {
      std::cerr << "FAIL: UI components missing after round-trip" << std::endl;
      return 1;
    }
    const auto &b2 = dst->GetEntityComponent<Hamster::UIButton>(btnId);
    if (b2.anchor != Hamster::UIAnchor::BottomRight ||
        b2.offset.x != 20.0f || b2.offset.y != 20.0f ||
        b2.size.x != 180.0f || b2.size.y != 48.0f ||
        b2.autoSize != true || b2.padding != 12.0f ||
        b2.bgColour.r != 0.3f || b2.bgColour.a != 0.9f ||
        b2.label != "Start" ||
        b2.fontSize != 22.0f ||
        b2.textAlign != Hamster::UITextAlign::Right) {
      std::cerr << "FAIL: UIButton round-trip lost fields" << std::endl;
      return 1;
    }
    const auto &t2 = dst->GetEntityComponent<Hamster::UIText>(txtId);
    if (t2.anchor != Hamster::UIAnchor::TopLeft ||
        t2.offset.x != 15.0f || t2.offset.y != 25.0f ||
        t2.text != "Score: 42" ||
        t2.fontSize != 18.0f ||
        t2.wrapWidth != 240.0f) {
      std::cerr << "FAIL: UIText round-trip lost fields" << std::endl;
      return 1;
    }
    std::cout << "PASS: UI-1 — UIButton + UIText serialise round-trip"
              << std::endl;
  }

  // ─── game-ui UI-2: find_entity_by_name ───
  {
    auto s = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    Hamster::UUID a = s->CreateEntity();
    Hamster::UUID b = s->CreateEntity();
    s->GetEntityComponent<Hamster::Name>(a).name = "StartButton";
    s->GetEntityComponent<Hamster::Name>(b).name = "Score";

    Hamster::UUID hitA = s->FindEntityByName("StartButton");
    Hamster::UUID hitB = s->FindEntityByName("Score");
    Hamster::UUID miss = s->FindEntityByName("Nonexistent");

    if (hitA != a || hitB != b) {
      std::cerr << "FAIL: find_entity_by_name returned wrong UUID"
                << std::endl;
      return 1;
    }
    if (!Hamster::UUID::IsNil(miss)) {
      std::cerr << "FAIL: find_entity_by_name didn't return nil on miss"
                << std::endl;
      return 1;
    }
    std::cout << "PASS: UI-2 — find_entity_by_name hit/hit/miss" << std::endl;
  }

  // ─── game-ui UI-3: anchor resolution ───
  // 9 anchors × known viewport (vw=1000, vh=600) × known size/offset (10,10).
  // For corner anchors, +offset always points inward (toward centre).
  {
    const float vw = 1000.0f, vh = 600.0f;
    auto check = [&](Hamster::UIAnchor anchor, float expX, float expY) {
      Hamster::UIButton b;
      b.anchor = anchor;
      b.offset = {10.0f, 10.0f};
      b.size = {100.0f, 50.0f};
      Hamster::UIRect r = Hamster::ResolveUIButtonRect(b, vw, vh);
      if (std::abs(r.x - expX) > 1.0f || std::abs(r.y - expY) > 1.0f) {
        std::cerr << "FAIL: UI-3 anchor=" << static_cast<int>(anchor)
                  << " expected (" << expX << "," << expY << ") got ("
                  << r.x << "," << r.y << ")" << std::endl;
        return false;
      }
      return true;
    };
    // Top-left: pivot at TL of rect; +x right, +y down → (10, 10).
    if (!check(Hamster::UIAnchor::TopLeft, 10.0f, 10.0f)) return 1;
    // Top-centre: pivot at top-centre (rect-x = anchor.x - size.x/2 + 10).
    //   anchor.x = 500; rect-x = 500 - 50 + 10 = 460; rect-y = 0 + 10 = 10.
    if (!check(Hamster::UIAnchor::TopCentre, 460.0f, 10.0f)) return 1;
    // Top-right: pivot at TR; +x flipped (moves left), +y down.
    //   anchor.x = 1000; rect-x = 1000 - 100 + (-10) = 890; rect-y = 10.
    if (!check(Hamster::UIAnchor::TopRight, 890.0f, 10.0f)) return 1;
    // Middle-left: pivot at ML; +x right, +y down.
    //   anchor = (0, 300); rect-x = 0 - 0 + 10 = 10; rect-y = 300 - 25 + 10 = 285.
    if (!check(Hamster::UIAnchor::MiddleLeft, 10.0f, 285.0f)) return 1;
    // Centre: pivot at centre.
    //   anchor = (500, 300); rect-x = 500 - 50 + 10 = 460; rect-y = 300 - 25 + 10 = 285.
    if (!check(Hamster::UIAnchor::Centre, 460.0f, 285.0f)) return 1;
    // Middle-right: pivot at MR; +x flipped.
    //   anchor = (1000, 300); rect-x = 1000 - 100 + (-10) = 890; rect-y = 285.
    if (!check(Hamster::UIAnchor::MiddleRight, 890.0f, 285.0f)) return 1;
    // Bottom-left: pivot at BL; +x right, +y flipped (moves up).
    //   anchor = (0, 600); rect-x = 0 - 0 + 10 = 10; rect-y = 600 - 50 + (-10) = 540.
    if (!check(Hamster::UIAnchor::BottomLeft, 10.0f, 540.0f)) return 1;
    // Bottom-centre: pivot at BC; +y flipped.
    //   anchor = (500, 600); rect-x = 460; rect-y = 540.
    if (!check(Hamster::UIAnchor::BottomCentre, 460.0f, 540.0f)) return 1;
    // Bottom-right: pivot at BR; both flipped (inset from corner).
    //   anchor = (1000, 600); rect-x = 890; rect-y = 540.
    if (!check(Hamster::UIAnchor::BottomRight, 890.0f, 540.0f)) return 1;
    std::cout << "PASS: UI-3 — anchor resolution (9 anchors × 1000×600 viewport)"
              << std::endl;
  }

  // ─── game-ui UI-4: synthesised click → ButtonClickedEvent ───
  // Posts a ButtonClickedEvent via the dispatcher (same path used by
  // EditorLayer when its hit-test fires in play mode) and verifies the
  // observer fires exactly once with the matching UUID.
  {
    auto s = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);
    Hamster::UUID btnId = s->CreateEntity();
    Hamster::UIButton b;
    b.anchor = Hamster::UIAnchor::Centre;
    b.offset = {0.0f, 0.0f};
    b.size = {200.0f, 80.0f};
    s->AddEntityComponent<Hamster::UIButton>(btnId, b);

    int hits = 0;
    Hamster::UUID lastUUID;
    auto handle = app.GetEventDispatcher()->Subscribe(
        Hamster::ButtonClicked,
        [&hits, &lastUUID](Hamster::Event &raw) {
          auto &e = static_cast<Hamster::ButtonClickedEvent &>(raw);
          ++hits;
          lastUUID = e.GetEntityId();
        });

    // Resolve the button against a 1000×600 viewport; pick a point inside
    // the resolved rect and confirm the rect contains it.
    Hamster::UIRect r = Hamster::ResolveUIButtonRect(b, 1000.0f, 600.0f);
    const float clickX = r.x + r.w * 0.5f;
    const float clickY = r.y + r.h * 0.5f;
    if (!r.ContainsPoint(clickX, clickY)) {
      std::cerr << "FAIL: UI-4 setup: rect doesn't contain its own centre"
                << std::endl;
      return 1;
    }

    // Post the click — same call EditorLayer uses on hit in play mode.
    Hamster::ButtonClickedEvent be(btnId);
    app.GetEventDispatcher()->Post<Hamster::ButtonClickedEvent>(be);

    if (hits != 1 || lastUUID != btnId) {
      std::cerr << "FAIL: UI-4 expected 1 click on btnId, got " << hits
                << " hits" << std::endl;
      return 1;
    }

    app.GetEventDispatcher()->Unsubscribe(Hamster::ButtonClicked, handle);
    std::cout << "PASS: UI-4 — ButtonClickedEvent dispatch" << std::endl;
  }

  // ─── game-ui UI-5: auto-size measurement ───
  // Only meaningful when the font atlas loaded — Phase B test exes don't
  // always land next to the editor's Resources dir, so a missing atlas is
  // a SKIP rather than a FAIL.
  {
    const Hamster::FontAtlas *atlas = app.GetRenderer()->GetFontAtlas();
    if (!atlas || !atlas->IsValid()) {
      std::cout << "SKIP: UI-5 — font atlas not available in this build" << std::endl;
    } else {
      Hamster::UIButton b;
      b.label = "Auto";
      b.fontSize = 18.0f;
      b.padding = 8.0f;
      b.autoSize = true;
      b.anchor = Hamster::UIAnchor::TopLeft;
      b.offset = {0.0f, 0.0f};

      Hamster::UIRect r =
          app.GetRenderer()->ResolveUIButton(b, 1000.0f, 600.0f);

      float measured = atlas->MeasureWidth(b.label, b.fontSize);
      float expectedW = measured + 2.0f * b.padding;
      float expectedH = b.fontSize + 2.0f * b.padding;
      if (std::abs(r.w - expectedW) > 1.0f ||
          std::abs(r.h - expectedH) > 1.0f) {
        std::cerr << "FAIL: UI-5 auto-size expected (" << expectedW << ","
                  << expectedH << ") got (" << r.w << "," << r.h << ")"
                  << std::endl;
        return 1;
      }
      std::cout << "PASS: UI-5 — auto-size button rect matches "
                   "MeasureWidth + 2·padding" << std::endl;
    }
  }

  // ─── game-ui UI-6: Python end-to-end (find + label + click) ───
  // Exercises the full Python surface: find_entity_by_name on two named
  // entities, EntityHandle.set_text for runtime mutation, and the
  // on_button_clicked virtual reception of a synthesised ButtonClickedEvent.
  {
    auto s = std::make_shared<Hamster::Scene>(
        app.GetEventDispatcher().get(), &app);

    // The "TestBtn" entity is a UIButton; "Counter" is a UIText; the third
    // entity owns the script.
    Hamster::UUID btnId = s->CreateEntity();
    s->GetEntityComponent<Hamster::Name>(btnId).name = "TestBtn";
    s->AddEntityComponent<Hamster::UIButton>(btnId);

    Hamster::UUID counterId = s->CreateEntity();
    s->GetEntityComponent<Hamster::Name>(counterId).name = "Counter";
    s->AddEntityComponent<Hamster::UIText>(counterId);

    Hamster::UUID mgrId = s->CreateEntity();
    s->AddEntityComponent<Hamster::Behaviour>(mgrId);
    auto script = std::make_shared<Hamster::HamsterScript>(
        fixtureDir / "ui_script.py", "ui_script");
    auto &beh = s->GetEntityComponent<Hamster::Behaviour>(mgrId);
    beh.scripts[script->GetUUID()] = script;

    app.AddScene(s);
    app.SetSceneActive(s->GetUUID());
    s->RunScene();

    std::filesystem::create_directories(markerDir);
    std::filesystem::remove(markerDir / "ui_create.ok");
    std::filesystem::remove(markerDir / "ui_clicked.ok");

    s->RunSceneSimulation();

    if (!std::filesystem::exists(markerDir / "ui_create.ok")) {
      std::cerr << "FAIL: UI-6 — on_create did not mark (find_entity_by_name "
                   "or set_text broken)" << std::endl;
      return 1;
    }
    // Verify set_text wrote through to the UIText component.
    const auto &txt = s->GetEntityComponent<Hamster::UIText>(counterId);
    if (txt.text != "Score: 1") {
      std::cerr << "FAIL: UI-6 — set_text didn't update UIText.text (got '"
                << txt.text << "')" << std::endl;
      return 1;
    }

    // Synthesise a click on TestBtn.
    Hamster::ButtonClickedEvent be(btnId);
    app.GetEventDispatcher()->Post<Hamster::ButtonClickedEvent>(be);

    // Drain via one OnUpdate tick — OnScriptUpdate dispatches the queue.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    s->OnUpdate();

    if (!std::filesystem::exists(markerDir / "ui_clicked.ok")) {
      std::cerr << "FAIL: UI-6 — on_button_clicked did not fire" << std::endl;
      return 1;
    }

    std::filesystem::remove_all(markerDir);
    std::cout << "PASS: UI-6 — Python end-to-end (find + set_text + click)"
              << std::endl;
  }

  // ─── Spritesheet support: data layer (stage 1) ───
  // SHEET-1: SheetSidecar round-trip. Hand-build 3 entries, write a sidecar
  // to a temp .png path, read it back, assert all fields match — and that
  // a non-trivial line shape (one renamed sub-sprite among auto-named ones)
  // survives.
  {
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "hamster_sheet_smoke";
    fs::create_directories(tmp);
    fs::path fakeSheet = tmp / "atlas.png";
    fs::remove(Hamster::SheetSidecar::SidecarPath(fakeSheet));

    std::vector<Hamster::SubSpriteEntry> entries = {
        {Hamster::UUID(), "atlas_0",  {0, 0, 32, 32}},
        {Hamster::UUID(), "wing_up",  {32, 0, 32, 32}},
        {Hamster::UUID(), "atlas_2",  {64, 0, 32, 32}},
    };

    if (!Hamster::SheetSidecar::Write(fakeSheet, entries)) {
      std::cerr << "FAIL: SHEET-1 — Write returned false" << std::endl;
      return 1;
    }

    std::vector<Hamster::SubSpriteEntry> readBack;
    if (!Hamster::SheetSidecar::Read(fakeSheet, readBack)) {
      std::cerr << "FAIL: SHEET-1 — Read returned false" << std::endl;
      return 1;
    }

    if (readBack.size() != 3) {
      std::cerr << "FAIL: SHEET-1 — expected 3 entries, got "
                << readBack.size() << std::endl;
      return 1;
    }
    for (size_t i = 0; i < 3; ++i) {
      bool same = readBack[i].uuid.GetUUID() == entries[i].uuid.GetUUID() &&
                  readBack[i].name == entries[i].name &&
                  readBack[i].pixelRect.x == entries[i].pixelRect.x &&
                  readBack[i].pixelRect.y == entries[i].pixelRect.y &&
                  readBack[i].pixelRect.z == entries[i].pixelRect.z &&
                  readBack[i].pixelRect.w == entries[i].pixelRect.w;
      if (!same) {
        std::cerr << "FAIL: SHEET-1 — entry " << i << " mismatch" << std::endl;
        return 1;
      }
    }
    fs::remove_all(tmp);
    std::cout << "PASS: SHEET-1 — SheetSidecar round-trip preserves "
                 "uuid/name/rect across 3 entries"
              << std::endl;
  }

  // SHEET-2 + SHEET-3: ResolveSpriteSource paths.
  // - Texture UUID returns (that texture, full UV).
  // - SubSprite UUID returns (parent texture, normalised pixel rect).
  // - Unknown UUID returns the MISSING fallback texture + missing=true.
  // Uses AddTexture(uuid, path, name) test helper which doesn't actually
  // load the .png (path is a placeholder), so the resolved Texture has
  // width=0/height=0. ResolveSpriteSource for the sub-sprite path would
  // see "tw <= 0" and short-circuit to MISSING; to exercise the normal
  // path we instead test only Texture + Missing resolutions here, and let
  // the sub-sprite UV math be covered by manual verification.
  {
    auto am = std::make_unique<Hamster::AssetManager>(
        [](std::function<void()>) {});

    Hamster::UUID textureUUID;  // mint
    am->AddTexture(textureUUID, "<test>", "test_texture");

    // Texture resolution
    Hamster::SpriteSource src = am->ResolveSpriteSource(textureUUID);
    if (src.missing || !src.texture) {
      std::cerr << "FAIL: SHEET-2 — texture resolve marked missing" << std::endl;
      return 1;
    }
    if (src.uvRect.x != 0.0f || src.uvRect.y != 0.0f ||
        src.uvRect.z != 1.0f || src.uvRect.w != 1.0f) {
      std::cerr << "FAIL: SHEET-2 — texture UV not (0,0,1,1)" << std::endl;
      return 1;
    }
    std::cout << "PASS: SHEET-2 — ResolveSpriteSource(texture UUID) "
                 "returns (texture, full UV)"
              << std::endl;

    // Missing UUID
    Hamster::UUID phantom;  // unknown
    Hamster::SpriteSource miss = am->ResolveSpriteSource(phantom);
    if (!miss.missing || !miss.texture) {
      std::cerr << "FAIL: SHEET-3 — phantom UUID did not return missing"
                << std::endl;
      return 1;
    }
    std::cout
        << "PASS: SHEET-3 — ResolveSpriteSource(unknown UUID) returns MISSING"
        << std::endl;

    // SHEET-4: FindAssetByName across the unified namespace.
    Hamster::UUID subUUID = am->AddSubSprite(textureUUID,
                                              glm::ivec4(0, 0, 16, 16),
                                              "named_sub");
    Hamster::UUID byTextureName = am->FindAssetByName("test_texture");
    Hamster::UUID bySubName = am->FindAssetByName("named_sub");
    Hamster::UUID byMissingName = am->FindAssetByName("does_not_exist");
    if (byTextureName.GetUUID() != textureUUID.GetUUID()) {
      std::cerr << "FAIL: SHEET-4 — texture name lookup miss" << std::endl;
      return 1;
    }
    if (bySubName.GetUUID() != subUUID.GetUUID()) {
      std::cerr << "FAIL: SHEET-4 — sub-sprite name lookup miss" << std::endl;
      return 1;
    }
    if (!Hamster::UUID::IsNil(byMissingName)) {
      std::cerr << "FAIL: SHEET-4 — phantom name returned non-nil" << std::endl;
      return 1;
    }
    std::cout
        << "PASS: SHEET-4 — FindAssetByName resolves textures + sub-sprites + nil"
        << std::endl;
  }

  // SHEET-5: RenameAsset moves the .png.sheet sidecar alongside the .png
  // (stage 9). Uses real files in a temp dir so the filesystem move runs;
  // PNG validity is irrelevant to the rename path.
  {
    namespace fs = std::filesystem;
    auto am = std::make_unique<Hamster::AssetManager>(
        [](std::function<void()>) {});

    fs::path dir = fs::temp_directory_path() / "hamster_sheet_rename_smoke";
    fs::create_directories(dir);
    fs::path oldPng = dir / "atlas.png";
    fs::path newPng = dir / "renamed.png";
    fs::remove(newPng);
    fs::remove(Hamster::SheetSidecar::SidecarPath(newPng));
    { std::ofstream(oldPng) << "not-a-real-png"; }

    std::vector<Hamster::SubSpriteEntry> entries;
    Hamster::SubSpriteEntry e;
    e.uuid = Hamster::UUID();
    e.name = "region0";
    e.pixelRect = glm::ivec4(0, 0, 8, 8);
    entries.push_back(e);
    Hamster::SheetSidecar::Write(oldPng, entries);

    Hamster::UUID tex;
    am->AddTexture(tex, oldPng.string(), "atlas");

    if (!am->RenameAsset(tex, "renamed.png")) {
      std::cerr << "FAIL: SHEET-5 — RenameAsset returned false" << std::endl;
      return 1;
    }
    if (!fs::exists(newPng)) {
      std::cerr << "FAIL: SHEET-5 — .png did not move" << std::endl;
      return 1;
    }
    if (!fs::exists(Hamster::SheetSidecar::SidecarPath(newPng))) {
      std::cerr << "FAIL: SHEET-5 — .sheet did not travel with the rename"
                << std::endl;
      return 1;
    }
    if (fs::exists(Hamster::SheetSidecar::SidecarPath(oldPng))) {
      std::cerr << "FAIL: SHEET-5 — old .sheet left behind" << std::endl;
      return 1;
    }
    std::vector<Hamster::SubSpriteEntry> back;
    if (!Hamster::SheetSidecar::Read(newPng, back) || back.size() != 1 ||
        back[0].name != "region0") {
      std::cerr << "FAIL: SHEET-5 — region lost after rename" << std::endl;
      return 1;
    }
    std::cout << "PASS: SHEET-5 — RenameAsset moves the .png.sheet sidecar"
              << std::endl;
    fs::remove_all(dir);
  }

  // ─── Project resolution: serialise round-trip + legacy default ───
  // PRJ-1: writes the new on-disk format manually (so we test Deserialise
  // against the exact byte layout we ship) and verifies all five fields
  // come back, including the appended targetWidth / targetHeight ints.
  {
    std::stringstream ss;
    const std::string name = "round_trip_proj";
    const std::string projDir = "C:/tmp/round_trip";
    const std::string scenePath = "scene_a.bin";
    const int32_t w = 1920;
    const int32_t h = 1080;

    auto writeStr = [&](const std::string &s) {
      std::size_t len = s.size();
      ss.write(reinterpret_cast<const char *>(&len), sizeof(len));
      ss.write(s.data(), len);
    };
    writeStr(name);
    writeStr(projDir);
    writeStr(scenePath);
    ss.write(reinterpret_cast<const char *>(&w), sizeof(w));
    ss.write(reinterpret_cast<const char *>(&h), sizeof(h));

    Hamster::ProjectConfig out = Hamster::ProjectSerialiser::Deserialise(ss);
    if (out.Name != name || out.TargetWidth != 1920 ||
        out.TargetHeight != 1080) {
      std::cerr << "FAIL: PRJ-1 — resolution round-trip "
                << "(name=" << out.Name << " w=" << out.TargetWidth
                << " h=" << out.TargetHeight << ")" << std::endl;
      return 1;
    }
    std::cout << "PASS: PRJ-1 — Project deserialise round-trip 1920x1080"
              << std::endl;
  }

  // PRJ-2: hand-crafted legacy buffer (only the original 3 string fields,
  // no resolution ints appended). Deserialise must return the
  // ProjectConfig defaults (1280x720) and not corrupt the parsed fields.
  {
    std::stringstream ss;
    const std::string name = "legacy_proj";
    const std::string projDir = "C:/tmp/legacy";
    const std::string scenePath = "old_scene.bin";

    auto writeStr = [&](const std::string &s) {
      std::size_t len = s.size();
      ss.write(reinterpret_cast<const char *>(&len), sizeof(len));
      ss.write(s.data(), len);
    };
    writeStr(name);
    writeStr(projDir);
    writeStr(scenePath);
    // No resolution bytes appended — emulates a pre-feature .hamproj.

    Hamster::ProjectConfig out = Hamster::ProjectSerialiser::Deserialise(ss);
    if (out.Name != name || out.TargetWidth != 1280 ||
        out.TargetHeight != 720) {
      std::cerr << "FAIL: PRJ-2 — legacy default "
                << "(name=" << out.Name << " w=" << out.TargetWidth
                << " h=" << out.TargetHeight << ")" << std::endl;
      return 1;
    }
    std::cout
        << "PASS: PRJ-2 — legacy .hamproj falls back to default 1280x720"
        << std::endl;
  }

  return 0;
}
