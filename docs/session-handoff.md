# Session handoff

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
