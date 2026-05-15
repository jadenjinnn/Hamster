# Session handoff

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
