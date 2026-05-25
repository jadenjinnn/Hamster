# Hamster — Resume Source Document

> **One-line summary**: Solo 2D C++ game engine for Windows with an embedded Python (pybind11) scripting layer and a full ImGui-based scene editor, including Box2D physics, OpenGL renderer with sprite batching and a quadtree spatial index, sprite-swap animation system, asset hot-reload via a Win32 file watcher, and Unity-style non-destructive play mode — measured across 21 smoke-test scenarios and a benchmark scene.

---

## Headline metrics (verified)

| Metric | Value | Source |
|---|---|---|
| Total git commits | **252** | `git log --oneline \| wc -l` |
| Conventional-prefix commits (recent phases) | feat 34 / fix 10 / refactor 13 / docs 10 / chore 2 / test 1 | `git log --pretty=%s` grouped |
| Time span on GitHub | **2023-12-31 → 2026-05-17** (≈ 17 active months across 2024, 2025, 2026) | `git log --format=%ad` |
| Source files (engine + editor + bindings + tests, excluding vendor) | **108 files** (.cpp/.h/.c/.py) | `Get-ChildItem` over `Hamster-Core/src`, `Hamster-Py/src`, `Hamster-Wheel/src`, `test/` |
| Source lines (same scope) | **≈ 15,416 lines** | line count over the same dirs |
| — Hamster-Core engine | 62 files, 7,175 lines (4,819 .cpp + 2,356 .h) | per-dir line count |
| — Hamster-Py bindings | 9 files, 545 lines (1 .cpp + 8 .h) | per-dir line count |
| — Hamster-Wheel editor | 37 files, 6,696 lines (4,633 .cpp + 2,063 .h) | per-dir line count |
| — Test (smoke + fixtures) | 6 files, 1,000 lines (1 .cpp + 5 .py) | per-dir line count |
| Smoke test scenarios | **21 / 21 passing** | `test/smoke_test.cpp` (counted manually) |
| Features shipped (Phase 5) | **15** | `docs/features/shipped/` |
| Features in flight | **4 active** (game-ui, scene-viewer-improvements, sprite-batching, plus draft) + **1 parked** | `docs/features/active/`, `docs/features/parked/` |
| Bugs logged & resolved | **6 closed**, **2 active** (bug 0008 Low severity, bug 0003 High but believed fixed pending close-out) | `docs/bugs/closed/`, `docs/bugs/active/` |
| Sprite batching benchmark | smoke asserts **2 draw calls for 4 sprites across 2 textures**; viewport culling asserts **2 draw calls when only 2 of 4 sprites are on-screen** | `test/smoke_test.cpp:700-714, 882-900` |
| Quadtree spatial index | bucket cap 8, depth cap 8; rebuilt every frame | `Hamster-Core/src/Utils/SpatialIndex.h:68-69` |
| Handle-leak fix outcome | per-cycle Windows kernel handle growth **114 → 44** after fix; UI duplication symptom **fully eliminated** | `docs/bugs/closed/0002-project-switch-handle-leak.md:107-111` |

*Notes on what's NOT verified:* the spatial-index spec targeted "≥60 FPS at 10,000 sprites with hover-pick active" but the actual draw-call/FPS number from `benchmark_batching.py` was deferred to the next session and is **not yet recorded in the repo** (see `docs/session-handoff.md:16`). Don't quote a 10k-sprite FPS number — quote the **smoke-test draw-call assertions** (verifiable) and describe the benchmark as in-place.

---

## What the project actually is

Plain-English description:

- A 2D game engine intended for teaching beginners Python. Game authors write gameplay in Python (`class Player(Hamster.HamsterBehaviour): def on_update(self, dt): ...`); the engine itself is C++ (`Hamster-Core`, static lib). Python ↔ C++ bridged through **pybind11** with an embedded interpreter.
- Three CMake subprojects:
  - **Hamster-Core** — engine static library: application loop, ECS via **EnTT**, **OpenGL** renderer, **Box2D 3.0.1** physics, ImGui integration, pybind11 interpreter lifecycle.
  - **Hamster-Py** — pybind11 extension module (`Hamster.pyd` on Windows / `Hamster.so` on Linux). Exposes engine types to Python under the `Hamster` namespace.
  - **Hamster-Wheel** — editor executable. ImGui-based scene editor with custom borderless title bar, hierarchy panel, property editor, animation timeline, asset browser, console, file browser, project hub. Renders scene to an FBO and blits into an ImGui image.
- Owner's framing: "personal keystone project being revived after ~1 year dormant. The owner is using it as the centerpiece of their SWE resume" (`CLAUDE.md:5`).

Project README (`README.md`) tagline: "Hamster is a game engine that is intended to be used in the classroom to teach beginners Python programming … its 'pythonic' design rather than the object oriented design of many libraries."

---

## Tech stack (verified from `CMakeLists.txt` and `docs/build.md`)

**Languages**
- **C++20** throughout the engine and editor (`Hamster-Core/CMakeLists.txt:5`, `Hamster-Wheel/CMakeLists.txt:5`)
- **Python 3.11** as the scripting layer (`docs/build.md`)
- **C** (single file — GLAD OpenGL loader)
- **GLSL** — built-in shaders: `SpriteShader`, `FlatShader`, `SpriteBatchShader`, `UIRectShader`

**Toolchain (current, Windows)**
- Compiler: **clang-cl 22.1.5** (LLVM, targets MSVC ABI) — recently bumped from 19.1.7 (`CLAUDE.local.md`, `docs/build.md`)
- Generator: **Ninja**
- Build system: **CMake 3.31.5** (min version 3.28)
- Python: 3.11.9 from `C:/Users/Jaden/AppData/Local/Programs/Python/Python311`

**Vendored third-party libraries** (`docs/architecture.md`)

| Library | Version / source | Purpose |
|---|---|---|
| **pybind11** | git submodule | C++/Python bindings + embedded interpreter |
| **EnTT** | git submodule | Entity-component-system |
| **GLFW** | git submodule | Window, OpenGL context, input |
| **GLAD** | vendored copy | OpenGL function loader |
| **GLM** | git submodule | Math (vec2/3/4, mat4) |
| **Dear ImGui** | vendored copy | Immediate-mode GUI; backends: GLFW + OpenGL3 |
| **Box2D** | 3.0.1, git submodule | 2D rigid-body physics |
| **stb_image** | git submodule | PNG/JPG loading |
| **tinyfiledialogs** | vendored copy | Native file open/save dialogs |
| **Boost 1.86.0** | vendored header-only copy (UUID + container hash) | UUID generation — flagged for replacement |

**Platform-specific bits**
- **Win32 APIs** used directly: `ReadDirectoryChangesW` (file watcher), `SetWindowSubclass` + `WM_NCCALCSIZE` (borderless window with Aero Snap), `Comctl32` linked for `SetWindowSubclass`.
- No package manager; all deps vendored.

---

## Architecture (verified from `docs/architecture.md`)

### Three-binary layout
1. `Hamster-Core` (static lib) — engine guts.
2. `Hamster-Py` (pybind11 module → `Hamster.pyd`) — Python bindings.
3. `Hamster-Wheel` (executable) — editor. Pushes a `ProjectHubLayer` or an `EditorLayer` onto its layer stack.

### ECS components (`Hamster-Core/src/Core/Components.h`)
`Transform`, `Sprite`, `Name`, `Rigidbody` (Box2D backed), `ID` (UUID), `Behaviour` (holds Python objects), `Animation` (sprite-swap keyframes), `Hierarchy` (parent UUID + sibling index), `UIButton`, `UIText` (in-flight UI feature). Each component is serialised via a fixed `ComponentID` enum (1..11) — raw binary, append-tolerant on the read side.

### Application main loop (`Application::Run()`, documented in `docs/architecture.md`)
Per frame, in order:
1. Flush pending layer push/pop changes.
2. Run `Layer::OnUpdate` on every layer (editor renders to an FBO, runs entity picking via the spatial index, resolves selection + grabber drags).
3. Flush pending layer changes again — so ImGui sees the new stack in the same frame.
4. `ImGuiLayer::Begin()`, run `OnImGuiUpdate` on every layer (panels, scene viewport image), `ImGuiLayer::End()`.
5. `Scene::OnUpdate` on the active scene (if simulation is running):
   - `b2World_Step(world, dt, 4)` (4 sub-steps).
   - Read `b2World_GetContactEvents`, resolve UUIDs via `b2Body_GetUserData`, post `CollisionEvent`.
   - Sync Box2D positions/rotations back to Transform (pixels-per-meter conversion).
   - Advance animation keyframes; swap `Sprite::texture`; post `AnimationCompletedEvent` for finished non-looping animations.
   - `OnScriptUpdate()` — `obj.attr("on_update")(delta_time)` on each Python behaviour; first script exception pauses the simulation and logs to the client logger.
6. `Window::Update` — `glfwSwapBuffers` + `glfwPollEvents`.

### C++ ↔ Python boundary

- One embedded interpreter for the lifetime of the process. Initialised in `Application::Application()` via `Scripting::InitInterpreter`; finalised LAST in `Application::~Application` (see Bug 0007 — strict ordering against pybind11 holders).
- Engine drives Python: `on_create` once, `on_update` every simulation tick. Python calls back into C++ via `HamsterBehaviour` methods (`self.transform`, `self.key_pressed`, `self.apply_force`, `self.subscribe`, `self.post`, `self.animate`, `self.create_entity`, `self.destroy_entity`, `self.set_parent`, etc.).
- Bindings registered in a fixed order (`Hamster-Py/src/main.cpp`): Vec2/Vec3/UUID/Transform/BodyType/ColliderShape/Sprite/Rigidbody/EntityHandle/KeyCode/Log/HamsterBehaviour/Scene/Application. Order is load-bearing — a previous fix in `2a417b3a` (`fix: Python bindings — auto-stringify log, __repr__, binding order`) corrected a registration-order crash.
- **User scripts in subdirectories** import as dotted Python modules (`enemies/boss.py` → `import enemies.boss`) via Python 3.3+ namespace packages, no `__init__.py` required.

### Asset identity (Unity-style sidecars — feature shipped 2026-05-17)
- Every script (`.py`) and texture (`.png`/`.jpg`) gets a `<file>.meta` sibling holding `{"uuid": "..."}` in tiny hand-rolled JSON.
- The sidecar is the **authoritative identity record** — UUIDs travel with the file across renames and even external copies.
- `.hanim` (animation) files are **excluded** — the format is self-identifying (UUID lives inside the file). Same dichotomy Unity applies to `.prefab` vs `.png`.
- A Win32 `ReadDirectoryChangesW` worker thread (`ProjectWatcher`) posts add / remove / rename events back to the main thread so external file changes (e.g. renames in VS Code or Explorer) show up live in the asset browser.
- Behaviour components persist a `cachedNames` map (UUID → last-known script name) so the Property Editor can show `MISSING: <cachedName>` in red and offer a one-click "Reassign to" submenu listing currently loaded scripts.
- `Scene::RunSceneSimulation` refuses to start if any Behaviour holds a missing-script reference, logging the entity name + cached script name to the client logger.

### Non-destructive play mode ("simulation snapshot" — feature shipped 2026-05-17)
- Unity-style: scene state is snapshotted into an in-memory `std::string` on play (via `SceneSerialiser` over a `std::stringstream`), restored on stop. Runtime-spawned entities, physics-moved transforms, and script-mutated components all revert when simulation ends.
- PropertyEditor wrapped in `BeginDisabled` and File → Save is gated during play to make the no-mutate guarantee explicit.
- Restore is **deferred to a frame boundary** (`Scene::ProcessPendingRestore` called from `Application::Run` at the top of each frame) to avoid invalidating EnTT iterators when a script's `on_create` exception triggers `PauseSceneSimulation` mid-iteration. Discovered as a critical bug after initial ship, see `docs/features/shipped/2026-05-17-simulation-snapshot.md` spec amendments.

### Sprite batching + spatial index (both shipped 2026-05-17)
- **Sprite batching v1**: replaces per-sprite `glDrawArrays` with a submit/flush API (`Renderer::BeginSpriteBatch` / `SubmitSprite` / `EndSpriteBatch`). Pre-allocated VBO (~1.7 MB); single draw call per (texture, z) boundary. New `SpriteBatchShader`. Draw-call HUD added to LevelEditor. Smoke asserts 2 draw calls for 4 sprites across 2 textures (`test/smoke_test.cpp:700-714`).
- **Quadtree spatial index** (`Hamster-Core/src/Utils/SpatialIndex.h`): stores `(UUID, AABB)` pairs over the ECS. Bucket cap 8, max depth 8. Rebuilt every frame from current Transform state. Two consumers:
  - `Scene::OnRender` — viewport-rect culling via `QueryRect` before sprite submission.
  - `EditorLayer` — `O(log N + k)` cursor-point picking via `QueryPoint`, replacing a previous per-frame full-scene FBO + `glReadPixels` pass.
- Hybrid picking: entity hit-test uses the spatial index; the 8 transform-grabber quads still go through the original FBO color-readback path (trivial cost, reuses existing color-encoded grabber draws).
- Tight AABB for rotated sprites: 4 sin/cos per sprite per rebuild for the four rotated corners.

### Event system (`Hamster-Core/src/Events/`)
- `EventDispatcher` with subscribe/unsubscribe. `Subscribe` returns a `SubscriptionHandle`; `HamsterBehaviour` destructor unsubscribes (fixed in Phase 2).
- Event types: `WindowEvents`, `ApplicationEvents`, `InputEvents`, `SceneEvents` (including `CollisionEvent`, `AnimationCompletedEvent`), `GuiEvents`, plus the in-flight `UIEvents` (`ButtonClickedEvent`).
- Editor panels (Hierarchy, PropertyEditor, Console, AssetBrowser, AnimationPanel) all capture their `SubscriptionHandle` and unsubscribe in their destructors (fix for bug 0006, which would crash the editor on project switch after bug 0002's fix made layers actually get deleted).

### Entity hierarchy (feature shipped 2026-05-17)
- Parent/child organisational only — **no transform inheritance** (deliberate scope decision; deferred as future work).
- `Hierarchy` component (`parent: UUID`, `siblingIndex: uint32_t`) + Scene-owned `m_ChildrenIndex` reverse map (`unordered_map<UUID, vector<UUID>>`).
- Cascading destroy: removing a parent removes the full descendant subtree (DFS, post-order).
- Cycle-creating reparents refused; smoke test asserts refusal does not mutate state.
- Hierarchy panel: recursive render with collapse/expand, drag-to-reparent (drop on row body) and drag-to-reorder-sibling (drop in row gaps), suppressed ImGui's default yellow drop highlight in favour of custom row outline + sibling-gap blue line.
- Python API: `self.parent`, `self.children`, `self.set_parent(uuid)`, `create_entity(name, transform, parent=uuid)`.

---

## Hard problems solved (in author's own words from docs + traceable in code)

These come from `docs/bugs/closed/*.md`, `docs/decisions.md`, and `docs/features/shipped/*.md`. Each was actually debugged, root-caused, and fixed — not a hypothetical.

### Cross-file lifecycle leak across project switches (bug 0002, Tier 2, High severity)
*Symptom*: switching between projects via `File → Open Project` leaked ~114 Windows kernel handles per cycle (588 → 1,500 over 8 cycles), and the Property Editor / Add Component dropdown UI duplicated once per cycle.
*Root cause*: three compounding ownership gaps — (1) `ProjectHubLayer::OnAttach` subscribed to `ProjectOpened` without capturing the `SubscriptionHandle`, (2) `LayerStack::PopLayer` removed from the vector but never `delete`d the layer, (3) no "swap main layer" semantic — the orphaned hub lambda kept pushing new `EditorLayer`s on top of stale ones.
*Fix*: five-file change making layer ownership uniform — `PopLayer` now `delete`s after `OnDetach`; `~Application` drains pending push/pop queues then pops everything remaining (deleting them); `ProjectHubLayer` no longer subscribes; `main.cpp` installs one persistent subscriber that tracks `Layer *mainLayer` and swaps cleanly.
*Outcome*: per-cycle handle growth dropped from 114 → 44, UI duplication symptom fully eliminated, idle decay to stable ~933 from 580 baseline. Required a related fix (bug 0006) for editor panels to capture and unsubscribe their dispatcher handles, otherwise the now-deleted layers' callbacks fired into freed memory and crashed silently.

### Python interpreter finalization vs. pybind11 holders (bug 0007, Tier 1, High severity)
*Symptom*: smoke test process segfaulted at exit after all assertions passed.
*Root cause*: `Application::~Application` called `Scripting::FinaliseInterpreter()` as the first line. After that, implicit member destruction ran in reverse declaration order; both `m_AssetManager` (owning `HamsterScript`s with `pybind11::module_` + `pybind11::handle` members) and `m_Scenes` (Behaviour components holding `std::vector<pybind11::object>`) decref'd Python objects on a dead interpreter — undefined behavior, sometimes benign, started crashing reliably once Phase 7 added more scripts.
*Fix*: reordered the destructor — explicit `m_Scenes.clear()` and `m_AssetManager.reset()` before `FinaliseInterpreter()` at the end. Documented as a general rule in `docs/decisions.md`: "high-level 'shut everything down' calls run LAST."

### Box2D 3.x integration (feature shipped 2026-05-11)
*Problem*: existing custom AABB system (`Physics::IsColliding`, `Physics::ResolveCollision`) had no concept of velocity, gravity, mass, friction, or restitution. Box2D 3.0.1 was vendored but unused.
*Solution*: full integration replacing the custom system. Scene owns a `b2WorldId`, created in `RunSceneSimulation` and destroyed in `PauseSceneSimulation`. Per-frame `b2World_Step(world, dt, 4)`; `b2World_GetContactEvents` → resolve UUIDs via `b2Body_GetUserData` → post `CollisionEvent`; sync `b2Body_GetPosition`/`b2Body_GetRotation` back to Transform (× pixels-per-meter, rad→deg conversion).
*Subtleties*:
  - Coordinate-system mismatch: renderer uses +Y-down screen coords; Box2D defaults to +Y-up. Gravity has to be set as `{0, 10}` not `{0, -10}`.
  - Rotation: Transform.rotation is degrees (used by `glm::rotate`); Box2D returns radians. Conversion at the boundary.
  - Pixels-to-meters scale: 50 PPM (50 px = 1 m). Without this, Box2D simulation is unstable at native pixel scales (designed for 0.1m–10m objects).
  - Serialization format changed: separate `Collider_ID = 7` block rather than appended fields on `Rigidbody_ID`, because the binary serialiser reads a fixed number of fields per component ID — appending would corrupt the parse stream on old files (see `docs/decisions.md`).
*Python API additions*: `self.velocity`, `self.set_velocity`, `self.apply_force(fx, fy)`, `self.apply_impulse(ix, iy)`. Force/impulse divided by PPM before passing to Box2D; velocity multiplied by PPM on read.

### Renderer projection vs. FBO size mismatch for entity picking (decision recorded 2026-05-17)
*Symptom*: spatial-index-based hover picking landed ~380 px too high at maximised window size.
*Root cause*: the renderer's projection covers the full window framebuffer (`m_ViewportHeight`), `glViewport` is the same size, but the level-editor FBO is panel-sized — so only the bottom `panel_h` rows of the projection are actually rendered. ImGui displays the FBO UV-flipped, so panel-top corresponds to FBO-top-row = world Y `cam.y + (vp_h − panel_h) / zoom`, not `cam.y`. The old FBO-pixel-readback pick path didn't have this bug because it sampled the panel-sized FBO directly — the coord mismatch was hidden.
*Fix*: `EditorLayer::PanelMouseToWorld(panelX, panelY)` helper used at all three pick sites. The deeper fix (renderer awareness of actual rendered region) is deferred.

### Custom borderless window with Win11 Aero Snap (decision recorded 2026-05-15)
*Problem*: a borderless GLFW window (for the custom title bar) loses `WS_THICKFRAME`, breaking Aero Snap.
*Fix* (`Window.cpp`): `SetWindowSubclass` re-adds `WS_THICKFRAME` after `glfwCreateWindow`. `WM_NCCALCSIZE` handler clamps the client rect to `mi.rcWork` when `SW_SHOWMAXIMIZED` to kill the DWM accent line that would otherwise show one pixel of white border on maximize.
*Plus*: `glfwSetWindowPos` drag oscillation fix — `glfwGetCursorPos` returns window-relative coords; converting to screen-space (`screenCursor = windowCursor + windowPos`) before computing drag deltas keeps drag stable regardless of window movement.
*Plus*: `Application` constructor posts a synthesised `FramebufferResizeEvent` after subsystems are wired so the renderer picks up the real maximised viewport size instead of the hardcoded 1920×1080.

### Dependency injection refactor (Phase 5, ~7 commits)
*Problem*: deep coupling to a `Hamster::Application::GetApplicationInstance()` singleton across `Scene`, `Project`, `Panel`, `ImGuiLayer`, `Scripting`, `AssetManager`, `EditorLayer`, `ProjectHubLayer`. 34 singleton calls.
*Fix*: introduced constructor-injected `EventDispatcher*` / `Application*` / `GLFWwindow*` parameters across the engine, eliminating the singleton from `Hamster-Core` entirely. Final state: 0 singleton calls in core, 2 remaining in editor (appropriate). Removed the `HAMSTER_LOG` macro in favour of direct `m_ClientLogger->Log()` calls.
*Plus*: `AssetManager` converted from all-static to an `Application`-owned `unique_ptr<AssetManager>` instance (with RAII ctor/dtor replacing `Init()`/`Terminate()`, fixing a bug where `Terminate` didn't clear `m_Scripts`); same for `Renderer` (also fixed an uninitialised-GLM-member bug introduced by the conversion — bug 0001).

### Toolchain bump under live development pressure
clang-cl 19.1.7 → 22.1.5 forced mid-Phase-5 because MSVC STL 14.51 now requires Clang 20+. Tracked in `CLAUDE.local.md` with date and rationale. Documented in `docs/build.md`.

### Performance investigation that pivoted on real evidence
Read-only audit identified four likely causes of slowdown (no vsync, missing GL destructors, per-frame FBO realloc, hover-pick re-renders the whole scene). User attempted ASan on Windows — discovered Windows ASan doesn't support leak detection AND ASan's interceptor crashes the smoke test (Python interop UB) — pivoted plan to **Visual Studio 2022 Diagnostic Tools** for heap snapshots. Idle measurement showed CPU 5–6% on a 16-core box (one core, vsync working) and FPS locked at 60 — refuted hypothesis #1, kept investigation focused on the actually-leaky path (project switching).

---

## Engineering practice (verified by docs and git)

- **Phased development discipline** (`CLAUDE.md`): six explicit phases — architecture reconstruction → Windows build → smoke test → refactor → features → packaging. "Pushback intensity decreases down this list. Phase 1 is the most collaborative; phase 6 is mostly execution." Phases 1–4 complete; Phase 5 in progress.
- **Feature workflow** (`docs/feature-workflow.md`): every feature gets one of three spec tiers based on a Reversibility × Scope matrix (Sketch / Design / Full spec). Every spec must include: classification, in scope, out of scope, API sketch, design, risks, success criteria, test extensions, "Why this approach", future work. Specs are approved before any code is written. **15 such specs have been written, approved, and shipped to date.**
- **Bug workflow** (`docs/bug-workflow.md`): triage by severity (Critical / High / Medium / Low) at capture, Tier (Quick / Investigation / Architectural) at fix time. Tier 2/3 bugs require a reliable reproduction case BEFORE fix; "no reproduction → no fix attempted." Tier 2/3 fixes require the author to write a "What would have prevented this" sentence in their own words. **8 bugs logged (6 closed, 2 active).**
- **Smoke-test-as-guardrail**: a 900-line single-file CTest target (`test/smoke_test.cpp`) exercises 21 scenarios covering the C++ → Python boundary (`on_create` + `on_update` markers, `apply_force`, runtime `create_entity`/`destroy_entity`, animation playback, hierarchy round-trips, asset sidecar reconciliation + rename + missing-script detection, simulation snapshot revert, sprite batching draw-call counts, spatial-index point / rect / rotated-AABB / culling queries). Run via `ctest --test-dir build -R SmokeTest -V`.
- **Decision log** (`docs/decisions.md`): non-obvious technical decisions are dated and explained (currently 9 entries covering colliders, picking, title bar drag, framebuffer clears, application constructor, asset sidecars, file watcher scope, destructor order, snapshot lifecycle, picking coord-system mismatch, spatial-index rebuild policy).
- **Session log** (`CLAUDE.local.md`): every session ends with a one-line entry `YYYY-MM-DD — phase N — <what we worked on> — <where we left off>`. Currently ~37 entries since 2026-05-09.
- **Session handoff** (`docs/session-handoff.md`): dated sections capture unresolved questions/decisions/context not yet in committed docs — bullets, not prose. Read at the start of each session.
- **Git hygiene**: Conventional Commits (`feat:`, `fix:`, `refactor:`, `docs:`, `chore:`, `test:`) used consistently throughout Phase 5. No `Co-Authored-By` AI attribution in any commit (per global preference; a single commit removed past attribution from 32 historical commits and force-pushed). Individual feature work lands as a single feat commit or a small series of commits per phase (e.g., asset-sidecars shipped in 7 commits, one per phase).
- **README "What's new" maintained on every feature ship.** Currently 12 bullet points listing major Phase-5 additions.

---

## Scope signals (raw numbers)

- **Commits**: 252 total. Most-recent burst: 70 commits in May 2026 alone (the active Phase 5 development). First commit Dec 2023; sustained activity 2024-04 through 2024-11, dormant winter 2024–2025, brief return Jan–Feb 2025, full revival May 2026.
- **Files**: 108 source files (excluding vendored libs). 18 resource files (fonts, icons, logo, sprites). 14 docs (`*.md`) outside the auto-generated `Hamster-Docs/`.
- **Lines**: ≈ 15,400 across engine + bindings + editor + tests (excluding vendor). See breakdown above.
- **Editor panels**: 12 distinct (Hierarchy, PropertyEditor, LevelEditor, AnimationPanel, BottomPanel, AssetBrowser, Console, ColliderEditor, CreateProjectModal, OpenProjectModal, RenameModal — all in `Hamster-Wheel/src/Panels/`).
- **Python-facing API surface** (counted from `Hamster-Py/src/main.cpp` registrations): 14 binding groups. Concretely surface includes: `Hamster.HamsterBehaviour` (the base class scripts subclass), `Hamster.Scene`, `Hamster.Application`, `Hamster.Transform`, `Hamster.vec2`/`vec3`, `Hamster.UUID`, `Hamster.key_code` (input enum), `Hamster.LogType`, `Hamster.Sprite`, `Hamster.Rigidbody`, `Hamster.BodyType`, `Hamster.ColliderShape`, `Hamster.EntityHandle`. Each `HamsterBehaviour` script gets `self.transform`, `self.parent`, `self.children`, `self.set_parent`, `self.uuid`, `self.scene`, `self.key_pressed`, `self.velocity`, `self.set_velocity`, `self.apply_force`, `self.apply_impulse`, `self.animate`, `self.stop_animation`, `self.is_animating`, `self.subscribe`, `self.post`, `self.log`, `self.colliding`, `self.collision_entities`, `self.create_entity(name, transform, parent=)`, `self.destroy_entity(uuid)`, `self.on_animation_complete`, `self.on_collision`, `self.on_create`, `self.on_update`, etc.
- **Custom file formats**: `.hamproj` (project, binary), `.hamscene` (scene, binary), `.hanim` (animation keyframes, self-identifying binary), `*.meta` (asset sidecar, JSON), `projects.json` (`%APPDATA%/Hamster/projects.json` for the cross-project registry).

---

## Skills demonstrated

### Hard skills
- **C++20**: templates, `std::unique_ptr` / `std::shared_ptr` ownership, RAII discipline (including a documented destructor-ordering bug fix — see Bug 0007), `std::function` callback registration with handles, EnTT view + group APIs.
- **OpenGL** (3.x via GLAD): shaders, VAO/VBO, framebuffer objects, blits, `glReadPixels` color-encoded picking (later replaced by spatial-index queries).
- **CMake**: multi-target project structure (static lib + pybind11 module + executable + test subproject), `find_package(Python ... Development REQUIRED)`, `pybind11_add_module`, vendored submodules + `add_subdirectory`, custom commands to copy `Hamster.pyd` into project Resources and the editor build dir, build-time `target_compile_definitions` for things like `HAMSTER_PY_MODULE_FILENAME`.
- **Python C-API via pybind11**: embedded interpreter lifecycle, trampoline classes (`HamsterBehaviour`), opaque types (`Scene`, `Application`), `pybind11::object` ownership, registration ordering, debugging UB at interpreter finalization.
- **2D physics**: Box2D 3.x's new C API (`b2WorldId`, `b2BodyId`, `b2World_Step`, `b2World_GetContactEvents`), pixels-to-meters scale design, body lifecycle tied to ECS components via EnTT signals.
- **Spatial data structures**: quadtree implementation with bucket cap + depth cap, tight AABB for rotated rectangles, point query with z-tiebreak, rect intersection query, per-frame rebuild design tradeoff vs incremental updates.
- **Renderer optimisation**: batched draw calls (pre-allocated VBO, single `glDrawArrays` per texture/z boundary), viewport culling via spatial index.
- **Windows native APIs**: Win32 directory change notification (`ReadDirectoryChangesW` worker thread + main-thread event marshalling), borderless-window subclassing (`SetWindowSubclass`, `WM_NCCALCSIZE`), Aero Snap interaction.
- **ImGui**: custom dock-replacement layout, custom borderless title bar (logo, menus, drag, maximise, close), tabbed panel headers, drag/drop with custom drop-target rendering (Hierarchy reparent + reorder), styled popups, font loading (Inter regular/semibold/bold + Font Awesome 6 solid), context-menu pattern, dropdowns, mutually-exclusive panel focus via `IsWindowFocused`.
- **Build tooling on Windows**: clang-cl with MSVC ABI, Ninja, navigating clang-cl-specific flag quirks (`/EHsc` required because pybind11 needs exceptions; `-Wno-unused-command-line-argument` required because Box2D's CMake passes `/experimental:c11atomics` that clang-cl doesn't recognise).
- **Custom serialisation**: raw binary with `reinterpret_cast` of structs, `size_t`-prefixed strings, fixed-field-count component IDs (with a documented portability concern + a known refactor that's been deferred until before Phase 6).

### Meta / engineering skills
- **System design** under self-imposed phase discipline: large-scale refactors (DI, static-to-instance for AssetManager + Renderer) sequenced behind a smoke-test guardrail.
- **Root-cause debugging**: Bug 0002's three-cause investigation (orphan subscription + non-deleting `PopLayer` + no swap-layer semantic) is documented with confirmed hypotheses, evidence (line numbers), and a verified before/after handle count.
- **Risk-driven design**: every feature spec lists concrete risks with specifics (not "performance might be bad" but "tight bbox of 4 rotated corners costs 4 sin/cos per sprite per rebuild"). Specs include explicit "out of scope" sections to fence scope creep.
- **Recovering after dormancy**: Phase 1 deliberately spent reading the whole codebase and writing `docs/architecture.md` from scratch before touching any code — knowing C++ skills had eroded over a year away from the project.
- **Tooling pivot under uncertainty**: ASan investigation pivoted to VS 2022 Diagnostic Tools when Windows-specific limitations surfaced (no leak detection + interceptor crash on Python interop).
- **Process self-discipline**: writing specs before code, separating bug logging from bug fixing ("found mid-task" rule: log, don't fix inline), capturing dated decisions, maintaining session logs, never committing without smoke passing.

---

## Feature catalog (all shipped in Phase 5 unless noted)

From `docs/features/shipped/`:

1. **editor-theme** (2026-05-09) — `HamsterTheme`, Inter font, Font Awesome 6 icons, viewport play/pause/stop overlay, panel audit.
2. **sprite-preview** (2026-05-10) — texture thumbnail with tint, dimensions, inline color picker in the PropertyEditor.
3. **box2d-physics** (2026-05-11, Full spec) — 2D physics integration (see above).
4. **project-hub-redesign** (2026-05-11) — fullscreen card-grid hub with top bar, create-project modal with templates, open-project flow.
5. **project-registry** (2026-05-11) — persistent JSON project list at `%APPDATA%/Hamster/projects.json`, CRUD from hub cards, rename/delete with CWD safety, missing-project detection.
6. **ui-polish** (2026-05-11) — bold typographic hierarchy, card-grid asset/file browsers, refined spacing across all panels.
7. **collider-editor** (2026-05-12) — visual editor for adjusting collider offset/size independently from sprite bounds, drag handles for box and circle shapes.
8. **runtime-entity-management** (2026-05-12) — `create_entity`, `destroy_entity`, `add_component` from Python at runtime.
9. **animation-system** (2026-05-13, Full spec) — `.hanim` file format, Animation component, timeline editor panel with draggable diamond keyframes, preview playback, Python API (`self.animate`, `self.stop_animation`, `self.is_animating`, `on_animation_complete`), `AnimationCompletedEvent`.
10. **editor-rewrite-from-prototype** (2026-05-15) — promoted a UI prototype (`Hamster-UIPrototype`) to the live editor, keeping the polished card-style layout. Borderless title bar, fixed proportional layout, reusable component helpers (`HButton`, `HCombo`, `AxisDotInput`, `SectionHeader`). Old editor preserved as `Hamster-Wheel-old`.
11. **entity-hierarchy** (2026-05-17, Full spec) — see above. 11/11 smoke pass on day of ship.
12. **asset-sidecars** (2026-05-17, Full spec, 7 phases) — see above. 15/15 smoke. Found + fixed bug 0007 during close-out.
13. **simulation-snapshot** (2026-05-17, Design tier) — see above. Required a critical post-ship fix (deferred restore) discovered during sprite-batching benchmark testing.
14. **sprite-batching** (2026-05-17, Sketch tier, v1) — see above.
15. **spatial-index** (2026-05-17, Design tier) — see above. 21/21 smoke on day of ship.

Active (in `docs/features/active/`):
- **scene-viewer-improvements** — dot grid, right-click context menus, zoom slider, axis gizmo, 8-handle selection box. (Largely shipped per session log entries 2026-05-11; spec file still in active.)
- **sprite-batching** — v2 (multi-texture via sampler array) and the formal benchmark measurement that becomes the resume number.
- **game-ui** — UIButton/UIText with stb_truetype-baked font atlas, screen-space anchoring, `ButtonClickedEvent`, `find_entity_by_name`. Started 2026-05-18.

Parked:
- **panel-layout** — superseded by editor-rewrite-from-prototype after the incremental approach was scrapped.

---

## Test coverage

`test/smoke_test.cpp` is a single 900-line CTest target that drives a live `Hamster::Application` end-to-end through 21 numbered scenarios. Each prints `PASS:` or `FAIL:` and the test exits 1 on any failure. Scenarios verified (literal `PASS` strings from the source):

1. `on_create and on_update both fired` — basic C++ → Python boundary
2. `Box2D gravity applied (y: ... -> ...)` — dynamic body falls, lands on static
3. `Python apply_force executed (x moved to ...)` — `self.apply_force` from Python
4. `runtime entity created (count: ...)` — `self.create_entity` from `on_create`
5. `runtime entity destroyed (count: ...)` — `self.destroy_entity` from `on_update`
6. `animation played, stopped on last frame, sprite swapped correctly` — Animation component lifecycle
7. `hierarchy built A->B->C` — `SetParent` builds the tree
8. `cycle-creating reparent refused` — cycle detection
9. `cascade destroy removed full subtree` — recursive destroy
10. `Python hierarchy API (parent/children/set_parent/create_entity(parent=))` — Python-side hierarchy
11. `hierarchy survives serialise round-trip` — `SceneSerialiser` round-trip
12. `sidecar reconciliation (adopt + mint)` — sidecar load semantics
13. `rename via API preserves UUID + moves sidecar` — `AssetManager::RenameAsset`
14. `same-folder collision refused` — rename collision guard
15. `missing-script detection survives scene round-trip` — `Behaviour::cachedNames` persistence
16. `simulation snapshot reverts runtime spawns and component mutations` — non-destructive play
17. `sprite batching emitted N draw call(s) for 4 sprites / 2 textures` — `BeginSpriteBatch`/`SubmitSprite`/`EndSpriteBatch`
18. `spatial index point query (hit, miss, z-tiebreak)`
19. `spatial index rect query (subset + empty)`
20. `spatial index rotated-sprite AABB`
21. `viewport culling — N draw call(s) for 2 visible of 4 total sprites`

Plus `_putenv_s("HAMSTER_TEST_MARKER_DIR", ...)` is set so Python scripts inside the test write marker files to a temp dir that the C++ driver checks.

Fixtures: 5 Python scripts (`smoke_script.py`, `force_script.py`, `spawn_script.py`, `hierarchy_script.py`, `snapshot_script.py`) that exercise specific Python API methods.

No CI is configured in this repo (`.github/` exists but no workflows verified in this scan — flag as unverified). Smoke is run locally via `ctest --test-dir build -R SmokeTest -V`.

---

## What an interviewer might dig into

- **Concrete bug fixes with measurable outcomes**: bug 0002 (114 → 44 handle growth, UI duplication eliminated), bug 0007 (latent UB in destructor ordering — fixed with a one-rule generalisation), bug 0001 (uninitialised GLM member discovered via git bisect from `c0c1f3bb` → `ac35b3f4`).
- **Architectural decisions defended in writing**: per-asset `.meta` sidecars (Unity-style) over a single project manifest (Godot/Unreal-style alternatives considered and rejected for specific reasons in `docs/features/shipped/2026-05-17-asset-sidecars.md` "Why this approach"); quadtree vs uniform spatial hash (rejected: pathological clustering); same-texture sprite batching first then sampler-array v2 (staging for shippable headline number).
- **Cross-language plumbing**: the entire `Hamster-Py/src/main.cpp` is a 30-line binding registration. The depth is in `HamsterBehaviour.h`, `EntityHandle.h`, and `Components.h` under `Hamster-Py/src/` — small, careful surface.
- **Why Python-not-Lua / Python-not-WASM**: the engine targets classroom Python teaching specifically; the API is designed to look "Pythonic" rather than mirror the OOP style of PyGame.
- **What's NOT done**: no standalone player binary (planned, not built), no joints/constraints in physics, no transform inheritance in hierarchy, no portable serialisation format (raw binary `reinterpret_cast` known smell), no Linux build currently (Win32-only chrome — file watcher, Aero Snap, title-bar drag — guarded by `#ifdef _WIN32` but a non-Win32 build path exists in code).

---

## Cited sources

- `README.md` — feature changelog (12 bullets).
- `CLAUDE.md` — project intro, communication style, phase plan.
- `CLAUDE.local.md` — toolchain notes + session log (37+ dated entries from 2026-05-09 to 2026-05-17).
- `docs/architecture.md` — full architecture reference, module map, main loop, dependency table.
- `docs/build.md` — toolchain + configure + build commands, known flag workarounds.
- `docs/conventions.md` — stub (TBD).
- `docs/decisions.md` — 9 dated non-obvious decisions.
- `docs/feature-workflow.md` — feature process governance.
- `docs/bug-workflow.md` — bug process governance.
- `docs/session-handoff.md` — most-recent dated session contexts.
- `docs/features/shipped/*.md` — 15 shipped specs.
- `docs/features/active/*.md` — 4 in-flight specs (game-ui, scene-viewer-improvements, sprite-batching).
- `docs/features/parked/panel-layout.md` — one parked spec.
- `docs/bugs/closed/000[1-7]-*.md` — 6 closed bugs (Tier 1/2, mix of High/Critical/Medium).
- `docs/bugs/active/0003-*.md`, `docs/bugs/active/0008-*.md` — 2 open bugs.
- `test/smoke_test.cpp` — 900-line CTest driver with 21 PASS strings.
- `Hamster-Core/CMakeLists.txt`, `Hamster-Py/CMakeLists.txt`, `Hamster-Wheel/CMakeLists.txt`, `test/CMakeLists.txt`, root `CMakeLists.txt` — build wiring.
- `Hamster-Core/src/Hamster.h` — single-include header summary.
- `Hamster-Core/src/Utils/SpatialIndex.h` — quadtree definition with `kBucketCap = 8`, `kMaxDepth = 8`.
- `Hamster-Core/src/Renderer/Renderer.h` — batching API.
- `Hamster-Core/src/Core/Components.h` — every component struct.
- `Hamster-Py/src/main.cpp` — binding registration list.

---

## Caveats and unverifieds (read before quoting numbers)

- **Insertion/deletion totals in `git log --shortstat` are inflated** by vendored library additions (ImGui, GLFW, EnTT, Box2D, Boost headers all committed via `add_subdirectory`). The 3,975,940 insertion count is real but not meaningful as a "code written" metric. The **15,400 source-line count** (excluding `Vendor/`) is the honest measurement.
- **The 60 FPS / 10,000-sprite spatial-index claim is not yet measured.** `docs/session-handoff.md:5-16` says the benchmark numbers must be read off the HUD in `build-release/` and "written into the resume bullet" — that hasn't happened. **Don't quote a 10k-FPS number until measured.** Smoke-test draw-call assertions (2 for 4 sprites; 2 for 2 visible after culling) ARE verifiable and SHOULD be cited.
- **GitHub Actions / CI**: a `.github/` directory exists but its contents weren't read during this scan. Flag as "no CI verified."
- **No published deployment yet.** Phase 6 (Windows redistributable / installer) hasn't started.
- **Docs site (`Hamster-Docs/`) is a Starlight skeleton** (`Hamster-Docs/README.md`) — the placeholder README content is unchanged. The README's "Getting Started → https://doritothepug.github.io/Hamster" link is for an aspirational docs site; current contents are the default Astro Starlight starter template.
- **Bug 0003 (GetScript throws on missing UUID)** is logged as "active" but per `docs/session-handoff.md:23` the underlying behaviour was changed in the asset-sidecars feature (`GetScript` now returns `nullptr`). Author may close it pending verification.
- **The owner's machine details** (CPU, GPU model) needed for benchmark provenance are not in any committed doc.
- The owner's framing of this as a **"resume centerpiece"** is in `CLAUDE.md:5` — that's documentation, not external validation.
