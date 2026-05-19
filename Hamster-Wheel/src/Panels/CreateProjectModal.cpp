#include "CreateProjectModal.h"

#include <Core/Application.h>
#include <Core/Project.h>

#include <imgui.h>
#include <tinyfiledialogs.h>
#include <cstring>

#include "../Theme.h"
#include "../ProjectRegistry.h"
#include "IconsFontAwesome6.h"

CreateProjectModal::CreateProjectModal(Hamster::Application *app)
    : m_App(app) {}

void CreateProjectModal::Render(ProjectRegistry *registry) {
  if (m_OpenRequested) {
    ImGui::OpenPopup("Create New Project");
    m_OpenRequested = false;
    m_IsOpen = true;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520, 640));

  ImGuiWindowFlags modalFlags =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

  if (ImGui::BeginPopupModal("Create New Project", &m_IsOpen, modalFlags)) {
    if (g_BoldFont) ImGui::PushFont(g_BoldFont);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "PROJECT NAME");
    if (g_BoldFont) ImGui::PopFont();

    ImGui::Spacing();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##ProjectName", "My Awesome Game", m_ProjectName,
                             IM_ARRAYSIZE(m_ProjectName));

    ImGui::Dummy(ImVec2(0, 16));

    if (g_BoldFont) ImGui::PushFont(g_BoldFont);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "TEMPLATE");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100.0f);
    ImGui::TextColored(ImVec4(kHubGreen.x, kHubGreen.y, kHubGreen.z, 0.8f),
                       "Select a starting point");
    if (g_BoldFont) ImGui::PopFont();

    ImGui::Spacing();

    struct TemplateInfo {
      const char *icon;
      const char *name;
      const char *desc;
    };

    static const TemplateInfo templates[] = {
        {ICON_FA_SQUARE, "Empty", "Start from scratch with\na blank canvas."},
        {ICON_FA_GAMEPAD, "Platformer 2D",
         "Basic physics, character\ncontroller & tiles."},
        {ICON_FA_COMPASS, "Top-Down",
         "8-way movement and simple\nmap setup."},
        {ICON_FA_WINDOW_MAXIMIZE, "UI Only",
         "Pre-configured canvas for\nmenu interfaces."},
    };

    float templateW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
    float templateH = 80.0f;

    for (int i = 0; i < 4; i++) {
      if (i % 2 != 0) ImGui::SameLine(0, 8.0f);

      ImVec2 cursor = ImGui::GetCursorScreenPos();
      ImDrawList *dl = ImGui::GetWindowDrawList();
      ImVec2 tMin = cursor;
      ImVec2 tMax = ImVec2(cursor.x + templateW, cursor.y + templateH);

      bool selected = (m_SelectedTemplate == i);
      bool hov = ImGui::IsMouseHoveringRect(tMin, tMax);

      ImU32 bg = selected ? IM_COL32(30, 50, 65, 255)
                 : hov    ? ImGui::ColorConvertFloat4ToU32(kSurfaceHov)
                          : ImGui::ColorConvertFloat4ToU32(kSurface);
      ImU32 border = selected
                         ? ImGui::ColorConvertFloat4ToU32(kHubGreen)
                         : ImGui::ColorConvertFloat4ToU32(kBorder);

      dl->AddRectFilled(tMin, tMax, bg, 6.0f);
      dl->AddRect(tMin, tMax, border, 6.0f, 0, selected ? 2.0f : 1.0f);

      dl->AddText(ImVec2(cursor.x + 14.0f, cursor.y + 14.0f),
                  ImGui::ColorConvertFloat4ToU32(selected ? kHubGreen : kTextDim),
                  templates[i].icon);

      if (g_BoldFont) ImGui::PushFont(g_BoldFont);
      dl->AddText(ImVec2(cursor.x + 44.0f, cursor.y + 12.0f),
                  ImGui::ColorConvertFloat4ToU32(kText), templates[i].name);
      if (g_BoldFont) ImGui::PopFont();

      dl->AddText(ImVec2(cursor.x + 44.0f, cursor.y + 32.0f),
                  ImGui::ColorConvertFloat4ToU32(kTextDim), templates[i].desc);

      if (hov && ImGui::IsMouseClicked(0))
        m_SelectedTemplate = i;

      ImGui::Dummy(ImVec2(templateW, templateH));
      if (i == 1)
        ImGui::Spacing();
    }

    ImGui::Dummy(ImVec2(0, 16));

    // ─── Resolution ───
    if (g_BoldFont) ImGui::PushFont(g_BoldFont);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "RESOLUTION");
    if (g_BoldFont) ImGui::PopFont();

    ImGui::Spacing();

    struct ResolutionPreset {
      const char *label;
      int width;  // 0 sentinels the "Custom" entry — see input fields below
      int height;
    };
    static const ResolutionPreset kPresets[] = {
        {"1280 x 720 (HD)", 1280, 720},
        {"1920 x 1080 (Full HD)", 1920, 1080},
        {"800 x 600", 800, 600},
        {"Custom...", 0, 0},
    };
    constexpr int kPresetCount =
        static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));
    constexpr int kCustomIndex = kPresetCount - 1;

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Resolution",
                          kPresets[m_SelectedResolutionPreset].label)) {
      for (int i = 0; i < kPresetCount; i++) {
        bool sel = (m_SelectedResolutionPreset == i);
        if (ImGui::Selectable(kPresets[i].label, sel)) {
          m_SelectedResolutionPreset = i;
        }
        if (sel) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    if (m_SelectedResolutionPreset == kCustomIndex) {
      ImGui::Spacing();
      float halfW = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
      ImGui::SetNextItemWidth(halfW);
      ImGui::InputInt("##CustomWidth", &m_CustomWidth);
      ImGui::SameLine(0, 8.0f);
      ImGui::SetNextItemWidth(halfW);
      ImGui::InputInt("##CustomHeight", &m_CustomHeight);
      // Clamp to sane range so 0 / negative / 8K+ are rejected at input.
      if (m_CustomWidth < 100) m_CustomWidth = 100;
      if (m_CustomWidth > 7680) m_CustomWidth = 7680;
      if (m_CustomHeight < 100) m_CustomHeight = 100;
      if (m_CustomHeight > 4320) m_CustomHeight = 4320;
    }

    ImGui::Dummy(ImVec2(0, 16));

    if (g_BoldFont) ImGui::PushFont(g_BoldFont);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "LOCATION");
    if (g_BoldFont) ImGui::PopFont();

    ImGui::Spacing();

    float browseW = 80.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseW - 8.0f);

    std::string dirDisplay =
        m_ProjectDirectory.empty() ? "" : m_ProjectDirectory.string();
    char dirBuf[512];
    strncpy(dirBuf, dirDisplay.c_str(), sizeof(dirBuf) - 1);
    dirBuf[sizeof(dirBuf) - 1] = '\0';
    ImGui::InputTextWithHint("##Location", "C:\\Projects\\Hamster", dirBuf,
                             sizeof(dirBuf), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse...", ImVec2(browseW, 0))) {
      const char *dir =
          tinyfd_selectFolderDialog("Select directory for project", "");
      if (dir) {
        m_ProjectDirectory = dir;
        m_NoDirectorySelected = false;
      }
    }

    if (m_NoDirectorySelected) {
      ImGui::TextColored(ImVec4(0.9f, 0.22f, 0.27f, 1.0f),
                         "You must choose a project directory!");
    }
    if (m_DirectoryExists) {
      ImGui::TextColored(
          ImVec4(0.9f, 0.22f, 0.27f, 1.0f),
          "Folder with same name already exists at that location!");
    }

    ImGui::Dummy(ImVec2(0, 16));

    float cancelW = 80.0f;
    float createW = 120.0f;
    float totalBtnW = cancelW + 8.0f + createW;
    ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - totalBtnW +
                         ImGui::GetCursorPosX());

    if (ImGui::Button("Cancel", ImVec2(cancelW, 32))) {
      m_IsOpen = false;
      m_NoDirectorySelected = false;
      m_DirectoryExists = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine(0, 8.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, kHubGreen);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kHubGreenHov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kHubGreenAct);
    if (ImGui::Button("Create Project", ImVec2(createW, 32))) {
      if (m_ProjectDirectory.empty()) {
        m_NoDirectorySelected = true;
      } else {
        Hamster::ProjectConfig config;
        config.Name = m_ProjectName;
        config.ProjectDirectory = m_ProjectDirectory / config.Name;

        // Resolve resolution from the picker — preset table is mirrored
        // from the UI above; Custom (sentinel width=0) uses the
        // m_CustomWidth/Height inputs.
        static const struct { int w; int h; } kPickerResolutions[] = {
            {1280, 720}, {1920, 1080}, {800, 600}, {0, 0},
        };
        const auto &picked = kPickerResolutions[m_SelectedResolutionPreset];
        if (picked.w == 0) {
          config.TargetWidth = m_CustomWidth;
          config.TargetHeight = m_CustomHeight;
        } else {
          config.TargetWidth = picked.w;
          config.TargetHeight = picked.h;
        }

        if (std::filesystem::is_directory(config.ProjectDirectory)) {
          m_DirectoryExists = true;
        } else {
          m_DirectoryExists = false;
          m_NoDirectorySelected = false;

          Hamster::Project::New(config, m_App);

          std::string hamLibPath =
              Hamster::Application::GetExecutablePath() +
              "/../share/Resources/Hamster-Wheel/Resources/Packages/"
              HAMSTER_PY_MODULE_FILENAME;
          std::filesystem::copy(hamLibPath, config.ProjectDirectory);

          if (registry) {
            registry->AddOrUpdate(config.Name, config.ProjectDirectory);
          }

          m_IsOpen = false;
          ImGui::CloseCurrentPopup();
        }
      }
    }
    ImGui::PopStyleColor(3);

    ImGui::EndPopup();
  }
}
