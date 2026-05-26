#pragma once

#include <imgui.h>
#include <glm/glm.hpp>
#include <memory>

#include <Hamster.h>
#include <Renderer/FramebufferTexture.h>

#include "Core/Scene.h"
#include <Renderer/Texture.h>
#include "Panels/LevelEditor.h"
#include "Panels/Hierarchy.h"
#include "Panels/PropertyEditor.h"
#include "Panels/BottomPanel.h"
#include "Panels/CreateProjectModal.h"
#include "Panels/OpenProjectModal.h"
#include "Panels/ColliderEditor.h"
#include "Panels/SpritesheetEditor.h"
#include "ProjectRegistry.h"

class EditorLayer : public Hamster::Layer {
public:
    EditorLayer(Hamster::Application *app);
    ~EditorLayer() override;

    void OnAttach() override;
    void OnUpdate() override;
    void OnImGuiUpdate() override;

    void ActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e);
    void FramebufferSizeChanged(Hamster::FramebufferResizeEvent &e);

private:
    // Panel-relative mouse → world. Different from Renderer::ScreenToWorldPos
    // because the renderer's projection covers the full window framebuffer
    // (m_ViewportHeight) but the level-editor FBO + scissor only render the
    // panel-sized area. The visible world rectangle is the BOTTOM portion of
    // the projection (gl-y ∈ [0, panel_h]) which corresponds to world Y in
    // [cam.y + (vp_h - panel_h)/zoom, cam.y + vp_h/zoom]. So mouse panel_y=0
    // is world cam.y + (vp_h - panel_h)/zoom, NOT cam.y. ScreenToWorldPos
    // misses this offset and picks land ~(vp_h - panel_h)/zoom px too high.
    glm::vec2 PanelMouseToWorld(float panelX, float panelY) const;

    Hamster::Application *m_App;
    Hamster::EventDispatcher *m_Dispatcher;
    Hamster::SubscriptionHandle m_ActiveSceneSub = 0;
    Hamster::SubscriptionHandle m_FramebufferSub = 0;
    Hamster::Renderer *m_Renderer;
    std::shared_ptr<Hamster::Scene> m_Scene;

    Hamster::FramebufferTexture m_FramebufferTexture;

    int m_ViewportWidth  = 1920;
    int m_ViewportHeight = 1080;

    ImVec2 m_LevelEditorAvailRegion = {0.0f, 0.0f};
    glm::vec2 m_ViewportOffset = {0.0f, 0.0f};

    std::unique_ptr<LevelEditor> m_LevelEditor;
    std::unique_ptr<Hierarchy> m_Hierarchy;
    std::unique_ptr<PropertyEditor> m_PropertyEditor;
    std::unique_ptr<BottomPanel> m_BottomPanel;
    std::unique_ptr<CreateProjectModal> m_CreateModal;
    std::unique_ptr<OpenProjectModal> m_OpenModal;
    std::unique_ptr<ColliderEditor> m_ColliderEditor;
    std::unique_ptr<SpritesheetEditor> m_SpritesheetEditor;

public:
    SpritesheetEditor *GetSpritesheetEditor() { return m_SpritesheetEditor.get(); }
    ProjectRegistry m_Registry;

    // Picking / drag state
    bool m_EntityHeld          = false;
    bool m_TopLeftGrabberHeld  = false;
    bool m_TopRightGrabberHeld = false;
    bool m_BotLeftGrabberHeld  = false;
    bool m_BotRightGrabberHeld = false;
    bool m_TopGrabberHeld      = false;
    bool m_RightGrabberHeld    = false;
    bool m_BotGrabberHeld      = false;
    bool m_LeftGrabberHeld     = false;
    bool m_BgHeld              = false;

    // Aspect-lock for corner resize: capture the sprite's aspect ratio when a
    // corner grab starts; while Shift is held the drag preserves it.
    bool  m_CornerGrabActive = false;
    float m_GrabAspect       = 1.0f;

    float m_MouseHeldTransformX = 0.0f;
    float m_MouseHeldTransformY = 0.0f;

    ImVec2 m_RightClickStartPos = {0.0f, 0.0f};
    bool m_RightClickDragged = false;
    glm::vec2 m_ContextMenuWorldPos = {0.0f, 0.0f};
    entt::entity m_ContextMenuEntity = entt::null;
    bool m_OpenSceneContextMenu = false;
    bool m_OpenEntityContextMenu = false;

    bool m_ViewportHovered = false;
    entt::entity m_HoveredEntity = entt::null;

    // UI drag (edit mode). On click into a UIButton, capture the initial
    // offset so subsequent mouse-drag deltas mutate it; the sign-flip is
    // re-applied per frame so +x mouse always moves the rect rightward
    // regardless of anchor side.
    bool m_UIHeld = false;
    entt::entity m_UIHeldEntity = entt::null;
    glm::vec2 m_UIHeldStartOffset = {0.0f, 0.0f};

    // Custom title bar state
    std::unique_ptr<Hamster::Texture> m_LogoTex;
    bool m_TitleDragging = false;
    double m_DragStartX = 0, m_DragStartY = 0;
    int m_WinStartX = 0, m_WinStartY = 0;
    bool m_Maximized = false;
};
