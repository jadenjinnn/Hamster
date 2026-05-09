# Architecture

> Living document. Updated via `/architecture-update`. Source of truth for "how this engine fits together." Read top-to-bottom should give a new engineer (or returning author) enough to navigate the codebase confidently in 10 minutes.

## One-paragraph overview

Hamster is a Windows-targeted 2D game engine with an embedded Python scripting layer, designed so that game authors write gameplay logic in Python against a C++ runtime. Three CMake subprojects make up the whole system: **Hamster-Core** (static C++ library — application loop, ECS via EnTT, OpenGL renderer, custom AABB physics, ImGui GUI integration, pybind11 interpreter lifecycle); **Hamster-Py** (a pybind11 extension module that exposes C++ types to Python under the `Hamster` namespace); and **Hamster-Wheel** (the editor executable — ImGui-based scene editor with hierarchy, property editor, asset browser, file browser, console, and project hub). Entities carry Transform, Sprite, Name, Rigidbody, ID, and Behaviour components; the Behaviour component stores instantiated Python objects (subclasses of `HamsterBehaviour`) that receive per-frame callbacks and engine events. A separate runtime-only player (no editor) is planned but not yet implemented.

## Entry points

- `Hamster-Wheel/src/HamsterWheelApp.cpp::main` — sole binary entry point; creates `Application`, loads the UI font, pushes `ProjectHubLayer`, then calls `Application::Run()`.
- `ProjectHubLayer` → on `ProjectOpened` event → creates `EditorLayer` and swaps itself out of the layer stack.
- User-authored `.py` files in the project directory — loaded by `HamsterScript` when the editor's play button is pressed (simulation start).

## Module map

- `Hamster-Core/src/Core/` — `Application` singleton + main loop, `Window` (GLFW wrapper), `LayerStack`, `Scene` + ECS façade, `Project` + `ProjectSerialiser`, `SceneSerialiser`, `UUID`, `Log`/`Logger`, `Components.h` (all component structs)
- `Hamster-Core/src/Events/` — `EventType` enum, `Event` base class, `EventDispatcher` (subscribe-only observer), all concrete event types (`WindowEvents`, `ApplicationEvents`, `InputEvents`, `SceneEvents`, `GuiEvents`)
- `Hamster-Core/src/Renderer/` — `Renderer` (static, OpenGL draw calls, camera/zoom), `Shader`, `Texture`, `FramebufferTexture`, two built-in GLSL shaders (`SpriteShader`, `FlatShader`)
- `Hamster-Core/src/Physics/` — `Physics` (static, custom AABB `IsColliding` + `ResolveCollision`)
- `Hamster-Core/src/Scripting/` — `Scripting` (interpreter lifecycle, default script generation), `HamsterBehaviour` (C++ base class Python scripts inherit from), `HamsterScript` (Python module loader + class scanner)
- `Hamster-Core/src/Gui/` — `ImGuiLayer` (begin/end frame wrapper), `Panel` + `Modal` base classes
- `Hamster-Core/src/Utils/` — `AssetManager` (textures + scripts, UUID-keyed, all-static), `InputManager` (GLFW key polling)
- `Hamster-Py/src/` — pybind11 bindings: `main.cpp` (module entry point), `HamsterBehaviour.h` (trampoline + binding), `Library.h` (vec2/vec3), `Core.h` (Scene/Application/EventDispatcher — opaque), `Input.h` (KeyCodes enum), `UUID.h`, `Log.h`
- `Hamster-Wheel/src/` — `HamsterWheelApp.cpp` (main), `EditorLayer` (scene viewport + entity picking), `ProjectHubLayer` (project open/create flow)
- `Hamster-Wheel/src/Panels/` — `Hierarchy`, `PropertyEditor`, `FileBrowser`, `AssetBrowser`, `Console`, `MenuBar`, `StartPauseModal`, `ProjectSelector`, `ProjectCreator`, `RenameModal`

## Main loop

`Application::Run()` each frame, in order:

1. **Flush pending layer changes** (push/pop queue from the previous frame's `OnUpdate` pass)
2. **`Layer::OnUpdate()`** on every layer in the stack
   - `EditorLayer::OnUpdate` renders to a framebuffer using flat entity colours, reads the pixel under the cursor, and resolves entity selection / guizmo drags
3. **Flush pending layer changes again** — commits any layer transitions triggered during step 2, so ImGui sees the new stack in the same frame
4. **`ImGuiLayer::Begin()`**
5. **`Layer::OnImGuiUpdate()`** on every layer — all panels and the scene viewport image are drawn here
6. **`ImGuiLayer::End()`** — submits ImGui draw commands to OpenGL
7. **`Scene::OnUpdate()`** on the active scene (if any):
   - `OnPhysicsDetect()`: O(n²) loop over all entities with `Rigidbody` — calls `Physics::IsColliding`, posts `CollisionEvent` on the injected dispatcher. Runs even when simulation is paused.
   - If simulation is **not** paused: `OnScriptUpdate()` calls `obj.attr("on_update")(delta_time)` on each Python behaviour; then `OnPhysicsResolve()` runs a second O(n²) pass to adjust positions via `Physics::ResolveCollision`.
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

C++ drives Python (not the reverse). Engine calls `on_create` once and `on_update` every simulation tick. Python calls back into C++ through `HamsterBehaviour` methods (`self.transform`, `self.key_pressed`, `self.log(...)`, `self.subscribe(...)`, `self.post(...)`).

### Python module deployment

The pybind11 module is compiled as `Hamster.pyd` (Windows) / `Hamster.so` (Linux). On project creation, `ProjectCreator` copies the `.pyd` from `Resources/Packages/` into the project directory. `Scripting::AddPathToPy` adds the project directory to `sys.path` on `ProjectOpened`, so `import Hamster` resolves to the `.pyd` in the project folder.

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

Scene::OnUpdate
  ├─ OnPhysicsDetect: O(n²) IsColliding → post CollisionEvent on injected dispatcher
  │     └─ HamsterBehaviour::OnCollision: fills m_CollisionEntities
  └─ [if simulation running]
        ├─ OnScriptUpdate: obj.on_update(dt) for each Python behaviour
        │     scripts read self.transform / self.key_pressed / self.colliding
        │     scripts write self.transform = ...
        └─ OnPhysicsResolve: O(n²) ResolveCollision (adjusts positions)

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
| Box2D | submodule | Physics — vendored but unused; planned for rigid-body dynamics | git submodule (`Hamster-Core/Vendor/box2d`) |
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
- ~~**Application singleton coupling**~~ Fixed in Phase 5: Scene, Project, Panel, ImGuiLayer, Scripting, AssetManager, EditorLayer, and ProjectHubLayer all receive dependencies (EventDispatcher*, Application*, GLFWwindow*) through constructors instead of calling `Application::GetApplicationInstance()`. Zero singleton calls remain in Hamster-Core; only 2 remain in Hamster-Wheel (ProjectCreator/ProjectSelector passing `&app` to Project methods). `HAMSTER_LOG` macro removed; replaced with direct `m_ClientLogger->Log()` calls.
- ~~**`AssetManager` is all-static**~~ Fixed: converted to an instance class owned by `Application` as `std::unique_ptr<AssetManager>`. RAII constructor/destructor replaced `Init()`/`Terminate()` (fixing a bug where `Terminate` didn't clear `m_Scripts`). Mutex removed — all writes happen on main thread. All call sites receive `AssetManager*` through constructors.
- **Serialization is raw binary and not portable** (`SceneSerialiser`, `ProjectSerialiser`, `AssetManager::Serialise`) — uses `reinterpret_cast` of structs, `size_t`-prefixed strings. Will break across Windows↔Linux or 32-vs-64-bit. Consider switching to a portable format (JSON, MessagePack, or versioned binary) before scene data accumulates.
- ~~**`HAMSTER_WHEEL_SRC_DIR` bakes the source path**~~ Fixed: all resource paths now use `GetExecutablePath()` relative to the build output. `HAMSTER_WHEEL_SRC_DIR` macro removed.
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
- When Box2D rigid-body dynamics are added, does it replace the custom AABB system entirely, or will both coexist?
- Should the serialization format be made portable before scene data accumulates (i.e., during refactor phase)?
