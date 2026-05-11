# Feature spec: Box2D Physics Integration

> Tier: **Full spec**
> Status: **in progress**
> Started: 2026-05-11
> Spec author: Jaden

---

## Classification

- **Reversibility**: Sticky
- **Scope**: Cross-cutting
- **Tier rationale**: Changes the Rigidbody component (serialized data format), adds Python-facing API surface (velocity, apply_force), and replaces the collision pipeline — spans Physics, Scene, Components, Python bindings, PropertyEditor, and serialization.

---

## Problem

The current physics system is a custom AABB implementation with two static methods (`Physics::IsColliding` and `Physics::ResolveCollision`). It has no concept of velocity, gravity, forces, mass, friction, or restitution. Collision resolution is purely positional — objects get pushed apart with no momentum. This limits game authors to the most basic "do these boxes overlap" logic and prevents common 2D game patterns like platformer gravity, bouncing balls, or force-driven movement. Box2D 3.0.1 is already vendored but completely unused.

## In scope

- **Box2D world** owned by Scene, stepped each simulation frame
- **Body types**: static, dynamic, kinematic — set per entity via the Rigidbody component
- **Collider shapes**: box (sized to entity Transform) and circle
- **Rigidbody properties**: mass (density), friction, restitution (bounciness), gravity scale — editable in PropertyEditor
- **Gravity**: configurable per-scene (default downward)
- **Velocity**: readable from Python (`self.velocity`)
- **Forces**: `self.apply_force(x, y)` and `self.apply_impulse(x, y)` from Python
- **Collision events**: Box2D contact callbacks post `CollisionEvent` on the existing EventDispatcher, preserving the current `on_collision` / `self.colliding` Python API
- **Sync loop**: Box2D body positions/rotations written back to Transform each frame
- **Serialization**: updated binary format for the expanded Rigidbody component
- **PropertyEditor**: UI for body type dropdown, collider shape, density, friction, restitution, gravity scale

## Out of scope

- Joints and constraints (springs, hinges, distance joints)
- Polygon colliders (arbitrary vertex shapes)
- Collision layers / filtering / masks
- Continuous collision detection (CCD) tuning
- Debug physics wireframe rendering
- Multiple colliders per entity
- Trigger/sensor bodies (non-physical overlap detection)
- Scene-level gravity UI in the editor (set via code or defaults for now)

## API sketch

```python
# Python usage — platformer character
import Hamster

class Player(Hamster.HamsterBehaviour):
    def on_create(self):
        pass

    def on_update(self, dt):
        # Read current velocity
        vel = self.velocity

        # Move left/right with forces
        if self.key_pressed == Hamster.key_code.D:
            self.apply_force(500.0, 0.0)
        if self.key_pressed == Hamster.key_code.A:
            self.apply_force(-500.0, 0.0)

        # Jump with impulse (only if on ground)
        if self.key_pressed == Hamster.key_code.SPACE:
            self.apply_impulse(0.0, -200.0)
```

```cpp
// C++ — Rigidbody component (expanded)
struct Rigidbody {
  enum class BodyType { Static, Dynamic, Kinematic };
  enum class ColliderShape { Box, Circle };

  BodyType bodyType = BodyType::Static;
  ColliderShape colliderShape = ColliderShape::Box;
  float density = 1.0f;
  float friction = 0.3f;
  float restitution = 0.0f;
  float gravityScale = 1.0f;

  b2BodyId bodyId = b2_nullBodyId; // runtime only, not serialized
};
```

```cpp
// C++ — Scene owns the Box2D world
class Scene {
  b2WorldId m_PhysicsWorld = b2_nullWorldId;

  void InitPhysicsWorld();     // called on simulation start
  void DestroyPhysicsWorld();  // called on simulation stop
  void SyncPhysicsToTransforms(); // after world step
};
```

---

## Design

### Data structures

**`Rigidbody` component** (`Components.h`): replaces the current `bool isStatic` with the struct shown in the API sketch. `b2BodyId` is runtime-only state (not serialized) — created when simulation starts, destroyed when it stops.

**`b2WorldId`** on Scene: owned by the scene, created in `RunSceneSimulation()`, destroyed in `PauseSceneSimulation()` / scene destructor. Gravity is set at world creation (default `{0.0f, 10.0f}` — downward in screen coords where +Y is down).

**No new files needed** — Physics.h/.cpp get gutted and rewritten as a thin wrapper, or the Box2D calls move directly into Scene. The `Physics` class in its current form (two static AABB methods) is replaced entirely.

### Module touchpoints

- `Hamster-Core/src/Core/Components.h` — expanded Rigidbody struct
- `Hamster-Core/src/Core/Scene.h/.cpp` — Box2D world lifecycle, step, sync, contact callback
- `Hamster-Core/src/Physics/Physics.h/.cpp` — removed or gutted (Box2D replaces it)
- `Hamster-Core/src/Core/SceneSerialiser.cpp` — serialize/deserialize new Rigidbody fields
- `Hamster-Core/src/Scripting/HamsterBehaviour.h/.cpp` — add velocity getter, apply_force, apply_impulse
- `Hamster-Py/src/HamsterBehaviour.h` — expose velocity, apply_force, apply_impulse to Python
- `Hamster-Py/src/Components.h` — expose Rigidbody properties to Python bindings
- `Hamster-Wheel/src/Panels/PropertyEditor.cpp` — body type dropdown, collider shape, density/friction/restitution/gravity scale controls

### Lifecycle / control flow

1. **Simulation start** (`Scene::RunSceneSimulation`):
   - Create `b2WorldId` with gravity
   - For each entity with Rigidbody: create a `b2BodyId` and fixture (box or circle, sized from Transform). Store `bodyId` in the Rigidbody component.
   - Register contact callback that posts `CollisionEvent`

2. **Each simulation frame** (`Scene::OnUpdate`):
   - `b2World_Step(m_PhysicsWorld, deltaTime, subStepCount)` — Box2D 3.x uses substeps instead of velocity/position iterations
   - Process contact events from `b2World_GetContactEvents` → post `CollisionEvent` for each begin-contact pair
   - Sync: for each entity with Rigidbody, read `b2Body_GetPosition` and `b2Body_GetRotation` → write to Transform
   - Then run `OnScriptUpdate()` (scripts see updated positions and can apply forces)

3. **Simulation stop** (`Scene::PauseSceneSimulation`):
   - Destroy all bodies (or destroy the world, which destroys all bodies)
   - Reset `bodyId` fields to `b2_nullBodyId`

4. **Entity creation during simulation**: if simulation is running and a Rigidbody is added, create the body immediately. (Defer to future work if complex.)

### Edge cases

- **Entity with Rigidbody but no Transform**: skip — Box2D body needs position/size from Transform. The current ECS always creates Transform, so this shouldn't happen in practice.
- **Entity with Rigidbody removed during simulation**: need to destroy the `b2BodyId` when the component is removed. Use an EnTT `on_destroy` signal on the Rigidbody storage.
- **Zero-size entity**: Box2D rejects zero-extent shapes. Clamp collider dimensions to a minimum (e.g., 0.1f).
- **Coordinate system**: the renderer uses pixels with origin at screen center. Box2D works in meters. Need a pixels-to-meters scale factor (e.g., 50 pixels = 1 meter). All positions passed to Box2D are divided by this factor; all positions read back are multiplied.
- **Rotation**: Box2D uses radians. Transform.rotation is currently in degrees (used by the renderer's `glm::rotate`). Convert at the boundary.

---

## Full spec

### Sequence diagrams / data flow

```
Scene::RunSceneSimulation()
  ├─ Create b2WorldId (gravity = {0, 10})
  ├─ For each entity with <Transform, Rigidbody>:
  │     ├─ b2CreateBody (type from Rigidbody.bodyType)
  │     ├─ b2CreateShape (box or circle from Transform.size + Rigidbody.colliderShape)
  │     └─ Store b2BodyId in Rigidbody.bodyId
  └─ Register on_destroy<Rigidbody> signal → b2DestroyBody

Scene::OnUpdate() [each frame, if simulation running]
  ├─ b2World_Step(world, dt, 4)
  ├─ b2World_GetContactEvents → for each beginEvent:
  │     ├─ Resolve shape → body → entity UUID (via b2Body_GetUserData)
  │     └─ Post CollisionEvent(uuidA, uuidB)
  ├─ SyncPhysicsToTransforms():
  │     for each entity with <Transform, Rigidbody>:
  │       ├─ Transform.position = b2Body_GetPosition * PIXELS_PER_METER
  │       └─ Transform.rotation = b2Rot_GetAngle(b2Body_GetRotation) * RAD2DEG
  ├─ OnScriptUpdate() — Python scripts run, can call apply_force / apply_impulse
  └─ [OnPhysicsDetect/OnPhysicsResolve removed — Box2D handles both]

HamsterBehaviour::ApplyForce(fx, fy)
  └─ b2Body_ApplyForceToCenter(m_Rigidbody->bodyId, {fx/PPM, fy/PPM}, true)

HamsterBehaviour::ApplyImpulse(ix, iy)
  └─ b2Body_ApplyLinearImpulseToCenter(m_Rigidbody->bodyId, {ix/PPM, iy/PPM}, true)

HamsterBehaviour::GetVelocity()
  └─ b2Body_GetLinearVelocity(m_Rigidbody->bodyId) * PIXELS_PER_METER → vec2
```

### Error handling

- **No Rigidbody on entity**: `apply_force`, `apply_impulse`, and `velocity` check `m_Rigidbody` and `bodyId` for null. If null, log a warning to the client logger and return zero/no-op. No Python exception — beginner-friendly.
- **Box2D assertions**: Box2D 3.x uses `b2_nullBodyId` checks. Invalid IDs are guarded at the call site.
- **Simulation not running**: force/impulse calls when simulation is stopped are no-ops (body doesn't exist yet).

### Performance considerations

- Box2D 3.x is significantly faster than 2.x (restructured for cache efficiency, SIMD). For a beginner engine with <100 entities, performance is not a concern.
- The O(n²) brute-force loops in `OnPhysicsDetect` and `OnPhysicsResolve` are eliminated — Box2D handles broadphase internally.
- `SyncPhysicsToTransforms` is O(n) over entities with Rigidbody — negligible.
- Sub-step count of 4 is Box2D 3.x's recommended default.

### Migration / compatibility

- **Existing save files break.** The Rigidbody serialization format changes from `{bool isStatic}` to `{int bodyType, int colliderShape, float density, float friction, float restitution, float gravityScale}`. Since there are no production save files (this is a development-phase engine), no migration code is needed. Old `.hamproj` files with Rigidbody entities will fail to deserialize correctly — they need to be recreated.
- **Existing Python scripts**: `self.colliding` and `self.collision_entities` continue to work unchanged. No breaking change to the Python API — only additions (`self.velocity`, `self.apply_force`, `self.apply_impulse`).

---

## Why this approach

Box2D 3.0.1 is already vendored and linked. Using it replaces ~100 lines of custom AABB code with a production-grade physics engine that handles broadphase, collision resolution, forces, constraints, and continuous simulation. The alternative — incrementally adding velocity, gravity, and friction to the custom system — would eventually converge on reimplementing Box2D poorly.

The key design choice is **Scene owns the world, bodies are created at simulation start and destroyed at simulation stop.** This matches the existing pattern where simulation is a distinct mode (play/pause/stop in the editor). It avoids the complexity of keeping Box2D bodies in sync with the ECS during editing. The tradeoff: no physics preview in the editor, and entities added during simulation need special handling.

The pixels-to-meters conversion is necessary because Box2D is tuned for real-world scales (0.1m to 10m objects). Passing raw pixel values (e.g., 400px position) would make the simulation unstable. A fixed scale factor (50 px/m) keeps the conversion simple and predictable.

## Risks / what could go wrong

1. **Box2D 3.x API differences from 2.x tutorials**: Box2D 3.0 is a C API, not C++ classes. Most online tutorials cover 2.x (`b2World`, `b2Body*`). 3.x uses `b2WorldId`, `b2BodyId`, and free functions (`b2CreateBody`, `b2Body_GetPosition`). Need to work from the 3.x header files and samples, not tutorials.

2. **Coordinate system mismatch**: the renderer uses a coordinate system where the origin is at screen center, +Y is up (OpenGL convention), but Box2D's default gravity is `{0, -10}` (positive Y = up, gravity pulls down). If the renderer actually uses +Y = down (screen coordinates), gravity needs to be `{0, 10}`. Getting this wrong means objects fall the wrong way. Need to verify the renderer's convention before setting gravity.

3. **Rotation convention mismatch**: Transform.rotation is used in `glm::rotate` in the renderer. If it's degrees and Box2D returns radians, or if the rotation direction is flipped, entities will spin incorrectly.

4. **Pixels-to-meters scale factor affects feel**: a bad PPM value makes physics feel floaty or twitchy. 50 PPM means a 100px entity is 2 meters — reasonable. But force/impulse magnitudes in the Python API will feel non-intuitive if the user thinks in pixels. Documentation or sensible defaults matter.

5. **Serialization format break**: old .hamproj files with Rigidbody entities will fail to load. Since there are no production files, this is acceptable — but any test scenes used during development need to be recreated.

6. **Entity lifetime during simulation**: if an entity with a Rigidbody is deleted during simulation, the `b2BodyId` must be destroyed first. If the EnTT `on_destroy` signal fires after the component is already invalidated, or if the world is destroyed first, this crashes. The destruction order needs to be: destroy body → remove component → (later) destroy world.

## Success criteria

1. Create an entity with a Dynamic Rigidbody in the editor, press Play — it falls due to gravity
2. Create a Static Rigidbody entity below it — the dynamic entity lands on it and stops
3. A Python script calls `self.apply_force(500, 0)` on key press — the entity accelerates horizontally
4. A Python script calls `self.apply_impulse(0, -200)` — the entity jumps
5. `self.velocity` returns a vec2 reflecting the current Box2D velocity
6. `self.colliding` and `self.collision_entities` work as before — CollisionEvent fires on contact
7. PropertyEditor shows body type dropdown (Static/Dynamic/Kinematic), collider shape (Box/Circle), and float sliders for density, friction, restitution, gravity scale
8. Saving and reloading a scene preserves all Rigidbody properties
9. Smoke test passes (existing C++→Python boundary test still works)

## Test extensions required

- Extend the smoke test to create two entities with Rigidbody (one Dynamic, one Static), step the simulation for a few frames, and verify the dynamic entity's Y position has changed (gravity applied). This validates the Box2D world lifecycle and the physics→Transform sync loop.

---

## Decisions during implementation

<!-- Append-only log. -->

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Joints and constraints (hinges, springs, distance)
- Polygon colliders (arbitrary vertex shapes)
- Collision layers / filtering / masks
- Trigger/sensor bodies
- Debug wireframe rendering (show collider outlines)
- Scene-level gravity editor UI
- Multiple colliders per entity
- Continuous collision detection (CCD) tuning
- `set_velocity` for direct velocity control
