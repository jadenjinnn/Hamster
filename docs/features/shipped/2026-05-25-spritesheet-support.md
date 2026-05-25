# Feature spec: spritesheet support

> Tier: **Full spec**
> Status: **shipped** (all 9 stages; 2026-05-25)
> Started: 2026-05-19
> Spec author: Jaden

---

## Classification

- **Reversibility**: Sticky
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: Introduces a new on-disk format (`.png.sheet` sidecar) and changes how the engine resolves sprite sources (`Sprite` UUID → either a Texture or a SubSprite); reverting after ship breaks any saved scene that references a sub-sprite, so Full spec depth is warranted.

---

## Problem

Today every sprite asset is a separate PNG/JPG that gets its own `Texture` + GL handle. There is no concept of "this image contains many sub-sprites cut from a single sheet." That blocks two things at once:
1. **Authoring** — a bird-flap animation requires three separate PNG files (or a one-time external splitting step), and the asset browser has no way to express that they belong together.
2. **Performance** — sprite-batching v1 collapses entities sharing the *same texture* into one draw call. With each frame in its own texture, atlas content can't take advantage of that batching. A real game with hundreds of on-screen UI elements or character frames pays N draw calls instead of 1.

A spritesheet workflow — one PNG, many named sub-sprite regions sharing the parent's GL handle — solves both problems at once.

## In scope

- **`.png.sheet` sidecar format** (JSON) — sits next to a `.png` in the project, holds an array of sub-sprite regions: `{uuid, name, rect: [x, y, w, h] in pixel coords}`. Read + write at project open / save. Loaded by `AssetManager` whenever a registered texture has a matching sidecar.
- **`SubSprite` asset type** in `AssetManager` — UUID-keyed map, each entry holds `{parentTextureUUID, pixelRect, name}`. Lives alongside `m_Textures` and `m_Scripts`.
- **`AssetManager::ResolveSpriteSource(UUID)`** — unified lookup that returns `{Texture*, glm::vec4 uvRect, bool missing}` from a UUID that could be either a Texture or a SubSprite (or unresolved). Sub-sprites resolve to (parent texture, normalised UV rect). Regular textures resolve to (themselves, full UV `(0,0,1,1)`).
- **Flat global name lookup** — `AssetManager::FindAssetByName(name)` searches Textures + SubSprites in one combined namespace. Collisions are validated at rename / slice-save time. Used by `EntityHandle::SetTexture(name)` so scripts can do `entity.set_texture("wing_up")` regardless of whether `wing_up` is a sub-sprite or a regular texture.
- **`Sprite` component** continues to store a single UUID. No struct change; the meaning of "what does this UUID point to" widens via `ResolveSpriteSource`. Existing scenes (with Texture UUIDs) load unchanged.
- **Renderer UV support** — `DrawSprite` + `SubmitSprite` gain a defaulted `glm::vec4 uvRect = (0,0,1,1)` argument. The non-batch `SpriteShader` gets a `uvRect` uniform; the batch `SpriteBatchShader` bakes the rect into per-vertex UVs at CPU submission time (no shader change). Other render paths (FlatShader, UI shaders, FontAtlas) unaffected.
- **Spritesheet Editor — floating ImGui window** (`ColliderEditor` pattern). Opens via:
  - Asset Browser → double-click a sheet card, OR
  - Asset Browser → right-click any texture card → "Edit Slices..."
  - Asset Browser → "Import Spritesheet" button (file picker → imports PNG as regular texture, then opens editor empty)
- **Manual rectangle slicing** — draw a new rect by drag, click an existing rect to select, drag edges/corners to resize, drag body to move, Del to delete. List view on the side of the window shows all regions with their auto-names; each row has an editable name field.
- **Sub-sprite auto-naming**: `<sheet_stem>_<index>` (e.g., `bird_atlas_0`, `bird_atlas_1`) in slice / insertion order. User can rename any sub-sprite via the list view in the editor.
- **Asset Browser display**:
  - A texture with a `.sheet` sidecar shows a small expand caret on its card.
  - Clicking the caret expands the card downward in-place to reveal sub-sprite mini-cards in a grid (each mini-card shows a thumbnail clipped to its UV rect + its name).
  - Sub-sprite mini-cards are individually clickable, multi-selectable (Ctrl-click / shift-click), and draggable.
  - Sheet PNG itself remains usable as a regular texture (drag the parent card onto a Sprite → entity renders the full sheet).
- **Multi-select drag from Asset Browser → AnimationPanel timeline** — drops N keyframes at evenly spaced times starting from the drop time, in selection order.
- **Drag a single sub-sprite onto a Sprite component** in the PropertyEditor → assigns the sub-sprite's UUID to the Sprite (same path as dragging a regular texture today).
- **MISSING placeholder** — pink-and-black checker rendered when `ResolveSpriteSource` can't resolve a UUID (parent sheet deleted, sub-sprite deleted, PNG replaced with mismatched dimensions where the rect now falls out-of-bounds). PropertyEditor also shows red `MISSING: <name>` text on the sprite field, mirroring the existing missing-script affordance.
- **Sub-sprite delete** in the Spritesheet Editor — removes the entry from the sidecar on save. Scene references become MISSING.
- **Sheet rename** travels with the existing `AssetManager::RenameAsset` machinery — `.sheet` sidecar moves alongside the PNG via the same atomic-move path that already handles `.meta` today.

## Out of scope

- **Auto-grid slicing** ("4 cols × 2 rows → 8 evenly sized rects"). Manual rectangles only for v1. Future work.
- **Polygonal / non-rect regions.** Rectangles only.
- **Pivot points, 9-slice metadata, trim** (tight bounds around opaque pixels).
- **Atlas packing** (combining multiple sprites into a sheet at export time).
- **Animation preview inside the Spritesheet Editor.** Use the existing AnimationPanel.
- **Re-slice prompt on PNG replacement.** If the parent PNG is replaced with one of different dimensions, existing rects stay where they were in pixel coords; rects that no longer fit render as MISSING. No auto-detect, no warning dialog (future work).
- **Sub-sprite drag-reorder in the Asset Browser.** Mini-cards display in sidecar / slice order.
- **Right-click sub-sprite mini-card → Rename / Delete** in the Asset Browser. Rename and delete happen in the Spritesheet Editor only. (Browser-side context menu = future work.)
- **"Convert this regular texture to a Spritesheet" affordance** other than the right-click "Edit Slices..." flow.
- **Spritesheet UI on the Hamster-Wheel-old branch.** New editor only.
- **Sub-sprite preview / colour-tint inspection** in the Spritesheet Editor — preview the sheet as-is, no per-region tinting.

## API sketch

Python side gains **nothing new**. Sub-sprites work through the existing `set_texture` flat-name lookup:

```python
class Bird(Hamster.HamsterBehaviour):
    def on_update(self, dt):
        # No special API — flat name resolves across textures + sub-sprites.
        # Auto-named or renamed sub-sprite both work.
        self.set_texture("wing_up")
```

C++ side, new shape:

```cpp
// New asset type alongside Texture / HamsterScript / AnimationData.
struct SubSprite {
  UUID uuid;
  UUID parentTextureUUID;
  glm::ivec4 pixelRect;        // x, y, w, h in source-pixel coords
  std::string name;
};

class AssetManager {
public:
  // Sheet authoring API used by the SpritesheetEditor + sidecar loader.
  UUID AddSubSprite(UUID parentTextureUUID, glm::ivec4 pixelRect,
                    const std::string &name);
  void RemoveSubSprite(UUID subSpriteUUID);
  void RenameSubSprite(UUID, const std::string &);

  // Unified resolution — Sprite component / Animation keyframe / Renderer
  // all call this. Returns missing=true if the UUID matches neither map.
  struct SpriteSource {
    Texture *texture = nullptr;
    glm::vec4 uvRect = {0.0f, 0.0f, 1.0f, 1.0f};  // normalised [0,1]
    bool missing = false;
  };
  SpriteSource ResolveSpriteSource(UUID uuid) const;

  // Flat-name lookup — Textures + SubSprites in one namespace. Used by
  // EntityHandle::SetTexture; collision-checked at rename time.
  UUID FindAssetByName(const std::string &name) const;

  // ... existing API unchanged ...
private:
  std::unordered_map<UUID, std::shared_ptr<SubSprite>> m_SubSprites;
};

// Renderer gains an optional UV-rect arg (default = full texture).
void Renderer::DrawSprite(Texture &texture, glm::vec2 position,
                          glm::vec2 size, float rotation, glm::vec3 colour,
                          glm::vec4 uvRect = {0, 0, 1, 1});
void Renderer::SubmitSprite(Texture &texture, glm::vec2 position,
                            glm::vec2 size, float rotation, glm::vec3 colour,
                            float z, glm::vec4 uvRect = {0, 0, 1, 1});
```

Sidecar format on disk (`bird_atlas.png.sheet`, JSON):

```json
{
  "subsprites": [
    {"uuid": "0a814ea1-41c5-40c6-9fc5-f87eff1af333",
     "name": "bird_atlas_0",
     "rect": [0, 0, 32, 32]},
    {"uuid": "1f95d8b2-...", "name": "wing_up",  "rect": [32, 0, 32, 32]},
    {"uuid": "2c4e3a01-...", "name": "bird_atlas_2", "rect": [64, 0, 32, 32]}
  ]
}
```

---

## Design

### Data structures

- **`SubSprite`** (`Hamster-Core/src/Utils/AssetManager.h`) — new struct, UUID-keyed; holds `parentTextureUUID`, `pixelRect` (`glm::ivec4`), `name`.
- **`AssetManager::m_SubSprites`** — `std::unordered_map<UUID, std::shared_ptr<SubSprite>>`. Same ownership pattern as `m_Textures` / `m_Scripts`.
- **`AssetManager::SpriteSource`** — `{Texture*, glm::vec4 uvRect, bool missing}`. The single resolution result type returned by all sprite-source lookups.
- **`MissingTexture`** singleton-style — a small in-memory `Texture` initialised once at `AssetManager` construction with a baked 8×8 pink-and-black checker via direct `glTexImage2D` (no PNG asset; keeps the binary self-contained). Returned by `ResolveSpriteSource` when `missing = true`.
- **`SheetSidecar`** (`Hamster-Core/src/Utils/SheetSidecar.h/.cpp`) — pair of `Read`/`Write` JSON I/O helpers, the same pattern as the existing `MetaFile` sidecar. JSON format mirrors the on-disk example above.

### Module touchpoints

- `Hamster-Core/src/Utils/AssetManager.h/.cpp` — add `SubSprite`, `m_SubSprites`, `AddSubSprite` / `RemoveSubSprite` / `RenameSubSprite`, `ResolveSpriteSource`, `FindAssetByName`, MISSING fallback texture, sheet-sidecar load step.
- `Hamster-Core/src/Utils/SheetSidecar.h/.cpp` (NEW) — JSON read + write for `.png.sheet` files.
- `Hamster-Core/src/Renderer/Renderer.h/.cpp` — `DrawSprite` + `SubmitSprite` take `uvRect`; non-batch sprite shader gets a `uvRect` uniform; batch path bakes UV into vertex attributes at submission time.
- `Hamster-Core/src/Renderer/DefaultShaders/SpriteShader.vs/.fs` — sprite shader UV transform.
- `Hamster-Core/src/Renderer/DefaultShaders/SpriteBatchShader.vs/.fs` — likely no shader change (UV already a vertex attribute; just write different values from the CPU).
- `Hamster-Core/src/Core/Scene.cpp` — `OnRender` (both batch + flat paths) calls `AssetManager::ResolveSpriteSource(sprite.uuid)` and forwards the resulting `(texture, uvRect)` to the renderer instead of `sprite.texture` directly. Animation-frame swap path (`Animation::currentTime` → texture swap) also resolves through this path.
- `Hamster-Core/src/Core/Components.h` — no change to `Sprite` struct; the UUID's *meaning* widens via the resolver. (Internal `Sprite::texture` raw pointer may eventually be deleted, but for v1 we can keep it as a cached resolver result.)
- `Hamster-Wheel/src/Panels/AssetBrowser.cpp` — sheet-card expand caret + inline mini-card grid; multi-select state; drag payload for sub-sprite UUIDs; "Import Spritesheet" button in the toolbar; right-click "Edit Slices..." menu item.
- `Hamster-Wheel/src/Panels/SpritesheetEditor.h/.cpp` (NEW) — floating ImGui window: image preview + zoom-pan, rect drawing / select / resize / move / delete, region list with editable names, Save / Cancel. Owns a copy of the sub-sprite list while editing; commits to `AssetManager` + writes the sidecar on Save.
- `Hamster-Wheel/src/EditorLayer.cpp` — instantiates the `SpritesheetEditor` (one shared instance, like `ColliderEditor`), routes the open-request from AssetBrowser to it.
- `Hamster-Wheel/src/Panels/AnimationPanel.cpp` — multi-sub-sprite drop handler that adds N keyframes at evenly spaced times.
- `Hamster-Wheel/src/Panels/PropertyEditor.cpp` — sub-sprite drop target on the Sprite field (already works for textures via UUID payload — sub-sprite UUIDs are the same payload type); MISSING label rendering when `ResolveSpriteSource` returns `missing = true`.
- `Hamster-Py/src/EntityHandle.h` — `SetTexture` switches from "scan `AssetManager::GetTextureMap()` for a name match" to `AssetManager::FindAssetByName` so sub-sprite names resolve too.
- `test/smoke_test.cpp` — see Test extensions.

### Lifecycle / control flow

```
[Authoring]
  user → AssetBrowser "Import Spritesheet" → file picker
       → AssetManager::AddTexture(picked.png)       (existing path)
       → SpritesheetEditor.Open(textureUUID, empty)
       → user draws 3 rectangles, edits 1 name
       → user clicks Save
         → for each rect:
             AssetManager::AddSubSprite(parentUUID, rect, name)
         → SheetSidecar::Write(parentPath, regions)

[Project open]
  AssetManager::LoadProjectScripts() — existing
  AssetManager::LoadProjectAnimations() — existing
  for each registered Texture with a sibling .sheet sidecar:
    regions = SheetSidecar::Read(parentPath)
    for each region: register a SubSprite with the same UUID stored in JSON
  AssetBrowser refresh — sheets now display the caret affordance

[Scene render]
  Scene::OnRender:
    for each (Sprite, Transform):
      auto src = AssetManager::ResolveSpriteSource(sprite.uuid)
      if (src.missing)  renderer->SubmitSprite(MissingTexture, ...,
                                                fullUV)
      else              renderer->SubmitSprite(*src.texture, ...,
                                                src.uvRect)
    EndSpriteBatch flushes per-(GL-handle) — sub-sprites of the same
    sheet all share one handle, so they collapse into one draw call.

[Slice tweak]
  user → AssetBrowser → right-click sheet → "Edit Slices..."
       → SpritesheetEditor.Open(textureUUID, existingRegions)
       → user moves / resizes / deletes / renames regions
       → user clicks Save
         → SpritesheetEditor diffs against AssetManager state:
             added regions  → AddSubSprite (new UUID)
             removed regions → RemoveSubSprite
             modified regions → mutate in place (UUID preserved)
             renamed regions → RenameSubSprite (collision-checked)
         → SheetSidecar::Write
```

### Edge cases

- **Sheet PNG deleted via Explorer while editor is open** — `ProjectWatcher` posts a remove event; `AssetManager::HandleFileEvents` removes the texture and its sub-sprites. Next frame, any Sprite referencing those UUIDs resolves to MISSING.
- **Sheet PNG replaced with a smaller-dimension PNG** — sub-sprite rects may now fall partially or fully outside the texture. `ResolveSpriteSource` clamps the rect to texture bounds; if the clamped rect has zero area, returns `missing = true`.
- **Rename collision** — user renames `bird_atlas_0` → `wing_up`, but a regular texture already named `wing_up` exists. `RenameSubSprite` returns false; the Spritesheet Editor's row shows a red border + inline message and refuses to save until resolved.
- **Sub-sprite UUID already assigned to a scene Sprite, then sub-sprite deleted** — scene's Sprite UUID becomes unresolvable. MISSING placeholder renders; PropertyEditor shows red `MISSING: <name>` label using the cached name from the sub-sprite's last-known state (cached in the same `Behaviour::cachedNames`-style map, but for Sprite — `m_CachedSubSpriteNames` on Scene or via the existing pattern).
- **PNG file watch event fires *during* a Spritesheet Editor session** — editor's working state holds a copy of the regions; if the watcher signals a sheet change, the editor disables Save and shows "sheet changed externally — close and reopen" rather than fighting for primacy.
- **Multi-select drag across the sheet boundary** — user expands two sheets and Ctrl-clicks one sub-sprite from each, then drags onto AnimationPanel. Works fine — the resulting `.hanim` has keyframes pointing into two different sheets. Sprite batcher won't collapse them into one draw call but the animation itself plays correctly.
- **`AssetManager::FindAssetByName` collision between a Texture and a SubSprite** — should never happen post-collision-check, but as a defensive choice, lookups return SubSprite first (texture is the wider container; user typing a sub-sprite name almost always means the sub-sprite).
- **Sheet with zero sub-sprites in the sidecar** — `.sheet` file exists but `subsprites: []`. The sheet still displays as a sheet card with an expand caret (caret expands to "no slices yet — double-click to add").
- **Editor renames `bird_atlas.png` → `enemies.png`** — `AssetManager::RenameAsset` moves the `.png`, the `.meta`, AND the `.sheet` sidecar (extend the existing rename to know about `.sheet` too). Sub-sprite auto-names retain their old `bird_atlas_*` prefixes (no auto-rename — user can rename by hand if it bugs them).

---

## Full spec

### Sequence diagram: import + slice + use

```
User         AssetBrowser       AssetManager    SpritesheetEditor   SheetSidecar    AnimationPanel
 |                |                  |                  |                |                |
 |--Import Sht-->|                  |                  |                |                |
 |               |--AddTexture----->|                  |                |                |
 |               |                  |<-textureUUID     |                |                |
 |               |--Open(uuid)----->|                  |                |                |
 |               |                  |                  |--render image--|                |
 |--draw rect 1->|                  |                  |                |                |
 |--draw rect 2->|                  |                  |                |                |
 |--draw rect 3->|                  |                  |                |                |
 |--rename rect2->|                 |                  |                |                |
 |--Save-------->|                  |                  |                |                |
 |               |                  |                  |--AddSubSprite x3->|             |
 |               |                  |                  |--Write(regions)-->|             |
 |               |<-expand caret on bird_atlas card----|                |                |
 |               |                                                                       |
 |--expand------>|                  |                  |                |                |
 |               |<-render mini-card grid                                                |
 |--Ctrl-click all 3-->|                                                                 |
 |--drag onto AnimationPanel timeline-------------------------------------- >|           |
 |                                                                          |--AddKeyframe x3 evenly-spaced
 |--press Play->|                                                                        |
 |              |--Scene::OnUpdate (animation advance swaps Sprite.uuid)                 |
 |              |--Scene::OnRender                                                       |
 |                |--ResolveSpriteSource(currentFrameUUID)              |                |
 |                |<-(parentTexture, uvRect)                            |                |
 |                |--SubmitSprite(texture, pos, size, ..., uvRect)      |                |
 |                |--EndSpriteBatch → 1 draw call (all 3 frames share parentTexture)     |
```

### Error handling

- **Sidecar JSON corrupt / unparseable** — `SheetSidecar::Read` returns false + logs to client logger; `AssetManager` skips registering sub-sprites from that sheet. The texture still loads. User sees the sheet card without an expand caret (treated as a plain texture).
- **Slice rect with zero or negative dimensions** — `AddSubSprite` validates `w > 0 && h > 0` and refuses to register the entry; logged as a warning.
- **Slice rect partially outside texture bounds** — registered as-is at slice time (editor lets you draw it). At resolve time, `ResolveSpriteSource` clamps to texture bounds; if clamped area is empty → `missing = true`.
- **`AssetManager::FindAssetByName` finds no match** — `EntityHandle::SetTexture` raises `py::value_error` with the name (consistent with existing behavior).
- **Sub-sprite rename to empty string** — refused at Spritesheet Editor save time; the row shows an inline error.
- **`SheetSidecar::Write` fails** (disk full, permission denied) — Save button logs + leaves the editor open; user can retry.

### Performance considerations

- **Batching win** — the main motivator. 50 entities using sub-sprites of one sheet → 1 draw call instead of 50. Memory cost: ~24 extra bytes per `SubSprite` entry; rounding error at typical sheet counts.
- **`ResolveSpriteSource` is a hot path** — called once per visible sprite per frame inside `Scene::OnRender`. The two-map lookup (sub-sprites first, then textures) is O(1) hash; one unordered_map lookup at scene render scale (10–1000 sprites) is sub-microsecond. Probably negligible — measure if it shows up in profile.
- **MISSING texture** is a single tiny 8×8 GL handle; rendering 1000 missing sprites costs 1 draw call after batching (since they all share the handle).
- **Sidecar I/O is one-shot** at project open. No per-frame I/O.

### Migration / compatibility

- **Existing scenes with regular Texture references** — load identically. `Sprite.uuid` resolves to a Texture; `ResolveSpriteSource` returns `(texture, (0,0,1,1))`; renderer behaves exactly as before. No save-format change.
- **Existing `.hanim` files** — keyframes already store UUIDs. Sub-sprite UUIDs are the same type; existing animations work unchanged. New animations using sub-sprites just store the sub-sprite UUID in the keyframe.
- **Pre-feature `.meta` sidecars** — unchanged. Only the new `.sheet` sidecar is added.
- **Pre-existing projects with no `.sheet` sidecars** — open and behave identically. Sheet support is purely additive.

---

## Why this approach

**Option A (UV regions over parent texture) over Option B (separate textures at import time)** — chosen during design conversation. Option B would orphan sprite-batching v1's wins for atlas content; Option A makes the batcher's "same texture = same draw call" property work for many-sub-sprite scenes. The code cost (UV rect plumbing through Renderer + Scene::OnRender) is real but contained.

**Manual rectangle slicing only** — auto-grid slicing is a 10× faster workflow for uniform sheets but adds a meaningful chunk of UI + naming-convention design. Author confirmed irregular slicing is the v1 case; auto-grid is logged as future work.

**Floating ImGui window for the Slice Editor** — matches `ColliderEditor`. Slicing is a one-shot setup operation (open, cut, save, done), not an ongoing tuning loop, so docking it like AnimationPanel would over-commit screen real estate.

**Sheet PNG remains usable as a regular texture after slicing** — the simpler model. Avoids "what does it mean to drag the parent unsliced sheet onto a Sprite?" confusion (answer: same as today, you get the full image).

**Flat global naming across textures + sub-sprites** — keeps `set_texture("wing_up")` script-friendly. Cost: collisions need validation at rename / slice-save time. Alternative ("bird_atlas/wing_up" sheet-qualified) was rejected to keep script syntax shorter.

**Pink-and-black checker for MISSING** — industry-standard "you have a bug" colour; consistent with Hamster's existing missing-script red-text affordance.

**Tradeoffs accepted:**
- UV plumbing through Renderer + Scene + shader is a non-trivial change. We trade complexity for the batching win.
- Sheet replacement (different dims) doesn't auto-recover; rects that no longer fit silently become MISSING. v1 punts this; future work has a re-slice-prompt or auto-clamp UI.
- Sub-sprite drag-reorder + browser-side rename/delete are deferred. Editor-side is the only mutation surface in v1.

## Risks / what could go wrong

1. **UV plumbing missed on a code path.** `Scene::OnRender` has two branches (batch + flat); `EditorLayer` has its own grabber-pick FBO render; the new MISSING placeholder needs to route through both. Easy to add UV to one path and not the other, producing visual misalignment that's hard to debug.
2. **Sprite component's `texture` raw pointer** is currently dereferenced directly in several spots (renderer cull, FBO render). Migrating these to `ResolveSpriteSource` is mechanical but error-prone; a single missed call site renders sub-sprite content as the full sheet.
3. **`.sheet` sidecar + `.meta` sidecar + the PNG can fall out of sync** — three files moving together. `AssetManager::RenameAsset` already handles the `.png` + `.meta` pair; extending to `.sheet` is the natural extension but the path-join logic needs care.
4. **Sub-sprite UUIDs persist across editor sessions through the sidecar** — if a user externally edits the JSON (or VCS merge conflicts the file), UUIDs can collide or duplicate. `AssetManager::AddSubSprite` should defensively check the UUID isn't already in use; on conflict, log + skip.
5. **Spritesheet Editor's working state** vs `AssetManager` truth. Closing the editor without Save discards changes; the user can still drag stale mini-cards in the asset browser if the AB hasn't repainted. Editor should not mutate `AssetManager` until Save; AB displays AssetManager truth, never the editor's WIP.
6. **Multi-select drag in ImGui** is fiddly. ImGui's `BeginDragDropSource` is one-payload-per-source. Multi-drag will need to bundle the selected UUIDs into a single payload (e.g., a serialised list) and have the drop handler unpack — non-trivial wire-up.
7. **Naming collision detection** at Spritesheet Editor save time must check the *combined* Texture + SubSprite namespace (since `FindAssetByName` is flat). Easy to forget to check Textures.
8. **AnimationPanel keyframe spacing** — "evenly spaced" needs a defined interval. Decision: use the same `defaultStepSeconds` the panel already exposes (probably 0.1s); the drop position determines the first keyframe's time, subsequent frames step by `defaultStepSeconds`.

## Success criteria

The feature is "shipped" when **all of the following** are observably true:

1. Asset Browser shows an "Import Spritesheet" button in the toolbar.
2. Clicking it opens a file picker; selecting a PNG imports it as a texture AND opens the Spritesheet Editor in a floating window.
3. Drawing 3 rectangles in the editor, naming one of them `wing_up`, and clicking Save creates a `<filename>.png.sheet` sidecar with the three regions (one named `wing_up`, two auto-named).
4. After Save, the parent sheet's Asset Browser card shows an expand caret. Clicking the caret reveals 3 sub-sprite mini-cards in an inline grid, each displaying a thumbnail clipped to its UV rect + its name.
5. Right-clicking the sheet card → "Edit Slices..." reopens the editor with all 3 regions visible and editable. Resizing one + Save updates the sidecar; UUIDs preserved.
6. Dragging a sub-sprite mini-card onto an entity's Sprite component in the PropertyEditor assigns it; the entity renders only the sub-sprite's region.
7. Multi-selecting all 3 sub-sprite cards (Ctrl-click) and dragging onto the AnimationPanel timeline adds 3 keyframes at evenly spaced times in selection order.
8. Pressing Play with that animation cycles through the 3 sub-sprites; sprite batching emits **one draw call** for all three frames (verified via the FPS / draw-call HUD already in LevelEditor).
9. Renaming a sub-sprite to a name that collides with a regular texture is refused at Save with an inline error in the editor row.
10. Deleting the parent sheet PNG from disk + reopening the project causes any Sprite component referencing one of its sub-sprites to render as a pink/black checker, with `MISSING: <last-known-name>` in the PropertyEditor.
11. Smoke test passes (existing 29 + new sub-sprite scenarios — see below).

## Test extensions required

- **New smoke: `.sheet` sidecar round-trip.** Hand-write a JSON sidecar with 3 regions, parse it via `SheetSidecar::Read`, assert all 3 regions come back with matching UUIDs / names / rects. Hand-write 3 regions via `SheetSidecar::Write`, parse the result, assert identical.
- **New smoke: `AssetManager::ResolveSpriteSource` unified resolution.** Register a Texture under UUID T1, then `AddSubSprite(T1, rect, "wing_up")` returning UUID S1. Assert `ResolveSpriteSource(T1).uvRect == (0,0,1,1)`, `ResolveSpriteSource(T1).texture == T1's texture`. Assert `ResolveSpriteSource(S1).uvRect == (rect normalised)`, `ResolveSpriteSource(S1).texture == T1's texture`.
- **New smoke: `ResolveSpriteSource` missing.** Pass a UUID that's in neither map; assert `missing == true` and `texture == MissingTexture`.
- **New smoke: `FindAssetByName` flat namespace.** Register a Texture named `bird` and a SubSprite named `wing_up`. Assert `FindAssetByName("bird") == texture UUID`. Assert `FindAssetByName("wing_up") == sub-sprite UUID`. Assert `FindAssetByName("does_not_exist") == nil`.
- **No window-level test** of the Spritesheet Editor or drag-drop UX — manual verification per the success criteria.

---

## Decisions during implementation

<!-- Append-only. -->

## Spec amendments

<!-- Append-only. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Auto-grid slicing** — typed cols × rows or pixel-size-per-frame; mass-create regions.
- **Pivot points / 9-slice / trim** — per-sub-sprite metadata for richer placement.
- **Spritesheet preview in the AnimationPanel** — quick "preview this animation against the source sheet" affordance.
- **PNG replacement re-slice prompt** — when the parent texture's dimensions change, prompt to clamp / invalidate / re-slice.
- **Sub-sprite drag-reorder** in the Asset Browser mini-card grid.
- **Browser-side right-click Rename / Delete** on sub-sprite mini-cards (today: only in the editor).
- **Animation builder shortcut** — "select 3 sub-sprites → right-click → Create Animation" generates a `.hanim` + assigns it to the selected entity.
- **Atlas packer** — combine multiple textures into one packed PNG + auto-generate the `.sheet`.
- **Per-vertex UV optimisation profile** — measure batch shader cost; consider a sprite-shader uniform-array path if profiling demands.
- **Sub-sprite-of-sub-sprite (nested atlases)** — unlikely needed; logged for completeness.
- **Spritesheet inspector in PropertyEditor** — for a selected sub-sprite, show its parent + rect + named neighbours.
