#include "EditorLayer.h"

#include <cmath>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#ifdef _WIN32
// Forward the title-bar drag to Windows so we get native behaviour:
// auto-restore-on-drag from maximised + Aero Snap to screen edges.
// GLFW_EXPOSE_NATIVE_WIN32 is already defined by the build.
#include <GLFW/glfw3native.h>
#include <Windows.h>
#endif

#include <Core/Base.h>
#include <Core/Components.h>
#include <Core/Project.h>
#include <Renderer/Renderer.h>
#include "Core/Application.h"

#include "IconsFontAwesome6.h"
#include "Theme.h"
#include "Panel.h"
#include "Components/Components.h"
#include "Panels/PropertyEditor.h"
#include "Panels/Hierarchy.h"
#include "Panels/BottomPanel.h"

#include <Events/UIEvents.h>

EditorLayer::EditorLayer(Hamster::Application *app)
    : m_App(app),
      m_Dispatcher(app->GetEventDispatcher().get()),
      m_Renderer(app->GetRenderer()),
      m_Scene(app->GetActiveScene()),
      m_FramebufferTexture(1920, 1080) {
    m_LevelEditor = std::make_unique<LevelEditor>();
    m_Hierarchy = std::make_unique<Hierarchy>(m_Dispatcher, m_Scene);
    m_ColliderEditor = std::make_unique<ColliderEditor>();
    m_SpritesheetEditor = std::make_unique<SpritesheetEditor>();
    m_PropertyEditor = std::make_unique<PropertyEditor>(
        m_Dispatcher, m_Scene, app->GetAssetManager(), m_ColliderEditor.get());
    m_BottomPanel = std::make_unique<BottomPanel>(
        m_Dispatcher, m_Scene, app->GetAssetManager(),
        m_SpritesheetEditor.get());
    m_CreateModal = std::make_unique<CreateProjectModal>(m_App);
    m_OpenModal   = std::make_unique<OpenProjectModal>(m_App);
}

EditorLayer::~EditorLayer() {
    m_Dispatcher->Unsubscribe(Hamster::ActiveSceneChanged, m_ActiveSceneSub);
    m_Dispatcher->Unsubscribe(Hamster::FramebufferResize, m_FramebufferSub);
}

void EditorLayer::OnAttach() {
    glfwGetFramebufferSize(m_App->GetWindow(), &m_ViewportWidth, &m_ViewportHeight);

    m_ActiveSceneSub = m_Dispatcher->Subscribe(Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(EditorLayer::ActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));

    m_FramebufferSub = m_Dispatcher->Subscribe(Hamster::FramebufferResize,
        FORWARD_CALLBACK_FUNCTION(EditorLayer::FramebufferSizeChanged,
                                  Hamster::FramebufferResizeEvent));

    glfwSetWindowSizeLimits(m_App->GetWindow(), 1024, 600,
                            GLFW_DONT_CARE, GLFW_DONT_CARE);

    std::string logoPath = Hamster::Application::GetExecutablePath() +
        "/../share/Resources/Hamster-Wheel/Resources/Logo/hamster-logo.png";
    m_LogoTex = std::make_unique<Hamster::Texture>(logoPath);
}

void EditorLayer::OnUpdate() {
    if (!m_Scene) return;
    if (m_LevelEditorAvailRegion.x <= 0 || m_LevelEditorAvailRegion.y <= 0) return;

    // ── Hover-pick: spatial-index point query (was a full-scene FBO render
    //    + glReadPixels every frame; that was the lag source at N≥1000) ──
    // Skipped while dragging (drag has its own held flags driving the selection).
    bool anyHeld = m_EntityHeld || m_BgHeld ||
                   m_TopLeftGrabberHeld || m_TopRightGrabberHeld ||
                   m_BotLeftGrabberHeld || m_BotRightGrabberHeld ||
                   m_TopGrabberHeld || m_BotGrabberHeld ||
                   m_LeftGrabberHeld || m_RightGrabberHeld;
    if (m_ViewportHovered && !anyHeld) {
        ImVec2 imGuiMousePos = ImGui::GetMousePos();
        float mousePosX = imGuiMousePos.x - m_ViewportOffset.x;
        float mousePosY = imGuiMousePos.y - m_ViewportOffset.y;

        m_HoveredEntity = entt::null;
        if (mousePosX > 0 && mousePosY > 0 &&
            mousePosX < m_LevelEditorAvailRegion.x &&
            mousePosY < m_LevelEditorAvailRegion.y) {
            glm::vec2 world = PanelMouseToWorld(mousePosX, mousePosY);
            Hamster::UUID hit = m_Scene->GetSpatialIndex().QueryPoint(
                world,
                [this](Hamster::UUID u) {
                    return m_Scene->GetEntityComponent<Hamster::Transform>(u).position.z;
                });
            if (!Hamster::UUID::IsNil(hit)) {
                m_HoveredEntity = m_Scene->GetEntity(hit);
            }
        }
    } else if (!m_ViewportHovered) {
        m_HoveredEntity = entt::null;
    }

    // ── Mouse-pick when click lands inside viewport ──
    // Order (per game-ui feature spec): screen-space UI hit-test first, then
    // grabbers (selected entity only), then world-space spatial-index. UI on
    // top of world means a UI rect over a world entity ALWAYS wins.
    if (ImGui::IsMouseClicked(0) && m_ViewportHovered) {
        ImVec2 imGuiMousePos = ImGui::GetMousePos();
        float mousePosX = imGuiMousePos.x - m_ViewportOffset.x;
        float mousePosY = imGuiMousePos.y - m_ViewportOffset.y;
        bool insideViewport = mousePosX > 0 && mousePosY > 0 &&
                              mousePosX < m_LevelEditorAvailRegion.x &&
                              mousePosY < m_LevelEditorAvailRegion.y;

        // Pass 0 — UI hit-test (screen-space). In play mode, post
        // ButtonClickedEvent; in edit mode, select the entity for editing.
        bool uiConsumed = false;
        if (insideViewport) {
            const bool simRunning = !m_Scene->IsSceneSimulationPaused();
            // UI anchors in the play-area box (world coords), so hit-test in
            // world coords to match the render. Fall back to panel coords if
            // there's no project/box.
            float uiW = m_LevelEditorAvailRegion.x;
            float uiH = m_LevelEditorAvailRegion.y;
            float hx = mousePosX, hy = mousePosY;
            if (auto proj = Hamster::Project::GetCurrentProject()) {
                uiW = static_cast<float>(proj->GetConfig().TargetWidth);
                uiH = static_cast<float>(proj->GetConfig().TargetHeight);
                glm::vec2 w = PanelMouseToWorld(mousePosX, mousePosY);
                hx = w.x;
                hy = w.y;
            }

            // Buttons.
            auto btnView = m_Scene->GetRegistry()
                               .view<Hamster::UIButton, Hamster::ID>();
            for (auto e : btnView) {
                auto &btn = btnView.get<Hamster::UIButton>(e);
                auto &id  = btnView.get<Hamster::ID>(e);
                // Hidden buttons (visible=false, set by scripts at runtime)
                // are neither drawn nor clickable.
                if (!btn.visible) continue;
                Hamster::UIRect r = m_Renderer->ResolveUIButton(btn, uiW, uiH);
                if (r.ContainsPoint(hx, hy)) {
                    if (simRunning) {
                        Hamster::ButtonClickedEvent be(id.uuid);
                        m_Dispatcher->Post<Hamster::ButtonClickedEvent>(be);
                    } else {
                        m_Hierarchy->SetSelectedEntity(e);
                        m_UIHeld = true;
                        m_UIHeldEntity = e;
                        m_UIHeldStartOffset = btn.offset;
                    }
                    uiConsumed = true;
                    break;
                }
            }

            // Text — edit-mode select/drag only (no click events in play).
            if (!uiConsumed && !simRunning) {
                auto txtView = m_Scene->GetRegistry()
                                   .view<Hamster::UIText, Hamster::ID>();
                for (auto e : txtView) {
                    auto &txt = txtView.get<Hamster::UIText>(e);
                    const Hamster::FontAtlas *fa =
                        (txt.bold && m_Renderer->GetFontAtlasBold() &&
                         m_Renderer->GetFontAtlasBold()->IsValid())
                            ? m_Renderer->GetFontAtlasBold()
                            : m_Renderer->GetFontAtlas();
                    if (!fa || !fa->IsValid()) continue;
                    glm::vec2 size = {fa->MeasureWidth(txt.text, txt.fontSize),
                                      txt.fontSize};
                    glm::vec2 tl = Hamster::ResolveAnchoredTopLeft(
                        txt.anchor, txt.offset, size, uiW, uiH);
                    Hamster::UIRect r{tl.x, tl.y, size.x, size.y};
                    if (r.ContainsPoint(hx, hy)) {
                        m_Hierarchy->SetSelectedEntity(e);
                        m_UIHeld = true;
                        m_UIHeldEntity = e;
                        m_UIHeldStartOffset = txt.offset;
                        uiConsumed = true;
                        break;
                    }
                }
            }
        }

        // Pass 1 — grabbers (only if something is selected).
        int grabberID = -1;
        entt::entity selectedEntity = m_Hierarchy->GetSelectedEntity();
        // UI entities don't use the transform resize grabbers — they're
        // anchored/auto-sized — so skip the grabber FBO pick for them.
        bool selIsUI = selectedEntity != entt::null &&
                       m_Scene->GetRegistry().valid(selectedEntity) &&
                       (m_Scene->GetRegistry().all_of<Hamster::UIButton>(
                            selectedEntity) ||
                        m_Scene->GetRegistry().all_of<Hamster::UIText>(
                            selectedEntity));
        if (!uiConsumed && !selIsUI && insideViewport &&
            selectedEntity != entt::null &&
            m_Scene->GetRegistry().valid(selectedEntity)) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, m_LevelEditorAvailRegion.x,
                      m_LevelEditorAvailRegion.y);
            m_FramebufferTexture.Bind();
            // Clear to the IdToColour(-1) sentinel so non-grabber pixels
            // round-trip back to ID -1 (no hit).
            glm::vec3 sentinel = Hamster::Application::IdToColour(-1);
            glClearColor(sentinel.r, sentinel.g, sentinel.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glDisable(GL_BLEND);
            m_Renderer->DrawGuizmo(
                m_Scene->GetRegistry().get<Hamster::Transform>(selectedEntity),
                Hamster::Translate, true);
            glEnable(GL_BLEND);

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            unsigned char data[4];
            glReadPixels((int)mousePosX,
                         (int)m_LevelEditorAvailRegion.y - (int)mousePosY,
                         1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            m_FramebufferTexture.Unbind();
            glDisable(GL_SCISSOR_TEST);

            int pickedID = Hamster::Application::ColourToId(
                glm::vec3(data[0], data[1], data[2]));
            switch (pickedID) {
                case TopLeftGrabberID:
                case TopRightGrabberID:
                case BottomLeftGrabberID:
                case BottomRightGrabberID:
                case TopGrabberID:
                case RightGrabberID:
                case BottomGrabberID:
                case LeftGrabberID:
                    grabberID = pickedID;
                    break;
            }
        }

        if (uiConsumed) {
            // UI consumed the click — no grabber / world pick.
        } else if (grabberID != -1) {
            switch (grabberID) {
                case TopLeftGrabberID:     m_TopLeftGrabberHeld  = true; break;
                case TopRightGrabberID:    m_TopRightGrabberHeld = true; break;
                case BottomLeftGrabberID:  m_BotLeftGrabberHeld  = true; break;
                case BottomRightGrabberID: m_BotRightGrabberHeld = true; break;
                case TopGrabberID:         m_TopGrabberHeld      = true; break;
                case RightGrabberID:       m_RightGrabberHeld    = true; break;
                case BottomGrabberID:      m_BotGrabberHeld      = true; break;
                case LeftGrabberID:        m_LeftGrabberHeld     = true; break;
            }
        } else if (insideViewport) {
            // Pass 2 — entity pick via spatial index.
            glm::vec2 world = PanelMouseToWorld(mousePosX, mousePosY);
            Hamster::UUID hit = m_Scene->GetSpatialIndex().QueryPoint(
                world,
                [this](Hamster::UUID u) {
                    return m_Scene->GetEntityComponent<Hamster::Transform>(u)
                        .position.z;
                });
            if (!Hamster::UUID::IsNil(hit)) {
                entt::entity pickedEntity = m_Scene->GetEntity(hit);
                if (m_Scene->GetRegistry().valid(pickedEntity)) {
                    m_Hierarchy->SetSelectedEntity(pickedEntity);
                    m_EntityHeld = true;
                    Hamster::Transform &t =
                        m_Scene->GetRegistry().get<Hamster::Transform>(
                            pickedEntity);
                    m_MouseHeldTransformX = t.position.x;
                    m_MouseHeldTransformY = t.position.y;
                }
            } else {
                // Background click — deselect.
                m_Hierarchy->SetSelectedEntity(entt::null);
                m_PropertyEditor->SetSelectedEntity(boost::uuids::nil_uuid());
            }
        }
    }

    // ── Right-click pan-start ──
    if (ImGui::IsMouseClicked(1) && m_ViewportHovered) {
        m_RightClickStartPos = ImGui::GetMousePos();
        m_RightClickDragged = false;
        m_BgHeld = true;
    }
    if (m_BgHeld && !m_RightClickDragged) {
        ImVec2 cur = ImGui::GetMousePos();
        float dx = cur.x - m_RightClickStartPos.x;
        float dy = cur.y - m_RightClickStartPos.y;
        if (dx * dx + dy * dy > 25.0f) m_RightClickDragged = true;
    }

    if (ImGui::IsMouseReleased(0)) {
        m_EntityHeld = false;
        m_TopLeftGrabberHeld = m_TopRightGrabberHeld = false;
        m_BotLeftGrabberHeld = m_BotRightGrabberHeld = false;
        m_TopGrabberHeld = m_RightGrabberHeld = false;
        m_BotGrabberHeld = m_LeftGrabberHeld = false;
        m_UIHeld = false;
        m_UIHeldEntity = entt::null;
    }
    if (ImGui::IsMouseReleased(1)) m_BgHeld = false;

    // ── Drag mutates selected entity transform ──
    ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
    entt::entity sel = m_Hierarchy->GetSelectedEntity();
    bool selValid = (sel != entt::null) && m_Scene->GetRegistry().valid(sel);

    if (selValid) {
        Hamster::Transform *t =
            &m_Scene->GetRegistry().get<Hamster::Transform>(sel);

        // Capture the sprite aspect ratio at the start of a corner grab, so
        // Shift can lock to it for the whole drag.
        bool cornerHeld = m_TopLeftGrabberHeld || m_TopRightGrabberHeld ||
                          m_BotLeftGrabberHeld || m_BotRightGrabberHeld;
        if (cornerHeld && !m_CornerGrabActive) {
            m_GrabAspect = (t->size.y != 0.0f) ? (t->size.x / t->size.y) : 1.0f;
        }
        m_CornerGrabActive = cornerHeld;

        // Corner resize. ex/ey: which edge moves (+1 right/bottom, -1 left/top).
        // Width follows horizontal mouse; height follows vertical, OR is derived
        // from width to preserve the aspect ratio while Shift is held. The
        // opposite corner stays fixed (position shifts when a left/top edge moves).
        const bool shiftLock = ImGui::GetIO().KeyShift && m_GrabAspect != 0.0f;
        auto resizeCorner = [&](float ex, float ey) {
            float oldW = t->size.x, oldH = t->size.y;
            float newW = oldW + ex * mouseDelta.x;
            float newH = shiftLock ? (newW / m_GrabAspect)
                                   : (oldH + ey * mouseDelta.y);
            t->size.x = newW;
            t->size.y = newH;
            if (ex < 0.0f) t->position.x += (oldW - newW); // left edge: keep right
            if (ey < 0.0f) t->position.y += (oldH - newH); // top edge: keep bottom
        };

        if (m_BotRightGrabberHeld)      { resizeCorner(+1.0f, +1.0f); }
        else if (m_TopLeftGrabberHeld)  { resizeCorner(-1.0f, -1.0f); }
        else if (m_TopRightGrabberHeld) { resizeCorner(+1.0f, -1.0f); }
        else if (m_BotLeftGrabberHeld)  { resizeCorner(-1.0f, +1.0f); }
        else if (m_TopGrabberHeld) {
            t->size.y -= mouseDelta.y; t->position.y += mouseDelta.y;
        }
        else if (m_BotGrabberHeld)   { t->size.y += mouseDelta.y; }
        else if (m_RightGrabberHeld) { t->size.x += mouseDelta.x; }
        else if (m_LeftGrabberHeld)  {
            t->size.x -= mouseDelta.x; t->position.x += mouseDelta.x;
        }
        else if (m_EntityHeld) {
            ImVec2 d = ImGui::GetMouseDragDelta();
            t->position.x = m_MouseHeldTransformX + d.x;
            t->position.y = m_MouseHeldTransformY + d.y;
        }
    }
    if (m_BgHeld && m_RightClickDragged) {
        m_Renderer->ChangeCameraOffset({mouseDelta.x, mouseDelta.y});
    }

    // UI drag — accumulate from start offset, sign-flip per anchor so +x
    // mouse always moves the rect right regardless of which corner it
    // anchors to.
    if (m_UIHeld && m_UIHeldEntity != entt::null &&
        m_Scene->GetRegistry().valid(m_UIHeldEntity)) {
        auto &reg = m_Scene->GetRegistry();
        Hamster::UIAnchor anchor = Hamster::UIAnchor::TopLeft;
        glm::vec2 *offset = nullptr;
        if (reg.all_of<Hamster::UIButton>(m_UIHeldEntity)) {
            auto &b = reg.get<Hamster::UIButton>(m_UIHeldEntity);
            anchor = b.anchor; offset = &b.offset;
        } else if (reg.all_of<Hamster::UIText>(m_UIHeldEntity)) {
            auto &t = reg.get<Hamster::UIText>(m_UIHeldEntity);
            anchor = t.anchor; offset = &t.offset;
        }
        if (offset) {
            ImVec2 d = ImGui::GetMouseDragDelta();
            // Offset is in play-area (world) units when anchored to the box, so
            // convert the pixel drag by the editor zoom.
            float zoom = m_Renderer->GetZoom();
            float div = (Hamster::Project::GetCurrentProject() && zoom > 0.0f)
                            ? zoom : 1.0f;
            float xSign = (anchor == Hamster::UIAnchor::TopRight ||
                           anchor == Hamster::UIAnchor::MiddleRight ||
                           anchor == Hamster::UIAnchor::BottomRight) ? -1.0f : 1.0f;
            float ySign = (anchor == Hamster::UIAnchor::BottomLeft ||
                           anchor == Hamster::UIAnchor::BottomCentre ||
                           anchor == Hamster::UIAnchor::BottomRight) ? -1.0f : 1.0f;
            offset->x = m_UIHeldStartOffset.x + (d.x / div) * xSign;
            offset->y = m_UIHeldStartOffset.y + (d.y / div) * ySign;
        }
    }

    // ── Render scene into FBO ──
    m_FramebufferTexture.ResizeFrameBuffer(m_LevelEditorAvailRegion.x,
                                           m_LevelEditorAvailRegion.y);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, m_LevelEditorAvailRegion.x, m_LevelEditorAvailRegion.y);

    m_FramebufferTexture.Bind();
    glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

    m_Renderer->Clear();

    // Dot grid
    {
        const float gridSpacing = 20.0f;
        const float dotSize = 2.0f;
        const glm::vec3 dotColor = {0.15f, 0.17f, 0.22f};

        glm::vec2 camOffset = m_Renderer->GetCameraOffset();
        float zoom = m_Renderer->GetZoom();

        float worldLeft  = camOffset.x;
        float worldTop   = camOffset.y;
        float worldRight = camOffset.x + static_cast<float>(m_ViewportWidth) / zoom;
        float worldBot   = camOffset.y + static_cast<float>(m_ViewportHeight) / zoom;

        float startX = std::floor(worldLeft / gridSpacing) * gridSpacing;
        float startY = std::floor(worldTop  / gridSpacing) * gridSpacing;

        for (float wy = startY; wy <= worldBot; wy += gridSpacing) {
            for (float wx = startX; wx <= worldRight; wx += gridSpacing) {
                m_Renderer->DrawFlat({wx - dotSize * 0.5f, wy - dotSize * 0.5f},
                                     {dotSize, dotSize}, 0.0f, dotColor);
            }
        }
    }

    m_Scene->OnRender(false);

    // Play-area outline at world (0,0)→(targetW, targetH). Drawn after the
    // sprite pass so it sits on top of background sprites but below the
    // grabbers/guizmos that come after. Hidden once the popout play window
    // is open (stage 7 — until then it always renders).
    if (auto activeProject = Hamster::Project::GetCurrentProject()) {
        const auto &cfg = activeProject->GetConfig();
        m_Renderer->DrawWorldRectOutline(
            glm::vec2(0.0f, 0.0f),
            glm::vec2(static_cast<float>(cfg.TargetWidth),
                      static_cast<float>(cfg.TargetHeight)),
            glm::vec3(0.45f, 0.45f, 0.45f), 2.0f, 1.0f);
    }

    if (m_HoveredEntity != entt::null && m_HoveredEntity != sel &&
        m_Scene->GetRegistry().valid(m_HoveredEntity)) {
        m_Renderer->DrawHoverOutline(
            m_Scene->GetRegistry().get<Hamster::Transform>(m_HoveredEntity));
    }

    if (selValid) {
        auto &reg = m_Scene->GetRegistry();
        bool isUI = reg.all_of<Hamster::UIButton>(sel) ||
                    reg.all_of<Hamster::UIText>(sel);
        if (isUI) {
            // UI entities live in anchored play-area space, not at their
            // Transform. Draw the selection outline at the resolved UI rect
            // (world coords — the play-area box IS world (0,0)→(target)), so
            // it tracks the element as its anchor/offset change.
            float uiW = m_LevelEditorAvailRegion.x;
            float uiH = m_LevelEditorAvailRegion.y;
            if (auto proj = Hamster::Project::GetCurrentProject()) {
                uiW = static_cast<float>(proj->GetConfig().TargetWidth);
                uiH = static_cast<float>(proj->GetConfig().TargetHeight);
            }
            Hamster::UIRect r{};
            bool haveRect = false;
            if (reg.all_of<Hamster::UIButton>(sel)) {
                r = m_Renderer->ResolveUIButton(reg.get<Hamster::UIButton>(sel),
                                                uiW, uiH);
                haveRect = true;
            } else {
                auto &txt = reg.get<Hamster::UIText>(sel);
                const Hamster::FontAtlas *fa =
                    (txt.bold && m_Renderer->GetFontAtlasBold() &&
                     m_Renderer->GetFontAtlasBold()->IsValid())
                        ? m_Renderer->GetFontAtlasBold()
                        : m_Renderer->GetFontAtlas();
                if (fa && fa->IsValid()) {
                    glm::vec2 size = {fa->MeasureWidth(txt.text, txt.fontSize),
                                      txt.fontSize};
                    glm::vec2 tl = Hamster::ResolveAnchoredTopLeft(
                        txt.anchor, txt.offset, size, uiW, uiH);
                    r = Hamster::UIRect{tl.x, tl.y, size.x, size.y};
                    haveRect = true;
                }
            }
            if (haveRect) {
                m_Renderer->DrawWorldRectOutline(
                    glm::vec2(r.x, r.y), glm::vec2(r.w, r.h),
                    glm::vec3(0.2f, 0.5f, 1.0f), 2.0f, 1.0f);
            }
        } else {
            m_Renderer->DrawGuizmo(reg.get<Hamster::Transform>(sel),
                                   Hamster::Translate, false);
        }
    }

    // ── UI pass ──
    // Render UI through the WORLD projection (the viewport set above,
    // 0..vpW x 0..vpH, is still active) anchored within (0,0)->(target), so it
    // lands on the play-area box and pans/zooms with it — same projection that
    // drew the play-area outline. Without a project, fall back to panel-anchored
    // screen-space UI. (bug 0019)
    if (m_LevelEditorAvailRegion.x > 0 && m_LevelEditorAvailRegion.y > 0) {
        if (auto activeProject = Hamster::Project::GetCurrentProject()) {
            const auto &cfg = activeProject->GetConfig();
            m_Scene->OnRenderUI(static_cast<float>(cfg.TargetWidth),
                                static_cast<float>(cfg.TargetHeight),
                                /*worldProjection=*/true);
        } else {
            glViewport(0, 0,
                       static_cast<int>(m_LevelEditorAvailRegion.x),
                       static_cast<int>(m_LevelEditorAvailRegion.y));
            m_Scene->OnRenderUI(m_LevelEditorAvailRegion.x,
                                m_LevelEditorAvailRegion.y);
        }
    }

    m_FramebufferTexture.Unbind();
    glDisable(GL_SCISSOR_TEST);

    // ── Popout play-window render pass ──
    // Second scene render — into the popout's default framebuffer when the
    // popout is open. The Renderer is shared with the editor, so we save
    // viewport / zoom / camera, install a fixed (0,0)→(W,H) at zoom=1 view
    // for the popout, render, swap its buffers, then restore. After this
    // block the editor's GL context is current again so ImGui draws into
    // the editor window as normal.
    if (m_App->IsPlayWindowOpen()) {
        GLFWwindow *popout = m_App->GetPlayWindow();
        GLFWwindow *editor = m_App->GetWindow();
        if (popout && editor) {
            int savedVpW = m_Renderer->GetViewportWidth();
            int savedVpH = m_Renderer->GetViewportHeight();
            float savedZoom = m_Renderer->GetZoom();
            glm::vec2 savedOffset = m_Renderer->GetCameraOffset();

            int popoutW = savedVpW;
            int popoutH = savedVpH;
            glfwGetFramebufferSize(popout, &popoutW, &popoutH);

            glfwMakeContextCurrent(popout);
            glViewport(0, 0, popoutW, popoutH);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            m_Renderer->SetViewport(popoutH, popoutW);
            m_Renderer->SetZoom(1.0f);
            m_Renderer->SetCameraOffset(glm::vec2(0.0f, 0.0f));

            // Bind the popout-context VAOs for this render (bug 0017).
            m_Renderer->SetPopoutMode(true);
            m_Scene->OnRender(false);
            m_Scene->OnRenderUI(static_cast<float>(popoutW),
                                static_cast<float>(popoutH));
            m_Renderer->SetPopoutMode(false);

            glfwSwapBuffers(popout);

            // Restore editor context + renderer state. The editor's existing
            // viewport-clear below runs against the editor's default FB.
            glfwMakeContextCurrent(editor);
            m_Renderer->SetViewport(savedVpH, savedVpW);
            m_Renderer->SetZoom(savedZoom);
            m_Renderer->SetCameraOffset(savedOffset);
        }
    }

    // Clear default framebuffer to the gap color — fills the regions outside
    // panels (the engine's main loop doesn't clear, the old prototype did this
    // in main.cpp before drawing ImGui).
    glfwGetFramebufferSize(m_App->GetWindow(), &m_ViewportWidth, &m_ViewportHeight);
    glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    glClearColor(kGap.x, kGap.y, kGap.z, kGap.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void EditorLayer::OnImGuiUpdate() {
    GLFWwindow *window = m_App->GetWindow();
    m_Maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0;
    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);
    const float W = static_cast<float>(winW);
    const float H = static_cast<float>(winH);

    static constexpr float kTitleBarH = 32.0f;

    // ── Custom borderless title bar ──
    {
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({W, kTitleBarH});
        float menuFrameY = (kTitleBarH - ImGui::GetFontSize()) * 0.5f;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8, menuFrameY});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, kPanel);
        ImGui::Begin("##TitleBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (m_LogoTex && m_LogoTex->GetTextureId() != 0) {
                float logoSz = kTitleBarH - 12.0f;
                ImVec2 cp = ImGui::GetCursorScreenPos();
                float logoY = cp.y + (kTitleBarH - logoSz) * 0.5f;
                ImGui::GetWindowDrawList()->AddImage(
                    reinterpret_cast<ImTextureID>(
                        static_cast<intptr_t>(m_LogoTex->GetTextureId())),
                    {cp.x, logoY}, {cp.x + logoSz, logoY + logoSz});
                ImGui::Dummy({logoSz, 0});
            }
            ImGui::SameLine(0, 16);

            if (HBeginMenu("File")) {
                // Save is blocked while simulation runs — pairs with the
                // PropertyEditor lock so play mode can't mutate disk either.
                const bool simRunning = m_Scene && !m_Scene->IsSceneSimulationPaused();
                if (HMenuItem("Save Scene", nullptr, !simRunning)) {
                    if (m_Scene) Hamster::Scene::SaveScene(m_Scene);
                }
                if (HMenuItem("New Project")) {
                    m_CreateModal->Show();
                }
                if (HMenuItem("Open Project")) {
                    m_OpenModal->Show();
                }
                HMenuSeparator();
                if (HMenuItem("Exit")) {
                    Hamster::WindowCloseEvent e;
                    m_Dispatcher->Post<Hamster::WindowCloseEvent>(e);
                }
                HEndMenu();
            }
            if (HBeginMenu("Edit"))   { HMenuItem("Undo"); HMenuItem("Redo"); HEndMenu(); }
            if (HBeginMenu("View"))   { HMenuItem("Property Editor"); HMenuItem("Hierarchy"); HMenuItem("Console"); HEndMenu(); }
            if (HBeginMenu("Build"))  { HMenuItem("Build Project"); HEndMenu(); }
            if (HBeginMenu("Help"))   { HMenuItem("About"); HEndMenu(); }

            // Window controls (right-aligned)
            float btnW = 32.0f;
            float btnsX = W - btnW * 3 - 8;
            ImGui::SetCursorPosX(btnsX);

            ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {1, 1, 1, 0.08f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, {1, 1, 1, 0.12f});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

            if (ImGui::Button(ICON_FA_MINUS "##min", {btnW, kTitleBarH})) {
                glfwIconifyWindow(window);
            }
            ImGui::SameLine(0, 0);
            if (ImGui::Button(m_Maximized ? ICON_FA_WINDOW_RESTORE "##max"
                                          : ICON_FA_WINDOW_MAXIMIZE "##max",
                              {btnW, kTitleBarH})) {
                if (m_Maximized) glfwRestoreWindow(window);
                else             glfwMaximizeWindow(window);
                m_Maximized = !m_Maximized;
            }
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.85f, 0.15f, 0.15f, 1.0f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  {0.70f, 0.10f, 0.10f, 1.0f});
            if (ImGui::Button(ICON_FA_XMARK "##close", {btnW, kTitleBarH})) {
                Hamster::WindowCloseEvent e;
                m_Dispatcher->Post<Hamster::WindowCloseEvent>(e);
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);

            ImGui::EndMenuBar();
        }

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
    }

    // Title-bar dragging — forward to Windows on Win32 so the OS handles drag,
    // auto-restore-from-maximised, and Aero Snap (drag to edges → half/full screen).
    {
        double cx, cy;
        glfwGetCursorPos(window, &cx, &cy);
        bool inTitleBar = cy < kTitleBarH && cx < (W - 96);

        if (inTitleBar && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
#ifdef _WIN32
            HWND hwnd = glfwGetWin32Window(window);
            ReleaseCapture();
            // WM_NCLBUTTONDOWN with HTCAPTION enters Windows' modal drag loop;
            // this call blocks until the user releases the mouse.
            SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            // Aero Snap may have toggled maximisation — sync our flag.
            m_Maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0;
#else
            // Fallback for non-Win32: simple manual drag.
            int wx, wy;
            glfwGetWindowPos(window, &wx, &wy);
            m_TitleDragging = true;
            m_DragStartX = cx + wx;
            m_DragStartY = cy + wy;
            m_WinStartX = wx;
            m_WinStartY = wy;
#endif
        }

#ifndef _WIN32
        bool mouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (m_TitleDragging && mouseDown) {
            double cx2, cy2;
            int wx2, wy2;
            glfwGetCursorPos(window, &cx2, &cy2);
            glfwGetWindowPos(window, &wx2, &wy2);
            double screenCx = cx2 + wx2;
            double screenCy = cy2 + wy2;
            int newX = m_WinStartX + static_cast<int>(screenCx - m_DragStartX);
            int newY = m_WinStartY + static_cast<int>(screenCy - m_DragStartY);
            glfwSetWindowPos(window, newX, newY);
        }
        if (!mouseDown) m_TitleDragging = false;
#endif

        if (inTitleBar && ImGui::IsMouseDoubleClicked(0)) {
            if (m_Maximized) glfwRestoreWindow(window);
            else             glfwMaximizeWindow(window);
            m_Maximized = !m_Maximized;
        }
    }

    const float baseX = 0.0f;
    const float baseY = kTitleBarH;

    const float gap = 8.0f;
    const float totalW = W - gap * 2.0f;
    const float totalH = H - kTitleBarH - gap * 2.0f;

    const float leftW   = totalW * 0.175f;
    const float rightW  = totalW * 0.145f;
    const float centerW = totalW - leftW - rightW - gap * 2.0f;
    const float bottomH = totalH * 0.28f;
    const float topH    = totalH - bottomH - gap;

    const float leftX   = baseX + gap;
    const float centerX = leftX + leftW + gap;
    const float rightX  = centerX + centerW + gap;
    const float topY    = baseY + gap;
    const float bottomY = topY + topH + gap;

    // Sync selection from Hierarchy to PropertyEditor
    if (m_Scene) {
        entt::entity sel = m_Hierarchy->GetSelectedEntity();
        if (m_Scene->GetRegistry().valid(sel)) {
            m_PropertyEditor->SetSelectedEntity(m_Scene->GetEntityUUID(sel));
        } else {
            m_PropertyEditor->SetSelectedEntity(Hamster::UUID::GetNil());
        }
    }

    // Property Editor
    ImGui::SetNextWindowPos({leftX, topY});
    ImGui::SetNextWindowSize({leftW, totalH});
    ImGui::Begin("##PropEditor", nullptr, kPanelFlags);
    m_PropertyEditor->Render();
    ImGui::End();

    // Level Editor
    ImGui::SetNextWindowPos({centerX, topY});
    ImGui::SetNextWindowSize({centerW, topH});
    static bool focusedLevelOnce = false;
    if (!focusedLevelOnce) {
        ImGui::SetNextWindowFocus();
        focusedLevelOnce = true;
    }
    ImGui::Begin("##LevelEditor", nullptr, kPanelFlags);
    ImVec2 outViewportTL;
    ImVec2 outAvail;
    m_LevelEditor->Render(m_FramebufferTexture.GetTextureID(),
                          m_Renderer, m_Scene.get(),
                          outViewportTL, outAvail);
    m_LevelEditorAvailRegion = outAvail;
    m_ViewportOffset.x = outViewportTL.x;
    m_ViewportOffset.y = outViewportTL.y;
    m_ViewportHovered = ImGui::IsWindowHovered();

    // Wheel-zoom + right-click context menu trigger
    if (m_ViewportHovered && m_Scene) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ImVec2 mp = ImGui::GetMousePos();
            float mpX = mp.x - m_ViewportOffset.x;
            float mpY = mp.y - m_ViewportOffset.y;
            m_Renderer->AdjustZoom(wheel * 0.1f, mpX, mpY);
        }

        // Right-click release (without drag) opens context menu
        if (ImGui::IsMouseReleased(1) && !m_RightClickDragged) {
            float mpX = m_RightClickStartPos.x - m_ViewportOffset.x;
            float mpY = m_RightClickStartPos.y - m_ViewportOffset.y;

            m_ContextMenuWorldPos = PanelMouseToWorld(mpX, mpY);

            // Spatial-index query replaces the previous full-scene flat
            // render + glReadPixels. Same semantics, no per-frame O(N) cost.
            Hamster::UUID hit = m_Scene->GetSpatialIndex().QueryPoint(
                m_ContextMenuWorldPos,
                [this](Hamster::UUID u) {
                    return m_Scene->GetEntityComponent<Hamster::Transform>(u)
                        .position.z;
                });

            if (!Hamster::UUID::IsNil(hit)) {
                entt::entity pickedEntity = m_Scene->GetEntity(hit);
                if (m_Scene->GetRegistry().valid(pickedEntity)) {
                    m_ContextMenuEntity = pickedEntity;
                    m_OpenEntityContextMenu = true;
                } else {
                    m_ContextMenuEntity = entt::null;
                    m_OpenSceneContextMenu = true;
                }
            } else {
                m_ContextMenuEntity = entt::null;
                m_OpenSceneContextMenu = true;
            }
        }
    }

    if (m_OpenSceneContextMenu) {
        ImGui::OpenPopup("##SceneCtx");
        m_OpenSceneContextMenu = false;
    }
    if (m_OpenEntityContextMenu) {
        ImGui::OpenPopup("##EntityCtx");
        m_OpenEntityContextMenu = false;
    }

    if (ImGui::BeginPopup("##SceneCtx")) {
        if (ImGui::MenuItem(ICON_FA_PLUS "  New Entity")) {
            Hamster::UUID newUUID = m_Scene->CreateEntity();
            auto &t = m_Scene->GetEntityComponent<Hamster::Transform>(newUUID);
            t.position.x = m_ContextMenuWorldPos.x;
            t.position.y = m_ContextMenuWorldPos.y;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##EntityCtx")) {
        if (m_ContextMenuEntity != entt::null &&
            m_Scene->GetRegistry().valid(m_ContextMenuEntity)) {
            Hamster::UUID euuid = m_Scene->GetEntityUUID(m_ContextMenuEntity);

            if (ImGui::MenuItem(ICON_FA_TRASH "  Delete")) {
                if (m_Hierarchy->GetSelectedEntity() == m_ContextMenuEntity) {
                    m_Hierarchy->SetSelectedEntity(entt::null);
                    m_PropertyEditor->SetSelectedEntity(Hamster::UUID::GetNil());
                }
                m_Scene->DestroyEntity(euuid);
            }
            if (ImGui::MenuItem(ICON_FA_CLONE "  Duplicate")) {
                Hamster::UUID newUUID = m_Scene->CreateEntity();
                auto &src = m_Scene->GetEntityComponent<Hamster::Transform>(euuid);
                auto &dst = m_Scene->GetEntityComponent<Hamster::Transform>(newUUID);
                dst.position = src.position + glm::vec3(10.0f, 10.0f, 0.0f);
                dst.rotation = src.rotation;
                dst.size = src.size;
                auto &srcName = m_Scene->GetEntityComponent<Hamster::Name>(euuid);
                auto &dstName = m_Scene->GetEntityComponent<Hamster::Name>(newUUID);
                dstName.name = srcName.name + " Copy";
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();

    // Focus the Console when the simulation starts (edit -> play edge), so
    // script logs and errors are immediately visible. SetNextWindowFocus before
    // Begin is the reliable focus path (a SetWindowFocus inside the panel got
    // overridden by the viewport keeping focus after the Play-button click).
    bool simRunning = m_Scene && !m_Scene->IsSceneSimulationPaused();
    bool focusConsole = simRunning && !m_PrevSimRunning;
    if (focusConsole) m_BottomPanel->ShowConsole();
    m_PrevSimRunning = simRunning;

    // Bottom panel
    ImGui::SetNextWindowPos({centerX, bottomY});
    ImGui::SetNextWindowSize({centerW, bottomH});
    if (focusConsole) ImGui::SetNextWindowFocus();
    ImGui::Begin("##BottomPanel", nullptr, kPanelFlags);
    m_BottomPanel->Render();
    ImGui::End();

    // Hierarchy
    ImGui::SetNextWindowPos({rightX, topY});
    ImGui::SetNextWindowSize({rightW, totalH});
    ImGui::Begin("##Hierarchy", nullptr, kPanelFlags);
    m_Hierarchy->Render();
    ImGui::End();

    // Modal-style floating windows
    m_CreateModal->Render(&m_Registry);
    m_OpenModal->Render(&m_Registry);
    if (m_ColliderEditor->IsOpen())       m_ColliderEditor->Render();
    if (m_SpritesheetEditor->IsOpen())    m_SpritesheetEditor->Render();
}

void EditorLayer::ActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
}

glm::vec2 EditorLayer::PanelMouseToWorld(float panelX, float panelY) const {
    glm::vec2 cam = m_Renderer->GetCameraOffset();
    float zoom = m_Renderer->GetZoom();
    int vpH = m_Renderer->GetViewportHeight();
    int panelH = static_cast<int>(m_LevelEditorAvailRegion.y);
    float worldX = cam.x + panelX / zoom;
    float worldY = cam.y + static_cast<float>(vpH - panelH + (int)panelY) / zoom;
    return {worldX, worldY};
}

void EditorLayer::FramebufferSizeChanged(Hamster::FramebufferResizeEvent &e) {
    m_ViewportWidth  = e.GetWidth();
    m_ViewportHeight = e.GetHeight();
}
