#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <hb.h>
#include "font_manager.hpp"

namespace eatsbits::text {

struct ShapedGlyph {
    uint32_t glyphIndex{0};
    uint32_t cluster{0}; // UTF-8 byte offset in source string for cursor mapping
    float xOffset{0.0f};
    float yOffset{0.0f};
    float xAdvance{0.0f};
    float yAdvance{0.0f};
    FontId fontId{kInvalidFontId}; // Font ID that provided this glyph (for fallback chains)
};

struct ShapedRun {
    FontId fontId{kInvalidFontId};
    float fontSize{12.0f};
    std::vector<ShapedGlyph> glyphs;
    float totalAdvanceX{0.0f};
    float totalAdvanceY{0.0f};

    void clear() {
        glyphs.clear();
        totalAdvanceX = 0.0f;
        totalAdvanceY = 0.0f;
    }
};

class TextShaper {
public:
    TextShaper();
    ~TextShaper();

    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;
    TextShaper(TextShaper&&) noexcept;
    TextShaper& operator=(TextShaper&&) noexcept;

    /**
     * Shapes UTF-8 text using HarfBuzz and FreeType font scaling.
     * Reuses internal HarfBuzz buffers to ensure zero allocations on steady-state shaping.
     * When glyphs are missing (.notdef / glyphIndex 0) and the font has a fallback configured,
     * seamlessly resolves them using the fallback font.
     */
    bool shapeText(FontManager& fontMgr, FontId fontId, float fontSize,
                   std::string_view utf8Text, ShapedRun& outRun,
                   const hb_feature_t* features = nullptr, unsigned int numFeatures = 0);

    float measureWidth(FontManager& fontMgr, FontId fontId, float fontSize, std::string_view utf8Text);

private:
    hb_buffer_t* hbBuffer_{nullptr};
};

} // namespace eatsbits::text
