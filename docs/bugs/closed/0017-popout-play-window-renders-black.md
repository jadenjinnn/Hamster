# Bug 0017: popout play window renders black (nothing drawn)

> Status: **fixed**
> Severity: **High**
> Tier: **2**
> Logged: 2026-05-26
> Found while: building the Flappy Bird demo game

---

## Symptom

Pressing Play opens the popout game window, but it's entirely black — no sprites, no UI — even with entities (background, bird) positioned inside the play area. The same scene renders correctly in the editor viewport.

## Suspected location

`Hamster-Core/src/Renderer/Renderer.cpp` (VAO usage), `Hamster-Wheel/src/EditorLayer.cpp` (popout render block), `Application::OpenPlayWindow`.

## Reproduction

1. Project with any sprite entity inside the play area.
2. Press Play. Observe: editor viewport shows the scene; popout window is black.

## Investigation

### Hypotheses considered
- *Entities positioned outside the play area.* Ruled out — a bird placed at `(120,330)`, well inside `(0,0)→(432,768)`, was also absent from the popout.
- *Popout never renders / no buffer swap.* Ruled out — `EditorLayer` does `Scene::OnRender` + `OnRenderUI` + `glfwSwapBuffers(popout)` with the viewport/zoom/camera correctly swapped; the projection is set via `UpdateViewMatrix`.
- *Shared-context object visibility.* Confirmed — the popout is created with a shared context (`glfwCreateWindow(..., editor)`), but **VAOs are not shared across GL contexts** (only buffers, textures, and shaders are).

### Evidence
- The renderer's VAOs (`m_BatchVAO`, `m_UIRectVAO`, `m_UITextVAO`, `m_VAO`) are created once in `InitRendererData`, which runs in the editor's context. Binding them in the popout's context fetches no vertex attributes, so `glDrawArrays` draws nothing.
- The popout's UI click hit-test worked (CPU-side `ResolveUIButton` math), which is why this was never noticed — the popout has rendered nothing visible since it was added.

## Root cause

OpenGL VAOs (and per-context GL render state such as blend/depth enables) are *not* shared between contexts, even when the contexts share objects. The popout play window uses a shared context but bound the editor context's VAOs, so no vertex data was sourced and every draw was a no-op → black window. Buffers, textures, and shaders *are* shared, so only the VAO containers (and the blend state) needed per-context setup.

## What would have prevented this

Knowing the OpenGL sharing rules (VAOs/FBOs are per-context; buffers/textures/shaders are shared) when the multi-window popout was designed — or actually verifying the popout drew pixels rather than only that clicks registered.

## Fix

- `Renderer::CreatePopoutVertexArrays()` creates popout-context duplicates of the three VAOs the popout render uses (sprite batch, UI rect, UI text), wiring the same shared VBOs with identical attribute layouts, and sets the popout context's blend/depth state.
- `Renderer::SetPopoutMode(bool)` selects the popout VAOs at the three draw-bind sites; `EditorLayer` wraps the popout's `OnRender`/`OnRenderUI` in `SetPopoutMode(true)`/`(false)`.
- `Application::OpenPlayWindow` calls `CreatePopoutVertexArrays()` **with an explicit `glfwMakeContextCurrent(popout)`** first. The original code (and a stale comment) assumed `glfwCreateWindow` makes the new context current — it does not — so the first cut of this fix created the popout VAOs in the editor context and stayed black; switching context explicitly is what made it render.

## Verification

- Smoke test PASS (editor render path unchanged; popout VAOs are additive).
- Editor: press Play → popout shows the scene (sprites + UI), matching the editor viewport. Confirmed by author.

---

## Related

- Latent since the project-resolution-and-popout-play-window feature (2026-05-19); visual verification of the popout had been deferred.
