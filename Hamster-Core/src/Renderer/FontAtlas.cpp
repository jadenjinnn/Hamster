#include "HamsterPCH.h"

#include "FontAtlas.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include <glad/glad.h>

// Single-translation-unit definition of stb_truetype. Other TUs that need
// the bakedchar struct can forward-declare it (as FontAtlas.h does).
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace Hamster {

FontAtlas::FontAtlas(const std::string &ttfPath) {
    std::ifstream in(ttfPath, std::ios::binary | std::ios::ate);
    if (!in) {
        std::cerr << "[FontAtlas] failed to open " << ttfPath << std::endl;
        return;
    }
    std::streamsize sz = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<unsigned char> ttfData(static_cast<size_t>(sz));
    if (!in.read(reinterpret_cast<char *>(ttfData.data()), sz)) {
        std::cerr << "[FontAtlas] failed to read " << ttfPath << std::endl;
        return;
    }

    // Get ascent for baseline positioning. stbtt_BakeFontBitmap doesn't
    // surface vMetrics, so initialise the font separately for them.
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttfData.data(),
                        stbtt_GetFontOffsetForIndex(ttfData.data(), 0))) {
        std::cerr << "[FontAtlas] stbtt_InitFont failed" << std::endl;
        return;
    }
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&info, kBakeSize);
    m_BakedAscentPx = static_cast<float>(ascent) * scale;

    // Bake atlas. The bitmap is R8 (one byte per pixel = coverage).
    std::vector<unsigned char> bitmap(kAtlasW * kAtlasH, 0);
    m_Chars.resize(kNumChars);
    int bakeRes = stbtt_BakeFontBitmap(
        ttfData.data(), 0, kBakeSize, bitmap.data(),
        kAtlasW, kAtlasH, kFirstChar, kNumChars, m_Chars.data());
    if (bakeRes <= 0) {
        // Negative = fit but not all glyphs; 0 = nothing fit. Either way the
        // atlas isn't trustworthy.
        std::cerr << "[FontAtlas] BakeFontBitmap returned " << bakeRes
                  << "; some glyphs may be missing" << std::endl;
        if (bakeRes == 0) {
            m_Chars.clear();
            return;
        }
        // bakeRes < 0 → continue; partially-baked atlas is still useful for
        // most ASCII text.
    }

    // Upload as a single-channel GL texture. The text shader samples .r and
    // multiplies by the tint colour.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &m_TextureId);
    glBindTexture(GL_TEXTURE_2D, m_TextureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                 kAtlasW, kAtlasH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

FontAtlas::~FontAtlas() {
    if (m_TextureId) {
        glDeleteTextures(1, &m_TextureId);
        m_TextureId = 0;
    }
}

const stbtt_bakedchar *FontAtlas::GetGlyph(char c) const {
    if (m_Chars.empty()) return nullptr;
    int idx = static_cast<int>(static_cast<unsigned char>(c)) - kFirstChar;
    if (idx < 0 || idx >= kNumChars) return nullptr;
    return &m_Chars[static_cast<size_t>(idx)];
}

float FontAtlas::MeasureWidth(const std::string &text, float fontSize) const {
    if (m_Chars.empty()) return 0.0f;
    const float scale = fontSize / kBakeSize;
    float width = 0.0f;
    for (char c : text) {
        const stbtt_bakedchar *bc = GetGlyph(c);
        if (!bc) bc = GetGlyph('?');
        if (!bc) continue;
        width += bc->xadvance * scale;
    }
    return width;
}

} // namespace Hamster
