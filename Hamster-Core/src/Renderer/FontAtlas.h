#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

// Pull in stb_truetype declarations only (the implementation is gated behind
// STB_TRUETYPE_IMPLEMENTATION which is defined exactly once, in FontAtlas.cpp).
#include <stb_truetype.h>

namespace Hamster {
// Single-font bitmap atlas baked once at startup. ASCII 32–126 only (95
// glyphs) — game-ui Phase B scope. Non-printable chars and codepoints
// outside that range fall back to '?' with a one-time warning logged from
// the renderer's text-submit path.
//
// One bake size (kBakeSize px). Runtime fontSize scales the rendered quads;
// at extreme zoom-outs the bitmap blurs. Multi-bake or SDF is future work.
class FontAtlas {
public:
    static constexpr int   kFirstChar = 32;
    static constexpr int   kNumChars  = 95;     // 32..126 inclusive
    // Bake at 64px into a 1024x1024 atlas: text renders crisp up to ~64px
    // (font sizes above this magnify the bitmap and soften). Raise kBakeSize
    // for larger crisp text — keep the atlas big enough to fit all glyphs.
    static constexpr int   kAtlasW    = 1024;
    static constexpr int   kAtlasH    = 1024;
    static constexpr float kBakeSize  = 64.0f;

    explicit FontAtlas(const std::string &ttfPath);
    ~FontAtlas();

    FontAtlas(const FontAtlas &) = delete;
    FontAtlas &operator=(const FontAtlas &) = delete;

    bool IsValid() const { return m_TextureId != 0; }
    unsigned int GetTextureId() const { return m_TextureId; }

    // Glyph lookup. Returns nullptr if c is outside the baked range; caller
    // should substitute the placeholder glyph (typically '?').
    const stbtt_bakedchar *GetGlyph(char c) const;

    // Width-only measurement at the given font size; height is just
    // `fontSize` (single-line). Non-printable / out-of-range chars are
    // treated as '?' for measurement.
    float MeasureWidth(const std::string &text, float fontSize) const;

    // Ascent in pixels at the baked size. Multiply by fontSize / kBakeSize
    // to position the baseline below a top-left text origin.
    float BakedAscentPx() const { return m_BakedAscentPx; }

    // Convert a top-left Y at the runtime fontSize to a baseline Y. Used by
    // the renderer when laying out glyphs.
    float BaselineYFromTop(float topY, float fontSize) const {
        return topY + m_BakedAscentPx * (fontSize / kBakeSize);
    }

private:
    unsigned int m_TextureId = 0;
    std::vector<stbtt_bakedchar> m_Chars;
    float m_BakedAscentPx = 0.0f;
};
} // namespace Hamster
