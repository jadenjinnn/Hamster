---
description: Run a clean CMake configure + build and report errors. Diagnose only, do not fix.
---

Run a clean CMake configure and build. Report results.

Steps:
1. If `build/` exists, remove it (clean build).
2. Run `cmake -S . -B build` with whatever generator is configured in `docs/build.md`.
3. Run `cmake --build build --config Debug` (or Release if specified).
4. Capture stdout and stderr.

Output format:
- **Status**: SUCCESS / CONFIGURE_FAILED / BUILD_FAILED
- **Errors**: list each as `path/to/file.cpp:line — short error summary`
- **Warnings**: count + first 5
- **Time**: configure + build duration

Do not attempt fixes. Diagnosis only. If the toolchain itself is broken, say so plainly and stop.
