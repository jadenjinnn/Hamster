#include "AnimationPanel.h"
#include "../Theme.h"
#include "../Components/Components.h"
#include "IconsFontAwesome6.h"

#include <Utils/AssetManager.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

AnimationPanel::AnimationPanel(Hamster::EventDispatcher *dispatcher,
                               std::shared_ptr<Hamster::Scene> scene,
                               Hamster::AssetManager *assetManager)
    : m_Dispatcher(dispatcher), m_Scene(std::move(scene)),
      m_AssetManager(assetManager) {
    m_Dispatcher->Subscribe(
        Hamster::ActiveSceneChanged,
        FORWARD_CALLBACK_FUNCTION(AnimationPanel::OnActiveSceneChanged,
                                  Hamster::ActiveSceneChangedEvent));
}

void AnimationPanel::OnActiveSceneChanged(Hamster::ActiveSceneChangedEvent &e) {
    m_Scene = e.GetActiveScene();
}

void AnimationPanel::Render() {
    ImDrawList *dl = ImGui::GetWindowDrawList();

    ImGui::Dummy({0, 4});

    // Toolbar: New / Save / Load
    if (HToolbarButton(ICON_FA_PLUS "  New")) {
        m_CurrentAnimUUID = Hamster::UUID::GetNil();
        m_AnimName = "New Animation";
        std::strncpy(m_NameBuffer, m_AnimName.c_str(), sizeof(m_NameBuffer) - 1);
        m_Keyframes.clear();
        m_Duration = 0.0f;
        m_Playing = false;
        m_PlaybackTime = 0.0f;
        m_SelectedKeyframe = -1;
        m_Dirty = true;
    }
    ImGui::SameLine(0, 6);
    if (HToolbarButton(ICON_FA_FLOPPY_DISK "  Save")) {
        if (!m_AnimName.empty()) {
            m_Duration = m_Keyframes.empty() ? 0.0f : m_Keyframes.back().time;
            if (Hamster::UUID::IsNil(m_CurrentAnimUUID)) {
                m_CurrentAnimUUID = m_AssetManager->AddAnimation(m_AnimName, m_Keyframes);
            } else {
                Hamster::AnimationData data;
                data.name = m_AnimName;
                data.keyframes = m_Keyframes;
                data.duration = m_Duration;
                m_AssetManager->AddAnimation(m_CurrentAnimUUID, data);
            }
            std::filesystem::path savePath =
                std::filesystem::path("Animations") / (m_AnimName + ".hanim");
            std::filesystem::create_directories(savePath.parent_path());
            m_AssetManager->SaveAnimationFile(m_CurrentAnimUUID, savePath);
            m_Dirty = false;
        }
    }
    ImGui::SameLine(0, 6);
    if (HToolbarButton(ICON_FA_FILM "  Load")) {
        ImGui::OpenPopup("Load Animation");
    }
    if (ImGui::BeginPopup("Load Animation")) {
        for (auto &[uuid, animData] : m_AssetManager->GetAnimationMap()) {
            if (ImGui::Selectable(animData->name.c_str())) {
                m_CurrentAnimUUID = uuid;
                m_AnimName = animData->name;
                std::strncpy(m_NameBuffer, m_AnimName.c_str(), sizeof(m_NameBuffer) - 1);
                m_Keyframes = animData->keyframes;
                m_Duration = animData->duration;
                m_Playing = false;
                m_PlaybackTime = 0.0f;
                m_SelectedKeyframe = -1;
                m_Dirty = false;
            }
        }
        if (m_AssetManager->GetAnimationMap().empty()) {
            ImGui::TextDisabled("No animations in project");
        }
        ImGui::EndPopup();
    }

    if (!m_AnimName.empty()) {
        ImGui::SameLine(0, 16);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 16);

        if (g_BoldFont) ImGui::PushFont(g_BoldFont);
        ImGui::Text("Name");
        if (g_BoldFont) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputText("##animname2", m_NameBuffer, sizeof(m_NameBuffer))) {
            m_AnimName = m_NameBuffer;
            m_Dirty = true;
        }
        if (m_Dirty) {
            ImGui::SameLine();
            ImGui::TextDisabled("(unsaved)");
        }
    }

    ImGui::Dummy({0, 2});

    if (Hamster::UUID::IsNil(m_CurrentAnimUUID) && m_AnimName.empty()) {
        ImGui::Dummy({0, 20});
        ImGui::TextDisabled("Create or load an animation to begin");
        return;
    }

    // Preview + right column (playback + timeline)
    float previewSize = 100.0f;
    float gap = 12.0f;
    float rightX = previewSize + gap;
    float rightW = ImGui::GetContentRegionAvail().x - kScrollGap - rightX;
    float timelineH = 40.0f;

    ImVec2 prevPos = ImGui::GetCursorScreenPos();

    // Preview: checkerboard + current keyframe texture
    dl->AddRectFilled(prevPos, {prevPos.x + previewSize, prevPos.y + previewSize},
                      ImGui::ColorConvertFloat4ToU32(kSurface), 4.0f);
    float checkSz = 8.0f;
    for (int cy = 0; cy < static_cast<int>(previewSize / checkSz); cy++) {
        for (int cx = 0; cx < static_cast<int>(previewSize / checkSz); cx++) {
            if ((cx + cy) % 2 == 0) {
                float x0 = prevPos.x + cx * checkSz;
                float y0 = prevPos.y + cy * checkSz;
                dl->AddRectFilled({x0, y0}, {x0 + checkSz, y0 + checkSz},
                                  IM_COL32(50, 50, 55, 255));
            }
        }
    }
    dl->AddRect(prevPos, {prevPos.x + previewSize, prevPos.y + previewSize},
                ImGui::ColorConvertFloat4ToU32(kBorder), 4.0f);

    // Preview content
    if (!m_Keyframes.empty() && m_Duration > 0.0f) {
        float previewTime = m_Playing ? m_PlaybackTime : 0.0f;
        if (m_SelectedKeyframe >= 0 &&
            m_SelectedKeyframe < static_cast<int>(m_Keyframes.size())) {
            previewTime = m_Keyframes[m_SelectedKeyframe].time;
        }
        const Hamster::AnimationKeyframe *current = &m_Keyframes[0];
        for (auto &kf : m_Keyframes) {
            if (kf.time <= previewTime) current = &kf;
            else break;
        }
        try {
            auto tex = m_AssetManager->GetTexture(current->textureUUID);
            if (tex && tex->GetTextureId() != 0) {
                dl->AddImage(reinterpret_cast<ImTextureID>(
                                 static_cast<intptr_t>(tex->GetTextureId())),
                             prevPos,
                             {prevPos.x + previewSize, prevPos.y + previewSize});
            }
        } catch (...) {}
    } else if (g_IconLarge) {
        const char *ic = ICON_FA_IMAGE;
        ImVec2 iSz = g_IconLarge->CalcTextSizeA(36.0f, FLT_MAX, 0, ic);
        dl->AddText(g_IconLarge, 36.0f,
                    {prevPos.x + (previewSize - iSz.x) * 0.5f,
                     prevPos.y + (previewSize - iSz.y) * 0.5f},
                    IM_COL32(100, 108, 125, 180), ic);
    }

    // Playback controls
    ImGui::SetCursorScreenPos({prevPos.x + rightX, prevPos.y});
    if (m_Playing) {
        if (HToolbarButton(ICON_FA_STOP "  Stop")) {
            m_Playing = false;
            m_PlaybackTime = 0.0f;
        }
    } else {
        bool canPlay = !m_Keyframes.empty();
        if (!canPlay) ImGui::BeginDisabled();
        if (HToolbarButton(ICON_FA_PLAY "  Play")) {
            m_Playing = true;
            m_PlaybackTime = 0.0f;
            m_LastFrameTime = static_cast<float>(glfwGetTime());
        }
        if (!canPlay) ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::TextColored(kTextDim, "%.2fs / %.2fs", m_PlaybackTime, m_Duration);
    ImGui::SameLine(0, 24);
    ImGui::TextColored(kTextDim, "Keyframes: %d",
                       static_cast<int>(m_Keyframes.size()));
    ImGui::SameLine(0, 24);
    if (HToolbarButton(ICON_FA_PLUS "  Add Keyframe")) {
        ImGui::OpenPopup("Add Keyframe");
    }
    if (ImGui::BeginPopup("Add Keyframe")) {
        for (auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
            Hamster::UUID mUUID = uuid;
            std::string label = texture->GetName() + "##" + mUUID.GetUUIDString();
            if (ImGui::Selectable(label.c_str())) {
                Hamster::AnimationKeyframe kf;
                kf.textureUUID = uuid;
                kf.time = m_Keyframes.empty() ? 0.0f : m_Keyframes.back().time + 0.1f;
                m_Keyframes.push_back(kf);
                m_Duration = m_Keyframes.back().time;
                m_SelectedKeyframe = static_cast<int>(m_Keyframes.size()) - 1;
                m_Dirty = true;
            }
        }
        if (m_AssetManager->GetTextureMap().empty()) {
            ImGui::TextDisabled("No textures in project");
        }
        ImGui::EndPopup();
    }

    // Timeline bar
    float tlY = ImGui::GetCursorScreenPos().y + 4;
    ImVec2 tlPos = {prevPos.x + rightX, tlY};

    dl->AddRectFilled(tlPos, {tlPos.x + rightW, tlPos.y + timelineH},
                      ImGui::ColorConvertFloat4ToU32(kSurface), 4.0f);
    dl->AddRect(tlPos, {tlPos.x + rightW, tlPos.y + timelineH},
                ImGui::ColorConvertFloat4ToU32(kBorder), 4.0f);

    if (m_Duration > 0.0f) {
        float interval = (m_Duration > 5.0f) ? 1.0f : (m_Duration > 2.0f ? 0.5f : 0.1f);
        for (float t = 0.0f; t <= m_Duration; t += interval) {
            float x = tlPos.x + (t / m_Duration) * rightW;
            dl->AddLine({x, tlPos.y + timelineH - 8},
                        {x, tlPos.y + timelineH},
                        IM_COL32(80, 80, 85, 255));
        }
        for (int i = 0; i < static_cast<int>(m_Keyframes.size()); i++) {
            float x = tlPos.x + (m_Keyframes[i].time / m_Duration) * rightW;
            float cy = tlPos.y + timelineH * 0.5f;
            float half = 6.0f;
            ImU32 col = (i == m_SelectedKeyframe)
                            ? ImGui::ColorConvertFloat4ToU32(kAccent)
                            : IM_COL32(200, 200, 210, 255);
            dl->AddQuadFilled({x, cy - half}, {x + half, cy},
                              {x, cy + half}, {x - half, cy}, col);
        }
        if (m_Playing) {
            float px = tlPos.x + (m_PlaybackTime / m_Duration) * rightW;
            dl->AddLine({px, tlPos.y}, {px, tlPos.y + timelineH},
                        IM_COL32(255, 80, 80, 255), 2.0f);
        }
    }

    // Click to select / drag keyframes
    ImGui::SetCursorScreenPos({tlPos.x, tlPos.y});
    ImGui::InvisibleButton("##tlbtn", {rightW, timelineH});
    if (ImGui::IsItemClicked() && m_Duration > 0.0f) {
        float mouseX = ImGui::GetMousePos().x - tlPos.x;
        float clickTime = (mouseX / rightW) * m_Duration;

        float bestDist = 999.0f;
        int bestIdx = -1;
        for (int i = 0; i < static_cast<int>(m_Keyframes.size()); i++) {
            float dist = std::abs(m_Keyframes[i].time - clickTime);
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
        float pixelThresh = 15.0f;
        float timeThresh = (pixelThresh / rightW) * m_Duration;
        if (bestDist <= timeThresh) {
            m_SelectedKeyframe = bestIdx;
            m_DraggingKeyframe = true;
        } else {
            m_SelectedKeyframe = -1;
            m_DraggingKeyframe = false;
        }
    }
    if (m_DraggingKeyframe && ImGui::IsItemActive() &&
        m_SelectedKeyframe >= 0 && m_Duration > 0.0f) {
        float mouseX = ImGui::GetMousePos().x - tlPos.x;
        float newTime = (mouseX / rightW) * m_Duration;
        if (newTime < 0.0f) newTime = 0.0f;
        if (newTime > 60.0f) newTime = 60.0f;
        m_Keyframes[m_SelectedKeyframe].time = newTime;
        m_Duration = 0.0f;
        for (auto &kf : m_Keyframes)
            if (kf.time > m_Duration) m_Duration = kf.time;
        m_Dirty = true;
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_DraggingKeyframe = false;
    }

    ImGui::SetCursorScreenPos({prevPos.x, prevPos.y + previewSize});
    ImGui::Dummy({0, 16});

    if (m_Playing) UpdatePreview();
}

void AnimationPanel::UpdatePreview() {
    float currentTime = static_cast<float>(glfwGetTime());
    float dt = currentTime - m_LastFrameTime;
    m_LastFrameTime = currentTime;

    m_PlaybackTime += dt;
    if (m_PlaybackTime >= m_Duration) m_PlaybackTime = 0.0f;
}
