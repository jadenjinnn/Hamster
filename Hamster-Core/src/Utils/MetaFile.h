#pragma once

#include <filesystem>
#include <optional>

#include "Core/UUID.h"

namespace Hamster {
// Sidecar metadata file format. One file per asset, named "<asset>.meta",
// living next to its asset on disk. Holds the asset's stable UUID so the
// identity survives the file being renamed via the editor.
//
// On-disk format is a tiny JSON document, intentionally hand-rolled so we
// don't pull in a JSON dependency for one field:
//     {"uuid":"01234567-89ab-cdef-0123-456789abcdef"}
struct MetaFile {
    // Returns the conventional sidecar path: "<assetPath>.meta".
    static std::filesystem::path SidecarPath(const std::filesystem::path &assetPath);

    // Writes the sidecar next to the asset. Overwrites any existing sidecar.
    // Returns true on success.
    static bool Write(const std::filesystem::path &assetPath, const UUID &uuid);

    // Reads the sidecar next to the asset. Returns nullopt if the sidecar is
    // missing or malformed (caller decides whether to mint a fresh UUID).
    static std::optional<UUID> Read(const std::filesystem::path &assetPath);
};
} // namespace Hamster
