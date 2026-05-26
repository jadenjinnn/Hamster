# Build and run

## Toolchain

| Tool       | Version  | Path / Notes |
|------------|----------|--------------|
| Compiler   | clang-cl 19.1.7 (LLVM) | `C:/Program Files/LLVM/bin/clang-cl.exe` — targets MSVC ABI |
| CMake      | 3.31.5   | |
| Generator  | Ninja    | |
| Python     | 3.11.9   | `C:/Users/Jaden/AppData/Local/Programs/Python/Python311` |
| pybind11   | vendored | `Hamster-Py/Vendor/pybind11/` |

## Dependencies

All vendored under `Hamster-Core/Vendor/` and `Hamster-Wheel/Vendor/`:
- GLFW, GLAD, GLM, ImGui, EnTT, Box2D, Boost (UUID only), stb_image, tinyfiledialogs

No package manager needed — everything builds from source via CMake.

## Configure

```powershell
cmake -G "Ninja" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" `
  -DPython_ROOT_DIR="C:/Users/Jaden/AppData/Local/Programs/Python/Python311" `
  -DCMAKE_C_FLAGS="-Wno-unused-command-line-argument" `
  -DCMAKE_CXX_FLAGS="/EHsc -Wno-unused-command-line-argument" `
  -S C:\Users\Jaden\Hamster `
  -B C:\Users\Jaden\Hamster\build
```

## Build

```powershell
cmake --build C:\Users\Jaden\Hamster\build --target Hamster-Wheel -j 8
```

Output: `build/Hamster-Wheel/Hamster-Wheel.exe`

## Run

```powershell
.\build\Hamster-Wheel\Hamster-Wheel.exe
```

Requires a project directory with scene files. Phase 3 (smoke test) will verify this end-to-end.

The editor is a **windowed app** (`/SUBSYSTEM:WINDOWS`) — no console window. Diagnostics (`std::cout`/`std::cerr`) are redirected to **`%APPDATA%/Hamster/log.txt`** (truncated each launch). Check that file when debugging a launch/runtime issue.

## Packaging (standalone installer)

Produces a redistributable that runs on a machine with **no Python and no Visual Studio**. See `docs/features/shipped/*-windows-installer.md` for the design.

### Prerequisites

- **Inno Setup 6** (`iscc.exe`) — https://jrsoftware.org/isdl.php. Only needed for the `-MakeInstaller` step.
- Internet access on first packaging run (fetches the pinned Python 3.11.9 embeddable into `Scripts/.cache/`, SHA256-verified).

### Release build

Same as the Configure recipe above but `-DCMAKE_BUILD_TYPE=Release` into a separate dir:

```powershell
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" `
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" `
  -DPython_ROOT_DIR="C:/Users/Jaden/AppData/Local/Programs/Python/Python311" `
  -DCMAKE_C_FLAGS="-Wno-unused-command-line-argument" `
  -DCMAKE_CXX_FLAGS="/EHsc -Wno-unused-command-line-argument" `
  -S C:\Users\Jaden\Hamster -B C:\Users\Jaden\Hamster\build-release

cmake --build C:\Users\Jaden\Hamster\build-release --target Hamster-Wheel Hamster -j 8
```

`Hamster` is the pybind11 module target (emits `Hamster.cp311-win_amd64.pyd`).

### Stage + build the installer

```powershell
.\Scripts\package.ps1                  # stage dist\staging\ + assert manifest
.\Scripts\package.ps1 -Run             # also launch the staged exe (smoke)
.\Scripts\package.ps1 -MakeInstaller   # stage, then iscc -> dist\HamsterSetup.exe
```

`package.ps1` stages the exact install layout (exe + bundled embeddable Python + app-local MSVC runtime DLLs + the `share/Resources` tree), then `Hamster.iss` packs it into `dist\HamsterSetup.exe` (per-user or per-machine; Start-menu shortcut; uninstaller).

> **MSVC runtime note:** the script ships `vcruntime140*.dll` + `msvcp140.dll` from the **highest-versioned** VS redist found (must be ≥ the toolset that built the exe — currently 14.51), overwriting the older copies the embeddable bundles. A lexical "newest edition" pick would wrongly grab an older redist.

## Flag workarounds

| Flag | Why |
|------|-----|
| `/EHsc` | clang-cl defaults to no C++ exceptions; pybind11 requires them |
| `-Wno-unused-command-line-argument` | Box2D's CMake passes `/experimental:c11atomics` which clang-cl doesn't recognize |

## Known warnings

- `strcpy` deprecation in `RenameModal.h` — harmless, will be cleaned up in Phase 4.
