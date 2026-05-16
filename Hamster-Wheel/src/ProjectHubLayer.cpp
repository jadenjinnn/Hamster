#include "ProjectHubLayer.h"

#include <chrono>
#include <imgui.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <tinyfiledialogs.h>

#include "Theme.h"
#include "IconsFontAwesome6.h"

static constexpr float kTopBarHeight = 50.0f;
static constexpr float kHeaderHeight = 90.0f;
static constexpr float kSidePadding = 32.0f;
static constexpr float kCardGap = 16.0f;
static constexpr int kColumns = 4;

ProjectHubLayer::ProjectHubLayer(Hamster::Application *app)
    : m_App(app), m_Dispatcher(app->GetEventDispatcher().get()) {
  m_CreateModal = std::make_unique<CreateProjectModal>(app);
}

void ProjectHubLayer::OnImGuiUpdate() {
  m_App->GetRenderer()->Clear();

  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                           ImGuiWindowFlags_NoCollapse |
                           ImGuiWindowFlags_NoBringToFrontOnFocus |
                           ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin("##ProjectHub", nullptr, flags);
  ImGui::PopStyleVar(2);

  float winW = ImGui::GetContentRegionAvail().x;
  float winH = ImGui::GetContentRegionAvail().y;

  RenderTopBar(winW);
  RenderHeader(winW);

  float gridStartY = ImGui::GetCursorScreenPos().y;
  float gridHeight = winH - kTopBarHeight - kHeaderHeight;
  RenderCardGrid(winW, gridStartY, gridHeight);

  ImGui::End();

  m_CreateModal->Render(&m_Registry);
  RenderRenameModal();
  RenderDeleteConfirmation();
  RenderMissingProjectDialog();
}

void ProjectHubLayer::RenderTopBar(float width) {
  // We hit-test geometrically via IsMouseHoveringRect; ImGui's modal input
  // blocking is focus-based and won't stop clicks from leaking through to
  // these widgets. Suppress hit-testing while any popup is open. See bug 0005.
  const bool inputBlocked = ImGui::IsPopupOpen(
      "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

  ImVec2 cursor = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  dl->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + kTopBarHeight),
                    ImGui::ColorConvertFloat4ToU32(kHeader));

  dl->AddLine(ImVec2(cursor.x, cursor.y + kTopBarHeight),
              ImVec2(cursor.x + width, cursor.y + kTopBarHeight),
              ImGui::ColorConvertFloat4ToU32(kBorder));

  if (g_BoldFont) ImGui::PushFont(g_BoldFont);
  ImVec2 brandPos = ImVec2(cursor.x + kSidePadding, cursor.y + 15.0f);
  dl->AddText(brandPos, ImGui::ColorConvertFloat4ToU32(kText), "Hamster");
  if (g_BoldFont) ImGui::PopFont();

  float rightX = cursor.x + width - kSidePadding;

  const char *icons[] = {ICON_FA_USER, ICON_FA_GEAR, ICON_FA_CIRCLE_QUESTION,
                         ICON_FA_BELL};
  float iconX = rightX;
  for (int i = 0; i < 4; i++) {
    ImVec2 textSize = ImGui::CalcTextSize(icons[i]);
    iconX -= textSize.x;
    dl->AddText(ImVec2(iconX, cursor.y + 16.0f),
                ImGui::ColorConvertFloat4ToU32(kTextDim), icons[i]);
    iconX -= 20.0f;
  }

  float btnTextW = ImGui::CalcTextSize("New Project").x;
  float btnW = btnTextW + 24.0f;
  float btnH = 30.0f;
  float btnX = iconX - btnW - 12.0f;
  float btnY = cursor.y + (kTopBarHeight - btnH) * 0.5f;
  ImVec2 btnMin = ImVec2(btnX, btnY);
  ImVec2 btnMax = ImVec2(btnX + btnW, btnY + btnH);

  bool hovered = !inputBlocked && ImGui::IsMouseHoveringRect(btnMin, btnMax);
  bool clicked = hovered && ImGui::IsMouseClicked(0);
  ImU32 btnCol = ImGui::ColorConvertFloat4ToU32(hovered ? kHubGreenHov : kHubGreen);

  dl->AddRectFilled(btnMin, btnMax, btnCol, 6.0f);
  ImVec2 textPos =
      ImVec2(btnX + (btnW - btnTextW) * 0.5f, btnY + (btnH - 16.0f) * 0.5f);
  dl->AddText(textPos, IM_COL32(255, 255, 255, 255), "New Project");

  if (clicked)
    m_CreateModal->Show();

  const char *openLabel = ICON_FA_FOLDER_OPEN "  Open Project";
  float openW = ImGui::CalcTextSize(openLabel).x;
  float openX = btnX - openW - 20.0f;
  ImVec2 openMin = ImVec2(openX - 8.0f, btnY);
  ImVec2 openMax = ImVec2(openX + openW + 8.0f, btnY + btnH);

  bool openHov = !inputBlocked && ImGui::IsMouseHoveringRect(openMin, openMax);
  bool openClk = openHov && ImGui::IsMouseClicked(0);
  if (openHov)
    dl->AddRectFilled(openMin, openMax,
                      ImGui::ColorConvertFloat4ToU32(kSurfaceHov), 6.0f);
  dl->AddText(ImVec2(openX, btnY + (btnH - 16.0f) * 0.5f),
              ImGui::ColorConvertFloat4ToU32(kText), openLabel);

  if (openClk)
    OpenProjectDialog();

  ImGui::SetCursorScreenPos(
      ImVec2(cursor.x, cursor.y + kTopBarHeight + 1.0f));
}

void ProjectHubLayer::RenderHeader(float width) {
  ImVec2 cursor = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  float textX = cursor.x + kSidePadding;
  float textY = cursor.y + 12.0f;

  if (g_HeaderFont) ImGui::PushFont(g_HeaderFont);
  dl->AddText(ImVec2(textX, textY),
              ImGui::ColorConvertFloat4ToU32(kText), "Projects");
  if (g_HeaderFont) ImGui::PopFont();

  dl->AddText(ImVec2(textX, textY + 36.0f),
              ImGui::ColorConvertFloat4ToU32(kTextDim),
              "Manage your active workspaces and recent edits.");

  ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + kHeaderHeight));
}

void ProjectHubLayer::RenderCardGrid(float width, float startY, float height) {
  // Suppress hit-testing while any popup/modal is open — modals block focus,
  // not geometry, so clicks would otherwise leak through. See bug 0005.
  const bool inputBlocked = ImGui::IsPopupOpen(
      "", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

  auto &entries = m_Registry.GetEntries();
  int projectCount = static_cast<int>(entries.size());

  float contentW = width - kSidePadding * 2.0f;
  float cardW =
      (contentW - kCardGap * (kColumns - 1)) / static_cast<float>(kColumns);
  float thumbH = cardW * 0.55f;
  float infoH = 90.0f;
  float cardH = thumbH + infoH;

  ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x, startY));
  ImGui::BeginChild("##CardGrid", ImVec2(width, height), false,
                    ImGuiWindowFlags_NoBackground);

  ImVec2 origin = ImGui::GetCursorScreenPos();
  origin.x += kSidePadding;
  origin.y += 8.0f;

  ImDrawList *dl = ImGui::GetWindowDrawList();

  int totalCards = 1 + projectCount; // "Create New" + real projects
  int rows = (totalCards + kColumns - 1) / kColumns;

  for (int idx = 0; idx < totalCards; idx++) {
    int col = idx % kColumns;
    int row = idx / kColumns;

    float x = origin.x + col * (cardW + kCardGap);
    float y = origin.y + row * (cardH + kCardGap);

    ImVec2 cardMin = ImVec2(x, y);
    ImVec2 cardMax = ImVec2(x + cardW, y + cardH);

    bool hovered = !inputBlocked && ImGui::IsMouseHoveringRect(cardMin, cardMax);

    if (idx == 0) {
      // "Create New Project" card
      ImU32 borderCol = ImGui::ColorConvertFloat4ToU32(hovered ? kSurfaceHov : kBorder);

      dl->AddRectFilled(cardMin, cardMax,
                        ImGui::ColorConvertFloat4ToU32(kSurface), 8.0f);
      dl->AddRect(cardMin, cardMax, borderCol, 8.0f, 0, 1.5f);

      float centerX = x + cardW * 0.5f;
      float centerY = y + cardH * 0.42f;
      float circleR = 24.0f;
      dl->AddCircle(ImVec2(centerX, centerY), circleR,
                    ImGui::ColorConvertFloat4ToU32(kTextDim), 32, 1.5f);

      const char *plusText = ICON_FA_PLUS;
      ImVec2 plusSize = ImGui::CalcTextSize(plusText);
      dl->AddText(ImVec2(centerX - plusSize.x * 0.5f,
                         centerY - plusSize.y * 0.5f),
                  ImGui::ColorConvertFloat4ToU32(kTextDim), plusText);

      const char *label = "Create New Project";
      ImVec2 labelSize = ImGui::CalcTextSize(label);
      dl->AddText(
          ImVec2(centerX - labelSize.x * 0.5f, centerY + circleR + 16.0f),
          ImGui::ColorConvertFloat4ToU32(kText), label);

      if (hovered && ImGui::IsMouseClicked(0))
        m_CreateModal->Show();
    } else {
      int projIdx = idx - 1;
      auto &entry = entries[projIdx];

      ImU32 cardBg = ImGui::ColorConvertFloat4ToU32(hovered ? kSurfaceHov : kSurface);
      ImU32 borderCol = entry.missing
                            ? ImGui::ColorConvertFloat4ToU32(kRed)
                            : ImGui::ColorConvertFloat4ToU32(kBorder);

      dl->AddRectFilled(cardMin, cardMax, cardBg, 8.0f);
      dl->AddRect(cardMin, cardMax, borderCol, 8.0f, 0, 1.0f);

      // Thumbnail area
      ImVec2 thumbMin = ImVec2(x + 1.0f, y + 1.0f);
      ImVec2 thumbMax = ImVec2(x + cardW - 1.0f, y + thumbH);
      dl->AddRectFilled(thumbMin, thumbMax,
                        ImGui::ColorConvertFloat4ToU32(kCanvas), 7.0f,
                        ImDrawFlags_RoundCornersTop);

      // Decorative shape
      float cx = x + cardW * 0.5f;
      float cy = y + thumbH * 0.5f;
      float shapeSize = thumbH * 0.25f;
      int shapeType = projIdx % 3;
      ImU32 shapeCol = entry.missing ? IM_COL32(180, 60, 60, 80)
                                     : IM_COL32(80, 180, 120, 120);

      if (shapeType == 0) {
        ImVec2 pts[4] = {ImVec2(cx, cy - shapeSize),
                         ImVec2(cx + shapeSize, cy),
                         ImVec2(cx, cy + shapeSize),
                         ImVec2(cx - shapeSize, cy)};
        dl->AddPolyline(pts, 4, shapeCol, ImDrawFlags_Closed, 1.5f);
      } else if (shapeType == 1) {
        dl->AddRect(ImVec2(cx - shapeSize, cy - shapeSize * 0.7f),
                    ImVec2(cx + shapeSize, cy + shapeSize * 0.7f), shapeCol,
                    0.0f, 0, 1.5f);
        dl->AddCircleFilled(ImVec2(cx + shapeSize * 0.3f, cy),
                            shapeSize * 0.3f,
                            entry.missing ? IM_COL32(180, 60, 60, 50)
                                          : IM_COL32(80, 180, 120, 80));
      } else {
        dl->AddCircle(ImVec2(cx, cy), shapeSize, shapeCol, 32, 1.5f);
      }

      if (entry.missing) {
        const char *missingLabel = ICON_FA_TRIANGLE_EXCLAMATION " Missing";
        ImVec2 missingSize = ImGui::CalcTextSize(missingLabel);
        dl->AddText(ImVec2(cx - missingSize.x * 0.5f, cy + shapeSize + 8.0f),
                    ImGui::ColorConvertFloat4ToU32(kRed), missingLabel);
      }

      // Info area
      float infoX = x + 14.0f;
      float infoY = y + thumbH + 10.0f;
      float infoRight = x + cardW - 14.0f;

      if (g_BoldFont) ImGui::PushFont(g_BoldFont);
      ImVec2 nameSize = ImGui::CalcTextSize(entry.name.c_str());
      float maxNameW = cardW - 60.0f;
      ImU32 nameCol = entry.missing ? IM_COL32(180, 140, 140, 255)
                                    : ImGui::ColorConvertFloat4ToU32(kText);
      if (nameSize.x > maxNameW) {
        dl->PushClipRect(ImVec2(infoX, infoY),
                         ImVec2(infoX + maxNameW, infoY + 20.0f), true);
        dl->AddText(ImVec2(infoX, infoY), nameCol, entry.name.c_str());
        dl->PopClipRect();
      } else {
        dl->AddText(ImVec2(infoX, infoY), nameCol, entry.name.c_str());
      }
      if (g_BoldFont) ImGui::PopFont();

      float bottomY = infoY + 28.0f;
      std::string timeStr = FormatTimestamp(entry.lastOpened);
      dl->AddText(ImVec2(infoX, bottomY),
                  ImGui::ColorConvertFloat4ToU32(kTextDim),
                  timeStr.c_str());

      // Ellipsis button
      const char *ellipsis = ICON_FA_ELLIPSIS;
      ImVec2 ellipsisSize = ImGui::CalcTextSize(ellipsis);
      float ellipsisX = infoRight - ellipsisSize.x;
      float ellipsisY = bottomY;
      ImVec2 ellipsisMin = ImVec2(ellipsisX - 4.0f, ellipsisY - 4.0f);
      ImVec2 ellipsisMax =
          ImVec2(ellipsisX + ellipsisSize.x + 4.0f, ellipsisY + 20.0f);

      bool ellipsisHov = !inputBlocked &&
                         ImGui::IsMouseHoveringRect(ellipsisMin, ellipsisMax);
      if (ellipsisHov)
        dl->AddRectFilled(ellipsisMin, ellipsisMax,
                          ImGui::ColorConvertFloat4ToU32(kSurfaceHov), 4.0f);
      dl->AddText(ImVec2(ellipsisX, ellipsisY),
                  ImGui::ColorConvertFloat4ToU32(ellipsisHov ? kText : kTextDim),
                  ellipsis);

      if (ellipsisHov && ImGui::IsMouseClicked(0)) {
        m_ContextMenuIndex = projIdx;
        ImGui::OpenPopup("##CardContextMenu");
      }

      if (hovered && !ellipsisHov && ImGui::IsMouseClicked(0)) {
        if (entry.missing) {
          m_MissingIndex = projIdx;
          m_ShowMissingDialog = true;
        } else {
          OpenProject(entry.path);
        }
      }
    }
  }

  if (ImGui::BeginPopup("##CardContextMenu")) {
    if (m_ContextMenuIndex >= 0 &&
        m_ContextMenuIndex < static_cast<int>(entries.size())) {
      if (ImGui::MenuItem(ICON_FA_PEN "  Rename")) {
        m_RenameIndex = m_ContextMenuIndex;
        auto &name = entries[m_RenameIndex].name;
        strncpy(m_RenameBuffer, name.c_str(), sizeof(m_RenameBuffer) - 1);
        m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
        m_ShowRenameModal = true;
      }
      if (ImGui::MenuItem(ICON_FA_TRASH "  Delete")) {
        m_DeleteIndex = m_ContextMenuIndex;
        m_ShowDeleteConfirm = true;
      }
    }
    ImGui::EndPopup();
  }

  float totalH = rows * (cardH + kCardGap) + 16.0f;
  ImGui::Dummy(ImVec2(0, totalH));

  ImGui::EndChild();
}


void ProjectHubLayer::RenderRenameModal() {
  if (m_ShowRenameModal) {
    ImGui::OpenPopup("Rename Project");
    m_ShowRenameModal = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(400, 160));

  if (ImGui::BeginPopupModal("Rename Project", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    auto &entries = m_Registry.GetEntries();
    if (m_RenameIndex >= 0 &&
        m_RenameIndex < static_cast<int>(entries.size())) {

      ImGui::Text("New name:");
      ImGui::SetNextItemWidth(-1);
      ImGui::InputText("##RenameBuf", m_RenameBuffer,
                       sizeof(m_RenameBuffer));

      if (m_RenameError) {
        ImGui::TextColored(ImVec4(0.9f, 0.22f, 0.27f, 1.0f),
                           "A folder with that name already exists!");
      }

      ImGui::Dummy(ImVec2(0, 12));

      float btnW = 80.0f;
      ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnW * 2 - 8.0f +
                           ImGui::GetCursorPosX());

      if (ImGui::Button("Cancel", ImVec2(btnW, 28))) {
        m_RenameIndex = -1;
        m_RenameError = false;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine(0, 8.0f);

      ImGui::PushStyleColor(ImGuiCol_Button, kHubGreen);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHubGreenHov);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, kHubGreenAct);
      if (ImGui::Button("Rename", ImVec2(btnW, 28))) {
        std::string newName(m_RenameBuffer);
        if (!newName.empty()) {
          auto &entry = entries[m_RenameIndex];
          if (m_Registry.Rename(entry.path, newName)) {
            m_RenameIndex = -1;
            m_RenameError = false;
            ImGui::CloseCurrentPopup();
          } else {
            m_RenameError = true;
          }
        }
      }
      ImGui::PopStyleColor(3);
    } else {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void ProjectHubLayer::RenderDeleteConfirmation() {
  if (m_ShowDeleteConfirm) {
    ImGui::OpenPopup("Delete Project?");
    m_ShowDeleteConfirm = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(460, 180));

  if (ImGui::BeginPopupModal("Delete Project?", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    auto &entries = m_Registry.GetEntries();
    if (m_DeleteIndex >= 0 &&
        m_DeleteIndex < static_cast<int>(entries.size())) {

      auto &entry = entries[m_DeleteIndex];
      ImGui::TextWrapped("This will permanently delete the project folder:");
      ImGui::Dummy(ImVec2(0, 4));

      if (g_BoldFont) ImGui::PushFont(g_BoldFont);
      ImGui::TextWrapped("%s", entry.path.string().c_str());
      if (g_BoldFont) ImGui::PopFont();

      ImGui::Dummy(ImVec2(0, 4));
      ImGui::TextColored(ImVec4(0.9f, 0.22f, 0.27f, 1.0f),
                         "This action cannot be undone.");

      ImGui::Dummy(ImVec2(0, 8));

      float btnW = 80.0f;
      ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnW * 2 - 8.0f +
                           ImGui::GetCursorPosX());

      if (ImGui::Button("Cancel", ImVec2(btnW, 28))) {
        m_DeleteIndex = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine(0, 8.0f);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
      if (ImGui::Button("Delete", ImVec2(btnW, 28))) {
        auto pathToDelete = entry.path;
        m_Registry.Remove(pathToDelete);

        auto cwd = std::filesystem::current_path();
        if (cwd.string().find(pathToDelete.string()) == 0)
          std::filesystem::current_path(pathToDelete.parent_path());

        std::error_code ec;
        std::filesystem::remove_all(pathToDelete, ec);
        if (ec) {
          tinyfd_messageBox(
              "Delete Warning",
              "Project removed from hub, but some files could not be deleted. "
              "You may need to remove them manually.",
              "ok", "warning", 1);
        }

        m_DeleteIndex = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::PopStyleColor(3);
    } else {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void ProjectHubLayer::RenderMissingProjectDialog() {
  if (m_ShowMissingDialog) {
    ImGui::OpenPopup("Project Not Found");
    m_ShowMissingDialog = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(460, 200));

  if (ImGui::BeginPopupModal("Project Not Found", nullptr,
                             ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove)) {
    auto &entries = m_Registry.GetEntries();
    if (m_MissingIndex >= 0 &&
        m_MissingIndex < static_cast<int>(entries.size())) {

      auto &entry = entries[m_MissingIndex];
      ImGui::TextWrapped(
          "The project folder could not be found at its expected location:");
      ImGui::Dummy(ImVec2(0, 4));

      if (g_BoldFont) ImGui::PushFont(g_BoldFont);
      ImGui::TextWrapped("%s", entry.path.string().c_str());
      if (g_BoldFont) ImGui::PopFont();

      ImGui::Dummy(ImVec2(0, 8));
      ImGui::Text("What would you like to do?");
      ImGui::Dummy(ImVec2(0, 8));

      float btnW = 100.0f;
      ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - btnW * 3 -
                           16.0f + ImGui::GetCursorPosX());

      if (ImGui::Button("Cancel", ImVec2(btnW, 28))) {
        m_MissingIndex = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine(0, 8.0f);

      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
      if (ImGui::Button("Remove", ImVec2(btnW, 28))) {
        m_Registry.Remove(entry.path);
        m_MissingIndex = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::PopStyleColor(3);
      ImGui::SameLine(0, 8.0f);

      ImGui::PushStyleColor(ImGuiCol_Button, kHubGreen);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHubGreenHov);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, kHubGreenAct);
      if (ImGui::Button("Locate...", ImVec2(btnW, 28))) {
        const char *dir =
            tinyfd_selectFolderDialog("Locate project folder", "");
        if (dir) {
          entry.path = std::filesystem::path(dir);
          entry.missing = !std::filesystem::exists(entry.path);
          m_Registry.Save();
          if (!entry.missing) {
            m_MissingIndex = -1;
            ImGui::CloseCurrentPopup();
          }
        }
      }
      ImGui::PopStyleColor(3);
    } else {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void ProjectHubLayer::OpenProjectDialog() {
  const char *filterPattern = "*.hamproj";
  const char *file = tinyfd_openFileDialog("Select Project File", "", 1,
                                           &filterPattern,
                                           "Hamster Project Files", 0);
  if (file) {
    std::filesystem::path path(file);
    if (path.extension() == ".hamproj") {
      auto projDir = path.parent_path();
      auto projName = path.stem().string();
      m_Registry.AddOrUpdate(projName, projDir);

      Hamster::Project::Open(path, m_App);
    }
  }
}

void ProjectHubLayer::OpenProject(const std::filesystem::path &projDir) {
  try {
    for (auto &f : std::filesystem::directory_iterator(projDir)) {
      if (f.path().extension() == ".hamproj") {
        m_Registry.UpdateTimestamp(projDir);
        Hamster::Project::Open(f.path(), m_App);
        return;
      }
    }

    tinyfd_messageBox("Open Failed",
                      "No .hamproj file found in the project directory.",
                      "ok", "error", 1);
  } catch (const std::exception &e) {
    std::cerr << "Failed to open project: " << e.what() << std::endl;
    tinyfd_messageBox("Open Failed", e.what(), "ok", "error", 1);
  }
}

std::string
ProjectHubLayer::FormatTimestamp(const std::string &iso) const {
  if (iso.empty())
    return "";

  std::tm tm{};
  std::istringstream ss(iso);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail())
    return iso;

  auto then = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  auto now = std::chrono::system_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - then);

  int minutes = static_cast<int>(diff.count());
  if (minutes < 1)
    return "Opened just now";
  if (minutes < 60)
    return "Opened " + std::to_string(minutes) + " min ago";

  int hours = minutes / 60;
  if (hours < 24)
    return "Opened " + std::to_string(hours) + " hr" +
           (hours > 1 ? "s" : "") + " ago";

  int days = hours / 24;
  if (days == 1)
    return "Opened yesterday";
  if (days < 7)
    return "Opened " + std::to_string(days) + " days ago";
  if (days < 14)
    return "Opened last week";

  std::ostringstream out;
  out << std::put_time(&tm, "Opened %b %d");
  return out.str();
}
