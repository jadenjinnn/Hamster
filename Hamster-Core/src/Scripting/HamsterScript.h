#pragma once

#include <filesystem>

#include <pybind11/pybind11.h>

namespace Hamster {
class Scene;
struct ID;
//     // class HamsterBehaviour;
class UUID;
class HamsterScript {
public:
  HamsterScript(const std::filesystem::path &scriptPath,
                const std::string &fileName);

  void ReloadScript();

  [[nodiscard]] std::filesystem::path GetScriptPath() { return m_ScriptPath; }
  //
  void SetUUID(UUID uuid) { m_UUID = uuid; }
  UUID GetUUID() { return m_UUID; }
  //
  void SetName(const std::string &name) { m_ScriptName = name; }
  std::string &GetName() { return m_ScriptName; }

  const std::string &GetFileName() { return m_FileName; }

  // Updates the script's on-disk path + Python module name. Called when the
  // user renames a script via the editor — the AssetManager has already moved
  // the .py and its .meta on disk, this just teaches the in-memory script
  // about its new identity. The next ReloadScript will re-import the module
  // under its new name.
  void SetScriptPath(const std::filesystem::path &newPath,
                     const std::string &newFileName);
  //
  std::vector<pybind11::handle> const &GetPyObjects() { return m_PyObjects; }
  //
private:
  std::string m_ScriptPath;
  std::string m_ScriptName = "Untitled Script";
  std::string m_FileName;
  UUID m_UUID;
  std::vector<pybind11::handle> m_PyObjects;
  pybind11::module_ m_Module;
};
} // namespace Hamster
