# Feature spec: Editor rewrite from UI prototype

> Tier: **Full spec**
> Status: **approved**
> Started: 2026-05-15
> Approved: 2026-05-15
> Spec author: Jaden (elicited by Claude)

---

## Classification

- **Reversibility**: Sticky — replaces the entire editor frontend; backing it out means restoring a deleted directory.
- **Scope**: Cross-cutting — every panel, EditorLayer, entry point, theme, build target are all touched.
- **Tier rationale**: Sticky + cross-cutting = Full spec.

---

## Problem

The current `Hamster-Wheel/` editor frontend grew organically and uses ImGui docking. Multiple attempts to retrofit a polished card-layout visual style on top of docking failed because ImGui docking forces `WindowRounding = 0` and renders panels flush. A clean visual prototype (`Hamster-UIPrototype/`) was built standalone — it's modular (Theme, Panel, Components, per-panel files), uses manual proportional layout, has reusable UI helpers (`HButton`, `HCombo`, `HDragFloat`, `AxisDotInput`, `SectionHeader`), and matches the target design. Rather than continue retrofitting, this spec promotes the prototype to *become* the editor: keep the prototype's source as the visual foundation, port the engine backend into it, then delete the old `Hamster-Wheel/`.

## In scope

- Add Hamster-Core + Hamster-Py engine dependencies to `Hamster-UIPrototype/CMakeLists.txt`
- Replace prototype's `main.cpp` with engine-integrated entry: create `Application`, push an editor `Layer`
- Per-panel backend wiring (keep prototype's visual code, swap fake data for engine data):
  - **PropertyEditor** — read `m_Transform`, `m_Sprite`, `m_Rigidbody`, `m_Animation`, `m_Behaviour`; add sections the prototype doesn't have (Sprite preview, Scripts list, Animation list, "Add Component" popup)
  - **Hierarchy** — iterate `registry.view<Hamster::Name>()`; selection, search filter, add/rename/delete entity
  - **LevelEditor** — replace checkerboard with `FramebufferTexture` blit; bring over entity picking (`glReadPixels` FBO trick), translate gizmo, 8-handle resize grabbers, zoom slider, axis gizmo, FPS counter, play/pause/stop overlay, right-click context menus
  - **AssetBrowser** — iterate `AssetManager` texture/script maps; import asset, new script, rename, delete (using prototype's card grid layout)
  - **AnimationPanel** — iterate AssetManager animations; timeline editing with draggable keyframes; save/load `.hanim`
  - **BottomPanel** — add a third **Console** tab tailing the scene's client logger
- Bring over modal/utility windows: `MenuBar` (File → Save / New Project / Open Project), `ColliderEditor`, `RenameModal`, `ProjectCreator`, `ProjectSelector`
- Keep the prototype's custom borderless title bar with drag / maximize / close, with the File menu integrated into it (as it is today in the prototype)
- Apply the prototype's theme palette unchanged; extend font globals only as needed
- Once everything works end-to-end: delete `Hamster-Wheel/`, rename `Hamster-UIPrototype/` → `Hamster-Wheel/`, update `CMakeLists.txt` root references

## Out of scope

- **ProjectHubLayer** — for v1, open a default project directly; project hub is a separate follow-up
- Functional "..." ellipsis menus on panel headers — still placeholder buttons
- User-resizable panel borders (drag-to-resize between panels)
- User-rearrangeable panel order / save layout preferences
- Migrating the existing serialization format — scenes load and save with the same `.hs`/`.hamproj` formats
- Python API changes — no engine-facing changes; `HamsterBehaviour` etc. stay identical
- The standalone `Hamster-UIPrototype` build target as a separately-shippable thing — it's consumed and replaced

## API sketch

No Python or public API changes. This is editor-internal. Backend stays as-is — we change only what consumes it.

```cpp
// New Hamster-UIPrototype/src/main.cpp (replacement)
int main() {
    auto *app = new Hamster::Application();
    std::string resourcePath = Hamster::Application::GetExecutablePath() +
        "/../share/Resources/Hamster-Wheel/Resources";

    ImGuiIO &io = ImGui::GetIO();
    ApplyTheme();           // prototype's theme namespace
    LoadFonts(io, resourcePath);

    // For v1, open a default project directly (skip ProjectHub)
    Hamster::Project::Open(defaultProjectPath, app);

    app->PushLayer(new EditorLayer(app, app->GetActiveScene()));
    app->Run();
    delete app;
}
```

```cpp
// EditorLayer is a Hamster::Layer that owns the prototype's render loop body.
// Its OnImGuiUpdate calls the prototype's positioned-window code, but each
// RenderPropertyEditor() etc. now reads from the real Scene/Application.

class EditorLayer : public Hamster::Layer {
    void OnImGuiUpdate() override {
        // Layout math from prototype's main.cpp
        // Custom title bar (kept from prototype)
        // Position each panel window with kPanelFlags
        // Call RenderPropertyEditor(m_PropertyEditorState),
        //      RenderHierarchy(m_HierarchyState),
        //      RenderLevelEditor(m_LevelEditorState),
        //      RenderBottomPanel(m_BottomState)
    }
};
```

---

## Design

### Data structures

No new component types or runtime data. The visual code (prototype's `Theme.h`, `Panel.h`, `Components/`, `Panels/*.h`) stays as free functions / structs. Each panel file gets either:
- A small `State` struct that holds what's currently in file-scope statics (`g_SelectedEntity`, `g_SelectedKF`, etc.), now owned by `EditorLayer` as members and passed by reference; **OR**
- Existing class-based panels (`PropertyEditor`, `Hierarchy`, etc.) where the engine-data reading lives on the class, and the visual rendering free function reads from the class.

Decision: convert the prototype's panel free functions into small classes owned by EditorLayer (mirroring current `Hamster-Wheel` structure). Each class holds engine pointers (`Application*`, `Scene*`, `AssetManager*`) and panel-local state.

### Module touchpoints

- `Hamster-UIPrototype/CMakeLists.txt` — add link to `Hamster-Core`, include paths for box2d, entt, glm, boost, pybind11; bake `RESOURCE_DIR` to point at `Hamster-Wheel/Resources/`
- `Hamster-UIPrototype/src/main.cpp` — replace with `Hamster-Wheel`-style entry that creates Application, opens a default project, pushes EditorLayer
- `Hamster-UIPrototype/src/EditorLayer.h/.cpp` — **new**, owns the panel layout, title bar, layout math
- `Hamster-UIPrototype/src/Panels/PropertyEditor.h/.cpp` — converted to class, wired to Transform/Sprite/Rigidbody/Animation/Behaviour
- `Hamster-UIPrototype/src/Panels/Hierarchy.h/.cpp` — converted to class, wired to registry
- `Hamster-UIPrototype/src/Panels/LevelEditor.h/.cpp` — converted to class, FBO + picking + gizmos
- `Hamster-UIPrototype/src/Panels/BottomPanel.h/.cpp` — class with three tabs (AssetBrowser, Animation, Console)
- `Hamster-UIPrototype/src/Panels/AssetBrowser.h/.cpp` — **new** sub-renderer for the asset tab
- `Hamster-UIPrototype/src/Panels/AnimationPanel.h/.cpp` — already exists; wire to AssetManager
- `Hamster-UIPrototype/src/Panels/Console.h/.cpp` — **new** sub-renderer for the console tab
- `Hamster-UIPrototype/src/Modals/` — new directory for `RenameModal`, `ProjectCreator`, `ProjectSelector`, `ColliderEditor`
- `Hamster-UIPrototype/src/MenuBar.h/.cpp` — **new**, drawn inside the custom title bar (the prototype already has the File menu sketch — wire it up to Save/New Project/Open Project actions)
- `Hamster-Core/src/Gui/ImGuiLayer.cpp` — remove `ImGuiConfigFlags_DockingEnable` (docking is no longer used)

**Untouched in this feature**:
- All of `Hamster-Core/` (except the docking-flag removal)
- All of `Hamster-Py/`
- `Hamster-Wheel/Resources/` — the prototype reuses these (fonts, icons, logo)
- The `.pyd` packaging path

**Deleted at close-out** (after end-to-end verification):
- `Hamster-Wheel/src/` (entire directory)
- Rename `Hamster-UIPrototype/` → `Hamster-Wheel/`, update root `CMakeLists.txt`

### Lifecycle / control flow

Same engine main loop as today (`Application::Run`). The differences are all in the editor's `OnImGuiUpdate`:

1. **Custom title bar** — `ImGui::Begin("##TitleBar", ..., MenuBar)`; draws logo, menu items (File/Edit/View/Build/Window/Help), and window controls (minimise/maximise/close). Drag-handling logic stays as-is from the prototype.
2. **Layout math** — read window size, compute panel rects (`leftW = 0.175 * totalW`, `rightW = 0.145 * totalW`, etc.).
3. **For each panel**: `SetNextWindowPos` + `SetNextWindowSize` + `Begin(name, nullptr, kPanelFlags)` + `panel.Render()` + `End`.
4. **Modal popups** (`ColliderEditor`, `RenameModal`, etc.) render after the main panels.

Per-frame data flow inside each panel is unchanged from the current editor — same FBO picking, same gizmo logic, same Python script callbacks, same EnTT registry queries.

### Edge cases

- **No project open** — for v1 with project-hub skipped, the editor opens with a hardcoded default project path. If that path doesn't exist, log error and bail with a placeholder empty scene rather than crashing.
- **Window resized below minimum** — already handled by `glfwSetWindowSizeLimits(1024, 600, ...)` from the prototype.
- **Title bar drag while maximised** — already handled in prototype's drag logic.
- **Panel content overflows** — prototype's `BeginContent(scrollable=true)` wraps content in a scrollable child window. Carries over.
- **Asset browser MenuBar** — current editor uses `ImGuiWindowFlags_MenuBar` for "File → Import Asset / New Script". Prototype doesn't. Replace with inline buttons at top of the asset tab.

---

## Full spec

### Sequence diagrams / data flow

```
main()
  └─ create Application
  └─ ApplyTheme(), LoadFonts()
  └─ Project::Open(defaultPath, app)   [v1: hardcoded; v2: ProjectHubLayer]
  └─ PushLayer(EditorLayer)
  └─ app->Run()
        └─ each frame:
              EditorLayer::OnImGuiUpdate()
                ├─ TitleBar (logo, menus, window controls)
                ├─ Layout math
                ├─ PropertyEditor window (positioned)
                │     └─ reads m_Hierarchy->GetSelectedEntity()
                │     └─ renders Transform/Sprite/Scripts/Rigidbody/Animation sections
                ├─ LevelEditor window (positioned, padding 0)
                │     └─ blits m_FramebufferTexture
                │     └─ handles picking, gizmos, grabbers, context menus
                │     └─ overlays: play/pause/stop, FPS, zoom slider, axis gizmo
                ├─ Hierarchy window (positioned)
                │     └─ iterates registry.view<Hamster::Name>()
                │     └─ selection + search + add/rename/delete
                └─ BottomPanel window (positioned, tabbed)
                      └─ tab 0 → AssetBrowser sub-render
                      └─ tab 1 → AnimationPanel sub-render
                      └─ tab 2 → Console sub-render
              Scene::OnUpdate()  [unchanged: physics, scripts, animation tick]
              Window::Update()
```

### Error handling

- **Default project missing**: log error to stderr, continue with an empty in-memory scene
- **Texture/script asset load failures**: existing behaviour preserved — `AssetManager` logs and continues
- **Python script exception during simulation**: existing behaviour — pause simulation, log to client logger, surface in Console tab
- **Panel rendering exceptions**: not expected (no per-frame allocations that can throw); ImGui drawing failures are not exceptions

### Performance considerations

- **Per-frame overhead**: identical to current editor — same number of windows, same FBO blit, same scene update. The visual chrome (header rects, custom tabs) is a few extra `DrawList::AddRectFilled`/`AddLine` per frame; negligible.
- **Font atlas**: adding `Inter-SemiBold` 15px + 17px + 28px (already in `Resources/Fonts/`) plus 36px FA icons increases atlas size; one-time cost at startup.
- **Build time**: linking Hamster-Core + Hamster-Py + box2d into the prototype target adds ~30–60s vs current prototype build; comparable to current Hamster-Wheel build.

### Migration / compatibility

- **No data format changes**. Existing `.hs` scenes, `.hamproj` project files, `.hanim` animations load unchanged.
- **No Python API changes**. Existing scripts run unchanged.
- **Existing user projects** continue to work — the editor reads/writes the same formats from the same locations.
- **`imgui.ini`** is no longer used (docking layout file). Safe to delete; will be regenerated empty or simply unused.
- **Last-known-good before this work**: commit `370efe28` (animation system shipped).

---

## Why this approach

**Alternatives considered**:

1. **Continue retrofitting `Hamster-Wheel` to look like the prototype** (the abandoned approach from 2026-05-15). Each panel's `Begin`/`End` becomes a wrapper around a manual layout system. Theme gets updated. Custom panel chrome gets added. The problem: the prototype is the source of truth for the look-and-feel, and every visual decision (`HButton` background colour, `AxisDotInput` rounded-corners-on-one-side trick, spacing) has to be re-derived in the existing panels. Discovered mid-port that the result was a Frankenstein of old visual code with new chrome stitched on. Slow, error-prone, and the existing `Hamster-Wheel/Panels/*` files have a lot of accumulated detail that conflicts with the prototype's cleaner structure.

2. **Manually copy prototype files into `Hamster-Wheel`, replacing each panel one-by-one in-place**. Mid-ground between (1) and the chosen approach. Rejected because we'd still have to delete a lot of existing code; cleaner to start from the prototype's directory structure and add to it than to delete-and-paste into the old structure.

3. **Chosen approach: promote the prototype to be the editor**. The prototype's directory structure becomes the new editor. Each panel's visual code stays; engine data wiring gets added. At end, rename the directory.

**Tradeoffs accepted**:

- One big migration vs many small edits. The diff at the close-out commit is large. Mitigated by working on `Hamster-UIPrototype` directly without touching `Hamster-Wheel` until the very end; if anything goes wrong, `Hamster-Wheel` is intact as fallback.
- Some functionality from the old editor doesn't have a 1:1 prototype equivalent (Hierarchy's flat list vs prototype's tree, AssetBrowser's MenuBar vs inline buttons). Each gets resolved by adapting the prototype's pattern, not regressing UX.
- Custom borderless title bar inherits the prototype's drag/maximise logic. It's already working; we accept the Win32-specific code (`GLFW_EXPOSE_NATIVE_WIN32`) and revisit for Linux later.

## Risks / what could go wrong

1. **Build configuration drift** — `Hamster-UIPrototype/CMakeLists.txt` currently only links imgui + glfw. Adding `Hamster-Core` brings in box2d, entt, glm, boost UUID, tinyfiledialogs, pybind11. Wiring all of these correctly with the existing `clang-cl` flags (`/EHsc`, `Wno-unused-command-line-argument`) is non-trivial; could miss a flag and get unexpected linker errors. Mitigation: copy the relevant target_include_directories/target_link_libraries blocks from `Hamster-Wheel/CMakeLists.txt` verbatim as starting point.
2. **`RESOURCE_DIR` macro collision** — prototype already bakes `RESOURCE_DIR` for its font path. `Hamster-Wheel` uses `Application::GetExecutablePath() + "/../share/..."` for runtime resource lookup. Risk of inconsistency between asset loading paths. Mitigation: prototype switches to the same runtime path strategy.
3. **`Application` singleton coupling** — current editor has 2 remaining `Application::GetApplicationInstance()` calls (in ProjectCreator/ProjectSelector). When porting those into the new editor, we must continue passing `Application*` to them, not reintroduce the singleton call.
4. **FBO mouse-coordinate mapping** — Level Editor picking uses `ImGui::GetMousePos() - m_ViewportOffset` where `m_ViewportOffset` is computed from window position. Moving from docking to positioned windows changes how that offset is derived. Risk of misaligned picks. Mitigation: keep the same `GetCursorScreenPos()` / `GetContentRegionAvail()` pattern from current editor; the position-source change is transparent to that math.
5. **Title bar + ImGui main viewport** — when ImGui owns the entire window (custom title bar via ImGui), there's no GLFW window decoration. `glfwSetWindowSizeLimits` still works; `glfwIconifyWindow`, `glfwMaximizeWindow`, drag handling all need to live in the editor's update loop. The prototype already has this working; risk is reintroducing it correctly during the integration.
6. **`ImGui::BeginMainMenuBar` vs custom title bar** — current `MenuBar` panel uses `ImGui::BeginMainMenuBar()`. Prototype uses an ImGui window with `MenuBar` flag inside the custom title bar. Need to migrate `MenuBar`'s contents (Save / New Project / Open Project / Save Dock) into the title bar's File menu.
7. **Two editor binaries during the transition** — until we delete `Hamster-Wheel/` at close-out, both `Hamster-Wheel.exe` and `Hamster-UIPrototype.exe` build. Could waste build time and confuse `Start-Process` calls. Mitigation: explicitly target `Hamster-UIPrototype` in build commands; only flip to renamed `Hamster-Wheel` at the close-out commit.
8. **Smoke test references** — `test/smoke_test.cpp` calls into Hamster-Core directly, not into the editor; should be unaffected. Verify by running it after the build switch.

## Success criteria

1. **Build succeeds**: `cmake --build build --target Hamster-UIPrototype` produces an exe with 0 errors (pre-existing strcpy warnings okay).
2. **Smoke test passes**: `ctest --test-dir build -R SmokeTest` passes.
3. **Editor launches and shows a scene**: hardcoded default project loads; entities visible in the viewport via the FBO blit.
4. **Visual style matches the prototype**: dark palette, rounded panels with gaps, custom title bar with logo + menus + window controls, custom panel headers with "..." button, accent-underlined active tab in bottom panel.
5. **Per-panel functional verification**:
   - PropertyEditor: select an entity in Hierarchy → its Transform/Sprite/Rigidbody/Animation/Scripts show; editing fields updates the entity in the viewport.
   - Hierarchy: shows entity names; selection highlights; search filters; "Add Entity" creates a new entity in the scene; double-click renames; right-click deletes.
   - LevelEditor: zoom slider works; axis gizmo pans the camera; entities can be selected by clicking; gizmo arrows drag the entity; 8 corner/edge grabbers resize the entity; right-click context menu offers New Entity / Delete / Rename / Duplicate; play/pause/stop run the simulation; FPS counter visible during play.
   - AssetBrowser: shows textures and scripts as cards; import asset adds a new texture; new script creates and opens a default `.py`; right-click renames/deletes.
   - AnimationPanel: timeline shows keyframes; new/save/load animations work; dragging keyframe diamonds adjusts timing; play preview shows the animation.
   - Console: scene client logger output appears as scripts run.
6. **MenuBar (File)**: Save / New Project / Open Project all function as today.
7. **ColliderEditor**: opens from PropertyEditor's "Edit Collider" button, drag handles work.
8. **No regression in Python scripting**: a script that uses `apply_force`, `set_velocity`, `key_pressed`, `transform`, `animate` works identically.

## Test extensions required

- **No new automated tests**. The smoke test (`test/smoke_test.cpp`) exercises the C++/Python boundary headlessly and does not touch the editor UI; it stays unchanged and must still pass after the rewrite.
- **Visual verification is mandatory**: each success criterion above is checked by running the editor and exercising the feature.

Why no extension: this feature is pure additive UI/layout — no new engine code paths, no new data formats, no new APIs. Existing engine functionality is consumed differently but not changed. The smoke test already covers the engine code paths; nothing the editor rewrite touches changes their behaviour.

---

## Decisions during implementation

<!-- Append-only log -->

### 2026-05-15 — Phase B uses standard decorated window
The prototype's custom borderless title bar (with drag/maximise/close + integrated File menu) is deferred to Phase E. For Phase B we use the engine's existing decorated GLFW window unchanged — this avoids modifying `Hamster::Window` mid-port and keeps the FBO viewport work isolated. The cost is that Phase B shows a normal OS title bar above the panel layout; this gets replaced once Phase E adds a borderless flag to `WindowProps` and ports the title-bar drawing into `EditorLayer::OnImGuiUpdate`.

### 2026-05-15 — Logo texture loading deferred to Phase E
The prototype's `main.cpp` loaded `hamster-logo.png` via `stb_image` for use in the custom title bar. With the title bar deferred to Phase E, the logo load goes with it. Removed prototype's `stb_image_impl.cpp` from the build (Hamster-Core already provides stb symbols, so linking both produced duplicate-symbol errors).

### 2026-05-15 — Default project lookup via ProjectRegistry
`main.cpp` now reads `%APPDATA%/Hamster/projects.json` via `ProjectRegistry` and opens the first non-missing entry. If no project is found, it logs and continues with empty engine state (no active scene); panels handle null-scene gracefully. `ProjectRegistry.cpp` is compiled into the prototype directly (file lives in `Hamster-Wheel/src/` until Phase F migration).

### 2026-05-15 — LevelEditor visual chrome kept, function deferred
The prototype's play/stop overlay, zoom slider, and axis gizmo are kept as draw-only visuals in Phase B (no click handling). They get wired to engine state in Phase D. The white placeholder entity square and selection handles are deleted — they conflict with real entities visible through the FBO blit.

## Spec amendments

<!-- Append-only log -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **ProjectHubLayer port** — the project picker before the editor opens. Skipped for v1.
- **Functional "..." ellipsis menus** — currently placeholder buttons; would expose per-panel options (collapse, reset, hide).
- **User-resizable panel borders** — drag between panels to adjust proportions.
- **Linux support for the custom title bar** — currently Win32-specific via `GLFW_EXPOSE_NATIVE_WIN32`.
- **Save/restore custom panel layout preferences** — once user-resizable panels exist.
- **Remove the standalone prototype build** — at close-out we rename it to `Hamster-Wheel`, but it still builds as a single editor target.
