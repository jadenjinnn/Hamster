#include "CreateProjectModal.h"

#include <Core/Application.h>
#include <Core/Project.h>

#include <imgui.h>
#include <tinyfiledialogs.h>
#include <cstdlib> // std::getenv — USERPROFILE for the default project location
#include <cstring>

#include "../Theme.h"
#include "../ProjectRegistry.h"
#include "IconsFontAwesome6.h"

CreateProjectModal::CreateProjectModal(Hamster::Application *app)
    : m_App(app) {
  // Default new projects to the user's Documents — a writable location.
  // An installed build's own dir is read-only for non-admins, so we never
  // want the empty/install-relative default. User can still Browse elsewhere.
  if (const char *userProfile = std::getenv("USERPROFILE")) {
    m_ProjectDirectory = std::filesystem::path(userProfile) / "Documents";
  }
}

void CreateProjectModal::Render(ProjectRegistry *registry) {
  if (m_OpenRequested) {
    ImGui::OpenPopup("Create New Project");
    m_OpenRequested = false;
    m_IsOpen = true;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  // Width fixed; height auto-fits the content (0 on an axis = auto-size).
  ImGui::SetNextWindowSize(ImVec2(520, 0));

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
