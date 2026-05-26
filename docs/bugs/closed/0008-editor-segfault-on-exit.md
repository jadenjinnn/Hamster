# Bug 0008: editor segfaults on exit (post-Application destructor)

> Status: **fixed**
> Severity: **Medium** (raised from Low — it caused bug 0016's data loss)
> Tier: **2**
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

Two independent teardown-order faults, both producing SIGSEGV:

1. **Dead GL context during layer shutdown.** `Application::Close` (the window-close handler) called `m_Window.reset()` immediately, which runs `glfwDestroyWindow` + `glfwTerminate`. The destructor then pops the layer stack — `ImGuiLayer::OnDetach` runs `ImGui_ImplOpenGL3_Shutdown` / `ImGui_ImplGlfw_Shutdown` — on an already-terminated GLFW/dead GL context.

2. **Dead interpreter during scene teardown (same class as bug 0007).** The destructor cleared `m_Scenes` and finalised Python, but `m_ActiveScene` is a *second* `shared_ptr<Scene>`, and the static `Project::s_ActiveProject` holds the start-scene plus the file-watcher thread. Those refs outlived `FinaliseInterpreter()` and were released during implicit/static destruction, decref-ing `Behaviour::pyObjects` on a dead interpreter (and the watcher thread touched a freed AssetManager). Bug 0007 fixed `m_Scenes` but missed these holders — exactly the "less-obvious holder" its notes predicted.

## What would have prevented this

Treating "release everything that holds a GL or Python handle, in dependency order, before terminating GLFW / finalising Python" as an explicit, single owned step (with the duplicate scene `shared_ptr`s accounted for) rather than leaning on implicit/static destruction order.

---

## Fix

`Hamster-Core/src/Core/Application.cpp` + `Project.{h,cpp}`:
- `Application::Close` no longer destroys the window; the window now outlives the destructor's layer-pop so ImGui/GL shutdown runs on a live context, and the window is destroyed last via implicit member destruction.
- The destructor resets `m_ActiveScene` and calls the new `Project::Close()` (which resets `s_ActiveProject`, stopping the watcher thread and releasing the start-scene) **before** `FinaliseInterpreter()` and `m_AssetManager.reset()`, so all scene/script teardown happens with a live interpreter and a live AssetManager.

## Verification

- Smoke test PASS — it constructs an `Application`, runs script scenarios (creating `Behaviour::pyObjects`), and tears down at exit; a clean pass exercises the interpreter-finalise ordering with no crash.
- Editor: open a project, Play/Stop, close the window → process exits 0 (no 139). Confirmed by author.

---

## Related

- Bug 0007 (closed) — Application destructor finalized Python before pybind
  member dtors ran. Same class of issue. Possible the current crash is a
  remaining instance of the same pattern in a less-obvious holder.
