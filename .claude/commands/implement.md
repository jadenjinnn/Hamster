---
description: Begin implementation of an approved feature spec.
---

I'm ready to implement a feature. Steps:

## Step 1: Verify the spec is approved

Ask me which feature spec we're implementing (file name in `docs/features/active/`).

Read the spec. Verify:

1. The "Why this approach", "Risks", "Success criteria", and "Test extensions required" sections are filled in.
3. The status line says "approved" (not "draft").

If any of these fail, stop and surface what's missing. Do not start coding.

## Step 2: Plan the implementation

Once the spec checks out, propose an implementation plan:

- Order of changes (what gets built first, second, third)
- Which files will be touched
- The "smallest viable end-to-end version" you'll build first before any polish
- Risks from the spec that the plan needs to actively guard against

Wait for my approval of the plan before any edits.

## Step 3: Implement, smallest version first

Build the smallest viable version end-to-end. Make it work. Then iterate toward the full spec.

While implementing:

- If you discover the spec was wrong about something — a hidden constraint, an incorrect assumption about an existing module, an API that doesn't behave as the spec assumed — **stop**. Do not work around it silently. Surface what's wrong, propose a spec amendment under the spec's "Spec amendments" section, get my approval, then continue.
- If a non-obvious decision comes up that wasn't in the spec, log it under "Decisions during implementation" in the spec. Brief: title, one paragraph what + why.
- If something tempts you to add scope ("while I'm here..."), it goes into the spec's "Future work" section, not into this implementation.
- Stay alert for the anti-patterns in `docs/feature-workflow.md` ("Anti-patterns Claude should refuse"). If I ask for one, push back once. If I confirm, log the override under "Decisions during implementation" and proceed.

## Step 4: Verify against success criteria

When the feature is functionally complete, walk through the spec's "Success criteria" section point by point and confirm each is met. Report which pass, which don't, and what's left to do.

## Step 5: Close out

Once success criteria pass:

1. Add/extend the smoke test as specified in "Test extensions required". Run it. Confirm it passes.
2. Update `docs/architecture.md` if module boundaries changed.
3. Add an entry to `docs/decisions.md` for any non-obvious decision logged in the spec that's worth preserving project-wide.
4. Move the spec from `docs/features/active/` to `docs/features/shipped/<YYYY-MM-DD>-<name>.md`.
5. Add a one-line entry to the "What's new" section of the public README.
6. Update the session log in `CLAUDE.local.md`.

Tell me when all five close-out steps are done.
