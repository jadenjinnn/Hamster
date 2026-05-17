#include "HamsterPCH.h"

#include "MetaFile.h"

#include <fstream>

namespace Hamster {
std::filesystem::path
MetaFile::SidecarPath(const std::filesystem::path &assetPath) {
    return std::filesystem::path(assetPath.string() + ".meta");
}

bool MetaFile::Write(const std::filesystem::path &assetPath, const UUID &uuid) {
    std::filesystem::path sidecar = SidecarPath(assetPath);

    std::ofstream out(sidecar, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "MetaFile: failed to write " << sidecar << std::endl;
        return false;
    }

    // Hand-rolled JSON — single field. boost UUIDs already stringify to the
    // canonical 8-4-4-4-12 hex form, no escaping needed.
    out << "{\"uuid\":\"" << boost::uuids::to_string(uuid.GetUUID()) << "\"}\n";
    return out.good();
}

std::optional<UUID> MetaFile::Read(const std::filesystem::path &assetPath) {
    std::filesystem::path sidecar = SidecarPath(assetPath);

    std::ifstream in(sidecar);
    if (!in.is_open()) {
        return std::nullopt;
    }

    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    // Look for "uuid":"..." — tolerant of whitespace. We deliberately don't
    // bring in a JSON parser for a single field; if the schema grows, swap to
    // a proper parser then.
    auto keyPos = contents.find("\"uuid\"");
    if (keyPos == std::string::npos) return std::nullopt;

    auto colonPos = contents.find(':', keyPos);
    if (colonPos == std::string::npos) return std::nullopt;

    auto firstQuote = contents.find('"', colonPos);
    if (firstQuote == std::string::npos) return std::nullopt;

    auto secondQuote = contents.find('"', firstQuote + 1);
    if (secondQuote == std::string::npos) return std::nullopt;

    std::string uuidStr = contents.substr(firstQuote + 1,
                                          secondQuote - firstQuote - 1);

    try {
        return UUID(uuidStr);
    } catch (const std::exception &e) {
        std::cerr << "MetaFile: malformed UUID in " << sidecar << ": "
                  << e.what() << std::endl;
        return std::nullopt;
    }
}
} // namespace Hamster
