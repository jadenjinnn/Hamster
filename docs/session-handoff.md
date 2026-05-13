# Session handoff

## 2026-05-13 — Animation system core implemented, panel not yet built

- Spec at `docs/features/active/animation-system.md` (approved, full spec tier)
- All core systems working: `AnimationData`/`AnimationKeyframe` structs, `Animation` component (`Animation_ID = 8`), `.hanim` binary file format, `AssetManager` animation storage + serialization
- Runtime: animation advance in `Scene::OnUpdate` (between physics and scripts), sprite texture swap, original texture snapshot/restore on sim start/stop
- Events: `AnimationCompletedEvent` posted when non-looping animation finishes, dispatched to Python `on_animation_complete(animation_name)` callback via `completedAnimations` queue in `OnScriptUpdate`
- Python API: `self.animate("name")` (uses component loop default), `self.animate("name", loop=False)`, `self.stop_animation()`, `self.is_animating`
- Editor: PropertyEditor has Animation component section (loop toggle, default animation combo, animation list with add/remove from AssetManager)
- Smoke test extended and passing
- **Not done yet**: Animation panel (timeline editor for creating/editing `.hanim` files visually), AssetBrowser `.hanim` display
- Next step: build the Animation panel — timeline with draggable keyframes, sprite drag-in, preview playback, save/load

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
