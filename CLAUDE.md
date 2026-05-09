# Project: [Hamster] — 2D C++ Game Engine with Python Scripting

## Context for Claude

This is a personal keystone project being revived after ~1 year dormant. The owner is using it as the centerpiece of their SWE resume. Code quality, clarity, and architectural soundness matter more than feature velocity.

The owner has not written C++ since this project was last touched. Treat them as someone fluent in software engineering generally but **rusty on C++, CMake, and the Windows toolchain specifically**. When using non-obvious C++ idioms, build flags, or CMake constructs, add a one-line "why" — not a tutorial, just enough to re-anchor.

## Communication style

- Concise. Brief reasoning only for non-trivial choices.
- No filler ("Great question!", "I'll now…"). Just do the thing or ask.
- When you finish a unit of work, end with a one-line status + the next proposed step. Wait for approval before continuing past phase boundaries.

## Autonomy

**Plan-first, ask before edits.** For any change beyond a one-line fix:

1. State the plan (files touched, approach, risks)
2. Wait for "go"
3. Execute
4. Report diff summary

You may freely: read files, run read-only commands, run the build, run tests, search the codebase, write to `docs/` and `.claude/`.

You may not without approval: modify source files, change `CMakeLists.txt`, add dependencies, change directory structure, run anything that mutates git history.

## Tech stack

- Language: C++ (engine), Python (scripting), pybind11 (binding layer)
- Build: CMake
- Platform: Windows (native). Previously built on Windows then Linux. Currently no working toolchain installed.
- Tests: none currently. We will add a smoke test before any refactor.
- Target deliverable: standalone redistributable (installer or zip) for Windows.

## Phases

We are working through these phases in order. Do not skip ahead without explicit approval.

1. **Architecture reconstruction** — explore the codebase, build `docs/architecture.md` (see template in `.claude/templates/`). Heavy collaboration expected — surface uncertainties, ask questions, don't paper over gaps.
2. **Build on Windows** — get a clean build from a fresh toolchain install. Heavy collaboration expected on toolchain choices.
3. **Smoke test** — minimal test that exercises the C++→Python boundary and runs at least one frame. Discuss approach before implementing.
4. **Refactor** — propose priorities based on what was learned in phase 1, execute incrementally with the smoke test as a guardrail.
5. **Features** — driven by `docs/feature-workflow.md`. Use `/feature` to start implement` once approved.
6. **Packaging** — Windows redistributable. Largely execution-mode.

Pushback intensity decreases down this list. Phase 1 is the most collaborative; phase 6 is mostly execution.

## Architecture

@docs/architecture.md

## Build and run

@docs/build.md

## Conventions

@docs/conventions.md

## Personal/machine-specific notes

@CLAUDE.local.md

## Feature workflow (Phase 5+)

@docs/feature-workflow.md

## Bug workflow

@docs/bug-workflow.md