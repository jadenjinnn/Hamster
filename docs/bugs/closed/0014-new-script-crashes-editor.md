# Bug 0014: "New Script" crashes the whole editor

> Status: **fixed**
> Severity: **Critical**
> Tier: **1**
> Logged: 2026-05-26
> Found while: windows-installer feature — clean-VM verification

---

## Symptom

Clicking "New Script" (Asset Browser add menu, or PropertyEditor → Add Component → Add Script → New Script) crashes the entire editor. Running from a terminal shows **no Python traceback** — just a silent process exit. First observed in the installed VM build, but reproduces on the dev build too.

## Suspected location

- `Hamster-Core/src/Utils/AssetManager.cpp:197` (`AddDefaultScript`) — passes the bare file stem as the import name.
- `Hamster-Core/src/Scripting/HamsterScript.cpp:11` (ctor) — `pybind11::module_::import(...)` with no try/catch.

## Reproduction

Confirmed via a standalone repro with the bundled interpreter (no GUI needed):
1. Project root on `sys.path` (as the editor does via `Scripting::AddPathToPy`), `Hamster.pyd` in the root, a script at `Assets/Scripts/Untitled_Script.py`.
2. `import Untitled_Script` (bare stem, what the button does) → **`ModuleNotFoundError: No module named 'Untitled_Script'`**.
3. `import Assets.Scripts.Untitled_Script` (dotted, what project-open does) → **OK**.

In the editor, step 2's exception is raised inside the unguarded `HamsterScript` ctor → unhandled `pybind11::error_already_set` → MSVC `terminate` (which prints nothing) → silent crash.

---

## Root cause

The 2026-05-25 `Assets/` restructure (commit `3f024ace`) moved new scripts into `<project>/Assets/Scripts/` and updated project-*open* loading (`AssetManager::LoadProjectScripts`) to import each script by its **dotted namespace-package name** (`Assets.Scripts.Untitled_Script`), since only the project root is on `sys.path`. But the New Script *button* (`AssetManager::AddDefaultScript`) was left passing the **bare file stem** (`Untitled_Script`) as the import target — which is no longer importable from the project root. The bare-stem import raises `ModuleNotFoundError`; because `HamsterScript`'s constructor calls `pybind11::module_::import` with no error handling, the exception propagates unhandled and `std::terminate` kills the editor (MSVC's terminate handler emits no message, hence "no traceback, just a crash").

## What would have prevented this

A smoke-test scenario that exercises the New Script path (mint a default script via `AddDefaultScript` and assert it imports) would have caught the bare-stem/dotted-name divergence the restructure introduced.

---

## Fix

Root-cause fix only (author scoped out the defensive ctor guard — see Note below):
- `Hamster-Core/src/Utils/AssetManager.cpp` — `AddDefaultScript` now derives the import name with `ModuleNameForScript(scriptPath, projectDir)` (the dotted name, `Assets.Scripts.Untitled_Script`), matching `LoadProjectScripts`, instead of the bare `scriptPath.stem()`. The file-local helper was forward-declared since it's defined below `AddDefaultScript`. Display name (`SetName`) still uses the stem.

**Note (not fixed, deliberately):** `HamsterScript`'s ctor (`HamsterScript.cpp:11`) still imports with no try/catch, so any *other* bad user script (e.g. a syntax error) will still hard-crash the editor via the same unhandled-`error_already_set` path. Author chose to fix only the root cause of 0014. A future bug/hardening item could guard the ctor.

## Verification

- Standalone repro (2026-05-26): with project root on `sys.path` and the script under `Assets/Scripts/`, `import Untitled_Script` (bare) → `ModuleNotFoundError`; `import Assets.Scripts.Untitled_Script` (dotted) → OK. The fix makes `AddDefaultScript` use the dotted form.
- Smoke test: new scenario "Assets/Scripts script imports by dotted module name (bug 0014)" added — asserts the bare stem is not importable *and* that `LoadProjectScripts` registers a subdir script via the dotted name. Suite PASS (Debug build).
- Editor symptom (click New Script → no crash): **confirmed fixed** by the author in the dev editor, 2026-05-26.

---

## Related

- Introduced by `3f024ace` (Assets/ restructure). Surfaced during the windows-installer VM verification.
- Same unguarded-import mechanism would turn any bad user script into an editor crash.
