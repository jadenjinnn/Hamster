# Bug workflow

This doc governs how bugs are logged, investigated, and fixed. It runs alongside `feature-workflow.md` — features and bugs share the codebase but have different shapes:

| | Features | Bugs |
|---|---|---|
| Starting question | "What should this do?" | "Why is it doing the wrong thing?" |
| Risk | Building wrong thing | Fixing the wrong thing (treating symptom not cause) |
| Up-front discipline | Spec first | Reproduce first |
| Done criteria | Behaves per spec | Repro case passes + no regression |

## Storage

- `docs/bugs/active/<id>-<slug>.md` — open bugs.
- `docs/bugs/closed/<id>-<slug>.md` — fixed bugs (moved on close-out).

IDs are sequential 4-digit numbers across active+closed: `0001`, `0002`, etc. Slugs are short kebab-case descriptors: `0007-entities-not-rendering.md`.

## Severity

Logged at capture time, refinable later. Used for ordering and triage.

- **Critical** — crashes, data loss, blocks any further development on the project. Drop everything.
- **High** — feature is broken or unusable, but workarounds exist. Fix in current cycle.
- **Medium** — works but wrong; visible to users. Fix when convenient.
- **Low** — cosmetic, minor, or only affects edge cases. Sweep eventually.

When unsure, guess and move on. Severity is reviewed when the bug is picked up for fixing.

## Tiers (assigned at fix time, not capture time)

- **Tier 1 — Quick fix.** Cause is obvious, fix is local, <30 minutes. No investigation doc; the bug file itself gets updated with cause + fix in a few lines. Reproduction case is optional but encouraged.
- **Tier 2 — Investigation.** Cause unclear or fix non-obvious. Full process: reproduce → root cause → propose fix → fix → verify. Reproduction case is required.
- **Tier 3 — Architectural.** The bug exposes a design flaw. Stop. Don't patch. Write up findings, escalate to refactor planning or a feature spec. Patches that ignore the design make things worse. Reproduction case is required.

## Hard rules

1. **Tier 2/3 require a reliable reproduction before fix.** "Reliable" means runnable now, observably triggers the bug. No reproduction → no fix attempted → either build a reproduction first, or downgrade to Tier 1 and accept the risk.
2. **Identify root cause, not symptom.** Before any fix, the bug file's "Root cause" section must be filled in. If the cause is genuinely unknown, the bug stays in investigation phase — fixing without a known cause is guessing.
3. **The fix verifies against the original repro case.** Close-out includes re-running the documented reproduction and confirming it no longer triggers the bug.
4. **Each fix or related-fix-batch is its own commit.** No mixing bug fixes into feature commits. Batching multiple related bugs in one commit is fine if they share a root cause.
5. **For Tier 2/3, the author writes a "What would have prevented this" line in their own words.** One sentence. This is a counter-rubber-stamp check: if you can't answer it, you didn't understand the bug.

## When found mid-task

See "Bugs found mid-task" in global `~/.claude/CLAUDE.md`. Default flow:

1. `/bug-log` to capture. Severity guess required, no other interruption.
2. If suspect file is known and obvious, leave a `// TODO(bug:NNNN): ...` comment.
3. Return to original task.

Do not switch to fixing the bug unless severity is Critical and it actually blocks the current task. "Critical that doesn't block current task" still goes in active/ and gets fixed next.

## Anti-patterns Claude should refuse or push back on

- **Symptom patches.** "Add a null check" without explaining why the value is null. The null is a clue, not the bug.
- **"Probably this"** fixes. Tier 2/3 fixes without confirmed root cause are guesses. Push back, ask for evidence.
- **Test-as-an-afterthought.** Bug-fixing without considering whether the smoke test or other tests should catch this in the future. The fix should make recurrence detectable.
- **Scope expansion.** While fixing bug A, finding and fixing bug B inline. B gets logged separately — even if it's "just one line."
- **Closing without verifying.** Marking a bug fixed without actually re-running the reproduction case.
