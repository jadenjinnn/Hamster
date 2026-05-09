//
// Created by Jaden on 23/08/2024.
//

#include "EditorLayer.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include <Renderer/Renderer.h>

#include "Core/Application.h"

EditorLayer::EditorLayer(Hamster::Application *app,
                         std::shared_ptr<Hamster::Scene> scene)
    : m_App(app), m_Dispatcher(app->GetEventDispatcher().get()),
      m_Scene(scene), m_FramebufferTexture(1920, 1080) {
    auto *assetManager = app->GetAssetManager();
    m_PropertyEditor = std::make_unique<PropertyEditor>(m_Dispatcher, m_Scene, assetManager);
    m_Hierarchy = std::make_unique<Hierarchy>(m_Dispatcher, m_Scene);
    m_FileBrowser = std::make_unique<FileBrowser>(m_Dispatcher, m_Scene);
    m_AssetBrowser = std::make_unique<AssetBrowser>(m_Dispatcher, m_Scene, assetManager);
    m_StartPauseModal = std::make_unique<StartPauseModal>(m_Dispatcher, m_Scene);
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
        Hamster::Renderer::Clear();
        m_Scene->OnRender(true);

        entt::entity selectedEntity = m_Hierarchy->GetSelectedEntity();

        if (selectedEntity != entt::null) {
            Hamster::Renderer::DrawGuizmo(
                m_Scene->GetRegistry().get<Hamster::Transform>(selectedEntity),
                Hamster::Translate, true);
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

        // Hamster::Renderer::Clear();

        int pickedID =
                Hamster::Application::ColourToId(glm::vec3(data[0], data[1], data[2]));

        // std::cout << (float)data[0] << ", " << (float)data[1] << ", " <<
        // (float)data[2] << std::endl;

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

                // mouseHeldOffsetX = mousePosX;
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
        sceneBackgroundHeld = true;
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
    }

    if (ImGui::IsMouseReleased(1)) {
        sceneBackgroundHeld = false;
    }

    // ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
    //
    // if (ImGui::IsMouseClicked(1) && m_Scene->IsSceneSimulationPaused()) {
    //     std::cout << mouseDelta.x << ", " << mouseDelta.y << std::endl;
    //
    //     Hamster::Renderer::ChangeCameraOffset(mouseDelta.x * m_MouseDragSpeed, mouseDelta.y * m_MouseDragSpeed);
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

        std::cout << ImGui::GetMouseDragDelta().x << std::endl;

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
    } else if (entityHeld) {
        Hamster::Transform *entityTransform =
                &m_Scene->GetRegistry().get<Hamster::Transform>(
                    m_Hierarchy->GetSelectedEntity());

        ImVec2 mouseDragDelta = ImGui::GetMouseDragDelta();

        entityTransform->position.x = mouseHeldTransformX + mouseDragDelta.x;
        entityTransform->position.y = mouseHeldTransformY + mouseDragDelta.y;
    } else if (sceneBackgroundHeld) {
        Hamster::Renderer::ChangeCameraOffset({mouseDelta.x * m_MouseDragSpeed, mouseDelta.y * m_MouseDragSpeed});
    }

    m_FramebufferTexture.ResizeFrameBuffer(m_LevelEditorAvailRegion.x,
                                           m_LevelEditorAvailRegion.y);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, m_LevelEditorAvailRegion.x, m_LevelEditorAvailRegion.y);

    m_FramebufferTexture.Bind();
    glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);

    Hamster::Renderer::Clear();
    m_Scene->OnRender(false);

    entt::entity selectedEntity = m_Hierarchy->GetSelectedEntity();

    if (selectedEntity != entt::null) {
        Hamster::Renderer::DrawGuizmo(
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


    if (!ImGui::Begin("Level Editor", &m_WindowOpen)) {
        ImGui::End();
        return;
    }

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

            Hamster::Renderer::AdjustZoom(mouseWheelDelta * m_MouseWheelZoomSpeed, mousePosX, mousePosY);
        }
    }

    const ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::GetWindowDrawList()->AddImage(
        (void *) (intptr_t) m_FramebufferTexture.GetTextureID(),
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + m_LevelEditorAvailRegion.x,
               pos.y + m_LevelEditorAvailRegion.y),
        ImVec2(0, 1), ImVec2(1, 0));

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

    if (m_FileBrowser->IsPanelOpen()) {
        m_FileBrowser->Render();
    }

    if (m_AssetBrowser->IsPanelOpen()) {
        m_AssetBrowser->Render();
    }

    if (m_Console->IsPanelOpen()) {
        m_Console->Render();
    }

    m_StartPauseModal->Render();
    m_MenuBar->Render();
}

void EditorLayer::ActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
}

void EditorLayer::FramebufferSizeChanged(Hamster::FramebufferResizeEvent &e) {
    m_ViewportHeight = e.GetHeight();
    m_ViewportWidth = e.GetWidth();
}
