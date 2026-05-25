# Feature spec: Game UI (v1 — buttons and text)

> Tier: **Full spec**
> Status: **approved**
> Started: 2026-05-18
> Spec author: Jaden

---

## Classification

- **Reversibility**: Sticky
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: New components serialized into scene files, a new Python-facing event, and a brand-new text-rendering subsystem with a built-in font asset — all touching renderer + components + scripting + editor; once scenes contain UI elements, reverting costs more than building it.

---

## Problem

Game scripts have no way to put screen-space UI in front of the player. Everything currently rendered is world-space sprites that scroll with the camera and can be occluded — there is no text output channel at all (engine logs are editor-only, not visible to a built game). Without UI, even the simplest game flow — a start button, a score counter, a "you died" message — isn't expressible in the engine. This feature unlocks that.

## In scope

- `UIButton` component: anchored screen-space clickable rectangle with a solid background colour and a text label.
- `UIText` component: anchored screen-space text element (no background, no click target).
- Anchoring: 9 anchor points (top-left, top-centre, top-right, middle-left, centre, middle-right, bottom-left, bottom-centre, bottom-right) plus pixel offset from the anchor; elements stay anchored when the window resizes.
- Per-element styling: background colour (button only), text colour, font size, text alignment within the button rect (horizontal left/centre/right).
- Button sizing: explicit `width`/`height`, plus an `auto_size` toggle that grows the button to fit its text + padding.
- UIText sizing: an optional `wrap_width` — when set, text wraps within that width; when unset, text flows on a single line.
- One built-in font baked into the engine (TTF compiled in or copied to Resources), loaded once at engine init.
- Dynamic text content — game scripts can update `ui_text.text = "Score: 42"` and the change is reflected next frame.
- Click handling: when a `UIButton` is clicked in play mode, the engine posts a `ButtonClickedEvent` carrying the button entity's UUID. Subscribers (Python behaviours) react via the existing `EventDispatcher` pattern.
- Edit mode: UI elements render in the scene viewport, are selectable like any other entity, and clicks select-for-edit (do **not** fire `ButtonClickedEvent`).
- Play mode: UI elements render, button clicks fire `ButtonClickedEvent`, entity is **not** selectable (consistent with the simulation-snapshot lock on the property editor).
- Editor authoring: the existing "Add Entity" pill in the Hierarchy panel is replaced with an "Add" dropdown exposing **Entity**, **UI Button**, **UI Text** (creates an entity with sensible defaults and the matching component pre-attached).
- PropertyEditor renders editors for `UIButton` and `UIText` components alongside Transform/Sprite/etc.
- Python access to UI entities by their `Name` component value via a new `scene.find_entity_by_name(name) -> EntityHandle | None` binding.
- Scene serialisation: UI components persist through save/load and through the simulation-snapshot round-trip.

## Out of scope

- Hover and pressed visual states on buttons.
- Sliders, checkboxes, dropdowns, text input fields, radio buttons, progress bars.
- Layout containers (vertical stack, horizontal stack, grid, flex).
- Image / sprite backgrounds on buttons — solid colour only for v1.
- Multiple fonts or loading custom fonts from project assets.
- Animations / tweens / transitions on UI elements.
- Sound effects on click.
- Touch or gamepad input — mouse only for v1.
- Right-click / middle-click handling on buttons.
- Multi-line text alignment beyond simple wrap (no justified text, no per-line alignment).
- UI elements nested inside other UI elements (parent/child layout). The existing entity-hierarchy parent relationship still works as a plain organisational link, but layout does not propagate.
- Localisation / text direction support beyond LTR ASCII.
- Vertical text alignment within a button (always vertically centred for v1).
- Drag-entity-reference-into-script-field (deliberately punted — see Future work).

## API sketch

```python
# script attached to a UI manager entity
import Hamster

class MainMenuManager(Hamster.HamsterBehaviour):
    def on_create(self):
        self.start_btn = self.scene.find_entity_by_name("StartButton")
        self.score_text = self.scene.find_entity_by_name("ScoreText")
        self.score = 0
        self.subscribe(Hamster.EventType.ButtonClicked, self.on_button)

    def on_button(self, evt):
        if evt.entity_id == self.start_btn.id:
            self.log("Game starting!")
            self.score = 0

    def on_update(self, dt):
        self.score += 1
        self.score_text.set_text(f"Score: {self.score}")
```

```cpp
// scene update — added near the existing sprite render pass in Scene::OnRender
m_Renderer->BeginUIPass(m_WindowWidth, m_WindowHeight);
for (auto [e, ui_btn] : m_Registry.view<UIButton>().each()) {
    m_Renderer->SubmitUIRect(ui_btn.ResolvedRect(...), ui_btn.bgColour);
    m_Renderer->SubmitUIText(ui_btn.label, ...);
}
for (auto [e, ui_txt] : m_Registry.view<UIText>().each()) {
    m_Renderer->SubmitUIText(ui_txt.text, ...);
}
m_Renderer->EndUIPass();
```

---

## Design

### Data structures

New components in `Hamster-Core/src/Core/Components.h`:

```cpp
enum class UIAnchor : uint8_t {
    TopLeft, TopCentre, TopRight,
    MiddleLeft, Centre, MiddleRight,
    BottomLeft, BottomCentre, BottomRight,
};

enum class UITextAlign : uint8_t { Left, Centre, Right };

struct UIButton {
    UIAnchor anchor = UIAnchor::TopLeft;
    glm::vec2 offset{10.0f, 10.0f};       // pixels from the anchor point
    glm::vec2 size{200.0f, 50.0f};        // pixels; ignored when autoSize == true
    bool autoSize = false;
    float padding = 8.0f;                 // used when autoSize == true
    glm::vec4 bgColour{0.2f, 0.4f, 0.8f, 1.0f};
    std::string label = "Button";
    glm::vec4 textColour{1.0f, 1.0f, 1.0f, 1.0f};
    float fontSize = 18.0f;
    UITextAlign textAlign = UITextAlign::Centre;
};

struct UIText {
    UIAnchor anchor = UIAnchor::TopLeft;
    glm::vec2 offset{10.0f, 10.0f};
    std::string text = "Text";
    glm::vec4 textColour{1.0f, 1.0f, 1.0f, 1.0f};
    float fontSize = 18.0f;
    float wrapWidth = 0.0f;               // 0 == no wrap; >0 wraps within this width
};
```

New event in `Hamster-Core/src/Events/`:

```cpp
class ButtonClickedEvent : public Event {
public:
    explicit ButtonClickedEvent(UUID entityId) : m_EntityId(entityId) {}
    EventType GetEventType() const override { return EventType::ButtonClicked; }
    UUID GetEntityId() const { return m_EntityId; }
private:
    UUID m_EntityId;
};
```

Plus `EventType::ButtonClicked` added to the enum.

Font asset: a TTF file committed under `Hamster-Wheel/Resources/Fonts/` (likely re-using Inter-Regular.ttf already shipped for the editor) is copied into `build/<target>/Resources/Fonts/` at build time. The engine loads it once into a `FontAtlas` instance owned by `Renderer`.

Text rendering: bring in **stb_truetype** (single header, BSD/Public-domain) — vendored under `Hamster-Core/Vendor/stb/` alongside the existing `stb_image`. At engine init, the renderer bakes the font at a chosen "atlas size" into a single GL texture using `stbtt_BakeFontBitmap` (covering ASCII 32–126 for v1). Per-glyph quads are submitted into the existing sprite batch or a parallel `UIQuad` batch — TBD in implementation.

### Module touchpoints

- `Hamster-Core/src/Core/Components.h` — add `UIAnchor`, `UITextAlign`, `UIButton`, `UIText` structs.
- `Hamster-Core/src/Core/SceneSerialiser.cpp` — serialise / deserialise the two new components.
- `Hamster-Core/src/Core/Scene.cpp` — render UI after the sprite pass (z-order: always on top); add `FindEntityByName(name)` lookup helper; rebuild the spatial index so UI entities are **not** included (their hit-testing is separate, screen-space).
- `Hamster-Core/src/Renderer/Renderer.cpp` — new `BeginUIPass` / `EndUIPass` (screen-space ortho, depth test off); new `SubmitUIRect(rect, colour)`; new `SubmitUIText(string, position, size, colour, wrap_width)`. New shaders: `UIRectShader`, `UITextShader` (text uses the font-atlas texture sampled with alpha).
- `Hamster-Core/src/Renderer/FontAtlas.h/.cpp` — new class wrapping the stb_truetype-baked GL texture + glyph metrics; owns `MeasureString(text, fontSize) -> vec2` for auto-sizing buttons and wrap layout.
- `Hamster-Core/src/Events/EventType.h` and a new `UIEvents.h` for `ButtonClickedEvent`.
- `Hamster-Wheel/src/EditorLayer.cpp` — split click handling: in play mode, hit-test UI in screen-space first (resolve anchor + offset against current viewport size) and post `ButtonClickedEvent` on hit; in edit mode, hit-test UI in screen-space first and select the entity, falling back to world-space spatial index for non-UI entities.
- `Hamster-Wheel/src/Panels/Hierarchy.cpp` — replace the "Add Entity" pill with an "Add" dropdown (uses existing `HBeginStyledPopup` machinery from the styled-menu work).
- `Hamster-Wheel/src/Panels/PropertyEditor.cpp` — render `UIButton` and `UIText` sections with the existing `HDragFloat` / `HCombo` / `HCheckbox` helpers.
- `Hamster-Py/src/Core.h` — bind `Scene::FindEntityByName` to Python.
- `Hamster-Py/src/Components.h` — bind `UIButton` and `UIText` so scripts can mutate text/colour at runtime via `EntityHandle.get_component(UIButton).label = "..."` (or similar — final pythonic API TBD in implementation).
- `Hamster-Py/src/main.cpp` — register `EventType::ButtonClicked` in the `EventType` Python enum.
- `Hamster-Core/CMakeLists.txt` — pick up `FontAtlas.cpp/.h`; vendor `stb_truetype.h` next to `stb_image.h`.
- `Hamster-Wheel/CMakeLists.txt` (or Core's CMake post-build) — make sure the font TTF is copied into the build output and into project Resources if needed.

### Lifecycle / control flow

- **Engine init**: `Renderer` constructor loads `Inter-Regular.ttf`, bakes the atlas at one canonical pixel height (e.g. 32 px) into a GL texture. Glyphs are sampled per-vertex with a scaled UV — runtime `fontSize` adjusts the quad size, not the bake. (Trade-off — see Risks.)
- **Per frame**:
  1. `Scene::OnRender` runs the world-sprite pass (existing).
  2. `Scene::OnRender` then runs the UI pass: `Renderer::BeginUIPass(viewportW, viewportH)` sets a screen-space ortho `(0, w, h, 0)` and disables depth test. Iterates `view<UIButton>` then `view<UIText>` (buttons render under text so a button's own label draws on top) and submits a batched quad + text per element. `EndUIPass` flushes and restores state.
  3. Click events: `EditorLayer::OnUpdate` checks if a mouse-click landed inside any UI element's resolved rect. In play mode → posts `ButtonClickedEvent`. In edit mode → selects the UI entity (does **not** post the event).
- **Anchor resolution** (`UIButton::ResolvedRect(viewportW, viewportH, fontAtlas)`):
  - `anchor` maps to a base screen point.
  - `offset` is added (interpretation: `(+x, +y)` always moves toward the centre of the screen — i.e., for a top-right anchor, `+x` moves left, `+y` moves down; this gives consistent "10 px inset from your anchor corner" semantics regardless of which corner).
  - `size` is used directly if `!autoSize`; else `size = fontAtlas.MeasureString(label, fontSize) + 2 * padding`.
- **Save/load**: serialise the structs flat (same pattern as Sprite/Rigidbody) — strings as size-prefixed bytes.
- **Simulation snapshot**: `Scene::m_PlaySnapshot` already captures component state via `SceneSerialiser`; adding the two new components to the serialiser inherits snapshot/restore for free.
- **No teardown specifics**: components live in EnTT; FontAtlas owned by Renderer is freed at process exit by the existing Renderer destructor (which currently doesn't free anything — pre-existing concern outside this feature).

### Edge cases

- **Empty label / empty text**: button still renders the background; UIText renders nothing but the entity is still selectable in edit mode (size-zero bounding box from `MeasureString("")`).
- **Glyph not in atlas** (e.g. non-ASCII character): fall back to a placeholder glyph (the atlas's '?' or a blank quad). Log once per unknown codepoint to the client logger to make the limitation visible to the game author.
- **Click in edit mode on a UI entity overlapping a world sprite**: UI wins (screen-space hit-test runs first). Documented behaviour, not a bug.
- **Window minimised / zero viewport**: skip the UI pass entirely (no-op).
- **`auto_size` with empty label**: button collapses to `2 * padding` so it remains visible and click-targetable.
- **`wrap_width > 0` but smaller than the widest glyph**: greedy break; one glyph per line. No special handling.
- **Snapshot restore mid-click**: the click was on a *pre-restore* entity. The `ButtonClickedEvent` carries the UUID, which after restore points to a re-instantiated entity — this is identical to how collision events work today, so no special handling.
- **`find_entity_by_name` with duplicate names**: returns the first match in EnTT iteration order. Document as undefined-but-deterministic.
- **Renaming a UI entity while a script holds a cached `EntityHandle` from a previous `find_entity_by_name` call**: the handle keeps working (it's UUID-backed). Subsequent calls with the old name return None.

---

## Full spec

### Sequence diagrams / data flow

**Click → event flow during play:**

```
GLFW mouse-down callback
  └─ Window posts MouseClickedEvent (existing)
       └─ EditorLayer::OnMouseClicked
             ├─ if play mode:
             │     ├─ resolve mouse to panel-relative (panelX, panelY)
             │     ├─ for each entity with UIButton:
             │     │     rect = button.ResolvedRect(viewportW, viewportH, fontAtlas)
             │     │     if rect.Contains(panelX, panelY): post ButtonClickedEvent(uuid) and break
             │     └─ if no UI hit: fall through to world-space pick (existing spatial-index path)
             └─ if edit mode:
                   ├─ resolve mouse to panel-relative (panelX, panelY)
                   ├─ for each entity with UIButton or UIText:
                   │     rect = element.ResolvedRect(viewportW, viewportH, fontAtlas)
                   │     if rect.Contains(panelX, panelY): SelectEntity(uuid) and break
                   └─ if no UI hit: fall through to world-space pick (existing)
```

**Render flow:**

```
Scene::OnRender
  ├─ Renderer::BeginSpriteBatch (existing)
  ├─ ... world sprites ...
  ├─ Renderer::EndSpriteBatch (existing)
  ├─ Renderer::BeginUIPass(viewportW, viewportH)
  ├─ for each UIButton:
  │     rect = button.ResolvedRect(viewportW, viewportH, fontAtlas)
  │     Renderer::SubmitUIRect(rect, button.bgColour)
  │     textRect = AlignText(button.label, rect, button.textAlign, fontAtlas)
  │     Renderer::SubmitUIText(button.label, textRect, button.textColour, button.fontSize)
  ├─ for each UIText:
  │     pos = text.ResolvedPosition(viewportW, viewportH)
  │     Renderer::SubmitUIText(text.text, pos, text.textColour, text.fontSize, text.wrapWidth)
  └─ Renderer::EndUIPass    // flushes batch, restores depth + ortho
```

### Error handling

- **Missing font asset at startup**: Renderer logs `[ERROR] failed to load font Inter-Regular.ttf, UI text will not render` to the client logger and proceeds with a null atlas. `SubmitUIText` becomes a no-op (button rects still draw). The engine does not crash.
- **`find_entity_by_name` not found**: returns Python `None`. Python script's job to null-check. No exception.
- **stb_truetype bake failure** (corrupted TTF, too-small atlas buffer): same as missing font — log + null atlas + degrade.
- **Anchor enum out of range** when deserialising: clamp to `UIAnchor::TopLeft` and log a warning naming the entity.

### Performance considerations

- **Per-frame UI cost**: at v1 scale (< 100 UI elements per scene is the realistic target), the UI pass adds ≤ 1 ms in debug. Buttons batch into one draw call (one rect shader, no texture). Text batches per font atlas — one draw call total (one shared atlas).
- **`MeasureString` cost**: O(glyphs) per call. Called per frame per `auto_size` button and per wrapped UIText. With < 100 UI elements and short labels, negligible.
- **Hot-path budget**: per-frame UI work must stay under 1 ms at 100 elements in debug. If profiling shows otherwise, fall back to caching `ResolvedRect` until a component dirty flag is set.
- **Memory**: font atlas one-time cost ~512 KB GL texture (256×256 R8 — TBD by chosen bake size); per-element cost is just the component struct (~80 bytes for UIButton, ~60 bytes for UIText).
- **No new per-frame heap allocations** in the steady state (vertex buffers pre-allocated; text laid out into a reusable scratch vector).

### Migration / compatibility

- **Existing scene files**: UI components are additive — old scenes load unchanged (no UIButton/UIText to deserialise). The two new component-tag bytes in the serialiser must be appended to the existing tag enum, not inserted, so old scenes' tag ordering stays valid.
- **Python scripts**: `find_entity_by_name` is additive. Existing scripts unchanged.
- **Editor**: the Hierarchy "Add Entity" → "Add" dropdown change is purely visual; clicking "Entity" in the dropdown does the same thing the old pill did.
- **Smoke test**: gains a new scenario, no existing scenarios break.

---

## Why this approach

The biggest forks during design were:

1. **UI as entities with components vs. UI as a separate hierarchy/system.** Picked entities-with-components because (a) the author chose it explicitly as "sounds easier" — meaning lower cognitive load for game authors, and (b) it lets us reuse the entity hierarchy, the property editor, scene serialisation, and the simulation-snapshot machinery for free instead of duplicating them in a parallel UI tree. The tradeoff is that anchoring and screen-space layout live in a *separate* coordinate system from world Transform, so `UIButton` does not use the `Transform` component at all — it holds its own anchor + offset. This is a small surprise for users coming from world-space-entity intuition, but documenting the convention is cheaper than building a parallel hierarchy.

2. **Click handling shape: poll vs callback vs event.** Picked event because the engine already has `CollisionEvent` and `AnimationCompletedEvent` and the script-side pattern (`self.subscribe(...)`) is established. A new mechanism would be one more thing to teach.

3. **Edit-mode click behaviour.** Picked "selects-for-edit, does not fire event" so the editor stays usable while a scene contains buttons. The alternative — "buttons are always live" — would let the author test interactions without entering play mode but would make placement / property-editing actively painful (every click moves the simulation forward).

4. **Drag-entity-reference-into-script-field vs. `find_entity_by_name`.** The author asked for Unity-style drag-references but, after seeing the cost (typed Python fields, drop-target rendering in PropertyEditor, scene persistence of `field_name → UUID`, runtime resolution), explicitly chose the cheaper name-lookup. We accept the looseness (string-typed lookup, can break if names are renamed, undefined under duplicates) in exchange for a much smaller scope. Drag-references are logged in Future work.

5. **Text rendering: stb_truetype vs. ImGui's atlas vs. bitmap fonts.** Picked stb_truetype because (a) we already vendor stb_image and the codebase has the import pattern, (b) it's single-header and BSD/public-domain, (c) it produces a real font atlas that supports dynamic strings and scales (versus pre-baked bitmaps), and (d) reusing ImGui's atlas would couple game rendering to the editor layer, which contradicts the goal of supporting a future standalone player.

6. **One bake size, scaled at runtime vs. one bake per fontSize.** Picked one bake size + runtime quad scaling because v1's font-size variation is small, the simplification is significant (one atlas, one texture, fixed-size MeasureString), and any quality loss at extreme sizes can be addressed later with multi-bake or SDF without breaking the API.

7. **Spec tier: Full spec vs. Design.** Picked Full spec because the feature is sticky on multiple axes (scene-file format, Python API, new dependency, font asset shipped in builds) and any one of those getting wrong on first cut is expensive to walk back.

## Risks / what could go wrong

- **stb_truetype bake + GL texture upload at engine init blocks startup**. Currently no other engine system loads a heavy asset at process start; this could push first-frame latency past where it is now. *Mitigation:* keep the atlas tiny (256×256 ASCII only); measure init time before/after and fail the spec amendment if it adds > 50 ms.
- **The "anchor + offset, offset always moves toward centre" convention will trip people up.** Top-left anchor `(10, 10)` means "10 right, 10 down"; top-right anchor `(10, 10)` means "10 left, 10 down" — handy for "10 px inset" but confusing if you expected `+x` to always mean right. *Mitigation:* document explicitly in the spec, the PropertyEditor labels, and the spec's "Decisions" if we change minds during implementation.
- **Click hit-testing UI before the world-space spatial index means a transparent button covers world entities below it**. This is the intended behaviour but worth flagging — game authors may not expect a placed-but-forgotten UI element to swallow clicks.
- **Text not in the baked atlas range (ASCII 32–126) silently degrades to placeholder glyphs**. Non-English game authors will hit this immediately. *Mitigation:* logged once per codepoint as documented; documented out-of-scope to avoid silent expectations.
- **The auto-sized button with a runtime-mutated label re-measures every frame**, contributing per-frame text-layout cost. At v1's element counts this is fine; at 1000+ buttons it would matter. *Mitigation:* documented as performance bound; cache invalidation can be added later if profiling shows it.
- **Sceneless / null-pointer paths** — UI rendering must no-op cleanly when there's no active scene (e.g. at the project hub). The render path will key off `m_ActiveScene` like the sprite pass does. Risk is that the new helper in EditorLayer needs the same guard or the editor crashes when clicking the viewport with no project loaded.
- **Snapshot restore + a still-pressed mouse button**: if the user clicks a button that stops simulation and the restore wipes the entity, the next mouse-up routes into edit-mode pick. This already works for collisions and should work here, but is a place where simulation-snapshot's `m_PendingRestore` defer matters — verify in implementation.
- **`Hierarchy` "Add" dropdown** has to integrate with the existing styled-popup helpers (`HBeginStyledPopup`) without breaking the surrounding pill-button styling — easy to make the popup look out-of-place if not careful.

## Success criteria

1. The smoke test loads a new fixture `ui_script.py` that creates two UI entities programmatically (one `UIButton` named "TestBtn", one `UIText` named "Counter"), runs ≥ 60 frames, and the test passes if (a) the renderer reports ≥ 2 UI draw calls per frame (one rect, one text — text-only frames are also fine for the UIText), (b) `find_entity_by_name("TestBtn")` returns a non-None handle, (c) injecting a synthesised click at the button's resolved screen rect produces exactly one `ButtonClickedEvent` for that UUID, (d) script-driven text mutation (`counter.text = "1"` etc.) reflects in the next frame's draw command (verified by a string-presence assertion in a renderer debug hook, not pixel sampling).
2. **Manual scenario A — anchor behaviour**: open a fresh project, add a UI Button via the Hierarchy "Add" dropdown, set its anchor to `BottomRight`, offset `(20, 20)`. Resize the editor window from default → small → maximised. The button stays 20 px from the bottom-right corner in all three states with no jitter or lag.
3. **Manual scenario B — play/edit click distinction**: place a button at the centre of the viewport, press Play, click the button — `ButtonClickedEvent` fires (verify via a one-line script that logs on subscribe). Press Stop. Click the same button — PropertyEditor opens to the button's properties, no event fires.
4. **Manual scenario C — auto-size + dynamic text**: place a UIButton with `autoSize = true` and label "Hi". Press Play, run a script that mutates the label to "Hello, world!" mid-frame. Button visibly grows to fit. On Stop, the simulation-snapshot restore reverts the label and the button shrinks back to its pre-play size.
5. **Manual scenario D — UIText wrap**: place a UIText with `wrapWidth = 100`, content "the quick brown fox jumps over the lazy dog". Text breaks into multiple lines, each ≤ 100 px wide.
6. No regression in the existing 21/21 smoke scenarios.
7. Editor exits cleanly after each scenario (no new crashes on shutdown beyond pre-existing Bug 0008).

## Test extensions required

- New smoke fixture `test/fixtures/ui_script.py` exercising programmatic UI creation, name lookup, click event subscription, and dynamic text mutation.
- New C++ smoke scenarios in `test/smoke_test.cpp`:
  - **UI-1 (creation + serialise)**: programmatically add a `UIButton` and a `UIText` to a scene, serialise → deserialise → verify component fields round-trip exactly.
  - **UI-2 (find_entity_by_name)**: create two entities with distinct `Name`s, verify `find_entity_by_name` returns each correctly, and verify it returns null for a missing name.
  - **UI-3 (anchor resolution)**: instantiate a `UIButton` with each of the 9 anchors at a known viewport size, verify the resolved screen-space rect is at the expected pixel coordinates within a 1-px tolerance.
  - **UI-4 (synthesised click → ButtonClickedEvent)**: subscribe a counter callback to `ButtonClickedEvent`, synthesise a click within a button's resolved rect via a test-only `Application::InjectClick(panelX, panelY)` hook, verify exactly one event fires.
  - **UI-5 (auto-size measurement)**: instantiate a `UIButton` with `autoSize = true` and a known label, verify resolved width = `MeasureString(label, fontSize).x + 2 * padding` ± 1 px.
- Smoke total target: 26/26 (current 21 + 5 new).

---

## Decisions during implementation

<!-- Append-only log. -->

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Drag-entity-reference-into-script-field** — Unity-style: declare `self.target = ExposedField(Entity)` in a script, drag an entity from the Hierarchy into the matching slot in PropertyEditor, get a typed runtime handle. Removes the string-typed fragility of `find_entity_by_name`. Likely its own Design-tier feature.
- **Hover and pressed visual states** — interpolated colour or sprite swap on `Hover`, `Pressed`, `Disabled`. Once we have the click hit-test in place this is mostly a styling addition.
- **More UI primitives** — slider, checkbox, dropdown, text input. Each is small individually; bundle into a "UI controls v2" spec.
- **Layout containers** — vstack / hstack / grid. Sticky and design-heavy; needs its own spec.
- **Image / sprite backgrounds on buttons** — generalise `bgColour` into an optional texture.
- **Custom fonts from project assets** — depends on AssetManager learning to recognise `.ttf` as an asset type with its own sidecar.
- **Multi-line vertical alignment within buttons** — top / middle / bottom for buttons with wrapped labels.
- **Touch / gamepad input on UI** — once the engine grows touch and gamepad input layers in general.
- **Standalone player** — a runtime-only binary (no editor) that loads a project and renders UI; the screen-space ortho UI pass is a pre-requisite and now exists after this feature.
