# Bug 0012: imported textures (and their .sheet sidecars) not persisted to project blob

> Status: **fixed**
> Severity: **Critical**
> Tier: **2 — investigation**
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

- The load path **does** read sidecars: `AssetManager::Deserialise` iterates the blob's texture list and calls the sync `AddTexture(path)` overload (`AssetManager.cpp:666`), which reads the `.png.sheet` sidecar (`AssetManager.cpp:137`) and re-registers sub-sprites. Confirmed by the "re-import surfaces the regions" symptom — same code path runs on re-import.
- So the gap is strictly **before** save: the blob never lists the imported texture, because no import path calls `Project::SaveCurrentProject`. Grep of the import call sites (`AssetBrowser.cpp` Import Texture / Import Spritesheet, `SpritesheetEditor.cpp` Save) found zero save calls.
- The only save sites are `Scene::RunSceneSimulation` (Play) and `Application::~Application` (exit). Pre-existing creation-time textures persist because `Project::Create` calls `SaveCurrentProject` (`Project.cpp:125`). cwd is set to the project dir in `Project::Open` (line 190) and not restored, so the relative-path save in `SaveCurrentProject` (`config.Name + ".hamproj"`) targets the project dir during normal editor use — the same assumption the working Create/Play saves rely on. The alt "cwd moved" hypothesis is therefore ruled out for the common case.

---

## Root cause

Importing a texture (or slicing a sheet) mutates the in-memory `AssetManager` but nothing persists that mutation to `<project>.hamproj`. The project blob is only rewritten on Play and on the exit-time destructor; the destructor save is the sole path that would normally capture an import, and it is unreliable (amplified by bug 0008's exit segfault). So a fresh import + close (without ever pressing Play) loses the texture entry from the blob. The `.png.sheet`/`.png.meta` sidecars survive on disk, but `Deserialise` never re-reads them because the blob doesn't list the texture.

## What would have prevented this (Tier 2/3 only)

A smoke-test scenario that imports a texture, serialises + re-deserialises the project, and asserts the texture is present would have caught that imports never persisted to the blob — and a single explicit save chokepoint (rather than relying on the exit-time dtor) would have made the loss impossible.

---

## Fix (implemented)

1. `Hamster-Wheel/src/Panels/AssetBrowser.cpp` — Import Texture: switched from `AddTextureAsync` to the **sync** `AddTexture` so the texture is in the map before saving (the async load is deferred to a later main-thread callback and would race the save), then call `Project::SaveCurrentProject(m_AssetManager)`. Import Spritesheet: call `SaveCurrentProject` right after `AddTexture`, before opening the slice editor.
2. `Hamster-Wheel/src/Panels/SpritesheetEditor.cpp` — added `#include <Core/Project.h>`; call `SaveCurrentProject(m_AssetManager)` after `SheetSidecar::Write`, before `Close()`, so a slice + Save cycle persists the texture entry too.
3. cwd confirmed stable for the common case (see Evidence) — no signature change needed; relies on the same project-dir-relative save that Create/Play already use.

Decision: the async path was converted to sync rather than wiring a completion-time save into `AssetManager`, which would have coupled `AssetManager` → `Project`. Import is an occasional, modal-blocking action and Import Spritesheet already used the sync overload, so both import paths are now consistent.

## Verification

(Required at close-out: re-run reproduction steps above; sheet + regions should survive reopen.)
Build + smoke green after fix (1/1 SmokeTest, Hamster-Wheel links clean). Verified on disk (2026-05-25): after import, `Untitled.hamproj` lists the imported texture (`7p2dx234dl291.png`); after a slice-editor Save the `.png.sheet` sidecar is written. Author confirmed the sheet + its regions survive close/reopen. PASS.

Note: surfaced a related interaction gap — closing the slice editor used to discard unsaved regions silently (the user was closing instead of clicking Save). Addressed by the confirm-on-close prompt (shipped alongside, see SpritesheetEditor).

---

## Related

- Bug 0008 (editor segfault on exit) — likely amplifier; if the dtor crashes, the only save path doesn't run.
- spritesheet-support feature (`docs/features/active/spritesheet-support.md`) — stages 1–6 shipped 2026-05-19; this bug surfaced during stage 7 manual verification.
- Bug 0013 (asset-browser sheet expansion layout) — separate UX-only concern, same panel.
