# Bug NNNN: <short title>

> Status: **open** | **investigating** | **fix-proposed** | **fixed**
> Severity: **Critical** | **High** | **Medium** | **Low**
> Tier: **1** | **2** | **3** (assigned at fix time, leave blank at log time)
> Logged: YYYY-MM-DD
> Found while: <task / feature / "incidental">

---

## Symptom

<!-- What is observably wrong? Be precise.
Bad: "Entities don't show up."
Good: "In the scene editor, after creating an entity via the entity panel,
the entity appears in the hierarchy tree but does not render in the viewport.
Other ImGui windows render fine. No console errors." -->

## Suspected location

<!-- File path, function, line if known. "Unknown" is a valid entry.
This is a starting hypothesis, not a commitment. -->

## Reproduction

<!-- Required for Tier 2/3 before fix. Optional for Tier 1.
Steps that someone (you, future you, Claude) can run cold and observe the bug.

Example:
1. `cmake --build build`
2. Run `build/Hamster-Wheel.exe`
3. Open project at `examples/sandbox/`
4. Click "Add Entity" in entity panel
5. Observe: entity in hierarchy, nothing in viewport
6. Expected: entity visible in viewport at default position

If repro requires specific data or assets, link them. -->

---

## Investigation (Tier 2/3 only)

<!-- Filled in during /bug-fix. Document what was checked, what was ruled out,
what evidence pointed to the actual cause. Brief — bullets are fine. -->

### Hypotheses considered

<!-- - Hypothesis A: ___. Ruled out because ___.
- Hypothesis B: ___. Confirmed because ___. -->

### Evidence

<!-- What did `git log` show? What did logs/debugger show?
What test isolated the cause? -->

---

## Root cause

<!-- One paragraph. WHY does this bug happen?
"The X function calls Y before Z is initialized, because the constructor
order in MainApp.cpp put Z after the panel that uses it."
Not "X is broken." -->

## What would have prevented this (Tier 2/3 only)

<!-- One sentence, drafted by Claude from the root cause. Postmortem value;
the author may edit but it does not gate close-out.

Examples of what makes a good answer:
- "An init-order assertion in Renderer::Begin would have caught this at startup."
- "A smoke test that creates an entity and checks the renderer's draw count
   would have caught this in CI."
- "A clearer ownership model — Renderer should own the FrameAllocator, not
   borrow it — would have made this impossible to write." -->

---

## Fix

<!-- What changed? File paths and brief description.
- src/render/Renderer.cpp:142 — added FlushBatch() call before viewport bind
- src/render/Renderer.h — exposed FlushBatch as public for editor integration -->

## Verification

<!-- Required at close-out. Repro case re-run after fix.
- Repro steps run on YYYY-MM-DD: PASS / FAIL
- Smoke test result: PASS / FAIL / N/A
- Any new test added: <test name and what it covers> -->

---

## Related

<!-- Other bug IDs, feature specs, or commits that relate to this. Optional. -->
