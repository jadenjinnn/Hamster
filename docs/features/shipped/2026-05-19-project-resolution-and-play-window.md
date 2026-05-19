# Feature spec: project resolution + play window

> Tier: **Full spec**
> Status: **approved**
> Started: 2026-05-19
> Spec author: Jaden

---

## Classification

- **Reversibility**: Sticky
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: The `.hamproj` save format gains two fields (every existing + future project locks in), a second OS window + GL context is introduced into the play path, and input routing splits between the editor and the popout — none of these can be unwound cheaply after ship, so Full spec is the right depth.

---

## Problem

Today, "Play" in the editor means the scene runs *inside* the editor's scene-viewport panel. There is no concept of "this game targets a specific screen size" — the play area is just whatever pixel region the panel happens to occupy, which shifts with window state, panel collapse, and zoom. A game built in the editor therefore has no honest preview of how it will look as a real, sized window. This also blocks the packaging feature: the eventual redistributable needs to know what window dimensions to spawn. Adding a per-project target resolution + a popout play window solves both problems and gives authors a "press Play, see your game in a real window" moment.

## In scope

- **Project creation modal** gains a resolution picker:
  - Preset list (1280×720, 1920×1080, 800×600 — small set, fixed for v1).
  - "Custom" option that reveals two integer fields (width, height).
  - Selected resolution is saved into `.hamproj`.
- **`.hamproj` format** gains two new integer fields: `targetWidth` and `targetHeight`. Default when missing (legacy projects): `1280×720`.
- **LevelEditor scene viewport** renders a grey rectangle outline at world-space coordinates `(0, 0)` to `(targetWidth, targetHeight)` representing the play area. Always visible in edit mode; hidden during play.
- **Pressing Play** creates a new GLFW window:
  - Title: `<projectName> — Play`.
  - Inner size: exactly `targetWidth × targetHeight`.
  - Non-resizable (`GLFW_RESIZABLE = false`), no maximise, no fullscreen.
  - Shares the editor's GL context (`glfwCreateWindow(..., shared = editorWindow)`) so textures, shaders, FBOs cross-reference correctly with no duplicate uploads.
  - Centred on the primary monitor (default).
- **Pressing Stop** destroys the popout window cleanly and returns the editor to edit mode.
- **Clicking the popout's X button** is treated identically to Stop.
- **Input routing**:
  - Keyboard + mouse events that arrive at the popout's GLFW handle feed the engine's event dispatcher and the running scripts.
  - Keyboard + mouse events that arrive at the editor's GLFW handle stay in the editor (panels, scene viewport interactions). Existing behaviour, unchanged.
  - "Focus-aware" falls out naturally — events are per-window — no extra focus tracking needed.
- **Editor scene viewport during play**:
  - Stays interactive: entity picking, transform grabber drags, property editor edits all keep working (with simulation-snapshot's existing semantics — edits revert on Stop unless explicitly saved).
  - Picking is **disabled** in the popout (clicks fall through to game scripts; no entity selection).
- **Scene rendering during play** targets the popout's default framebuffer (i.e. `Scene::OnRender` issues draw calls into the popout's GL context). The editor scene viewport continues to render to its existing FBO each frame for the in-editor preview, so authors can see the simulation in both windows.

## Out of scope

- **Fullscreen toggle.** Window is windowed-only for v1.
- **Resize during play.** Window is fixed-size.
- **Per-scene resolution overrides.** Resolution is a project-level property only.
- **DPI awareness / HiDPI scaling.** Window is sized in raw pixels.
- **Multiple play windows simultaneously.** One popout max.
- **Runtime resolution change from a Python script.** Resolution is set at project creation only; you'd close + edit `.hamproj` to change it.
- **Editor-side resolution change UI.** No "Project Settings → change resolution" in v1 — author edits `.hamproj` by hand or recreates the project. (Future work.)
- **Custom title-bar / window chrome on the popout.** OS default chrome only.
- **Window position memory across sessions.** Always centred on Play.
- **Picking via mouse-position routed to scripts in popout.** Mouse events flow to the popout for UI clicks (the existing `UIButton` hit-test), but there's no engine API for scripts to read raw mouse position in v1.

## API sketch

The user-visible Python API does **not** change. The new fields are config in `.hamproj`, not a runtime concept.

```python
# Nothing new in user scripts. UI button click events fire the same way
# they do today (on_button_clicked), but in the popout window rather
# than in the editor's scene viewport.
class Manager(Hamster.HamsterBehaviour):
    def on_button_clicked(self, uuid):
        # Same dispatch path. Mouse came from popout's window.
        pass
```

C++ side, the new internal API:

```cpp
// Application gains a popout-window lifecycle.
class Application {
public:
  // Called by EditorLayer when the Play button is pressed. Creates the
  // popout GLFW window sized to the project's targetWidth × targetHeight,
  // sharing the editor's GL context, wires its input callbacks to the
  // event dispatcher, and marks the popout as the current play target.
  void OpenPlayWindow(int targetWidth, int targetHeight,
                      const std::string &title);

  // Called on Stop or when the user closes the popout's X. Idempotent.
  void ClosePlayWindow();

  GLFWwindow *GetPlayWindow() const { return m_PlayWindow; }
  bool IsPlayWindowOpen() const { return m_PlayWindow != nullptr; }

private:
  GLFWwindow *m_PlayWindow = nullptr;
  // ...
};

// Project gains two fields, persisted by ProjectSerialiser.
struct Project {
  // ... existing fields ...
  int targetWidth  = 1280;
  int targetHeight = 720;
};
```

```cpp
// LevelEditor renders the grey outline by submitting a Renderer line-loop
// at world coords (0,0) → (W,H) every edit-mode frame.
if (!app->IsPlayWindowOpen()) {
  renderer->SubmitWorldRectOutline(
      glm::vec2(0, 0), glm::vec2(W, H),
      /*colour*/ glm::vec4(0.45f, 0.45f, 0.45f, 1.0f),
      /*thickness*/ 2.0f);
}
```

---

## Design

### Data structures

- `Project` (`Hamster-Core/src/Core/Project.h`): two new integer fields `targetWidth`, `targetHeight`. Defaults `1280, 720`.
- `ProjectSerialiser` (`Hamster-Core/src/Core/ProjectSerialiser.cpp`): both fields written + read. For legacy `.hamproj` files without the fields, the deserialiser uses the defaults (1280×720) and re-writes them on next save — silent forward-migration.
- `Application` (`Hamster-Core/src/Core/Application.h/.cpp`): adds `m_PlayWindow` (`GLFWwindow*`), `m_PlayWindowOpen` (bool), and the two methods sketched above.
- `EditorLayer` (`Hamster-Wheel/src/EditorLayer.cpp`): the Play button click handler gains a call to `app->OpenPlayWindow(...)` with the project's resolution; the Stop button gains `app->ClosePlayWindow()`.

### Module touchpoints

- `Hamster-Core/src/Core/Project.h` — add 2 fields.
- `Hamster-Core/src/Core/ProjectSerialiser.cpp` — serialise + deserialise 2 fields, with legacy default.
- `Hamster-Core/src/Core/Application.h` / `.cpp` — popout window lifecycle: `OpenPlayWindow`, `ClosePlayWindow`, GLFW callback registration for the popout's keyboard / mouse / close events, glfwSwapBuffers loop integration.
- `Hamster-Core/src/Core/Window.cpp` — possibly refactor so it can be parameterised (the popout reuses much of the editor window's bring-up code: GLAD context, vsync, input callbacks).
- `Hamster-Core/src/Core/Scene.cpp` — `OnRender` learns to optionally target a specific GL context's default framebuffer (the popout) rather than the editor's scene FBO.
- `Hamster-Core/src/Renderer/Renderer.cpp` — add `SubmitWorldRectOutline` (line-loop in world space using the existing flat shader) for the editor's grey outline. Likely 30 lines + shader-reuse.
- `Hamster-Wheel/src/EditorLayer.cpp` — Play / Stop button handlers wired to the new Application methods; grey outline draw call submitted each edit-mode frame.
- `Hamster-Wheel/src/Panels/ProjectCreator.cpp` / `.h` — resolution picker UI in the create-project modal (preset combo + custom width/height when "Custom" selected).

### Lifecycle / control flow

```
[Editor running, no popout]
  → user clicks Play button in editor
  → EditorLayer reads project.targetWidth / targetHeight
  → calls app->OpenPlayWindow(W, H, projectName + " — Play")
      → glfwCreateWindow(W, H, title, nullptr, editorGLFWWindow /*share ctx*/)
      → register input callbacks on the popout (key, mouse button, cursor pos, close)
      → store m_PlayWindow handle
  → calls scene->RunSceneSimulation() (existing path, no change)
  → editor scene viewport hides grey outline; popout window is now ready

[Per frame during play]
  → Application::Run() top-of-loop:
      → glfwPollEvents() processes both windows' input → dispatches into engine event bus
      → editor frame proceeds as today
      → Scene::OnUpdate() (physics, scripts) runs once — shared simulation state
  → Render pass:
      → for editor scene viewport: glfwMakeContextCurrent(editorWindow); Scene::OnRender(editorFBO)
      → for popout: glfwMakeContextCurrent(popoutWindow); Scene::OnRender(popoutDefaultFB)
      → glfwSwapBuffers(editorWindow) and glfwSwapBuffers(popoutWindow)
  → glfwWindowShouldClose(popout) on each iteration: if true, treat as Stop.

[Stop pressed OR popout closed]
  → EditorLayer calls scene->PauseSceneSimulation() (existing)
  → EditorLayer calls app->ClosePlayWindow()
      → glfwDestroyWindow(m_PlayWindow)
      → m_PlayWindow = nullptr
  → grey outline reappears in editor scene viewport
```

### Edge cases

- **Editor's GL context not current when popout draws**: rendering to the popout requires `glfwMakeContextCurrent(popout)` before its draw calls and `glfwSwapBuffers(popout)`. Forgetting this is the single most likely bug source. The Renderer must not assume one global context across the frame.
- **Shared GL resources**: textures / shaders / VBOs uploaded into the editor's context are visible from the popout because we shared the context. We must NOT re-upload anything when the popout opens.
- **Popout closed via X mid-frame**: GLFW's `WindowShouldClose` is checked at frame top after `glfwPollEvents`; the frame finishes drawing into a potentially soon-to-be-destroyed window. We tolerate this by deferring `ClosePlayWindow` to the next frame's top, OR by short-circuiting `Scene::OnRender(popout)` when shouldClose is true.
- **Custom resolution = 0×0 or negative**: ProjectCreator must clamp custom inputs to a sensible range (probably 100×100 minimum, 7680×4320 maximum — 8K).
- **Legacy `.hamproj` files**: deserialiser silently fills defaults (1280×720). On next save, fields are persisted. No upgrade dialog required.
- **Popout window position on multi-monitor setups**: GLFW centres on primary monitor by default; for v1 we accept that. If users have unusual setups they can drag the window after it opens — but they can't resize. Future work to remember last position.
- **Simulation runs at high FPS in the popout window**: Editor + popout both `glfwSwapBuffers` per frame, and only the editor has vsync. The popout may tear. Mitigation: enable vsync on the popout's context too (it's a one-line `glfwSwapInterval(1)` after `glfwMakeContextCurrent(popout)`).
- **Editor's scene viewport during play is interactive but also rendering the live simulation**: the existing edit-mode picking continues to work because the editor uses its own FBO + screen-to-world for picking. This is shared simulation state — moving an entity in the editor moves it in the popout in the same frame.
- **Z-order / focus stealing**: opening a new GLFW window may steal focus. Acceptable in v1; user clicks back into the editor if they want to keep editing.

---

## Full spec

### Sequence diagram: Play → Stop

```
User                EditorLayer       Application       Scene          Popout GLFWwindow
 |                       |                |                |                   |
 |--click Play---------->|                |                |                   |
 |                       |--Open(W,H)---->|                |                   |
 |                       |                |--glfwCreate--->|                   |
 |                       |                |  (share ctx)   |---------(spawn)-->|
 |                       |                |<--handle-------|                   |
 |                       |--RunSim()------|--------------->|                   |
 |                       |                |                |--on_create() called for behaviours
 |                       |                |                |                   |
 |  --- per frame ---    |                |                |                   |
 |                       |    glfwPollEvents (both windows)|                   |
 |                       |    Scene::OnUpdate (physics + scripts)              |
 |                       |    Render editor FBO                                |
 |                       |    Render popout (after MakeContextCurrent popout)  |
 |                       |    SwapBuffers both                                 |
 |                       |                                                     |
 |--click Stop---------->|                |                |                   |
 |                       |--PauseSim()--->|--------------->|                   |
 |                       |--ClosePlayWnd->|--glfwDestroy-->|---------(close)-->|
 |                                                                             |
```

### Error handling

- **Popout window creation fails** (rare — usually driver / display issues): `OpenPlayWindow` returns silently, simulation does *not* start, error logged to client logger ("Could not open play window"). Editor stays in edit mode. User can retry.
- **Resolution out of valid range** at project creation: ProjectCreator validates before save; out-of-range values are clamped + visual warning in the modal.
- **`.hamproj` deserialise sees garbage values** (negative, zero, absurdly large): defaults to 1280×720 + logs a warning. Project still opens.
- **GL context share failure**: GLFW returns `nullptr`. Treated as create-failure (above). No fallback — Play is unavailable on that machine.

### Performance considerations

- **Two render passes per frame during play.** Each pass touches every visible entity once. Simulation runs *once* — the second pass is just re-rasterisation. Spatial-index culling already in place reduces both passes.
  - Estimated cost at 100 sprites: ~2× draw-call overhead vs. edit-mode. Modest GPU work increase, negligible CPU.
  - Estimated cost at 1000 sprites: doubles per-frame GPU work in the render path. Sprite-batching v1 makes this tolerable. If it becomes a problem, a per-frame "render once, blit twice" path is a future-work optimisation.
- **GL context switch per frame** (two `glfwMakeContextCurrent` calls) has some driver overhead. Measured cost on modern drivers: sub-microsecond. Accepted.
- **Popout vsync** caps the popout's frame rate at the monitor refresh. The editor's existing vsync caps both.

### Migration / compatibility

- **Legacy `.hamproj` files** (created before this feature): deserialiser fills `1280×720` defaults. No breakage.
- **Pre-existing scenes** continue to load and render unchanged in edit mode. They will simply gain a grey outline showing the (default-1280×720) play area, and pressing Play will open a 1280×720 window.
- **`.pyd` API** is unchanged — Python scripts written before this feature work identically.
- **Smoke test fixtures**: existing smoke scenarios don't open a GLFW window at all (they embed Python and run frames headlessly). New smoke scenarios for this feature are limited to `.hamproj` serialise/deserialise round-trip (see Test extensions). Window-creation can't be smoke-tested; relies on manual verification.

---

## Why this approach

**Alternatives considered:**

1. **In-editor render at picked resolution** (letterboxed inside the scene viewport panel). Rejected because the user specifically wanted a separate window that "feels like a real game" — and because it also serves as the architectural foundation for the packaging redistributable.
2. **Spawn popout but reuse the editor's GLFW window via fullscreen-toggle**. Rejected because losing the editor entirely during play is a regression on the "edit during play" requirement.
3. **Separate process for the popout** (engine launches a runtime-only player as a subprocess). Rejected as drastically over-scope for v1 — would also break shared scene state.
4. **Render once into a shared FBO, blit to both windows**. Considered as a perf optimisation. Rejected for v1 because the implementation is a meaningful step up in renderer complexity and the cost of "render twice" is not yet measured as a bottleneck. Logged in Future work.

**Why these specific design choices:**

- **Recreate on Play, destroy on Stop**: simplest lifecycle. Window-reuse is a known footgun with leftover GL state. The few hundred ms of overhead per Play is acceptable; nobody plays-stops-plays a hundred times in a session.
- **Share GL context with editor**: textures, shaders, atlases all live in the editor's context already. Re-uploading on each Play would be wasteful and slow. Shared contexts are well-supported by GLFW.
- **All game input through popout**: avoids any "did the user mean to flap or type in a text field?" ambiguity. Falls out of separate windows naturally.
- **Editor stays editable during play**: matches Unity, where this is genuinely useful for tuning gameplay values mid-run. The simulation-snapshot feature already restores state on Stop, so non-destructive edits work cleanly.
- **Picking disabled in popout**: clicks in the popout are *game inputs* (UI buttons in particular). Letting them also pick entities would be confusing.

**Tradeoffs accepted**:
- 2× render passes per frame during play (until we add render-once-blit-twice).
- Non-resizable popout — no "drag corner to make it bigger" affordance.
- No per-scene resolution overrides — projects are monolithic on this dimension.

## Risks / what could go wrong

1. **GL context-current bugs.** Forgetting `glfwMakeContextCurrent(popout)` before its draw calls means we'd render to the wrong framebuffer — symptoms range from "popout is black" to "editor's scene viewport gets the popout's content overlaid." The single most likely class of bug. Mitigation: a `ScopedContext` RAII helper in `Application` that switches and restores; instrumented with `assert(glGetCurrentContext() == expected)` in debug builds.
2. **Window lifetime + frame split.** `glfwDestroyWindow` mid-frame (e.g. user clicks the X) while the popout is in the middle of being drawn into causes a crash or undefined draw. Mitigation: defer destroy to the next frame's top, after `glfwPollEvents` and the close-check, with a "should-close" flag set instead.
3. **Vsync timing across two windows.** Both windows vsync independently. On some drivers, two SwapBuffers in one frame can stall waiting for two retraces — effectively halving FPS. Mitigation: measure; if observed, disable vsync on the popout and rely on the editor's vsync to pace the loop.
4. **Editor's scene FBO and the popout's default FB have different sizes / aspect ratios.** ScreenToWorldPos is currently EditorLayer's helper for picking; it bakes panel size + scene FBO size. Re-using it for the popout would be wrong. Mitigation: keep picking entirely in the editor's coordinate system; popout has no picking by design.
5. **`.hamproj` format change breaks the existing per-project `.pyd` copy / project-watcher path** if the deserialiser is brittle. Mitigation: the new fields are appended; missing fields fall back to defaults. ProjectSerialiser already has skip-unknown-key behaviour for JSON fields (verify during implementation; if it's a positional binary format, we add a versioned header).
6. **Multi-monitor + DPI quirks.** A user with a 4K monitor and 200% scaling will see the popout at half its intended apparent size. Out of scope for v1 explicitly, but it's the first thing a recruiter on a Surface might notice. Document in the project README as a known v1 limitation.
7. **Popout window steals focus from the editor on creation.** Annoying when iterating fast. Mitigation: pass `GLFW_FOCUS_ON_SHOW=GLFW_FALSE` if it doesn't break the user's expectation; if it does, accept it.

## Success criteria

This feature is "shipped" when **all of the following** are observably true:

1. Opening the editor's Create Project flow shows a resolution picker with at least 3 presets and a Custom option.
2. Selecting a preset and creating the project produces a `.hamproj` that, when reopened, reports the same selected resolution.
3. Opening a project shows a grey rectangle outline at the project's target resolution in the editor's scene viewport, anchored at world `(0,0)` to `(targetWidth, targetHeight)`.
4. Pressing Play opens a separate, non-resizable OS window sized exactly `targetWidth × targetHeight`.
5. The popout window renders the live simulation. Pressing keys / clicking the popout drives game input. Pressing keys in the editor does not affect the simulation.
6. The editor's scene viewport continues to render the same simulation in real time and remains interactive (entity picking, transform drags, property edits all work).
7. Clicking the popout's X button stops the simulation cleanly (returns editor to edit mode, snapshot-restore runs, no crash).
8. Pressing the editor's Stop button destroys the popout window cleanly.
9. A legacy `.hamproj` (created before this feature) opens with the default 1280×720 resolution, no crash, no warning popup.
10. The smoke test passes (existing 27 scenarios + new ones — see below).

## Test extensions required

- **New smoke scenario: `.hamproj` resolution serialise round-trip.** Construct a Project with `targetWidth=1920`, `targetHeight=1080`, serialise to a temp file, deserialise from that file, assert both fields are 1920 and 1080.
- **New smoke scenario: legacy-deserialise default.** Hand-craft (or fixture-store) a `.hamproj` without the new fields, deserialise, assert defaults applied (1280×720), assert next-serialise re-emits them.
- **No window-creation smoke** — GLFW windowing isn't smoke-testable in this engine's current harness (which doesn't open a real window). Window behaviour is verified manually via the success criteria above.

---

## Decisions during implementation

<!-- Append-only. -->

## Spec amendments

<!-- Append-only. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Render-once-blit-twice** optimisation: render the simulation once into a shared FBO, then `glBlitFramebuffer` to both the editor's scene FBO and the popout's default FB. Removes the 2× draw-call cost.
- **Project settings panel** in-editor: lets the author change resolution after creation without editing `.hamproj` by hand.
- **DPI awareness**: handle HiDPI scaling so a 1280×720 project renders at logical-not-physical pixels on a 4K display.
- **Fullscreen toggle from inside the popout** (F11 or similar) — would require window-flag flipping at runtime.
- **Window position memory** across sessions.
- **Multiple popout windows** (e.g. local multiplayer split-screen, or one window per player view).
- **Runtime resolution change from a Python script** — `application.set_resolution(W, H)` or similar; requires the popout to recreate.
- **Packaging redistributable**: the runtime-only player (separate feature spec to come) reuses the popout window code as its primary window. This feature is the structural foundation for that.
- **Custom title-bar / chrome** on the popout — match the editor's borderless aesthetic if desired.
