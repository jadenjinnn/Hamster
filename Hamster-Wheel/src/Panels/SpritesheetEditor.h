#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include <Core/UUID.h>
#include <Renderer/Texture.h>
#include <Utils/AssetManager.h>
#include <Utils/SheetSidecar.h>

// Floating ImGui window for slicing a texture into sub-sprites.
// Lifecycle mirrors ColliderEditor — single shared instance owned by
// EditorLayer; Open() loads working state, Render() draws each frame
// when m_Open is true.
//
// Stage 3 (this file): zoom-pan image preview, draw ONE pending rect by
// drag, Save commits it via AssetManager::AddSubSprite + writes the
// sidecar. Stage 4 extends to multi-rect, resize handles, list view,
// rename, delete, and diff-aware Save.
class SpritesheetEditor {
public:
    void Open(Hamster::UUID textureUUID, Hamster::AssetManager *am);
    void Close();
    void Render();

    bool IsOpen() const { return m_Open; }

private:
    bool m_Open = false;
    Hamster::UUID m_TextureUUID = Hamster::UUID::GetNil();
    Hamster::AssetManager *m_AssetManager = nullptr;

    // Working list of regions — loaded from AssetManager + sidecar on Open,
    // mutated during the session, committed on Save. Stage 3 only ever
    // produces at most one new entry per session (the pending draw); stage 4
    // grows this to full multi-rect editing.
    std::vector<Hamster::SubSpriteEntry> m_Working;

    // True while the user is dragging a new rectangle.
    bool m_Drawing = false;
    glm::ivec2 m_DrawStartPixel = {0, 0};
    glm::ivec2 m_DrawCurrentPixel = {0, 0};

    // Zoom factor (1 source pixel = m_Zoom screen pixels). Pan offset in
    // screen pixels relative to the image's top-left.
    float m_Zoom = 4.0f;
};
