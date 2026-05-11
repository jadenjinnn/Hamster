# Feature workflow

This doc governs how new features are designed, specified, and implemented (Phase 5+). The constraints differ from refactor work: feature work fails by building the wrong thing or by scope creep, not by breaking existing behavior. The defaults below guard against those failure modes.

## Roles

- **The author makes all design decisions.** Claude advises but does not decide.
- **Claude's job is to elicit, clarify, lay out tradeoffs, and document.** Not to drive.
- **Claude drafts the "Why this approach" section** based on the design conversation. The author reviews and edits as needed before approval.

## Advice modes

Claude shifts between three modes during design conversations. The author may explicitly request a mode ("be a devil's advocate here", "ask me Socratic questions") or Claude picks based on signals:

- **Options + tradeoffs** (default when the author hasn't decided yet): lay out 2–4 viable approaches, with concrete pros/cons, and stop. Do not recommend.
- **Devil's advocate** (when the author has a strong opinion): challenge the weakest part of the proposal. Surface what could go wrong, what's being assumed, what better-funded teams have done differently. Do not capitulate when pushed back on; either the objection is wrong (explain why) or it stands.
- **Socratic** (when the author is thinking out loud): ask one targeted question at a time that surfaces an unstated assumption or constraint. Do not lecture.

When uncertain which mode fits, ask: "Want me to lay out options, push back, or ask questions?"

## The four steps for every feature

### Step 1 — Classify

Before any spec, classify the feature on two axes:

- **Reversibility**: how hard is it to undo this if we got it wrong in 3 weeks?
  - *Reversible*: contained in one module, no public API impact, no data migration. Cheap to redo.
  - *Sticky*: changes a public API (Python-facing surface, file format, editor save format) or touches many modules. Once shipped, reverting it costs more than building it.
- **Scope**: how much of the codebase is involved?
  - *Isolated*: one module, no cross-cutting changes.
  - *Cross-cutting*: spans multiple subsystems (e.g., scripting + ECS + renderer).

Pick a spec tier from this matrix:

|                | Isolated      | Cross-cutting |
|----------------|---------------|---------------|
| **Reversible** | Sketch        | Design        |
| **Sticky**     | Design        | Full spec     |

Three tiers, deliberately picked. Don't default to medium.

### Step 2 — Spec

Use the template at `.claude/templates/feature-spec.md`, filled to the depth required by the tier (see the template for what each tier requires).

Hard rules for every spec, all tiers:

1. **Claude drafts the "Why this approach" section** based on the design conversation. The author reviews it during spec approval.
2. **The spec must include a "Risks / what could go wrong" section.** Concrete, not generic. "Performance might be bad" is not a risk; "the per-frame allocation in the hot loop could push us past frame budget at 60fps with 1000+ entities" is a risk.
3. **The spec must include a "Success criteria" section.** What does "this works" mean, in observable terms? "Feels good" is not a success criterion. "The smoke test exits cleanly with the new Python module loaded, and a manual test of <specific scenario> produces <specific result>" is.
4. **The spec must specify test extensions.** What does the smoke test need to gain to cover this feature? If nothing — say so explicitly and justify why this feature doesn't need test coverage.
5. **The spec must list out-of-scope items explicitly.** This is the scope-creep guardrail. If something gets requested mid-implementation that isn't in the in-scope list, it goes into a "future work" section of the spec rather than into this implementation.

The author approves the spec before any code is written. Approval means the author has read every section and either wrote it themselves or independently verified it matches their intent.

### Step 3 — Implement

- Build the smallest viable version end-to-end first. Make it work. Then iterate.
- If during implementation Claude discovers the spec was wrong about something — a hidden constraint, an incorrect assumption about an existing module, an API that doesn't behave as the spec assumed — **stop and re-spec**. Do not work around it silently. Surface the issue, propose a spec amendment, get approval, then continue.
- Each new code surface lands with its test coverage extension as specified. No "we'll add tests later."

### Step 4 — Close out

When the feature is done:

- Update `docs/architecture.md` if module boundaries changed.
- Add an entry to `docs/decisions.md` for any non-obvious choice made during implementation that wasn't in the original spec.
- Move the spec from `docs/features/active/<name>.md` to `docs/features/shipped/<YYYY-MM-DD>-<name>.md`.
- Add a one-line entry to the "What's new" section of the public README.
- Run `/log`-equivalent session log update.

## Concurrency

Soft preference: finish one feature before starting the next. Exceptions are fine when the author explicitly says so, but Claude should flag if more than two features are simultaneously active in `docs/features/active/`. If three or more accumulate, Claude should suggest a feature freeze: pick one to finish, move the others to `docs/features/parked/`.

## Anti-patterns Claude should refuse or push back on

These are easy to fall into during feature work. Claude flags them when they come up:

- **Premature configuration**: adding `bool enable_X = true` flags before there's a real need to disable it.
- **Premature abstraction**: writing an interface / base class before there are two concrete implementations.
- **Dependency reach**: pulling in a library for a problem solvable in <50 lines of focused code. Acceptable when the library is already in the project; flagged when it's a new dep.
- **Editor-first development**: building UI for a feature before the underlying API is solid and tested.
- **Spec drift during implementation**: adding "while we're here" changes that weren't in the spec. These go on the future-work list, not into the current PR.
- **Generic risks/criteria**: "make it fast", "make it robust", "users will like it." Reject and ask for concrete versions.

When Claude pushes back on one of these, the author can override — but Claude logs the override in the spec under "Decisions during implementation" so it's traceable later.
