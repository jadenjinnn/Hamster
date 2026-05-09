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

## Flag workarounds

| Flag | Why |
|------|-----|
| `/EHsc` | clang-cl defaults to no C++ exceptions; pybind11 requires them |
| `-Wno-unused-command-line-argument` | Box2D's CMake passes `/experimental:c11atomics` which clang-cl doesn't recognize |

## Known warnings

- `strcpy` deprecation in `RenameModal.h` — harmless, will be cleaned up in Phase 4.
