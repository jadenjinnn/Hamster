# Feature spec: Sprite preview in PropertyEditor

> Tier: **Sketch**
> Status: **shipped**
> Started: 2026-05-10
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated
- **Tier rationale (1 sentence)**: Contained entirely within the PropertyEditor panel — no API, file format, or cross-module changes.

---

## Problem

The PropertyEditor's sprite component section shows only data fields (texture path, color tint) with no visual feedback. The author has to run the simulation or mentally map a file name to a texture to know what an entity looks like. A thumbnail preview would let the author confirm sprite assignment and tint at a glance while editing entity properties.

## In scope

- Large-ish square thumbnail of the assigned texture, tinted by the entity's sprite color, displayed in the sprite component section of the PropertyEditor
- Sprite information displayed next to the thumbnail: texture file name, pixel dimensions, color tint
- Empty placeholder box when no texture is assigned

## Out of scope

- Zooming, panning, or pixel inspection on the thumbnail
- Changing the texture or tint by interacting with the thumbnail (editing stays via existing fields)
- Animated sprite / spritesheet preview
- Sprite preview anywhere outside the PropertyEditor (e.g., asset browser tooltips)

## API sketch

```cpp
// In PropertyEditor's sprite component section (ImGui):
// Layout: [thumbnail square] [info column]
//
// Thumbnail: ImGui::Image() with the sprite's OpenGL texture ID, tinted via ImGui's tint_col parameter
// Info: file name, WxH dimensions, color tint display
// No-texture case: ImGui::Dummy() or colored rect as placeholder
```

---

## Why this approach (AUTHOR WRITES THIS — Claude must not draft)

<!-- AUTHOR: write your rationale here -->
Easy, small change allowing users to preview what the sprite looks like. Sets up for future possible sprite editor.

## Risks / what could go wrong

- **Texture not yet loaded**: `AssetManager::AddTextureAsync` loads on a background thread. If the PropertyEditor renders before the GPU upload completes, the texture ID could be invalid or zero. Need to check texture readiness and fall back to placeholder.
- **Large textures dominating the panel**: A 4096×4096 texture displayed as a thumbnail is fine (ImGui samples it down), but if the aspect ratio is extreme (e.g., 16×2048 strip), the square thumbnail will distort it or waste space. Need to decide: always square with letterboxing, or fit-to-aspect?

## Success criteria

- Select an entity with an assigned sprite texture → PropertyEditor shows a tinted thumbnail with correct file name, dimensions, and tint values next to it
- Select an entity with a sprite component but no texture → PropertyEditor shows an empty placeholder box, info fields show "None" or equivalent
- Change the color tint on an entity → thumbnail updates to reflect the new tint in the same frame
- Build succeeds, smoke test passes (no regression)

## Test extensions required

None — pure additive UI in the PropertyEditor panel. The smoke test doesn't exercise ImGui panel rendering. Manual visual verification is the appropriate test for this feature.

---

## Decisions during implementation

### 2026-05-10 — Tint field is editable via ColorEdit3

Spec originally scoped tint as display-only text. Author requested an interactive color picker instead. Added `ImGui::ColorEdit3` with `NoInputs` flag (shows a clickable color swatch that opens a picker popup). Writes back to `Sprite::colour` directly. This is a small scope addition beyond the original "display info" intent.

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

<!-- Captured during implementation if anything comes up. -->
