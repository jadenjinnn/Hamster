# Feature spec: sprite-batching

> Tier: **Sketch**
> Status: **draft**
> Started: 2026-05-17
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated (Renderer module + one caller line in Scene + new benchmark Python script)
- **Tier rationale (1 sentence)**: Self-contained renderer change with no external API surface; the staging (v1 same-texture → v2 sampler array) is implementation sequencing, not design complexity that needs spec coverage.

---

## Problem

`Renderer::DrawSprite` issues one `glDrawArrays` call per visible sprite, with a texture bind, two uniform updates, and a VAO bind/unbind around it. At ~100 sprites this is invisible; at ~5000 sprites it tanks frame rate and pegs the CPU on driver overhead. The engine has no measurable rendering optimization story today, and the author wants a concrete, defensible performance bullet for resume use ("reduced draw calls from N to ceil(N/16) for 5000-sprite benchmark, raised FPS from X to Y on hardware Z"). The number only exists if both a benchmark exists to measure against and the optimization actually lands.

## In scope

- **v1 — same-texture batching.** Replace per-sprite `DrawSprite` with a submit/flush API. Accumulate sprite quad vertices into a single dynamic VBO. Flush on: texture change, z-value change (preserves sort order), or batch full. One `glDrawArrays` per batch instead of per sprite.
- **v2 — multi-texture batching via sampler array.** Vertex shader gains a per-vertex texture-slot index. Fragment shader binds up to 16 textures (`uniform sampler2D images[16]`) and indexes into the array. Flush only on slot exhaustion (instead of every texture change) or z-change.
- **Benchmark Python script.** `benchmark_batching.py` spawns N sprites with M unique textures via existing `create_entity()` API. Configurable N and M at the top of the file.
- **Per-frame draw-call counter.** Renderer exposes `GetLastFrameDrawCallCount()`. Console panel displays it next to FPS (small addition to existing FPS display).
- **One-shot measurement.** Author runs benchmark, records baseline vs v1 vs v2 numbers in `docs/decisions.md` under this feature's close-out entry. Becomes the resume bullet source.

## Out of scope

- Editor pick-render path (the `Scene::OnRender(renderFlat=true)` pass that draws flat colors for `glReadPixels`). Stays per-sprite — it only runs on hover/click, not steady-state, and batching it would double the surface area.
- `Renderer::DrawFlat` / `DrawGuizmo` / `DrawHoverOutline` — the flat-shader path stays unchanged.
- Sprite atlas tooling or runtime atlas packing.
- Text rendering, particles, line draws.
- Z-sort algorithm changes — the existing per-frame `m_RenderGroup.sort<Transform>` keeps running. Batching just respects its output.
- Persistent-mapped buffer / triple-buffering / fancy upload strategies. Start with `glBufferSubData` per flush; revisit only if the benchmark shows upload itself is the bottleneck.

## API sketch

Internal renderer API:

```cpp
// Renderer.h additions
void BeginSpriteBatch();
void SubmitSprite(Texture &texture, glm::vec2 position, glm::vec2 size,
                  float rotation, glm::vec3 colour, float z);
void EndSpriteBatch();   // flushes anything remaining
uint32_t GetLastFrameDrawCallCount() const;
```

`Renderer::DrawSprite` keeps its current signature for now and internally forwards to `SubmitSprite` so existing call sites (editor pick path, anywhere else) don't break. Scene::OnRender wraps its sprite-draw loop:

```cpp
// Scene.cpp around the existing loop
renderer->BeginSpriteBatch();
sortedView.each([&](auto &transform, auto &sprite) {
    renderer->SubmitSprite(*sprite.texture, transform.position, transform.size,
                           transform.rotation, sprite.colour, transform.z);
});
renderer->EndSpriteBatch();
```

Python side — no change. Benchmark script uses existing API:

```python
import Hamster, random

NUM_SPRITES = 5000
NUM_UNIQUE_TEXTURES = 50   # 1 for v1 baseline, 50 for v2 measurement

class BenchmarkSpawner(Hamster.HamsterBehaviour):
    def on_create(self):
        for i in range(NUM_SPRITES):
            e = self.create_entity(name=f"s{i}")
            e.add_component("Sprite", texture=self.pick_texture(i))
            t = e.transform
            t.position = Hamster.vec2(random.uniform(0, 1920), random.uniform(0, 1080))
```

---

## Why this approach

- **Submit/flush API rather than replacing `DrawSprite` outright** so any unconverted caller keeps working during the transition. The pick-render path explicitly stays on per-sprite.
- **Same-texture batching first (v1), sampler array second (v2)** because v1 alone is the headline number for the resume bullet (O(N) → O(1) for same-texture sprites) and ships end-to-end faster. v2 is an additive refinement once the harness exists.
- **Sampler array over texture atlas** — was originally discussed. Atlas requires either offline packing tooling or runtime repacking on every texture add (since AssetManager loads textures dynamically). Sampler array works with whatever textures are already loaded, no new tooling, same bullet readability. Atlas-style packing is parked as future work if it ever becomes interesting.
- **16-slot cap** — GL 4.0 guarantees 16 texture image units across all stages; most desktop drivers expose 32 in the fragment stage but the spec floor is 16. Cap there for portability without per-machine probing.
- **`glBufferSubData` per flush** rather than persistent-mapped / orphan-and-fill — simplest path that gets a real number. Optimize if the benchmark shows the upload itself is hot.

## Risks / what could go wrong

1. **Vertex format inflation.** Going from per-sprite uniforms to per-vertex attributes means each of the 6 vertices per sprite carries: position (8 bytes), UV (8 bytes), color (12 bytes), z (4 bytes), and in v2 slot index (4 bytes). ~32–36 bytes per vertex × 6 = ~200 bytes per sprite. 10k sprites = ~2 MB uploaded per frame. Fine, but worth measuring upload time vs draw time in the benchmark.
2. **Z-sort interaction limits batch effectiveness.** Scene sorts entities by `transform.z` before drawing. If z values are fragmented (e.g. layer 0, layer 1, layer 0, layer 1...) the batch flushes constantly even with one texture. The benchmark scene should keep z constant across sprites for the v1 headline number, and the spec should disclose this constraint in the measurement notes — not bury it.
3. **Animation system causes texture churn.** Animated sprites swap `Sprite::texture` per frame. Within a single frame all animated sprites still hold one texture each, so v1 batching still flushes on each unique texture. Not a bug, just bounds the achievable ratio when animations dominate. v2 (sampler array) recovers this.
4. **Sampler array shader limit on older drivers.** Some drivers report `GL_MAX_TEXTURE_IMAGE_UNITS` as 16 but reject `uniform sampler2D x[16]` due to indirect indexing restrictions. Mitigation: query `GL_MAX_TEXTURE_IMAGE_UNITS` at startup; fall back to v1 single-texture mode if < 16. (Author's machine reports 32, so this is a safety net not a primary path.)
5. **GPU memory leak from VBO orphaning.** If the dynamic VBO is allocated per `BeginSpriteBatch` instead of reused, every frame leaks until GC. Implementation must allocate once at Renderer construction; `Begin` resets the write cursor, doesn't allocate. Currently no destructors on GL wrappers (separate known bug); make sure the new VBO is owned by Renderer with a real destructor.
6. **Pick path divergence.** Editor pick render uses the unbatched `DrawSprite` path with a flat-color shader. After this feature, the pick path's flat-shader call still issues per-sprite draw calls. That's the documented intent (out of scope), but means hover-over-N-sprites is *not* improved by this feature. Worth being clear about in the bullet ("steady-state render", not "all render").

## Success criteria

All numbers measured on author's machine (clang-cl 22.1.5, Windows 11, GPU TBD-record-at-measurement-time):

1. **Baseline measurement recorded.** Before any code change: run benchmark with N=5000, M=1. Record draw-call count and FPS. Expected: 5000 draw calls, FPS likely single-digit-or-tens depending on GPU.
2. **v1 measurement recorded.** After v1 lands: run benchmark with N=5000, M=1. Draw-call count drops to ≤10 (one or two flushes total). FPS at minimum 60 (vsync-capped) on a developer GPU.
3. **v2 measurement recorded.** After v2 lands: run benchmark with N=5000, M=50. Draw-call count drops to ≤10 (`ceil(M/16) = 4` slot exhaustions plus a small overhead). Compare to v1-with-M=50 (which would be ~50 draw calls due to per-texture flush).
4. **Visual parity.** Run smoke test and editor manually — sprites render identically to before (position, rotation, color, z-order). No visual regression.
5. **Smoke test passes.** Extended smoke includes a multi-sprite scene that exercises batching (see Test extensions).
6. **Numbers documented.** `docs/decisions.md` gets a `sprite-batching close-out` entry with the three measurements and a one-paragraph methodology note. This is the source of truth for the resume bullet.

## Test extensions required

- Extend the existing smoke test with a `batching_scenario`:
  - Load a small scene with 4 sprites: 2 using texture A, 2 using texture B, all at z=0.
  - Run one frame.
  - Assert `Renderer::GetLastFrameDrawCallCount()` is exactly 2 (one batch per texture in v1; one batch total in v2 since 2 textures < 16 slots → adjust assertion when v2 lands).
  - Assert no GL errors via `glGetError`.
- Existing 15 smoke scenarios must continue to pass — they cover the pick path indirectly and verify the unbatched fallback is intact.

The benchmark script itself is *not* part of the smoke test (it spawns 5000 entities and runs for measurement, not assertion). Ship it under `Resources/Benchmarks/` (or similar) and run manually.

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building. -->

## Spec amendments

<!-- Append-only log of times the spec changed mid-implementation. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Batched flat / pick render.** Apply the same submit/flush pattern to the flat shader path; would speed up the FBO pick render and grabber draws.
- **Sprite atlas packer.** Offline tool that packs multiple textures into one. Reduces texture count which makes v2 batching even more effective.
- **Persistent-mapped buffer upload.** If `glBufferSubData` shows up as a measurable cost in the benchmark.
- **Per-frame draw-call HUD.** Already in scope (Console FPS display), but could be expanded into a full GPU stats panel.
- **Z-sort batching hint.** Have the renderer auto-group same-texture sprites that happen to share z, reducing flushes from fragmented z values.
