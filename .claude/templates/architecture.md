# Architecture

> Living document. Updated via `/architecture-update`. Source of truth for "how this engine fits together." Read top-to-bottom should give a new engineer (or returning author) enough to navigate the codebase confidently in 10 minutes.

## One-paragraph overview

<!-- What is this engine? What does it do? What's the headline design choice (e.g. "ECS with Python scripting via pybind11, immediate-mode rendering on top of SDL2")? Keep to ~5 sentences. -->

## Entry points

<!-- Where does execution start? List the binary entry point(s) and the Python entry point(s).
Format:
- `src/main.cpp::main` — boots the engine, parses CLI args, hands off to Application
- `scripts/game.py` — example user script, loaded by the engine on startup
-->

## Module map

<!-- Top-level directories or logical modules. One line each.
Format:
- `src/core/` — Application lifecycle, main loop, time, logging
- `src/render/` — 2D rendering abstraction, texture/sprite management
- `src/scripting/` — pybind11 bindings, Python interpreter lifecycle
- `src/ecs/` — entity/component storage and systems
- `src/assets/` — asset loading, serialization
- `bindings/` — pybind11 module definitions
- `scripts/` — example/built-in Python scripts
- `assets/` — runtime assets (textures, configs)
-->

## Main loop

<!-- Pseudocode or bullet list of what happens each frame. Where does Python get called? Where does rendering happen? Order matters here. -->

## The C++ ↔ Python boundary

<!-- This is the highest-risk section. Be specific.
- Which C++ classes are exposed to Python? (list)
- Where are the bindings defined? (file paths)
- How is the Python interpreter created and torn down?
- How are scripts discovered and loaded?
- What's the calling pattern — does C++ call into Python (callbacks/hooks) or does Python drive C++ (script-as-main)?
- Threading: is the GIL held during engine ticks? Any threads that touch Python?
- Object lifetime: who owns what? Any known leaks or shared_ptr/PyObject reference cycles?
-->

## Data flow

<!-- How does data move through the engine in a typical frame?
Example:
1. Input polled in `Application::tick`
2. Input dispatched to ECS InputSystem
3. Python `update()` callbacks invoked per scripted entity
4. ECS systems run (physics, collision, ...)
5. Render system walks renderable components, issues draw calls
6. Frame presented
-->

## Build system

<!-- Pointer to docs/build.md for details. Here, just the shape:
- Top-level CMakeLists.txt at repo root
- Subdirectories: src/, bindings/, [tests/]
- Third-party dependencies and how they're acquired (vendored / FetchContent / find_package / vcpkg / Conan)
- Output artifacts: engine executable, Python module (.pyd on Windows)
-->

## Third-party dependencies

<!-- Name + version + purpose + how it's brought in.
Format:
- pybind11 (vX.Y) — C++/Python bindings — FetchContent
- SDL2 (vX.Y) — windowing/input/rendering — vcpkg
- ...
-->

## Known smells / refactor candidates

<!-- Populated during phase 1 exploration and updated during phase 4. Be concrete: file paths and one-line descriptions.
- `src/render/Renderer.cpp` — 800-line god class, mixes SDL calls with high-level sprite logic
- `bindings/engine.cpp` — bindings defined inline; should be split per-module
-->

## Open questions

<!-- Things Claude or the author is unsure about. Resolve and remove as we go.
- Is `AssetCache` thread-safe? It looks like it assumes single-threaded access but is called from the render thread.
- ...
-->
