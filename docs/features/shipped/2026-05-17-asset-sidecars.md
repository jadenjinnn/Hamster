# Feature spec: asset-sidecars

> Tier: **Full spec**
> Status: **approved**
> Started: 2026-05-17
> Spec author: Jaden

---

## Classification

- **Reversibility**: Sticky
- **Scope**: Cross-cutting
- **Tier rationale (1 sentence)**: Changes the on-disk project format (new `.meta` sidecars), changes scene serialisation (denormalised asset names), and touches AssetManager, Asset Browser, Property Editor, Python import setup, and a new file-watcher module — Sticky × Cross-cutting → Full spec.

---

## Problem

Today every script in a Hamster project lives on disk as `Untitled_Script_<UUID>.py` — the UUID is baked into the filename because the editor needs a stable key and has no other way to bind a Python file to its in-engine identity. The result is three pains: scripts look ugly in the asset browser and the file system; the user can't tell two scripts apart at a glance; and sharing or hand-editing scripts across projects is painful because the filename is owned by the engine, not the author. Textures and animations have the same latent rename/identity problem — today nobody hits it because their UUIDs travel via the binary `AssetManager::Serialise` blob and renames just aren't supported. We want assets that the user can name freely (`player_movement.py`, `enemy_spritesheet.png`), survive renames from outside the editor, and still keep their entity attachments intact.

## In scope

- Sidecar `.meta` files (`<asset_filename>.<ext>.meta`) for every project asset: scripts, textures, animations.
- `.meta` content: JSON, one field today: `{"uuid": "abc-123-..."}`. Extensible later for per-asset settings.
- `AssetManager` reads UUIDs from sidecars on project open. Files without a sidecar get a fresh UUID written.
- Orphan sidecars (`.meta` whose paired asset is gone) are removed on next project open.
- File-system watcher (Win32 `ReadDirectoryChangesW`) on the project directory, debounced ~150ms, reacts to add / delete / rename / modify events.
- Allow subdirectories. Script imports use Python's dotted module convention (`enemies.boss` for `enemies/boss.py`).
- Unique-name rule: within a single folder, no two assets of the same type can share a name. Cross-folder duplicates allowed.
- Asset Browser rename (already wired via right-click → Rename for textures): renames both the asset and its `.meta` atomically. Refuses if the new name collides in the same folder. Allowed during simulation.
- Asset Browser gains folder navigation (current-folder breadcrumb + folder cards) so subdirectories are usable.
- Scene file format gains a denormalised "last-known name" cache on Behaviour (script name) and Sprite (texture name) so missing-asset UI can still display something meaningful.
- Missing-asset UI in Property Editor: red placeholder showing the cached name, single-click "reassign" dropdown.
- Simulation start refuses if any selected entity's Behaviour has a missing-script reference; logs: `Entity '<name>' references missing script '<cached_name>' — did you rename or delete it?`
- Old projects (those with `Untitled_Script_<UUID>.py` filenames) are not migrated. Opening one is undefined-behaviour — user should treat them as throwaway. Documented in the close-out README note.

## Out of scope

- Hot-reload of scripts mid-simulation when a `.py` is edited externally. (File watcher reacts to renames/adds/deletes, not "module needs reload after content change".)
- Migration from old `Untitled_Script_<UUID>.py` projects. They are discarded.
- Cross-platform file watcher. Win32 only, consistent with the editor's existing Win32-only chrome (Aero Snap, drag).
- Sidecars for scene files (`.hamscene`), project files (`.hamproj`), the `Hamster.pyd` engine artifact, or any generated files (`__pycache__`, `.pyc`).
- Folder-tree navigation in the asset browser (a single-level breadcrumb is enough for v1; nested tree view is future work).
- Drag-and-drop reorganisation in the asset browser (e.g., dragging a script into a folder card). Out of scope; user reorganises via file system.
- Content-hash–based rename recovery (option D from design discussion). Accepted risk: a sloppy external rename that only moves the `.py` will break attachments; user fixes via the missing-asset reassign dropdown.
- Per-asset import settings (filter mode for textures, etc.) inside `.meta`. The schema is forward-compatible but only `uuid` is written today.

## API sketch

```python
# User has player_movement.py in their project. Attaches it to an entity via
# the property editor as today. From Python, nothing changes — UUID-based
# attachment is still the underlying contract.

class PlayerMovement(Hamster.HamsterBehaviour):
    def on_update(self, dt):
        if self.key_pressed(Hamster.key_code.W):
            self.apply_force(0, 100)
```

```python
# Subdirectories work as Python expects — dotted imports under the hood.
# enemies/boss.py is imported as `enemies.boss`. The user doesn't write the
# import themselves; the engine resolves the class through its UUID.

# enemies/boss.py
class Boss(Hamster.HamsterBehaviour):
    def on_create(self):
        self.log("boss spawned")
```

```cpp
// AssetManager API gains a name-aware lookup. UUID is still the canonical key;
// name is a convenience for the asset browser and missing-asset display.

Hamster::UUID uuid = m_AssetManager->GetScriptUUIDByPath("enemies/boss.py");
auto script = m_AssetManager->GetScript(uuid);

// On rename, the editor calls:
m_AssetManager->RenameAsset(uuid, "enemies/final_boss.py");
// → atomically renames both enemies/boss.py and enemies/boss.py.meta on disk,
// updates internal indices, posts AssetRenamedEvent.
```

```jsonc
// player_movement.py.meta
{
  "uuid": "01234567-89ab-cdef-0123-456789abcdef"
}
```

---

## Design

### Data structures

- **`.meta` file on disk**: tiny JSON document. Today `{"uuid": "<uuid>"}`. Lives next to its paired asset, named `<asset_basename>.<ext>.meta` (so `player.py` → `player.py.meta`, `hero.png` → `hero.png.meta`).
- **`AssetManager` gains a path index**: `std::unordered_map<std::string, UUID> m_PathIndex` mapping project-relative paths to UUIDs. Primary key remains UUID; path index is a secondary lookup for "find me the asset at this path".
- **`AssetManager` gains a missing-asset registry**: `std::unordered_set<UUID> m_MissingAssets` — UUIDs referenced by scenes but with no on-disk asset. Populated lazily on scene deserialise.
- **`Behaviour` component gains a `cachedName` field per attached script**: `std::vector<std::pair<UUID, std::string>>` or parallel arrays. Written when the user attaches a script via the editor; updated on subsequent scene saves with the current live name. Used only when the UUID has no live script.
- **`Sprite` component gains a `textureCachedName` string**: same purpose.
- **File watcher state** lives in a new `Hamster::ProjectWatcher` class owned by `Project`. Holds the `HANDLE` from `CreateFileW(projectDir, FILE_LIST_DIRECTORY, ...)`, a worker thread running `ReadDirectoryChangesW`, and a debounce queue of pending events keyed by path.

### Module touchpoints

- `Hamster-Core/src/Utils/AssetManager.{h,cpp}` — add sidecar I/O, path index, rename, missing-asset registry. Drop the `AssetManager::Serialise` binary blob (sidecars replace it). New events posted: `AssetAddedEvent`, `AssetRemovedEvent`, `AssetRenamedEvent`.
- `Hamster-Core/src/Core/Project.{h,cpp}` — own a `ProjectWatcher`. Wire `ProjectOpened` → start watcher. `ProjectClosed` → stop.
- `Hamster-Core/src/Core/SceneSerialiser.cpp` — write/read `cachedName` for Behaviour and Sprite. Bump scene version. Existing scene files lacking the field deserialise with empty cachedName (acceptable since we're not migrating old data anyway).
- `Hamster-Core/src/Core/Components.h` — extend `Behaviour` and `Sprite` with `cachedName` field(s).
- `Hamster-Core/src/Scripting/Scripting.{h,cpp}` — `AddPathToPy` already adds the project dir to `sys.path`; for dotted imports, subdirectories don't need to be on `sys.path` individually (Python resolves `enemies.boss` if `enemies/__init__.py` exists or if it's a namespace package). Decide on namespace packages (no `__init__.py` required) for simplicity — `from enemies import boss` works without one in Python 3.3+.
- `Hamster-Core/src/Scripting/HamsterScript.{h,cpp}` — import by full dotted name derived from project-relative path, not just filename stem.
- New: `Hamster-Core/src/Utils/ProjectWatcher.{h,cpp}` — Win32 directory watcher. Single worker thread, debounced events drained on the main thread via the existing `Application::EnqueueOnMainThread` mechanism.
- `Hamster-Wheel/src/Panels/AssetBrowser.{h,cpp}` — current-folder breadcrumb, folder cards, rename collision check, double-click into folder.
- `Hamster-Wheel/src/Panels/PropertyEditor.{h,cpp}` — red "MISSING" placeholder for Behaviour entries and Sprite texture when the UUID resolves to nothing; "Reassign…" dropdown that lists available assets.
- `Hamster-Wheel/src/EditorLayer.{h,cpp}` — simulation-start guard: before `RunSceneSimulation`, walk Behaviour components, refuse + log if any reference a missing script.
- `Hamster-Wheel/src/Panels/ProjectCreator.cpp` — when creating a fresh project, write the `Hamster.pyd` and no other assets. No need to generate `.meta` for the .pyd (engine artifact, excluded).

### Lifecycle / control flow

**On `ProjectOpened`** (single event handler in `AssetManager::OnProjectOpened`):

1. Recursively walk `projectDir`.
2. For each file with an asset extension (`.py`, `.png`, `.jpg`, `.hanim`), check for a sibling `.meta`.
   - Sibling present: parse JSON, take its `uuid`, register the asset.
   - Sibling absent: generate fresh `UUID`, write `<file>.meta`, register.
3. For each `.meta` whose paired asset is missing: delete the orphan `.meta`.
4. `ProjectWatcher::Start(projectDir)` begins the directory-change worker.

**On a file-watcher event** (debounced ~150ms by path on the worker thread; deduped events posted to main thread):

| Event | Action |
|---|---|
| File created (`.py`/`.png`/`.jpg`/`.hanim`) | If no sibling `.meta`: generate UUID, write `.meta`, `AssetAddedEvent`. If `.meta` already present (unusual but possible — e.g., a git pull): adopt its UUID. |
| File deleted | Mark asset as missing, post `AssetRemovedEvent`. Delete the orphan `.meta` from disk. (Scenes' Behaviour/Sprite cachedName fields keep the display info.) |
| File renamed (single OS-level rename event) | Move `<old>.meta` → `<new>.meta` on disk, update path index, `AssetRenamedEvent`. UUID preserved. |
| File modified | No action for now (no hot-reload). |
| `.meta` file events | Ignored — `.meta`s are editor-internal. |

**On `RunSceneSimulation` start**:

1. Iterate every Behaviour on every entity.
2. If any references a UUID not in `AssetManager.m_Scripts`, refuse to start; log per missing reference: `Entity '<name>' references missing script '<cachedName>' — did you rename or delete it?`
3. Otherwise proceed as today.

**On asset rename via editor** (right-click → Rename in the asset browser):

1. Validate: target name doesn't collide with an existing asset of the same type in the destination folder.
2. Move both `<old_path>` and `<old_path>.meta` to `<new_path>` + `<new_path>.meta` via a single `std::filesystem::rename` per file.
3. Update path index. Refresh asset browser. Post `AssetRenamedEvent`.

### Edge cases

- **Two scripts with the same name in different folders**: allowed. Python sees them as `<folder1>.boss` vs `<folder2>.boss` — no collision.
- **Renaming a script while simulation is running**: allowed. Already-instantiated `pybind11::object` instances hold their own class refs; the rename is purely a file-system + identity-tracking concern.
- **Renaming a folder containing scripts**: out of scope for editor UI in v1 (no folder-rename action). If user does it externally, the file watcher sees N rename events. Each individual `.py` and its `.meta` arrive paired, so UUIDs survive. Behaviour cachedName goes stale but UUID resolves, so display switches to live name on next scene save.
- **Deleting an asset while it's attached to entities**: `AssetRemovedEvent` posted. Scenes' Behaviour entries become "missing". Property Editor shows the red MISSING placeholder using cachedName.
- **A sloppy external rename (only the `.py`, not the `.meta`)**: the file watcher receives `deleted player_movement.py` + `created player_controller.py`. We treat them independently: orphan `.meta` deleted, new file gets fresh UUID. Entity attachments now reference a dead UUID. UI shows MISSING with the old cachedName. User clicks reassign in Property Editor.
- **Editor closed when a sloppy external rename happens**: same as above on next open — orphan `.meta` cleaned, new file gets fresh UUID, references dangle. Recovered via reassign.
- **Two contributors add scripts named `enemy.py` on different branches, both folders are the project root**: merge conflict in git when both `enemy.py` and `enemy.py.meta` files appear. Standard git resolution applies; UUIDs differ but the conflict is visible. We don't try to be cleverer than git.
- **A `.meta` exists in git but no `.py` exists yet**: on project open, orphan-cleanup deletes the `.meta`. Acceptable — git users should always commit both files together. (Future enhancement: detect this and warn rather than auto-delete.)
- **User renames a script to a name that collides in the same folder**: editor refuses with a console message. Project disk state unchanged.
- **The asset browser is open in a folder, and the watcher posts an `AssetAddedEvent` for a file in that folder**: browser refreshes. No flicker — the breadcrumb + card grid re-renders on next ImGui frame.

---

## Full spec

### Sequence diagrams / data flow

**Project open → asset reconciliation:**

```
main.cpp / ProjectHubLayer
  └─ Project::Open(path)
      ├─ ProjectOpenedEvent posted
      ├─ AssetManager::OnProjectOpened
      │   ├─ walk projectDir
      │   ├─ for each (.py / .png / .jpg / .hanim) file:
      │   │   ├─ sibling .meta exists?
      │   │   │   ├─ yes → parse json, register UUID
      │   │   │   └─ no  → mint UUID, write .meta, register
      │   ├─ for each orphan .meta:
      │   │   └─ std::filesystem::remove(.meta)
      │   └─ asset count + missing-asset registry initialised
      └─ ProjectWatcher::Start(projectDir)
          └─ worker thread: ReadDirectoryChangesW loop
```

**External rename (clean OS rename event):**

```
VS Code rename: player_movement.py → player_controller.py
  └─ Windows posts FILE_ACTION_RENAMED_OLD_NAME + FILE_ACTION_RENAMED_NEW_NAME
      └─ ProjectWatcher worker thread
          └─ debounce 150ms, then enqueue on main thread:
              └─ AssetManager::HandleRename(old, new)
                  ├─ rename: player_movement.py.meta → player_controller.py.meta
                  ├─ update m_PathIndex
                  └─ AssetRenamedEvent posted
                      └─ AssetBrowser refresh, PropertyEditor refresh
```

**Sloppy external rename (delete + create, .meta not moved):**

```
Some tool: deletes player_movement.py, creates player_controller.py with same bytes
  └─ Watcher: FILE_ACTION_REMOVED + FILE_ACTION_ADDED, .meta untouched
      └─ debounce + main thread
          ├─ AssetManager::HandleRemoved(player_movement.py)
          │   ├─ remove orphan player_movement.py.meta from disk
          │   ├─ mark UUID as missing
          │   └─ AssetRemovedEvent
          └─ AssetManager::HandleAdded(player_controller.py)
              ├─ no sibling .meta → mint fresh UUID
              ├─ write player_controller.py.meta
              └─ AssetAddedEvent
                  → entities referencing old UUID now MISSING in property editor
```

**Simulation start with a missing-script entity:**

```
EditorLayer: user clicks Play
  └─ check Behaviour components against AssetManager
      └─ found 1 missing-script reference
          └─ refuse, log to client logger:
              "Entity 'Player' references missing script 'player_movement.py' —
               did you rename or delete it?"
          └─ Play state stays Stopped, no Scene::RunSceneSimulation call
```

### Error handling

| Failure | Surfacing |
|---|---|
| `.meta` parse error (corrupt JSON) | Log `"<path>.meta: corrupt; regenerating UUID"`, generate fresh UUID, overwrite. Attachments break — surfaced as MISSING in property editor (cachedName from scene file). |
| `.meta` write fails (permission / disk full) | Log error, leave asset unregistered. Next project open will retry. |
| Rename target collides | Log `"Cannot rename: '<new>' already exists in <folder>"`. Disk state unchanged. |
| `ReadDirectoryChangesW` fails to start | Log error, project still opens but external file events are not detected. Editor rename still works. Acceptable degraded mode. |
| Worker thread crashes inside watcher | Log via `Application::EnqueueOnMainThread` so the main-thread logger sees it. Watcher stops; same degraded mode as above. |
| Scene load with missing script UUID | Normal flow — UUID kept in Behaviour, cachedName used for display, registered in `m_MissingAssets`. |
| Simulation start with missing reference | Refuse + log per missing reference (one console line each). |
| Two entities both reference the same missing UUID | One log line per entity, mentioning each entity's name. |
| Sidecar file appears on disk but its declared UUID is already registered (e.g., user copy-pasted a file with its .meta) | Log a warning, mint a fresh UUID for the duplicate, rewrite its `.meta`. UUIDs must remain unique. |

### Performance considerations

- **Project-open scan**: O(N) `std::filesystem::directory_iterator` walk + sidecar read. For N=200 assets, well under a second on any SSD. Not a hot path.
- **File watcher**: kernel-driven, near-zero idle cost. Debounce buffer is at most a few dozen entries during a save-burst.
- **Per-frame cost**: zero. No watcher work runs in the main loop; events are drained from the queue, but events are rare.
- **Simulation-start guard**: O(M) where M = number of entities with Behaviour. Trivial.
- **PathIndex / MissingAssets lookups**: `unordered_map`, O(1) amortised.

### Migration / compatibility

- **No migration of pre-feature projects.** Documented in the close-out README entry: "Asset sidecars: existing test projects with `Untitled_Script_<UUID>.py` filenames are not migrated. Create new projects after this change." The user has confirmed they're throwaway.
- **Scene file format bump required.** `cachedName` fields added to Behaviour and Sprite serialisation. Scene version increments. Old scene files would read with default-empty cachedName, which would mean MISSING displays as an empty red placeholder — acceptable because old projects are discarded.
- **Smoke test fixtures need refresh.** Any current fixture project on disk that the smoke test loads will break unless rebuilt against the new format. The smoke test currently constructs scenes in code, so this is contained.

---

## Why this approach

Three approaches were on the table:

- **(A) Single project-wide manifest** (initial preference): one `assets.json` or binary file in the project root mapping every UUID ↔ path. Pros: clean file tree. Cons: every add/delete/rename rewrites the same file; two contributors adding scripts on the same branch will conflict; the manifest is harder to read in a `git diff`.
- **(B) Per-asset sidecars (Unity-style)** (picked): one `.meta` next to each asset. Pros: diffs are local, merge conflicts are rare, survives partial moves and reorganisations, mirrors what every shipped asset-pipeline engine does (Unity, Godot). Cons: clutters the file tree with `.meta` files. The clutter is offset by the practical wins.
- **(C) Embed UUID in the asset itself** (Unreal-style): a `# UUID: ...` comment at the top of every `.py`. Pros: no sidecar file. Cons: gross for plain-text scripts; impossible for `.png` (not without a metadata format); confusing if the user deletes the comment by accident.

For rename recovery, we picked Unity's stance: trust the OS rename event, accept that "sloppy" external renames break attachments, and recover via a user-visible MISSING placeholder with a one-click reassign. The content-hash recovery (option D) was rejected because it adds complexity without solving the actual case the user hits (sloppy renames are rare; the recovery UI is fast enough that automating the rare case isn't worth the implementation cost).

For format, we picked JSON over binary because each `.meta` is one tiny file with one field; readability and git-diff cleanliness matter more than parser-dependency hygiene. A 10-line hand-rolled JSON writer is enough — we don't pull in a JSON library.

For subdirectories, we picked dotted module imports because that's how Python natively resolves nested packages. The alternative (flat-only) would have hit a wall the first time the user has ≥20 scripts.

Accepted tradeoffs:
- `.meta` clutter in the file tree (mitigated: hidden in the asset browser).
- Sloppy external renames break attachments (mitigated: red MISSING placeholder with reassign).
- No cross-platform watcher in v1 (consistent with editor's existing Win32-only chrome).
- No migration from old projects (user confirmed they're throwaway).

## Risks / what could go wrong

- **`ReadDirectoryChangesW` event ordering on Windows is loose.** Sometimes a clean rename arrives as a single `RENAMED_OLD_NAME` + `RENAMED_NEW_NAME` pair; sometimes as `REMOVED` + `ADDED`. We can't always distinguish "clean rename" from "delete + create". The debounce + pairing logic must handle both; we accept that some clean renames will look sloppy and the user will use the reassign UI to fix the rare miss.
- **Debounce window is a guess.** 150ms may be too short for slow disks or anti-virus interference. Symptom: write-tmp + rename sequences from editors look like two separate events. Mitigation: tunable constant; we may need to raise it during early use.
- **Python namespace package gotcha.** Subdirectories without `__init__.py` are namespace packages in Python 3.3+. They work but have edge cases (e.g., `sys.modules` caching, reload semantics differ from regular packages). At v1 we don't reload modules during simulation, so this is theoretical, but it'll bite if/when hot-reload lands.
- **Scene format change is sticky.** Once shipped, scenes have cachedName fields. Reverting the feature means another format bump.
- **Asset browser folder navigation is new UX.** A single-level breadcrumb is the smallest viable answer, but if the user goes 3+ folders deep it'll feel awkward. Future-work item: nested tree view.
- **A `.meta` whose JSON parses but contains a malformed UUID** (e.g., truncated string from an interrupted write). Current plan: regenerate. Edge case: the regenerated UUID will not match the scene's old reference, so attachments break silently. Mitigation: log loudly when we regenerate any UUID.
- **AssetManager's existing binary `Serialise` blob** writes all UUIDs and names into the `.hamproj` file. After this feature, that responsibility moves to sidecars. We have to be careful not to leave dead code paths in `ProjectSerialiser` that re-read the old blob and conflict with the new sidecar-driven init.
- **`Hamster.pyd` lives in the project root** and the watcher will fire events for it on first project open. We must filter the watcher's "interesting extensions" to exclude `.pyd`, `.pyc`, `.hamproj`, `.hamscene`, and the `.meta` files themselves.

## Success criteria

Verified before marking the feature shipped:

1. **Fresh project flow**: create a new project via the project hub. Drop `player.py` into the project folder via Windows Explorer. Within ~1 second the asset browser shows `player.py`. A `player.py.meta` file exists next to it on disk, containing valid JSON with a UUID.
2. **Attach + rename roundtrip**: attach `player.py` to an entity via the property editor. Rename it via the asset browser right-click → Rename to `player_controller.py`. The property editor still shows `player_controller.py` attached to the entity — UUID survived. The scene file persists the new name on save.
3. **External rename (clean)**: with the editor open, rename `enemy.py` to `enemy_ai.py` via a single `mv`/PowerShell `Rename-Item`. Within ~1 second the asset browser shows `enemy_ai.py`. On-disk: `enemy_ai.py.meta` exists; old `enemy.py.meta` is gone. The UUID in the new `.meta` equals the old UUID.
4. **Sloppy external rename**: delete `boss.py` via Explorer; create `final_boss.py` with the same content via Explorer. Attached entity shows MISSING (red, name="boss.py") in the property editor. Property editor's reassign dropdown lists `final_boss.py`; clicking it restores the attachment.
5. **Simulation refuses with missing reference**: with an entity in a MISSING state, clicking Play does not run the simulation. The console shows `Entity '<name>' references missing script 'boss.py' — did you rename or delete it?`.
6. **Same-folder name collision refused**: with both `enemy.py` and `boss.py` in the project root, attempt to rename `enemy.py` to `boss.py` via the asset browser. Rename is refused; both files remain unchanged on disk; console logs the refusal.
7. **Cross-folder same-name allowed**: place `enemies/boss.py` and `bosses/boss.py`. Both load. Both can be attached to different entities. Python module names resolve as `enemies.boss` and `bosses.boss` respectively without import errors.
8. **Subdirectory navigation**: asset browser shows folder cards. Double-clicking enters the folder; breadcrumb updates. Files inside the folder render and can be attached. Going back via breadcrumb works.
9. **Smoke test passes** with the new sidecar-aware AssetManager and updated scene format.

## Test extensions required

Smoke test gains three scenarios:

1. **Sidecar reconciliation**: create a temp project dir with a hand-written `player.py` (no sibling `.meta`) and a hand-written `enemy.py` + `enemy.py.meta` with a fixed UUID. Open the project via `Project::Open`. Assert:
   - `AssetManager` reports both scripts registered.
   - `player.py.meta` now exists on disk with a freshly generated UUID.
   - `enemy.py`'s registered UUID equals the hand-written one from its `.meta`.
2. **Rename via API**: call `AssetManager::RenameAsset(uuid, "renamed.py")`. Assert:
   - Both `renamed.py` and `renamed.py.meta` exist on disk.
   - Old name's files are gone.
   - Path index reflects the new path.
3. **Missing-script detection on scene load**: serialise a scene with one Behaviour referencing a UUID that won't exist. Deserialise into a fresh AssetManager. Assert:
   - `AssetManager::IsMissing(uuid)` returns true.
   - `m_MissingAssets` contains the UUID.
   - Calling the simulation-start guard returns false / refuses.

File-watcher behaviour is **not** smoke-tested in v1: it's Win32-API-bound and asynchronous, making it brittle to test in CI. We verify it manually per the success criteria.

---

## Decisions during implementation

### 2026-05-17 — Animations don't get sidecars; .hanim is self-identifying

`.hanim` files already contain the animation's UUID + name + keyframes (the format was designed that way before this feature). A sidecar for `.hanim` would duplicate the UUID and add nothing — renaming a `.hanim` doesn't change the internal UUID, so identity already survives renames. v1 therefore treats animations as self-identifying: on project open, `LoadProjectAnimations(projectDir)` walks the project's `Animations/` directory and `LoadAnimationFile`s each `.hanim`. The animations section is dropped from the project blob entirely. This is the same pattern Unity uses for `.prefab` (self-identifying) vs `.png` (sidecar).

### 2026-05-17 — Texture names stay in the project blob, not in the sidecar

The texture sidecar holds UUID only — texture names continue to live in the project blob alongside the path. Putting names in the sidecar would mean (a) extending the JSON schema and parser, and (b) writing names next to every imported texture file even when the file lives outside the project (e.g., a user-imported sprite from Downloads). Keeping names in the blob preserves per-project naming for default sprites ("Square" / "Triangle" / "Circle" — distinct from their `square.png` / `triangle.png` / `circle.png` filenames) without cluttering external folders. Sidecar carries only the persistent identity (UUID); the project carries everything else.


## Spec amendments

### 2026-05-17 — Sidecars live next to the file, wherever it is (not in the project dir)

**What was wrong**: the spec implicitly assumed all assets live in the project directory, so `<file>.meta` would always be a child of the project root. They don't. `AssetBrowser → Add Asset → Import Texture` records the user-picked absolute path (e.g. `C:\Users\Jaden\Downloads\sprite.png`) and the texture file is never copied into the project. `.hanim` files have similar mixed locations. Only scripts (created via `AddDefaultScript`) are reliably in the project dir.

**New plan**: `.meta` lives next to its asset, wherever that asset is on disk. A texture at `C:\Users\Jaden\Downloads\sprite.png` gets `C:\Users\Jaden\Downloads\sprite.png.meta`. A script in the project dir gets a `.meta` in the project dir. The UUID record is tied to the file's *location at the time it was added*, and travels with the file if the user copies it (since `.meta` is right next to it).

**Impact on the rest of the spec**:
- "Sidecars for everything" stands — universal, but the location follows the asset, not the project.
- `AssetManager::Serialise` / `Deserialise` for textures and animations can be slimmed: only the absolute path needs to persist in the `.hamproj`; UUID + name come from the sidecar on load.
- File watcher: **only the project directory** is watched live. Live add/delete/rename detection only applies to assets inside the project (which is where scripts live). External assets (textures in Downloads, etc.) are reconciled on project open: their sidecar is read or written then. External-rename-while-editor-open is best-effort — the user can manually re-import. This is a deliberate tradeoff against watching N arbitrary directories.
- Sloppy external rename of a texture: editor doesn't notice until next project open; on next open the texture appears missing (the absolute path doesn't resolve), property editor shows the cached name as MISSING with a reassign dropdown.
- Permissions: writing a `.meta` next to a user-picked asset requires write access to that directory. Almost always fine for `Downloads` / `Desktop` / project folders; problematic only for read-only system dirs. If a `.meta` write fails for an external asset, log + skip (the asset still loads, just with a freshly generated in-memory UUID; attachments will break on next project open). This is logged as an accepted risk.

**Approval**: pending — author's go required before plan + implementation proceeds.


---

## Future work (out-of-scope ideas surfaced during this feature)

- Hot-reload of scripts when their content changes mid-edit (use the existing file-modified watcher event).
- Nested folder-tree navigation in the asset browser (today: breadcrumb + folder cards, one level at a time).
- Drag-and-drop reorganisation (drop a script onto a folder card to move it).
- Per-asset import settings inside `.meta` (texture filter mode, animation framerate, script default-attach flag).
- Cross-platform file watcher (`inotify` on Linux, `FSEvents` on macOS).
- Content-hash–based rename recovery for sloppy external renames.
- Migration tool for projects with the old `Untitled_Script_<UUID>.py` naming.
- Sidecars for scenes and project files (would let multiple scenes per project share a stable per-scene UUID).
- Asset browser search with folder-scope filter.
