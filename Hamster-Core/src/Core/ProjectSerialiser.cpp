//
// Created by Jaden on 31/08/2024.
//

#include "HamsterPCH.h"

#include "ProjectSerialiser.h"

#include "Utils/AssetManager.h"

namespace Hamster {
void ProjectSerialiser::Serialise(std::ostream &out) {
  const ProjectConfig &projectConfig = m_Project->GetConfig();

  std::size_t nameLength = projectConfig.Name.size();
  out.write(reinterpret_cast<const char *>(&nameLength), sizeof(nameLength));
  out.write(projectConfig.Name.data(), nameLength);

  std::string projectPathStr = projectConfig.ProjectDirectory.string();

  std::size_t projectPathLength = projectPathStr.size();
  out.write(reinterpret_cast<const char *>(&projectPathLength),
            sizeof(projectPathLength));
  out.write(projectPathStr.data(), projectPathLength);

  std::string startScenePathStr = projectConfig.StartScenePath.string();

  std::size_t startScenePathLength = startScenePathStr.size();
  out.write(reinterpret_cast<const char *>(&startScenePathLength),
            sizeof(startScenePathLength));
  out.write(startScenePathStr.data(), startScenePathLength);

  // Appended at end so legacy .hamproj files (without these bytes) still
  // deserialise — see Deserialise below for the EOF-tolerant read path.
  int32_t targetWidth = projectConfig.TargetWidth;
  int32_t targetHeight = projectConfig.TargetHeight;
  out.write(reinterpret_cast<const char *>(&targetWidth), sizeof(targetWidth));
  out.write(reinterpret_cast<const char *>(&targetHeight),
            sizeof(targetHeight));
}

ProjectConfig ProjectSerialiser::Deserialise(std::istream &in) {
  ProjectConfig projectConfig;

  std::size_t nameLength;
  in.read(reinterpret_cast<char *>(&nameLength), sizeof(nameLength));

  std::string nameStr(nameLength, '\0');
  in.read(nameStr.data(), nameLength);

  projectConfig.Name = nameStr;

  std::size_t projectPathLength;
  in.read(reinterpret_cast<char *>(&projectPathLength),
          sizeof(projectPathLength));

  std::string projectPathStr(projectPathLength, '\0');
  in.read(projectPathStr.data(), projectPathLength);

  projectConfig.ProjectDirectory = projectPathStr;

  std::size_t startScenePathLength;
  in.read(reinterpret_cast<char *>(&startScenePathLength),
          sizeof(startScenePathLength));

  std::string startScenePathStr(startScenePathLength, '\0');
  in.read(startScenePathStr.data(), startScenePathLength);

  projectConfig.StartScenePath = startScenePathStr;

  // Resolution fields appended in this feature. Legacy .hamproj files end
  // here, so peek for EOF before each int32 read; on partial / missing
  // bytes, clear stream state and fall back to the ProjectConfig defaults.
  int32_t targetWidth = projectConfig.TargetWidth;
  int32_t targetHeight = projectConfig.TargetHeight;
  if (in.peek() != EOF) {
    in.read(reinterpret_cast<char *>(&targetWidth), sizeof(targetWidth));
    if (in.gcount() != sizeof(targetWidth)) {
      targetWidth = projectConfig.TargetWidth;
      in.clear();
    }
  }
  if (in.peek() != EOF) {
    in.read(reinterpret_cast<char *>(&targetHeight), sizeof(targetHeight));
    if (in.gcount() != sizeof(targetHeight)) {
      targetHeight = projectConfig.TargetHeight;
      in.clear();
    }
  }
  projectConfig.TargetWidth = targetWidth;
  projectConfig.TargetHeight = targetHeight;

  return projectConfig;
}
} // namespace Hamster
