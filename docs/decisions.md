# Decisions

Non-obvious technical decisions made during development, preserved for future reference.

## Collider serialization uses a separate ComponentID (2026-05-12)

Collider offset/size data is stored on the `Rigidbody` struct but serialized as its own `Collider_ID = 7` block rather than appended to the `Rigidbody_ID` block. The binary serializer reads a fixed number of fields per component ID — appending fields would cause old scene files (which have fewer Rigidbody fields) to corrupt the parse stream by reading the next component ID as float data. A separate `Collider_ID` block is cleanly skipped by old deserializers and defaults to (0,0) when absent.

## Box2D shape-local offset for colliders (2026-05-12)

Collider offset is applied as a shape-local offset (`b2MakeOffsetBox` center parameter, `b2Circle::center`) rather than shifting the body position. This means `SyncPhysicsToTransforms` doesn't need to account for the offset at all — the body center remains the entity center, and only the collision shape is displaced. Avoids the rotation-dependent offset math that would be needed if the body position were shifted.

## Picking Y-flip uses FBO height, not window framebuffer height (2026-05-15)

The Renderer's orthographic projection covers `(0,0)` to `(m_ViewportWidth, m_ViewportHeight)` (window framebuffer size). The Level Editor's FBO is sized to the panel's available content region, which is smaller. To pick an entity under the cursor we read pixels from the FBO at `(mouseX, FBO_height - mouseY)`, **not** `(mouseX, m_ViewportHeight - mouseY)`. The old code in Hamster-Wheel used `m_ViewportHeight` and worked only because the docked Level Editor panel filled the window, making the two heights equal. With a non-docked layout the panel is smaller and the formula reads out-of-bounds pixels.

## Custom title bar drag uses screen-space cursor coords (2026-05-15)

`glfwGetCursorPos` returns cursor position relative to the window's client area. When the title bar drag handler moves the window via `glfwSetWindowPos`, the cursor's window-relative position changes too, which feeds back into the drag math and oscillates the window by one pixel per frame. Fix: convert to screen-space by adding the window position (`screenCursor = windowCursor + windowPos`) and compute drag deltas against the screen-space coordinate, which is stable regardless of window movement.

## Default framebuffer must be cleared each frame (2026-05-15)

The engine's `ImGuiLayer::Begin/End` does not clear the default framebuffer. With a docked editor layout the panels cover the full screen and the unclear is invisible. With the new fixed-card layout there are gaps between panels showing the dark background — without an explicit clear those gaps render stale framebuffer content from prior frames (smeared ImGui draw lists, garbage). EditorLayer's `OnUpdate` now ends with `glClear(GL_COLOR_BUFFER_BIT)` to the gap colour after unbinding the scene FBO.

## Application accepts WindowProps in its constructor (2026-05-15)

Added `Application::Application(const WindowProps &props)` so the prototype-rooted editor can request a borderless window (`GLFW_DECORATED=GLFW_FALSE`) needed for the custom title bar. The default constructor delegates to it with default props, preserving existing behaviour for callers that don't pass a WindowProps. `WindowProps` gained a `borderless` field; if true, `Window`'s constructor sets the `GLFW_DECORATED` hint to `GLFW_FALSE` before `glfwCreateWindow`.
