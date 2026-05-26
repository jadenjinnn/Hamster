# Architecture

> Living document. Updated via `/architecture-update`. Source of truth for "how this engine fits together." Read top-to-bottom should give a new engineer (or returning author) enough to navigate the codebase confidently in 10 minutes.

## One-paragraph overview

Hamster is a Windows-targeted 2D game engine with an embedded Python scripting layer, designed so that game authors write gameplay logic in Python against a C++ runtime. Three CMake subprojects make up the whole system: **Hamster-Core** (static C++ library — application loop, ECS via EnTT, OpenGL renderer, Box2D 3.x physics, ImGui GUI integration, pybind11 interpreter lifecycle); **Hamster-Py** (a pybind11 extension module that exposes C++ types to Python under the `Hamster` namespace); and **Hamster-Wheel** (the editor executable — ImGui-based scene editor with hierarchy, property editor, asset browser, spritesheet slice editor, console, and project hub). Entities carry Transform, Sprite (a texture **or** a spritesheet sub-sprite, resolved through `Sprite::assetUUID`), Name, Rigidbody, ID, Animation, Behaviour, and screen-space UIButton/UIText components; the Behaviour component stores instantiated Python objects (subclasses of `HamsterBehaviour`) that receive per-frame callbacks and engine events. The renderer draws a batched world pass plus a separate anchored UI pass (`Scene::OnRenderUI`). On disk, a project keeps its assets under `Assets/{Textures,Scripts,Animations}` and the asset browser is rooted there. Pressing the editor's Play button spawns a separate **popout play window** sized to the project's target resolution; rendering and input both fork along the editor's existing path with the popout's GLFW handle as the second target. A separate runtime-only player (no editor) is planned but not yet implemented.

## Entry points

- `Hamster-Wheel/src/main.cpp::main` — sole binary entry point; constructs `Hamster::WindowProps{borderless=true}`, creates the `Application` with it, applies the prototype's theme + fonts, reads `ProjectRegistry` for the most-recently-opened project, calls `Project::Open(...)`, then pushes a single `EditorLayer` and runs.
- `EditorLayer::OnImGuiUpdate` — owns the custom borderless title bar (logo, File/Edit/View/Build/Help menus, drag/maximise/close), the layout math, and the four positioned panel windows (Property Editor, Level Editor, Hierarchy, Bottom).
- User-authored `.py` files in the project directory — loaded by `HamsterScript` when the editor's play button is pressed (simulation start).
- `Hamster-Wheel-old/` — preserved historical reference of the docked-layout editor. Not built; root `CMakeLists.txt` keeps the `add_subdirectory` line commented.

## Module map

- `Hamster-Core/src/Core/` — `Application` singleton + main loop (owns the editor `Window` and the optional popout play window via `OpenPlayWindow` / `ClosePlayWindow` / `IsPlayWindowOpen` / `GetPlayWindow`), `Window` (GLFW wrapper), `LayerStack`, `Scene` + ECS façade, `Project` + `ProjectSerialiser` (the `.hamproj` blob carries `TargetWidth` / `TargetHeight` for the popout play window — legacy projects without these fields default to 1280×720), `SceneSerialiser`, `UUID`, `Log`/`Logger`, `Components.h` (all component structs including `AnimationKeyframe`, `AnimationData`, `Animation`, the screen-space `UIButton` / `UIText` + `UIAnchor` / `UITextAlign` enums + the `ResolveAnchoredTopLeft` helper, and `Sprite::assetUUID`)
- `Hamster-Core/src/Events/` — `EventType` enum, `Event` base class, `EventDispatcher` (subscribe-only observer), all concrete event types (`WindowEvents`, `ApplicationEvents`, `InputEvents`, `SceneEvents`, `GuiEvents`, `UIEvents` — `ButtonClickedEvent`)
- `Hamster-Core/src/Renderer/` — `Renderer` (instance owned by Application, OpenGL draw calls, camera/zoom; same-texture **sprite batching** via `BeginSpriteBatch`/`SubmitSprite`/`EndSpriteBatch`, and an anchored **UI pass** via `BeginUIPass`/`SubmitUIRect`/`SubmitUIText`/`EndUIPass` + `ResolveUIButton`), `Shader`, `Texture`, `FramebufferTexture`, `FontAtlas` (bakes Inter-Regular into an R8 glyph atlas for UI text), GLSL shaders loaded as files from `Renderer/DefaultShaders/*.vs|.fs` (`SpriteShader`, `FlatShader`, `SpriteBatchShader`, `UIRectShader`, `UITextShader`) — resolved exe-relative under `share/Resources/Hamster-Core/Renderer/DefaultShaders` with a source-dir fallback
- `Hamster-Core/src/Physics/` — gutted; Box2D 3.x replaces the old custom AABB system. Physics world lifecycle and stepping live in `Scene`.
- `Hamster-Core/src/Scripting/` — `Scripting` (interpreter lifecycle, default script generation), `HamsterBehaviour` (C++ base class Python scripts inherit from), `HamsterScript` (Python module loader + class scanner)
- `Hamster-Core/src/Gui/` — `ImGuiLayer` (begin/end frame wrapper), `Panel` + `Modal` base classes
- `Hamster-Core/src/Utils/` — `AssetManager` (textures, scripts, animations, and spritesheet **sub-sprites** — UUID-keyed, instance owned by Application; `.hanim` file I/O; reconciles UUIDs against `.meta` sidecars on project open; `ResolveSpriteSource(UUID) → {Texture*, uvRect, missing}` resolves a Sprite's `assetUUID` to either a whole texture or a parent-texture + UV sub-rect; `FindAssetByName` over the unified texture+sub-sprite namespace), `SheetSidecar` (`.png.sheet` JSON-lines I/O — one sub-sprite per line), `InputManager` (GLFW key polling), `MetaFile` (sidecar JSON I/O — `{uuid}` per asset), `ProjectWatcher` (Win32 `ReadDirectoryChangesW` worker thread that posts add/remove/rename events back to AssetManager on the main thread), `SpatialIndex` (quadtree over (UUID, AABB) — rebuilt per-frame from Transform state; consumed by Scene::OnRender for viewport-rect culling and by EditorLayer for cursor-point picking)
- `Hamster-Py/src/` — pybind11 bindings: `main.cpp` (module entry point), `HamsterBehaviour.h` (trampoline + binding; adds `find_entity_by_name`, `animate`/`stop_animation`, parent/child API), `EntityHandle.h` (runtime entity handle: `add_component`, `set_texture`/`set_label`/`set_text`, `transform`, `set_velocity`, `apply_impulse`, parent/children), `Components.h` (Transform/Sprite/Rigidbody/BodyType/ColliderShape bindings), `Library.h` (vec2/vec3/vec4), `Core.h` (Scene/Application/EventDispatcher — opaque; `Scene.find_entity_by_name`), `Input.h` (KeyCodes enum), `UUID.h`, `Log.h`
- `Hamster-Wheel/src/` — `main.cpp` (entry — borderless WindowProps, ProjectRegistry-driven default project, single `EditorLayer` pushed), `EditorLayer` (custom title bar, layout math, entity picking + drag + context menus, scene viewport blit), `ProjectRegistry` (persistent JSON project list at `%APPDATA%/Hamster/projects.json`), `Theme` + `Panel` (palette / fonts / panel chrome helpers — `DrawHeader`, `DrawTabbedHeader`, `BeginContent`/`EndContent`)
- `Hamster-Wheel/src/Components/` — reusable UI helpers (`HButton`, `HToolbarButton`, `HCombo`, `HDragFloat`, `HCheckbox`, `SectionHeader`, `SectionSeparator`, `AxisDotInput`) consumed by every panel
- `Hamster-Wheel/src/Panels/` — `Hierarchy`, `PropertyEditor`, `LevelEditor`, `BottomPanel` (Asset Browser + Animation + Console as tabs), `AssetBrowser` (rooted at the project's `Assets/`, folder-scoped with a top breadcrumb + back button; sub-sprite expansion + drag), `AnimationPanel`, `SpritesheetEditor` (floating window — slice a texture into sub-sprites, writes the `.png.sheet` sidecar, confirm-on-close for unsaved edits), `Console`, `CreateProjectModal` (shared by hub + editor; also copies the `.pyd` into the new project), `OpenProjectModal`, `RenameModal`, `ColliderEditor` (`ProjectSelector`/`ProjectCreator` were removed)
- `Hamster-Wheel/Resources/` — fonts (Inter-Regular/SemiBold/Bold, Font Awesome 6 solid), icons, logo, sprites, packages folder for the `.pyd` module
- `Hamster-Wheel-old/` — pre-rewrite editor preserved as reference. Not in any build.

## Main loop

`Application::Run()` each frame, in order:

1. **Flush pending layer changes** (push/pop queue from the previous frame's `OnUpdate` pass)
1a. **Popout-close check** — if the popout play window is open and its GLFW should-close flag is set (user clicked the X), pause the active scene's simulation and destroy the popout cleanly. Always runs *before* any per-frame rendering so the popout is never destroyed mid-frame.
2. **`Layer::OnUpdate()`** on every layer in the stack
   - `EditorLayer::OnUpdate` renders to a framebuffer using flat entity colours, reads the pixel under the cursor, and resolves entity selection / guizmo drags
   - If `Application::IsPlayWindowOpen()`, also issues a second `Scene::OnRender` + `Scene::OnRenderUI` into the popout's default framebuffer (with the renderer's viewport / zoom / camera-offset swapped to `(0,0)→(targetW,H)` at zoom=1 and restored afterward)
3. **Flush pending layer changes again** — commits any layer transitions triggered during step 2, so ImGui sees the new stack in the same frame
4. **`ImGuiLayer::Begin()`**
5. **`Layer::OnImGuiUpdate()`** on every layer — all panels and the scene viewport image are drawn here
6. **`ImGuiLayer::End()`** — submits ImGui draw commands to OpenGL
7. **`Scene::OnUpdate()`** on the active scene (if any, and only when simulation is running):
   - `StepPhysics()`: `b2World_Step` with 4 sub-steps
   - `ProcessContactEvents()`: reads `b2World_GetContactEvents`, resolves entity UUIDs via `b2Body_GetUserData`, posts `CollisionEvent`
   - `SyncPhysicsToTransforms()`: copies Box2D body positions/rotations back to Transform components (with pixels-per-meter conversion)
   - Animation advance: iterates entities with `Animation` + `Sprite`, advances `currentTime`, swaps `Sprite::texture` to the current keyframe's texture, posts `AnimationCompletedEvent` for non-looping animations that finish
   - `OnScriptUpdate()`: calls `obj.attr("on_update")(delta_time)` on each Python behaviour — scripts can call `self.apply_force()`, `self.apply_impulse()`, read `self.velocity`; dispatches `on_animation_complete` callbacks from the `completedAnimations` queue
   - Python errors are caught; first exception pauses the simulation and logs to the scene's client logger
8. **`Window::Update`** — `glfwSwapBuffers` + `glfwPollEvents`

Note: `Scene::OnUpdate` runs *after* ImGui, so Python transform changes are first visible in the next frame's render.

## The C++ ↔ Python boundary

### Exposed C++ classes (Python-visible)

| C++ type | Python name | Binding file |
|---|---|---|
| `Hamster::HamsterBehaviour` | `Hamster.HamsterBehaviour` | `Hamster-Py/src/HamsterBehaviour.h` |
| `Hamster::Scene` | `Hamster.Scene` (opaque) | `Hamster-Py/src/Core.h` |
| `Hamster::Application` | `Hamster.Application` (opaque) | `Hamster-Py/src/Core.h` |
| `Hamster::Transform` | `Hamster.Transform` | `Hamster-Py/src/HamsterBehaviour.h` |
| `glm::vec2` | `Hamster.vec2` | `Hamster-Py/src/Library.h` |
| `glm::vec3` | `Hamster.vec3` | `Hamster-Py/src/Library.h` |
| `Hamster::UUID` | `Hamster.UUID` | `Hamster-Py/src/UUID.h` |
| `Hamster::KeyCodes` | `Hamster.key_code` | `Hamster-Py/src/Input.h` |
| `Hamster::LogType` | `Hamster.LogType` | `Hamster-Py/src/Log.h` |

### Interpreter lifecycle

- `pybind11::initialize_interpreter()` — called once in `Application::Application()` via `Scripting::InitInterpreter()`
- `pybind11::finalize_interpreter()` — called in `Application::~Application()` via `Scripting::FinaliseInterpreter()`
- Single interpreter for the lifetime of the process; no subinterpreters

### Script discovery and loading

1. User adds a `.py` file to the project via the editor (File Browser or "New Script").
2. `AssetManager::AddScript` creates a `HamsterScript`, which calls `pybind11::module_::import(filename_stem)` — the stem must be importable (i.e., on `sys.path`).
3. On simulation start, `Scene::RunSceneSimulation` calls `HamsterScript::ReloadScript` on every script in every entity's `Behaviour` component.
4. `ReloadScript` reloads the module, then scans `module.__dict__` for classes that are subclasses of `Hamster.HamsterBehaviour`. Each found class is pushed to `m_PyObjects`.
5. `Scene::RunSceneSimulation` then instantiates each class: `pyClass(entity_uuid, scene_ptr, &application)`, stores the result in `Behaviour::pyObjects`, and calls `on_create()`.
6. Each frame: `obj.attr("on_update")(delta_time)` → `obj.attr("reset_input")()`.

### Calling pattern

C++ drives Python (not the reverse). Engine calls `on_create` once and `on_update` every simulation tick, plus event callbacks (`on_collision`, `on_animation_complete`, `on_button_clicked`). Python calls back into C++ through `HamsterBehaviour` methods (`self.transform`, `self.key_pressed`, `self.log(...)`, `self.apply_force/apply_impulse/set_velocity`, `self.animate(...)`, `self.find_entity_by_name(...)`, `self.create_entity/destroy_entity`) and through the `EntityHandle` returned by entity lookups/creation (`set_texture`, `set_label`, `set_text`, `transform`, parent/children).

### Python module deployment

The pybind11 module is compiled as `Hamster.pyd` (Windows) / `Hamster.so` (Linux). On project creation, `CreateProjectModal` copies the `.pyd` from `Resources/Packages/` into the project directory. `Scripting::AddPathToPy` adds the project directory to `sys.path` on `ProjectOpened`, so `import Hamster` resolves to the `.pyd` in the project folder.

User scripts in subdirectories use Python's dotted module convention — `enemies/boss.py` is imported as `enemies.boss` via namespace package resolution (no `__init__.py` required, Python 3.3+). `AssetManager::LoadProjectScripts` derives the dotted name from each script's project-relative path and passes it to `HamsterScript`'s ctor as the import target.

### Asset identity (sidecars)

Scripts and textures persist their UUID in a sibling `.meta` file (`player.py` ⇄ `player.py.meta`, JSON: `{"uuid":"..."}`). The sidecar is the authoritative identity record and travels with the asset across renames. The project blob in `.hamproj` stores asset paths + display names but never UUIDs — UUIDs come from sidecars on load.

`.hanim` files do NOT get sidecars (the format is self-identifying — UUID lives inside the file). Spritesheet sub-sprites persist in a `.png.sheet` sidecar next to the texture (`SheetSidecar`, one JSON line per sub-sprite — uuid + name + pixel rect); the parent texture's blob entry is what makes them re-load on open.

**On-disk layout.** New projects scaffold `Assets/{Textures,Scripts,Animations}` (created on open too, if missing). Imports land in `Assets/Textures/`, new scripts in `Assets/Scripts/`, animations in `Assets/Animations/`. On project open, `AssetManager::LoadProjectScripts` recursively walks the project dir for `.py` files (adopting `.meta` UUIDs or minting + writing new ones) and cleans orphan sidecars; `LoadProjectAnimations` walks `<projectDir>/Assets/Animations/` for `.hanim`. While the editor is open, `ProjectWatcher` (Win32 `ReadDirectoryChangesW`, `watchSubtree=TRUE`) posts add/remove/rename events back to `AssetManager::HandleFileEvents` on the main thread so external file changes show up live in the asset browser. Editor-initiated renames go through `AssetManager::RenameAsset` which moves both the asset and the `.meta` atomically and refuses same-folder name collisions.

Behaviour components persist a `cachedNames` map (UUID → last-known script name) so the property editor can show "MISSING: `<cachedName>`" in red when a scene references a UUID with no registered script, with a "Reassign to" submenu listing currently loaded scripts. `Scene::RunSceneSimulation` refuses to start if any Behaviour holds a missing-script reference, logging the entity name + cached script name to the client logger.

### Threading

Single-threaded for Python. All `on_update` and `on_create` calls happen on the main thread. `AssetManager::AddTextureAsync` loads image data on a background thread but marshals the OpenGL upload back to the main thread via a stored enqueue callback (set during `AssetManager::Init`). The GIL is held for all Python calls.

### Object lifetime

`pybind11::object` instances live in `Behaviour::pyObjects` (`std::vector<pybind11::object>`) on the entity. They are ref-counted by pybind11. Each `HamsterBehaviour` holds a raw `Transform*` into the EnTT registry (valid as long as the entity is alive) and a raw `Application*` (valid for the process lifetime).

~~**Known leak**: `EventDispatcher` has no unsubscribe.~~ Fixed: `Subscribe` returns a `SubscriptionHandle`; `HamsterBehaviour` destructor unsubscribes its 3 callbacks.

## Data flow (typical simulation frame)

```
glfwPollEvents
  └─ GLFW key callback → KeyPressedEvent posted → HamsterBehaviour::OnKeyPressed (all instances)

EditorLayer::OnUpdate
  └─ Mouse click: render to FBO with flat colours → glReadPixels → entity selection

Scene::OnUpdate [if simulation running]
  ├─ b2World_Step (4 sub-steps)
  ├─ ProcessContactEvents: b2World_GetContactEvents → resolve UUID via userData → post CollisionEvent
  │     └─ HamsterBehaviour::OnCollision: fills m_CollisionEntities
  ├─ SyncPhysicsToTransforms: b2Body_GetPosition/Rotation → Transform (× PPM, rad→deg)
  └─ OnScriptUpdate: obj.on_update(dt) for each Python behaviour
        scripts read self.transform / self.velocity / self.key_pressed / self.colliding
        scripts call self.apply_force(fx, fy) / self.apply_impulse(ix, iy)

EditorLayer renders the world (Scene::OnRender, batched) then the anchored
  screen-space UI (Scene::OnRenderUI) into the viewport FramebufferTexture
ImGui panels render scene viewport (FramebufferTexture → AddImage)
glfwSwapBuffers
```

## Build system

See `docs/build.md` for the full build recipe (populated in Phase 2). Shape:

- Root `CMakeLists.txt` adds subdirectories: `Hamster-Core`, `Hamster-Py`, `Hamster-Wheel`, then vendor libs: `imgui`, `glfw`, `entt`, `box2d`, `tinyfiledialogs`
- `Hamster-Py/CMakeLists.txt` adds pybind11 (git submodule); this makes `pybind11::embed` available globally to Hamster-Wheel (order-dependent, fragile)
- Output artifacts: `Hamster-Wheel` executable; `Hamster.pyd`/`.so` (pybind11 module, copied to `Resources/Packages/` post-build)
- Last known working build: Linux, Clang, Python 3.10, CLion/Ninja — see `Scripts/compile.sh`
- Current target: Windows native; toolchain TBD (Phase 2)
- C++ standard: C++20 throughout

## Third-party dependencies

| Library | Version | Purpose | How acquired |
|---|---|---|---|
| pybind11 | submodule | C++/Python bindings + embedded interpreter | git submodule (`Hamster-Py/Vendor/pybind11`) |
| EnTT | submodule | Entity-component-system | git submodule (`Hamster-Core/Vendor/entt`) |
| GLFW | submodule | Window, OpenGL context, input | git submodule (`Hamster-Core/Vendor/glfw`) |
| GLAD | generated | OpenGL function loader | vendored copy (`Hamster-Core/Vendor/glad`) |
| GLM | submodule | Math (vec2/3/4, mat4) | git submodule (`Hamster-Core/Vendor/glm`) |
| Dear ImGui | copy | Immediate-mode GUI; backends: GLFW + OpenGL3 | vendored directory copy (`Hamster-Core/Vendor/imgui`) |
| Box2D | submodule (3.0.1) | 2D rigid-body physics — world step, collision detection, forces/impulses | git submodule (`Hamster-Core/Vendor/box2d`) |
| stb_image | submodule | PNG/JPG loading | git submodule (`Hamster-Core/Vendor/stb`) |
| tinyfiledialogs | copy | Native file open/save dialogs | vendored copy (`Hamster-Wheel/Vendor/tinyfiledialogs`) |
| Boost 1.86.0 | copy | UUID generation + container hash | vendored header-only copy (`Hamster-Core/Vendor/boost_1_86_0`); **planned for replacement** with a lighter UUID header |
| Python | system | Embedded interpreter | `find_package(Python ... Development REQUIRED)` |

## Known smells / refactor candidates

- ~~**`Events/ScriptingEvent.h/.cpp` missing**~~ Fixed: removed in Phase 2 along with `HamsterBehaviour::Subscribe/Post`.
- ~~**`EventDispatcher` has no unsubscribe**~~ Fixed: `Subscribe` returns a `SubscriptionHandle`; `Unsubscribe(EventType, handle)` removes it. `HamsterBehaviour` destructor unsubscribes its 3 callbacks.
- ~~**`HamsterBehaviour.h:35` — `GetKeyReleased()` returns `m_KeyPressed`**~~ Fixed in Phase 2.
- ~~**Collision resolved twice per frame**~~ Fixed: first loop now uses `IsColliding` (detect + post event), second loop uses `ResolveCollision` (adjust positions).
- ~~**`Scene.h` — dead member `test_t`**~~ Fixed: removed in Phase 2.
- ~~**Application singleton coupling**~~ Fixed in Phase 5: Scene, Project, Panel, ImGuiLayer, Scripting, AssetManager, EditorLayer, and ProjectHubLayer all receive dependencies (EventDispatcher*, Application*, GLFWwindow*) through constructors instead of calling `Application::GetApplicationInstance()`. Zero singleton calls remain in Hamster-Core; a couple remain in Hamster-Wheel (the project create/open modals passing `&app` to `Project` methods). `HAMSTER_LOG` macro removed; replaced with direct `m_ClientLogger->Log()` calls.
- ~~**`AssetManager` is all-static**~~ Fixed: converted to an instance class owned by `Application` as `std::unique_ptr<AssetManager>`. RAII constructor/destructor replaced `Init()`/`Terminate()` (fixing a bug where `Terminate` didn't clear `m_Scripts`). Mutex removed — all writes happen on main thread. All call sites receive `AssetManager*` through constructors.
- ~~**`Renderer` is all-static**~~ Fixed: converted to an instance class owned by `Application` as `std::unique_ptr<Renderer>`. Constructor replaces `Init()`, empty `Terminate()` removed. All call sites receive `Renderer*` through `Application::GetRenderer()` or constructor injection.
- **Serialization is raw binary and not portable** (`SceneSerialiser`, `ProjectSerialiser`, `AssetManager::Serialise`) — uses `reinterpret_cast` of structs, `size_t`-prefixed strings. Will break across Windows↔Linux or 32-vs-64-bit. Consider switching to a portable format (JSON, MessagePack, or versioned binary) before scene data accumulates. Note: per-asset `.meta` sidecars now use JSON for a single field; they're the entry point if/when this refactor lands.
- ~~**`HAMSTER_WHEEL_SRC_DIR` bakes the source path**~~ Fixed: `HAMSTER_WHEEL_SRC_DIR` macro removed; Hamster-Wheel resources resolve via `GetExecutablePath()`. **Note:** `HAMSTER_CORE_SRC_DIR` still exists — used for shader and font *fallback* paths in `Renderer.cpp`; both try an exe-relative `share/Resources/...` path first (so installed builds work) and fall back to the baked source dir for dev/in-place runs.
- ~~**Build artifacts committed to git**~~ Fixed: untracked and added to `.gitignore`.
- ~~**`Hamster-Py` STATIC target is dead**~~ Fixed: removed from `Hamster-Py/CMakeLists.txt`.
- ~~**Linux install targets in `Hamster-Wheel/CMakeLists.txt`**~~ Fixed: removed in Phase 2.
- ~~**`ImGui::ShowDemoWindow()` left in `EditorLayer::OnImGuiUpdate`**~~ Fixed: removed.
- ~~**`HamsterWheelApp.cpp:32` — uninitialized `EditorLayer*`**~~ Fixed: initialized to `nullptr`.
- **`Hamster-WheelQT/`** — abandoned Qt UI experiment, not in any CMakeLists. Dormant for now; planned to eventually replace Hamster-Wheel.

## Open questions

- ~~What Python version will be used on Windows?~~ Resolved: Python 3.11.
- ~~What is the intended `HamsterPCK` deployment strategy?~~ Resolved: simplified to `import Hamster`; `.pyd` copied flat into project directory, project directory on `sys.path`.
- ~~Should `ScriptingEventDispatcher` be implemented or removed?~~ Resolved: removed in Phase 2 along with `HamsterBehaviour::Subscribe/Post`.
- ~~When Box2D rigid-body dynamics are added, does it replace the custom AABB system entirely, or will both coexist?~~ Resolved: Box2D replaces it entirely. Old `Physics::IsColliding`/`ResolveCollision` removed.
- Should the serialization format be made portable before scene data accumulates (i.e., during refactor phase)?
