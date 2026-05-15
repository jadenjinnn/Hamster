#include "AnimationPanel.h"

#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>

#include <Utils/AssetManager.h>
#include "Theme/IconsFontAwesome6.h"
#include "Theme/HamsterTheme.h"

AnimationPanel::AnimationPanel(Hamster::EventDispatcher *dispatcher,
                               std::shared_ptr<Hamster::Scene> scene,
                               Hamster::AssetManager *assetManager)
    : Hamster::Panel(dispatcher, scene, true), m_AssetManager(assetManager) {}

void AnimationPanel::Render() {
  if (!ImGui::Begin("Animation", &m_WindowOpen)) {
    ImGui::End();
    return;
  }

  RenderToolbar();
  ImGui::Separator();

  if (!Hamster::UUID::IsNil(m_CurrentAnimUUID) || !m_AnimName.empty()) {
    RenderTimeline();
    ImGui::Separator();
    RenderKeyframeList();
  } else {
    ImGui::Dummy({0, 20});
    float textWidth = ImGui::CalcTextSize("Create or load an animation to begin").x;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - textWidth) * 0.5f +
                         ImGui::GetCursorPosX());
    ImGui::TextDisabled("Create or load an animation to begin");
  }

  if (m_Playing) {
    UpdatePreview();
  }

  ImGui::End();
}

void AnimationPanel::RenderToolbar() {
  if (ImGui::Button(ICON_FA_PLUS "  New")) {
    m_CurrentAnimUUID = Hamster::UUID::GetNil();
    m_AnimName = "New Animation";
    strncpy(m_NameBuffer, m_AnimName.c_str(), sizeof(m_NameBuffer) - 1);
    m_NameBuffer[sizeof(m_NameBuffer) - 1] = '\0';
    m_Keyframes.clear();
    m_Duration = 0.0f;
    m_Playing = false;
    m_PlaybackTime = 0.0f;
    m_SelectedKeyframe = -1;
    m_Dirty = true;
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save")) {
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
          std::filesystem::path("Animations") /
          (m_AnimName + ".hanim");
      std::filesystem::create_directories(savePath.parent_path());
      m_AssetManager->SaveAnimationFile(m_CurrentAnimUUID, savePath);
      m_Dirty = false;
    }
  }

  ImGui::SameLine();

  if (ImGui::Button(ICON_FA_FILM "  Load")) {
    ImGui::OpenPopup("Load Animation");
  }

  if (ImGui::BeginPopup("Load Animation")) {
    for (auto &[uuid, animData] : m_AssetManager->GetAnimationMap()) {
      if (ImGui::Selectable(animData->name.c_str())) {
        m_CurrentAnimUUID = uuid;
        m_AnimName = animData->name;
        strncpy(m_NameBuffer, m_AnimName.c_str(), sizeof(m_NameBuffer) - 1);
        m_NameBuffer[sizeof(m_NameBuffer) - 1] = '\0';
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
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImFont *bf = HamsterTheme::GetBoldFont()) ImGui::PushFont(bf);
    ImGui::Text("Name");
    if (HamsterTheme::GetBoldFont()) ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##animname", m_NameBuffer, sizeof(m_NameBuffer))) {
      m_AnimName = m_NameBuffer;
      m_Dirty = true;
    }

    if (m_Dirty) {
      ImGui::SameLine();
      ImGui::TextDisabled("(unsaved)");
    }
  }
}

void AnimationPanel::RenderTimeline() {
  ImGui::Dummy({0, 4});

  // Playback controls
  if (m_Playing) {
    if (ImGui::Button(ICON_FA_STOP "  Stop")) {
      m_Playing = false;
      m_PlaybackTime = 0.0f;
    }
  } else {
    bool canPlay = !m_Keyframes.empty();
    if (!canPlay) ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_PLAY "  Play")) {
      m_Playing = true;
      m_PlaybackTime = 0.0f;
      m_LastFrameTime = static_cast<float>(glfwGetTime());
    }
    if (!canPlay) ImGui::EndDisabled();
  }

  ImGui::SameLine();
  ImGui::Text("%.2fs / %.2fs", m_PlaybackTime, m_Duration);

  ImGui::SameLine();
  ImGui::Text("  Keyframes: %d", static_cast<int>(m_Keyframes.size()));

  // Timeline bar
  ImGui::Dummy({0, 4});

  float timelineWidth = ImGui::GetContentRegionAvail().x;
  float timelineHeight = 40.0f;
  ImVec2 timelinePos = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  // Background
  dl->AddRectFilled(timelinePos,
                    {timelinePos.x + timelineWidth, timelinePos.y + timelineHeight},
                    IM_COL32(30, 30, 34, 255), 4.0f);

  // Border
  dl->AddRect(timelinePos,
              {timelinePos.x + timelineWidth, timelinePos.y + timelineHeight},
              IM_COL32(60, 60, 65, 255), 4.0f);

  if (m_Duration > 0.0f) {
    // Time markers
    float markerInterval = 0.1f;
    if (m_Duration > 2.0f) markerInterval = 0.5f;
    if (m_Duration > 5.0f) markerInterval = 1.0f;

    for (float t = 0.0f; t <= m_Duration; t += markerInterval) {
      float x = timelinePos.x + (t / m_Duration) * timelineWidth;
      dl->AddLine({x, timelinePos.y + timelineHeight - 8},
                  {x, timelinePos.y + timelineHeight},
                  IM_COL32(80, 80, 85, 255));
    }

    // Keyframe markers
    for (int i = 0; i < static_cast<int>(m_Keyframes.size()); i++) {
      float t = m_Keyframes[i].time;
      float x = timelinePos.x + (t / m_Duration) * timelineWidth;
      float markerHalf = 6.0f;

      ImU32 color = (i == m_SelectedKeyframe) ? IM_COL32(88, 135, 247, 255)
                                               : IM_COL32(200, 200, 210, 255);

      // Diamond shape
      dl->AddQuadFilled({x, timelinePos.y + timelineHeight * 0.5f - markerHalf},
                        {x + markerHalf, timelinePos.y + timelineHeight * 0.5f},
                        {x, timelinePos.y + timelineHeight * 0.5f + markerHalf},
                        {x - markerHalf, timelinePos.y + timelineHeight * 0.5f},
                        color);
    }

    // Playhead
    if (m_Playing) {
      float px = timelinePos.x + (m_PlaybackTime / m_Duration) * timelineWidth;
      dl->AddLine({px, timelinePos.y}, {px, timelinePos.y + timelineHeight},
                  IM_COL32(255, 80, 80, 255), 2.0f);
    }
  }

  // Click to select / drag keyframe diamonds
  ImGui::InvisibleButton("##timeline", {timelineWidth, timelineHeight});

  if (ImGui::IsItemClicked() && m_Duration > 0.0f) {
    float mouseX = ImGui::GetMousePos().x - timelinePos.x;
    float clickTime = (mouseX / timelineWidth) * m_Duration;

    float bestDist = 999.0f;
    int bestIdx = -1;
    for (int i = 0; i < static_cast<int>(m_Keyframes.size()); i++) {
      float dist = std::abs(m_Keyframes[i].time - clickTime);
      if (dist < bestDist) {
        bestDist = dist;
        bestIdx = i;
      }
    }

    float pixelThreshold = 15.0f;
    float timeThreshold = (pixelThreshold / timelineWidth) * m_Duration;
    if (bestDist <= timeThreshold) {
      m_SelectedKeyframe = bestIdx;
      m_DraggingKeyframe = true;
    } else {
      m_SelectedKeyframe = -1;
      m_DraggingKeyframe = false;
    }
  }

  if (m_DraggingKeyframe && ImGui::IsItemActive() &&
      m_SelectedKeyframe >= 0 && m_Duration > 0.0f) {
    float mouseX = ImGui::GetMousePos().x - timelinePos.x;
    float newTime = (mouseX / timelineWidth) * m_Duration;
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

  // Preview sprite
  if (!m_Keyframes.empty() && m_Duration > 0.0f) {
    float previewTime = m_Playing ? m_PlaybackTime : 0.0f;
    if (m_SelectedKeyframe >= 0) {
      previewTime = m_Keyframes[m_SelectedKeyframe].time;
    }

    const Hamster::AnimationKeyframe *current = &m_Keyframes[0];
    for (auto &kf : m_Keyframes) {
      if (kf.time <= previewTime)
        current = &kf;
      else
        break;
    }

    try {
      auto tex = m_AssetManager->GetTexture(current->textureUUID);
      if (tex && tex->GetTextureId() != 0) {
        ImGui::Dummy({0, 4});
        constexpr float previewSize = 64.0f;
        ImGui::Image((ImTextureID)(intptr_t)tex->GetTextureId(),
                     {previewSize, previewSize});
        ImGui::SameLine();
        ImGui::Text("%s", tex->GetName().c_str());
      }
    } catch (...) {}
  }
}

void AnimationPanel::RenderKeyframeList() {
  ImGui::Dummy({0, 4});

  if (ImFont *bf = HamsterTheme::GetBoldFont()) ImGui::PushFont(bf);
  ImGui::Text("Keyframes");
  if (HamsterTheme::GetBoldFont()) ImGui::PopFont();

  ImGui::SameLine();

  if (ImGui::SmallButton(ICON_FA_PLUS "##addkf")) {
    ImGui::OpenPopup("Add Keyframe Texture");
  }

  if (ImGui::BeginPopup("Add Keyframe Texture")) {
    for (auto &[uuid, texture] : m_AssetManager->GetTextureMap()) {
      Hamster::UUID mutableUUID = uuid;
      std::string label = texture->GetName() + "##" + mutableUUID.GetUUIDString();
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

  if (m_SelectedKeyframe >= 0) {
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_TRASH "##rmkf")) {
      m_Keyframes.erase(m_Keyframes.begin() + m_SelectedKeyframe);
      m_SelectedKeyframe = -1;
      m_Duration = m_Keyframes.empty() ? 0.0f : m_Keyframes.back().time;
      m_Dirty = true;
    }
  }

  ImGui::Dummy({0, 4});

  for (int i = 0; i < static_cast<int>(m_Keyframes.size()); i++) {
    ImGui::PushID(i);

    // Build label for the selectable
    std::string label;
    try {
      auto tex = m_AssetManager->GetTexture(m_Keyframes[i].textureUUID);
      label = "#" + std::to_string(i) + "  " + tex->GetName();
    } catch (...) {
      label = "#" + std::to_string(i) + "  (missing texture)";
    }

    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "  %.3fs", m_Keyframes[i].time);
    label += timeStr;

    bool selected = (i == m_SelectedKeyframe);
    ImGui::Selectable(label.c_str(), selected);

    if (ImGui::IsItemClicked()) {
      m_SelectedKeyframe = i;
    }

    // Reorder: when dragged over a neighbor, swap
    if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
      int n_next = i + (ImGui::GetMouseDragDelta(0).y < 0.0f ? -1 : 1);
      if (n_next >= 0 && n_next < static_cast<int>(m_Keyframes.size())) {
        std::swap(m_Keyframes[i].textureUUID, m_Keyframes[n_next].textureUUID);
        if (m_SelectedKeyframe == i)
          m_SelectedKeyframe = n_next;
        else if (m_SelectedKeyframe == n_next)
          m_SelectedKeyframe = i;
        m_Dirty = true;
        ImGui::ResetMouseDragDelta();
      }
    }

    ImGui::PopID();
  }

  // Time editor for selected keyframe
  if (m_SelectedKeyframe >= 0 && m_SelectedKeyframe < static_cast<int>(m_Keyframes.size())) {
    ImGui::Dummy({0, 4});
    if (ImFont *bf = HamsterTheme::GetBoldFont()) ImGui::PushFont(bf);
    ImGui::Text("Time");
    if (HamsterTheme::GetBoldFont()) ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat("##kftime", &m_Keyframes[m_SelectedKeyframe].time,
                         0.01f, 0.0f, 60.0f, "%.3fs")) {
      m_Duration = 0.0f;
      for (auto &kf : m_Keyframes)
        if (kf.time > m_Duration) m_Duration = kf.time;
      m_Dirty = true;
    }
  }
}

void AnimationPanel::UpdatePreview() {
  float currentTime = static_cast<float>(glfwGetTime());
  float dt = currentTime - m_LastFrameTime;
  m_LastFrameTime = currentTime;

  m_PlaybackTime += dt;
  if (m_PlaybackTime >= m_Duration) {
    m_PlaybackTime = 0.0f;
  }
}
