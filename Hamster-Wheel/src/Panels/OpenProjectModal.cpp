#include "OpenProjectModal.h"

#include <Core/Application.h>
#include <Core/Project.h>

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

#include "../Theme.h"
#include "../ProjectRegistry.h"
#include "IconsFontAwesome6.h"

OpenProjectModal::OpenProjectModal(Hamster::Application *app) : m_App(app) {}

namespace {

std::string FormatTimestamp(const std::string &iso) {
  if (iso.empty()) return "";

  std::tm tm{};
  std::istringstream ss(iso);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  if (ss.fail()) return iso;

  auto then = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  auto now = std::chrono::system_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::minutes>(now - then);

  int minutes = static_cast<int>(diff.count());
  if (minutes < 1) return "Just now";
  if (minutes < 60) return std::to_string(minutes) + " min ago";

  int hours = minutes / 60;
  if (hours < 24)
    return std::to_string(hours) + " hr" + (hours > 1 ? "s" : "") + " ago";

  int days = hours / 24;
  if (days == 1) return "Yesterday";
  if (days < 7) return std::to_string(days) + " days ago";
  if (days < 14) return "Last week";

  std::ostringstream out;
  out << std::put_time(&tm, "%b %d");
  return out.str();
}

bool MatchesSearch(const std::string &name, const char *needle) {
  if (!needle || !needle[0]) return true;
  auto tolower_str = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
  };
  return tolower_str(name).find(tolower_str(needle)) != std::string::npos;
}

} // namespace

void OpenProjectModal::Render(ProjectRegistry *registry) {
  if (m_OpenRequested) {
    ImGui::OpenPopup("Open Project");
    m_OpenRequested = false;
    m_IsOpen = true;
    m_SearchBuffer[0] = '\0';
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(520, 520));

  ImGuiWindowFlags modalFlags =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

  if (!ImGui::BeginPopupModal("Open Project", &m_IsOpen, modalFlags)) return;

  // ── Search bar ─────────────────────────────────────────────────────────
  if (g_BoldFont) ImGui::PushFont(g_BoldFont);
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "SEARCH");
  if (g_BoldFont) ImGui::PopFont();

  ImGui::Spacing();
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##OpenSearch", ICON_FA_MAGNIFYING_GLASS "  Filter by name…",
                           m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));

  ImGui::Dummy(ImVec2(0, 12));

  if (g_BoldFont) ImGui::PushFont(g_BoldFont);
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "RECENT PROJECTS");
  if (g_BoldFont) ImGui::PopFont();
  ImGui::Spacing();

  // ── Filtered + sorted entry list (most-recent first) ───────────────────
  std::vector<const ProjectRegistry::Entry *> entries;
  if (registry) {
    for (const auto &e : registry->GetEntries()) {
      if (MatchesSearch(e.name, m_SearchBuffer)) entries.push_back(&e);
    }
    // Manual insertion sort by lastOpened desc. std::stable_sort with a
    // pointer-comparator lambda crashed deterministically after ~340 idle
    // frames — suspect MSVC STL debug iterator interaction with our heap.
    // For modest project counts this is plenty fast.
    for (size_t i = 1; i < entries.size(); i++) {
      for (size_t j = i; j > 0; j--) {
        if (entries[j]->lastOpened > entries[j - 1]->lastOpened) {
          std::swap(entries[j], entries[j - 1]);
        } else {
          break;
        }
      }
    }
  }

  // ── Scrollable list ────────────────────────────────────────────────────
  // Leave room for the Cancel button row at the bottom.
  float listH = ImGui::GetContentRegionAvail().y - 56.0f;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kCanvas);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
  ImGui::BeginChild("##ProjectList", ImVec2(-1, listH), true);
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  bool requestedOpen = false;
  std::filesystem::path requestedPath;

  if (entries.empty()) {
    ImGui::Dummy(ImVec2(0, 12));
    ImGui::PushStyleColor(ImGuiCol_Text, kTextDim);
    const char *empty = m_SearchBuffer[0] ? "No projects match that filter."
                                          : "No projects yet — create one first.";
    float w = ImGui::CalcTextSize(empty).x;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - w) * 0.5f);
    ImGui::Text("%s", empty);
    ImGui::PopStyleColor();
  } else {
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float rowH = 56.0f;
    float pad = 12.0f;

    for (size_t ri = 0; ri < entries.size(); ri++) {
      const auto *entry = entries[ri];
      ImVec2 rowStart = ImGui::GetCursorScreenPos();
      float availW = ImGui::GetContentRegionAvail().x;

      ImGui::PushID(entry->path.string().c_str());
      ImGui::InvisibleButton("##row", ImVec2(availW, rowH));
      bool hovered = ImGui::IsItemHovered();
      bool clicked = ImGui::IsItemClicked();
      ImGui::PopID();

      ImU32 bg = hovered ? ImGui::ColorConvertFloat4ToU32(kSurfaceHov)
                         : ImGui::ColorConvertFloat4ToU32(kSurface);
      dl->AddRectFilled(rowStart, ImVec2(rowStart.x + availW, rowStart.y + rowH),
                        bg, 4.0f);
      if (entry->missing) {
        dl->AddRect(rowStart,
                    ImVec2(rowStart.x + availW, rowStart.y + rowH),
                    ImGui::ColorConvertFloat4ToU32(kRed), 4.0f, 0, 1.0f);
      }

      ImU32 nameCol = entry->missing
                          ? ImGui::ColorConvertFloat4ToU32(
                                ImVec4(0.85f, 0.55f, 0.55f, 1.0f))
                          : ImGui::ColorConvertFloat4ToU32(kText);

      if (g_BoldFont) ImGui::PushFont(g_BoldFont);
      dl->AddText(ImVec2(rowStart.x + pad, rowStart.y + 8.0f), nameCol,
                  entry->name.c_str());
      if (g_BoldFont) ImGui::PopFont();

      std::string timeStr = FormatTimestamp(entry->lastOpened);
      if (!timeStr.empty()) {
        ImVec2 tSize = ImGui::CalcTextSize(timeStr.c_str());
        dl->AddText(ImVec2(rowStart.x + availW - pad - tSize.x,
                           rowStart.y + 8.0f),
                    ImGui::ColorConvertFloat4ToU32(kTextDim), timeStr.c_str());
      }

      std::string pathStr = entry->path.string();
      if (entry->missing) pathStr += "  " ICON_FA_TRIANGLE_EXCLAMATION " missing";
      // Clip path to row width.
      dl->PushClipRect(ImVec2(rowStart.x + pad, rowStart.y + 28.0f),
                       ImVec2(rowStart.x + availW - pad, rowStart.y + rowH),
                       true);
      dl->AddText(ImVec2(rowStart.x + pad, rowStart.y + 30.0f),
                  ImGui::ColorConvertFloat4ToU32(
                      entry->missing ? kRed : kTextDim),
                  pathStr.c_str());
      dl->PopClipRect();

      if (clicked && !entry->missing) {
        requestedOpen = true;
        requestedPath = entry->path;
      }

      ImGui::Dummy(ImVec2(0, 4));
    }
  }

  ImGui::EndChild();

  // ── Footer ─────────────────────────────────────────────────────────────
  ImGui::Dummy(ImVec2(0, 4));
  float cancelW = 80.0f;
  ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - cancelW +
                       ImGui::GetCursorPosX());
  if (ImGui::Button("Cancel", ImVec2(cancelW, 32))) {
    m_IsOpen = false;
    ImGui::CloseCurrentPopup();
  }

  if (requestedOpen) {
    // Locate the .hamproj file inside the project directory and open it.
    try {
      for (auto &f : std::filesystem::directory_iterator(requestedPath)) {
        if (f.path().extension() == ".hamproj") {
          if (registry) registry->UpdateTimestamp(requestedPath);
          Hamster::Project::Open(f.path(), m_App);
          m_IsOpen = false;
          ImGui::CloseCurrentPopup();
          break;
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Failed to open project: " << e.what() << std::endl;
    }
  }

  ImGui::EndPopup();
}
