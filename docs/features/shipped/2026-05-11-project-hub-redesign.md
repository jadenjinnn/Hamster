# Feature spec: Project Hub Redesign

> Tier: **Sketch**
> Status: **shipped**
> Started: 2026-05-11
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated
- **Tier rationale (1 sentence)**: Visual-only overhaul of one screen with no data model, API, or cross-module changes.

---

## Problem

The project hub (ProjectSelector + ProjectCreator) is two unstyled ImGui windows with raw text, buttons, and file dialogs. It's the first screen users see and sets the tone for the entire editor. It looks like a debug UI, not a polished tool. A visual redesign brings it in line with the Figma-inspired theme already applied to the editor panels.

## In scope

- **Top bar**: "Hamster" brand text on the left, "New Project" green button on the right, static placeholder icons (bell, help, gear, user)
- **Header section**: "Projects" bold heading, "Manage your active workspaces and recent edits." subtitle, static search input (no filtering logic), grid/list toggle (grid only, toggle is visual-only)
- **Project card grid**: 4-column responsive grid of cards, each with:
  - Dark thumbnail area (placeholder — static colored shapes or empty)
  - Project name (bold)
  - Star/favorite icon (static, non-functional)
  - Tag pill (static hardcoded text like "2D", "RPG")
  - "Opened X ago" timestamp (static hardcoded text)
  - "..." context menu icon (static, non-functional)
- **"Create New Project" card**: First card in the grid with dashed border, "+" icon, "Create New Project" label. Clicking opens the create project modal.
- **Create Project modal**: Centered overlay with dimmed background containing:
  - "Create New Project" title with X close button
  - "PROJECT NAME" field with placeholder text (wired to existing name input)
  - "TEMPLATE" section with 4 static template cards (Empty, Platformer 2D, Top-Down, UI Only) — each with icon, name, description. Visual selection highlight, no template logic.
  - "LOCATION" field with folder icon, path display, "Browse..." button (wired to existing tinyfiledialogs directory picker)
  - "Cancel" and "Create Project" (green) buttons (Create wired to existing project creation logic)
- **Existing "Open Project" flow**: Add an "Open Project" button or import action somewhere accessible (e.g., in the top bar or as a secondary action) that triggers the existing tinyfiledialogs .hamproj file picker. The current ProjectSelector panel functionality is preserved but relocated.

## Out of scope

- Persistent recent projects list (no storage/serialization of opened projects)
- Functional search/filter
- Functional grid/list toggle
- Project templates (actual template data, scene scaffolding)
- Favorites/starring (persistence, sorting)
- Real scene thumbnails
- Context menu actions on project cards
- Top bar icons doing anything (notifications, settings, user profile)

## API sketch

No new APIs. This is a pure ImGui rendering change within `ProjectHubLayer` and its sub-panels.

```cpp
// ProjectHubLayer::OnImGuiUpdate() renders the full hub as a single
// fullscreen ImGui window instead of two separate floating panels.
// ProjectCreator::Render() becomes a modal overlay.
```

---

## Why this approach

The current hub uses two separate ImGui windows (`ProjectSelector`, `ProjectCreator`) which look like debug panels. The mockup unifies them into a single fullscreen layout with a card grid — matching the Figma-inspired dark theme already applied throughout the editor. By rendering static placeholders for data-driven elements (tags, timestamps, thumbnails, favorites), the visual design is established now while backend systems (recent project list, templates) can be wired in later without another visual pass. The modal approach for "Create New Project" keeps the creation flow separate from browsing, matching the user's mental model of "pick vs. create."

## Risks / what could go wrong

- **ImGui layout complexity**: The card grid with thumbnail areas, overlapping icons (star, "..."), and tag pills requires manual ImGui `DrawList` rendering. If card layout breaks at different window sizes, it'll look worse than the current minimal UI. Mitigation: test at the editor's default window size and one smaller size.
- **Modal overlay rendering**: ImGui modals have quirks with focus and stacking. If the modal doesn't properly capture input or dim the background, it could feel broken. Mitigation: use `ImGui::OpenPopup` / `BeginPopupModal` which handle focus trapping natively.
- **Hardcoded placeholder data becoming permanent**: Static cards with fake names/tags risk shipping as "good enough" and never getting replaced with real data. Mitigation: mark all placeholder data with a clear comment pattern (`// PLACEHOLDER`) and list real data integration as future work.

## Success criteria

1. Editor launches to a fullscreen project hub — no floating ImGui windows visible
2. Top bar, header, search input, and grid/list toggle are all visually present and match the mockup's layout/spacing
3. At least 4 placeholder project cards render in a grid with thumbnail areas, names, tag pills, timestamps, star icons, and "..." icons
4. "Create New Project" card is the first item in the grid with a "+" icon and dashed border style
5. Clicking "Create New Project" card opens a centered modal with project name input, 4 template cards, location picker, and Cancel/Create buttons
6. The "Create Project" button in the modal successfully creates a project (existing functionality preserved)
7. An "Open Project" action exists somewhere in the hub that opens the .hamproj file dialog (existing functionality preserved)
8. Visual style matches the editor's existing dark theme (colors, Inter font, rounded corners, subtle borders)

## Test extensions required

None. This is a pure additive UI change to the project hub screen. The existing smoke test doesn't exercise the project hub (it creates a project programmatically). Visual correctness is verified by launching the editor and inspecting the hub screen.

---

## Decisions during implementation

<!-- Append-only log. -->

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Persistent recent projects list (serialize opened project paths, display as real cards)
- Functional project templates (scene scaffolding per template type)
- Functional search/filter across project list
- Grid/list view toggle
- Favorites with sorting (starred projects pinned to top)
- Real scene thumbnails (render scene to texture, cache as PNG)
- Context menu on project cards (rename, delete, reveal in explorer)
- Top bar icon actions (settings dialog, help overlay)
