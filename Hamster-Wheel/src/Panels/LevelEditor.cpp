#include "LevelEditor.h"
#include "../Panel.h"
#include "../Theme.h"
#include "IconsFontAwesome6.h"

#include <Renderer/Renderer.h>
#include <Core/Scene.h>

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <GLFW/glfw3.h>

static Panel g_LevelPanel = {"Level Editor", "...##le"};

void LevelEditor::Render(unsigned int fbTexId,
                         Hamster::Renderer *renderer,
                         Hamster::Scene *scene,
                         ImVec2 &outViewportTL,
                         ImVec2 &outAvailSize) {
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();
    ImDrawList *dl = ImGui::GetWindowDrawList();

    g_LevelPanel.DrawHeader();

    ImVec2 vpPos  = {wp.x, wp.y + kHeaderH};
    ImVec2 vpSize = {ws.x, ws.y - kHeaderH};

    outViewportTL = vpPos;
    outAvailSize  = vpSize;

    // Canvas background (visible briefly during resize)
    dl->AddRectFilled(vpPos, {vpPos.x + vpSize.x, vpPos.y + vpSize.y},
                      ImGui::ColorConvertFloat4ToU32(kCanvas),
                      0, ImDrawFlags_RoundCornersBottom);

    // FBO blit
    if (fbTexId != 0) {
        dl->AddImage(
            reinterpret_cast<ImTextureID>(static_cast<intptr_t>(fbTexId)),
            vpPos,
            {vpPos.x + vpSize.x, vpPos.y + vpSize.y},
            {0, 1}, {1, 0});
    }

    // ── Play / Pause / Stop overlay (top-centre) ──
    if (scene) {
        bool paused = scene->IsSceneSimulationPaused();
        const float btnSz = 28.0f;
        const float spacing = 6.0f;
        const float totalW = btnSz * 3 + spacing * 2;
        const float ox = vpPos.x + (vpSize.x - totalW) * 0.5f;
        const float oy = vpPos.y + 8.0f;

        ImGui::SetCursorScreenPos({ox, oy});
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4, 4});

        ImVec4 green     = {0.337f, 0.576f, 0.439f, 1.0f};
        ImVec4 greenHov  = {0.400f, 0.660f, 0.510f, 1.0f};
        ImVec4 greenAct  = {0.260f, 0.490f, 0.360f, 1.0f};
        ImVec4 red       = {0.878f, 0.290f, 0.310f, 1.0f};
        ImVec4 redHov    = {0.920f, 0.360f, 0.380f, 1.0f};
        ImVec4 redAct    = {0.780f, 0.220f, 0.240f, 1.0f};

        ImGui::PushStyleColor(ImGuiCol_Button,        green);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, greenHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  greenAct);
        if (paused) {
            if (ImGui::Button(ICON_FA_PLAY "##play", {btnSz, btnSz})) {
                scene->RunSceneSimulation();
            }
        } else {
            if (ImGui::Button(ICON_FA_PAUSE "##pause", {btnSz, btnSz})) {
                scene->PauseSceneSimulation();
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0, spacing);
        ImGui::PushStyleColor(ImGuiCol_Button,        red);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, redHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  redAct);
        if (ImGui::Button(ICON_FA_STOP "##stop", {btnSz, btnSz})) {
            scene->PauseSceneSimulation();
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar(2);
    }

    // ── FPS counter (top-right, during simulation) ──
    if (scene && scene->IsSceneRunning()) {
        char fpsBuf[32];
        std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", ImGui::GetIO().Framerate);
        ImVec2 textSize = ImGui::CalcTextSize(fpsBuf);
        float fpsX = vpPos.x + vpSize.x - textSize.x - 12.0f;
        float fpsY = vpPos.y + 12.0f;
        dl->AddRectFilled({fpsX - 6, fpsY - 3},
                          {fpsX + textSize.x + 6, fpsY + textSize.y + 3},
                          IM_COL32(0, 0, 0, 140), 4.0f);
        dl->AddText({fpsX, fpsY}, IM_COL32(255, 255, 255, 220), fpsBuf);
    }

    // ── Zoom slider (bottom-right) ──
    float zoomTotalW = 0, zoomBx = 0, zoomBy = 0;
    if (renderer) {
        const float margin = 20.0f;
        const float trackW = 70.0f;
        const float trackH = 3.0f;
        const float dotR   = 5.0f;
        const float zoomMin = 0.1f;
        const float zoomMax = 5.0f;

        float zoom = renderer->GetZoom();
        char zoomBuf[16];
        std::snprintf(zoomBuf, sizeof(zoomBuf), "%.0f%%", zoom * 100.0f);
        ImVec2 zoomSz = ImGui::CalcTextSize(zoomBuf);

        const char *icon = ICON_FA_MAGNIFYING_GLASS;
        ImVec2 iSz = ImGui::CalcTextSize(icon);

        zoomTotalW = iSz.x + 6 + trackW + 8 + zoomSz.x;
        float padH = 8.0f, padV = 8.0f;
        float boxW = zoomTotalW + padH * 2;
        float boxH = std::max(iSz.y, zoomSz.y) + padV * 2;

        ImVec2 boxP0 = {vpPos.x + vpSize.x - margin - boxW,
                        vpPos.y + vpSize.y - margin - boxH};
        ImVec2 boxP1 = {boxP0.x + boxW, boxP0.y + boxH};
        dl->AddRectFilled(boxP0, boxP1,
                          ImGui::ColorConvertFloat4ToU32(kHeader), 6.0f);
        dl->AddRect(boxP0, boxP1,
                    ImGui::ColorConvertFloat4ToU32(kBorder), 6.0f);

        zoomBx = boxP0.x + padH;
        zoomBy = boxP0.y + boxH * 0.5f;

        dl->AddText({zoomBx, zoomBy - iSz.y * 0.5f},
                    IM_COL32(255, 255, 255, 140), icon);

        float tx = zoomBx + iSz.x + 6;
        dl->AddRectFilled({tx, zoomBy - trackH / 2},
                          {tx + trackW, zoomBy + trackH / 2},
                          IM_COL32(255, 255, 255, 50), trackH);

        float t = (zoom - zoomMin) / (zoomMax - zoomMin);
        float dotX = tx + t * trackW;

        ImVec2 mp = ImGui::GetMousePos();
        bool hovered = mp.x >= tx - dotR && mp.x <= tx + trackW + dotR &&
                       mp.y >= zoomBy - dotR * 2 && mp.y <= zoomBy + dotR * 2;
        if (hovered && ImGui::IsMouseClicked(0)) m_ZoomDragging = true;
        if (ImGui::IsMouseReleased(0))           m_ZoomDragging = false;
        if (m_ZoomDragging) {
            float newT = (mp.x - tx) / trackW;
            newT = std::clamp(newT, 0.0f, 1.0f);
            float newZoom = zoomMin + newT * (zoomMax - zoomMin);
            float cx = vpSize.x * 0.5f;
            float cy = vpSize.y * 0.5f;
            renderer->AdjustZoom(newZoom - zoom, cx, cy);
        }

        ImU32 dotCol = m_ZoomDragging ? IM_COL32(255, 255, 255, 255)
                                       : IM_COL32(200, 200, 200, 220);
        dl->AddCircleFilled({dotX, zoomBy}, dotR, dotCol);
        dl->AddText({tx + trackW + 8, zoomBy - zoomSz.y * 0.5f},
                    IM_COL32(255, 255, 255, 180), zoomBuf);
    }

    // ── 2-axis gizmo (bottom-right, above zoom) ──
    if (renderer) {
        const float arm   = 35.0f;
        const float arSz  = 7.0f;
        const float gizmoW = arm + arSz + 12;
        float gx = zoomBx + (zoomTotalW - gizmoW) * 0.5f + arm * 0.1f;
        float gy = zoomBy - 30.0f;

        ImU32 xC = m_AxisDraggingX ? IM_COL32(255, 120, 120, 255)
                                    : IM_COL32(220, 70, 70, 230);
        ImU32 yC = m_AxisDraggingY ? IM_COL32(120, 255, 120, 255)
                                    : IM_COL32(70, 200, 70, 230);

        dl->AddLine({gx, gy}, {gx + arm, gy}, xC, 2.0f);
        dl->AddTriangleFilled({gx + arm, gy - arSz * 0.5f},
                              {gx + arm, gy + arSz * 0.5f},
                              {gx + arm + arSz, gy}, xC);
        dl->AddText({gx + arm + arSz + 2, gy - 6}, xC, "X");

        dl->AddLine({gx, gy}, {gx, gy - arm}, yC, 2.0f);
        dl->AddTriangleFilled({gx - arSz * 0.5f, gy - arm},
                              {gx + arSz * 0.5f, gy - arm},
                              {gx, gy - arm - arSz}, yC);
        dl->AddText({gx - 4, gy - arm - arSz - 14}, yC, "Y");

        ImVec2 mp = ImGui::GetMousePos();
        ImVec2 md = ImGui::GetIO().MouseDelta;

        float xDistY = std::abs(mp.y - gy);
        bool xHov = mp.x >= gx - 6.0f && mp.x <= gx + arm + arSz + 4.0f && xDistY < 10.0f;

        float yDistX = std::abs(mp.x - gx);
        bool yHov = mp.y >= gy - arm - arSz - 4.0f && mp.y <= gy + 6.0f && yDistX < 10.0f;

        if (ImGui::IsMouseClicked(0)) {
            if (xHov)      m_AxisDraggingX = true;
            else if (yHov) m_AxisDraggingY = true;
        }
        if (ImGui::IsMouseReleased(0)) {
            m_AxisDraggingX = false;
            m_AxisDraggingY = false;
        }
        if (m_AxisDraggingX) renderer->ChangeCameraOffset({md.x, 0.0f});
        if (m_AxisDraggingY) renderer->ChangeCameraOffset({0.0f, md.y});
    }
}
