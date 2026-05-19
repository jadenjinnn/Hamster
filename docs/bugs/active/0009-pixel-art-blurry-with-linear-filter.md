# Bug 0009: pixel-art sprites blurry — engine uses GL_LINEAR filtering on every texture

> Status: **fix-proposed**
> Severity: **High**
> Tier: **1**
> Logged: 2026-05-19
> Found while: flappy-bird-sample feature (importing real pixel-art sprites for the first time)

---

## Symptom

Pixel-art sprites loaded into the editor look blurred / smeared when the scene viewport zoom is anything other than the source-art's native pixel scale. Effect is most visible on small-source sprites (e.g. a 16×16 or 32×32 bird) displayed at 4×–8× zoom: edges become anti-aliased gradients instead of crisp pixel steps.

## Suspected location

`Hamster-Core/src/Renderer/Texture.cpp:43-44, 77-80` — both `Texture` constructors (path-based and `Init`-from-data) hard-code:
```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```
There is no API to select nearest-neighbour filtering. `FramebufferTexture` and `FontAtlas` also use `GL_LINEAR` but they're not the cause of the user-visible blur on game sprites.

## Reproduction

1. Build the editor (`cmake --build build --target Hamster-Wheel -j 8`).
2. Open any Hamster project.
3. Add a small (≤64px square) pixel-art PNG via the Asset Browser.
4. Drag the texture onto an entity's Sprite component.
5. Scale the entity transform up by 4×–8×, OR zoom the scene viewport in.
6. Observe: edges of the sprite are blurred / interpolated, not crisp pixel boundaries.
7. Expected: each source-art pixel renders as a flat, equal-sized block on screen (nearest-neighbour sampling).

---

## Root cause

`Texture::Init` and the path-based `Texture` constructor both unconditionally call `glTexParameteri(..., GL_TEXTURE_MIN_FILTER, GL_LINEAR)` and `GL_TEXTURE_MAG_FILTER, GL_LINEAR`. Linear filtering averages neighbouring texels when a fragment samples between two pixel centres; with pixel art, that average is exactly the blurred-edge artifact. The engine has no opt-out — every loaded texture gets linear filtering.

---

## Fix

Implemented on 2026-05-19. Tier 1, single commit.

- `Hamster-Core/src/Renderer/Texture.h` — added `enum class FilterMode { Linear, Nearest }`. `Texture(path, FilterMode)` ctor and `Init(textData, FilterMode)` both gained a defaulted `FilterMode = Linear` parameter, so any caller not passing one is unchanged.
- `Hamster-Core/src/Renderer/Texture.cpp` — both texture-creation paths now select `GL_NEAREST` or `GL_LINEAR` based on the parameter.
- `Hamster-Core/src/Utils/AssetManager.cpp` — all three sprite-loading sites (async `AddTextureAsync`, sync `AddTexture` taking a path, and the deserialise-path overload taking UUID + name) pass `FilterMode::Nearest`.
- `FramebufferTexture` and `FontAtlas` left on `Linear` — correct for FBO blits and font atlases.
- No save-format change.

## Verification

- Repro steps re-run on 2026-05-19: PENDING — needs visual check by author. Launching the editor for that purpose.
- Smoke test result: PASS (27/27, full SmokeTest invocation took 2.57s).
- New test added: none planned (visual-only; smoke test can't easily assert pixel crispness).

---

## Related

- Blocks: `docs/features/active/flappy-bird-sample.md`
- Out of scope for this bug, future work: pixels-per-unit scaling, pixel-snapping entity positions, integer-scale reference-resolution upscale (full "Unity pixel-perfect" behaviour).
