# Bug 0004: AssetManager retains state across project switches, polluting subsequent projects

> Status: **fix-implemented**
> Severity: **High**
> Tier: **1**
> Logged: 2026-05-16
> Found while: manual verification of bug 0002 fix

---

## Symptom

After a failed `Project::Open` (e.g. the one in bug 0003), opening a different project — or creating a new one — pulls in textures/scripts/animations from the previously-attempted project. Concretely: the user did a click-through `Open` that failed mid-deserialisation (10 textures already added to AssetManager), then created a brand-new project from the hub, and the new `.hamproj` was saved containing those 10 textures from the unrelated project.

Same mechanism applies to a clean successful `Open` followed by another `Open`: textures from project A remain in the AssetManager when project B is loaded, and any save of project B then writes both sets out.

## Suspected location

- `Hamster-Core/src/Core/Project.cpp:99` (`Project::Open`) — does not clear AssetManager before `assetManager->Deserialise(...)`.
- `Hamster-Core/src/Core/Project.cpp:21` (`Project::New`) — does not clear AssetManager before populating the new project's default assets.
- `Hamster-Core/src/Utils/AssetManager.cpp:344` (`AssetManager::Deserialise`) — appends to `m_Textures` / `m_Scripts` / `m_Animations` rather than resetting them.

## Reproduction

1. Build and run `Hamster-Wheel.exe`.
2. Open project A (any working project with at least one custom texture).
3. From the editor, `File → Open Project`, pick project B.
4. Observe project B's asset browser showing project A's textures in addition to its own.
5. Save and inspect project B's `.hamproj` — it now persists the merged set.

---

## Investigation

Confirmed by reading `Project::Open` and `AssetManager::Deserialise` directly — no clear path is invoked between the two. Bug 0002's investigation plan flagged the same concern under "AssetManager retains state across project loads".

## Root cause

`Project::Open` and `Project::New` assume `AssetManager` represents the *current* project, but never reset it when the current project changes. The maps `m_Textures`, `m_Scripts`, `m_Animations` therefore monotonically grow across project loads. `AssetManager::Deserialise` uses `emplace` and `operator[]` which keep prior entries.

## What would have prevented this

A clearer ownership model: AssetManager should be reset (or recreated) on project change. The fix matches that — clear all three maps at the start of `Project::Open` and `Project::New`.

---

## Fix

Added explicit clears in both project lifecycle entry points and exposed an `AssetManager::Clear()` helper for the call sites.

- `Hamster-Core/src/Utils/AssetManager.h` — declared `void Clear();`.
- `Hamster-Core/src/Utils/AssetManager.cpp` — defined `Clear()` to clear `m_Textures`, `m_Scripts`, `m_Animations`.
- `Hamster-Core/src/Core/Project.cpp` — `Project::Open` calls `assetManager->Clear()` after saving the previous project but before `Deserialise`. `Project::New` calls `assetManager->Clear()` at the same point.

## Verification

- [x] Build clean.
- [x] Smoke test passes.
- [ ] Manual: open project A, open project B from File menu — project B's asset browser shows only project B's assets.

---

## Related

- Bug 0002: family member — same "ownership not cleaned up on lifecycle event" theme.
- Bug 0003: triggered the failed `Open` that surfaced this leak.
