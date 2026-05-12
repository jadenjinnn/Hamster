# Feature spec: Collider Editor

> Tier: **Design**
> Status: **approved**
> Started: 2026-05-11
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated
- **Tier rationale (1 sentence)**: Additive fields on an existing component plus a new self-contained editor window; no Python API changes and serialization is backwards-compatible via defaults.

---

## Problem

Colliders currently match the entity's full `Transform::size` with no offset, so there's no way to fit a hitbox to a character's actual shape (e.g. bottom half only, or a slightly smaller box to avoid catching edges). Every 2D engine needs per-entity collider tuning; without it, physics interactions feel wrong and authors can't build polished gameplay.

## In scope

- Two new fields on `Rigidbody`: `colliderOffset` (vec2, default 0,0) and `colliderSize` (vec2, default matches transform size)
- An "Edit Collider" button in PropertyEditor (visible when entity has a Rigidbody)
- A modal/window that opens showing the entity's sprite (or transform bounds as a grey rect if no sprite) with a green outline representing the collider
- Drag handles on the green outline to resize the collider
- Drag the collider body to reposition it (sets offset relative to entity center)
- Works for both Box and Circle collider shapes (shape chosen in PropertyEditor before opening editor)
- Collider shape (box vs circle) is NOT changeable while the editor is open
- Serialization of colliderOffset and colliderSize in SceneSerialiser
- `InitPhysicsWorld` uses colliderOffset/colliderSize instead of deriving from Transform::size

## Out of scope

- Polygon/vertex-level collider editing
- Auto-fit collider to sprite alpha (trace outline)
- Multiple colliders per entity
- Exposing colliderOffset/colliderSize to Python scripting API
- Visualizing colliders in the main scene viewport (only in the editor window)

## API sketch

No new Python API. C++ changes only:

```cpp
// Components.h — Rigidbody additions
struct Rigidbody {
  // ... existing fields ...
  glm::vec2 colliderOffset = glm::vec2(0.0f);
  glm::vec2 colliderSize = glm::vec2(0.0f); // 0,0 means "use transform size"
};
```

```cpp
// InitPhysicsWorld — use custom collider bounds
glm::vec2 size = (rb.colliderSize.x > 0.0f && rb.colliderSize.y > 0.0f)
    ? rb.colliderSize : transform.size;
glm::vec2 offset = rb.colliderOffset;

float halfW = std::max(std::abs(size.x) / PIXELS_PER_METER * 0.5f, 0.05f);
float halfH = std::max(std::abs(size.y) / PIXELS_PER_METER * 0.5f, 0.05f);

// Body position includes offset
bodyDef.position = {
    (transform.position.x + transform.size.x * 0.5f + offset.x) / PIXELS_PER_METER,
    (transform.position.y + transform.size.y * 0.5f + offset.y) / PIXELS_PER_METER
};
```

---

## Design (Design tier)

### Data structures

Two new fields on `Rigidbody` in `Components.h`:
- `glm::vec2 colliderOffset = glm::vec2(0.0f)` — offset from entity center in pixels
- `glm::vec2 colliderSize = glm::vec2(0.0f)` — custom collider dimensions in pixels; `(0,0)` sentinel means "use transform size" for backwards compatibility

### Module touchpoints

- `Hamster-Core/src/Core/Components.h` — add colliderOffset, colliderSize to Rigidbody
- `Hamster-Core/src/Core/Scene.cpp` — `InitPhysicsWorld` and `SyncPhysicsToTransforms` use custom collider bounds/offset
- `Hamster-Core/src/Core/SceneSerialiser.cpp` — serialize/deserialize the two new fields
- `Hamster-Wheel/src/Panels/PropertyEditor.cpp` — add "Edit Collider" button
- `Hamster-Wheel/src/Panels/ColliderEditor.cpp` (new) — the editor window/modal
- `Hamster-Wheel/src/Panels/ColliderEditor.h` (new) — header
- `Hamster-Wheel/src/EditorLayer.cpp` — own and render the ColliderEditor panel

### Lifecycle / control flow

1. User selects an entity with a Rigidbody in the scene
2. PropertyEditor shows "Edit Collider" button
3. User clicks button → ColliderEditor window opens
4. ColliderEditor reads the entity's Sprite texture (or Transform size if no sprite) and Rigidbody collider fields
5. Renders the sprite/rect centered in the window with a green outline at colliderOffset/colliderSize
6. User drags handles to resize → updates `colliderSize` live on the component
7. User drags the outline body → updates `colliderOffset` live on the component
8. User closes the window → values are already on the component, nothing to "apply"
9. On scene save, SceneSerialiser writes the new fields
10. On play, `InitPhysicsWorld` reads colliderOffset/colliderSize to create the Box2D shape

### Edge cases

- Entity has no Sprite: editor shows a grey rectangle at transform size as reference, collider outline overlaid
- colliderSize is (0,0) (default / never edited): `InitPhysicsWorld` falls back to transform size — no behavior change for existing scenes
- Circle collider: resize handles control the radius (max of width/height); circle is always uniform
- Very small collider: clamp to min 0.05m in Box2D (same as current clamp)
- Entity transform changes while editor is open: editor should re-read transform each frame so the reference rect stays accurate
- Old scene files without colliderOffset/colliderSize: deserializer uses defaults (0,0) — backwards compatible

---

## Why this approach

A dedicated editor window (rather than inline PropertyEditor sliders) was chosen because visual collider editing requires seeing the sprite and the collider shape together — sliders for offset/size would be trial-and-error. The (0,0) sentinel for colliderSize preserves backwards compatibility: existing scenes serialize without these fields and default to transform-size behavior on load. Storing the data on Rigidbody (rather than a separate component) keeps the data co-located with the physics config it affects, avoiding component coupling issues.

## Risks / what could go wrong

1. **SyncPhysicsToTransforms offset math**: when syncing Box2D positions back to Transform, the collider offset must be subtracted. Getting this wrong means entities visually drift from their physics bodies. The offset is in entity-local pixel space, so rotation complicates it — need to rotate the offset by the entity's rotation when converting to/from Box2D.
2. **Circle resize UX**: circles have one radius, not width/height. If the editor shows 4 drag handles but the shape is a circle, dragging a corner handle needs to uniformly scale the radius. This could feel unintuitive if not clearly communicated.
3. **Serialization versioning**: old scenes won't have colliderOffset/colliderSize. The deserializer reads fields in order — if the end-of-entity marker (-1) comes before these fields, it just won't read them. But the new serializer writes them, so re-saving an old scene adds the fields. Need to verify the sentinel-based format handles this gracefully.
4. **Editor window texture rendering**: rendering a Sprite's texture into an ImGui window requires the texture's OpenGL handle. This is already done for the sprite preview in PropertyEditor, so the pattern exists, but the collider editor needs it at a larger scale with pan/zoom eventually.

## Success criteria

1. An entity with a Rigidbody shows an "Edit Collider" button in PropertyEditor
2. Clicking it opens a window showing the sprite (or grey rect) with a green collider outline
3. Dragging handles resizes the collider; dragging the body repositions it
4. The customized collider persists after save/load (close editor, save scene, reopen project, verify values)
5. During simulation, physics uses the custom collider bounds — a dynamic entity with a collider half the sprite size should fall through a gap that the full sprite wouldn't fit through
6. Existing scenes without custom collider data load and behave identically to before (backwards compatible)
7. Both box and circle shapes work in the editor

## Test extensions required

None — this is a pure additive editor UI feature. The underlying physics (collider size used in `InitPhysicsWorld`) is already tested by the existing smoke test. The new fields default to (0,0) which preserves existing behavior. Manual verification is appropriate for the visual editor interaction.

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building. -->

## Spec amendments

- **Serialization: Collider_ID instead of appending to Rigidbody block.** The spec assumed colliderOffset/colliderSize could be appended to the Rigidbody binary data and old files would skip them via the -1 end marker. Wrong — the -1 marker is between components, not within a component's data. Old files have exactly 6 fields after Rigidbody_ID; reading 10 would consume the next component ID as float data. Fix: new `Collider_ID = 7` in ComponentID enum, written as its own block after Rigidbody. Old files without it use defaults (0,0).

---

## Future work (out-of-scope ideas surfaced during this feature)

- Visualize colliders in the main scene viewport (toggle overlay)
- Expose colliderOffset/colliderSize to Python scripting API
- Support multiple colliders per entity
- Auto-fit collider to sprite alpha outline
