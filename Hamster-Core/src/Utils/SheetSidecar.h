#pragma once

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "Core/UUID.h"

namespace Hamster {
// Spritesheet sidecar — sits next to a .png as "<asset>.png.sheet". Holds
// the list of sub-sprite regions cut from the parent texture. One
// SubSpriteEntry per line (NDJSON), hand-rolled for the same reason
// MetaFile is hand-rolled — single tiny format, no JSON dependency.
//
// On-disk format:
//   {"uuid":"01234567-...","name":"bird_atlas_0","rect":[0,0,32,32]}
//   {"uuid":"...","name":"wing_up","rect":[32,0,32,32]}
//
// Blank lines and lines beginning with '#' are tolerated.
struct SubSpriteEntry {
    UUID uuid;
    std::string name;
    glm::ivec4 pixelRect;
};

struct SheetSidecar {
    // Returns "<assetPath>.sheet". The full sidecar filename for "foo.png"
    // is "foo.png.sheet", paralleling MetaFile's "foo.png.meta".
    static std::filesystem::path SidecarPath(
        const std::filesystem::path &assetPath);

    // Writes the sidecar next to the asset. Overwrites any existing sidecar.
    // Returns true on success.
    static bool Write(const std::filesystem::path &assetPath,
                      const std::vector<SubSpriteEntry> &entries);

    // Reads + parses the sidecar. Returns true with entries populated on
    // success; false if the sidecar is missing or unparseable (caller
    // decides whether to log + skip).
    static bool Read(const std::filesystem::path &assetPath,
                     std::vector<SubSpriteEntry> &out);
};
} // namespace Hamster
