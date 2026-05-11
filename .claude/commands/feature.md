---
description: Start a new feature. Classification + spec, no code.
---

I want to start a new feature. Follow the process in `docs/feature-workflow.md`. Do NOT edit any source files in this command — spec only.

## Step 1: Mode check

Ask me one question first: which advice mode do I want for this feature's design conversation?

- **Options + tradeoffs** (you lay out 2–4 approaches, I pick)
- **Devil's advocate** (I have an opinion, push back hard)
- **Socratic** (ask me targeted questions to surface my own thinking)
- **Mixed** (you'll switch as we go)

Wait for my answer.

## Step 2: Elicit the feature

Once I've picked a mode, ask me clarifying questions about the feature itself:

- What problem it solves and who needs it
- What the smallest viable version looks like
- What's explicitly out of scope
- What existing modules it touches
- Any constraints I already know about

Keep questioning until you have a clear picture. If I'm vague, push for specificity.

## Step 3: Classify

Once the picture is clear, propose a classification:

- Reversibility (reversible / sticky) — with one-sentence justification
- Scope (isolated / cross-cutting) — with one-sentence justification
- Resulting spec tier (sketch / design / full spec)

Wait for me to confirm or override the classification before drafting the spec.

## Step 4: Draft the spec

Create `docs/features/active/<feature-name>.md` from the template at `.claude/templates/feature-spec.md`.

Fill in every section including "Why this approach" — draft it based on the design conversation.

If I'm at the Sketch tier, omit the Design and Full spec sections.
If I'm at the Design tier, include Design but omit Full spec.

## Step 5: Hand off

When the draft is ready, tell me:
- Where the spec file is
- That no code will be written until I explicitly say "spec approved"

Stop there. Wait for me.
