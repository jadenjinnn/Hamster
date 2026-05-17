# Feature spec: entity-hierarchy

> Tier: **Full spec**
> Status: **shipped**
> Started: 2026-05-16
> Approved: 2026-05-16
> Shipped: 2026-05-17
> Spec author: Jaden

---

## Classification

- **Reversibility**: Sticky
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: Adds a permanent Python API (`parent`/`children`/`set_parent`) and a scene-file format change, and threads across ECS + serialiser + Hierarchy panel + Python bindings + Scene's entity-destroy plumbing.

---

## Problem

Scenes currently treat all entities as a flat list. Game scripts and editor users have no way to group related entities (e.g. a player with sub-emitters, a UI root with widgets) so that they can be reasoned about, destroyed, or visually organised together. This blocks any project past trivial complexity: the hierarchy panel becomes an unsorted dump as entity count grows, and scripts that spawn multiple cooperating entities have to track them externally instead of letting the engine model the relationship.

## In scope

- Each entity has at most one parent (a `UUID`, or nil). Parents may have any number of children.
- Multi-level nesting: a child can itself be a parent.
- Cascading destroy: destroying an entity destroys its entire descendant subtree.
- Cycle prevention: any reparent operation (editor or script) that would create a cycle is refused with an error.
- Sibling order is preserved and reorderable (drag in the Hierarchy panel).
- Hierarchy panel renders the tree with collapse/expand on each parent; drag-and-drop reparents (drop *on* an entity → child) and reorders (drop *between* entities → sibling at that slot).
- Python API on `HamsterBehaviour`:
  - `self.parent` (read-only property returning parent's `UUID` or nil).
  - `self.children` (read-only property returning a list of child `UUID`s in sibling order).
  - `self.set_parent(uuid)` — re-parent self under given entity, or pass nil to detach to top level.
  - `create_entity(parent=uuid)` — optional kwarg on the existing runtime create API.
- Scene serialiser persists parent UUID + sibling index per entity so projects round-trip.

## Out of scope

- Transform inheritance — the parent/child relationship is purely organisational. Moving a parent does **not** move its children. Same for rotation, scale, visibility.
- Copying/duplicating a subtree as a unit. Copy is per-entity only (and the copy starts un-parented).
- Visual indication in the level-editor viewport (e.g. lines connecting parent to children, or selection lasso that follows the tree).
- Multi-selection drag of a subtree (no group-drag of parent+children in the hierarchy).
- Toggling a parent's visibility hiding its children's renders (no cascade for any property other than destroy).

## API sketch

```python
import Hamster

class Spawner(Hamster.HamsterBehaviour):
    def on_create(self):
        # Read hierarchy
        print("my parent:", self.parent)        # UUID or Hamster.UUID.nil()
        print("my children:", self.children)    # list[UUID]

        # Create an entity as a child of self
        bullet_uuid = self.create_entity(parent=self.uuid)

        # Re-parent self under another entity (looked up by UUID)
        self.set_parent(some_other_uuid)

        # Detach to top level
        self.set_parent(Hamster.UUID.nil())
```

```cpp
// C++ side (rough). Each entity gains a Hierarchy component (or fields
// in an existing component — TBD in Design section).
struct Hierarchy {
    UUID parent = UUID::GetNil();   // nil ⇒ top-level
    uint32_t siblingIndex = 0;      // order among siblings of the same parent
};

// Scene exposes helpers.
class Scene {
    bool SetParent(UUID child, UUID newParent);   // false on cycle
    const std::vector<UUID>& GetChildren(UUID parent);
};
```

---

## Design

### Data structures

- **`Hierarchy` component (new)** — `parent: UUID`, `siblingIndex: uint32_t`. Lives in `Hamster-Core/src/Core/Components.h`. Every entity created via `Scene::CreateEntity*` gets one defaulted (parent=nil, siblingIndex=appended to top-level order). Stored in the existing EnTT registry like Transform/Sprite.
- **`Scene::m_ChildrenIndex` (new)** — `std::unordered_map<UUID, std::vector<UUID>>` mapping each parent UUID to its children's UUIDs in sibling order. Top-level entities live under the key `UUID::GetNil()`. This is a derived view, kept in sync with the Hierarchy components by the same methods that mutate the relationship. Avoids an O(N) scan of the registry every time the Hierarchy panel renders.
- **No registry changes** beyond adding the new component. The flat `m_Entities` map stays.

### Module touchpoints

- `Hamster-Core/src/Core/Components.h` — declare `Hierarchy` POD.
- `Hamster-Core/src/Core/Scene.h/.cpp` — add `Hierarchy` to default entity creation; add `m_ChildrenIndex`; add `SetParent`, `GetParent`, `GetChildren`, `GetTopLevelEntities`; modify `DestroyEntity` to cascade.
- `Hamster-Core/src/Core/SceneSerialiser.cpp` — serialise the `Hierarchy` component (new component ID; parent UUID + sibling index). Bump on-disk versioning if a marker exists; otherwise add a tolerant read path (see Migration below).
- `Hamster-Py/src/HamsterBehaviour.h` — add `parent`/`children` properties and `set_parent` method on the trampoline.
- `Hamster-Py/src/EntityHandle.h` — extend `create_entity` to take an optional `parent` UUID; expose hierarchy queries here too if appropriate.
- `Hamster-Wheel/src/Panels/Hierarchy.cpp/.h` — rewrite the flat list render into a recursive tree render. Add per-row collapse triangle state (`unordered_set<UUID> m_Collapsed`). Add drag-source on every row, drop-target on every row (child-drop) + thin gaps between rows (sibling-drop). On drop, call `Scene::SetParent` / `Scene::ReorderSibling`. Search input behaviour: when a filter is active, fall back to flat list (same as today) so users can find a deep child quickly.
- `Hamster-Wheel/src/EditorLayer.cpp` — no expected changes; the panel already owns its own state.

### Lifecycle / control flow

- **Entity creation** — `Scene::CreateEntity*` now also adds `Hierarchy{parent=nil, siblingIndex=m_ChildrenIndex[nil].size()}` and appends the new UUID to `m_ChildrenIndex[nil]`.
- **Entity destroy** — `Scene::DestroyEntity(uuid)` builds the descendant list via DFS on `m_ChildrenIndex`, then destroys each in post-order (leaves first). Each destroy also removes the UUID from its parent's `m_ChildrenIndex` slot and erases its `m_ChildrenIndex` entry.
- **Reparent** — `Scene::SetParent(child, newParent)`:
  1. If `newParent == child`, return false (self-parent).
  2. Walk `newParent` up via `Hierarchy::parent` chain; if `child` appears, return false (cycle).
  3. Remove `child` from old parent's `m_ChildrenIndex` slot.
  4. Append `child` to `m_ChildrenIndex[newParent]`; update `Hierarchy::parent` and `Hierarchy::siblingIndex`.
  5. Renumber the affected slots' `siblingIndex` fields so they remain contiguous.
- **Reorder** — `Scene::ReorderSibling(uuid, newIndex)` slides `uuid` within its parent's children vector, then re-numbers `siblingIndex` for the affected siblings.
- **Hierarchy panel render** — per frame, iterate `m_ChildrenIndex[nil]` and recurse, drawing each entity as a row (icon, name, collapse triangle if it has children). Recursion stops if a node is in `m_Collapsed`. ImGui's drag/drop API handles the gesture; on drop, call into Scene.
- **Save/load** — `SceneSerialiser` writes the `Hierarchy` component per entity. On load, the deserialiser populates `m_ChildrenIndex` after all entities have been read (so parents that appear later in the file resolve correctly).

### Edge cases

- **Loading a scene file that pre-dates this feature** — no `Hierarchy_ID` records present; every entity defaults to `Hierarchy{nil, 0}`. After load, fix up `siblingIndex` by assigning insertion order. See Migration.
- **Destroying the active scene's *only* entity that has children** — cascading destroy must complete even though `m_ChildrenIndex[nil]` is mutated mid-iteration. Snapshot descendants before destroying.
- **`set_parent` from a script targeting a UUID that doesn't exist in this scene** — return false / raise `ValueError` from the Python side.
- **`set_parent` from a script during `on_create`** — fine, but the entity hasn't been registered with a behaviour callback yet; just ensure `Hierarchy` exists by the time `on_create` runs (the entity was created via `CreateEntity*`, so it does).
- **Cycle attempt during drag in the panel** — refuse the drop, optionally flash the offending target row red for one frame. Don't surface a popup.
- **Reordering a child within the same parent vs reparenting** — drag-drop must distinguish "drop on" (reparent under) from "drop between" (reorder among siblings). The panel uses two separate drop targets per row (the row body + a thin gap above).
- **The parent UUID stored in `Hierarchy` points to an entity that was destroyed without going through `Scene::DestroyEntity`** — shouldn't happen, but if a Python script ever does so, the child should self-orphan to top-level on next `SetParent` call. Don't crash.

---

## Full spec

### Sequence diagrams / data flow

**Create child from script**:

```
Python: self.create_entity(parent=p_uuid)
   └─ EntityHandle::create_entity (Hamster-Py)
        └─ Scene::CreateEntityWithUUID(new_uuid)         [returns the bare entity]
        └─ Scene::SetParent(new_uuid, p_uuid)            [validates, indexes]
```

**Destroy parent**:

```
Scene::DestroyEntity(p_uuid)
   ├─ Snapshot all descendants via DFS on m_ChildrenIndex
   ├─ For each in post-order (leaves first):
   │    └─ EnTT registry destroys entity + components
   │    └─ Remove UUID from m_ChildrenIndex[parent] and erase m_ChildrenIndex[self]
   └─ Remove p_uuid itself from m_ChildrenIndex[its parent]
```

**Drag-drop reparent in panel**:

```
User drags row A onto row B's body
   └─ ImGui drop target on B fires with payload = A.uuid
        └─ Scene::SetParent(A.uuid, B.uuid)
              ├─ Cycle check: walk B's parent chain for A → if found, return false (and panel flashes B red)
              └─ Else: detach A from old parent, append to B's children
```

### Error handling

- **C++ `Scene::SetParent`** returns `bool`. False on: self-parent, cycle, nonexistent UUIDs. No exceptions.
- **Python `set_parent`** raises `ValueError("would create a cycle")` or `ValueError("entity not found")`. Cycle / not-found both reported as ValueError with distinct messages.
- **Python `parent` / `children` properties** — never throw. If the entity is unknown (shouldn't happen for `self`), `parent` returns nil and `children` returns `[]`.
- **Hierarchy panel drag-drop** — silent refusal on cycle: drop is rejected, target row flashes red for ~250 ms, no console log spam.
- **Deserialise mismatch** — if a `Hierarchy` record references a parent UUID not in the scene, log a warning and reset the child to top-level. Don't fail the whole load.

### Performance considerations

- **Hot path**: the Hierarchy panel rerenders every frame. Tree render is `O(visible nodes)`; with `m_Collapsed`, collapsed branches are skipped. Budget for `N=200` entities in flat-list mode today is sub-millisecond; tree shouldn't regress that.
- **`m_ChildrenIndex` lookups** are `unordered_map<UUID, vector<UUID>>::find`. Per-frame: one lookup per visible node. Fine.
- **`SetParent` cycle check** is `O(depth)` from `newParent` to root. With `depth < 100` (realistic), negligible.
- **Memory** — `Hierarchy` is 16 bytes (UUID) + 4 bytes (siblingIndex) padded to 24 bytes. For 1000 entities that's 24 KB. `m_ChildrenIndex` adds a vector header (~24 B) per parent + 16 B per child entry. Trivial.
- **Cascading destroy** — `O(subtree size)`. Already linear in the number of entities affected; no surprise.

### Migration / compatibility

- **Existing scene files** lack `Hierarchy_ID` records. On load:
  - The deserialiser reads each entity. If a `Hierarchy_ID` record is present, use it. Otherwise, default to `Hierarchy{nil, siblingIndex = order-of-appearance}`.
  - After load, rebuild `m_ChildrenIndex` from the resulting components.
- This is **forward-compatible silently**: old scenes load as flat lists at the top level (matching current behaviour). New scenes saved by this version include `Hierarchy_ID` records.
- **Roll back risk**: scenes saved with this feature have the new component records. An older build that doesn't recognise `Hierarchy_ID` would skip those records (assuming the existing component-ID switch falls through to a "skip unknown" branch — verify this in `SceneSerialiser::DeserialiseComponent`). If the existing code crashes on unknown IDs, that's a Spec amendment to fix.
- **Python scripts** that use `create_entity()` without `parent=` keep working — the kwarg is optional.

---

## Why this approach

- **Why a separate `Hierarchy` component (not a field on Transform or ID)?** Keeps the relationship optional in concept and easy to skip in serialisation for entities that never need it. Also avoids growing every entity's `Transform` (which is touched on the hot render path) with fields the renderer doesn't care about.
- **Why a separate `m_ChildrenIndex` mirror, not a tree walk on render?** The Hierarchy panel renders every frame. Iterating the EnTT registry to find children-of-X is O(N) per call; with deep trees, that's O(N × visible_nodes) per frame. A reverse-index map collapses it to O(visible_nodes).
- **Why pure organisational (no transform inheritance)?** The author chose this explicitly to keep the feature small. Transform inheritance is the natural next step but doubles the implementation surface (matrix composition, dirty flags, sync ordering with physics) and is much harder to revert.
- **Why refuse cycles instead of silently fixing them?** A cycle is a programming bug or a UI misclick; either way the user wants to know rather than have entities silently teleport in the tree. Surfacing it loudly here keeps the data model truthful.
- **Why save sibling index per entity instead of letting `m_ChildrenIndex` be the canonical store on disk?** Per-entity records keep serialisation symmetric with how every other component is stored (one entity's record = one entity's full state). `m_ChildrenIndex` is rebuilt on load from the component data.
- **Why drag in the panel for both reparent and reorder?** Two gestures cover both use cases without a separate mode switch. ImGui's drag/drop API supports stacked drop targets (row body vs sibling gap) so the two are visually distinct.

## Risks / what could go wrong

- **Cascading destroy interacts with running Python behaviours**: if a script destroys a parent during its own `on_update`, sibling behaviours of the destroyed subtree may be mid-callback. We need to ensure `on_update` is iterating a snapshot of the behaviour set, not the live one. (See `Scene::OnScriptUpdate` — verify it copies before iterating.)
- **Serialiser format additions break the smoke test fixture** if the test loads a pre-feature scene file. Verify the smoke test's fixture path works with the new tolerant-load path, or regenerate the fixture.
- **Drag/drop in the panel mis-fires** when filter search is active — the recursive tree isn't being drawn, so dragging across the flat list might silently no-op or do the wrong thing. Decide: either disable reorder while filtering, or have reorder follow the underlying tree.
- **`m_ChildrenIndex` drift**: any code path that touches `Hierarchy::parent` without going through `Scene::SetParent` will corrupt the mirror. Risk = future refactor accidentally bypasses it. Mitigation: keep `Hierarchy` writes private to Scene (component is public-read, only Scene mutates it).
- **Cycle-check cost on bulk reparent from a script**: a script that reparents 1000 entities in `on_create` does 1000 cycle checks, each O(depth). For shallow trees (depth ≤ 10) that's 10k ops — fine. For pathological deep trees, could regress.
- **Python kwarg `parent=` and runtime-created entities**: the order of `Scene::CreateEntityWithUUID(new) → Scene::SetParent(new, p)` matters; if `on_create` for the new entity runs *between* those two steps, it observes itself as un-parented. Confirm where `on_create` is fired in the create flow.

## Success criteria

- [ ] Building a scene with three nested entities in the editor (grandparent → parent → child), saving, reopening, and seeing the same tree restored.
- [ ] Drag-and-drop reparents in the Hierarchy panel and reorders siblings; cycle-creating drops are refused with a red flash and no state change.
- [ ] A Python script that does `bullet = self.create_entity(parent=self.uuid)` produces a child of the script's entity, visible under the parent in the panel; `self.children` includes `bullet.uuid`.
- [ ] Destroying the grandparent in step 1 cascades: both descendants are gone from `Scene::GetEntityMap` after the destroy call.
- [ ] An existing pre-feature scene file from `test/fixtures/` loads with every entity at the top level (no crash, no extra/missing entities).
- [ ] Smoke test passes — `ctest --test-dir build -R SmokeTest -V`.

## Test extensions required

- Add a new smoke scenario `hierarchy_script.py` that:
  - Creates entity A.
  - Creates entity B with `parent=A.uuid`.
  - Asserts `A.children == [B.uuid]` and `B.parent == A.uuid`.
  - Calls `B.set_parent(Hamster.UUID.nil())`; asserts B is now top-level.
  - Destroys A; asserts `Scene::GetEntityCount()` decreased by exactly 1 (B already detached).
- Add a second scenario that re-tests destroy cascading: create A, B (child of A), C (child of B), destroy A, assert entity count drops by 3.
- Add a serialiser round-trip in C++ smoke: create scene with nested tree, serialise to a temp file, deserialise into a fresh scene, assert the tree matches.
- The existing flat-entity smoke scenarios (force_script, animation, etc.) should still pass without modification — they don't use parent and shouldn't regress.

---

## Decisions during implementation

### 2026-05-16 — Extended hierarchy methods to `EntityHandle`

The spec listed `parent`/`children`/`set_parent` only on `HamsterBehaviour` (`self.X`). Without the same methods on `EntityHandle` (returned by `create_entity`), a script couldn't query or reparent any entity it didn't own. Same semantic, different access path — straightforward extension that makes the test scenario expressible. Added the three methods to `EntityHandle` as well.

### 2026-05-16 — Deserialiser stays intolerant to unknown component IDs

`SceneSerialiser::DeserialiseEntity`'s `default:` arm throws `std::runtime_error`. New scenes saved by this build include `Hierarchy_ID` records — an older build reading those would throw. Forward compatibility (new build reading old files) is preserved because old files simply don't contain `Hierarchy_ID` records and the deserialiser falls through to load defaults. Backward compatibility (downgrade) is not. Not blocking since users don't typically downgrade.

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Transform inheritance (parent transform composes into children) — the natural next-step feature; would build on this hierarchy.
- Subtree copy/duplicate operation (one click copies an entity and all descendants).
- Viewport visual indication of parent/child relationship (line from parent centre to selected child).
- Multi-selection drag of a subtree as a unit.
- Cascading visibility / cascading enable-flag.
- Prefab-style: save an entity subtree as a reusable template.
