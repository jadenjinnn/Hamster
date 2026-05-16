# Bug 0003: `AssetManager::GetScript` throws cryptic exception on missing UUID

> Status: **open**
> Severity: **High**
> Tier:
> Logged: 2026-05-16
> Found while: manual verification of bug 0002 fix

---

## Symptom

When opening a project whose scene file references a script UUID that is not in the project's `.hamproj` asset table, the editor aborts the open with: `Failed to open project: invalid unordered_map<K, T> key`.

Terminal context immediately before the error (from the user's actual session):
```
trying to add script with uuid of: 97b5432e-4d69-4324-bf63-3ce8f1b0bf45
Failed to open project: invalid unordered_map<K, T> key
```

The exception text is MSVC STL's `out_of_range` wording from `std::unordered_map::at`, surfaced via the catch block in `ProjectHubLayer::OpenProject`. The actual code path that throws is `SceneSerialiser.cpp:294` calling `m_AssetManager->GetScript(scriptUUID)` — `GetScript` is `m_Scripts.at(uuid)` (`AssetManager.cpp:178`).

The user's project has at least one scene that references a script UUID not present in the project-level asset list. This means the data is broken on disk, but the error message exposes a raw STL-internal string and aborts the entire `Project::Open` rather than skipping the broken behaviour.

## Suspected location

- `Hamster-Core/src/Utils/AssetManager.cpp:177–179` — `GetScript` uses `m_Scripts.at(uuid)` without checking presence.
- `Hamster-Core/src/Core/SceneSerialiser.cpp:293–294` — calls `GetScript` inside a deserialise loop with no validation.
- Project save path — somewhere the script reference is being added to a scene but not committed to the asset manager's persisted list. Pre-existing inconsistency between `Scene::Serialise` and `AssetManager::Serialise`.

## Reproduction

The user's local project at `C:\Users\Jaden\Downloads\<flappy bird-style project>` reproduces this on every open. Generic repro:

1. Open a project that has a `Behaviour` component whose `scripts` field references a UUID not present in the project's `.hamproj` asset section. (Easiest synthetic repro: open a project, manually edit the `.hamproj` to remove a script entry while leaving the scene file untouched, save, re-open.)
2. Click the project card in the hub.
3. Observe error dialog: `Failed to open project: invalid unordered_map<K, T> key`.

Expected: the broken behaviour reference is either dropped with a logged warning, or the open succeeds with the script slot empty and a console warning. Either way, the user should be able to enter the editor.

---

## Related

- Bug 0002: blocked manual verification of bug 0002's repro on the user's local project — discovered while attempting that verification.
- Bug 0004: AssetManager cross-project pollution — same family, same root area.
