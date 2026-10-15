#include "eatsbits/text/text_shaper.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace eatsbits::text {

TextShaper::TextShaper() {
    hbBuffer_ = hb_buffer_create();
    hbFallbackBuffer_ = hb_buffer_create();
}

TextShaper::~TextShaper() {
    if (hbBuffer_) {
        hb_buffer_destroy(hbBuffer_);
        hbBuffer_ = nullptr;
    }
    if (hbFallbackBuffer_) {
        hb_buffer_destroy(hbFallbackBuffer_);
        hbFallbackBuffer_ = nullptr;
    }
}

TextShaper::TextShaper(TextShaper&& o) noexcept {
    hbBuffer_ = o.hbBuffer_;
    o.hbBuffer_ = nullptr;
    hbFallbackBuffer_ = o.hbFallbackBuffer_;
    o.hbFallbackBuffer_ = nullptr;
}

TextShaper& TextShaper::operator=(TextShaper&& o) noexcept {
    if (this != &o) {
        if (hbBuffer_) {
            hb_buffer_destroy(hbBuffer_);
        }
        if (hbFallbackBuffer_) {
            hb_buffer_destroy(hbFallbackBuffer_);
        }
        hbBuffer_ = o.hbBuffer_;
        o.hbBuffer_ = nullptr;
        hbFallbackBuffer_ = o.hbFallbackBuffer_;
        o.hbFallbackBuffer_ = nullptr;
    }
    return *this;
}

bool TextShaper::shapeText(FontManager& fontMgr, FontId fontId, float fontSize,
                           std::string_view utf8Text, ShapedRun& outRun,
                           const hb_feature_t* features, unsigned int numFeatures) {
    if (!hbBuffer_ || utf8Text.empty() || fontId == kInvalidFontId || fontSize <= 0.0f) {
        outRun.clear();
        outRun.fontId = fontId;
        outRun.fontSize = fontSize;
        return false;
    }

    FT_Face face = fontMgr.getFtFace(fontId);
    hb_font_t* hbFont = fontMgr.getHbFont(fontId);
    if (!face || !hbFont) {
        outRun.clear();
        return false;
    }

    // Set pixel size and notify HarfBuzz
    FT_UInt pixelSize = static_cast<FT_UInt>(std::max(1, static_cast<int>(std::round(fontSize))));
    FT_Set_Pixel_Sizes(face, 0, pixelSize);
    hb_ft_font_changed(hbFont);

    // Reuse HarfBuzz buffer without reallocation
    hb_buffer_reset(hbBuffer_);
    hb_buffer_add_utf8(hbBuffer_, utf8Text.data(), static_cast<int>(utf8Text.size()), 0, static_cast<int>(utf8Text.size()));
    hb_buffer_set_direction(hbBuffer_, HB_DIRECTION_LTR);
    hb_buffer_set_script(hbBuffer_, HB_SCRIPT_LATIN);
    hb_buffer_set_language(hbBuffer_, hb_language_from_string("en", -1));

    // Shape with features
    hb_shape(hbFont, hbBuffer_, features, numFeatures);

    unsigned int glyphCount = 0;
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(hbBuffer_, &glyphCount);
    hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(hbBuffer_, &glyphCount);

    outRun.fontId = fontId;
    outRun.fontSize = fontSize;
    outRun.glyphs.clear();
    outRun.glyphs.reserve(glyphCount);
    outRun.totalAdvanceX = 0.0f;
    outRun.totalAdvanceY = 0.0f;

    FontId fallbackId = fontMgr.getFallbackFont(fontId);
    FT_Face fallbackFace = fallbackId != kInvalidFontId ? fontMgr.getFtFace(fallbackId) : nullptr;
    hb_font_t* fallbackHbFont = fallbackId != kInvalidFontId ? fontMgr.getHbFont(fallbackId) : nullptr;

    if (fallbackFace && fallbackHbFont) {
        FT_Set_Pixel_Sizes(fallbackFace, 0, pixelSize);
        hb_ft_font_changed(fallbackHbFont);
    }

    for (unsigned int i = 0; i < glyphCount; ++i) {
        uint32_t cp = glyphInfo[i].codepoint;
        FontId activeFont = fontId;
        float xOff = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        float yOff = static_cast<float>(glyphPos[i].y_offset) / 64.0f;
        float xAdv = static_cast<float>(glyphPos[i].x_advance) / 64.0f;
        float yAdv = static_cast<float>(glyphPos[i].y_advance) / 64.0f;

        // Fallback resolution for missing glyph (.notdef)
        if (cp == 0 && fallbackHbFont) {
            uint32_t clusterStart = glyphInfo[i].cluster;
            uint32_t clusterEnd = (i + 1 < glyphCount) ? glyphInfo[i + 1].cluster : static_cast<uint32_t>(utf8Text.size());
            if (clusterEnd > clusterStart && clusterEnd <= utf8Text.size()) {
                std::string_view clusterSlice = utf8Text.substr(clusterStart, clusterEnd - clusterStart);

                hb_buffer_reset(hbFallbackBuffer_);
                hb_buffer_add_utf8(hbFallbackBuffer_, clusterSlice.data(), static_cast<int>(clusterSlice.size()), 0, static_cast<int>(clusterSlice.size()));
                hb_buffer_set_direction(hbFallbackBuffer_, HB_DIRECTION_LTR);
                hb_buffer_set_script(hbFallbackBuffer_, HB_SCRIPT_LATIN);
                hb_buffer_set_language(hbFallbackBuffer_, hb_language_from_string("en", -1));
                hb_shape(fallbackHbFont, hbFallbackBuffer_, features, numFeatures);

                unsigned int fbCount = 0;
                hb_glyph_info_t* fbInfo = hb_buffer_get_glyph_infos(hbFallbackBuffer_, &fbCount);
                hb_glyph_position_t* fbPos = hb_buffer_get_glyph_positions(hbFallbackBuffer_, &fbCount);

                for (unsigned int f = 0; f < fbCount; ++f) {
                    ShapedGlyph g;
                    g.glyphIndex = fbInfo[f].codepoint;
                    g.cluster = clusterStart + fbInfo[f].cluster;
                    g.xOffset = static_cast<float>(fbPos[f].x_offset) / 64.0f;
                    g.yOffset = static_cast<float>(fbPos[f].y_offset) / 64.0f;
                    g.xAdvance = static_cast<float>(fbPos[f].x_advance) / 64.0f;
                    g.yAdvance = static_cast<float>(fbPos[f].y_advance) / 64.0f;
                    g.fontId = fallbackId;

                    outRun.totalAdvanceX += g.xAdvance;
                    outRun.totalAdvanceY += g.yAdvance;
                    outRun.glyphs.push_back(g);
                }
                continue;
            }
        }

        ShapedGlyph g;
        g.glyphIndex = cp;
        g.cluster = glyphInfo[i].cluster;
        g.xOffset = xOff;
        g.yOffset = yOff;
        g.xAdvance = xAdv;
        g.yAdvance = yAdv;
        g.fontId = activeFont;

        outRun.totalAdvanceX += xAdv;
        outRun.totalAdvanceY += yAdv;
        outRun.glyphs.push_back(g);
    }

    return true;
}

float TextShaper::measureWidth(FontManager& fontMgr, FontId fontId, float fontSize, std::string_view utf8Text) {
    ShapedRun run;
    if (shapeText(fontMgr, fontId, fontSize, utf8Text, run)) {
        return run.totalAdvanceX;
    }
    return 0.0f;
}

} // namespace eatsbits::text
