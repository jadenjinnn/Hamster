#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <Core/UUID.h>
#include <Renderer/Texture.h>
#include <Utils/AssetManager.h>
#include <Utils/SheetSidecar.h>

// Floating ImGui window for slicing a texture into sub-sprites. Single
// shared instance owned by EditorLayer; Open() seeds working state from
// the parent texture's currently registered sub-sprites; Render() runs
// each frame while m_Open.
//
// v2 (this file) supports multi-rect editing: click-select, drag body
// to move, 8 handles to resize, Del to delete, list view with editable
// names. Save diffs the working list against AssetManager (add / remove /
// modify-in-place) with collision-checked renaming against the unified
// Texture + SubSprite namespace.
class SpritesheetEditor {
public:
    void Open(Hamster::UUID textureUUID, Hamster::AssetManager *am);
    void Close();
    void Render();

    bool IsOpen() const { return m_Open; }

private:
    // Diffs m_Working against AssetManager, writes the .sheet sidecar, and
    // persists the project. Returns true on success; on a name collision it
    // sets m_LastError and returns false without writing. Does not close.
    bool CommitSave();

    enum class DragMode {
        None,
        DrawNew,
        MoveBody,
        ResizeHandle,  // m_DragHandle = 0..7 (TL, T, TR, R, BR, B, BL, L)
    };

    bool m_Open = false;
    Hamster::UUID m_TextureUUID = Hamster::UUID::GetNil();
    Hamster::AssetManager *m_AssetManager = nullptr;

    // Working copy of the slice list — mutated freely until Save commits.
    std::vector<Hamster::SubSpriteEntry> m_Working;

    int m_SelectedIndex = -1;  // index into m_Working, -1 = none
    // Per-row editable name buffers (one per working entry).
    std::vector<std::array<char, 64>> m_NameBuffers;

    // Drag state.
    DragMode m_DragMode = DragMode::None;
    int m_DragHandle = -1;
    glm::ivec2 m_DragStartPixel = {0, 0};
    glm::ivec4 m_DragStartRect = {0, 0, 0, 0};

    // Inline error from last Save attempt (collision detection).
    std::string m_LastError;

    // Unsaved-changes guard. m_Dirty is set on any region edit and cleared on
    // Open/Save; m_WantClosePrompt requests the confirm-on-close modal.
    bool m_Dirty = false;
    bool m_WantClosePrompt = false;

    // Display.
    float m_Zoom = 1.0f;
};
