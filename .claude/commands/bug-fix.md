---
description: Investigate and fix a logged bug. Reproduce-first discipline.
---

I want to fix a logged bug. Follow `docs/bug-workflow.md`.

## Step 1: Pick the bug

If I named a bug ID, use it. Otherwise list `docs/bugs/active/` ordered by severity (Critical → High → Medium → Low), then ask which one to fix.

Read the full bug file before doing anything else.

## Step 2: Triage tier

Based on the bug file (and a brief look at the suspected location if available), propose a tier:

- **Tier 1** — quick fix: cause looks obvious, fix looks local, <30 min estimate.
- **Tier 2** — investigation: cause unclear or fix non-obvious.
- **Tier 3** — architectural: bug suggests a design flaw, not a coding error.

State your reasoning in one sentence. Wait for my confirmation or override.

Update the bug file's Tier field once confirmed.

## Step 3: Reproduce (Tier 2/3 only — required)

If the bug file's "Reproduction" section is "needs investigation" or empty:

1. Stop. We can't fix without a repro.
2. Propose a plan to construct one — what would we run, what would we observe?
3. Get my approval, then build the repro.
4. Once repro is reliable, update the bug file's Reproduction section with concrete steps.

If reproduction already documented: run it once now, confirm the bug actually triggers as described. If it doesn't, the bug may be already-fixed or the repro is wrong — surface this and ask before continuing.

For Tier 1: skip this step unless I want a repro for safety.

## Step 4: Investigate (Tier 2/3 only)

Form hypotheses about the cause. For each, what would confirm or rule it out? Run those checks (read code, git log, add temporary logging — never commit log statements).

Update the bug file's "Investigation" section as you go: hypotheses considered, evidence found.

**If your leading hypothesis changes mid-investigation:**

- Stop.
- State what changed and why.
- Update the bug file with both the old hypothesis (under "ruled out") and the new one.
- Get my acknowledgment before continuing the new line of investigation.

This mirrors the feature workflow's "stop and re-spec" rule.

## Step 5: Root cause + fix proposal

When the cause is identified:

1. Write the "Root cause" section in the bug file. One paragraph, in plain language. Why does this bug happen? Not "what's broken" — *why*.
2. For Tier 2/3: leave the "What would have prevented this" section with a placeholder telling me to write it myself: `<!-- AUTHOR: write one sentence in your own words -->`. Do NOT draft this section.
3. Propose the fix: which files change, what changes, why.
4. If the fix is large or touches multiple modules unexpectedly: this might be Tier 3 in disguise. Surface that and ask whether to escalate.
5. Wait for my approval before any source edits.

## Step 6: Author rubber-stamp check (Tier 2/3 only)

Before I approve the fix, prompt me:

> "In your own words, what would have prevented this bug? One sentence — write it directly into the bug file's 'What would have prevented this' section."

Wait for me. Don't draft it. Don't accept "yeah looks right" — I have to write the sentence.

For Tier 1: skip this.

## Step 7: Implement

Apply the fix. Update the bug file's "Fix" section with files changed and what changed.

## Step 8: Verify against repro

Run the documented reproduction case. Confirm the bug no longer triggers.

Update "Verification" section with PASS/FAIL and date.

If FAIL: do not close. The fix is incomplete. Either iterate or surface that we missed something in root cause analysis (which means going back to Step 4 with new info — log this in Investigation).

## Step 9: Test extension

Ask: should the smoke test (or any other test) be extended to catch this bug if it returns?

- For Critical/High severity bugs: strongly encourage yes.
- For Medium/Low: ask, but accept "no" if I have reason.

If yes: add the test. Run the smoke test, confirm it passes. Document under "Verification".

## Step 10: Close out

1. Update bug status to "fixed".
2. Move the file: `docs/bugs/active/NNNN-slug.md` → `docs/bugs/closed/NNNN-slug.md`.
3. Stage the changes. Propose a commit:
   - Message format: `fix(NNNN): <symptom> (<root cause in 5-10 words>)`
   - Example: `fix(0007): entities not rendering (Renderer init order before panel ctor)`
4. If multiple related bugs were fixed in this pass: one commit, message lists all IDs.
5. Wait for me to confirm before committing.

After commit, tell me what's next: any remaining bugs of equal/higher severity, or return me to the previous task if I was mid-feature.
