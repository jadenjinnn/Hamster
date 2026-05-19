# Bug 0010: framerate drops when zooming out (suspected: dot grid)

> Status: **open**
> Severity: **Medium**
> Tier: (assigned at fix time)
> Logged: 2026-05-19
> Found while: project-resolution-and-play-window feature, stage 4

---

## Symptom

When zooming out in the editor's scene viewport, FPS drops noticeably. Author suspects the dot grid: as the camera zooms out, more grid dots fall inside the visible region and each is drawn as its own `DrawFlat` call. At extreme zoom-out, this could be thousands of per-frame draw calls.

## Suspected location

`Hamster-Wheel/src/EditorLayer.cpp` — the dot-grid render loop (around line 340–350, the block that iterates `wx, wy` over a spacing grid and calls `m_Renderer->DrawFlat(...)` once per dot).

## Reproduction

1. Build the editor (`cmake --build build --target Hamster-Wheel -j 8`).
2. Open any project.
3. Note the FPS counter (top-right of scene viewport).
4. Hold the zoom-out shortcut / scroll wheel to zoom the camera far out.
5. Observe: FPS drops the further you zoom out; recovers when you zoom back in.

---

## Investigation

<!-- Filled in at /bug-fix time. -->

## Root cause

<!-- Pending /bug-fix. -->

## What would have prevented this (AUTHOR WRITES — Tier 2/3 only)

<!-- AUTHOR: write one sentence in your own words. -->

---

## Fix

<!-- Pending. -->

## Verification

<!-- Pending. -->

---

## Related

Likely overlaps with the existing sprite-batching v1: if dots are submitted via DrawFlat (a per-call draw), batching them through the sprite-batcher or a dedicated point-list draw call would collapse the work into one draw per frame regardless of zoom.
