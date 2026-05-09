---
description: Run the smoke test. Report pass/fail. Do not fix anything.
---

Run the smoke test (defined in `tests/smoke/` once it exists).

Output format:
- **Status**: PASS / FAIL / NOT_BUILT
- **Output**: last 30 lines of stdout/stderr if FAIL, else just confirm key lifecycle markers fired (engine init, Python script loaded, frame ran, clean shutdown).
- **Duration**: wall-clock seconds.

If the smoke test does not yet exist, say so and stop. Do not create it from this command — that's done in phase 3 with explicit planning.

Do not attempt fixes if it fails. Diagnosis only.
