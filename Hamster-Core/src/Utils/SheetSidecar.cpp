#include "HamsterPCH.h"

#include "SheetSidecar.h"

#include <fstream>
#include <sstream>

namespace Hamster {

std::filesystem::path SheetSidecar::SidecarPath(
    const std::filesystem::path &assetPath) {
    return std::filesystem::path(assetPath.string() + ".sheet");
}

bool SheetSidecar::Write(const std::filesystem::path &assetPath,
                         const std::vector<SubSpriteEntry> &entries) {
    std::filesystem::path sidecar = SidecarPath(assetPath);

    std::ofstream out(sidecar, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "SheetSidecar: failed to write " << sidecar << std::endl;
        return false;
    }

    out << "# Hamster spritesheet sidecar — one sub-sprite per line\n";
    for (const auto &e : entries) {
        out << "{\"uuid\":\""
            << boost::uuids::to_string(e.uuid.GetUUID())
            << "\",\"name\":\"" << e.name
            << "\",\"rect\":[" << e.pixelRect.x << "," << e.pixelRect.y
            << "," << e.pixelRect.z << "," << e.pixelRect.w
            << "]}\n";
    }
    return out.good();
}

namespace {
// Tiny key=value extractor — finds `"key":"value"` and returns value (sans
// quotes). Returns empty optional on miss. Tolerant of whitespace but not
// of escaped quotes inside the value (which we never write).
static std::optional<std::string> ReadStringField(const std::string &line,
                                                  const std::string &key) {
    std::string needle = "\"" + key + "\"";
    auto k = line.find(needle);
    if (k == std::string::npos) return std::nullopt;
    auto colon = line.find(':', k);
    if (colon == std::string::npos) return std::nullopt;
    auto open = line.find('"', colon);
    if (open == std::string::npos) return std::nullopt;
    auto close = line.find('"', open + 1);
    if (close == std::string::npos) return std::nullopt;
    return line.substr(open + 1, close - open - 1);
}

// Extracts the rect array `"rect":[a,b,c,d]` into four ints. Returns false
// on any parse failure (caller falls back to skipping the line).
static bool ReadRectField(const std::string &line, glm::ivec4 &out) {
    auto k = line.find("\"rect\"");
    if (k == std::string::npos) return false;
    auto open = line.find('[', k);
    if (open == std::string::npos) return false;
    auto close = line.find(']', open);
    if (close == std::string::npos) return false;
    std::string body = line.substr(open + 1, close - open - 1);
    std::stringstream ss(body);
    char comma;
    if (!(ss >> out.x)) return false;
    if (!(ss >> comma) || comma != ',') return false;
    if (!(ss >> out.y)) return false;
    if (!(ss >> comma) || comma != ',') return false;
    if (!(ss >> out.z)) return false;
    if (!(ss >> comma) || comma != ',') return false;
    if (!(ss >> out.w)) return false;
    return true;
}
} // namespace

bool SheetSidecar::Read(const std::filesystem::path &assetPath,
                        std::vector<SubSpriteEntry> &out) {
    std::filesystem::path sidecar = SidecarPath(assetPath);
    std::ifstream in(sidecar);
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        // Strip CR if present (Windows line endings).
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // Skip blanks + comments.
        if (line.empty()) continue;
        auto firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos) continue;
        if (line[firstNonSpace] == '#') continue;

        auto uuidStr = ReadStringField(line, "uuid");
        auto name = ReadStringField(line, "name");
        glm::ivec4 rect{0, 0, 0, 0};
        bool gotRect = ReadRectField(line, rect);
        if (!uuidStr || !name || !gotRect) {
            std::cerr << "SheetSidecar: malformed line skipped: " << line
                      << std::endl;
            continue;
        }

        try {
            SubSpriteEntry entry;
            entry.uuid = UUID(*uuidStr);
            entry.name = *name;
            entry.pixelRect = rect;
            out.push_back(std::move(entry));
        } catch (const std::exception &e) {
            std::cerr << "SheetSidecar: malformed UUID in " << sidecar
                      << " line skipped: " << e.what() << std::endl;
        }
    }
    return true;
}

} // namespace Hamster
