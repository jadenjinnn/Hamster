# Bug 0013: expanded sheet mini-cards should lay out horizontally, not vertically

> Status: **fixed**
> Severity: **Low**
> Tier: **1 — quick fix**
> Logged: 2026-05-24
> Found while: stage 7 manual verification of spritesheet-support

---

## Symptom

When a sheet card is expanded (caret clicked) in the Asset Browser, the sub-sprite mini-cards tile into the same multi-column flow grid as the parent textures. The author wants them to lay out as a single horizontal strip immediately below the parent sheet card, then the rest of the asset grid resumes below the strip.

This is a UX preference / spec refinement rather than a functional bug, but logged here so it gets resolved alongside the same area of code being touched by bug 0012.

## Suspected location

`Hamster-Wheel/src/Panels/AssetBrowser.cpp:379–~440` — the `if (isSheet && m_ExpandedSheets.count(mUUID) > 0)` block. Each mini-card currently calls the grid's `nextRow()` helper after itself, which makes them tile into the parent grid layout.

## Reproduction

1. Open a project with at least one sliced spritesheet (resolve bug 0012 first; otherwise the sheet won't appear after reopen).
2. Click the chevron caret on the sheet card.
3. **Observed**: mini-cards stack into the same grid as regular textures, breaking left-to-right reading flow.
4. **Expected**: mini-cards laid out as a single horizontal row (with horizontal scroll if too many) directly below the parent sheet card.

---

## Investigation (Tier 2/3 only)

### Hypotheses considered

### Evidence

---

## Root cause

(N/A — UX layout choice rather than a code defect; fix is a layout rewrite in the expansion block.)

## What would have prevented this (AUTHOR WRITES — Tier 2/3 only)

---

## Fix (implemented)

`Hamster-Wheel/src/Panels/AssetBrowser.cpp` — the expanded-sheet block no longer tiles mini-cards through the grid's `nextRow()`. Instead:
- The parent card skips its `nextRow()` SameLine when expanded, so the strip falls onto the next line beneath it.
- Sub-sprite mini-cards render inside a full-content-width `BeginChild` (`ImVec2(avail, miniH + 14.0f)`, `ImGuiWindowFlags_HorizontalScrollbar`), laid out left-to-right with `SameLine(0, 8.0f)` between cards. Unique child id (`##sheet_strip_<uuid>`) avoids collisions when multiple sheets are expanded.
- After `EndChild`, `colIdx` resets to 0 so the next parent card starts a fresh grid row.

Known minor deviation from the spec's "directly below the parent card": the strip is a full-width row below the parent card's row rather than anchored under the card's column. Acceptable for a Low/UX item; can be refined if the author wants strict column anchoring.

## Verification

(Required at close-out — visual check of the expanded strip layout.)
Build + link clean after fix. Visual check **pending author**: expand a sliced sheet's caret → mini-cards should appear as one horizontal row (scrolling if many) directly beneath the sheet card, with the rest of the asset grid resuming below.

---

## Related

- Bug 0012 — same panel; will likely be touched in the same change.
- spritesheet-support feature (`docs/features/active/spritesheet-support.md`) — stage 5 work.
