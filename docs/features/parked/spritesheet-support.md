# Feature placeholder: spritesheet support

> Tier: **Full spec** (provisional guess — pending proper /feature pass)
> Status: **draft (placeholder)**
> Started: 2026-05-19
> Spec author: Jaden

> **This is a lightweight capture, not a proper spec.** Re-run `/feature` and elevate to a real spec when this feature is picked up for implementation.

---

## One-line summary

Import a single PNG, draw rectangles in an editor UI to define sub-sprites, and have each sub-sprite show up as a first-class asset in the Asset Browser — grouped under its parent spritesheet, Unity-style.

## What the user described (verbatim, paraphrased)

- "I can import a sprite sheet, draw out where each sprite is and we cut them up into separate sprites."
- "They should all be grouped in the asset browser by sprite sheet like in Unity."

## Rough scope

In:
- Asset Browser: spritesheet imports show as collapsible group cards (parent: the source PNG; children: each defined sub-sprite).
- A "Spritesheet Editor" modal / panel: opens by double-clicking the source spritesheet, shows the image with a draw-rectangles overlay. User adds, edits, deletes named regions. Save persists to a sidecar.
- New sidecar format: `<sheet>.png.sheet` (JSON) listing region rectangles + name + UUID per sub-sprite. Probably an extension to the existing `.meta` mechanism rather than a wholly new file.
- Sprite component accepts either a full Texture or a sub-sprite UUID. Choose at design time:
  - **Option A** (Unity-like, cheaper for batching): sub-sprites are UV regions over the source Texture's GL handle. Sprite stores `{textureUUID, uvRect}` or `{subSpriteUUID}` that resolves to that pair. Renderer uses the region instead of `[0,1]×[0,1]`.
  - **Option B** (simpler, fatter on GPU): cut the PNG into separate Texture objects at import time. Each sub-sprite has its own GL texture handle.
- Drag a sub-sprite onto a Sprite component, same as a regular texture today.

Out of scope (initial guesses, lock during proper /feature pass):
- Auto-slice / grid-slice (Unity has this for evenly-spaced sheets). Manual rectangles only for v1.
- Animation timeline that auto-pulls a sequence of sub-sprites for a flap loop (would be nice with the existing Animation system, but separate feature).
- Trim / pivot / 9-slice metadata.
- Polygonal regions. Rectangles only.
- Atlas packing in the other direction (combining many sprites into one sheet at export).

## Tentative classification

- **Reversibility: Sticky** — changes the `Sprite` component data model and the on-disk asset model. Any saved scene file referencing sub-sprites locks us into the design.
- **Scope: Cross-cutting** — `Sprite` component, Renderer (UV handling), AssetManager (sub-asset concept), Asset Browser UI, save format, sprite batcher (will benefit because shared atlas = bigger batches).
- **Tier guess: Full spec.**

## Open questions (need answering during real spec pass)

- **Option A vs B above** — biggest design call. Option A integrates with sprite-batching for free but requires careful Sprite-component design. Option B is simpler but loses batching wins.
- How are sub-sprites identified persistently? Sub-UUIDs minted at slice time, stored in the sidecar? Renames in the editor — how do they propagate?
- Asset Browser grouping: tree view? Expandable card? Folder-like card that drills into a child grid?
- What if the source PNG is replaced (same path, different pixels)? Re-slice? Warn? Invalidate stale regions?
- Behaviour when a scene references a sub-sprite that's been deleted from the sheet (Unity shows pink "missing texture") — mirror the missing-script red-placeholder pattern already used for behaviours?
- Does the Spritesheet Editor support animation preview directly, or is that a separate "import these N sub-sprites as an animation" action that hands off to the existing `.hanim` system?

## Related

- The existing **Animation** system (`.hanim`) currently takes one texture per keyframe. Sub-sprites should plug in naturally once Sprite supports them. Likely the most-asked-for next step after this lands.
- The existing **sprite-batching** v1 batches by `textureID`. Option A would *increase* batch sizes (many sub-sprites → one texture → one draw call) — a meaningful perf win and an argument for Option A.
- A hypothetical asset-rename / move feature (Unity's "ProjectSettings → Editor → Sprite Atlas" etc.) is way out of scope here.
