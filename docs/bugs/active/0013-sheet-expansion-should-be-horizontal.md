# Bug 0013: expanded sheet mini-cards should lay out horizontally, not vertically

> Status: **open**
> Severity: **Low**
> Tier:
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

## Fix

Likely approach (un-verified): flush to next row, `BeginChild` a thin strip the width of the asset-browser content area, render mini-cards with `SameLine()` between them (and enable horizontal scrollbar via `ImGuiWindowFlags_HorizontalScrollbar`), `EndChild`, flush again before the next parent card row.

## Verification

(Required at close-out — visual check of the expanded strip layout.)

---

## Related

- Bug 0012 — same panel; will likely be touched in the same change.
- spritesheet-support feature (`docs/features/active/spritesheet-support.md`) — stage 5 work.
