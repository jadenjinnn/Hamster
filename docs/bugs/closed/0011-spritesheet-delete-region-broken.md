# Bug 0011: SpritesheetEditor delete (Del key + X button) doesn't remove regions

> Status: **fixed**
> Severity: **High**
> Tier: **1 — quick fix** (X button); Del key pending re-verification
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

## Investigation

### Evidence

The region-list row (`SpritesheetEditor.cpp:374-396`) submits, in order on one line:
`Selectable("##row", …, size {0, h})` → `SameLine` → `InputText("##name")` → `SameLine` → `SmallButton("X")`.

A `Selectable` with `size.x == 0` stretches to the full content width of the list child, so its hit box covers the entire row — including the screen area where the InputText and the "X" button are drawn afterward. In ImGui ≥1.89 a Selectable does **not** yield hover/click to later overlapping items unless it is flagged `ImGuiSelectableFlags_AllowOverlap`. Without the flag the Selectable consumed every click on the row, so the "X" `SmallButton` never reported pressed and `m_Working.erase(...)` never ran — matching the "X does nothing" symptom exactly. This is the confirmed root cause for the X button. (The Del-key sub-symptom is separate — `IsKeyPressed(Delete)` is likely being eaten while the name `InputText` holds keyboard focus; left for re-verification after this fix.)

---

## Root cause

The full-width row `Selectable` in the region list was not flagged `ImGuiSelectableFlags_AllowOverlap`, so it claimed the click for the entire row and shadowed the per-row name field and "X" delete button drawn on the same line. Clicks on "X" hit the Selectable (re-selecting the row) instead of the button, so no region was ever removed.

---

## Fix (implemented)

`Hamster-Wheel/src/Panels/SpritesheetEditor.cpp` — added `ImGuiSelectableFlags_AllowOverlap` to the region-list row Selectable so the name InputText and "X" button on the same line receive their own clicks.

## Verification

Build + link clean. Author confirmed (2026-05-25): the "X" button now removes the region row + canvas rect. PASS.

---

## Related

- spritesheet-support feature (`docs/features/active/spritesheet-support.md`) — stage 4 work, shipped 2026-05-19.
