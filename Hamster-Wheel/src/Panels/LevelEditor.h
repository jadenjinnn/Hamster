#ifndef UIPROTO_LEVEL_EDITOR_H
#define UIPROTO_LEVEL_EDITOR_H

#include <imgui.h>

namespace Hamster {
class Renderer;
class Scene;
}

class LevelEditor {
public:
    LevelEditor() = default;

    // Caller has already SetNextWindow + Begin'd the panel window.
    // Returns FBO blit top-left (screen) and viewport content size; EditorLayer
    // reuses these for FBO sizing + picking the next frame.
    void Render(unsigned int fbTexId,
                Hamster::Renderer *renderer,
                Hamster::Scene *scene,
                ImVec2 &outViewportTL,
                ImVec2 &outAvailSize);

private:
    bool m_ZoomDragging = false;
    bool m_AxisDraggingX = false;
    bool m_AxisDraggingY = false;
};

#endif // UIPROTO_LEVEL_EDITOR_H
