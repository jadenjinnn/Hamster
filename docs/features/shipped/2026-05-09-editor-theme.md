# Feature spec: Editor Theme

> Tier: **Design**
> Status: **approved**
> Started: 2026-05-09
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: Purely visual changes that are easy to undo, but touches every panel plus adds font/icon assets and changes StartPauseModal layout.

---

## Problem

The editor currently uses ImGui's default theme, which looks like an internal debug tool rather than a user-facing product. For a resume-quality engine, the editor needs to feel polished and intentional. Every panel has inconsistent spacing, no custom font, and the default grey-on-grey color scheme doesn't communicate "this is a real tool." This is a blocker for presenting Hamster as a serious project.

## In scope

- Custom ImGui theme: dark, muted palette with subtle borders, Figma-inspired (low-contrast backgrounds, sparse accent color, generous padding, rounded corners)
- Ship Inter (or similar clean sans-serif) as the editor font, loaded at startup
- Icon set for common actions (play, pause, stop, add, delete, etc.) — loaded as an ImGui font atlas or texture atlas
- Consistent styling applied to all existing editor panels: Hierarchy, PropertyEditor, FileBrowser, AssetBrowser, Console, MenuBar
- Rework StartPauseModal: remove as standalone modal, overlay play/pause/stop icons onto the scene viewport panel (Level Editor)
- Centralized theme code: one place to define all colors, sizes, fonts, spacing — not scattered across panels

## Out of scope

- Project Hub (create/open screen) restyling — separate future feature
- Adding new panels or removing existing panels (other than StartPauseModal overlay change)
- Custom window chrome / native title bar
- Dark/light theme toggle — dark only for now
- Custom-drawn widgets beyond what's needed for the overlay and basic styling
- Animated transitions or hover effects beyond ImGui's built-in capabilities

## API sketch

```cpp
// Theme initialization — called once during EditorLayer::OnAttach
// Centralized in a single file so all style constants live together
HamsterTheme::Apply(ImGuiIO &io);

// Font loading
ImFont *primaryFont = io.Fonts->AddFontFromFileTTF(
    (execPath + "/../share/Resources/Hamster-Wheel/Resources/Fonts/Inter-Regular.ttf").c_str(),
    16.0f);

// Icon font merged into the same atlas
static const ImWchar iconRanges[] = { ICON_MIN, ICON_MAX, 0 };
ImFontConfig iconConfig;
iconConfig.MergeMode = true;
io.Fonts->AddFontFromFileTTF(iconFontPath, 16.0f, &iconConfig, iconRanges);
```

```cpp
// StartPause overlay — drawn inside EditorLayer::OnImGuiUpdate
// after the scene viewport image, positioned at top-center of viewport
ImGui::SetCursorPos(overlayCenterPos);
if (ImGui::ImageButton("play", playIconTexture, {32, 32})) {
    // toggle simulation
}
```

---

## Design (Design tier)

### Data structures

- `HamsterTheme` — new static utility class or free function in `Hamster-Wheel/src/Theme/HamsterTheme.h/.cpp`. Contains:
  - `Apply(ImGuiIO &io)` — sets all ImGuiStyle values, loads fonts, configures color palette
  - Color constants as named `constexpr ImVec4` values for reuse
- Icon font or icon texture atlas — asset file in `Resources/Fonts/` or `Resources/Icons/`
- No new runtime data structures; this is configuration applied at startup

### Module touchpoints

- `Hamster-Wheel/src/Theme/HamsterTheme.h/.cpp` — **new files**, theme definition
- `Hamster-Wheel/src/EditorLayer.cpp` — call `HamsterTheme::Apply()` in `OnAttach`, draw play/pause overlay in `OnImGuiUpdate`
- `Hamster-Wheel/src/Panels/StartPauseModal.h/.cpp` — gutted or removed; functionality moves to EditorLayer overlay
- `Hamster-Wheel/src/EditorLayer.h` — remove `m_StartPauseModal` member, add overlay state
- `Hamster-Wheel/CMakeLists.txt` — add new Theme source files
- `Resources/Fonts/` — new directory with Inter font file(s) and icon font
- Individual panel `.cpp` files — may need minor adjustments if any panel uses hardcoded style pushes that conflict with the global theme

### Lifecycle / control flow

1. `EditorLayer::OnAttach()` calls `HamsterTheme::Apply(ImGui::GetIO())` once after ImGui is initialized
2. Theme sets all `ImGuiStyle` colors, sizes, rounding, padding, and loads the font atlas (Inter + icon font merged)
3. All panels render as before — they inherit the global style automatically
4. `EditorLayer::OnImGuiUpdate()` draws the play/pause overlay after the scene viewport image, using icon font glyphs or icon textures
5. Overlay buttons trigger the same simulation start/pause/stop logic currently in StartPauseModal

### Edge cases

- Font file not found at runtime: fall back to ImGui default font, log a warning — don't crash
- Icon font/atlas not found: fall back to text labels ("Play", "Pause") instead of icons
- Viewport too small for overlay: hide or shrink the overlay buttons below a minimum viewport size threshold
- Panels that push their own style colors (check each panel for `PushStyleColor` calls that might fight the theme)

---

## Why this approach (AUTHOR WRITES THIS — Claude must not draft)

<!-- AUTHOR: write your rationale here -->
I considered rewriting the entire codebase using QT instead, my rationale being that most enterprise software use QT and it would look "better". However, this would require a massive overhaul and honestly most developer-forward applications like game engines don't really use QT anyways, a minority do but the majority use a custom UI drawing framework. ImGUI can be made to look just as nice with custom widget drawing. A possible tradeoff would be QT maybe more crossplatform compatible but for this project I don't think that's really a big concern.

## Risks / what could go wrong

- **Font licensing**: Inter is OFL-licensed (free for all use), but if a different font is chosen, licensing must be verified before shipping. Shipping a font without checking its license is a legal risk for a portfolio project.
- **Icon font coverage**: If we pick an icon font (e.g., Font Awesome, Material Icons), we need to verify it covers all the actions we need. Missing icons mid-implementation would force fallback to text or a second icon source.
- **Panel hardcoded styles**: Some panels may have `PushStyleColor`/`PushStyleVar` calls that override the global theme in ways that look wrong after restyling. Each panel needs to be audited, and those overrides either removed or updated to work with the new palette.
- **StartPauseModal removal breaks simulation flow**: The overlay replaces the modal's functionality. If the wiring is wrong, play/pause/stop could silently fail or trigger in the wrong order. The simulation start/pause logic must be tested end-to-end after the move.
- **DPI scaling**: Inter at a fixed pixel size may look wrong on high-DPI displays. ImGui supports font scaling, but we need to test on at least one high-DPI screen or the font will look tiny/blurry.

## Success criteria

- Editor launches with the custom theme visible on all panels — no ImGui default grey anywhere in the editor (Project Hub excluded)
- Inter font renders clearly at the chosen size on a 1080p display
- Play/pause/stop overlay is visible and functional on the scene viewport; clicking play starts simulation, pause pauses it, stop resets it — same behavior as the old modal
- Old StartPauseModal is removed from the codebase; no dead code left behind
- A screenshot comparison (before/after) demonstrates the Figma-inspired dark aesthetic: muted backgrounds, subtle borders, accent color on interactive elements
- Smoke test still passes (theme changes should not affect C++/Python boundary or simulation logic)

## Test extensions required

None — this is pure additive UI styling. The smoke test exercises the C++/Python boundary and simulation logic, which are unaffected by visual theme changes. The success criteria are verified visually (screenshot comparison) and by confirming the smoke test still passes unchanged.

---

## Decisions during implementation

<!-- Append-only log. Updated by Claude during implementation. -->

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Project Hub restyling to match the new theme
- Dark/light theme toggle
- Custom window chrome / native title bar
- Per-panel color accent customization
