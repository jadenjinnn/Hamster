#include "ColliderEditor.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

#include "Theme/HamsterTheme.h"
#include "Theme/IconsFontAwesome6.h"

static constexpr float kHandleSize = 8.0f;
static constexpr float kHandleHitSize = 14.0f;
static constexpr ImU32 kColliderColor = IM_COL32(0, 200, 80, 255);
static constexpr ImU32 kColliderFill  = IM_COL32(0, 200, 80, 30);
static constexpr ImU32 kHandleColor   = IM_COL32(255, 255, 255, 255);
static constexpr ImU32 kHandleBorder  = IM_COL32(0, 200, 80, 255);
static constexpr ImU32 kRefRectColor  = IM_COL32(120, 120, 120, 180);

void ColliderEditor::Open(Hamster::UUID entityUUID,
                          std::shared_ptr<Hamster::Scene> scene) {
  m_EntityUUID = entityUUID;
  m_Scene = scene;
  m_Open = true;
  m_DraggingBody = false;
  m_DraggingHandle = -1;
}

void ColliderEditor::Close() {
  m_Open = false;
  m_DraggingBody = false;
  m_DraggingHandle = -1;
}

void ColliderEditor::Render() {
  if (!m_Open)
    return;

  if (Hamster::UUID::IsNil(m_EntityUUID) || !m_Scene)
    return;

  if (!m_Scene->EntityHasComponent<Hamster::Rigidbody>(m_EntityUUID)) {
    Close();
    return;
  }

  auto &rb = m_Scene->GetEntityComponent<Hamster::Rigidbody>(m_EntityUUID);
  auto &transform =
      m_Scene->GetEntityComponent<Hamster::Transform>(m_EntityUUID);

  bool hasSprite =
      m_Scene->EntityHasComponent<Hamster::Sprite>(m_EntityUUID);
  Hamster::Sprite *sprite = hasSprite
      ? &m_Scene->GetEntityComponent<Hamster::Sprite>(m_EntityUUID)
      : nullptr;

  ImGui::SetNextWindowSize({460, 460}, ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Collider Editor", &m_Open)) {
    ImGui::End();
    return;
  }

  // Reset button
  if (ImGui::Button("Reset to Transform Size")) {
    rb.colliderOffset = {0.0f, 0.0f};
    rb.colliderSize = {0.0f, 0.0f};
  }

  ImGui::SameLine();

  // Show current values
  glm::vec2 effectiveSize = (rb.colliderSize.x > 0.0f && rb.colliderSize.y > 0.0f)
      ? rb.colliderSize : transform.size;
  bool isCircle = rb.colliderShape == Hamster::ColliderShape::Circle;

  ImGui::TextDisabled("Offset: %.1f, %.1f  Size: %.1f x %.1f  %s",
                      rb.colliderOffset.x, rb.colliderOffset.y,
                      effectiveSize.x, effectiveSize.y,
                      isCircle ? "(Circle)" : "(Box)");

  ImGui::Dummy({0, 4});

  // Canvas area
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  if (canvasSize.x < 100) canvasSize.x = 100;
  if (canvasSize.y < 100) canvasSize.y = 100;

  ImGui::InvisibleButton("##canvas", canvasSize,
                         ImGuiButtonFlags_MouseButtonLeft);
  bool canvasHovered = ImGui::IsItemHovered();
  bool canvasActive = ImGui::IsItemActive();

  ImDrawList *dl = ImGui::GetWindowDrawList();

  // Dark background for canvas
  dl->AddRectFilled(canvasPos,
                    {canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y},
                    IM_COL32(30, 30, 30, 255));

  // Compute scale: fit the transform rect into the canvas with padding
  float padding = 60.0f;
  float scaleX = (canvasSize.x - padding * 2) / std::max(transform.size.x, 1.0f);
  float scaleY = (canvasSize.y - padding * 2) / std::max(transform.size.y, 1.0f);
  float scale = std::min(scaleX, scaleY);
  scale = std::max(scale, 0.1f);

  // Center of canvas in screen coords
  ImVec2 center = {canvasPos.x + canvasSize.x * 0.5f,
                   canvasPos.y + canvasSize.y * 0.5f};

  // Reference rect (transform bounds) centered in canvas
  float refW = transform.size.x * scale;
  float refH = transform.size.y * scale;
  ImVec2 refTL = {center.x - refW * 0.5f, center.y - refH * 0.5f};
  ImVec2 refBR = {center.x + refW * 0.5f, center.y + refH * 0.5f};

  // Draw sprite or grey reference rect
  if (sprite && sprite->texture && sprite->texture->GetTextureId() != 0) {
    dl->AddImage(
        (ImTextureID)(intptr_t)sprite->texture->GetTextureId(),
        refTL, refBR,
        {0, 0}, {1, 1},
        IM_COL32(255, 255, 255, 200));
  } else {
    dl->AddRectFilled(refTL, refBR, kRefRectColor);
  }
  dl->AddRect(refTL, refBR, IM_COL32(80, 80, 80, 255));

  // Collider rect/circle
  float colW = effectiveSize.x * scale;
  float colH = effectiveSize.y * scale;
  float offX = rb.colliderOffset.x * scale;
  float offY = rb.colliderOffset.y * scale;

  ImVec2 colCenter = {center.x + offX, center.y + offY};

  if (isCircle) {
    float radius = std::max(colW, colH) * 0.5f;
    dl->AddCircleFilled(colCenter, radius, kColliderFill);
    dl->AddCircle(colCenter, radius, kColliderColor, 0, 2.0f);

    // 4 handles at cardinal points for circle
    ImVec2 handles[4] = {
        {colCenter.x, colCenter.y - radius},     // top
        {colCenter.x + radius, colCenter.y},      // right
        {colCenter.x, colCenter.y + radius},      // bottom
        {colCenter.x - radius, colCenter.y},      // left
    };

    for (int i = 0; i < 4; i++) {
      dl->AddRectFilled(
          {handles[i].x - kHandleSize * 0.5f, handles[i].y - kHandleSize * 0.5f},
          {handles[i].x + kHandleSize * 0.5f, handles[i].y + kHandleSize * 0.5f},
          kHandleColor);
      dl->AddRect(
          {handles[i].x - kHandleSize * 0.5f, handles[i].y - kHandleSize * 0.5f},
          {handles[i].x + kHandleSize * 0.5f, handles[i].y + kHandleSize * 0.5f},
          kHandleBorder);
    }

    // Handle interaction for circle
    ImVec2 mousePos = ImGui::GetMousePos();

    if (canvasHovered && ImGui::IsMouseClicked(0)) {
      m_DraggingHandle = -1;
      m_DraggingBody = false;

      for (int i = 0; i < 4; i++) {
        if (std::abs(mousePos.x - handles[i].x) < kHandleHitSize &&
            std::abs(mousePos.y - handles[i].y) < kHandleHitSize) {
          m_DraggingHandle = i;
          break;
        }
      }

      if (m_DraggingHandle == -1) {
        float dx = mousePos.x - colCenter.x;
        float dy = mousePos.y - colCenter.y;
        if (dx * dx + dy * dy < radius * radius) {
          m_DraggingBody = true;
          m_DragStart = {mousePos.x, mousePos.y};
        }
      }
    }

    if (ImGui::IsMouseReleased(0)) {
      m_DraggingHandle = -1;
      m_DraggingBody = false;
    }

    ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;

    if (m_DraggingHandle >= 0) {
      float delta = 0.0f;
      if (m_DraggingHandle == 0) delta = -mouseDelta.y;       // top
      else if (m_DraggingHandle == 1) delta = mouseDelta.x;   // right
      else if (m_DraggingHandle == 2) delta = mouseDelta.y;   // bottom
      else if (m_DraggingHandle == 3) delta = -mouseDelta.x;  // left

      float currentRadius = std::max(effectiveSize.x, effectiveSize.y) * 0.5f;
      currentRadius += delta / scale;
      currentRadius = std::max(currentRadius, 2.0f);

      float diameter = currentRadius * 2.0f;
      rb.colliderSize = {diameter, diameter};
    }

    if (m_DraggingBody) {
      rb.colliderOffset.x += mouseDelta.x / scale;
      rb.colliderOffset.y += mouseDelta.y / scale;
    }

  } else {
    // Box collider
    ImVec2 colTL = {colCenter.x - colW * 0.5f, colCenter.y - colH * 0.5f};
    ImVec2 colBR = {colCenter.x + colW * 0.5f, colCenter.y + colH * 0.5f};

    dl->AddRectFilled(colTL, colBR, kColliderFill);
    dl->AddRect(colTL, colBR, kColliderColor, 0.0f, 0, 2.0f);

    // 8 handles: 4 corners + 4 edge midpoints
    ImVec2 handles[8] = {
        {colTL.x, colTL.y},                                   // 0: top-left
        {colBR.x, colTL.y},                                   // 1: top-right
        {colBR.x, colBR.y},                                   // 2: bottom-right
        {colTL.x, colBR.y},                                   // 3: bottom-left
        {(colTL.x + colBR.x) * 0.5f, colTL.y},               // 4: top-mid
        {colBR.x, (colTL.y + colBR.y) * 0.5f},                // 5: right-mid
        {(colTL.x + colBR.x) * 0.5f, colBR.y},               // 6: bottom-mid
        {colTL.x, (colTL.y + colBR.y) * 0.5f},                // 7: left-mid
    };

    for (int i = 0; i < 8; i++) {
      dl->AddRectFilled(
          {handles[i].x - kHandleSize * 0.5f, handles[i].y - kHandleSize * 0.5f},
          {handles[i].x + kHandleSize * 0.5f, handles[i].y + kHandleSize * 0.5f},
          kHandleColor);
      dl->AddRect(
          {handles[i].x - kHandleSize * 0.5f, handles[i].y - kHandleSize * 0.5f},
          {handles[i].x + kHandleSize * 0.5f, handles[i].y + kHandleSize * 0.5f},
          kHandleBorder);
    }

    // Handle interaction for box
    ImVec2 mousePos = ImGui::GetMousePos();

    if (canvasHovered && ImGui::IsMouseClicked(0)) {
      m_DraggingHandle = -1;
      m_DraggingBody = false;

      for (int i = 0; i < 8; i++) {
        if (std::abs(mousePos.x - handles[i].x) < kHandleHitSize &&
            std::abs(mousePos.y - handles[i].y) < kHandleHitSize) {
          m_DraggingHandle = i;
          break;
        }
      }

      if (m_DraggingHandle == -1) {
        if (mousePos.x >= colTL.x && mousePos.x <= colBR.x &&
            mousePos.y >= colTL.y && mousePos.y <= colBR.y) {
          m_DraggingBody = true;
        }
      }
    }

    if (ImGui::IsMouseReleased(0)) {
      m_DraggingHandle = -1;
      m_DraggingBody = false;
    }

    ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;

    if (m_DraggingHandle >= 0) {
      float dx = mouseDelta.x / scale;
      float dy = mouseDelta.y / scale;

      float w = effectiveSize.x;
      float h = effectiveSize.y;
      float ox = rb.colliderOffset.x;
      float oy = rb.colliderOffset.y;

      switch (m_DraggingHandle) {
      case 0: // top-left: shrink from top-left
        w -= dx; h -= dy;
        ox += dx * 0.5f; oy += dy * 0.5f;
        break;
      case 1: // top-right
        w += dx; h -= dy;
        ox += dx * 0.5f; oy += dy * 0.5f;
        break;
      case 2: // bottom-right
        w += dx; h += dy;
        ox += dx * 0.5f; oy += dy * 0.5f;
        break;
      case 3: // bottom-left
        w -= dx; h += dy;
        ox += dx * 0.5f; oy += dy * 0.5f;
        break;
      case 4: // top-mid (height only)
        h -= dy;
        oy += dy * 0.5f;
        break;
      case 5: // right-mid (width only)
        w += dx;
        ox += dx * 0.5f;
        break;
      case 6: // bottom-mid (height only)
        h += dy;
        oy += dy * 0.5f;
        break;
      case 7: // left-mid (width only)
        w -= dx;
        ox += dx * 0.5f;
        break;
      }

      w = std::max(w, 2.0f);
      h = std::max(h, 2.0f);

      rb.colliderSize = {w, h};
      rb.colliderOffset = {ox, oy};
    }

    if (m_DraggingBody) {
      rb.colliderOffset.x += mouseDelta.x / scale;
      rb.colliderOffset.y += mouseDelta.y / scale;
    }
  }

  ImGui::End();
}
