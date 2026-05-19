# Feature spec: Scene viewer improvements

> Tier: **Sketch**
> Status: **approved**
> Started: 2026-05-10
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated
- **Tier rationale (1 sentence)**: All changes are contained within EditorLayer's viewport rendering and input handling — no API, file format, or cross-cutting changes.

---

## Problem

The scene viewport lacks basic editing affordances. There is no visual reference for spatial orientation when panning (easy to get lost), no way to create entities at a specific world position without manually setting Transform values, and no way to delete or manage entities directly from the viewport. These are standard 2D editor interactions that the viewport is missing.

## In scope

- **Background dot grid**: subtle fixed-spacing dot grid rendered behind all entities, providing spatial reference while panning/zooming
- **Right-click context menu on empty space**: opens a popup with "New Entity" that creates an entity with its Transform position set to the world-space coordinate under the cursor
- **Right-click context menu on entity**: opens a popup with "Delete", "Rename", and "Duplicate" options
  - Delete: removes the entity from the scene
  - Rename: opens the existing RenameModal for the entity
  - Duplicate: creates a copy of the entity (same components/values, new UUID)
- **Entity detection**: reuses the existing flat-color FBO + `glReadPixels` picking mechanism to determine whether right-click landed on an entity or empty space
- **Zoom slider**: overlaid in the top-right corner of the viewport, displays current zoom level and allows adjusting it via slider. Range: 0.1x–5.0x (10%–500%)
- **Axis gizmo**: small widget in the top-right corner of the viewport, above the zoom slider. Shows X and Y arrows; click-and-drag on an arrow constrains panning to that axis only

## Out of scope

- Grid scaling with zoom level (grid stays fixed regardless of zoom)
- Copy/paste or other scene-level context menu actions
- Multi-select or bulk operations
- Undo/redo for delete/duplicate
- Keyboard shortcuts for delete/duplicate
- Zoom via scroll wheel (separate from the slider)
- Axis gizmo snapping or numeric input

## API sketch

```cpp
// Grid rendering — in EditorLayer::OnUpdate or OnImGuiUpdate:
// Draw dots at fixed world-space intervals across the visible viewport area
// Dots rendered via Renderer or ImGui drawlist, behind entities

// Context menu — in EditorLayer::OnImGuiUpdate:
// Right-click in viewport → check picked entity via FBO
// If entity: show entity context menu (Delete, Rename, Duplicate)
// If empty: show scene context menu (New Entity at world pos)

// World-space coordinate from screen pos:
// screen_pos → viewport-relative pos → unproject using camera offset and zoom

// Zoom slider — in EditorLayer::OnImGuiUpdate:
// ImGui overlay in top-right of viewport, ImGui::SliderFloat for zoom (0.1–5.0)
// Reads/writes Renderer's zoom value directly

// Axis gizmo — in EditorLayer::OnImGuiUpdate:
// Small X/Y arrow widget above zoom slider
// Drag on X arrow → pan constrained to horizontal axis
// Drag on Y arrow → pan constrained to vertical axis
```

---

## Why this approach (AUTHOR WRITES THIS — Claude must not draft)

<!-- AUTHOR: write your rationale here -->
UX improvements

## Risks / what could go wrong

- **Grid performance**: drawing thousands of dots per frame could be expensive if done naively via individual draw calls. Need to batch — either via a single `ImGui::GetBackgroundDrawList` pass or a dedicated grid shader/VBO.
- **World-space coordinate accuracy**: converting screen-space mouse position to world-space depends on correctly accounting for viewport offset, camera offset, and zoom. If any of these are wrong, new entities will spawn at the wrong location.
- **Right-click conflicting with existing input**: EditorLayer already handles left-click for entity selection via the FBO pick. Right-click must not interfere with selection state or trigger a pick-and-select side effect.
- **Duplicate with Python scripts**: duplicating an entity that has a Behaviour component with attached scripts needs to correctly deep-copy or re-reference the scripts. If scripts hold per-instance state from a previous simulation run, the duplicate could inherit stale Python objects.
- **Context menu z-order**: ImGui popups in a dockspace viewport need to render above the scene image. If the popup opens behind the viewport image or gets clipped, it won't be usable.
- **Zoom slider vs existing zoom**: if zoom is already controlled elsewhere (scroll wheel, Renderer API), the slider must stay in sync. A desync means the slider shows one value while the viewport renders at another.
- **Axis-constrained pan input conflicts**: the gizmo arrow drag must not be consumed by the viewport's normal pan handler. Need to distinguish "dragging the gizmo arrow" from "dragging the viewport background."

## Success criteria

- Pan around the scene → dot grid is visible in the background, provides spatial reference, does not move with entities
- Right-click on empty space → context menu appears with "New Entity"; clicking it creates an entity whose Transform position matches the clicked world coordinate (within ~1 unit tolerance)
- Right-click on an entity → context menu appears with Delete, Rename, Duplicate
  - Delete removes the entity from the hierarchy and viewport
  - Rename opens the RenameModal
  - Duplicate creates a new entity with the same components visible in the hierarchy
- Zoom slider in top-right of viewport displays current zoom; dragging the slider changes the viewport zoom level in real time
- Axis gizmo above the zoom slider shows X and Y arrows; dragging an arrow pans the viewport strictly along that axis
- Build succeeds, smoke test passes (no regression)

## Test extensions required

None — pure additive UI/interaction in the editor viewport. The smoke test doesn't exercise ImGui rendering or mouse input. Manual visual verification is the appropriate test.

---

## Decisions during implementation

<!-- Append-only log. -->

## Spec amendments

### 2026-05-10 — Right-click panning coexistence

Right-click is already used for viewport panning (drag). Context menus also need right-click. Resolution: distinguish click from drag using a 5px movement threshold. Right-click release without significant movement opens context menu; right-click + drag pans as before. Author approved in plan review.

---

## Future work (out-of-scope ideas surfaced during this feature)

- Grid density scaling with zoom level
- Copy/paste entities
- Keyboard shortcuts (Del for delete, Ctrl+D for duplicate)
- Undo/redo system
- Multi-select context menu operations
