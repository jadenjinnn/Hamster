# Feature spec: <feature name>

> Tier: **Sketch** | **Design** | **Full spec** (delete the two that don't apply)
> Status: **draft** | **approved** | **in progress** | **shipped**
> Started: YYYY-MM-DD
> Spec author: <name>

---

## Classification

- **Reversibility**: Reversible | Sticky
- **Scope**: Isolated | Cross-cutting
- **Tier rationale (1 sentence)**: <why this tier was picked>

---

## Problem

<!-- What capability is missing? Who needs it? Why now?
2–4 sentences. Concrete.
Bad: "We need particles for visual effects."
Good: "Game scripts can't currently spawn visual effects from Python — sound is the only audio/visual feedback channel. This blocks a class of demos (explosions, hits, ambient effects) that any 2D engine should support out of the box." -->

## In scope

<!-- Bullet list of what this feature will do. Keep tight.
- Bullet 1
- Bullet 2
-->

## Out of scope

<!-- Bullet list of what this feature will NOT do. This is the scope-creep guardrail.
- Bullet 1
- Bullet 2
-->

## API sketch

<!-- For any tier: at least one code example showing how this feature gets used.
Python-side example AND C++-side example if it crosses the boundary.
Keep it minimal — this is intent, not final syntax. -->

```python
# Example Python usage
```

```cpp
// Example C++ usage (if applicable)
```

---

## Design (Design tier and Full spec only — delete this section if Sketch tier)

### Data structures

<!-- What new types? Where do they live? What lifetimes? -->

### Module touchpoints

<!-- Which existing modules does this change?
- src/scripting/Bindings.cpp — add binding for new type
- src/render/Renderer.cpp — new draw call path for particles
-->

### Lifecycle / control flow

<!-- How does this feature get initialized, used per frame, torn down?
Where does it hook into the existing main loop / scene update? -->

### Edge cases

<!-- Concrete edge cases the design must handle.
- What happens if X is called before Y?
- What if there are zero / one / many?
- What if it's called from the wrong thread?
-->

---

## Full spec (Full spec tier only — delete this section if Sketch or Design tier)

### Sequence diagrams / data flow

<!-- For complex interactions: ASCII sequence diagram or step-by-step trace of
"what happens when user does X." Use mermaid if helpful. -->

### Error handling

<!-- What errors are possible? How are they surfaced? Python exceptions?
C++ return codes? Logged and ignored? -->

### Performance considerations

<!-- Concrete numbers and constraints, not vibes.
- Hot path: this runs per-frame for every entity with component X
- Budget: must not exceed Y microseconds at N=1000
- Memory: per-instance footprint should stay under Z bytes
-->

### Migration / compatibility

<!-- Does this break existing scripts? Existing save files?
If yes, migration plan. If no, state how compatibility is preserved. -->

---

## Why this approach

<!-- Explain:
- What alternatives were considered
- Why this one was picked
- What tradeoffs are being accepted -->

## Risks / what could go wrong

<!-- Concrete risks with concrete consequences. Not generic.
Bad: "Could be slow."
Good: "Particle pool is per-scene; switching scenes mid-emission could leak
allocated buffers if the dtor order is wrong (see Scene::~Scene line 142
which assumes no live emitters)."

3–6 risks for most features. -->

## Success criteria

<!-- Observable signals that this feature works.
Bad: "Particles look good."
Good: "Smoke test loads particles_demo.py, runs 60 frames, and the test
passes if (a) no exceptions raised, (b) the renderer reports >0 draw calls
from the particle system path, (c) memory is back to baseline within
100ms of scene end."

These will be verified before marking the feature shipped. -->

## Test extensions required

<!-- What does the smoke test need to gain to cover this feature?
- Add a smoke scenario that exercises X
- Extend the existing Y test to include Z

If "none" — explain why this feature doesn't need test coverage.
Acceptable reasons: pure additive UI, ephemeral debug tooling.
Not acceptable: "it's small", "I'll test manually". -->

---

## Decisions during implementation

<!-- Append-only log of non-obvious decisions made while building.
Each entry is dated. Updated by Claude during implementation, reviewed by author.

### YYYY-MM-DD — <decision title>
<one-paragraph what / why> -->

## Spec amendments

<!-- Append-only log of times the spec changed mid-implementation because
the original was wrong. Each entry includes what changed and why.

### YYYY-MM-DD — <amendment title>
<what was wrong, what's the new plan, who approved> -->

---

## Future work (out-of-scope ideas surfaced during this feature)

<!-- When something gets requested mid-implementation that isn't in scope,
it goes here, not into the current implementation. After this feature ships,
these become candidates for new feature specs. -->
