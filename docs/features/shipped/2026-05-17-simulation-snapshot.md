# Feature spec: simulation-snapshot

> Tier: **Design**
> Status: **approved**
> Started: 2026-05-17
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Cross-cutting (Scene + SceneSerialiser + PropertyEditor)
- **Tier rationale (1 sentence)**: In-process behavior change with no external API impact, but the snapshot/restore lifecycle has real edge cases (project-close mid-play, play without a scene, restore failure) that deserve being written down rather than discovered during `/implement`.

---

## Problem

Today, anything that happens during simulation persists into the editable scene state: physics moves rigidbodies, scripts mutate components, runtime `create_entity` calls add entities to the registry. When the user clicks stop the next save writes all of it to disk. This is the Unity "play mode is destructive" trap and it actively blocks workflows that spawn many entities at runtime (e.g. the upcoming sprite-batching benchmark, which spawns thousands per play). Author wants Unity-style snapshot-on-play, restore-on-stop semantics so play mode is non-destructive.

## In scope

- Snapshot the entire scene to an in-memory blob on simulation start.
- Restore from the blob on simulation stop, before the next frame begins.
- Lock the PropertyEditor while simulation is running (edits during play are visible but discarded on stop — matches Unity).
- Release the snapshot on project close so it doesn't leak between projects.

## Out of scope

- Undo / redo of any kind (snapshot is play-only, not a general history).
- Per-property "this one stays, this one reverts" granularity.
- Saving a runtime snapshot to disk as a new scene file ("save play state").
- Snapshotting asset-registry state (textures, scripts, animations). Asset state is intentionally not reverted — new files on disk shouldn't disappear when stop is pressed. Per author: the file watcher is believed to be effectively dormant during play, so asset registry shouldn't mutate anyway. Flagged as a risk to verify.
- Multiple scene support (only one scene exists at a time; scene-switching mid-play isn't possible).

## API sketch

No Python-facing API change. Internal C++ flow:

```cpp
// In Scene::RunSceneSimulation, before any setup:
m_PlaySnapshot.clear();
SceneSerialiser::SerialiseToBuffer(*this, m_PlaySnapshot);

// ... existing simulation setup (physics world build, script reload, etc.)

// In Scene::PauseSceneSimulation, after teardown:
// ... existing teardown (DestroyPhysicsWorld, etc.)
SceneSerialiser::DeserialiseFromBuffer(*this, m_PlaySnapshot);
m_PlaySnapshot.clear();
```

PropertyEditor gains an `m_SceneRunning` flag (fed via existing event), wraps its widget tree in `ImGui::BeginDisabled(m_SceneRunning)` / `EndDisabled()`.

---

## Design

### Data structures

- `Scene::m_PlaySnapshot` — `std::vector<uint8_t>`. Empty when simulation is not running. Owned by Scene; freed in destructor + on project close + on simulation stop after successful restore.
- `SceneSerialiser` gains two new entry points:
  - `static void SerialiseToBuffer(Scene&, std::vector<uint8_t>& out);`
  - `static bool DeserialiseFromBuffer(Scene&, const std::vector<uint8_t>& in);`
  Existing `Serialise(path)` / `Deserialise(path)` keep using file I/O; the new entry points share the per-component serialization code through an internal stream abstraction.

### Module touchpoints

- `Hamster-Core/src/Core/Scene.{h,cpp}` — add `m_PlaySnapshot`, call serialize on `RunSceneSimulation` start, call deserialize on `PauseSceneSimulation` end. Add cleanup in destructor.
- `Hamster-Core/src/Core/SceneSerialiser.{h,cpp}` — factor existing file I/O into stream-based core, add buffer overloads.
- `Hamster-Wheel/src/Panels/PropertyEditor.{h,cpp}` — subscribe to existing scene-running events (or read `Scene::IsRunning()`), wrap widget tree in `BeginDisabled`/`EndDisabled`.
- `Hamster-Core/src/Core/Project.cpp` — on project close, if scene is running, stop simulation first (which triggers restore). If snapshot exists when project closes for any reason, clear it.

### Lifecycle / control flow

```
User clicks Play
  └─ Scene::RunSceneSimulation
       ├─ SerialiseToBuffer(*this, m_PlaySnapshot)   <-- NEW
       ├─ Clear Behaviour::pyObjects (existing)
       ├─ InitPhysicsWorld (existing)
       ├─ ReloadScripts + instantiate + on_create (existing)
       └─ m_IsRunning = true

Per frame while running:
  └─ Scene::OnUpdate    (physics step, scripts, animations — existing)

User clicks Stop
  └─ Scene::PauseSceneSimulation
       ├─ DestroyPhysicsWorld (existing)
       ├─ Clear Behaviour::pyObjects (existing)
       ├─ DeserialiseFromBuffer(*this, m_PlaySnapshot)   <-- NEW
       ├─ m_PlaySnapshot.clear()
       └─ m_IsRunning = false
```

Next render frame draws the restored scene state. The render loop already runs *after* `Scene::OnUpdate`, so there's no one-frame window where the restored state is invisible.

### Edge cases

- **Play without a scene loaded.** `RunSceneSimulation` should already early-return; snapshot path is gated on having a registry to serialize. Confirm + add guard if missing.
- **Project closed while simulation running.** Order of operations matters: must stop simulation (which restores state) *before* writing the scene to disk. Currently project close presumably calls Scene::OnSaveScene or similar — Project.cpp must call `PauseSceneSimulation` first.
- **Serialize failure** (e.g. an entity holds a component the serializer doesn't yet support). The snapshot is empty / corrupt → restore on stop will partially fail. Decision: log + abort play with an error toast. Do not enter play mode if snapshot fails. Better to refuse than to enter a state we can't get out of.
- **Restore failure on stop.** The user is stuck — the scene is mid-simulation state on disk. Decision: log loudly, keep the snapshot blob alive so re-trying the restore is possible, and don't allow the user to save until restore succeeds or they explicitly discard.
- **Play → stop → play in rapid succession.** Each play makes a fresh snapshot. Old snapshot is overwritten when the new one is taken; nothing leaks. Verify the existing `m_IsRunning` flag prevents nested play calls.
- **Snapshot size in pathological scenes.** For very large scenes (10k+ entities), the blob could be tens of MB. Currently no concern; flag if it becomes one.
- **`Behaviour::pyObjects`** are intentionally not snapshotted — they're recreated by the next `ReloadScripts + instantiate` cycle on the next play. The snapshot captures the *component* state (script UUIDs in the Behaviour map, cached names), which is enough.

---

## Why this approach

Considered three options for the snapshot mechanism:

1. **In-memory `SceneSerialiser` round-trip** (chosen). Reuses every component's existing serialize/deserialize code. Smaller diff. Risk inherited: any bug in SceneSerialiser shows up in snapshot/restore as well as in disk save/load. But that's a forcing function to keep SceneSerialiser correct, not a new failure mode.
2. **EnTT snapshot API** (`entt::snapshot` / `entt::snapshot_loader`). Cleaner conceptually — copies registry state directly without going through a binary buffer. But requires registering every component type with the snapshot loader, parallel to SceneSerialiser. Two places to maintain on every component addition. Rejected because the duplication will rot.
3. **Manual per-component copy in Scene**. Most flexible but most code. Rejected — there's no reason to invent a third path when (1) reuses existing code.

The "lock PropertyEditor during play" decision matches Unity's UX. Alternative was "allow edits, revert on stop" — but the user's mental model is clearer if the panel literally refuses input than if their edits silently disappear.

## Risks / what could go wrong

1. **SceneSerialiser round-trip completeness.** The serializer must capture every field of every component used in a scene (Transform, Sprite, Name, Rigidbody, ID, Animation, Behaviour, parent/child relationships, etc.). If any field is omitted from the on-disk format today, it's also omitted from the snapshot, and that field silently resets to default on stop. Mitigation: explicit smoke-test extension that creates an entity with one of each component type, plays, mutates, stops, asserts equality.
2. **Box2D world lifecycle interaction.** `RunSceneSimulation` builds the Box2D world from Rigidbody components; `PauseSceneSimulation` destroys it. After restore, the registry is back to pre-play Rigidbody state. Next play call rebuilds the world from scratch — should be fine, but worth verifying that nothing in the registry holds a stale `b2BodyId` after restore.
3. **File watcher mid-play assumption.** Author asserts the watcher doesn't actually mutate AssetManager state during play. If wrong, a script that references a runtime-loaded asset after stop will hit a missing-asset error. Verify during implementation: read `ProjectWatcher::Run` / `HandleFileEvents` flow and confirm whether dispatch is gated on `!m_IsRunning` anywhere. If not, decide: gate the dispatch, or accept the asset-registry-during-play behavior as documented.
4. **PropertyEditor lock breaks inspection workflows.** Some users want to *read* component values during play (e.g. watching a transform.position update from physics). `BeginDisabled` greys out the widgets but they're still readable. Acceptable. Flag if it turns out to be annoying.
5. **Restore failure leaves scene in undefined state.** See edge case above. Mitigation: keep snapshot blob until next successful save or explicit user discard. Add a "Restore failed, retry?" modal as fallback.
6. **Snapshot taken before script-side validation runs.** `RunSceneSimulation` currently refuses to start if any Behaviour holds a missing-script reference (asset-sidecars feature). The snapshot must happen *after* that check — otherwise a refused play would still leave a snapshot in memory. Order matters in the new code.

## Success criteria

Observable, measurable signals — all must hold:

1. **Smoke test: runtime spawn is reverted.** Add a smoke scenario that: loads a one-entity scene → starts simulation → Python script calls `create_entity()` adding 5 entities → 3 frames tick → stops simulation → asserts the registry has exactly 1 entity (the original).
2. **Smoke test: component mutation is reverted.** Same scene → starts → Python script sets `self.transform.position = (999, 999)` on the existing entity → 3 frames tick → stops → asserts `transform.position` matches the pre-play value.
3. **Manual test: physics is reverted.** Add a Rigidbody to an entity, position it at (100, 100), play, watch it fall under gravity to (100, 500), stop. Entity is back at (100, 100).
4. **Manual test: PropertyEditor is locked during play.** All input widgets greyed out while playing; live values still visible.
5. **Manual test: project close mid-play does not save runtime state.** Spawn 100 entities via script during play, close the project window (X button or File→Close), reopen the project. Scene file has the pre-play entity count.

## Test extensions required

- New smoke scenario `simulation_snapshot_scenario` in the existing smoke test driver. Steps as in success criteria 1 and 2 above. Failure modes: registry count mismatch, transform mismatch, exception during snapshot/restore.
- Smoke test must still pass all existing 15 scenarios.

No new test framework or test target needed — the smoke test already covers C++→Python→C++ round-trips and can host this scenario directly.

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building. -->

### 2026-05-17 — SceneSerialiser already stream-based; no buffer overloads needed

The spec's Design section assumed `SceneSerialiser` needed to be factored to expose `SerialiseToBuffer` / `DeserialiseFromBuffer` static methods. Reading the existing code, `Serialise(std::ostream&)` / `Deserialise(std::istream&)` are already the public interface — the file I/O happens in callers, not in the serialiser. So in-memory round-trip is achieved by passing `std::stringstream` directly to the existing methods. No new static methods added; no refactor required. Saves a meaningful chunk of work.

### 2026-05-17 — File→Save during play is blocked

Spec specified PropertyEditor lock during play but didn't address `EditorLayer.cpp:354`'s File→Save shortcut. Three options surfaced (block / allow-writes-runtime-state / save-writes-snapshot). Chose block: symmetry with PropertyEditor — during play, neither in-memory state nor disk state can be mutated by the user. Allowing the menu save would write runtime state to disk, defeating the "non-destructive play" guarantee. Implementation: disable the menu item via `BeginDisabled` keyed on the same `!IsSceneSimulationPaused()` signal.

### 2026-05-17 — Post ActiveSceneChangedEvent after restore to invalidate panel pointer caches

First manual test crashed the editor on stop. Root cause: `PropertyEditor` (and likely other panels) caches raw component pointers — `Name*`, `Transform*`, `Sprite*` — across frames via `SetSelectedEntity`. The snapshot restore path does `m_Registry.clear()` followed by `Deserialise`, which destroys every component and rebuilds at new addresses. The next render in the *same frame* dereferences the now-dangling pointers. Fix: post `ActiveSceneChangedEvent` synchronously immediately after a successful restore — each panel's existing handler nulls its cached pointers and selection, so subsequent rendering finds a clean slate. Semantically a slight stretch (the scene shared_ptr didn't change), but the panels' response is exactly what's needed and avoids adding a new event type for one consumer. Side effect: stop clears the user's selection — acceptable.

## Spec amendments

<!-- Append-only log of times the spec changed mid-implementation. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Asset-registry snapshot** — only relevant if file watcher dispatch during play turns out to mutate state. Currently parked.
- **"Save play state as new scene"** — useful for capturing emergent simulation states. Not now.
- **SceneSerialiser portability refactor** — known smell, separate effort. Snapshot will silently benefit if/when it lands.
