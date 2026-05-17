# Feature spec: spatial-index

> Tier: **Design**
> Status: **approved**
> Started: 2026-05-17
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Cross-cutting (new `SpatialIndex` module + Scene rebuild lifecycle + Renderer culling consumer + EditorLayer picking consumer)
- **Tier rationale (1 sentence)**: New foundation type with two consumers across three subsystems; the data-structure choice + rebuild lifecycle + viewport semantics all have concrete design decisions worth writing down ahead of `/implement`.

---

## Problem

Both the render path and the editor's entity-picking path iterate every entity in the scene every frame:

- `Scene::OnRender` walks the full `m_RenderGroup` and submits every sprite, even off-screen ones. With v1 sprite batching this becomes one draw call regardless, but the CPU still computes 6 transformed vertices per sprite and uploads them.
- `EditorLayer::OnUpdate` runs a separate full-scene FBO render every frame whenever the mouse hovers the viewport, in order to pick entities via colour readback. With 5000 sprites and hover-pick active the editor visibly lags — even though the headline batching number (1 draw call) is already great.
- A quadtree spatial index over the scene's AABBs solves both at once: cull the render submission to viewport-intersecting entities, replace the pick FBO pass with a point query against the index. One foundation type, two consumers, two distinct resume bullets.

## In scope

- New `SpatialIndex` class — quadtree storing `(UUID, AABB)` per entity. Rebuilt every frame from current Transform components. Bucket cap and depth cap configurable; sensible defaults.
- Tight AABB computation for rotated sprites (axis-aligned bbox of the four rotated corners).
- Viewport-rect query: returns the subset of UUIDs whose AABB intersects a given rect. `Scene::OnRender` uses this to skip submitting off-screen sprites.
- Point query: returns the topmost entity (highest `position.z`) whose AABB contains the cursor world point. `EditorLayer::OnUpdate` uses this to replace its FBO-based entity pick.
- Hybrid picking: the existing FBO pass stays *only* for transform grabbers (8 quads — trivial cost). Entity picking goes through the spatial index.
- Smoke-test extensions covering both queries directly (point query, rect query) against a known scene.

## Out of scope

- Collision broad-phase (Box2D already has its own internal broad-phase; not touching).
- Incremental updates (insert/remove/move) to the tree. v1 rebuilds every frame; future work if profiling shows rebuild is dominant.
- Threaded queries.
- 3D / depth-stencil-anything — engine is 2D.
- Replacing the EnTT registry. The index is an *index over* the registry, not a replacement.
- Pixel-precise alpha-aware picking. AABB precision matches Hamster's all-rectangular sprite content.
- Persistent scene-wide partitioning data (no serialization — the index is purely transient per frame).

## API sketch

C++ (Hamster-Core):

```cpp
// New: Hamster-Core/src/Utils/SpatialIndex.h
namespace Hamster {
struct AABB {
    glm::vec2 min;
    glm::vec2 max;
    bool ContainsPoint(glm::vec2 p) const;
    bool Intersects(const AABB &other) const;
};

class SpatialIndex {
public:
    SpatialIndex();
    void Rebuild(const std::vector<std::pair<UUID, AABB>> &entities);

    // Topmost entity (sorted by descending z; tie → first inserted) whose
    // AABB contains the point. Returns UUID::Nil() on no hit.
    UUID QueryPoint(glm::vec2 worldPoint,
                    const std::function<float(UUID)> &zOf) const;

    // All UUIDs whose AABB intersects `rect`. Order unspecified.
    std::vector<UUID> QueryRect(const AABB &rect) const;
};
} // namespace Hamster
```

Internal call site (Renderer culling):

```cpp
// In Scene::OnRender(false), before the sprite loop:
auto viewportAABB = renderer->GetViewportWorldAABB();
auto visible = m_SpatialIndex.QueryRect(viewportAABB);
// Iterate `visible` instead of full m_RenderGroup.
```

EditorLayer (picking):

```cpp
// Replace FBO-based entity pick:
glm::vec2 worldPos = renderer->ScreenToWorldPos(mouse);
UUID hit = scene->GetSpatialIndex().QueryPoint(worldPos,
    [scene](UUID u) { return scene->GetEntityComponent<Transform>(u).position.z; });
// Grabber pick still goes through the existing FBO path.
```

No Python-facing API. No file-format change.

---

## Design

### Data structures

```cpp
class SpatialIndex {
    struct Node {
        AABB bounds;
        std::vector<std::pair<UUID, AABB>> entries;  // empty for internal nodes
        std::unique_ptr<Node> children[4];           // null for leaves
        bool IsLeaf() const { return children[0] == nullptr; }
    };
    std::unique_ptr<Node> m_Root;
    // Tunables — start conservative, profile later.
    static constexpr int kBucketCap = 8;
    static constexpr int kMaxDepth  = 8;   // 4^8 = 65k leaves max
};
```

`AABB` and the spatial-index types live in `Hamster-Core/src/Utils/SpatialIndex.{h,cpp}`. The index is held as a member of `Scene`: `SpatialIndex m_SpatialIndex;` — exposed via `Scene::GetSpatialIndex() const`.

### Module touchpoints

- `Hamster-Core/src/Utils/SpatialIndex.{h,cpp}` — new files. Node struct, build, point query, rect query.
- `Hamster-Core/src/Core/Scene.{h,cpp}` — add `m_SpatialIndex` member, add private `RebuildSpatialIndex()` helper that walks `m_RenderGroup` (or a broader view), computes tight AABBs for rotated sprites, and calls `m_SpatialIndex.Rebuild(...)`. Call it at the start of `OnRender`. Add public `GetSpatialIndex()` getter.
- `Hamster-Core/src/Renderer/Renderer.{h,cpp}` — add `AABB GetViewportWorldAABB() const` that derives the world-space rect from `m_CameraOffset`, viewport size, and zoom.
- `Hamster-Core/src/Core/Scene.cpp::OnRender(false)` — call `renderer->GetViewportWorldAABB()`, query the index, iterate only visible UUIDs. Renderer pick path (`renderFlat=true`) keeps iterating full `m_RenderGroup` — fewer hits than render, not the bottleneck.
- `Hamster-Wheel/src/EditorLayer.cpp` — entity pick: replace the FBO-color path with `scene->GetSpatialIndex().QueryPoint(world, zLookup)`. Grabber pick: keep FBO (still renders 8 grabber quads only — no full-scene render).

### Lifecycle / control flow

```
Application::Run frame:
  ├─ ExecuteMainThread
  ├─ Scene::ProcessPendingRestore (simulation-snapshot)
  ├─ Layer push/pop
  ├─ Layer::OnUpdate
  │    └─ EditorLayer::OnUpdate
  │         ├─ Pick if mouse moved:
  │         │    ├─ Entity pick → SpatialIndex.QueryPoint   <-- NEW
  │         │    └─ Grabber pick → FBO render (8 quads only, unchanged)
  │         └─ Main FBO render via Scene::OnRender(false)
  │              ├─ RebuildSpatialIndex                     <-- NEW
  │              ├─ Sort m_RenderGroup by z
  │              ├─ Query viewport AABB                     <-- NEW
  │              └─ For each visible UUID: BeginSpriteBatch → SubmitSprite → EndSpriteBatch
  ├─ ImGui begin / panels / end
  └─ Scene::OnUpdate (physics + scripts)
```

`RebuildSpatialIndex` runs once per frame, before sort. Sort happens on the same data either way; the index just narrows what we submit.

### Edge cases

- **Sprite outside the root AABB.** Compute root bounds from min/max over all sprite AABBs during rebuild. Empty scene → root bounds = degenerate (a small rect around origin), index is empty.
- **Sprite that straddles quadrant boundary.** Store in the smallest fully-containing internal node (don't recurse further). Common case — large sprites are correctly handled; query checks them when descending past their level.
- **Many sprites at same position.** Bucket cap of 8 with depth cap of 8 → eventually leaves can exceed cap without splitting. Acceptable; query degrades to linear over the bucket. Document the cap.
- **Rotated sprite AABB.** Compute the four world-space corners using the rotation matrix already in `DrawSprite` / `SubmitSprite`. Tight AABB = min/max over corners. Costs 4 sin/cos per sprite per rebuild — trivial.
- **Z-tie among point-query hits.** Sort candidates by `z` descending; for equal z, take the first encountered in iteration order. Matches current FBO behaviour for ties (whichever was rendered last in EnTT order).
- **Viewport rect partially off-world.** `Intersects` is robust to negative coords; nothing special.
- **No active scene.** Renderer must guard `if (m_ActiveScene)` before querying — same as existing code paths.
- **Picking during simulation.** EditorLayer still allows hovering during play. Spatial index is rebuilt every frame regardless of `m_IsSimulationPaused`, so picking works during play too. (Current FBO path also works during play.)
- **Picking on a frame where the index has been wiped by `ProcessPendingRestore`.** Sequence: stop → restore clears registry → next frame rebuilds index (empty registry → empty index → query returns Nil). Safe.

---

## Why this approach

Alternatives considered:

1. **Uniform spatial hash** — grid of fixed-size cells, sprites bucketed by cell. ~50 LoC simpler than a quadtree. Rejected: a single user-clustered region of 5000 sprites would all fall into one cell, defeating the index. Resume-bullet language is also weaker ("flat grid" vs "recursive spatial subdivision").
2. **Incremental quadtree (no rebuild)** — track transform changes via EnTT observers, move entities through the tree. Defers rebuild cost across frames but adds significant invariants (entity moved → reposition; entity created → insert; entity destroyed → remove). Rejected for v1: rebuild at 5000 sprites is sub-millisecond; profiling would tell us if v2 needs incremental.
3. **Loose quadtree** (Thatcher Ulrich's variant) — handles boundary-straddling entities by loosening node bounds. More complex; marginal win at our scale. Future work.
4. **Replace FBO entirely (no hybrid)** — drop FBO for both entities and grabbers. The grabbers are 8 fixed-position quads; a precise rect test against each is trivial. Tempting but expands the in-scope surface. Hybrid keeps the diff focused on the painful case (entities).

The chosen design (quadtree, rebuild every frame, tight AABBs, hybrid picking) keeps the change contained and produces two cleanly separable bullets:

- "Implemented quadtree spatial index over the ECS with viewport-rect culling; sustains 60 FPS at 10k+ sprites by submitting only on-screen entities."
- "Replaced full-scene FBO render pass for entity picking with O(log N + k) spatial query; editor stays responsive at 10k+ sprites."

## Risks / what could go wrong

1. **Culling false-negative pops.** If `GetViewportWorldAABB` is computed wrong (off-by-zoom, off-by-camera-offset, y-flip), on-screen entities get filtered out and visibly disappear. Mitigation: smoke scenario explicitly asserts that a known-in-rect entity is returned and a known-out-of-rect entity is not. Manual test: pan/zoom around at the spec's success-criteria scale and look for pop-in.
2. **Rotated-AABB miscompute.** Wrong rotation sign or off-by-half-size in the corner derivation → entities pick-test against the wrong rect, picking gets weird at non-zero rotation. Smoke scenario covers a 45°-rotated sprite specifically.
3. **Rebuild cost dominates.** At 50k sprites the rebuild might be a few ms. Budget the rebuild and confirm with the FPS HUD that the new total frame time is still ≤16ms at the success-criteria scale. If not, fall back to incremental updates (logged as future work).
4. **Bucket pathology.** Many sprites at the same position fill a single leaf bucket beyond the cap (depth cap stops splits). Query degrades to linear over that bucket. Acceptable; document.
5. **Z-tie behaviour drift.** If picking semantics change for overlapping same-z entities (e.g. always returns a different one than before), feels broken to the user. Mitigation: order candidates by `z` descending, break ties by EnTT iteration order — matches current FBO behaviour as closely as possible.
6. **EnTT iteration ordering instability.** EnTT may reorder internal storage on add/remove. Picking ties may flicker frame-to-frame. Acceptable visual nit for v1; flag if it becomes annoying.

## Success criteria

Numbers measured on author's machine, with the existing benchmark (`benchmark_batching.py`) bumped to N as listed:

1. **10,000 sprites with hover-pick active sustains ≥60 FPS** (vsync-capped). Today's number is the lag baseline — record both before/after for the resume bullet.
2. **Editor remains visually responsive at N=10,000** — moving cursor across viewport, panning, zooming all stay smooth (no perceptible lag).
3. **Manual picking parity** — clicking entities in a normal scene (e.g. 10 sprites) selects the expected entity. No regression in the editor's perceived pick behaviour.
4. **Smoke scenario A (point query):** scene with 5 sprites at known positions, query for a point inside one of them, assert returned UUID matches. Query for a point outside all of them, assert Nil. Query for a point at the overlap of two sprites with different z, assert the higher-z one wins.
5. **Smoke scenario B (rect query):** same scene; query a rect that contains 2 of 5 sprites, assert exactly those 2 are returned. Query an empty rect, assert empty set.
6. **Smoke scenario C (rotated AABB):** a single 45°-rotated sprite; query the four corners of its tight bbox, assert all four are inside; query a point just outside the tight bbox, assert not inside.

## Test extensions required

- Three new smoke scenarios (A, B, C above) — direct against the spatial-index API. No rendering required.
- One additional smoke scenario integrating the index with the existing batching test: build a scene of 4 sprites spread across the world, set the renderer viewport to cover only 2 of them, call OnRender, assert `GetLastFrameDrawCallCount()` reflects only the visible 2.
- Existing 17/17 must still pass.

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building. -->

### 2026-05-17 — Clear spatial index in `ProcessPendingRestore`

Without this, the simulation-snapshot restore wipes `m_Registry` + `m_Entities` but leaves stale UUIDs in `m_SpatialIndex` from the previous frame. The next hover-pick's zMap callback then dereferences `GetEntityComponent<Transform>` on a stale UUID; `m_Entities[uuid]` (operator[]) inserts a default-constructed `entt::null` entry, and `registry.get<Transform>(null)` segfaults inside EnTT. Fix: `m_SpatialIndex.Rebuild({})` immediately after the registry/entities/children clears in `ProcessPendingRestore`. The next `Scene::OnRender` rebuilds the index from the restored entity set.

### 2026-05-17 — Picking needs its own panel-aware world transform (`PanelMouseToWorld`)

`Renderer::ScreenToWorldPos` was off by `(vp_h - panel_h)/zoom` in Y for the level-editor panel — picks landed ~380 px too high at typical maximized-window sizes. Reason: the renderer's projection covers the full window framebuffer (`m_ViewportHeight`), `glViewport` is set to the same size, but the level-editor FBO is panel-sized so only the bottom panel_h rows of the projection are written. ImGui then displays the FBO UV-flipped, so panel-top corresponds to FBO-top-row = world Y ≈ `cam.y + (vp_h - panel_h)/zoom`. The old FBO-pixel-readback pick path happened to match because it sampled directly from the panel-sized FBO. The spatial-index pick path uses world coords, so it needs the offset applied. Implemented as a tiny `EditorLayer::PanelMouseToWorld(panelX, panelY)` helper called from the three pick sites; not pushed into Renderer because the panel-vs-window-FB mismatch is the editor's concern, not the renderer's.

## Spec amendments

<!-- Append-only log of times the spec changed mid-implementation. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Incremental updates.** Track transform changes via EnTT signals; insert/remove/move in the tree instead of full rebuild. Worth it once rebuild cost shows up in profiling at the scale you're targeting.
- **Loose quadtree** for cleaner boundary-straddling.
- **Spatial-index-backed grabber pick.** Replace the remaining FBO usage entirely.
- **Frustum-culling expansion for the v2 sampler-array batching path** (when v2 lands).
- **Visualization overlay** — debug view that draws the quadtree node boundaries on top of the scene. Useful for tuning, also a great screenshot for the resume.
