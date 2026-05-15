# Feature spec: Panel layout — rounded cards with gaps

> Tier: **Design**
> Status: **superseded**
> Started: 2026-05-14
> Superseded: 2026-05-15
> Spec author: Jaden (elicited by Claude)

> **Superseded by** [`editor-rewrite-from-prototype`](../active/editor-rewrite-from-prototype.md).
> A partial implementation was attempted (port docking→manual layout into `Hamster-Wheel` in place) and reverted on 2026-05-15. The new approach inverts the direction: the polished `Hamster-UIPrototype/` becomes the editor frontend, and the engine backend is ported into it.

---

## Classification

- **Reversibility**: Reversible — purely visual/layout, no data formats or APIs change
- **Scope**: Cross-cutting — touches EditorLayer, all panel Begin/End calls, ImGuiLayer (docking removal), theme
- **Tier rationale**: Reversible + cross-cutting = Design

---

## Problem

The editor panels are currently laid out via ImGui's built-in docking system. ImGui docking forces `WindowRounding = 0` on docked windows and renders them flush against each other with no gaps. This makes it impossible to achieve the target visual style: rounded-corner panels separated by visible dark background, as shown in the reference image (Figma-inspired card layout). A previous attempt to patch ImGui's dock node rendering was abandoned because the docking system doesn't expose enough control for per-panel rounding.

## In scope

- Remove ImGui docking from the editor layout entirely
- Manually position and size all editor panels each frame based on window dimensions, using proportional layout
- Enforce a minimum window size so panels don't collapse
- Rounded corners and visible gaps (dark background) between all panels
- Panel title bars with "..." ellipsis button (non-functional for now)
- Bottom panel area has tabs for Asset Browser, Animation, and Console
- Colors matched to the reference image

## Out of scope

- User-resizable panel borders (drag to resize individual panels)
- User-rearrangeable panel order
- Functional "..." button (future feature)
- Collapsible/hideable panels
- ProjectHubLayer layout changes (it has its own fullscreen layout already)

## API sketch

No Python or public API changes. This is editor-internal.

```cpp
// In EditorLayer::OnImGuiUpdate — replace DockSpaceOverViewport with manual layout
void EditorLayer::OnImGuiUpdate() {
    // Compute panel rects from window size
    ImVec2 windowSize = ImGui::GetMainViewport()->Size;
    float gap = 6.0f;
    float menuBarHeight = ImGui::GetFrameHeight();

    float leftW = windowSize.x * 0.18f;
    float rightW = windowSize.x * 0.20f;
    float centerW = windowSize.x - leftW - rightW - gap * 4;
    float bottomH = windowSize.y * 0.28f;
    float topH = windowSize.y - menuBarHeight - bottomH - gap * 3;

    // Position each panel with SetNextWindowPos/SetNextWindowSize + NoMove/NoResize flags
    ImGui::SetNextWindowPos({gap, menuBarHeight + gap});
    ImGui::SetNextWindowSize({leftW, windowSize.y - menuBarHeight - gap * 2});
    // ... Property Editor Begin/End

    ImGui::SetNextWindowPos({gap + leftW + gap, menuBarHeight + gap});
    ImGui::SetNextWindowSize({centerW, topH});
    // ... Level Editor Begin/End

    // etc.
}
```

---

## Design

### Data structures

No new types. Add layout constants (gap size, panel width ratios, minimum window dimensions) as private members or constexpr in EditorLayer.

### Module touchpoints

- `Hamster-Core/src/Gui/ImGuiLayer.cpp` — remove `ImGuiConfigFlags_DockingEnable` from io.ConfigFlags
- `Hamster-Wheel/src/EditorLayer.cpp` — replace `ImGui::DockSpaceOverViewport()` with manual `SetNextWindowPos`/`SetNextWindowSize` calls; remove `ImGui::LoadIniSettingsFromDisk` (no longer needed); add minimum window size enforcement via GLFW
- `Hamster-Wheel/src/EditorLayer.h` — possibly add layout ratio constants
- `Hamster-Wheel/src/Theme/HamsterTheme.cpp` — adjust colors to match reference image (panel background, gaps background); ensure `WindowRounding` is applied (it already is at 8.0f but was being overridden by docking)
- `Hamster-Wheel/Resources/default.ini` — can be deleted or ignored (no longer drives layout)
- All panel Render() methods — add `ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse` flags to their `ImGui::Begin()` calls; render "..." button in title bar area
- Panel base class (`Hamster-Core/src/Gui/Panel.h`) — potentially add a helper for the standard title bar with "..." button

### Lifecycle / control flow

Each frame in `EditorLayer::OnImGuiUpdate()`:

1. Read main viewport size from `ImGui::GetMainViewport()->Size`
2. Compute panel rects: left sidebar (Property Editor), center top (Level Editor), right sidebar (Hierarchy), center bottom (Asset Browser / Animation / Console tabs)
3. For each panel: `ImGui::SetNextWindowPos(rect.pos)`, `ImGui::SetNextWindowSize(rect.size)`, then the panel's existing `Render()` call
4. The gaps between panels show the main viewport's clear color (dark background)

Minimum window size set once via `glfwSetWindowSizeLimits()` in `EditorLayer::OnAttach()`.

### Edge cases

- **Window at minimum size**: panels hit their minimum proportional sizes; content scrolls within panels as it does today
- **Very wide or very tall aspect ratios**: proportional layout stretches naturally; no special handling needed since content is scrollable
- **Panel content taller than available height**: already handled by ImGui's built-in scrolling within windows
- **Bottom tabs (Asset Browser / Animation / Console)**: use ImGui's `BeginTabBar`/`BeginTabItem` within a single positioned window, rather than three separate positioned windows

---

## Why this approach

The previous attempt tried to keep ImGui docking and patch the rendering to add rounded corners. That failed because docking fundamentally overrides `WindowRounding` on host windows and renders panels flush — the rounding code was being ignored at the dock node level, not the window level. Patching `imgui.cpp` dock rendering at three separate locations still didn't produce visible rounding because the host window's background fills edge-to-edge.

Abandoning docking entirely and using `SetNextWindowPos`/`SetNextWindowSize` sidesteps the entire problem: ImGui treats each panel as a regular floating window (which fully respects `WindowRounding`), and the gaps between panels are simply regions where no window is drawn, showing the dark viewport background. This is a well-established ImGui pattern for fixed-layout editors.

The tradeoff is losing the ability for users to drag-resize or rearrange panels. For Hamster's use case (classroom tool, consistent experience for students), a fixed layout is arguably preferable — students see the same editor layout as the instructor.

## Risks / what could go wrong

1. **Panel content may assume it can stretch**: some panels might have hardcoded minimum widths or content that doesn't reflow well at narrower proportional widths. Mitigation: test at minimum window size during implementation.
2. **Level Editor viewport sizing**: the framebuffer texture and OpenGL viewport are tied to the Level Editor panel size. Changing how that panel is sized could break FBO dimensions or mouse coordinate mapping. Mitigation: the existing code already reads `GetContentRegionAvail()` — as long as the positioned window has the right size, this should work unchanged.
3. **Tab bar in bottom panel**: combining three currently-separate panels (Asset Browser, Animation, Console) into tabs within one window means their Begin/End calls change shape. If any panel assumes it's the sole owner of its window, that assumption breaks. Mitigation: each panel's Render() already begins with `ImGui::Begin("PanelName")`; we'll change these to `ImGui::BeginTabItem("PanelName")` within a shared outer window.
4. **MenuBar rendering**: the MenuBar panel currently relies on ImGui's main menu bar or docked window title bar. Without docking, it needs to be a standalone `BeginMainMenuBar()` or positioned window at the top. Currently uses `ImGui::BeginMainMenuBar()` so this should be unaffected.
5. **Scrollbar rounding style**: the pill-style scrollbar (`ScrollbarRounding = 99`) should still work in positioned windows, but verify visually.

## Success criteria

1. Editor launches with panels in a fixed card layout matching the reference image: Property Editor on the left, Level Editor in the center, Hierarchy on the right, tabbed bottom panel with Asset Browser / Animation / Console
2. Visible dark gaps between all panels with rounded corners on every panel
3. Resizing the editor window scales all panels proportionally — no panel overlaps or disappears
4. A minimum window size prevents panels from becoming unusable
5. All existing panel functionality works: entity selection, property editing, asset browsing, animation editing, console output, viewport interaction (zoom, pan, entity drag, selection box, context menus)
6. Build succeeds with 0 errors, smoke test passes

## Test extensions required

None — this is pure additive UI/layout. The smoke test doesn't exercise the editor layout (it runs headless). Verification is visual.

---

## Decisions during implementation

<!-- Append-only log -->

## Spec amendments

<!-- Append-only log -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Functional "..." ellipsis menus (panel-specific options, e.g. "Reset layout")
- User-resizable panel borders (drag-to-resize between panels)
- Collapsible/hideable panels
- Save/restore custom layout preferences
