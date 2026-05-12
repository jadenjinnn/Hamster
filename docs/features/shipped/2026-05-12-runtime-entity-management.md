# Feature spec: Runtime entity management from Python

> Tier: **Full spec**
> Status: **approved**
> Started: 2026-05-12
> Spec author: Jaden / Claude

---

## Classification

- **Reversibility**: Sticky — adds new Python API surface (`create_entity`, `destroy_entity`, `add_component`, constructable component objects) that scripts will depend on.
- **Scope**: Cross-cutting — touches Python bindings, Scene (creation/destruction + deferred queues), HamsterBehaviour (new methods), physics world (deferred body creation), and component structs (Python-constructable Sprite/Rigidbody).
- **Tier rationale (1 sentence)**: Sticky + cross-cutting = full spec.

---

## Problem

Python scripts cannot create or destroy entities at runtime. This blocks any game pattern that requires spawning objects dynamically — obstacles in a scrolling game, projectiles, pickups, enemies. Currently every entity must exist in the scene file before simulation starts, which limits what games can be built with Hamster.

## In scope

- `self.create_entity(name, transform)` — creates a new entity with a Name and Transform, returns its UUID
- `self.destroy_entity(uuid)` — marks an entity for destruction (by UUID, so any script can destroy any entity)
- `entity.add_component(component)` — adds a Sprite or Rigidbody to an entity handle returned by `create_entity`
- Python-constructable `Sprite` and `Rigidbody` component objects
- Deferred destruction at end of frame (after all `on_update` calls complete)
- Deferred physics body creation (new Rigidbody bodies created at start of next physics step)

## Out of scope

- Destroying entities from the editor during simulation
- Adding Behaviour (scripts) to runtime-spawned entities
- Prefab/template system for entity archetypes
- Entity parenting / hierarchy
- Serializing runtime-spawned entities to the scene file
- Removing individual components from entities

---

## API sketch

```python
import Hamster

class GameManager(Hamster.HamsterBehaviour):
    def on_create(self):
        self.spawn_timer = 0.0

    def on_update(self, delta_time):
        self.spawn_timer += delta_time

        if self.spawn_timer > 2.0:
            self.spawn_timer = 0.0

            # Create entity with name and transform
            t = Hamster.Transform(Hamster.vec3(400, 0, 0), 0.0, Hamster.vec2(50, 300))
            entity = self.create_entity("pipe", t)

            # Add components
            entity.add_component(Hamster.Sprite(Hamster.vec3(0.2, 0.8, 0.2)))
            entity.add_component(Hamster.Rigidbody(
                body_type=Hamster.BodyType.Kinematic
            ))

        # Destroy off-screen entities
        # (assumes some way to check position — e.g., stored UUIDs)
        for uuid in self.pipes_to_remove:
            self.destroy_entity(uuid)
```

```cpp
// C++ side — Scene additions
UUID Scene::CreateEntityRuntime(const std::string& name, const Transform& transform);
void Scene::QueueDestroyEntity(UUID uuid);
void Scene::FlushDestroyQueue();      // called at end of OnUpdate
void Scene::CreatePendingBodies();    // called at start of physics step
```

---

## Design (Design tier and Full spec only)

### Data structures

**`EntityHandle`** — a lightweight Python-side object that wraps a UUID and a Scene pointer. Exposes `add_component()`. Lives in `Hamster-Py/src/EntityHandle.h` (new file).

```cpp
struct EntityHandle {
    UUID uuid;
    std::shared_ptr<Scene> scene;
    
    void AddSprite(const Sprite& sprite);
    void AddRigidbody(const Rigidbody& rigidbody);
};
```

**Destroy queue** — `std::vector<UUID> m_DestroyQueue` on Scene. Populated by `QueueDestroyEntity`, flushed by `FlushDestroyQueue` at end of `Scene::OnUpdate`.

**Pending bodies list** — `std::vector<UUID> m_PendingBodies` on Scene. Entities whose Rigidbody was added at runtime get pushed here. `CreatePendingBodies` processes them at the start of the next physics step (before `ApplyPendingForces`).

### Module touchpoints

- `Hamster-Core/src/Core/Components.h` — add default constructor + kwargs-style constructor to `Rigidbody`, ensure `Sprite` constructors are compatible
- `Hamster-Core/src/Core/Scene.h/.cpp` — add `CreateEntityRuntime`, `QueueDestroyEntity`, `FlushDestroyQueue`, `CreatePendingBodies`, `m_DestroyQueue`, `m_PendingBodies`
- `Hamster-Core/src/Scripting/HamsterBehaviour.h/.cpp` — add `CreateEntity(name, transform)` returning `EntityHandle`, `DestroyEntity(UUID)`
- `Hamster-Py/src/EntityHandle.h` (new) — Python binding for EntityHandle with `add_component`
- `Hamster-Py/src/Components.h` — add `SpriteBinding`, `RigidbodyBinding` (Python-constructable), `BodyTypeBinding`, `ColliderShapeBinding`
- `Hamster-Py/src/main.cpp` — register new bindings

### Lifecycle / control flow

**Creation:**
1. Python calls `self.create_entity("pipe", transform)` → `HamsterBehaviour::CreateEntity`
2. `CreateEntity` calls `Scene::CreateEntityRuntime(name, transform)` which creates the entity in the ECS immediately (so `add_component` works in the same frame)
3. Returns an `EntityHandle` wrapping the new UUID + scene pointer
4. Python calls `entity.add_component(sprite)` → `EntityHandle::AddSprite` → `Scene::AddEntityComponent<Sprite>(uuid, sprite)`
5. If a Rigidbody is added, the UUID is pushed to `m_PendingBodies`

**Physics body creation (deferred):**
1. `Scene::OnUpdate` calls `CreatePendingBodies()` before `ApplyPendingForces()`
2. For each UUID in `m_PendingBodies`, create a Box2D body using the same logic as `InitPhysicsWorld` but for a single entity
3. Clear `m_PendingBodies`

**Destruction (deferred):**
1. Python calls `self.destroy_entity(uuid)` → `HamsterBehaviour::DestroyEntity`
2. `DestroyEntity` calls `Scene::QueueDestroyEntity(uuid)` which pushes to `m_DestroyQueue`
3. At end of `Scene::OnUpdate`, after `OnScriptUpdate()`, `FlushDestroyQueue()` runs:
   - For each UUID: destroy Box2D body if present, remove from entity map, destroy ECS entity
   - Clear `m_DestroyQueue`

**Frame order with new steps:**
```
Scene::OnUpdate:
  CreatePendingBodies()      ← NEW
  ApplyPendingForces()
  StepPhysics()
  ProcessContactEvents()
  SyncPhysicsToTransforms()
  CacheVelocities()
  OnScriptUpdate()
  FlushDestroyQueue()        ← NEW
```

### Edge cases

- **Destroy an entity that was created this frame**: works — entity exists in ECS, gets queued for destruction, removed at end of frame. No physics body was created yet so nothing to clean up there.
- **Destroy an entity twice in one frame**: `FlushDestroyQueue` should check entity still exists before destroying. Skip duplicates.
- **Destroy the entity running the current script**: the Python object holds a `Transform*` and `Rigidbody*` that become dangling. Since destruction is deferred to end-of-frame (after all `on_update` calls), the pointers remain valid during the frame. Next frame, the entity is gone and its behaviours won't be iterated.
- **Add Rigidbody to entity that already has one**: `emplace` will assert/throw — `AddRigidbody` should check `EntityHasComponent<Rigidbody>` first and log a warning if already present.
- **`add_component` called with wrong type**: Python type checking — `add_component` dispatches based on the Python type of the argument. Unrecognized types raise a Python TypeError.
- **Create entity while simulation is not running**: should be disallowed — `CreateEntity` only works during simulation. Log error and return nil UUID if called outside simulation.
- **Destroy self**: a script destroys its own entity. Deferred destruction means `on_update` finishes normally. Next frame the entity is gone.

---

## Full spec (Full spec tier only)

### Sequence diagrams / data flow

**Entity spawn + first physics frame:**
```
Frame N:
  OnScriptUpdate()
    Python: entity = self.create_entity("pipe", t)
      → Scene::CreateEntityRuntime → entity in ECS, UUID returned
    Python: entity.add_component(Hamster.Rigidbody(...))
      → Scene::AddEntityComponent<Rigidbody> + m_PendingBodies.push(uuid)

Frame N+1:
  CreatePendingBodies()
    → creates Box2D body for "pipe" entity
  ApplyPendingForces()
  StepPhysics()
    → "pipe" now participates in physics
  ...
  OnScriptUpdate()
    → other scripts can collide with "pipe"
```

**Entity destruction:**
```
Frame N:
  OnScriptUpdate()
    Python: self.destroy_entity(pipe_uuid)
      → Scene::QueueDestroyEntity(pipe_uuid)
    (other scripts still see pipe_uuid this frame)
  FlushDestroyQueue()
    → destroy Box2D body
    → m_Registry.destroy(entity)
    → m_Entities.erase(uuid)

Frame N+1:
  pipe_uuid no longer exists in any view
```

### Error handling

- `create_entity` outside simulation → logs error to client logger, returns `EntityHandle` with nil UUID (Python can check `entity.uuid` for nil)
- `destroy_entity` with invalid/already-destroyed UUID → silently skipped (idempotent)
- `add_component` with duplicate component → logs warning, ignores the call
- `add_component` with unrecognized type → raises Python `TypeError`
- `add_component` on an EntityHandle with nil UUID → logs error, no-op

### Performance considerations

- `CreateEntityRuntime` is O(1) ECS entity creation — negligible
- `CreatePendingBodies` runs once per frame, processes only new entities — bounded by spawn rate, not total entity count
- `FlushDestroyQueue` iterates only entities queued for destruction — O(k) where k is destroys per frame
- `DrawFlat` calls for each spawned entity's Sprite happen naturally through the existing render loop — no special handling needed
- Risk: spawning hundreds of entities per frame could create physics body creation spikes. For a classroom engine this is acceptable; a pooling system would be future work.

### Migration / compatibility

- No existing scripts break — new API is purely additive
- No save file changes — runtime entities are not serialized
- Existing scenes work unchanged — `m_PendingBodies` and `m_DestroyQueue` start empty

---

## Why this approach

**Entity creation is immediate, physics body creation is deferred.** The alternative would be to defer everything (entity + components) to the start of the next frame. But that would prevent calling `add_component` on the entity in the same `on_update` — you'd need a builder pattern or kwargs on `create_entity` for all component types. Immediate ECS creation + deferred physics keeps the API simple: create, configure, done.

**Destruction is deferred to end-of-frame.** The alternative is immediate destruction, but that risks invalidating iterators and dangling pointers mid-frame. End-of-frame destruction means scripts can safely reference any entity during their `on_update` — even if another script queued it for destruction earlier in the same frame.

**EntityHandle as a return type rather than raw UUID.** Returning a UUID would work but requires the user to know to call `scene.add_component(uuid, ...)` — which means exposing Scene to Python in a non-opaque way. EntityHandle keeps the API object-oriented and discoverable: `entity.add_component(...)`.

**`add_component` dispatches on Python type rather than separate `add_sprite`/`add_rigidbody` methods.** This is more Pythonic and extensible — adding a new component type later just requires adding a branch, not a new method.

## Risks / what could go wrong

1. **Dangling `Transform*` / `Rigidbody*` in HamsterBehaviour after destruction.** If a script holds a reference to a destroyed entity's behaviour object, accessing `self.transform` would dereference freed memory. Mitigation: deferred destruction ensures pointers are valid during the frame; next frame the behaviour is not iterated. Risk remains if a Python variable holds a reference across frames (e.g., `self.other = other_behaviour`) — this is a known limitation, documented but not guarded.

2. **EnTT pool reallocation invalidating raw pointers.** Creating new entities with `emplace` can reallocate component pools, invalidating existing `Transform*` and `Rigidbody*` pointers held by `HamsterBehaviour` instances. This is a pre-existing issue but spawning entities at runtime makes it much more likely to trigger. Mitigation: document as a known limitation for now; a proper fix would replace raw pointers with UUID-based lookups (future work).

3. **Physics body creation for destroyed entities.** If an entity is created with a Rigidbody in frame N and destroyed in frame N (before `CreatePendingBodies` runs in frame N+1), the pending body list will reference a dead UUID. Mitigation: `CreatePendingBodies` checks entity existence before creating the body.

4. **Sprite requires a texture pointer from AssetManager.** `Sprite(vec3 colour)` works for solid-colour sprites, but textured sprites need a `shared_ptr<Texture>` from `AssetManager`. The initial version only supports colour-based sprites; textured sprite creation would need `AssetManager` access from Python (future work).

5. **`add_component` type dispatch is a chain of `isinstance` checks.** If many component types are added in the future, this becomes unwieldy. Acceptable for the 2-3 component types in the initial version.

## Success criteria

1. A Python script can call `self.create_entity("test", transform)` during simulation and the entity appears in the rendered scene on the next frame
2. `entity.add_component(Hamster.Sprite(Hamster.vec3(1,0,0)))` makes the entity visible with the given colour
3. `entity.add_component(Hamster.Rigidbody(body_type=Hamster.BodyType.Dynamic))` causes the entity to fall under gravity on the next frame
4. `self.destroy_entity(uuid)` removes the entity — it no longer renders or participates in physics on the next frame
5. Destroying an entity mid-frame does not crash — other scripts' `on_update` calls complete normally
6. Smoke test exercises create + destroy path without exceptions

## Test extensions required

- Extend the smoke test with a scenario that:
  1. Creates an entity with a Transform via Python during `on_create`
  2. Adds a Rigidbody component to it
  3. Runs several frames and verifies the entity exists in the scene
  4. Destroys the entity and verifies it's gone from the scene
- This exercises the create → add_component → physics body creation → destroy pipeline end-to-end

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building. -->

## Spec amendments

<!-- Append-only log of times the spec changed mid-implementation. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Textured sprite creation (requires AssetManager access from Python)
- Adding Behaviour (scripts) to runtime-spawned entities
- Entity prefab/template system
- Removing individual components from entities
- Entity pooling for high-frequency spawn/destroy patterns
- Replace raw `Transform*`/`Rigidbody*` pointers with UUID-based lookups to avoid dangling pointers
