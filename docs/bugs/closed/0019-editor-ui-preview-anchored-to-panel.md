# Bug 0019: editor previews screen-space UI anchored to the panel, not the play area

> Status: **fixed**
> Severity: **Low**
> Tier: **2**
> Logged: 2026-05-26
> Found while: building the Flappy Bird demo game

---

## Symptom

A `UIText`/`UIButton` renders in the wrong place in the **editor** viewport (anchored to the Level Editor panel's corner) but is **correct in play** (anchored to the play window). E.g. a `TopLeft` UIText sits at the panel's top-left rather than the play-area box's top-left.

## Root cause

UI is screen-space — anchored to whatever render target it's drawn into. In play, the target is the popout window, which *is* the play area, so anchors are correct. In the editor, `EditorLayer` rendered the UI pass against the whole Level Editor panel (`OnRenderUI(m_LevelEditorAvailRegion...)`), so anchors resolved to the panel, not the play-area box. Compounding it, the editor's world projection covers the full window framebuffer while rendering into a panel-sized FBO (the documented panel-vs-window offset behind `PanelMouseToWorld`), so a naive panel-space UI viewport wouldn't line up with the play-area box anyway.

## Fix

`EditorLayer.cpp` UI pass now anchors the preview to the play-area box. The play-area world rect `(0,0)→(TargetWidth, TargetHeight)` is mapped to an FBO viewport via the inverse of `PanelMouseToWorld` (accounting for `vpH - panelH`), and the UI is rendered sized to the target resolution into that viewport. So the editor preview matches the play-window placement. Falls back to the full panel when there's no active project.

## Verification

- Build compiles; editor: a UIText anchored TopLeft now previews at the play-area box's corner and tracks it under camera pan/zoom; matches the popout. Confirmed by author.

---

## Related

- The panel-vs-window-framebuffer projection offset — see `docs/decisions.md` "Editor pick coords differ from Renderer::ScreenToWorldPos".
