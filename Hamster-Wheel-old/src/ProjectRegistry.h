#ifndef PROJECT_REGISTRY_H
#define PROJECT_REGISTRY_H

#include <filesystem>
#include <string>
#include <vector>

class ProjectRegistry {
public:
  struct Entry {
    std::string name;
    std::filesystem::path path;
    std::string lastOpened; // ISO 8601
    bool missing = false;   // set at load time if path doesn't exist
  };

  ProjectRegistry();

  void Load();
  void Save() const;

  void AddOrUpdate(const std::string &name, const std::filesystem::path &path);
  void Remove(const std::filesystem::path &path);
  bool Rename(const std::filesystem::path &oldPath,
              const std::string &newName);
  void UpdateTimestamp(const std::filesystem::path &path);

  std::vector<Entry> &GetEntries() { return m_Entries; }
  const std::vector<Entry> &GetEntries() const { return m_Entries; }

private:
  static std::filesystem::path GetRegistryPath();
  static std::string NowISO8601();

  std::vector<Entry> m_Entries;
};

#endif // PROJECT_REGISTRY_H
