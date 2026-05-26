# Feature spec: Windows installer (bundled redistributable)

> Tier: **Design**
> Status: **shipped** (2026-05-26 — clean-VM verified)
> Started: 2026-05-25
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: Almost entirely additive build/packaging tooling (an Inno Setup script, an assembly script, a Release build, bundled embeddable-Python files) with at most two tiny code touches, but it spans the build system + Python runtime layout + resource layout + MSVC runtime + installer toolchain — so the design value is making those pieces line up on a clean machine, not API depth.

---

## Problem

Hamster only runs on the author's dev box: the editor's embedded interpreter is linked against a specific Python install (`C:/Users/Jaden/AppData/.../Python311`), resources are read from the build tree, and the MSVC runtime is assumed present. A teacher or student can't install and run it. The keystone deliverable (per `CLAUDE.md` Phase 6) is a **standalone Windows installer**: double-click, install, launch the editor, make a game, press Play — on a machine that has never had Python or Visual Studio.

## In scope

- A **Windows installer** (Inno Setup `.iss`) that installs the **editor** to Program Files (per-machine) or `%LOCALAPPDATA%` (per-user, no admin), with a Start-menu shortcut and an uninstaller.
- **Bundled Python** via CPython's official **Windows embeddable package** (`python311.dll`, `python311.zip` stdlib, `python311._pth`) placed next to `Hamster-Wheel.exe` so the interpreter self-locates and runs isolated.
- **Bundled MSVC runtime** (app-local `vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`; UCRT assumed present on Win10+) so the clang-cl-built exe launches with no Visual Studio installed.
- A **Release** build configuration + a **packaging assembly script** (`Scripts/package.ps1`) that stages the exact install layout and invokes Inno Setup (`iscc`).
- The install tree reproduces the build's `<exe>/../share/Resources/...` layout (so existing relative resource paths resolve) and includes `Resources/Packages/Hamster.cp311-win_amd64.pyd` (so new projects get the module).
- Two small code touches to avoid writing into the (read-only) install dir: point ImGui's `imgui.ini` to `%APPDATA%/Hamster/`, and default new-project creation to a user-writable location (e.g. `Documents`).
- `docs/build.md` gains the Release + packaging recipe; the Inno Setup dependency is documented.

## Out of scope

- **Game export / standalone player** — shipping a student's finished game as its own double-clickable `.exe`. The runtime-only player doesn't exist yet; this is a separate, larger feature.
- **Code signing / Authenticode** — unsigned installer will trip SmartScreen (noted in future work).
- **Auto-update.**
- **macOS / Linux packaging.**
- **MSI / WiX / enterprise (GPO) deployment.**
- **pip / third-party Python packages** — the embeddable runtime is stdlib-only by design.

## API sketch

No runtime Python/C++ API — this is build + deploy tooling. The "interface" is the developer's packaging command and the resulting install tree.

```powershell
# Developer: produce the installer
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ... -B build-release -S .
cmake --build build-release --target Hamster-Wheel -j 8
.\Scripts\package.ps1   # stages the layout below, then runs iscc Hamster.iss
# → dist\HamsterSetup.exe
```

```text
# Installed layout (what makes it run with nothing pre-installed):
<InstallRoot>\
  Hamster-Wheel\
    Hamster-Wheel.exe
    python311.dll          # CPython embeddable
    python311.zip          # stdlib
    python311._pth         # → isolated, self-locating stdlib
    vcruntime140.dll  vcruntime140_1.dll  msvcp140.dll   # app-local MSVC runtime
  share\Resources\Hamster-Wheel\Resources\
    Fonts\  Icons\  Logo\  Sprites\  DefaultShaders\
    Packages\Hamster.cp311-win_amd64.pyd   # copied into each new project
```

---

## Design

### Data structures

No new C++ types. New repo artifacts:
- `Hamster.iss` — Inno Setup script (install layout, shortcut, uninstaller, per-user/per-machine choice).
- `Scripts/package.ps1` — assembles the staging dir from `build-release/` + a fetched embeddable-Python folder + the VC runtime DLLs, asserts the manifest, then runs `iscc`.
- Vendored or fetched: the Python 3.11.x **embeddable** zip (pinned to match the dev interpreter's minor version and the `.pyd`'s `cp311` ABI).

### Module touchpoints

- **NEW** `Hamster.iss`, `Scripts/package.ps1`.
- `docs/build.md` — Release + packaging recipe; document the Inno Setup (`iscc`) dependency.
- `Hamster-Wheel/src/main.cpp` (or wherever ImGui init lives) — set `io.IniFilename` to `%APPDATA%/Hamster/imgui.ini` (one line; the install dir is read-only for non-admins).
- `Hamster-Wheel/src/Panels/CreateProjectModal.cpp` — default new-project directory to a user-writable path (`Documents`), not `C:\Projects\...` or anything under the install dir.
- `CMakeLists.txt` — *optional* `install()` target; preferred approach keeps assembly in `package.ps1` so no CMake change is needed (CMake edits require separate approval anyway).
- **No change** to interpreter bootstrap: the adjacent `python311.dll` + `._pth` is self-locating, so `Scripting::InitInterpreter`'s `pybind11::initialize_interpreter()` works unchanged.

### Lifecycle / control flow

- **Build time:** configure Release → build `Hamster-Wheel.exe` + `Hamster.cp311-win_amd64.pyd` → `package.ps1` stages the install tree (copies exe, the freshly-built `.pyd`, the embeddable Python, app-local VC DLLs, and the `share/Resources` tree) → `iscc Hamster.iss` → `dist/HamsterSetup.exe`.
- **Install time:** user runs `HamsterSetup.exe` → chooses per-user or per-machine → files land under the install root → Start-menu shortcut + uninstaller registered.
- **Runtime (installed):** launch → OS loads the adjacent `python311.dll` → `._pth` points the interpreter at `python311.zip` in isolated mode → `ProjectRegistry` reads `%APPDATA%/Hamster/projects.json` → user creates/opens a project under `Documents` → `CreateProjectModal` copies the `.pyd` from the install's `Resources/Packages` into the project → Play loads user scripts (project dir added to `sys.path` by `AddPathToPy`, unchanged).

### Edge cases

- **Fresh machine — no Python, no Visual Studio:** must launch and Play. Covered by adjacent embeddable Python + app-local VC runtime; UCRT is an OS component on Win10+.
- **Install dir is read-only (Program Files, non-admin):** the app must never write there. `imgui.ini` → `%APPDATA%`; projects default to `Documents`; `ProjectRegistry` already lives in `%APPDATA%`.
- **Target machine *has* a different Python installed:** the adjacent `python311.dll` (same dir as the exe) must win the loader search, and `._pth` isolated mode must ignore `PYTHONHOME`/registry. Explicitly verify on a Python-having machine — no interpreter hijack.
- **Python minor-version drift:** the `.pyd` is `cp311`; the embeddable must be 3.11.x. The packaging script copies the just-built `.pyd` (never a stale one) and the pinned 3.11.x embeddable.
- **No admin rights:** Inno Setup `PrivilegesRequiredOverridesAllowed` → per-user install to `%LOCALAPPDATA%`.
- **Resource tree misplaced by one folder:** fonts/shaders/logo fail to load → blank/garbage UI. The assembly script reproduces the exact relative layout and the manifest check fails the build if a file is missing.

---

## Why this approach

- **Embeddable package + `._pth`** over setting `PyConfig.home` at startup: it's the CPython distribution built for embedding, self-locates relative to its DLL, runs isolated (kills the system-Python-hijack risk), and needs **zero** interpreter-bootstrap code changes. Tradeoff accepted: it's stripped (no pip / site-packages) — exactly what we want for a locked-down classroom runtime.
- **Inno Setup** over WiX/MSI: far simpler script, supports per-user (no-admin) installs, gives shortcuts + uninstaller. Over a portable zip: the author wants a real install experience (discoverability, Start-menu, clean uninstall).
- **App-local VC runtime DLLs** over static `/MT` linking: `python311.dll` is built against the dynamic UCRT; static-linking the app's CRT risks a mixed-CRT heap/FILE* mismatch across the boundary. App-local DLLs are Microsoft-redistributable and need no prerequisite installer on Win10+. (Chaining `vc_redist.x64.exe` is the fallback if app-local proves insufficient on some target.)

## Risks / what could go wrong

1. **Interpreter hijack** — on a machine with Python 3.11 already on `PATH`/SxS, the loader could grab the wrong `python311.dll`. Consequence: imports fail or load a foreign stdlib. Mitigation: ship adjacent (same-dir DLL wins normal search) + `._pth` isolated mode; verify explicitly on a Python-having box.
2. **Missing MSVC runtime** — without `vcruntime140*.dll`/`msvcp140.dll` the exe dies at launch with `0xc000007b` / "DLL not found," a confusing failure with no window. Mitigation: app-local the three DLLs; fall back to chaining `vc_redist`. UCRT assumed (Win10+); a Win8.1 target would need the UCRT update — out of scope.
3. **Resource path layout** — the exe reads `<exe>/../share/Resources/...`; one misplaced directory and the UI ships blank or crashes loading a font/shader. Mitigation: assembly script reproduces the build's relative layout byte-for-byte and asserts a file manifest before invoking Inno Setup.
4. **`.pyd` / Python ABI mismatch** — a stale or wrong-version `.pyd` vs the embeddable runtime → `ImportError`/crash on first `import Hamster` (i.e. on Play). Mitigation: script copies the freshly-built `.pyd`; pin the embeddable to 3.11.x.
5. **Writable-state under the install dir** — if `imgui.ini` or project creation targets the install folder, it silently fails for non-admin users (no layout persistence, can't create a project in Program Files). Mitigation: redirect ImGui ini to `%APPDATA%`; default projects to `Documents`; never write under the install root.
6. **Unsigned binary → SmartScreen** — Windows warns "unrecognized app," which scares non-technical users. Out of scope to fix (no cert); documented so it's a known, expected prompt, not a packaging bug.

## Success criteria

Verified on a **clean Windows 10/11 environment with no Python and no Visual Studio** (a VM, or a fresh user account):

1. `HamsterSetup.exe` installs without error (both per-user and per-machine modes); a Start-menu shortcut launches `Hamster-Wheel`.
2. The editor **renders correctly** — Inter fonts, Font Awesome icons, theme, logo — proving the `share/Resources` tree resolved.
3. **Create a new project** succeeds: it lands in a user-writable location and the `.pyd` is copied into it.
4. **Press Play** on a project whose entity has a trivial script: the simulation runs ≥1 frame with **no Python import error**, proving the bundled interpreter + `python311.zip` stdlib + `Hamster.pyd` all work.
5. Close + reopen the project → persisted state returns (project blob + sidecars readable).
6. The **uninstaller** removes the install dir and shortcuts.
7. On a machine that **does** have a different Python 3.11 installed, the editor still launches and Plays with no interpreter hijack.

## Test extensions required

- **Smoke test: no change.** Packaging is a build/deploy artifact; the in-repo smoke test runs against the dev build and cannot prove a clean-machine install works — the meaningful test ("does it run where nothing is installed") requires a clean environment, which is the manual checklist in Success criteria. This is the one feature where automated in-repo coverage can't substitute for clean-machine verification.
- **New in-repo check (cheap, structural):** `Scripts/package.ps1` asserts a **file manifest** of the staging dir (exe, `python311.dll`, `python311.zip`, `python311._pth`, the three VC DLLs, the full `share/Resources` tree, `Resources/Packages/*.pyd`) and fails the build if anything is missing — catches Risk #3/#2/#4 layout/copy mistakes before an installer is ever produced.

---

## Decisions during implementation

<!-- Append-only, dated, during build. -->

**2026-05-26 — MSVC redist selection by version, not edition name.** Two VS installs are present (18 Community = MSVC 14.51 / VC145, which built the exe; 2022 Enterprise = 14.44 / VC143). A naive "newest edition" sort ranks `"2022" > "18"` lexically and ships the *older* 14.44 `msvcp140.dll` — a downgrade below the toolset, the exact Risk #2 mixed-runtime footgun. `package.ps1` parses each redist's version folder as `[version]` and picks the max, and overwrites the two `vcruntime140*.dll` the embeddable bundles (newer vcruntime stays forward-compatible for `python311.dll`). Resolves to 14.51.

**2026-05-26 — Embeddable fetched + SHA256-pinned, not vendored.** `package.ps1` downloads `python-3.11.9-embed-amd64.zip` into a gitignored `Scripts/.cache/` and verifies `sha256=009D6BF7...CFD3B`; keeps a ~10 MB binary out of git. `dist/` and `Scripts/.cache/` added to `.gitignore`.

**2026-05-26 — `python311._pth` kept isolated (`python311.zip` + `.`, site disabled).** The embeddable default already does what we need; runtime `sys.path.append` (`AddPathToPy`) still adds the project dir on top in isolated mode, so no interpreter-bootstrap change was needed. Standalone `python.exe`/`pythonw.exe` launchers are stripped from the staged dir.

**2026-05-26 — `package.ps1` and `Hamster.iss` are ASCII-only.** Windows PowerShell 5.1 reads BOM-less `.ps1` as ANSI and ISCC reads `.iss` as ANSI; non-ASCII punctuation mis-decodes and breaks parsing (hit during build with em-dashes).

**2026-05-26 — ImGui ini path held in a `static std::string`.** `io.IniFilename` stores the `const char*` by pointer without copying, so the backing string must outlive the context; redirected to `%APPDATA%/Hamster/imgui.ini` (dir created on attach). New projects default to `%USERPROFILE%\Documents`.

**2026-05-26 — Windowed (no-console) build + file logging (added mid-implementation at author request).** The exe was a console-subsystem app (terminal pops on launch). Switched `Hamster-Wheel` to `/SUBSYSTEM:WINDOWS` with `/ENTRY:mainCRTStartup` (keeps `main()`, no `WinMain`) via `target_link_options(... "LINKER:/SUBSYSTEM:WINDOWS" "LINKER:/ENTRY:mainCRTStartup")` — the `LINKER:` prefix is needed so the flags reach the linker through the clang-cl driver. Because a windowed app has no console, `main.cpp` now redirects `std::cout`/`std::cerr` to `%APPDATA%/Hamster/log.txt` (static `ofstream` so the buffer outlives all logging incl. static destruction). Verified the built + staged exe report PE subsystem 2 (GUI) and that the log file captures startup output. This was the first non-tooling code/CMake change beyond the two the spec named; folded in here rather than deferred since a terminal-flashing installer isn't shippable.

## Spec amendments

<!-- Append-only, dated, when the spec turns out wrong. -->

**2026-05-26 — Shader files are runtime resources (found during VM verification).** The installer ran and rendered the UI in the VM, but the renderer threw "Shader file could not be successfully read": `Renderer::InitRendererData` loads its five shader pairs from `HAMSTER_CORE_SRC_DIR + "/Renderer/DefaultShaders/..."`, a compile-time **absolute dev-machine source path** that doesn't exist on an installed box. The spec (and architecture.md) wrongly assumed shaders were built-in / already under `share/Resources`. Fix: (1) `Renderer::InitRendererData` now resolves the shader dir exe-relatively (`<exe>/../share/Resources/Hamster-Core/Renderer/DefaultShaders`) with the `HAMSTER_CORE_SRC_DIR` source fallback for dev/in-place runs — mirroring the adjacent FontAtlas path logic; (2) `package.ps1` stages the 10 shader files into that share path and the manifest asserts all 10 (it had been scoped to the Hamster-Wheel resource tree only, which is why Risk #3's manifest didn't catch this); (3) architecture.md corrected — shaders are file-based, and the "all resource paths use GetExecutablePath" claim was incomplete (`HAMSTER_CORE_SRC_DIR` survived for shaders). Verified: staged exe loads shaders with no error; dev build still works via the fallback; smoke 34/34.

---

## Future work (out-of-scope ideas surfaced during this feature)

- **Code signing** (Authenticode cert) to avoid the SmartScreen "unrecognized app" prompt.
- **Game export** — a standalone runtime player so a student's finished game ships as its own `.exe` (depends on building the runtime-only player first).
- **CI packaging job** — build the installer automatically on a tagged release.
- **Versioned/portable serialization** before wide distribution (already logged in architecture "Known smells") so projects made in one Hamster version open in the next.
