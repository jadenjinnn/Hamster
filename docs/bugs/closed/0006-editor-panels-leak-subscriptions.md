# Bug 0006: EditorLayer + panels never unsubscribe from the event dispatcher

> Status: **fix-implemented**
> Severity: **Critical**
> Tier: **1**
> Logged: 2026-05-16
> Found while: manual verification of bug 0002 fix

---

## Symptom

After bug 0002's fix made `PopLayer` actually `delete` the popped layer, switching projects A → B → A crashed the editor silently (no exception, no error dialog — process exits).

Sequence:
1. Open project A from hub. `EditorLayer #1` is constructed and pushed. Its panels (`Hierarchy`, `PropertyEditor`, `Console`, `AssetBrowser`, `AnimationPanel`) each `Subscribe` to `ActiveSceneChanged`. `EditorLayer` itself subscribes to `ActiveSceneChanged` and `FramebufferResize`.
2. `File → Open Project` → project B. `Project::Open` posts `ActiveSceneChanged`, then `ProjectOpened`. The handler in `main.cpp` pops `EditorLayer #1` (now deleted) and pushes `EditorLayer #2`. Before deletion, none of `EditorLayer #1`'s subscriptions are removed.
3. `File → Open Project` → project A again. `Project::Open` posts `ActiveSceneChanged`. Dispatch iterates the subscriber list, which still contains 7 stale lambdas captured against the deleted `EditorLayer #1` (and its deleted panels). Dereferencing the captured `this` is UB → silent crash.

## Suspected location

- `Hamster-Wheel/src/EditorLayer.cpp:47–53` — two `Subscribe` calls, return values discarded.
- `Hamster-Wheel/src/Panels/Hierarchy.cpp:19`, `PropertyEditor.cpp:24`, `Console.cpp:10`, `AssetBrowser.cpp:21`, `AnimationPanel.cpp:22` — same pattern.

`HamsterBehaviour` already follows the correct pattern (captures `SubscriptionHandle`, unsubscribes in destructor — see `docs/architecture.md`). The editor-side code was just never updated to do the same.

## Reproduction

1. Build and run `Hamster-Wheel.exe`.
2. Create two clean projects, `test1` and `test2`.
3. Open `test1` from the hub. From the editor, `File → Open Project` → pick `test2`. From `test2`'s editor, `File → Open Project` → pick `test1`. Observe: the process exits with no error.

(Without the bug 0002 fix, the same sequence leaks layers instead of crashing.)

---

## Root cause

`Subscribe` returns a `SubscriptionHandle` that the caller is supposed to hold and pass to `Unsubscribe` at end-of-life. `EditorLayer` and its panels discard the handles and have no destructors, so their dispatcher callbacks outlive the objects they capture. With bug 0002's fix in place, the layers are actually destroyed on project switch, turning the latent leak into a use-after-free.

## What would have prevented this

A `SubscriptionGuard` RAII type (similar to `std::lock_guard` for the dispatcher) returned from `Subscribe` would have made discarding the handle structurally impossible. Storing it as a member would have auto-unsubscribed on destruction.

---

## Fix

For each of the 6 sites, add a `Hamster::SubscriptionHandle m_ActiveSceneSub` member (and `m_FramebufferSub` in EditorLayer's case), capture the return of `Subscribe`, and add a destructor that calls `m_Dispatcher->Unsubscribe(...)` with the stored handle.

Files touched:
- `Hamster-Wheel/src/EditorLayer.h` / `.cpp` — `~EditorLayer()` added; two handles stored.
- `Hamster-Wheel/src/Panels/Hierarchy.h` / `.cpp` — `~Hierarchy()` added; one handle stored.
- `Hamster-Wheel/src/Panels/PropertyEditor.h` / `.cpp` — same.
- `Hamster-Wheel/src/Panels/Console.h` / `.cpp` — same.
- `Hamster-Wheel/src/Panels/AssetBrowser.h` / `.cpp` — same.
- `Hamster-Wheel/src/Panels/AnimationPanel.h` / `.cpp` — same.

## Verification

- [x] Build clean.
- [x] Smoke test passes.
- [ ] Manual: cycle project A → B → A → B at least 4 times; observe no crash and stable kernel-handle count.

---

## Related

- Bug 0002: this bug is *only* observable because 0002's fix actually destroys the popped layer. Same family — "ownership/lifecycle not respected for stack-pushed resources".
