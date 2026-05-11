#include "ProjectRegistry.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

ProjectRegistry::ProjectRegistry() { Load(); }

std::filesystem::path ProjectRegistry::GetRegistryPath() {
  const char *appdata = std::getenv("APPDATA");
  if (!appdata)
    return {};
  std::filesystem::path dir =
      std::filesystem::path(appdata) / "Hamster";
  std::filesystem::create_directories(dir);
  return dir / "projects.json";
}

std::string ProjectRegistry::NowISO8601() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  localtime_s(&tm, &time);
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  return ss.str();
}

void ProjectRegistry::Load() {
  m_Entries.clear();
  auto path = GetRegistryPath();
  if (path.empty() || !std::filesystem::exists(path))
    return;

  std::ifstream in(path);
  if (!in.is_open())
    return;

  try {
    json j = json::parse(in);
    if (!j.contains("projects") || !j["projects"].is_array())
      return;

    for (auto &item : j["projects"]) {
      Entry e;
      e.name = item.value("name", "");
      e.path = std::filesystem::path(item.value("path", ""));
      e.lastOpened = item.value("lastOpened", "");
      e.missing = !std::filesystem::exists(e.path);
      m_Entries.push_back(std::move(e));
    }
  } catch (const json::exception &ex) {
    std::cerr << "[ProjectRegistry] Failed to parse " << path << ": "
              << ex.what() << "\n";
  }
}

void ProjectRegistry::Save() const {
  auto path = GetRegistryPath();
  if (path.empty())
    return;

  json arr = json::array();
  for (auto &e : m_Entries) {
    arr.push_back({{"name", e.name},
                   {"path", e.path.string()},
                   {"lastOpened", e.lastOpened}});
  }
  json j = {{"projects", arr}};

  // Write to temp file, then rename for crash safety
  auto tmp = path;
  tmp += ".tmp";
  {
    std::ofstream out(tmp);
    out << j.dump(2);
  }

  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    // Fallback: copy then remove temp
    std::filesystem::copy_file(tmp, path,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    std::filesystem::remove(tmp, ec);
  }
}

void ProjectRegistry::AddOrUpdate(const std::string &name,
                                  const std::filesystem::path &path) {
  auto canonical = std::filesystem::weakly_canonical(path);
  for (auto &e : m_Entries) {
    if (std::filesystem::weakly_canonical(e.path) == canonical) {
      e.name = name;
      e.lastOpened = NowISO8601();
      e.missing = false;
      Save();
      return;
    }
  }
  Entry e;
  e.name = name;
  e.path = path;
  e.lastOpened = NowISO8601();
  e.missing = false;
  m_Entries.insert(m_Entries.begin(), e);
  Save();
}

void ProjectRegistry::Remove(const std::filesystem::path &path) {
  auto canonical = std::filesystem::weakly_canonical(path);
  m_Entries.erase(
      std::remove_if(m_Entries.begin(), m_Entries.end(),
                     [&](const Entry &e) {
                       return std::filesystem::weakly_canonical(e.path) ==
                              canonical;
                     }),
      m_Entries.end());
  Save();
}

bool ProjectRegistry::Rename(const std::filesystem::path &oldPath,
                             const std::string &newName) {
  auto canonical = std::filesystem::weakly_canonical(oldPath);
  for (auto &e : m_Entries) {
    if (std::filesystem::weakly_canonical(e.path) != canonical)
      continue;

    auto newPath = oldPath.parent_path() / newName;

    if (std::filesystem::exists(newPath)) {
      std::cerr << "[ProjectRegistry] Cannot rename: " << newPath
                << " already exists\n";
      return false;
    }

    // Windows blocks rename if any process has CWD inside the directory
    std::filesystem::current_path(oldPath.parent_path());

    std::error_code ec;
    std::filesystem::rename(oldPath, newPath, ec);
    if (ec) {
      std::cerr << "[ProjectRegistry] Failed to rename " << oldPath
                << " -> " << newPath << ": " << ec.message() << "\n";
      return false;
    }

    e.name = newName;
    e.path = newPath;
    e.missing = false;
    Save();
    return true;
  }
  return false;
}

void ProjectRegistry::UpdateTimestamp(const std::filesystem::path &path) {
  auto canonical = std::filesystem::weakly_canonical(path);
  for (auto &e : m_Entries) {
    if (std::filesystem::weakly_canonical(e.path) == canonical) {
      e.lastOpened = NowISO8601();
      Save();
      return;
    }
  }
}
