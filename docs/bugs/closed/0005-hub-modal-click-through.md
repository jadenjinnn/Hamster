# Bug 0005: project-hub card grid receives clicks through open modal dialogs

> Status: **fix-implemented**
> Severity: **Medium**
> Tier: **1**
> Logged: 2026-05-16
> Found while: manual verification of bug 0002 fix

---

## Symptom

When the project-hub's Create Project modal (or Rename / Delete / Missing dialogs) is open, clicking inside the modal — specifically on the project-name input — registers as a click on whatever card is geometrically behind that input. If the modal sits over a project card, clicking the input triggers `OpenProject(card.path)` on the card, which posts `ProjectOpened` and (after my bug 0002 fix) swaps the hub for an EditorLayer.

Symptom path: user opens Create modal → clicks the name input → unrelated project starts opening behind the modal → if that open succeeds, the hub disappears mid-modal; if it fails, an error dialog stacks on top of the modal.

## Suspected location

- `Hamster-Wheel/src/ProjectHubLayer.cpp:184–333` — `RenderCardGrid` uses raw `ImGui::IsMouseHoveringRect` + `ImGui::IsMouseClicked(0)` checks instead of routing through ImGui's focus / popup-block system.
- Similar pattern in `RenderTopBar` (lines 108–135) for the "Open Project" pseudo-button.

The card grid draws via `ImDrawList` primitives and hit-tests geometrically, so an open `BeginPopupModal` does not intercept the click — modals block via ImGui's input routing, not via geometry.

## Reproduction

1. Build and run `Hamster-Wheel.exe`.
2. From the hub, click "New Project" — Create Project modal opens.
3. Position the modal so it overlaps an existing project card (default centering already does this in most cases).
4. Click inside the project-name text input.
5. Observe: the project behind the modal begins loading. On a clean session this opens; with bug 0003 present it errors first.

Expected: clicks on modal widgets stay in the modal.

---

## Fix

`Hamster-Wheel/src/ProjectHubLayer.cpp` — both `RenderTopBar` and `RenderCardGrid` now compute a single `inputBlocked` flag at entry via `ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)` and AND it into every `IsMouseHoveringRect`. Rendering is unaffected; only hover/click detection is gated. Touches: New Project button, Open Project pseudo-button, every project card body, the ellipsis (context-menu) button.

## Verification

- [x] Build clean, smoke test passes.
- [ ] Manual: open Create modal, click the project-name input, confirm no project opens behind the modal.

---

## Related

- Bug 0002: surfaced because the post-fix behaviour exposed the click-through more clearly (pre-fix, the first click opened the editor and hid the hub, masking subsequent click-throughs).
