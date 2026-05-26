---
title: Components & Types
description: Components, enums, and math types exposed to Python.
---

The data types in the `Hamster` module: components you add to entities, the enums they use, and the math types.

## Components

### Transform

Position, rotation, and size. Every entity has one.

```python
Hamster.Transform(position: vec3, rotation: float, size: vec2)
```

| Field | Type | Description |
|---|---|---|
| `position` | `vec3` | World position (x, y, z). z orders draw depth. |
| `rotation` | `float` | Rotation in degrees. |
| `size` | `vec2` | Width and height in world units. |

```python
t = Hamster.Transform(Hamster.vec3(0, 0, 0), 0.0, Hamster.vec2(100, 100))
```

### Sprite

A drawable texture/colour on an entity.

```python
Hamster.Sprite(colour: vec3)
```

| Field | Type | Description |
|---|---|---|
| `colour` | `vec3` | Tint multiplied with the texture (white = unmodified). |

The texture itself is assigned in the editor, or at runtime with `EntityHandle.set_texture(name)`.

### Rigidbody

Box2D physics body. All constructor arguments are optional keywords.

```python
Hamster.Rigidbody(
    body_type=Hamster.BodyType.Static,
    collider_shape=Hamster.ColliderShape.Box,
    density=1.0,
    friction=0.3,
    restitution=0.0,
    gravity_scale=1.0,
)
```

| Field | Type | Description |
|---|---|---|
| `body_type` | `BodyType` | `Static`, `Dynamic`, or `Kinematic`. |
| `collider_shape` | `ColliderShape` | `Box` or `Circle`. |
| `density` | `float` | Mass per area. |
| `friction` | `float` | Surface friction. |
| `restitution` | `float` | Bounciness (0 = no bounce). |
| `gravity_scale` | `float` | Multiplier on gravity (0 = unaffected). |

### UIButton

A screen-space button. Anchored to a corner/edge and offset in pixels.

```python
b = Hamster.UIButton()
```

| Field | Type | Description |
|---|---|---|
| `anchor` | `UIAnchor` | Which screen point `offset` is measured from. |
| `offset` | `vec2` | Pixel offset from the anchor. |
| `size` | `vec2` | Button size in pixels. |
| `auto_size` | `bool` | Size to fit the label instead of `size`. |
| `padding` | `vec2` | Padding around the label when `auto_size`. |
| `bg_colour` | `vec4` | Background colour (r, g, b, a). |
| `label` | `str` | Button text. |
| `text_colour` | `vec4` | Label colour. |
| `font_size` | `float` | Label size. |
| `text_align` | `UITextAlign` | `Left`, `Centre`, or `Right`. |

Clicks are delivered to scripts via `on_button_clicked(self, uuid)`.

### UIText

Screen-space text.

| Field | Type | Description |
|---|---|---|
| `anchor` | `UIAnchor` | Anchor point for `offset`. |
| `offset` | `vec2` | Pixel offset from the anchor. |
| `text` | `str` | The text to draw. |
| `text_colour` | `vec4` | Colour. |
| `font_size` | `float` | Size. |
| `wrap_width` | `float` | Wrap width in pixels (0 = no wrap). |

Update at runtime with `EntityHandle.set_text(text)`.

## Enums

### BodyType

`Static` · `Dynamic` · `Kinematic`

### ColliderShape

`Box` · `Circle`

### UIAnchor

`TopLeft` · `TopCentre` · `TopRight` · `MiddleLeft` · `Centre` · `MiddleRight` · `BottomLeft` · `BottomCentre` · `BottomRight`

### UITextAlign

`Left` · `Centre` · `Right`

### log_type

`Info` · `Warning` · `Error`

### key_code

Accessed as `Hamster.key_code.<name>`. The set mirrors GLFW keys with a `key_` prefix.

| Group | Names |
|---|---|
| Letters | `key_a` … `key_z` |
| Digits | `key_0` … `key_9` |
| Arrows | `key_left`, `key_right`, `key_up`, `key_down` |
| Modifiers | `key_left_shift`, `key_left_control`, `key_left_alt`, `key_left_super` (and `key_right_*`) |
| Function | `key_f1` … `key_f25` |
| Keypad | `key_kp_0` … `key_kp_9`, `key_kp_add`, `key_kp_subtract`, `key_kp_enter`, … |
| Editing | `key_space`, `key_enter`, `key_tab`, `key_backspace`, `key_delete`, `key_insert`, `key_escape` |
| Navigation | `key_home`, `key_end`, `key_page_up`, `key_page_down` |
| Punctuation | `key_comma`, `key_period`, `key_minus`, `key_equal`, `key_slash`, `key_semicolon`, `key_apostrophe`, `key_left_bracket`, `key_right_bracket`, `key_backslash`, `key_grave_accent` |
| None | `key_not_pressed` (returned when no key is pressed) |

## Math types

### vec2

```python
v = Hamster.vec2(x, y)
```

Fields `x`, `y`. Supports `+`, `-`, `*` (component-wise) between vectors and `*` by a float.

### vec3

```python
v = Hamster.vec3(x, y, z)
```

Fields `x`, `y`, `z`. Supports `+`, `-`, `*` between vectors.

### vec4

```python
v = Hamster.vec4(x, y, z, w)
```

Fields `x`, `y`, `z`, `w`, also aliased as `r`, `g`, `b`, `a` for colours.

### UUID

An entity id. Compare with `==` / `!=`, hashable, and `str()`-able. Obtained from `self.uuid`, `EntityHandle.uuid`, `self.parent`, etc. — you don't construct these yourself.
