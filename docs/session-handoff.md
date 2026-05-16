# Session handoff

## 2026-05-15 (session 4) — Bug 0002 investigated, fix approved, implementation deferred

- **Phase 1A measurement results** (done):
  - Idle is clean — CPU 5–6% on a 16-core box ≈ 1 core, FPS locked at 60 matching refresh. **Vsync is somehow already working** despite no `glfwSwapInterval(1)` in our source. Original plan's #1 fix is now de-prioritized.
  - No idle memory / handle / GDI / user-object growth over several minutes.
- **Project-switch leak confirmed** — Task Manager Handles 588 → ~1500 over 8 open/close cycles via `File → Open Project` (≈ +114 handles/cycle). UI duplication symptom (Add Component dropdown listing every option × N, Property Editor "select an entity first" × N) pinned the root cause to layer-stack accumulation.
- **Bug 0002** (`docs/bugs/active/0002-project-switch-handle-leak.md`) — Tier 2, severity High, status `investigating`. Investigation, root cause, fix plan, and author rubber-stamp are all in the bug file. Three compounding root-cause threads:
  1. `ProjectHubLayer::OnAttach` subscribes to `ProjectOpened`, discards the `SubscriptionHandle`, never unsubscribes. Lambda outlives the hub's tenure in the layer stack.
  2. `LayerStack::PopLayer` removes from the vector + calls `OnDetach` but never `delete`s the layer. Hub heap allocation + its captured lambda persist forever.
  3. No swap-main-layer API; the hub's orphaned lambda is the only `ProjectOpened` subscriber and naively `PushLayer`s a new `EditorLayer` on every fire without popping the previous one. Each accumulated `EditorLayer` keeps its FBO + logo `Texture` + panel objects + panel-level dispatcher subscriptions alive.
- **Fix plan (approved, not yet implemented)** — 4 files:
  1. `Hamster-Core/src/Core/LayerStack.cpp` — `PopLayer` should `delete layer` after `OnDetach` (LayerStack owns layers transferred at push).
  2. `Hamster-Core/src/Core/Application.cpp` — `~Application` should pop+delete remaining layers before the existing scene-save loop.
  3. `Hamster-Wheel/src/ProjectHubLayer.cpp` + `.h` — remove the `ProjectOpened` subscription entirely; remove `OnAttach`; remove `m_EditorLayer` member.
  4. `Hamster-Wheel/src/main.cpp` — install one persistent `ProjectOpened` handler that tracks the current main layer via a captured local, pops + deletes the previous one, pushes a new `EditorLayer`.
- **Scope note**: this fix does **not** address missing GL destructors on `Texture` / `Shader` / `FramebufferTexture` — those still leak driver-side handles per construction, but after this fix each construction only happens once per actual switch instead of stacking. Re-measure handle growth after the fix; if non-zero, follow up with destructor work as a separate bug.
- **Smoke test impact**: none expected — no Python/simulation changes.
- **Verification after fix**: re-run the bug's reproduction (10 project switches via `File → Open Project`); Handles should stay flat.
- **Next session step**: implement the 4-file fix, build, run smoke test, then re-run the handle-count reproduction to verify it's flat. After commit, decide whether residual handle growth warrants logging a follow-up bug for the GL destructors.

## 2026-05-15 (session 3) — Performance/leak investigation queued for measurement

- Plan at `C:\Users\Jaden\.claude\plans\velvety-crafting-sketch.md` (saved as `velvety-crafting-sketch.md`, approved). Read-only audit identified four likely causes of system slowdown when running Hamster:
  1. **No `glfwSwapInterval`** anywhere in our source — main loop runs uncapped, pegs a CPU core.
  2. **`Texture`, `Shader`, `FramebufferTexture` have no destructors** — every `glGen*` leaks its GL handle. Renderer's `VBO` is also generated into a local var that goes out of scope, leaking on every Renderer construction.
  3. **`FramebufferTexture::ResizeFrameBuffer` called every frame unconditionally** at `EditorLayer.cpp:246` — two GPU allocs/frame.
  4. **Hover-pick re-renders the whole scene** to the FBO + `glReadPixels` stall every hovered frame at `EditorLayer.cpp:74–105`. Combined with the main display render, that's 2–3 full scene draws/frame while hovering.
- Lower-severity findings (Scene without destructor, per-frame Python `attr` lookup, per-frame EnTT group sort, unbounded main-thread queue) listed in the plan.
- **ASan was tried first, did not work**:
  - Windows ASan does not support leak detection (`detect_leaks=1` prints "not supported on this platform"). Pre-build attempt confirmed.
  - Smoke test crashes inside ASan's instruction interceptor: `interception_win: unhandled instruction` (known Win11 issue, likely Python interop).
  - `build-asan/` directory left intact for possible future use-after-free / heap-overflow checking — feel free to delete it if reclaiming disk space.
- **Plan pivoted to Visual Studio 2022 Diagnostic Tools (Memory Usage)** for heap leak detection. VS 2022 Enterprise already installed. No rebuild needed — runs on the existing `build/Hamster-Wheel/Hamster-Wheel.exe`.
- **Queued user-driven measurements** (not yet run):
  1. Task Manager symptom check — CPU%, Memory, Handles, GDI, GPU%, FPS at idle + while hovering viewport for 30s + after 5min idle.
  2. VS 2022 Performance Profiler → Memory Usage → attach to Hamster-Wheel.exe → snapshot → open/close 5 projects → snapshot → diff the heap.
  3. (Optional) RenderDoc Resource Inspector for GL handle counts across project switches.
- **Next session step**: run the queued measurements above and paste the readings / heap-diff screenshot back so we can confirm which findings are real and prioritize fixes (vsync is almost certainly fix #1 regardless).

## 2026-05-15 (session 2) — UI polish + project hub port + windows snap

- Committed `302ce5f9` and pushed (129 files, +44860/-2376). This bundle also captured the prior editor-rewrite work that had never been committed.
- **Resolved from previous handoff**:
  - ProjectHubLayer ported from Hamster-Wheel-old; `main.cpp` pushes it instead of EditorLayer directly. `ProjectOpened` event triggers the layer swap.
  - Hardcoded debug project path removed from `main.cpp`.
- **Still deferred**:
  - `RenameModal` wiring — only the AssetBrowser texture rename path uses it. Hierarchy entity rename + PropertyEditor name-field rename modal not wired.
  - Title bar drag + Aero Snap are Win32-only (`#ifdef _WIN32`). Linux port deferred.
  - Portable serialization still open (architecture.md open question).
- **Architectural notes worth remembering**:
  - Borderless-with-snap pattern lives in `Window.cpp` via `SetWindowSubclass` + WM_NCCALCSIZE clamping to `mi.rcWork` when `SW_SHOWMAXIMIZED`. WS_THICKFRAME is required for Aero Snap; GLFW_DECORATED=FALSE strips it, so the subclass re-adds it.
  - Initial viewport size fix: Application ctor posts a synthesized `FramebufferResizeEvent` after subsystems are wired, so the renderer picks up the real maximized size instead of the hardcoded 1920×1080.
  - Panel focus state is read from `ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)` in `Panel::DrawHeader`/`DrawTabbedHeader`. No global focus tracking — mutual exclusion is free.
  - First-render auto-focus is avoided via `ImGuiWindowFlags_NoFocusOnAppearing` on panel windows; LevelEditor uses `SetNextWindowFocus` once on first frame so it's the default-selected panel.
- **Next session step**: nothing specific. Options — wire RenameModal into Hierarchy/PropertyEditor, work an active spec, or pick from architecture.md open questions (portable serialization).

## 2026-05-15 — Editor rewrite from prototype shipped

- **`Hamster-UIPrototype/` → `Hamster-Wheel/`** rename complete. The new editor is the prototype with engine deps and per-panel data wiring added. Old editor preserved as `Hamster-Wheel-old/`, not built (`add_subdirectory` commented in root `CMakeLists.txt`).
- All 6 phases complete: build wiring, engine entry + FBO blit, per-panel data wiring, LevelEditor interactivity, MenuBar + modals, disable docking + rename. Smoke test passes.
- Spec moved to `docs/features/shipped/2026-05-15-editor-rewrite-from-prototype.md`. Old `panel-layout` spec parked at `docs/features/parked/panel-layout.md` (superseded).
- **Hamster-Core change**: `Application::Application(const WindowProps &)` added; `WindowProps.borderless` field added. Default constructor delegates to the new one with default props. Docking flag removed from `ImGuiLayer.cpp`.
- **Deferred / not yet wired (no spec, follow-ups when relevant)**:
  - `RenameModal` is compiled and instantiable but not yet wired into rename context menus (Hierarchy entity rename, AssetBrowser asset rename). PropertyEditor still has the rename modal field commented out in its sections.
  - ProjectHub for v1 was skipped — new editor opens the most-recently-opened project directly from `ProjectRegistry`. v2 should port `ProjectHubLayer` and push it before `EditorLayer` (or via `ProjectOpened` event swap).
  - Hardcoded default-project debug path in `main.cpp` (`C:\Users\Jaden\Downloads\Untitled12\Untitled12.hamproj`) — remove once a proper "no projects" fallback / ProjectHub path is in.
  - Title bar drag is Win32-specific via `GLFW_EXPOSE_NATIVE_WIN32`. Linux support deferred.
- **Other in-flight specs**: `docs/features/active/scene-viewer-improvements.md` is still in active. Unrelated to this work.
- **Next session step**: nothing specific. Pick from open questions in `architecture.md` (portable serialization format), or work the active scene-viewer-improvements spec.

## 2026-05-14 (session 2) — Panel layout prototype finalized, ready for porting

- **Hamster-UIPrototype** standalone target is now fully polished and split into clean architecture:
  - `src/Theme.h/.cpp` — color palette (14 colors), font globals, ApplyTheme(), LoadFonts()
  - `src/Panel.h/.cpp` — Panel struct: DrawHeader(), DrawTabbedHeader(), BeginContent()/EndContent() with scroll support
  - `src/Components/Components.h/.cpp` — reusable UI helpers: SectionHeader, HButton, HToolbarButton, HCombo, HDragFloat, HCheckbox, AxisDotInput, SectionSeparator
  - `src/Panels/` — PropertyEditor, LevelEditor, Hierarchy, AnimationPanel, BottomPanel (tabbed Asset Browser + Animation)
  - `src/main.cpp` — entry point, custom borderless title bar with drag/maximize, layout math
- Logo added: `Resources/Logo/hamster-logo.png` (128x128, white-on-transparent, converted from `Untitled.svg`), rendered in title bar via DrawList::AddImage
- Key layout constants: 8px gaps, 16px side padding, 8px header-to-content gap, 36px header height, 32px title bar
- Panel proportions: left 17.5%, right 14.5%, bottom 28% of content area
- **Next step**: port prototype code back into EditorLayer.cpp and Hamster-Wheel panels — the prototype files map 1:1 to real editor panels
- Spec at `docs/features/active/panel-layout.md` still active

## 2026-05-14 (session 1) — Panel layout: standalone UI prototype approach

- Previous incremental approach (editing EditorLayer directly) was scrapped — all Hamster-Wheel changes reverted via `git restore`
- New approach: **Hamster-UIPrototype** standalone target builds against imgui+glfw+glad only, no engine deps
- Prototype closely matches the Stitch reference design after many iterations
- See session 2 for final state

## 2026-05-13 — Animation system fully shipped

- Feature closed out: spec moved to `docs/features/shipped/2026-05-13-animation-system.md`, architecture.md updated, README updated
- Committed as 370efe28, pushed to origin/master
- No pending work from this feature

## 2026-05-12 — Rounded panels attempted and scrapped

- Tried 3 approaches to get rounded docked panels: style-only overlay, draw-list rect inside each panel, and patching ImGui's dock node rendering (`imgui.cpp` lines 6744, 16408, 17887)
- Root cause: ImGui docking forces `WindowRounding = 0` on viewport-owning host windows, and the dock node rendering uses `host_window->WindowRounding` (always 0) instead of `style.WindowRounding`
- Even after patching to use `style.WindowRounding`, the panels still appeared un-rounded — likely because the host window's own background fills edge-to-edge and covers the rounded corners before they're visible
- All changes fully reverted — no residual modifications in any file
- Conclusion: achieving the reference image's card look may require a more invasive ImGui fork (custom dock node layout with per-node margins) or abandoning ImGui's built-in docking for a manual panel layout

## 2026-05-11 — Collider editor spec approved

- Collider editor feature spec approved at `docs/features/active/collider-editor.md`
- Adds `colliderOffset` and `colliderSize` fields to Rigidbody, visual editor window with drag handles
- Next step: run `/implement collider-editor` to begin coding
- All box2d bugfixes and Python QoL changes from earlier this session are committed and pushed

## 2026-05-11 — UI polish pass shipped

- Bold font variants loaded (Inter-Bold 16px + 18px) for typographic hierarchy
- Property editor: bold section headers and field labels, increased spacing between component sections
- Hierarchy: flat entity list (no root node), search bar with filter, full-width selection highlight
- Asset browser + file browser: grid card layout with icons, centered labels, faint borders, ellipsis truncation
- Zoom slider: moved to bottom-right, magnifying glass icon, pill-shaped track, percentage label
- Axis gizmo: repositioned to bottom-right above zoom slider
- Feature workflow updated: Claude now drafts "Why this approach" section (author reviews during approval)

## 2026-05-11 — Scene viewer improvements shipped

- Scene viewer feature complete: dot grid, right-click context menus, zoom slider, axis gizmo, 8-handle selection box
- FBO pick bug from 2026-05-10 resolved: grabber hit areas were too small (16px centered on corners), not a render-order issue. Fix: 24px invisible pick zones, larger than the 8px visual squares
- Selection box reworked: 8 white squares with blue outlines (4 corners + 4 edge midpoints), blue entity outline, replaces old 4-corner circle design
- FlatShader: `circleMode` replaced with `borderMode`/`borderWidthX`/`borderWidthY` for outline rendering
- Edge grabbers constrain resize to one axis (top/bottom = height, left/right = width)

## 2026-05-09 (session 3)

- DI refactor complete (7 commits): Scene, Project, Panel, ImGuiLayer, Scripting, AssetManager, EditorLayer, ProjectHubLayer all receive dependencies via constructors
- 0 singleton calls remain in Hamster-Core; 2 remain in Hamster-Wheel (editor-level, appropriate)
- HAMSTER_LOG macro removed; replaced with direct m_ClientLogger->Log() calls
- Remaining Phase 5 work: AssetManager still all-static (lifecycle/ownership), Renderer static state
- Serialization portability still deferred until before Phase 6
- Smoke test guardrail: `ctest --test-dir build -R SmokeTest`

## 2026-05-09 (session 2)

- Phases 2-4 complete, all committed and pushed
- Phase 5 starts with dependency injection refactor:
  - Pass EventDispatcher, etc. through constructors instead of reaching through Application singleton
  - Start with Scene — most painful coupling point
  - User understands the "ownership vs access" distinction and is on board
- Other Phase 5 architectural work: AssetManager/Renderer static state, HAMSTER_LOG macro decoupling
- Serialization portability deferred until before Phase 6
- Smoke test guardrail: `ctest --test-dir build -R SmokeTest`
