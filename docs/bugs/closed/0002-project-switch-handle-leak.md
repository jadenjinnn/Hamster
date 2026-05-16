# Bug 0002: project switches leak ~114 Windows kernel handles per cycle

> Status: **closed (root cause fixed; residual growth remains, see Verification)**
> Severity: **High**
> Tier: **2**
> Logged: 2026-05-15
> Closed: 2026-05-16
> Found while: performance/leak investigation (`plans/velvety-crafting-sketch.md`)

---

## Symptom

Switching projects in the editor leaks Windows kernel handles. Measured via Task Manager → Details → Handles column: `Hamster-Wheel.exe` starts at ~588 handles with one project open, grows to ~1500 after 8 open/close cycles — roughly +114 handles per cycle, scaling linearly. GDI Objects and User Objects do not grow. Memory working set grows but was not separately quantified in this test.

The kernel-handle count is the *Windows* metric; the read-only audit also predicted OpenGL handle leaks (driver-side) from missing destructors on `Texture`, `Shader`, and `FramebufferTexture`. Those are not visible in Task Manager but are almost certainly part of the same broken-teardown picture.

**Visible UI accumulation**: each project switch also duplicates editor state observable in the panels — dropdown menus gain another full copy of their option set per switch (e.g. after N switches, the Add Component dropdown has every option listed N times), and the Property Editor shows the "select an entity first" placeholder N times when no entity is selected. Strongly suggests `EditorLayer` (and its owned panels) is being pushed onto the layer stack on each project open without the previous one being popped — so N stacked EditorLayers render their UI on top of each other, and each one also keeps its own subscriptions / resources alive, which would explain the kernel-handle growth too.

## Suspected location

- `Hamster-Core/src/Utils/AssetManager.h/.cpp` — `m_Textures`, `m_Scripts`, `m_Animations` maps live for the lifetime of the `Application`, never cleared on project close. Per-project assets accumulate forever.
- `Hamster-Core/src/Renderer/Texture.h` — no destructor, no `glDeleteTextures`. Every loaded sprite leaks a GL texture handle (kernel section on Windows via DXGI).
- `Hamster-Core/src/Renderer/Shader.h` — no destructor, no `glDeleteProgram`.
- `Hamster-Core/src/Renderer/FramebufferTexture.h` — no destructor; leaks FBO + color texture + renderbuffer per construction.
- `Hamster-Core/src/Renderer/Renderer.cpp:90–93` — `glGenBuffers` writes to a local `VBO` variable that goes out of scope, leaking the VBO handle on every Renderer construction; `m_VAO` is stored but never deleted.
- `Hamster-Core/src/Core/Scene.h` — no destructor. `DestroyPhysicsWorld()` exists but is only called from `PauseSceneSimulation`. If a project is closed mid-simulation or never simulated, the Box2D world + body handles leak.
- `Hamster-Core/src/Utils/AssetManager.cpp` — `AddTextureAsync` spawns background threads; thread handles may not be joined when their AssetManager owner is destroyed.
- `Hamster-Wheel/src/ProjectHubLayer.cpp` and the `ProjectOpened` event handler — strongest candidate for the UI-duplication symptom. The hub likely pushes a new `EditorLayer` on `ProjectOpened` without popping itself or the previous `EditorLayer`. Check whether the close-project path pops the `EditorLayer` before returning to the hub.
- `Hamster-Core/src/Core/Application.cpp` — `LayerStack` push/pop behavior on project change. May need an explicit "swap top layer" semantic instead of plain push.

The original audit findings are listed in detail in `C:\Users\Jaden\.claude\plans\velvety-crafting-sketch.md`.

## Reproduction

1. Build: `cmake --build build --target Hamster-Wheel -j 8` (skip if already built).
2. Launch `build\Hamster-Wheel\Hamster-Wheel.exe`.
3. Open Task Manager → **Details** tab → right-click any column header → **Select columns** → enable **Handles**. Find the row for `Hamster-Wheel.exe`.
4. From the project hub, open project A. Wait for the editor to finish loading. Note the Handles count.
5. Close project A. Open project B. Close project B. Open project A again. Repeat for 5–10 total open/close cycles.
6. Observe: Handles count grows roughly linearly by ~114 per open/close cycle. Measured: 588 → ~1500 over 8 cycles.
7. After a few cycles, open the **Add Component** dropdown in the Property Editor — observe every option listed multiple times (once per accumulated `EditorLayer`). With no entity selected, the Property Editor area also shows the "select an entity first" placeholder once per stacked layer.

Expected: Handles count should stabilize (small jitter is fine) — each close+open should release what it acquired. Dropdowns and placeholders should each appear exactly once regardless of how many project switches have happened.

---

## Investigation (Tier 2/3 only)

### Hypotheses considered

- **H1: `ProjectHubLayer` subscribes to `ProjectOpened` and never unsubscribes — confirmed.**
  - `ProjectHubLayer::OnAttach` (`ProjectHubLayer.cpp:26–32`) calls `m_Dispatcher->Subscribe(ProjectOpened, [this](...) { ... })`. The returned `SubscriptionHandle` is discarded.
  - No `OnDetach`, no destructor — nothing ever removes the subscription. The lambda's captured `this` outlives the hub's presence in the layer stack.

- **H2: `LayerStack::PopLayer` does not delete the popped layer — confirmed.**
  - `LayerStack.cpp:16–23`: pops from the vector and calls `OnDetach`, but never `delete layer`. The raw `Layer*` is leaked on every PopLayer call.
  - `Application::~Application` (`Application.cpp:106–118`) also doesn't iterate the layer stack to delete remaining layers — only saves scenes.

- **H3: There is no "Close Project → return to hub" code path — confirmed.**
  - Searched `Hamster-Wheel/src/` for PushLayer/PopLayer: only two call sites — `main.cpp:23` (push hub at startup) and `ProjectHubLayer.cpp:29–30` (push EditorLayer + pop hub on `ProjectOpened`).
  - No code anywhere creates a new `ProjectHubLayer` after startup.

- **H4: Project switching from the editor is via `File → Open Project` — confirmed.**
  - `EditorLayer.cpp:353` menu item opens `ProjectSelector`, which calls `Hamster::Project::Open(...)` (`ProjectSelector.cpp:62`), which posts `ProjectOpened`.

### Evidence

Leak sequence per project switch from the editor:

1. App start: `main()` creates `ProjectHubLayer`, pushes it. `OnAttach` subscribes to `ProjectOpened`.
2. User opens first project from hub: lambda fires → creates `EditorLayer #1` → pushes it → calls `PopLayer(this)` on the hub. The hub is removed from the layer stack but **not deleted**, and its subscription **remains active**.
3. User selects `File → Open Project` and picks another `.hamproj`: `Project::Open` posts `ProjectOpened`.
4. The orphaned hub's lambda fires *again* (its captured `this` is still valid, the subscription is still active). It creates `EditorLayer #2`, pushes it, tries to `PopLayer(this)` — but the hub isn't in the stack anymore, so the pop is a no-op.
5. Now both `EditorLayer #1` and `EditorLayer #2` are in the layer stack. Every frame, `Application::Run` calls `OnUpdate` and `OnImGuiUpdate` on both. Each renders its own Property Editor, Hierarchy, BottomPanel, etc. → the observed UI duplication.
6. Each `EditorLayer` constructor allocates a `FramebufferTexture` (3 GL handles → kernel section handles), a logo `Texture` (1 GL handle), plus all its panels (each with their own dispatcher subscriptions). None of those have destructors that release GL handles, but they also stay alive forever since `EditorLayer #1` is never popped — explaining both the kernel-handle growth and the UI duplication.

---

## Root cause

Three independent ownership/lifecycle gaps compound into the observed leak. (1) `ProjectHubLayer` subscribes to `ProjectOpened` in `OnAttach` but never unsubscribes — its lambda persists for the lifetime of the process. (2) `LayerStack::PopLayer` removes a layer from the vector and calls `OnDetach` but never `delete`s it, so the hub's heap allocation (and its captured `this` lambda) outlive the layer stack. (3) The system has no "swap the main layer" semantic — the hub's lambda is the *only* code that pushes an `EditorLayer`, so when `File → Open Project` posts `ProjectOpened` a second time, the orphaned hub lambda is what handles it and naively pushes a *new* `EditorLayer` on top of the existing one instead of replacing it. The accumulated `EditorLayer`s each keep their FramebufferTexture / logo Texture / panel objects / dispatcher subscriptions alive, producing both the ~114-handle-per-cycle growth and the visible UI duplication.

## What would have prevented this (AUTHOR WRITES — Tier 2/3 only)

<!-- AUTHOR: write one sentence in your own words -->
Make sure resources pushed to a stack are properly destroyed upon pop

---

## Fix

Five-file change to make layer ownership uniform and remove the orphaned subscription:

1. **`Hamster-Core/src/Core/LayerStack.cpp`** — `PopLayer` now `delete`s the layer after `OnDetach` + `erase`. LayerStack owns its layers.
2. **`Hamster-Core/src/Core/Application.h`** — `ImGuiLayer m_ImGuiLayer` → `ImGuiLayer *m_ImGuiLayer`. Required so it can live in the LayerStack like every other layer; otherwise PopLayer would `delete` a value member.
3. **`Hamster-Core/src/Core/Application.cpp`** — `m_ImGuiLayer = new ImGuiLayer()` in the constructor; `.` calls become `->`. `~Application` now drains `m_LayersPendingPush` (these never made it to the stack), clears `m_LayersPendingPop`, then `PopLayer`s everything still in `m_LayerStack` (which deletes them, including `m_ImGuiLayer`). Done before the scene-save loop so layers are gone before the asset/scene state they reference.
4. **`Hamster-Wheel/src/ProjectHubLayer.cpp/.h`** — removed `OnAttach` entirely (and its `ProjectOpened` subscription), removed the stale `EditorLayer *m_EditorLayer` member, removed the now-unused `EditorLayer.h` include from the header.
5. **`Hamster-Wheel/src/main.cpp`** — installs a persistent `ProjectOpened` subscription that captures a `Hamster::Layer *mainLayer` local. Each `ProjectOpened`: `PopLayer(mainLayer)` (queued, deleted on next flush), `mainLayer = new EditorLayer(app)`, `PushLayer(mainLayer)`. Single owner of the swap semantic.

Build verified: `cmake --build build --target Hamster-Wheel` succeeded after upgrading LLVM 19.1.7 → 22.1.5 (pre-existing toolchain mismatch with MSVC 14.51 STL, unrelated to this bug). Smoke test passes (`ctest -R SmokeTest`).

## Verification

- [x] Build succeeds, smoke test passes throughout the fix series.
- [x] UI duplication symptom (stacked dropdowns + "select entity first" placeholders) fully gone — confirmed by user during repro of 0006.
- [x] Original repro re-run by user (8 open/close cycles, Task Manager → Handles).
  - **Before**: 588 → ~1500 over 8 cycles (~114/cycle), no idle decay.
  - **After 0002 fix alone**: still ~114/cycle (other root causes still present).
  - **After 0002 + 0004 + 0005 + 0006 fixes** (subscription cleanup) **+ GL destructors** (Texture/Shader/FramebufferTexture) **+ per-frame FBO-resize guard**: 580 → ~1500 peak → decays to ~933 stable after idle. Persistent: ~44/cycle. No perceptible lag during sustained use.

**Conclusion**: bug 0002's named cause (orphaned `ProjectHubLayer` subscription + `LayerStack::PopLayer` not deleting + no main-layer swap semantic) is fixed. Stacked-layer UI duplication is fully eliminated. Residual ~44 handles/cycle remains and is suspected to come from sources outside this bug's scope — Box2D world recreation, dispatcher subscriptions still leaked by `Scene::SceneCreated` (never unsubscribed), or driver-managed resources with delayed reclamation. Closing 0002 since its named root causes are resolved; tracking the residual as future work (not currently a critical-path issue per author).

---

## Related

- Plan: `C:\Users\Jaden\.claude\plans\velvety-crafting-sketch.md` — read-only audit that surfaced the underlying code-level issues.
- Session log: `2026-05-15 — Phase 5 — performance/leak investigation started`.
