#include "eatsbits/text/embedded_fonts.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

using namespace eatsbits::text;

static void testFreeTypeInitAndFontLoading() {
    std::cout << "[Test] FreeType initialization & memory font loading..." << std::endl;
    FT_Library ft = nullptr;
    FT_Error err = FT_Init_FreeType(&ft);
    assert(err == 0 && "FT_Init_FreeType failed");
    assert(ft != nullptr);

    FT_Face face = nullptr;
    err = FT_New_Memory_Face(ft, kShareTechMonoTtf, static_cast<FT_Long>(kShareTechMonoTtfSize), 0, &face);
    assert(err == 0 && "FT_New_Memory_Face failed with embedded TTF font");
    assert(face != nullptr);

    err = FT_Set_Pixel_Sizes(face, 0, 16);
    assert(err == 0 && "FT_Set_Pixel_Sizes failed");

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    std::cout << "  Passed FreeType loading." << std::endl;
}

static void testHarfBuzzShaping() {
    std::cout << "[Test] HarfBuzz buffer creation & text shaping..." << std::endl;
    FT_Library ft = nullptr;
    FT_Error err = FT_Init_FreeType(&ft);
    assert(err == 0);

    FT_Face face = nullptr;
    err = FT_New_Memory_Face(ft, kShareTechMonoTtf, static_cast<FT_Long>(kShareTechMonoTtfSize), 0, &face);
    assert(err == 0);
    err = FT_Set_Pixel_Sizes(face, 0, 16);
    assert(err == 0);

    hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
    assert(hbFont != nullptr && "hb_ft_font_create failed");

    hb_buffer_t* buf = hb_buffer_create();
    assert(buf != nullptr && "hb_buffer_create failed");
    assert(hb_buffer_allocation_successful(buf));

    // Test 1: Shape "!="
    {
        hb_buffer_reset(buf);
        hb_buffer_add_utf8(buf, "!=", -1, 0, -1);
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hbFont, buf, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

        assert(glyphCount >= 1 && "Shaping '!=' yielded 0 glyphs");
        assert(glyphInfo != nullptr);
        assert(glyphPos != nullptr);

        for (unsigned int i = 0; i < glyphCount; ++i) {
            assert(glyphInfo[i].codepoint > 0 && "Glyph index should be valid (> 0)");
            assert(glyphPos[i].x_advance > 0 && "Monospace glyph x_advance should be > 0");
        }
        std::cout << "  Shaped '!=' into " << glyphCount << " glyph(s)." << std::endl;
    }

    // Test 2: Shape "->"
    {
        hb_buffer_reset(buf);
        hb_buffer_add_utf8(buf, "->", -1, 0, -1);
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hbFont, buf, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

        assert(glyphCount >= 1 && "Shaping '->' yielded 0 glyphs");
        for (unsigned int i = 0; i < glyphCount; ++i) {
            assert(glyphInfo[i].codepoint > 0);
            assert(glyphPos[i].x_advance > 0);
        }
        std::cout << "  Shaped '->' into " << glyphCount << " glyph(s)." << std::endl;
    }

    // Test 3: Shape standard string "Eatsbits 2026"
    {
        const std::string text = "Eatsbits 2026";
        hb_buffer_reset(buf);
        hb_buffer_add_utf8(buf, text.c_str(), static_cast<int>(text.size()), 0, -1);
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hbFont, buf, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

        assert(glyphCount == text.size() && "Glyph count for ASCII monospace should match length");
        assert(glyphInfo != nullptr);
        assert(glyphPos != nullptr);

        // Verify uniform advances
        int32_t firstAdv = glyphPos[0].x_advance;
        assert(firstAdv > 0);
        for (unsigned int i = 1; i < glyphCount; ++i) {
            assert(glyphPos[i].x_advance == firstAdv && "Monospace font advances must be uniform");
        }
        std::cout << "  Shaped '" << text << "' with uniform monospace advance " << firstAdv << "." << std::endl;
    }

    hb_buffer_destroy(buf);
    hb_font_destroy(hbFont);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    std::cout << "  Passed HarfBuzz shaping." << std::endl;
}

int main() {
    std::cout << "===========================================" << std::endl;
    std::cout << " Running FreeType & HarfBuzz Smoke Test    " << std::endl;
    std::cout << "===========================================" << std::endl;

    testFreeTypeInitAndFontLoading();
    testHarfBuzzShaping();

    std::cout << "===========================================" << std::endl;
    std::cout << " [PASS] All FreeType + HarfBuzz Tests OK   " << std::endl;
    std::cout << "===========================================" << std::endl;
    return 0;
}
