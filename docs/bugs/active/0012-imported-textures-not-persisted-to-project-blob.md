# Bug 0012: imported textures (and their .sheet sidecars) not persisted to project blob

> Status: **open**
> Severity: **Critical**
> Tier:
> Logged: 2026-05-24
> Found while: stage 7 manual verification of spritesheet-support

---

## Symptom

After importing a PNG via Asset Browser → "Import Spritesheet" (or "Import Texture"), slicing regions in the SpritesheetEditor, clicking Save, and closing the editor: on reopening the project, the imported texture **and all its sub-sprites are gone from the Asset Browser**. Re-importing the same PNG from disk re-surfaces the regions, because `AddTexture(path)` re-reads the `.sheet` sidecar that was correctly written next to the PNG.

So the data is on disk (sidecar persists), but the project's `.hamproj` blob does not list the texture, so on load `AssetManager::Deserialise` never asks the sidecar to be read.

This is the root cause behind two reported symptoms:
- "sub-sprites don't appear in the asset browser" (after reopen)
- "spritesheets don't save properly … but re-importing brings the regions back"

## Suspected location

`Hamster-Wheel/src/Panels/AssetBrowser.cpp:228` (Import Texture, async) and `:241` (Import Spritesheet, sync) — neither call `Project::SaveCurrentProject` after adding the texture.

`Hamster-Wheel/src/Panels/SpritesheetEditor.cpp:442–477` — Save handler writes the `.sheet` sidecar and registers sub-sprites with `AssetManager`, but does not save the project blob, so a fresh-import + slice + close cycle still loses the texture entry.

The only places that currently save the blob are `Scene::RunSceneSimulation` (on Play) and `Application::~Application` (on exit). The exit-time save fails or is bypassed for many users — bug 0008 (editor segfault on exit) is the most likely culprit; if the dtor crashes mid-cleanup, the blob is never updated.

## Reproduction

1. Open editor; open or create a project.
2. Asset Browser → Import Spritesheet → pick a PNG.
3. In the editor that pops up: draw a region, click Save.
4. Verify the sheet's expand caret appears on its card; expand → mini-card visible.
5. Close the editor window (X / Alt-F4 / File→Exit).
6. Relaunch the editor; reopen the same project.
7. **Observed**: the imported sheet is missing from the Asset Browser entirely; no sub-sprites.
   On disk: `<project>/<filename>.png.sheet` exists and is valid.
8. Re-import the same PNG via "Import Spritesheet" → the sheet appears with all original regions intact (loaded from the `.sheet` sidecar).

Pre-existing textures (added at project creation — square/triangle/circle) survive reopen, because `Project::Create` explicitly calls `SaveCurrentProject` at line 125.

---

## Investigation (Tier 2/3 only)

### Hypotheses considered

- Root cause (strong): imports don't trigger `Project::SaveCurrentProject`; only the exit-time save persists them, and that save doesn't always run cleanly (bug 0008).
- Alt: cwd is moved before exit-time save runs, so `SaveCurrentProject` writes to the wrong directory. Unverified — line 197 uses relative path `config.Name + ".hamproj"`, cwd is set in `Project::Open` line 190 and not obviously restored elsewhere. Worth checking.
- Alt: `SheetSidecar::Read` is failing silently on a path-format mismatch (e.g. backslash vs forward slash, or absolute vs relative). Less likely — `AddTexture(path)` works on re-import via the same code path, so the read itself is fine.

### Evidence

(Pending — not yet confirmed end-to-end.)

---

## Root cause

(Pending investigation; strongest hypothesis: imports are never explicitly persisted; the only save path is the exit-time dtor, which fails in the presence of bug 0008.)

## What would have prevented this (AUTHOR WRITES — Tier 2/3 only)

---

## Fix (proposed, not yet implemented)

1. `Hamster-Wheel/src/Panels/AssetBrowser.cpp` — after Import Spritesheet (sync, line 241) call `Hamster::Project::SaveCurrentProject(m_AssetManager)`. After Import Texture (async, line 228) wire an enqueue-time save or convert to sync — TBD during fix.
2. `Hamster-Wheel/src/Panels/SpritesheetEditor.cpp` — at the end of a successful Save (after `SheetSidecar::Write`, before `Close()`) call `Hamster::Project::SaveCurrentProject(m_AssetManager)` as belt-and-braces.
3. Confirm cwd is stable between `Project::Open` and the eventual save call, or pass project dir explicitly into `SaveCurrentProject`.

## Verification

(Required at close-out: re-run reproduction steps above; sheet + regions should survive reopen.)

---

## Related

- Bug 0008 (editor segfault on exit) — likely amplifier; if the dtor crashes, the only save path doesn't run.
- spritesheet-support feature (`docs/features/active/spritesheet-support.md`) — stages 1–6 shipped 2026-05-19; this bug surfaced during stage 7 manual verification.
- Bug 0013 (asset-browser sheet expansion layout) — separate UX-only concern, same panel.
