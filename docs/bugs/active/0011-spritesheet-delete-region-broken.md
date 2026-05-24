# Bug 0011: SpritesheetEditor delete (Del key + X button) doesn't remove regions

> Status: **open**
> Severity: **High**
> Tier:
> Logged: 2026-05-24
> Found while: stage 7 manual verification of spritesheet-support

---

## Symptom

In the floating SpritesheetEditor window: pressing **Delete** with a region selected does not remove the rectangle from the canvas or the region list. Clicking the **X** button on a region's row in the right-hand region list also does not remove it. After Save the regions persist as if no delete was attempted.

## Suspected location

`Hamster-Wheel/src/Panels/SpritesheetEditor.cpp`
- Del key handler at line 358 (`ImGui::IsKeyPressed(ImGuiKey_Delete)`)
- X button handler at line 387 (`ImGui::SmallButton("X")`)

Hypothesis (un-investigated): Del key never fires because ImGui keyboard nav was disabled in a prior feature (per session log 2026-05-16: "Keyboard-nav config flag dropped"), so `IsKeyPressed` may not be wired without it. The X button may be shadowed by the row's `Selectable` (line 376) which claims full row width — click lands on Selectable, not the button. Both are guesses; needs `/bug-fix` Tier 2 investigation.

## Reproduction

1. Open the editor on an existing or fresh project.
2. Asset Browser → Import Spritesheet → pick any multi-region PNG.
3. Draw 2–3 rectangles in the canvas.
4. Click a rect to select it (its border turns accent).
5. Press **Delete** → expected: rect removed; observed: nothing.
6. In the right-hand region list, click the **X** button on any row → expected: row + rect removed; observed: nothing.
7. Click Save → all regions persist.

---

## Investigation (Tier 2/3 only)

### Hypotheses considered

### Evidence

---

## Root cause

## What would have prevented this (AUTHOR WRITES — Tier 2/3 only)

---

## Fix

## Verification

---

## Related

- spritesheet-support feature (`docs/features/active/spritesheet-support.md`) — stage 4 work, shipped 2026-05-19.
