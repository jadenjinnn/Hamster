# Session handoff

## 2026-05-25 — spritesheet finished + closed out; Windows-installer spec drafted (awaiting approval)

Shipped + pushed to origin/master this session: spritesheet stages 7–9 + close-out (spec moved to `docs/features/shipped/`), bug fixes 0011/0012/0013, SpritesheetEditor confirm-on-close, the `Assets/{Textures,Scripts,Animations}` project structure + folder-scoped asset browser (breadcrumb + back button), a large editor polish pass, and `docs/architecture.md` brought current.

- **New feature spec drafted, NOT committed**: `docs/features/active/windows-installer.md` (Phase 6 — Design tier). Status **draft**, **awaiting "spec approved"** before any code. Open question: commit the spec or keep it local until reviewed.
- Design calls already baked into the spec (don't re-litigate): bundle Python via the **embeddable package + `._pth`** (self-locating, isolated, no interpreter-bootstrap code change); **Inno Setup** installer (per-user or per-machine); **app-local MSVC runtime DLLs** (not static `/MT`); install tree mirrors `<exe>/../share/Resources/...`; two small code touches (ImGui ini → `%APPDATA%`, default project dir → Documents). Editor-only; game-export out of scope (no runtime-only player exists).
- Installer verification is inherently a **manual clean-machine (VM / fresh user, no Python, no VS)** checklist — the smoke test can't cover it; a structural file-manifest check in `package.ps1` is the in-repo contribution.
- Other open threads (unchanged): **game-ui** still un-closed-out (spec in `active/`); bug **0008** (exit segfault), bug **0010** (zoom-out FPS); latent **`AssetManager::GetTexture` `.at()`** footgun (throws on a bad key — only worked around at the PropertyEditor call site); **animation × `Sprite::assetUUID`** unverified (anim swaps `texture`, renderer resolves `assetUUID` first → a sub-sprite-backed animated sprite may not display).
- Working tree at break: dirty with the uncommitted `windows-installer.md` spec + this handoff edit (`CLAUDE.local.md` is gitignored).

## 2026-05-24 — spritesheet stage 7 shipped (code); manual verify blocked by bugs 0011–0013

- **Shipped this session (committed)**:
  - `a7c3cafc` `feat(spritesheet): AnimationPanel accepts multi-sub-sprite drop` — stage 7 of spritesheet-support. Timeline's `InvisibleButton` now also acts as a drag-drop target for `HAMSTER_SUBSPRITE_UUIDS` (`uint32 count + count × boost::uuids::uuid`). On drop, N keyframes appended in selection order. First-keyframe time = cursor X on bar if duration > 0, else `back().time + 0.1s` (or `0.0s` if empty). Step = `0.1s` (panel's existing convention), capped at `60.0s`. Last inserted keyframe selected, panel marked dirty. Smoke 33/33 still green. No new smoke per spec — stage 7 is manual-test only.
  - `<bug-log commit>` `docs(bugs): log 0011 + 0012 + 0013 from spritesheet stage 7 verification`.
- **Blocked — stage 7 manual end-to-end can't run cleanly until**:
  - **Bug 0011** (High) — SpritesheetEditor delete (Del key + X button) does not remove regions. Hypothesis: Del needs ImGui keyboard nav (dropped in 2026-05-16) or the row's `Selectable` shadows the X button (full-width `{0, ...}` claim swallows the click). Needs `/bug-fix` Tier 2 investigation.
  - **Bug 0012** (Critical) — imported textures + their `.sheet` sidecars don't persist through project close/reopen. Root-cause hypothesis: imports never trigger `Project::SaveCurrentProject`; the only save paths are `Scene::RunSceneSimulation` (on Play) and `Application::~Application` (on exit), and the dtor save is likely amplified by bug 0008 (exit segfault). The sidecar file is on disk and intact (re-importing the same PNG reads it correctly) — the project blob just doesn't reference the texture, so `AssetManager::Deserialise` never asks for the sidecar to be read.
  - **Bug 0013** (Low / UX) — author wants expanded sheet mini-cards to lay out as a horizontal strip below the parent card, not tile into the parent grid. Will be touched in the same `AssetBrowser.cpp` change as 0012.
- **Approved triage**: pause stage 7 → fix 0012 + 0013 in one commit (same panel/area) → log 0011 (done) → resume stage 7 verify. Bug 0011 implementation deferred.
- **Proposed fix for 0012 + 0013 (drafted, awaiting "go")**:
  1. `AssetBrowser.cpp:241` (sync Import Spritesheet) — after `AddTexture(path)`, call `Hamster::Project::SaveCurrentProject(m_AssetManager)`.
  2. `AssetBrowser.cpp:228` (async Import Texture) — TBD: either wire enqueue-time save or convert to sync; user's actual workflow is the sync path.
  3. `SpritesheetEditor.cpp:442–477` (Save handler) — belt-and-braces: after `SheetSidecar::Write`, before `Close()`, also call `Project::SaveCurrentProject(m_AssetManager)`.
  4. Bug 0013: rework `AssetBrowser.cpp:379–~440` expansion block so mini-cards lay out as a single horizontal strip (`BeginChild` with horizontal scrollbar, `SameLine()` between cards), flushed to next row before/after.
  5. Verify cwd is stable between `Project::Open` (sets cwd in line 190) and the new save call sites — `SaveCurrentProject` writes `config.Name + ".hamproj"` relative to cwd.
- **Still pending after 0012 + 0013 land**:
  - Stage 7 manual verify (import sheet → slice 3 → Ctrl-click → drag onto timeline → 3 keyframes evenly spaced → Save → Play cycles through).
  - Stage 8 of spritesheet-support: PropertyEditor — sub-sprite drag onto Sprite field + MISSING red label when `ResolveSpriteSource` returns missing.
  - Stage 9 of spritesheet-support: `AssetManager::RenameAsset` moves `.sheet` sidecar alongside `.png` + `.meta`; ProjectWatcher external-rename path also covers `.sheet`.
  - Bug 0011 (delete-region) when convenient.
  - Existing open threads from prior handoffs: FlappyBird sample parked, bug 0008 (exit segfault), bug 0010 (zoom-out FPS drop), game-ui feature close-out.
- **Next concrete step on return**: implement the 0012 + 0013 fix above (likely batch), rebuild + manual-verify the persistence roundtrip, then re-run stage 7 manual verification end-to-end.

## 2026-05-19 — spritesheet support stages 1–6 shipped; 7–9 pending

- **Shipped today (committed, smoke 33/33)**: project-resolution-and-play-window feature (Full spec, 8 stages, closed out — moved to `shipped/2026-05-19-...`), then spritesheet-support stages 1–6:
  - **Stage 1**: SubSprite data + `.png.sheet` sidecar + `AssetManager::AddSubSprite/Remove/Rename` + `ResolveSpriteSource` + `FindAssetByName` + MISSING placeholder (8×8 pink-black checker). Smoke +4.
  - **Stage 2**: Renderer UV plumbing — `DrawSprite`/`SubmitSprite` take `uvRect`; SpriteShader.fs gains `uvRect` uniform; batch path bakes per-vertex UV. Sprite component gains `assetUUID = UUID::GetNil()` default. Scene::OnRender resolves UUIDs via `AssetManager::ResolveSpriteSource`. SceneSerialiser routes either Texture or SubSprite UUIDs through the resolver. (Bug caught mid-stage: default UUID() generates random not nil — all sprites went through MISSING. Fixed.)
  - **Stage 3**: Floating SpritesheetEditor window — draw one rect, save.
  - **Stage 4**: Full slicing UX — multi-rect, click-select, 8 resize handles, drag-move, Del key, side list with editable name + X delete, diff-aware Save with collision check against the unified Texture+SubSprite namespace.
  - **Stage 5**: Asset Browser sheet card chevron caret → inline sub-sprite mini-card grid; mini-cards show clipped texture (uv0/uv1) + name; Ctrl-click multi-select (orange border); drag bundles `HAMSTER_SUBSPRITE_UUIDS` payload (uint32 count + N × `boost::uuids::uuid`).
  - **Stage 6**: "Import Spritesheet" item in the Add-Asset popup + right-click texture card → "Edit Slices..." both open SpritesheetEditor.
  - Post-stage-6 fix: clicks on the slice canvas were moving the editor window (ImGui::Image doesn't consume input) → added InvisibleButton over the canvas. Wheel zoom (cursor-centered, ~12%/notch) added — pinch-equivalent on desktop.
- **Pending — spritesheet stages 7–9**:
  - **Stage 7**: AnimationPanel accepts the `HAMSTER_SUBSPRITE_UUIDS` payload — drop adds N evenly-spaced keyframes in selection order using `defaultStepSeconds`.
  - **Stage 8**: PropertyEditor — sub-sprite drag onto the Sprite field (UUID payload already wires up via stage-2 resolver), plus the MISSING red label when `ResolveSpriteSource` returns missing. Pink-checker placeholder renders in-world via the existing renderer path.
  - **Stage 9**: `AssetManager::RenameAsset` extension — `.sheet` sidecar moves alongside `.png` + `.meta`. ProjectWatcher external-rename path also covers `.sheet`.
- **Other open threads**:
  - FlappyBird sample feature is parked at `docs/features/parked/flappy-bird-sample.md` — to resume after spritesheet ships. Engine prereqs (EntityHandle.transform/set_velocity/apply_impulse) already merged. The author will hand-import sprites + build the `bird_flap` animation in the editor.
  - Bug 0008 (editor segfault on exit) still open — pre-existing, low-priority. Reproduces consistently on every exit (exit code 139 in the background-launch task output).
  - Bug 0010 (zoom-out FPS drop, suspect dot grid) still open. No work done.
  - `docs/features/active/game-ui.md` still in active/ — close-out (architecture.md, decisions.md, README, spec move) was deferred when this session started and never picked back up. Should land before next round of feature work.
- **Active feature folder state**: only `game-ui.md` + `spritesheet-support.md` in `active/`. FB and the two other big features properly parked / shipped.

## 2026-05-18 — game-ui Phases A + B implemented; close-out pending

- **Shipped (functionally complete, smoke green 27/27)**: game-ui Phase A (anchored UIButton + UIText, screen-space hit-test, edit-mode drag, ButtonClickedEvent dispatch, Hierarchy "Add" dropdown, PropertyEditor sections, Python `find_entity_by_name` + `on_button_clicked` virtual). Phase B (FontAtlas via stb_truetype, UITextShader, button labels with textAlign, UIText with wrap, auto-size buttons, runtime label/text mutation from Python).
- **Spec status**: still in `docs/features/active/game-ui.md`. Status flipped to "approved" but the close-out checklist hasn't been run yet.
- **Close-out NOT done** — pick these up when you return:
  1. Update `docs/architecture.md` — add `Renderer::FontAtlas`, `UIButton`/`UIText` to module list, mention `Renderer::ResolveUIButton`.
  2. Add entry to `docs/decisions.md` for the `on_button_clicked` virtual dispatch choice (spec sketch used `self.subscribe(...)` but the body said "existing EventDispatcher pattern" — followed `on_animation_complete` precedent instead).
  3. Move spec: `docs/features/active/game-ui.md` → `docs/features/shipped/2026-05-18-game-ui.md`.
  4. README "What's new" entry.
  5. `/log` to update session log (already done in CLAUDE.local.md but the convention is to run `/log` at close-out).
- **Decisions during implementation** to log in the spec's section before moving it:
  - `on_button_clicked(uuid)` virtual override (not `self.subscribe(EventType.ButtonClicked, fn)`). Mirrors `on_animation_complete`.
  - `find_entity_by_name` exists on **both** `Scene` (`scene.find_entity_by_name`) AND `HamsterBehaviour` (`self.find_entity_by_name`). Scripts use the latter since they don't have a Scene handle in Python.
  - `EntityHandle.set_label(text)` / `set_text(text)` for runtime mutation of UIButton.label and UIText.text respectively. Spec sketch implied direct attribute access but EntityHandle has no live UIButton accessor; setters were the minimal viable surface.
  - Single bake size (32px) — text blurs at extreme zoom. Multi-bake / SDF deferred to Future Work.
  - `Renderer.h` includes `FontAtlas.h` which transitively pulls `stb_truetype.h` into every Renderer.h consumer. Compile-time bloat; cleanup is a forward-decl + pImpl pass, left for future cleanup.
- **Build gotchas hit during implementation (worth remembering)**:
  - `cmake --build --target Hamster-Wheel` does NOT relink `SmokeTest` even when `Hamster-Core.lib` changed — must explicitly add `SmokeTest` to the target list. ABI mismatch from stale SmokeTest.exe caused a segv inside `CreateEntity` (Scene class layout changed when I added m_ClickedButtonsThisFrame + m_ButtonClickedHandle).
  - Editor must be killed before relink — file lock on `Hamster-Wheel.exe`.
- **Known Phase B limits** (already documented in spec's Future Work):
  - UIText is selectable from Hierarchy only — screen-space hit-test still only covers UIButton.
  - ASCII 32–126 only. Non-ASCII renders as `?`. No localisation support.
  - Drag works only for UIButton (UIText would need a measured rect for hit).
- **Test artefact**: `C:\Users\Jaden\Downloads\scenetest\ui_test.py` is a manual test script the user has been using. Will be left in place — not part of the smoke fixtures (those live under `test/fixtures/`).
- **Untracked dirs**: `build-asan/`, `build-release/` still present (long-standing). Bug 0008 still open (low severity exit segv, pre-existing).
- **Next session step**: run the 5 game-ui close-out items above (architecture.md update, decisions.md entry, spec move, README, /log), then `git add -A` + commit + push. After that, the spec status flips from "approved" → "shipped" and the feature is officially closed.

## 2026-05-17 (session 3) — sprite-batching v1 + spatial-index shipped + pushed

- **Shipped + pushed**: sprite-batching v1 (same-texture batching) + simulation-snapshot critical fix (deferred restore) + spatial-index (quadtree culling + fast picking). 3 commits on `origin/master` (4ce42190, 7a4669eb, 54bf6d0d, 959445c1). Smoke 21/21.
- **Resume bullets unlocked**:
  - "Multi-texture sprite batching: ~N → 1 draw call for same-texture scenes (v1 measured: 5000 sprites in 1 batch)"
  - "Quadtree spatial index with viewport-rect culling + O(log N + k) picking, replaces per-frame full-scene FBO + glReadPixels"
  - Numbers should be measured cleanly on a **release build** (`build-release/`) — debug build is 5-10× slower due to MSVC iterator debug level 2; the resume number lives in release.
- **Bug 0008** (`docs/bugs/active/0008-editor-segfault-on-exit.md`) — logged, not investigated. Editor exits with SIGSEGV after `Application::~Application`. Cosmetic (process is exiting anyway) but noisy. Suspected related to bug 0007's class of issue (a latent pybind11 holder finalising on a dead interpreter). **Severity Low** — don't drop higher-leverage work for it.
- **Pre-existing perf gaps surfaced but not fixed** (user explicitly didn't want bug logs for these):
  - **Dot grid in level editor** (`EditorLayer.cpp:291-296`) issues 1 unbatched `DrawFlat` per grid dot — ~5000 calls/frame at default viewport. Pre-existing. Fix would be a flat-shader batch path mirroring sprite-batch v1, or a single-quad procedural-pattern shader.
  - **Hierarchy panel** renders every entity row every frame — at 5000+ entities this dominates ImGui frame time in debug. Fix would be ImGui virtual scrolling / clipper.
- **Renderer's projection-vs-FBO mismatch**: documented in `docs/decisions.md` under "Editor pick coords differ from `Renderer::ScreenToWorldPos`". Worked around in `EditorLayer::PanelMouseToWorld`; the deeper fix is to either teach the renderer about the panel size or to clip the projection rather than the framebuffer. Future work — current workaround is fine.
- **Untracked dirs**: `build-asan/` (long-standing) and `build-release/` (new this session — release build used to confirm perf). Safe to delete `build-asan`; keep `build-release` if you want to re-run the benchmark.
- **Next session step**: Open `scenetest` in `build-release/Hamster-Wheel/Hamster-Wheel.exe`, attach `benchmark_batching.py` with `NUM_SPRITES = 10000`, hit play, **read the draw-call count and FPS from the HUD top-right**, write those numbers into the resume bullet. Then decide what's next — frustum-culling-by-zoom, particle system, tilemap, fix bug 0008, etc.

## 2026-05-17 (session 2) — asset-sidecars shipped (locally); manual UI tests pending; 10 commits unpushed

- **Feature done**: asset-sidecars closed out across 7 phases + bug 0007 fix + close-out docs. Spec at `docs/features/shipped/2026-05-17-asset-sidecars.md`. Smoke test 15/15.
- **Not yet pushed**: 10 commits on `master` since `56a0eba6` (entity-hierarchy). No `git push` ran — user should review the manual-test results first.
- **Manual test checklist** (given to user, not yet run): 11 tests covering fresh-project sidecar writes, editor + external rename, sloppy-rename + MISSING UI, simulation guard, subdirectories + dotted imports, same-name-across-folders. Tests 5–11 exercise the Win32 file watcher + subdir browser UI which couldn't be smoke-tested. **Anything that fails should be reported with the test number and the exact symptom.**
- **Bug 0003** (`docs/bugs/active/0003-getscript-throws-on-missing-uuid.md`) is effectively resolved by Phase 5: `AssetManager::GetScript` now returns `nullptr` instead of throwing. Worth moving the file to `closed/` and writing a short verification note — but I left it active so author can confirm intent before closing.
- **Bug 0007** discovered + fixed: `Application::~Application` finalized Python before pybind member dtors ran (latent UB; smoke segfault at exit). Fix moved `FinaliseInterpreter()` to the LAST line of the dtor with explicit `m_Scenes.clear()` + `m_AssetManager.reset()` before it. See `docs/bugs/closed/0007-...md`.
- **Leftover on disk**: `build-asan/` directory from earlier leak-investigation (still gitignored / untracked). Safe to delete to reclaim disk if you want.
- **Next session step**: run the manual test checklist (Q&A response in chat). If everything passes, `git push origin master`. If something fails, fix → smoke → push.

## 2026-05-15 (session 4) — Bug 0002 investigated, fix approved, implementation deferred

- **Phase 1A measurement results** (done):
  - Idle is clean — CPU 5–6% on a 16-core box ≈ 1 core, FPS locked at 60 matching refresh. **Vsync is somehow already working** despite no `glfwSwapInterval(1)` in our source. Original plan's #1 fix is now de-prioritized.
  - No idle memory / handle / GDI / user-object growth over several minutes.
- **Project-switch leak confirmed** — Task Manager Handles 588 → ~1500 over 8 open/close cycles via `File → Open Project` (≈ +114 handles/cycle). UI duplication symptom (Add Component dropdown listing every option × N, Property Editor "select an entity first" × N) pinned the root cause to layer-stack accumulation.
- **Bug 0002** (`docs/bugs/active/0002-project-switch-handle-leak.md`) — Tier 2, severity High, status `investigating`. Investigation, root cause, fix plan, and author rubber-stamp are all in the bug file. Three compounding root-cause threads:
  1. `ProjectHubLayer::OnAttach` subscribes to `ProjectOpened`, discards the `SubscriptionHandle`, never unsubscribes. Lambda outlives the hub's tenure in the layer stack.
  2. `LayerStack::PopLayer` removes from the vector + calls `OnDetach` but never `delete`s the layer. Hub heap allocation + its captured lambda persist forever.
  3. No swap-main-layer API; the hub's orphaned lambda is the only `ProjectOpened` subscriber and naively `PushLayer`s a new `EditorLayer` on every fire without popping the previous one. Each accumulated `EditorLayer` keeps its FBO + logo `Texture` + panel objects + panel-level dispatcher subscriptions alive.
- **Fix plan (approved, not yet implemented)** — 4 files:
  1. `Hamster-Core/src/Core/LayerStack.cpp` — `PopLayer` should `delete layer` after `OnDetach` (LayerStack owns layers transferred at push).
  2. `Hamster-Core/src/Core/Application.cpp` — `~Application` should pop+delete remaining layers before the existing scene-save loop.
  3. `Hamster-Wheel/src/ProjectHubLayer.cpp` + `.h` — remove the `ProjectOpened` subscription entirely; remove `OnAttach`; remove `m_EditorLayer` member.
  4. `Hamster-Wheel/src/main.cpp` — install one persistent `ProjectOpened` handler that tracks the current main layer via a captured local, pops + deletes the previous one, pushes a new `EditorLayer`.
- **Scope note**: this fix does **not** address missing GL destructors on `Texture` / `Shader` / `FramebufferTexture` — those still leak driver-side handles per construction, but after this fix each construction only happens once per actual switch instead of stacking. Re-measure handle growth after the fix; if non-zero, follow up with destructor work as a separate bug.
- **Smoke test impact**: none expected — no Python/simulation changes.
- **Verification after fix**: re-run the bug's reproduction (10 project switches via `File → Open Project`); Handles should stay flat.
- **Next session step**: implement the 4-file fix, build, run smoke test, then re-run the handle-count reproduction to verify it's flat. After commit, decide whether residual handle growth warrants logging a follow-up bug for the GL destructors.

## 2026-05-15 (session 3) — Performance/leak investigation queued for measurement

- Plan at `C:\Users\Jaden\.claude\plans\velvety-crafting-sketch.md` (saved as `velvety-crafting-sketch.md`, approved). Read-only audit identified four likely causes of system slowdown when running Hamster:
  1. **No `glfwSwapInterval`** anywhere in our source — main loop runs uncapped, pegs a CPU core.
  2. **`Texture`, `Shader`, `FramebufferTexture` have no destructors** — every `glGen*` leaks its GL handle. Renderer's `VBO` is also generated into a local var that goes out of scope, leaking on every Renderer construction.
  3. **`FramebufferTexture::ResizeFrameBuffer` called every frame unconditionally** at `EditorLayer.cpp:246` — two GPU allocs/frame.
  4. **Hover-pick re-renders the whole scene** to the FBO + `glReadPixels` stall every hovered frame at `EditorLayer.cpp:74–105`. Combined with the main display render, that's 2–3 full scene draws/frame while hovering.
- Lower-severity findings (Scene without destructor, per-frame Python `attr` lookup, per-frame EnTT group sort, unbounded main-thread queue) listed in the plan.
- **ASan was tried first, did not work**:
  - Windows ASan does not support leak detection (`detect_leaks=1` prints "not supported on this platform"). Pre-build attempt confirmed.
  - Smoke test crashes inside ASan's instruction interceptor: `interception_win: unhandled instruction` (known Win11 issue, likely Python interop).
  - `build-asan/` directory left intact for possible future use-after-free / heap-overflow checking — feel free to delete it if reclaiming disk space.
- **Plan pivoted to Visual Studio 2022 Diagnostic Tools (Memory Usage)** for heap leak detection. VS 2022 Enterprise already installed. No rebuild needed — runs on the existing `build/Hamster-Wheel/Hamster-Wheel.exe`.
- **Queued user-driven measurements** (not yet run):
  1. Task Manager symptom check — CPU%, Memory, Handles, GDI, GPU%, FPS at idle + while hovering viewport for 30s + after 5min idle.
  2. VS 2022 Performance Profiler → Memory Usage → attach to Hamster-Wheel.exe → snapshot → open/close 5 projects → snapshot → diff the heap.
  3. (Optional) RenderDoc Resource Inspector for GL handle counts across project switches.
- **Next session step**: run the queued measurements above and paste the readings / heap-diff screenshot back so we can confirm which findings are real and prioritize fixes (vsync is almost certainly fix #1 regardless).

## 2026-05-15 (session 2) — UI polish + project hub port + windows snap

- Committed `302ce5f9` and pushed (129 files, +44860/-2376). This bundle also captured the prior editor-rewrite work that had never been committed.
- **Resolved from previous handoff**:
  - ProjectHubLayer ported from Hamster-Wheel-old; `main.cpp` pushes it instead of EditorLayer directly. `ProjectOpened` event triggers the layer swap.
  - Hardcoded debug project path removed from `main.cpp`.
- **Still deferred**:
  - `RenameModal` wiring — only the AssetBrowser texture rename path uses it. Hierarchy entity rename + PropertyEditor name-field rename modal not wired.
  - Title bar drag + Aero Snap are Win32-only (`#ifdef _WIN32`). Linux port deferred.
  - Portable serialization still open (architecture.md open question).
- **Architectural notes worth remembering**:
  - Borderless-with-snap pattern lives in `Window.cpp` via `SetWindowSubclass` + WM_NCCALCSIZE clamping to `mi.rcWork` when `SW_SHOWMAXIMIZED`. WS_THICKFRAME is required for Aero Snap; GLFW_DECORATED=FALSE strips it, so the subclass re-adds it.
  - Initial viewport size fix: Application ctor posts a synthesized `FramebufferResizeEvent` after subsystems are wired, so the renderer picks up the real maximized size instead of the hardcoded 1920×1080.
  - Panel focus state is read from `ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)` in `Panel::DrawHeader`/`DrawTabbedHeader`. No global focus tracking — mutual exclusion is free.
  - First-render auto-focus is avoided via `ImGuiWindowFlags_NoFocusOnAppearing` on panel windows; LevelEditor uses `SetNextWindowFocus` once on first frame so it's the default-selected panel.
- **Next session step**: nothing specific. Options — wire RenameModal into Hierarchy/PropertyEditor, work an active spec, or pick from architecture.md open questions (portable serialization).

## 2026-05-15 — Editor rewrite from prototype shipped

- **`Hamster-UIPrototype/` → `Hamster-Wheel/`** rename complete. The new editor is the prototype with engine deps and per-panel data wiring added. Old editor preserved as `Hamster-Wheel-old/`, not built (`add_subdirectory` commented in root `CMakeLists.txt`).
- All 6 phases complete: build wiring, engine entry + FBO blit, per-panel data wiring, LevelEditor interactivity, MenuBar + modals, disable docking + rename. Smoke test passes.
- Spec moved to `docs/features/shipped/2026-05-15-editor-rewrite-from-prototype.md`. Old `panel-layout` spec parked at `docs/features/parked/panel-layout.md` (superseded).
- **Hamster-Core change**: `Application::Application(const WindowProps &)` added; `WindowProps.borderless` field added. Default constructor delegates to the new one with default props. Docking flag removed from `ImGuiLayer.cpp`.
- **Deferred / not yet wired (no spec, follow-ups when relevant)**:
  - `RenameModal` is compiled and instantiable but not yet wired into rename context menus (Hierarchy entity rename, AssetBrowser asset rename). PropertyEditor still has the rename modal field commented out in its sections.
  - ProjectHub for v1 was skipped — new editor opens the most-recently-opened project directly from `ProjectRegistry`. v2 should port `ProjectHubLayer` and push it before `EditorLayer` (or via `ProjectOpened` event swap).
  - Hardcoded default-project debug path in `main.cpp` (`C:\Users\Jaden\Downloads\Untitled12\Untitled12.hamproj`) — remove once a proper "no projects" fallback / ProjectHub path is in.
  - Title bar drag is Win32-specific via `GLFW_EXPOSE_NATIVE_WIN32`. Linux support deferred.
- **Other in-flight specs**: `docs/features/active/scene-viewer-improvements.md` is still in active. Unrelated to this work.
- **Next session step**: nothing specific. Pick from open questions in `architecture.md` (portable serialization format), or work the active scene-viewer-improvements spec.

## 2026-05-14 (session 2) — Panel layout prototype finalized, ready for porting

- **Hamster-UIPrototype** standalone target is now fully polished and split into clean architecture:
  - `src/Theme.h/.cpp` — color palette (14 colors), font globals, ApplyTheme(), LoadFonts()
  - `src/Panel.h/.cpp` — Panel struct: DrawHeader(), DrawTabbedHeader(), BeginContent()/EndContent() with scroll support
  - `src/Components/Components.h/.cpp` — reusable UI helpers: SectionHeader, HButton, HToolbarButton, HCombo, HDragFloat, HCheckbox, AxisDotInput, SectionSeparator
  - `src/Panels/` — PropertyEditor, LevelEditor, Hierarchy, AnimationPanel, BottomPanel (tabbed Asset Browser + Animation)
  - `src/main.cpp` — entry point, custom borderless title bar with drag/maximize, layout math
- Logo added: `Resources/Logo/hamster-logo.png` (128x128, white-on-transparent, converted from `Untitled.svg`), rendered in title bar via DrawList::AddImage
- Key layout constants: 8px gaps, 16px side padding, 8px header-to-content gap, 36px header height, 32px title bar
- Panel proportions: left 17.5%, right 14.5%, bottom 28% of content area
- **Next step**: port prototype code back into EditorLayer.cpp and Hamster-Wheel panels — the prototype files map 1:1 to real editor panels
- Spec at `docs/features/active/panel-layout.md` still active

## 2026-05-14 (session 1) — Panel layout: standalone UI prototype approach

- Previous incremental approach (editing EditorLayer directly) was scrapped — all Hamster-Wheel changes reverted via `git restore`
- New approach: **Hamster-UIPrototype** standalone target builds against imgui+glfw+glad only, no engine deps
- Prototype closely matches the Stitch reference design after many iterations
- See session 2 for final state

## 2026-05-13 — Animation system fully shipped

- Feature closed out: spec moved to `docs/features/shipped/2026-05-13-animation-system.md`, architecture.md updated, README updated
- Committed as 370efe28, pushed to origin/master
- No pending work from this feature

## 2026-05-12 — Rounded panels attempted and scrapped

- Tried 3 approaches to get rounded docked panels: style-only overlay, draw-list rect inside each panel, and patching ImGui's dock node rendering (`imgui.cpp` lines 6744, 16408, 17887)
- Root cause: ImGui docking forces `WindowRounding = 0` on viewport-owning host windows, and the dock node rendering uses `host_window->WindowRounding` (always 0) instead of `style.WindowRounding`
- Even after patching to use `style.WindowRounding`, the panels still appeared un-rounded — likely because the host window's own background fills edge-to-edge and covers the rounded corners before they're visible
- All changes fully reverted — no residual modifications in any file
- Conclusion: achieving the reference image's card look may require a more invasive ImGui fork (custom dock node layout with per-node margins) or abandoning ImGui's built-in docking for a manual panel layout

## 2026-05-11 — Collider editor spec approved

- Collider editor feature spec approved at `docs/features/active/collider-editor.md`
- Adds `colliderOffset` and `colliderSize` fields to Rigidbody, visual editor window with drag handles
- Next step: run `/implement collider-editor` to begin coding
- All box2d bugfixes and Python QoL changes from earlier this session are committed and pushed

## 2026-05-11 — UI polish pass shipped

- Bold font variants loaded (Inter-Bold 16px + 18px) for typographic hierarchy
- Property editor: bold section headers and field labels, increased spacing between component sections
- Hierarchy: flat entity list (no root node), search bar with filter, full-width selection highlight
- Asset browser + file browser: grid card layout with icons, centered labels, faint borders, ellipsis truncation
- Zoom slider: moved to bottom-right, magnifying glass icon, pill-shaped track, percentage label
- Axis gizmo: repositioned to bottom-right above zoom slider
- Feature workflow updated: Claude now drafts "Why this approach" section (author reviews during approval)

## 2026-05-11 — Scene viewer improvements shipped

- Scene viewer feature complete: dot grid, right-click context menus, zoom slider, axis gizmo, 8-handle selection box
- FBO pick bug from 2026-05-10 resolved: grabber hit areas were too small (16px centered on corners), not a render-order issue. Fix: 24px invisible pick zones, larger than the 8px visual squares
- Selection box reworked: 8 white squares with blue outlines (4 corners + 4 edge midpoints), blue entity outline, replaces old 4-corner circle design
- FlatShader: `circleMode` replaced with `borderMode`/`borderWidthX`/`borderWidthY` for outline rendering
- Edge grabbers constrain resize to one axis (top/bottom = height, left/right = width)

## 2026-05-09 (session 3)

- DI refactor complete (7 commits): Scene, Project, Panel, ImGuiLayer, Scripting, AssetManager, EditorLayer, ProjectHubLayer all receive dependencies via constructors
- 0 singleton calls remain in Hamster-Core; 2 remain in Hamster-Wheel (editor-level, appropriate)
- HAMSTER_LOG macro removed; replaced with direct m_ClientLogger->Log() calls
- Remaining Phase 5 work: AssetManager still all-static (lifecycle/ownership), Renderer static state
- Serialization portability still deferred until before Phase 6
- Smoke test guardrail: `ctest --test-dir build -R SmokeTest`

## 2026-05-09 (session 2)

- Phases 2-4 complete, all committed and pushed
- Phase 5 starts with dependency injection refactor:
  - Pass EventDispatcher, etc. through constructors instead of reaching through Application singleton
  - Start with Scene — most painful coupling point
  - User understands the "ownership vs access" distinction and is on board
- Other Phase 5 architectural work: AssetManager/Renderer static state, HAMSTER_LOG macro decoupling
- Serialization portability deferred until before Phase 6
- Smoke test guardrail: `ctest --test-dir build -R SmokeTest`
