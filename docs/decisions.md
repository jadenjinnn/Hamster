# Decisions

Non-obvious technical decisions made during development, preserved for future reference.

## Collider serialization uses a separate ComponentID (2026-05-12)

Collider offset/size data is stored on the `Rigidbody` struct but serialized as its own `Collider_ID = 7` block rather than appended to the `Rigidbody_ID` block. The binary serializer reads a fixed number of fields per component ID — appending fields would cause old scene files (which have fewer Rigidbody fields) to corrupt the parse stream by reading the next component ID as float data. A separate `Collider_ID` block is cleanly skipped by old deserializers and defaults to (0,0) when absent.

## Box2D shape-local offset for colliders (2026-05-12)

Collider offset is applied as a shape-local offset (`b2MakeOffsetBox` center parameter, `b2Circle::center`) rather than shifting the body position. This means `SyncPhysicsToTransforms` doesn't need to account for the offset at all — the body center remains the entity center, and only the collision shape is displaced. Avoids the rotation-dependent offset math that would be needed if the body position were shifted.
