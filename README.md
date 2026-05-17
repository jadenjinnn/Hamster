# Hamster

Hamster is a game engine that is intended to be used in the classroom to teach beginners Python programming. It is intuitive and simple to use. The Python api can be used by itself as a library or used together with the graphical editor (work in progress). Hamster differs from existing Python libraries which enable game creation such as PyGame in its "pythonic" design rather than the object oriented design of many libraries. It is also much easier to use and get started.

## What's new

- **Editor theme**: dark Figma-inspired theme with Inter font, Font Awesome icons, viewport play/pause/stop overlay, and consistent panel styling
- **Sprite preview**: texture thumbnail with tint, dimensions, and inline color picker in the PropertyEditor
- **Scene viewer**: dot grid, right-click context menus, zoom slider, axis gizmo, 8-handle selection box with resize
- **UI polish**: bold typographic hierarchy, card-grid asset/file browsers, refined spacing across all panels
- **Project hub redesign**: fullscreen card-grid hub with top bar, search, template-based create modal, and open-project flow
- **Project registry**: persistent project list with create, open, rename, and delete from the hub
- **Box2D physics**: gravity, velocity, forces/impulses, body types (static/dynamic/kinematic), box and circle colliders with density, friction, restitution, and gravity scale
- **Collider editor**: visual editor for adjusting collider offset and size independently from sprite bounds, with drag handles for box and circle shapes
- **Runtime entity management**: create and destroy entities from Python scripts at runtime with `create_entity`, `destroy_entity`, and `add_component` for Sprite and Rigidbody
- **Animation system**: time-based sprite-swap animations with `.hanim` file format, Animation component, timeline editor panel with draggable keyframes, preview playback, Python API (`self.animate()`, `self.stop_animation()`, `self.is_animating`, `on_animation_complete`), and `AnimationCompleted` event
- **Editor rewrite**: editor frontend rebuilt from the UI prototype with a fixed proportional card-style layout, custom borderless title bar with integrated menus and drag/maximise/close, custom panel headers with the underlined-accent tab style, and reusable component helpers (`HButton`, `HCombo`, `AxisDotInput`, `SectionHeader`) consumed by every panel
- **Entity hierarchy**: parent/child entity relationships (organisational — no transform inheritance), recursive Hierarchy panel with collapse / drag-to-reparent / drag-to-reorder, cascading destroy, cycle-creating reparents refused, Python API (`self.parent`, `self.children`, `self.set_parent`, `create_entity(parent=)`), scene file persistence
- **Asset sidecars**: scripts and textures get human-readable filenames (no more `Untitled_Script_<uuid>.py`); identity persists in `.meta` sidecars next to each file so renames survive both the editor and external tools (VS Code, Explorer); subdirectories supported with Python dotted-module imports (`enemies/boss.py` → `import enemies.boss`); a Win32 file watcher picks up external add/remove/rename live; missing references render as red `MISSING: <name>` in the Property Editor with a one-click "Reassign to" recovery; simulation refuses to start while any reference is unresolved

## Getting Started
To get started, follow the documentation at https://doritothepug.github.io/Hamster
