# Bug 0015: sprite z-ordering ignored in the batched render path

> Status: **fixed**
> Severity: **High**
> Tier: **1**
> Logged: 2026-05-26
> Found while: building the Flappy Bird demo game

---

## Symptom

`Transform.position.z` has no effect on draw order in the editor viewport / play window. A background sprite at `z=0` renders *in front of* a ground sprite at `z=1`; layering appears arbitrary regardless of the z values set.

## Suspected location

`Hamster-Core/src/Core/Scene.cpp`, `Scene::OnRender` (the world/batched render path).

## Reproduction

1. Create two overlapping sprite entities with different textures.
2. Give one `z=0` and the other `z=1`.
3. Observe: which one draws on top does not correspond to z; it follows entity/spatial order instead.

## Root cause

`Scene::OnRender` sorts `m_RenderGroup` by `position.z` ascending, but the batched render path **no longer iterates that sorted group**. It iterates `m_SpatialIndex.QueryRect(viewport)`, which returns UUIDs in **quadtree traversal order**, and submits them to the sprite batch in that order. With the depth test disabled (`Renderer.cpp`: `glDisable(GL_DEPTH_TEST)`), draw order is pure painter's algorithm — i.e. submission order — so layering follows quadtree order, not z. The `z` passed to `Renderer::SubmitSprite` only controls *batch-flush boundaries*, never ordering. The `m_RenderGroup.sort` call has been vestigial since the spatial-index culling feature replaced `m_RenderGroup.each` with the `QueryRect` loop. (The flat/picking path still uses `m_RenderGroup.each`, so it stayed z-correct — which is why this hid.)

## What would have prevented this

A smoke/visual check that two overlapping sprites with distinct z draw in z order, or a comment/assert noting that the spatial-index path must preserve the group's z sort, would have caught the regression when culling was introduced.

## Fix

`Hamster-Core/src/Core/Scene.cpp` — after `QueryRect`, sort the `visible` UUID list by each entity's `Transform.position.z` ascending before submitting to the batch. Restores painter's-algorithm layering (lower z behind, higher z in front). Batching is unaffected — the batch already flushes at z boundaries, which the sort groups together. `<algorithm>` was already in use in the file.

## Verification

- Rebuilt Release; smoke test PASS (C++↔Python boundary + frame unaffected).
- Editor: background `z=0` now draws behind ground `z=1`; Flappy Bird layers (bg 0 / pipes 1 / ground 2 / bird 3) render correctly. Confirmed by author in the editor.

---

## Related

- Regressed when the spatial-index feature (2026-05-17) replaced the z-sorted `m_RenderGroup.each` render with the unordered `QueryRect` loop.
