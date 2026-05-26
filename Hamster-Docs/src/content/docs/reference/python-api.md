---
title: Python API Reference
description: HamsterBehaviour, EntityHandle, and Scene — every method and property exposed to Python.
---

Everything Python-facing lives in the `Hamster` module. This page covers the behaviour base class and entity handles; data types (components, enums, math) are on the [Components & Types](/Hamster/reference/components-and-types) page.

## HamsterBehaviour

The base class for all scripts. Subclass it, attach the script to an entity in the editor, and the engine instantiates one object per attached entity.

```python
import Hamster

class MyScript(Hamster.HamsterBehaviour):
    def on_update(self, delta_time):
        ...
```

### Lifecycle methods

You define these; the engine calls them. All are optional except that a useful script defines at least `on_update`.

| Method | Description |
|---|---|
| `on_create(self)` | Called once when the simulation starts. |
| `on_update(self, delta_time)` | Called every frame. `delta_time` is seconds since the previous frame. |
| `on_animation_complete(self, animation_name)` | Called when a non-looping animation finishes. |
| `on_button_clicked(self, uuid)` | Called when a `UIButton` is clicked; `uuid` is the button entity. |

### Properties

| Property | Type | Access | Description |
|---|---|---|---|
| `transform` | `Transform` | read / write | The entity's position, rotation, and size. |
| `velocity` | `vec2` | read | Current rigidbody velocity. |
| `key_pressed` | `key_code` | read | Key pressed this frame, or `key_code.key_not_pressed`. |
| `key_released` | `key_code` | read | Key released this frame. |
| `colliding` | `bool` | read | Whether the entity is touching anything this frame. |
| `collision_entities` | `list[UUID]` | read | Entities currently in contact. |
| `is_animating` | `bool` | read | Whether an animation is playing. |
| `uuid` | `UUID` | read | This entity's id. |
| `parent` | `UUID` | read | Parent entity id. |
| `children` | `list[UUID]` | read | Child entity ids. |

### Methods

**Physics** (require a `Rigidbody` component)

| Method | Description |
|---|---|
| `apply_force(fx, fy)` | Apply a continuous force this frame. |
| `apply_impulse(ix, iy)` | Apply an instantaneous impulse. |
| `set_velocity(vx, vy)` | Set velocity directly. |

**Entities**

| Method | Returns | Description |
|---|---|---|
| `find_entity_by_name(name)` | `EntityHandle` or `None` | Look up another entity by name. |
| `create_entity(name, transform, parent=None)` | `EntityHandle` | Spawn an entity at runtime. `parent` is an optional `UUID`. |
| `destroy_entity(uuid)` | — | Destroy an entity (and its children). |
| `set_parent(uuid)` | — | Reparent this entity. |

**Collisions**

| Method | Description |
|---|---|
| `reset_collision_entities()` | Clear `collision_entities`. |

**Animation**

| Method | Description |
|---|---|
| `animate(name, loop=None)` | Play an animation by name; `loop` defaults to the animation's own setting. |
| `stop_animation()` | Stop the current animation. |

**Logging**

| Method | Description |
|---|---|
| `log(msg, type=Hamster.log_type.Info)` | Write to the editor Console. `type` is a `log_type` (`Info`, `Warning`, `Error`). `msg` is stringified. |

## EntityHandle

Returned by `find_entity_by_name` and `create_entity`. A reference to another entity that you can read and mutate.

| Member | Type | Description |
|---|---|---|
| `uuid` | `UUID` | The entity's id (read-only). |
| `transform` | `Transform` | Get / set the entity's transform. |
| `add_component(component)` | — | Add a `Sprite` or `Rigidbody` instance. |
| `set_texture(name)` | — | Assign a texture by name (entity needs a `Sprite`). |
| `set_label(text)` | — | Set a `UIButton`'s label text. |
| `set_text(text)` | — | Set a `UIText`'s text content. |
| `set_velocity(vx, vy)` | — | Set velocity (entity needs a `Rigidbody`). |
| `apply_impulse(ix, iy)` | — | Apply an impulse (entity needs a `Rigidbody`). |
| `parent` | `UUID` | Parent id (read-only). |
| `children` | `list[UUID]` | Child ids (read-only). |
| `set_parent(uuid)` | — | Reparent this entity. |

Methods that require a component (`set_velocity`, `set_label`, etc.) raise `ValueError` if the entity lacks it — so a script fails loudly in the Console rather than silently doing nothing.

```python
enemy = self.find_entity_by_name("Enemy")
if enemy is not None:
    enemy.set_velocity(-200.0, 0.0)
    t = enemy.transform
    t.size = Hamster.vec2(64, 64)
    enemy.transform = t
```

## Scene

`self.scene` is a `Scene` (mostly opaque to Python). The one useful method is also surfaced directly on `HamsterBehaviour`, so you rarely need the Scene itself:

| Method | Returns | Description |
|---|---|---|
| `find_entity_by_name(name)` | `EntityHandle` or `None` | Same as `self.find_entity_by_name`. |
