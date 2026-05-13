# Feature spec: Animation system

> Tier: **Full spec**
> Status: **approved**
> Started: 2026-05-12
> Spec author: Jaden (design), Claude (drafting)

---

## Classification

- **Reversibility**: Sticky — adds a new Python API (`self.animate()`), a new component type (Animation), and a new file format (`.hanim`), all of which become surfaces game authors depend on.
- **Scope**: Cross-cutting — touches ECS components, serialization, asset management, the editor (new animation panel), the renderer (sprite swapping), Python bindings, and the scene update loop.
- **Tier rationale (1 sentence)**: Sticky + cross-cutting = full spec per the classification matrix.

---

## Problem

Game entities currently have no way to display a sequence of sprites over time. A "Run" cycle, "Idle" bob, or "Death" sequence all require the game author to manually swap textures from Python each frame — tedious, error-prone, and missing basic timing control. An animation system is a baseline expectation for any 2D game engine.

## In scope

- **Animation data format** (`.hanim` file): a named sequence of sprite references with per-keyframe time positions, stored as a standalone asset file.
- **Animation component**: an ECS component on entities that holds a map of animation name → `.hanim` asset reference, plus runtime playback state (current animation, current time, looping flag).
- **Animation panel** in the editor: a timeline view where the user drags in sprites to create keyframes, adjusts their time positions, previews playback, and saves/loads `.hanim` files.
- **Scene serialization**: Animation component data (which animations are assigned, default animation, looping flag) serialized with the entity.
- **Asset management**: `.hanim` files loaded/tracked by `AssetManager`, visible in the asset browser.
- **Python API**: `self.animate("Run")` to play an animation, `self.stop_animation()` to stop, `self.is_animating` property.
- **Runtime playback**: per-entity animation state updated each frame during `Scene::OnUpdate`, swapping the entity's `Sprite::texture` based on the current keyframe.
- **Looping control**: per-animation default (set in component), overridable at runtime from Python.
- **AnimationCompleted event**: posted through `EventDispatcher` when a non-looping animation reaches the end, carrying the entity UUID and animation name. Scripts can subscribe via `self.subscribe(...)` or handle via an `on_animation_complete` callback.

## Out of scope

- Property animation (position, scale, rotation over time) — future work.
- Animator state machine / transition graph — future work.
- Animation blending or crossfade between animations.
- Spritesheet/atlas support (individual texture files per frame for now).
- Per-keyframe callbacks (e.g., "call this function on frame 5").
- Bone/skeletal animation.

## API sketch

```python
# Python usage — inside a HamsterBehaviour subclass
class Player(Hamster.HamsterBehaviour):
    def on_create(self):
        self.animate("Idle")  # play the "Idle" animation (loops by default)

    def on_update(self, dt):
        if self.key_pressed == Hamster.key_code.D:
            self.animate("Run")          # switch to Run animation
        elif self.key_pressed == Hamster.key_code.SPACE:
            self.animate("Jump", loop=False)  # play once, stop on last frame

        if self.is_animating:
            self.log("Currently animating")

        self.stop_animation()  # stop and hold current frame

    def on_animation_complete(self, animation_name):
        # called when a non-looping animation finishes
        if animation_name == "Jump":
            self.animate("Idle")
```

```cpp
// C++ — Animation component on an entity
struct Animation {
    // name → animation asset UUID
    std::unordered_map<std::string, UUID> animations;
    std::string defaultAnimation;  // plays on simulation start if set
    bool loop = true;              // default loop setting

    // Runtime state (not serialized)
    std::string currentAnimation;
    float currentTime = 0.0f;
    bool playing = false;
    bool runtimeLoop = true;
};
```

---

## Design (Design tier and Full spec only)

### Data structures

**`AnimationData`** (loaded from `.hanim` file, lives in `AssetManager`):
```cpp
struct AnimationKeyframe {
    float time;       // seconds from animation start
    UUID textureUUID; // references a texture in AssetManager
};

struct AnimationData {
    std::string name;
    std::vector<AnimationKeyframe> keyframes; // sorted by time
    float duration; // time of last keyframe
};
```

**`Animation` component** (on entity, in `Components.h`):
```cpp
struct Animation {
    // Serialized
    std::unordered_map<std::string, UUID> animations; // name → AnimationData UUID in AssetManager
    std::string defaultAnimation;
    bool loop = true;

    // Runtime only
    std::string currentAnimation;
    float currentTime = 0.0f;
    bool playing = false;
    bool runtimeLoop = true;
};
```

**`.hanim` file format**: binary, matching the project's existing serialization style (raw binary with `reinterpret_cast`, `size_t`-prefixed strings). Fields:
1. `name` (size-prefixed string)
2. `keyframeCount` (uint32_t)
3. For each keyframe: `time` (float), `textureUUID` (UUID)

New `ComponentID`: `Animation_ID = 8`.

**`AnimationCompletedEvent`** (in `SceneEvents.h`):
```cpp
class AnimationCompletedEvent : public Event {
public:
    AnimationCompletedEvent(UUID entityUUID, std::string animationName)
        : m_EntityUUID(entityUUID), m_AnimationName(std::move(animationName)) {}

    UUID GetEntityUUID() const { return m_EntityUUID; }
    const std::string& GetAnimationName() const { return m_AnimationName; }

    BIND_EVENT_TYPE(AnimationCompleted);
private:
    UUID m_EntityUUID;
    std::string m_AnimationName;
};
```
New `EventType`: `AnimationCompleted` added to the enum.

### Module touchpoints

- `Hamster-Core/src/Core/Components.h` — add `Animation` struct + `Animation_ID`
- `Hamster-Core/src/Core/Scene.cpp` — add animation advance logic inline in `Scene::OnUpdate`, between physics sync and script update; iterate entities with `Animation` + `Sprite`, advance time, swap texture
- `Hamster-Core/src/Core/SceneSerialiser.cpp` — serialize/deserialize `Animation` component
- `Hamster-Core/src/Utils/AssetManager.h/.cpp` — add `AnimationData` storage (`m_Animations` map), `AddAnimation`/`GetAnimation`/`RemoveAnimation` methods, `.hanim` file I/O, serialization
- `Hamster-Core/src/Events/Event.h` — add `AnimationCompleted` to `EventType` enum
- `Hamster-Core/src/Events/SceneEvents.h` — add `AnimationCompletedEvent` class
- `Hamster-Core/src/Scripting/HamsterBehaviour.h/.cpp` — add `Animate()`, `StopAnimation()`, `IsAnimating()` methods; subscribe to `AnimationCompleted` event, call `on_animation_complete` on the Python object
- `Hamster-Py/src/HamsterBehaviour.h` — bind `animate`, `stop_animation`, `is_animating`
- `Hamster-Wheel/src/Panels/` — new `AnimationPanel` class (timeline editor)
- `Hamster-Wheel/src/Panels/PropertyEditor.cpp` — add Animation component section (assigned animations list, default animation, loop toggle, add/remove animation slots)
- `Hamster-Wheel/src/Panels/AssetBrowser.cpp` — show `.hanim` files with appropriate icon
- `Hamster-Wheel/src/EditorLayer.cpp` — instantiate `AnimationPanel`, pass to panel list

### Lifecycle / control flow

**Editor (authoring):**
1. User opens the Animation panel.
2. User drags textures from the asset browser onto the timeline → creates keyframes at default spacing.
3. User drags keyframes left/right to adjust timing.
4. User clicks play in the Animation panel → preview playback (advances time, shows sprite in the panel preview area — does not affect the scene viewport unless simulation is running).
5. User saves → writes `.hanim` file to the project directory.

**Editor (assigning):**
1. User selects an entity, opens Property Editor → Animation component section.
2. User adds an animation slot: picks a `.hanim` file from asset browser, gives it a name (e.g., "Run").
3. User can set a default animation and loop toggle.

**Runtime (simulation):**
1. `Scene::RunSceneSimulation` — for each entity with an `Animation` component, if `defaultAnimation` is set, begin playing it.
2. `Scene::OnUpdate` → `AnimationUpdate(dt)`:
   - For each entity with `Animation` + `Sprite` where `playing == true`:
     - Advance `currentTime += dt`
     - If `currentTime >= duration`: if looping, wrap; else stop on last frame, set `playing = false`
     - Find the current keyframe (last keyframe with `time <= currentTime`)
     - Set `Sprite::texture` to that keyframe's texture (looked up from `AssetManager`)
     - If a non-looping animation just finished: post `AnimationCompletedEvent(entityUUID, animationName)` through `EventDispatcher`
3. `HamsterBehaviour` subscribes to `AnimationCompleted` in its constructor. When received, if the event's entity UUID matches, calls `obj.attr("on_animation_complete")(animationName)` on the Python object (if the method exists).
4. Python scripts call `self.animate("Run")` → sets `currentAnimation`, resets `currentTime`, sets `playing = true`, optionally sets `runtimeLoop`.
5. `self.stop_animation()` → sets `playing = false`.

**Teardown:**
- `Scene::StopSceneSimulation` resets `Animation` runtime state (currentTime, playing, currentAnimation) and restores original sprite texture.

### Edge cases

- `self.animate("Run")` called but entity has no Animation component → Python exception with clear message ("Entity has no Animation component").
- `self.animate("Foo")` called but "Foo" is not in the entity's animation map → Python exception ("Animation 'Foo' not found on this entity").
- `self.animate("Run")` called while "Run" is already playing → restart from the beginning (reset `currentTime` to 0).
- Animation with 0 keyframes → no-op, don't crash, log a warning.
- Animation with 1 keyframe → immediately display that sprite, nothing to animate.
- Keyframe references a texture UUID that isn't loaded → log warning, skip that keyframe (keep showing previous sprite).
- Entity destroyed mid-animation → no issue, EnTT handles component cleanup.
- `.hanim` file references textures not in the project → error on load, skip those keyframes, log warning.
- Animation panel preview while simulation is not running → panel manages its own timer independent of `Scene::OnUpdate`.

---

## Full spec (Full spec tier only)

### Sequence diagrams / data flow

**Runtime animation update (per frame):**
```
Scene::OnUpdate
  ├─ StepPhysics()
  ├─ ProcessContactEvents()
  ├─ SyncPhysicsToTransforms()
  ├─ [animation advance]           ← NEW (inline, not a named phase)
  │     for each entity with Animation + Sprite:
  │       if not playing: skip
  │       currentTime += dt
  │       if currentTime >= duration:
  │         if looping: currentTime = fmod(currentTime, duration)
  │         else: currentTime = duration; playing = false
  │       keyframe = last kf where kf.time <= currentTime
  │       sprite.texture = assetManager->GetTexture(keyframe.textureUUID)
  └─ OnScriptUpdate()
```

**Python `self.animate("Run")` call path:**
```
Python: self.animate("Run")
  → HamsterBehaviour::Animate("Run")
    → get entity's Animation component
    → look up "Run" in animations map → get AnimationData UUID
    → get AnimationData from AssetManager → get duration
    → set currentAnimation = "Run", currentTime = 0, playing = true
    → runtimeLoop = loop param (defaults to component's loop setting)
```

### Error handling

- **Python API errors**: `self.animate()` with missing component or unknown animation name → `pybind11::value_error` with descriptive message. Same pattern as other Python API methods that access components.
- **Asset loading errors**: `.hanim` file corrupt or referencing unknown textures → log warning via client logger, skip bad keyframes, continue with whatever is loadable. Don't crash.
- **Serialization errors**: missing or corrupt Animation component data in scene file → skip the component, log warning, entity loads without animation. Same pattern as existing component deserialization.

### Performance considerations

- `AnimationUpdate` iterates only entities with both `Animation` and `Sprite` components, and only those where `playing == true`. For a typical scene with <100 animated entities, this is negligible (<0.1ms per frame).
- Texture lookup per frame is a `std::unordered_map` lookup by UUID — O(1), already the same pattern used elsewhere.
- No per-frame allocations. `AnimationData` is loaded once and shared (via `AssetManager`). The per-entity `Animation` component holds only indices/references.
- Keyframe lookup is a linear scan of the keyframe vector. For typical sprite animations (2-20 frames), this is faster than binary search due to cache locality. If animations ever reach 100+ keyframes, binary search would be worth adding — but that's a property-animation concern (out of scope).

### Migration / compatibility

- **Existing scripts**: unaffected. No existing API changes. `animate` is additive.
- **Existing scene files**: unaffected. Entities without `Animation` components load as before. The deserializer skips unknown `ComponentID` values (though we add `Animation_ID = 8` to the enum).
- **Existing `.hanim` files**: none exist, so no migration needed.

---

## Why this approach

The simplest approach that gives game authors usable sprite animation: a standalone asset file (`.hanim`) for reusability across entities, a component for per-entity assignment and runtime state, and a direct `self.animate("name")` Python API matching the Unity legacy pattern. The time-based keyframe model (vs. fixed-fps) gives authors control over timing without requiring a separate playback-rate concept. The animation panel with timeline provides visual authoring rather than requiring manual file editing. Property animation and state machines are deliberately deferred — sprite-swap-only keeps the scope manageable and the API surface small, while still covering the vast majority of 2D game animation needs (walk cycles, idle bobs, attack sequences, death animations).

## Risks / what could go wrong

1. **Texture swapping during render**: `AnimationUpdate` runs after physics but before script update in `Scene::OnUpdate`, which runs *after* `ImGui::End()`. The sprite change won't be visible until the next frame's render pass. This is the same one-frame delay that already exists for script-driven transform changes — consistent behavior, not a bug, but worth documenting.

2. **Original sprite not restored on simulation stop**: when the user stops the simulation, animated entities need their sprite texture restored to whatever it was before `RunSceneSimulation`. If we don't snapshot the original texture, the entity is left showing whatever frame the animation was on when stopped. Fix: store original `Sprite::texture` in the `Animation` component's runtime state on simulation start, restore on stop.

3. **`.hanim` file format uses raw binary serialization**: inherits the existing portability concern (see architecture.md "Known smells"). If/when the project migrates to a portable format, `.hanim` files will need migration too. Accepted risk — consistency with current approach is more valuable than being the first portable format in the project.

4. **Animation panel complexity**: a timeline with draggable keyframes is the most complex editor panel built so far. Risk of scope creep and polish cycles. Mitigation: build the smallest working version first (list of keyframes with time inputs, no drag), then iterate toward drag-based timeline if time permits.

5. **AssetManager grows another asset type**: `AssetManager` now manages textures, scripts, and animations. The class is getting wider. Not a blocker, but if a fourth asset type comes along, it may be time to consider an `AssetRegistry` abstraction. Noted for future work, not this feature.

6. **Sprite component coupling**: the animation system directly mutates `Sprite::texture`. If the entity's sprite is also being set from Python (`self.transform` pattern doesn't apply here, but a future `self.sprite` API might), the two could fight. Mitigation: document that `animate()` takes ownership of sprite texture while playing; manual sprite changes while animating are undefined behavior.

## Success criteria

1. Create a `.hanim` file in the animation panel with 3+ keyframes at different time positions. Save it. Close and reopen the project. The `.hanim` file loads correctly with all keyframes intact.
2. Assign a `.hanim` animation to an entity via the Property Editor, naming it "Walk". Run the simulation. The entity's sprite visually cycles through the keyframes at the authored timing.
3. From a Python script, call `self.animate("Walk")`. The animation plays. Call `self.animate("Walk", loop=False)`. The animation plays once and stops on the last frame.
4. Call `self.stop_animation()`. The animation stops on the current frame.
5. `self.is_animating` returns `True` while playing, `False` when stopped.
6. Play a non-looping animation. When it finishes, `on_animation_complete` is called with the animation name.
7. Stop the simulation. The entity's sprite returns to its pre-animation texture.
8. The smoke test exercises animation playback programmatically (see below).

## Test extensions required

- Add a smoke test scenario that:
  1. Creates an `AnimationData` asset with 2 keyframes programmatically.
  2. Creates an entity with `Sprite` and `Animation` components.
  3. Assigns the animation as "Test" on the entity.
  4. Starts playback via `Animate("Test")`.
  5. Advances time past the first keyframe's time.
  6. Asserts that `Sprite::texture` changed to the second keyframe's texture.
  7. Advances time past the duration with `loop = false`.
  8. Asserts `playing == false`.

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building.
Each entry is dated. Updated by Claude during implementation, reviewed by author.

### YYYY-MM-DD — <decision title>
<one-paragraph what / why> -->

## Spec amendments

<!-- Append-only log of times the spec changed mid-implementation because
the original was wrong. Each entry includes what changed and why.

### YYYY-MM-DD — <amendment title>
<what was wrong, what's the new plan, who approved> -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Property animation**: animate position, scale, rotation, color over time (not just sprite swaps).
- **Animator state machine**: visual state graph with transitions, conditions, and parameters (Unity Animator Controller equivalent).
- **Animation blending/crossfade**: smooth transitions between animations.
- **Spritesheet/atlas support**: load frames from a single spritesheet image with UV coordinates instead of individual texture files.
- **Animation events**: trigger callbacks at specific keyframes (e.g., play sound on frame 5).
- **AssetRegistry abstraction**: if another asset type is added after this, consider extracting a generic registry pattern from AssetManager.
