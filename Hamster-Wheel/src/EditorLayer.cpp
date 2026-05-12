//
// Created by Jaden on 23/08/2024.
//

#include "EditorLayer.h"

#include <cmath>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include <Renderer/Renderer.h>

#include "Core/Application.h"
#include "Theme/IconsFontAwesome6.h"

EditorLayer::EditorLayer(Hamster::Application *app,
                         std::shared_ptr<Hamster::Scene> scene)
    : m_App(app), m_Dispatcher(app->GetEventDispatcher().get()),
      m_Renderer(app->GetRenderer()), m_Scene(scene),
      m_FramebufferTexture(1920, 1080) {
    auto *assetManager = app->GetAssetManager();
    m_ColliderEditor = std::make_unique<ColliderEditor>();
    m_PropertyEditor = std::make_unique<PropertyEditor>(m_Dispatcher, m_Scene, assetManager, m_ColliderEditor.get());
    m_Hierarchy = std::make_unique<Hierarchy>(m_Dispatcher, m_Scene, m_Renderer);
    m_AssetBrowser = std::make_unique<AssetBrowser>(m_Dispatcher, m_Scene, assetManager);
    m_PropertyEditor->SetAssetBrowser(m_AssetBrowser.get());
    m_MenuBar = std::make_unique<MenuBar>(m_Dispatcher, m_Scene);
    m_Console = std::make_unique<Console>(m_Dispatcher, m_Scene);
}

void EditorLayer::OnAttach() {
    glfwGetFramebufferSize(
        m_App->GetWindow(),
        &m_ViewportWidth, &m_ViewportHeight);

    m_Dispatcher->Subscribe(Hamster::ActiveSceneChanged,
                        FORWARD_CALLBACK_FUNCTION(EditorLayer::ActiveSceneChanged,
                                                  Hamster::ActiveSceneChangedEvent));

    m_Dispatcher->Subscribe(Hamster::FramebufferResize,
                        FORWARD_CALLBACK_FUNCTION(EditorLayer::FramebufferSizeChanged,
                                                  Hamster::FramebufferResizeEvent));

    std::string iniPath = Hamster::Application::GetExecutablePath() +
        "/../share/Resources/Hamster-Wheel/Resources/default.ini";

    ImGui::LoadIniSettingsFromDisk(iniPath.c_str());

    // change this to somewhere thats only executed when a new project is created
}

void EditorLayer::OnUpdate() {
    if (ImGui::IsMouseClicked(0)) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, m_LevelEditorAvailRegion.x, m_LevelEditorAvailRegion.y);

        m_FramebufferTexture.Bind();
        m_Renderer->Clear();
        m_Scene->OnRender(true);

        entt::entity selectedEntity = m_Hierarchy->GetSelectedEntity();

        if (selectedEntity != entt::null) {
            glDisable(GL_BLEND);
            m_Renderer->DrawGuizmo(
                m_Scene->GetRegistry().get<Hamster::Transform>(selectedEntity),
                Hamster::Translate, true);
            glEnable(GL_BLEND);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        unsigned char data[4];

        const ImVec2 imGuiMousePos = ImGui::GetMousePos();

        float mousePosX = imGuiMousePos.x - m_ViewportOffset.x;
        float mousePosY = imGuiMousePos.y - m_ViewportOffset.y;

        // dont forget to check mousepos is valid
        glReadPixels(mousePosX, m_ViewportHeight - mousePosY, 1, 1, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);

        m_FramebufferTexture.Unbind();
        glDisable(GL_SCISSOR_TEST);

        int pickedID =
                Hamster::Application::ColourToId(glm::vec3(data[0], data[1], data[2]));

        auto pickedEntity = static_cast<entt::entity>(pickedID);

        //   if (pickedID != -1 && m_Scene->GetRegistry().valid(pickedEntity)) {
        //     m_Hierarchy->SetSelectedEntity(pickedEntity);
        //   } else if (pickedID == -1) {
        //     m_Hierarchy->SetSelectedEntity(entt::null);
        //     m_PropertyEditor->SetSelectedEntity(boost::uuids::nil_uuid());
        //   } else if (pickedID == XGuizmoID) {
        //     xGuizmoHeld = true;
        //     mouseHeldOffsetX =
        //         mousePosX - m_Scene->GetRegistry()
        //                         .get<Hamster::Transform>(selectedEntity)
        //                         .position.x;
        //   } else if (pickedID == YGuizmoID) {
        //     yGuizmoHeld = true;
        //     mouseHeldOffsetY =
        //         mousePosY - m_Scene->GetRegistry()
        //                         .get<Hamster::Transform>(selectedEntity)
        //                         .position.y;
        //   } else if (pickedID == GrabberGuizmoID) {
        //     pickedHeld = true;
        //
        //     mouseHeldOffsetX =
        //         mousePosX - m_Scene->GetRegistry()
        //                         .get<Hamster::Transform>(selectedEntity)
        //                         .position.x;
        //
        //     mouseHeldOffsetY =
        //         mousePosY - m_Scene->GetRegistry()
        //                         .get<Hamster::Transform>(selectedEntity)
        //                         .position.y;
        //   }
        // }


        switch (pickedID) {
            case -1: {
                if (mousePosX > 0 && mousePosY > 0 && mousePosX < m_LevelEditorAvailRegion.x && mousePosY <
                    m_LevelEditorAvailRegion.y) {
                    m_Hierarchy->SetSelectedEntity(entt::null);
                    m_PropertyEditor->SetSelectedEntity(boost::uuids::nil_uuid());
                }


                break;
            }
            case TopLeftGrabberID: {
                topLeftGrabberHeld = true;

                // mouseHeldOffsetX =
                //     mousePosX - m_Scene->GetRegistry()
                //                     .get<Hamster::Transform>(selectedEntity)
                //                     .position.x;

                break;
            }

            case TopRightGrabberID: {
                topRightGrabberHeld = true;

                break;
            }
            case BottomLeftGrabberID: {
                bottomLeftGrabberHeld = true;

                break;
            }
            case BottomRightGrabberID: {
                bottomRightGrabberHeld = true;
                break;
            }
            case TopGrabberID: {
                topGrabberHeld = true;
                break;
            }
            case RightGrabberID: {
                rightGrabberHeld = true;
                break;
            }
            case BottomGrabberID: {
                bottomGrabberHeld = true;
                break;
            }
            case LeftGrabberID: {
                leftGrabberHeld = true;
                break;
            }
            default: {
                if (m_Scene->GetRegistry().valid(pickedEntity)) {
                    m_Hierarchy->SetSelectedEntity(pickedEntity);

                    entityHeld = true;

                    Hamster::Transform &transform =
                            m_Scene->GetRegistry().get<Hamster::Transform>(pickedEntity);

                    mouseHeldTransformX = transform.position.x;
                    mouseHeldTransformY = transform.position.y;
                }

                break;
            }
        }
    }

    if (ImGui::IsMouseClicked(1)) {
        m_RightClickStartPos = ImGui::GetMousePos();
        m_RightClickDragged = false;
        sceneBackgroundHeld = true;
    }

    if (sceneBackgroundHeld && !m_RightClickDragged) {
        ImVec2 currentPos = ImGui::GetMousePos();
        float dx = currentPos.x - m_RightClickStartPos.x;
        float dy = currentPos.y - m_RightClickStartPos.y;
        if (dx * dx + dy * dy > 25.0f) {
            m_RightClickDragged = true;
        }
    }

    if (ImGui::IsMouseReleased(0)) {
        xGuizmoHeld = false;
        yGuizmoHeld = false;
        pickedHeld = false;

        entityHeld = false;

        topLeftGrabberHeld = false;
        topRightGrabberHeld = false;
        bottomLeftGrabberHeld = false;
        bottomRightGrabberHeld = false;
        topGrabberHeld = false;
        rightGrabberHeld = false;
        bottomGrabberHeld = false;
        leftGrabberHeld = false;
    }

    if (ImGui::IsMouseReleased(1)) {
        sceneBackgroundHeld = false;
    }

    // ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
    //
    // if (ImGui::IsMouseClicked(1) && m_Scene->IsSceneSimulationPaused()) {
    //     std::cout << mouseDelta.x << ", " << mouseDelta.y << std::endl;
    //
    //     m_Renderer->ChangeCameraOffset(mouseDelta.x * m_MouseDragSpeed, mouseDelta.y * m_MouseDragSpeed);
    // }

    ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;


    if (bottomRightGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.x += mouseDelta.x;
        entityTransform->size.y += mouseDelta.y;
    } else if (topLeftGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.x -= mouseDelta.x;
        entityTransform->size.y -= mouseDelta.y;

        entityTransform->position.x += mouseDelta.x;
        entityTransform->position.y += mouseDelta.y;
    } else if (topRightGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.x += mouseDelta.x;
        entityTransform->size.y -= mouseDelta.y;

        entityTransform->position.y += mouseDelta.y;
    } else if (bottomLeftGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.x -= mouseDelta.x;
        entityTransform->size.y += mouseDelta.y;

        entityTransform->position.x += mouseDelta.x;
    } else if (topGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.y -= mouseDelta.y;
        entityTransform->position.y += mouseDelta.y;
    } else if (bottomGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.y += mouseDelta.y;
    } else if (rightGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.x += mouseDelta.x;
    } else if (leftGrabberHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        entityTransform->size.x -= mouseDelta.x;
        entityTransform->position.x += mouseDelta.x;
    } else if (entityHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        ImVec2 mouseDragDelta = ImGui::GetMouseDragDelta();

        entityTransform->position.x = mouseHeldTransformX + mouseDragDelta.x;
        entityTransform->position.y = mouseHeldTransformY + mouseDragDelta.y;
    } else if (sceneBackgroundHeld && m_RightClickDragged) {
        m_Renderer->ChangeCameraOffset({mouseDelta.x * m_MouseDragSpeed, mouseDelta.y * m_MouseDragSpeed});
    }

    m_FramebufferTexture.ResizeFrameBuffer(m_LevelEditorAvailRegion.x,
                                           m_LevelEditorAvailRegion.y);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, m_LevelEditorAvailRegion.x, m_LevelEditorAvailRegion.y);

    m_FramebufferTexture.Bind();
    glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

    m_Renderer->Clear();

    // Dot grid — rendered into the framebuffer so it sits behind all sprites
    {
        const float gridSpacing = 20.0f;
        const float dotSize = 2.0f;
        const glm::vec3 dotColor = {0.25f, 0.25f, 0.25f};

        glm::vec2 camOffset = m_Renderer->GetCameraOffset();
        float zoom = m_Renderer->GetZoom();

        float worldLeft = camOffset.x;
        float worldTop = camOffset.y;
        float worldRight = camOffset.x + static_cast<float>(m_ViewportWidth) / zoom;
        float worldBottom = camOffset.y + static_cast<float>(m_ViewportHeight) / zoom;

        float startX = std::floor(worldLeft / gridSpacing) * gridSpacing;
        float startY = std::floor(worldTop / gridSpacing) * gridSpacing;

        for (float wy = startY; wy <= worldBottom; wy += gridSpacing) {
            for (float wx = startX; wx <= worldRight; wx += gridSpacing) {
                m_Renderer->DrawFlat({wx - dotSize * 0.5f, wy - dotSize * 0.5f},
                                     {dotSize, dotSize}, 0.0f, dotColor);
            }
        }
    }

    m_Scene->OnRender(false);

    entt::entity selectedEntity = m_Hierarchy->GetSelectedEntity();

    if (selectedEntity != entt::null) {
        m_Renderer->DrawGuizmo(
            m_Scene->GetRegistry().get<Hamster::Transform>(selectedEntity),
            Hamster::Translate, false);
    }

    m_FramebufferTexture.Unbind();
    glDisable(GL_SCISSOR_TEST);
}

void EditorLayer::OnImGuiUpdate() {
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(100, 100),
        ImVec2(m_ViewportWidth + 16.0f, m_ViewportHeight + 35.0f));
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());


    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!ImGui::Begin("Level Editor", &m_WindowOpen)) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }
    ImGui::PopStyleVar();

    m_WindowFocused = ImGui::IsWindowFocused();

    m_LevelEditorAvailRegion = ImGui::GetContentRegionAvail();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 windowBorderSize = ImGui::GetStyle().WindowPadding;

    m_ViewportOffset.x =
            windowPos.x + (windowSize.x - m_LevelEditorAvailRegion.x) / 2;
    m_ViewportOffset.y =
            windowPos.y +
            (windowSize.y - m_LevelEditorAvailRegion.y - windowBorderSize.y) -
            (m_ViewportHeight - m_LevelEditorAvailRegion.y);

    if (ImGui::IsWindowHovered()) {
        float mouseWheelDelta = ImGui::GetIO().MouseWheel;

        if (mouseWheelDelta != 0.0f) {
            const ImVec2 imGuiMousePos = ImGui::GetMousePos();

            float mousePosX = imGuiMousePos.x - m_ViewportOffset.x;
            float mousePosY = imGuiMousePos.y - m_ViewportOffset.y;

            m_Renderer->AdjustZoom(mouseWheelDelta * m_MouseWheelZoomSpeed, mousePosX, mousePosY);
        }

        if (ImGui::IsMouseReleased(1) && !m_RightClickDragged) {
            float mousePosX = m_RightClickStartPos.x - m_ViewportOffset.x;
            float mousePosY = m_RightClickStartPos.y - m_ViewportOffset.y;

            m_ContextMenuWorldPos = m_Renderer->ScreenToWorldPos({mousePosX, mousePosY});

            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, m_LevelEditorAvailRegion.x, m_LevelEditorAvailRegion.y);
            m_FramebufferTexture.Bind();
            m_Renderer->Clear();
            m_Scene->OnRender(true);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            unsigned char data[4];
            glReadPixels(mousePosX, m_ViewportHeight - mousePosY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
            m_FramebufferTexture.Unbind();
            glDisable(GL_SCISSOR_TEST);

            int pickedID = Hamster::Application::ColourToId(glm::vec3(data[0], data[1], data[2]));
            auto pickedEntity = static_cast<entt::entity>(pickedID);

            if (pickedID != -1 && m_Scene->GetRegistry().valid(pickedEntity)) {
                m_ContextMenuEntity = pickedEntity;
                m_OpenEntityContextMenu = true;
            } else {
                m_ContextMenuEntity = entt::null;
                m_OpenSceneContextMenu = true;
            }
        }
    }

    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::GetWindowDrawList()->AddImage(
        (void *) (intptr_t) m_FramebufferTexture.GetTextureID(),
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + m_LevelEditorAvailRegion.x,
               pos.y + m_LevelEditorAvailRegion.y),
        ImVec2(0, 1), ImVec2(1, 0));

    // Play/Pause/Stop overlay — top-center of the viewport
    {
        const float btnSize = 28.0f;
        const float spacing = 6.0f;
        const float overlayW = btnSize * 3 + spacing * 2;
        const float overlayX = pos.x + (m_LevelEditorAvailRegion.x - overlayW) * 0.5f;
        const float overlayY = pos.y + 8.0f;

        ImGui::SetCursorScreenPos({overlayX, overlayY});

        bool paused = m_Scene->IsSceneSimulationPaused();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4, 4});

        if (paused) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.337f, 0.576f, 0.439f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.400f, 0.660f, 0.510f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.260f, 0.490f, 0.360f, 1.0f));
            if (ImGui::Button(ICON_FA_PLAY "##play", {btnSize, btnSize})) {
                m_Scene->RunSceneSimulation();
            }
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.337f, 0.576f, 0.439f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.400f, 0.660f, 0.510f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.260f, 0.490f, 0.360f, 1.0f));
            if (ImGui::Button(ICON_FA_PAUSE "##pause", {btnSize, btnSize})) {
                m_Scene->PauseSceneSimulation();
            }
            ImGui::PopStyleColor(3);
        }

        ImGui::SameLine(0, spacing);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.878f, 0.290f, 0.310f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.920f, 0.360f, 0.380f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.780f, 0.220f, 0.240f, 1.0f));
        if (ImGui::Button(ICON_FA_STOP "##stop", {btnSize, btnSize})) {
            m_Scene->PauseSceneSimulation();
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar(2);
    }

    // FPS counter — top-right of viewport, visible only during simulation
    if (m_Scene->IsSceneRunning()) {
        char fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", ImGui::GetIO().Framerate);
        ImVec2 textSize = ImGui::CalcTextSize(fpsBuf);
        float fpsX = pos.x + m_LevelEditorAvailRegion.x - textSize.x - 12.0f;
        float fpsY = pos.y + 12.0f;
        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled({fpsX - 6, fpsY - 3}, {fpsX + textSize.x + 6, fpsY + textSize.y + 3},
                          IM_COL32(0, 0, 0, 140), 4.0f);
        dl->AddText({fpsX, fpsY}, IM_COL32(255, 255, 255, 220), fpsBuf);
    }

    // Zoom slider overlay — bottom-right, pill track with magnifying glass icon
    {
        const float trackW = 120.0f;
        const float trackH = 4.0f;
        const float dotRadius = 6.0f;
        const float zoomMin = 0.1f;
        const float zoomMax = 5.0f;
        const float margin = 16.0f;

        float zoom = m_Renderer->GetZoom();
        char zoomLabel[16];
        snprintf(zoomLabel, sizeof(zoomLabel), "%.0f%%", zoom * 100.0f);
        ImVec2 textSize = ImGui::CalcTextSize(zoomLabel);

        const char *icon = ICON_FA_MAGNIFYING_GLASS;
        ImVec2 iconSize = ImGui::CalcTextSize(icon);

        const float totalW = iconSize.x + 8.0f + trackW + 10.0f + textSize.x;
        const float overlayX = pos.x + m_LevelEditorAvailRegion.x - totalW - margin;
        const float overlayY = pos.y + m_LevelEditorAvailRegion.y - margin - dotRadius;

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        // Magnifying glass icon
        drawList->AddText(ImVec2(overlayX, overlayY - iconSize.y * 0.5f),
                          IM_COL32(255, 255, 255, 160), icon);

        // Track (pill-shaped)
        float trackX = overlayX + iconSize.x + 8.0f;
        float trackY = overlayY;

        drawList->AddRectFilled(
            ImVec2(trackX, trackY - trackH * 0.5f),
            ImVec2(trackX + trackW, trackY + trackH * 0.5f),
            IM_COL32(255, 255, 255, 60), trackH * 0.5f);

        float t = (zoom - zoomMin) / (zoomMax - zoomMin);
        float dotX = trackX + t * trackW;

        ImVec2 mousePos = ImGui::GetMousePos();
        bool hovered = mousePos.x >= trackX - dotRadius &&
                       mousePos.x <= trackX + trackW + dotRadius &&
                       mousePos.y >= trackY - dotRadius * 2 &&
                       mousePos.y <= trackY + dotRadius * 2;

        static bool zoomSliderDragging = false;

        if (hovered && ImGui::IsMouseClicked(0)) {
            zoomSliderDragging = true;
        }
        if (ImGui::IsMouseReleased(0)) {
            zoomSliderDragging = false;
        }

        if (zoomSliderDragging) {
            float newT = (mousePos.x - trackX) / trackW;
            if (newT < 0.0f) newT = 0.0f;
            if (newT > 1.0f) newT = 1.0f;
            float newZoom = zoomMin + newT * (zoomMax - zoomMin);

            float centerX = m_LevelEditorAvailRegion.x * 0.5f;
            float centerY = m_LevelEditorAvailRegion.y * 0.5f;
            m_Renderer->AdjustZoom(newZoom - zoom, centerX, centerY);
        }

        ImU32 dotColor = zoomSliderDragging ? IM_COL32(255, 255, 255, 255)
                                            : IM_COL32(255, 255, 255, 200);
        drawList->AddCircleFilled(ImVec2(dotX, trackY), dotRadius, dotColor);

        // Percentage label
        drawList->AddText(ImVec2(trackX + trackW + 10.0f, overlayY - textSize.y * 0.5f),
                          IM_COL32(255, 255, 255, 200), zoomLabel);
    }

    // Axis gizmo overlay — bottom-right, above zoom slider
    {
        const float armLength = 40.0f;
        const float lineThickness = 3.0f;
        const float arrowSize = 8.0f;
        const float margin = 16.0f;

        const float gizmoW = armLength + arrowSize;
        const float originX = pos.x + m_LevelEditorAvailRegion.x - margin - gizmoW * 0.5f - 20.0f;
        const float originY = pos.y + m_LevelEditorAvailRegion.y - margin - 30.0f;

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        ImVec2 origin(originX, originY);
        ImVec2 xEnd(originX + armLength, originY);
        ImVec2 yEnd(originX, originY - armLength);

        ImU32 xColor = m_AxisGizmoDraggingX ? IM_COL32(255, 120, 120, 255) : IM_COL32(220, 80, 80, 220);
        ImU32 yColor = m_AxisGizmoDraggingY ? IM_COL32(120, 255, 120, 255) : IM_COL32(80, 200, 80, 220);

        drawList->AddLine(origin, xEnd, xColor, lineThickness);
        drawList->AddTriangleFilled(
            ImVec2(xEnd.x, xEnd.y - arrowSize * 0.6f),
            ImVec2(xEnd.x, xEnd.y + arrowSize * 0.6f),
            ImVec2(xEnd.x + arrowSize, xEnd.y),
            xColor);

        drawList->AddLine(origin, yEnd, yColor, lineThickness);
        drawList->AddTriangleFilled(
            ImVec2(yEnd.x - arrowSize * 0.6f, yEnd.y),
            ImVec2(yEnd.x + arrowSize * 0.6f, yEnd.y),
            ImVec2(yEnd.x, yEnd.y - arrowSize),
            yColor);

        drawList->AddText(ImVec2(xEnd.x + arrowSize + 2.0f, xEnd.y - 7.0f), xColor, "X");
        drawList->AddText(ImVec2(yEnd.x - 4.0f, yEnd.y - arrowSize - 14.0f), yColor, "Y");

        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;

        float xDistY = std::abs(mousePos.y - origin.y);
        bool xHovered = mousePos.x >= origin.x - 6.0f &&
                        mousePos.x <= xEnd.x + arrowSize + 4.0f &&
                        xDistY < 10.0f;

        float yDistX = std::abs(mousePos.x - origin.x);
        bool yHovered = mousePos.y >= yEnd.y - arrowSize - 4.0f &&
                        mousePos.y <= origin.y + 6.0f &&
                        yDistX < 10.0f;

        if (ImGui::IsMouseClicked(0)) {
            if (xHovered) m_AxisGizmoDraggingX = true;
            else if (yHovered) m_AxisGizmoDraggingY = true;
        }

        if (ImGui::IsMouseReleased(0)) {
            m_AxisGizmoDraggingX = false;
            m_AxisGizmoDraggingY = false;
        }

        if (m_AxisGizmoDraggingX || m_AxisGizmoDraggingY) {
            if (m_AxisWrapSkipFrame) {
                m_AxisWrapSkipFrame = false;
            } else {
                GLFWwindow *window = m_App->GetWindow();
                double cx, cy;
                glfwGetCursorPos(window, &cx, &cy);
                int winW, winH;
                glfwGetWindowSize(window, &winW, &winH);

                bool wrapped = false;
                if (cx <= 1.0) { cx = winW - 2.0; wrapped = true; }
                else if (cx >= winW - 1.0) { cx = 2.0; wrapped = true; }
                if (cy <= 1.0) { cy = winH - 2.0; wrapped = true; }
                else if (cy >= winH - 1.0) { cy = 2.0; wrapped = true; }

                if (wrapped) {
                    glfwSetCursorPos(window, cx, cy);
                    m_AxisWrapSkipFrame = true;
                } else {
                    if (m_AxisGizmoDraggingX) {
                        m_Renderer->ChangeCameraOffset({mouseDelta.x * m_MouseDragSpeed, 0.0f});
                    }
                    if (m_AxisGizmoDraggingY) {
                        m_Renderer->ChangeCameraOffset({0.0f, mouseDelta.y * m_MouseDragSpeed});
                    }
                }
            }
        }
    }

    // Context menus
    if (m_OpenSceneContextMenu) {
        ImGui::OpenPopup("##SceneContextMenu");
        m_OpenSceneContextMenu = false;
    }
    if (m_OpenEntityContextMenu) {
        ImGui::OpenPopup("##EntityContextMenu");
        m_OpenEntityContextMenu = false;
    }

    if (ImGui::BeginPopup("##SceneContextMenu")) {
        if (ImGui::MenuItem(ICON_FA_PLUS "  New Entity")) {
            Hamster::UUID newUUID = m_Scene->CreateEntity();
            auto &transform = m_Scene->GetEntityComponent<Hamster::Transform>(newUUID);
            transform.position.x = m_ContextMenuWorldPos.x;
            transform.position.y = m_ContextMenuWorldPos.y;
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("##EntityContextMenu")) {
        if (m_ContextMenuEntity != entt::null && m_Scene->GetRegistry().valid(m_ContextMenuEntity)) {
            Hamster::UUID entityUUID = m_Scene->GetEntityUUID(m_ContextMenuEntity);

            if (ImGui::MenuItem(ICON_FA_TRASH "  Delete")) {
                if (m_Hierarchy->GetSelectedEntity() == m_ContextMenuEntity) {
                    m_Hierarchy->SetSelectedEntity(entt::null);
                    m_PropertyEditor->SetSelectedEntity(boost::uuids::nil_uuid());
                }
                m_Scene->DestroyEntity(entityUUID);
            }

            if (ImGui::MenuItem(ICON_FA_PEN "  Rename")) {
                auto &name = m_Scene->GetEntityComponent<Hamster::Name>(entityUUID);
                m_ViewportRenameModal = std::make_shared<RenameModal>(name.name);
                m_ViewportRenameModalOpen = true;
            }

            if (ImGui::MenuItem(ICON_FA_CLONE "  Duplicate")) {
                Hamster::UUID newUUID = m_Scene->CreateEntity();

                auto &srcTransform = m_Scene->GetEntityComponent<Hamster::Transform>(entityUUID);
                auto &dstTransform = m_Scene->GetEntityComponent<Hamster::Transform>(newUUID);
                dstTransform.position = srcTransform.position + glm::vec3(10.0f, 10.0f, 0.0f);
                dstTransform.rotation = srcTransform.rotation;
                dstTransform.size = srcTransform.size;

                auto &srcName = m_Scene->GetEntityComponent<Hamster::Name>(entityUUID);
                auto &dstName = m_Scene->GetEntityComponent<Hamster::Name>(newUUID);
                dstName.name = srcName.name + " Copy";

                if (m_Scene->EntityHasComponent<Hamster::Sprite>(entityUUID)) {
                    auto &srcSprite = m_Scene->GetEntityComponent<Hamster::Sprite>(entityUUID);
                    m_Scene->AddEntityComponent<Hamster::Sprite>(newUUID, srcSprite.texture, srcSprite.colour);
                }

                if (m_Scene->EntityHasComponent<Hamster::Rigidbody>(entityUUID)) {
                    auto &srcRb = m_Scene->GetEntityComponent<Hamster::Rigidbody>(entityUUID);
                    Hamster::Rigidbody newRb;
                    newRb.bodyType = srcRb.bodyType;
                    newRb.colliderShape = srcRb.colliderShape;
                    newRb.density = srcRb.density;
                    newRb.friction = srcRb.friction;
                    newRb.restitution = srcRb.restitution;
                    newRb.gravityScale = srcRb.gravityScale;
                    newRb.colliderOffset = srcRb.colliderOffset;
                    newRb.colliderSize = srcRb.colliderSize;
                    m_Scene->AddEntityComponent<Hamster::Rigidbody>(newUUID, newRb);
                }
            }
        }
        ImGui::EndPopup();
    }

    if (m_ViewportRenameModalOpen) {
        ImGui::OpenPopup("Rename Window");
        m_ViewportRenameModalOpen = false;
    }

    if (m_ViewportRenameModal) {
        m_ViewportRenameModal->Render();
    }

    ImGui::End();

    if (m_Hierarchy->IsPanelOpen()) {
        m_Hierarchy->Render();
    }

    if (m_PropertyEditor->IsPanelOpen()) {
        entt::entity e = m_Hierarchy->GetSelectedEntity();

        entt::registry &registry = m_Scene->GetRegistry();

        if (registry.valid(e)) {
            m_PropertyEditor->SetSelectedEntity(m_Scene->GetEntityUUID(e));
        }

        m_PropertyEditor->Render();
    }

    if (m_AssetBrowser->IsPanelOpen()) {
        m_AssetBrowser->Render();
    }

    if (m_Console->IsPanelOpen()) {
        m_Console->Render();
    }

    if (m_ColliderEditor->IsOpen()) {
        m_ColliderEditor->Render();
    }

    m_MenuBar->Render();
}

void EditorLayer::ActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
}

void EditorLayer::FramebufferSizeChanged(Hamster::FramebufferResizeEvent &e) {
    m_ViewportHeight = e.GetHeight();
    m_ViewportWidth = e.GetWidth();
}
