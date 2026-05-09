# Bug 0001: Entities not rendering in scene editor viewport

> Status: **fixed**
> Severity: **High**
> Tier: **2**
> Logged: 2026-05-09
> Found while: editor-theme feature visual verification

---

## Symptom

Entities are not being rendered at all in the scene editor viewport. They may appear in the hierarchy panel but nothing is visible in the viewport.

## Suspected location

Unknown.

## Reproduction

1. Build and run `Hamster-Wheel.exe`
2. Open or create a project with entities
3. Observe: entities are not visible in the scene viewport

---

## Investigation (Tier 2/3 only)

### Hypotheses considered

1. **`m_IsRunning` guard blocking editor-mode rendering** — Confirmed: `Scene::OnRender` wrapped all drawing in `if (m_IsRunning)`, which is false in editor mode. Removing the guard was necessary but not sufficient — entities still didn't render during play mode.
2. **FramebufferTexture resize bug** — Investigated, ruled out. The FBO staying at 1920x1080 is intentional design (editor shows a subsection of the full scene).
3. **Renderer static→instance refactor broke rendering** — Confirmed via git bisection: commit `ac35b3f4` (Renderer refactor) is the breaking commit, `c0c1f3bb` (one commit before) works. The diff is purely mechanical (static→instance), no logic changes.
4. **Uninitialized GLM instance members** — Root cause. `inline static glm::vec2 m_CameraOffset` was zero-initialized by C++ standard; instance member `glm::vec2 m_CameraOffset` is left uninitialized by GLM's default constructor (unless `GLM_FORCE_CTOR_INIT` is defined). The garbage offset fed into `UpdateViewMatrix()` during construction produced a garbage projection matrix.

### Evidence

- Git bisect: `c0c1f3bb` renders entities on play, `ac35b3f4` does not.
- Diff between commits is purely mechanical (static→instance), no logic changes, no shader changes.
- C++ standard guarantees zero-initialization for static storage duration objects; GLM's vec2/mat4 default constructors leave instance members uninitialized for performance.

---

## Root cause

Two issues combined:

1. **Primary**: When `Renderer` was converted from static to instance class, `glm::vec2 m_CameraOffset` lost its guaranteed zero-initialization. As a static member (`inline static glm::vec2`), C++ guarantees zero-init. As an instance member, GLM's default constructor leaves it uninitialized. `UpdateViewMatrix()` reads `m_CameraOffset` during construction to compute the `glm::ortho` projection matrix — garbage offset values produce a garbage projection, causing all entities to project to invisible locations.

2. **Secondary**: `Scene::OnRender()` wrapped all drawing inside `if (m_IsRunning)`, preventing entities from rendering in editor mode (only during play/simulation).

---

## What would have prevented this

Do not change static elements to non-static without deep reasoning.

## Fix

- `Hamster-Core/src/Renderer/Renderer.h` — explicitly initialized `m_CameraOffset{0.0f, 0.0f}` and `m_ViewMatrix{1.0f}` to prevent uninitialized member reads.
- `Hamster-Core/src/Core/Scene.cpp` — removed the `if (m_IsRunning)` guard around the render loop in `OnRender()`, so entities render in both editor and play modes.

## Verification

- Repro steps run on 2026-05-09: PASS (visual — run editor, open project, entities visible in viewport without pressing play)

---

## Related
