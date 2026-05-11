# Feature spec: Project Registry (Hub CRUD)

> Tier: **Sketch**
> Status: **shipped**
> Started: 2026-05-11
> Spec author: Jaden

---

## Classification

- **Reversibility**: Reversible
- **Scope**: Isolated
- **Tier rationale**: All changes are contained in the hub layer plus a small JSON registry utility — no public API, file format, or cross-cutting changes.

---

## Problem

The project hub currently renders hardcoded placeholder cards. Users can't create a project and see it appear in the hub, reopen it later, rename it, or delete it. Without a persistent project registry, the hub is a static mockup — every session starts blank, and the only way to open a project is through a file dialog.

## In scope

- **Persistent JSON registry** at `%APPDATA%/Hamster/projects.json` storing project entries (name, path, last-opened timestamp)
- **Create**: modal already wired to `Project::New()` — additionally writes a new entry to the registry and shows the card immediately
- **Open from card**: clicking a project card opens that project (same flow as the existing file-dialog open)
- **Open from file dialog**: existing "Open Project" button; also adds/updates the registry entry
- **Rename**: via ellipsis menu on each card — renames the display name in the registry AND the project directory on disk
- **Delete**: via ellipsis menu — confirmation dialog ("This permanently deletes the project folder"), then removes from registry AND deletes from disk
- **Missing project handling**: if a project's path no longer exists when the hub loads (or when the user clicks the card), show a warning offering to relocate the project or remove it from the registry
- **Cards display real data**: name and last-opened timestamp from the registry replace the placeholder data

## Out of scope

- Tags, favorites, star ratings, thumbnails (stay as visual placeholders or removed)
- Search functionality (search bar stays visual-only)
- Grid/list view toggle (stays visual-only)
- Template-based project creation (templates in the modal stay visual-only; all projects use the empty template)
- Project sorting or filtering
- Cloud/remote project storage
- Project import/export

## API sketch

```json
// %APPDATA%/Hamster/projects.json
{
  "projects": [
    {
      "name": "My Game",
      "path": "C:/Users/Jaden/Projects/MyGame",
      "lastOpened": "2026-05-11T14:30:00Z"
    }
  ]
}
```

```cpp
// New utility class — ProjectRegistry
class ProjectRegistry {
public:
    struct Entry {
        std::string name;
        std::filesystem::path path;
        std::string lastOpened; // ISO 8601
    };

    static ProjectRegistry Load();  // reads from %APPDATA%/Hamster/projects.json
    void Save() const;              // writes back

    void AddOrUpdate(const std::string& name, const std::filesystem::path& path);
    void Remove(const std::filesystem::path& path);
    void Rename(const std::filesystem::path& oldPath, const std::string& newName);
    const std::vector<Entry>& GetEntries() const;
};
```

---

## Why this approach

A single JSON file in `%APPDATA%` is the simplest persistence mechanism that survives reinstalls and works per-user. Alternatives considered:

- **Scan a directory for `.hamproj` files** — forces all projects into one folder, or requires recursive scan which is slow. Rejected because users choose project locations freely.
- **SQLite database** — overkill for a flat list of <100 entries. Adds a dependency. Rejected.
- **Registry file next to the exe** — non-standard on Windows, may be read-only in Program Files, doesn't work on multi-user machines. Rejected.

The tradeoff accepted: if the user deletes the `%APPDATA%/Hamster/` folder, the project list is lost (but projects themselves remain on disk). This matches how VS Code, Unity, and similar tools behave.

## Risks / what could go wrong

1. **Disk deletion is irreversible**: delete removes the entire project folder. If the wrong folder is targeted (e.g., a symlink or junction pointing somewhere unexpected), data loss could be severe. Mitigation: confirmation dialog with the full path displayed.
2. **Rename fails mid-operation**: renaming involves updating both the registry and the directory on disk. If the directory rename fails (permissions, file lock) but the registry was already updated, the entry points to a nonexistent path. Mitigation: rename the directory first, only update the registry on success.
3. **Concurrent access**: if two instances of the editor run simultaneously, registry writes could clobber each other. Mitigation: none for v1 — this is a single-user desktop app. Document as a known limitation.
4. **JSON corruption**: a crash mid-write could leave a truncated file. Mitigation: write to a temp file, then atomic rename (as atomic as Windows allows).

## Success criteria

1. Create a project via the modal → card appears in the hub immediately with the correct name and timestamp
2. Close and reopen the editor → the created project still appears in the hub
3. Click a project card → the project opens in the editor
4. Rename a project via ellipsis menu → card updates, directory on disk is renamed
5. Delete a project via ellipsis menu → confirmation dialog appears; on confirm, card disappears and directory is deleted from disk
6. Manually delete a project's folder on disk → next hub load shows a warning on that card; user can relocate or remove it
7. Registry file exists at `%APPDATA%/Hamster/projects.json` with valid JSON after any operation

## Test extensions required

None — pure additive UI feature with no engine-core changes. The smoke test exercises the C++→Python boundary and scene lifecycle, which this feature doesn't touch. Manual verification against the success criteria above is sufficient.

---

## Decisions during implementation

<!-- Append-only log. -->

## Spec amendments

<!-- Append-only log. -->

---

## Future work (out-of-scope ideas surfaced during this feature)

- Project tags and filtering
- Favorites / pinning
- Project thumbnails (last scene screenshot)
- Search functionality (filter cards by name)
- Template-based project scaffolding (platformer, top-down, etc.)
- Grid/list view toggle
- Project sorting (by name, date, size)
