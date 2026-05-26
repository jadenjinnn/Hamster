# Bug 0016: scene entities not persisted on editor close (data loss)

> Status: **fixed**
> Severity: **Critical**
> Tier: **2**
> Logged: 2026-05-26
> Found while: building the Flappy Bird demo game

---

## Symptom

After building a scene (entities + components) and closing the editor, reopening the project shows an empty scene — all entities are gone. The on-disk scene file stays at its 20-byte creation size; only the `.hamproj` and asset sidecars update. No error or warning.

## Suspected location

`Hamster-Core/src/Core/Application.cpp` (shutdown/close path), `Scene::SaveScene`, `Project::SaveCurrentProject`.

## Reproduction

1. New project; add several entities with components in edit mode.
2. Close the editor window (do not explicitly "Save Scene").
3. Reopen the project. Observe: entities gone; `Scenes/*.scene` still 20 bytes from creation.

## Investigation

### Hypotheses considered
- *Save writes the wrong path.* Ruled out — `Scene::SaveScene` writes `scene->GetPath()` relative to the project cwd, which is correct.
- *`SaveCurrentProject` should persist the scene.* Confirmed contributing — it serialises the `.hamproj` + AssetManager only, **never the scene**, so imports (which call it) made saving *look* like it worked while the scene was never written.
- *The destructor's save never runs.* Confirmed primary — the dtor does loop `Scene::SaveScene` (Application.cpp), but it runs amid teardown that crashes (bug 0008) and, separately, `Application::Close` destroyed the window immediately, so the save either never executed or was pre-empted by the crash.

### Evidence
- Scene file mtime = project-creation time; never updated across a full build session.
- `.hamproj`/`.sheet` mtimes = import time → confirms `SaveCurrentProject` ran but didn't touch the scene.

## Root cause

Scene entities are persisted *only* by `Scene::SaveScene`, which is invoked on an explicit "Save Scene" menu action, on Play, and in the `Application` destructor. There is no autosave. `Project::SaveCurrentProject` — called by imports and project switches — deliberately does not save the scene, so normal editing produced no scene write. The one implicit save (the destructor) sits downstream of the bug-0008 teardown crash, so in practice it never lands. Net: unless the user manually clicked "Save Scene", entities lived only in memory and were lost on close.

## What would have prevented this

Saving the scene at close-request time (while everything is still alive), rather than relying on a destructor that runs during crash-prone teardown, would have made persistence robust regardless of bug 0008.

## Fix

- `Application::Close` now saves the project **and** every scene immediately on the window-close request — before any teardown — so the write is guaranteed (`Application.cpp`). The destructor still saves as a backstop.
- `Scene::SaveScene` now passes the `AssetManager` to `SceneSerialiser` (was the AssetManager-less ctor), matching the play-snapshot serialiser so sprite asset references persist.
- Depends on the bug-0008 fix so the close path no longer crashes before/around the save.

## Verification

- Smoke test PASS (serialisation paths unaffected).
- Editor: build a scene, close, reopen → entities persist; `Scenes/*.scene` grows past 20 bytes with a fresh timestamp. Confirmed by author.

---

## Related

- **Bug 0008** (exit segfault) — fixed in the same change; its teardown crash is why the destructor's existing save never landed.
