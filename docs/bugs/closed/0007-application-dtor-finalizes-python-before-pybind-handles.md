# Bug 0007: Application destructor finalizes Python before destroying pybind11 handles

> Status: **fixed**
> Severity: **High**
> Tier: **1**
> Logged: 2026-05-17
> Found while: asset-sidecars feature, Phase 7 close-out (extended smoke test surfaced the latent UB)

---

## Symptom

Smoke test process segfaults at exit after all assertions print PASS. The crash always happens between the test body's `return 0;` and the OS reaping the process.

## Suspected location

`Hamster-Core/src/Core/Application.cpp` — `Application::~Application`.

## Reproduction

1. Build the project (`cmake --build build -j 8`).
2. Run `ctest --test-dir build -R SmokeTest -V`.
3. Before the fix: every test case prints PASS, then ctest reports `Exception: SegFault` and the run is marked failed. The crash reproduces on every run with the extended smoke test (asset-sidecars Phase 7).

---

## Root cause

`Application::~Application` called `Scripting::FinaliseInterpreter()` as the very first line of its body. That call invokes `pybind11::finalize_interpreter()`, after which the Python interpreter no longer exists.

When the destructor body finishes, C++ then runs member destructors in reverse declaration order. Two of those members hold pybind11 handles that need a live interpreter to decref correctly:

- `m_AssetManager` (unique_ptr<AssetManager>) — owns `m_Scripts`, a map of `shared_ptr<HamsterScript>`. Each HamsterScript holds a `pybind11::module_` (`m_Module`) and a `std::vector<pybind11::handle>` (`m_PyObjects`). Destroying the script decrefs both.
- `m_Scenes` (map<UUID, shared_ptr<Scene>>) — each Scene's EnTT registry holds `Behaviour` components, and each Behaviour holds `std::vector<pybind11::object> pyObjects`.

Decrefing a Python object after `Py_Finalize` is undefined behaviour. The old test sometimes worked because UB sometimes doesn't crash — only one HamsterScript was created and the timing happened to be benign. The Phase 7 smoke extension creates three additional HamsterScripts (and replaces one via `SetScriptPath` triggering a re-import), which was enough to push the UB into consistent segfaults.

## Fix

`Hamster-Core/src/Core/Application.cpp` — reorder the destructor:

1. Move `Scripting::FinaliseInterpreter()` to the END of the destructor body.
2. Before finalizing, explicitly `m_Scenes.clear()` and `m_AssetManager.reset()` so all pybind11-holding objects are destroyed while the interpreter is still alive.

The rest of the dtor (save project, save scenes, pop layers) stays in the same order — those run before the explicit teardown of Python-holding members.

## Verification

Re-ran `ctest --test-dir build -R SmokeTest -V`. All 15 PASS lines print, "Application destroyed" prints, the process exits 0, ctest reports `100% tests passed`.

## What would have prevented this

Calling `finalize_interpreter` as the first dtor line was strictly out of order — any future member that holds Python state would have been a latent crash. A general rule: when teardown order matters (Python interpreter, OpenGL context, audio context), the highest-level "shut everything down" call should be the LAST thing the destructor body runs, with members holding state explicitly released just before it.
