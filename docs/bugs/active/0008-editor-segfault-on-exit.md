# Bug 0008: editor segfaults on exit (post-Application destructor)

> Status: **open**
> Severity: **Low**
> Tier: blank
> Logged: 2026-05-17
> Found while: spatial-index feature manual verification

---

## Symptom

`Hamster-Wheel.exe` exits with code 139 (SIGSEGV) when the user closes the
window after a normal session. The crash happens *after* `Application::~Application`
finishes its body (the "Application destroyed" log line prints) — so during
implicit member destruction, static destructors, or runtime shutdown. No
ImGui error, no GL error logged. Frequent across both debug and release
builds.

User impact: cosmetic — the process is exiting anyway. No data loss, no
UB-affects-other-runs concern. But noisy in console / task notifications
and would tank Windows Error Reporting if telemetry were ever enabled.

## Suspected location

`Hamster-Core/src/Core/Application.cpp` — destructor + downstream static
teardown. Possible suspects:
- A pybind11 `module_` / `handle` member somewhere that wasn't released
  before `FinaliseInterpreter()` (echoes bug 0007's class of issue, but
  in a different holder).
- `glfwTerminate` interaction with the Win32 borderless subclass not being
  removed in `Window::~Window`.
- A static-storage `pybind11::object` somewhere (e.g. in a binding helper)
  that runs after `FinaliseInterpreter`.
- Some `std::function` capturing a dangling reference released during
  unspecified-order static teardown.

## Reproduction

1. `cmake --build build-release --target Hamster-Wheel`
2. Run `build-release/Hamster-Wheel/Hamster-Wheel.exe`
3. Open any project (e.g. `scenetest`).
4. Optionally hit Play, hit Stop.
5. Click the window's X button or File → Exit.
6. Observe: bash reports `Segmentation fault` after process termination.
   Console output ends after the normal shutdown sequence — no last log
   line indicates exactly where it crashes.

Reproduces with no project loaded too (just launch and exit). Sometimes
the crash happens before any "Application destroyed" log; sometimes after.

---

## Investigation (Tier 2/3 only)

<!-- not yet investigated; logged for triage -->

### Hypotheses considered

<!-- - Hypothesis A: latent pybind11 finalize order — similar to bug 0007.
       Not yet ruled in/out. -->

### Evidence

<!-- - Crash appears consistently across debug + release builds. -->

---

## Root cause

<!-- TBD — needs investigation. Filled at fix time. -->

## What would have prevented this (AUTHOR WRITES — Tier 2/3 only)

<!-- AUTHOR: write one sentence in your own words at fix time. -->

---

## Fix

<!-- TBD -->

## Verification

<!-- TBD -->

---

## Related

- Bug 0007 (closed) — Application destructor finalized Python before pybind
  member dtors ran. Same class of issue. Possible the current crash is a
  remaining instance of the same pattern in a less-obvious holder.
