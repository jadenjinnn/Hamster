# Feature placeholder: project resolution + play window

> Tier: **Full spec** (provisional guess — pending proper /feature pass)
> Status: **draft (placeholder)**
> Started: 2026-05-19
> Spec author: Jaden

> **This is a lightweight capture, not a proper spec.** Re-run `/feature` and elevate to a real spec when this feature is picked up for implementation.

---

## One-line summary

Projects declare a target window resolution at creation; the editor shows a grey-bordered outline of that resolution inside the scene viewport; pressing Play opens a separate, sized OS window that runs the simulation.

## What the user described (verbatim, paraphrased)

- "When creating a project we should be able to click the resolution/dimension of window we want."
- "In the editor we should see a grey border outline to show where the screen is."
- "When we press play we should get a separate window at that dimension."

## Rough scope

In:
- Project creation modal grows a resolution picker (presets — 1280×720, 1920×1080, 800×600 — plus custom width/height).
- `.hamproj` gains `targetWidth` and `targetHeight` integer fields. Default if missing: TBD (probably 1280×720).
- LevelEditor renders a grey rectangle outline at world coordinates `[(-w/2,-h/2) .. (w/2,h/2)]` (or `[0,0..w,h]` — to decide) to indicate the play bounds. Visible always in edit mode, possibly hidden in play mode.
- Pressing Play creates a second GLFW window sized exactly `targetWidth × targetHeight`, runs the scene's main loop into that window's context, and shuts it down on Stop.
- The editor's scene viewport may continue to mirror the play view (live preview) or go inert during play — to decide.

Out of scope (initial guesses, lock during proper /feature pass):
- Fullscreen toggle.
- Per-scene resolution overrides (project-level only).
- DPI awareness / scaling.
- Multiple play windows.
- Resolution change at runtime (e.g. settings menu in the game).

## Tentative classification

- **Reversibility: Sticky** — changes the project file format and introduces a second OS window in the play path; can't be ripped out without breaking saved projects.
- **Scope: Cross-cutting** — Project + ProjectSerialiser + ProjectCreator modal + LevelEditor render + Application/Window (second context) + possibly Renderer (separate FBO per window).
- **Tier guess: Full spec.**

## Open questions (need answering during real spec pass)

- Does the second window share the GL context with the editor's window, or get its own? (Sharing is faster but messes with ImGui's context assumptions.)
- Does Stop close the window, or hide it? (Stop → reset → Play again — recreating the window every time is wasteful but simplest.)
- Does the play window need a custom title bar / chrome, or use OS-default?
- Does the grey outline live in world space (scales with zoom) or screen space?
- What happens when the editor viewport aspect ratio doesn't match the play window aspect ratio — letterbox in editor, or just show the outline?
- How does this interact with the upcoming **packaging** feature — does the redistributable still spawn a "play" window, or does it just *be* that window (no editor)?

## Related

- Connects forward to the **packaging** feature: a runtime-only player (no editor) needs to know how to size its own window, presumably from this same `.hamproj` field.
- Connects forward to a hypothetical pixel-perfect / reference-resolution upscale feature (Bug 0009 mentions integer-scale upscaling as Future work).
