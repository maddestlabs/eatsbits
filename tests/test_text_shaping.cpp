#include "eatsbits/text/embedded_fonts.hpp"
#include "eatsbits/text/font_manager.hpp"
#include "eatsbits/text/text_shaper.hpp"
#include "eatsbits/text/glyph_atlas.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::abort(); \
    } \
} while (0)

using namespace eatsbits::text;

static void testFreeTypeInitAndFontLoading() {
    std::cout << "[Test] FreeType initialization & memory font loading..." << std::endl;
    FT_Library ft = nullptr;
    FT_Error err = FT_Init_FreeType(&ft);
    TEST_ASSERT(err == 0 && "FT_Init_FreeType failed");
    TEST_ASSERT(ft != nullptr);

    FT_Face face = nullptr;
    err = FT_New_Memory_Face(ft, kShareTechMonoTtf, static_cast<FT_Long>(kShareTechMonoTtfSize), 0, &face);
    TEST_ASSERT(err == 0 && "FT_New_Memory_Face failed with embedded TTF font");
    TEST_ASSERT(face != nullptr);

    err = FT_Set_Pixel_Sizes(face, 0, 16);
    TEST_ASSERT(err == 0 && "FT_Set_Pixel_Sizes failed");

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    std::cout << "  Passed FreeType loading." << std::endl;
}

static void testHarfBuzzShaping() {
    std::cout << "[Test] HarfBuzz buffer creation & text shaping..." << std::endl;
    FT_Library ft = nullptr;
    FT_Error err = FT_Init_FreeType(&ft);
    TEST_ASSERT(err == 0);

    FT_Face face = nullptr;
    err = FT_New_Memory_Face(ft, kShareTechMonoTtf, static_cast<FT_Long>(kShareTechMonoTtfSize), 0, &face);
    TEST_ASSERT(err == 0);
    err = FT_Set_Pixel_Sizes(face, 0, 16);
    TEST_ASSERT(err == 0);

    hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
    TEST_ASSERT(hbFont != nullptr && "hb_ft_font_create failed");

    hb_buffer_t* buf = hb_buffer_create();
    TEST_ASSERT(buf != nullptr && "hb_buffer_create failed");
    TEST_ASSERT(hb_buffer_allocation_successful(buf));

    // Test 1: Shape "!="
    {
        hb_buffer_reset(buf);
        hb_buffer_add_utf8(buf, "!=", -1, 0, -1);
        hb_buffer_guess_segment_properties(buf);
        hb_shape(hbFont, buf, nullptr, 0);

        unsigned int glyphCount = 0;
        hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
        hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

        TEST_ASSERT(glyphCount >= 1 && "Shaping '!=' yielded 0 glyphs");
        TEST_ASSERT(glyphInfo != nullptr);
        TEST_ASSERT(glyphPos != nullptr);

        for (unsigned int i = 0; i < glyphCount; ++i) {
            TEST_ASSERT(glyphInfo[i].codepoint > 0 && "Glyph index should be valid (> 0)");
            TEST_ASSERT(glyphPos[i].x_advance > 0 && "Monospace glyph x_advance should be > 0");
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

        TEST_ASSERT(glyphCount >= 1 && "Shaping '->' yielded 0 glyphs");
        for (unsigned int i = 0; i < glyphCount; ++i) {
            TEST_ASSERT(glyphInfo[i].codepoint > 0);
            TEST_ASSERT(glyphPos[i].x_advance > 0);
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

        TEST_ASSERT(glyphCount == text.size() && "Glyph count for ASCII monospace should match length");
        TEST_ASSERT(glyphInfo != nullptr);
        TEST_ASSERT(glyphPos != nullptr);

        // Verify uniform advances
        int32_t firstAdv = glyphPos[0].x_advance;
        TEST_ASSERT(firstAdv > 0);
        for (unsigned int i = 1; i < glyphCount; ++i) {
            TEST_ASSERT(glyphPos[i].x_advance == firstAdv && "Monospace font advances must be uniform");
        }
        std::cout << "  Shaped '" << text << "' with uniform monospace advance " << firstAdv << "." << std::endl;
    }

    hb_buffer_destroy(buf);
    hb_font_destroy(hbFont);
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
    std::cout << "  Passed HarfBuzz shaping." << std::endl;
}

static void testFontManager() {
    std::cout << "[Test] FontManager loading & fallback..." << std::endl;
    FontManager mgr;
    TEST_ASSERT(mgr.initialize());
    TEST_ASSERT(mgr.isInitialized());
    TEST_ASSERT(mgr.getFtLibrary() != nullptr);

    // Initial state
    TEST_ASSERT(mgr.getLoadedFontCount() == 0);
    TEST_ASSERT(mgr.getFontId("nonexistent") == kInvalidFontId);

    // Load embedded fallback
    FontId id1 = mgr.loadEmbeddedFallback("mono_primary");
    TEST_ASSERT(id1 != kInvalidFontId);
    TEST_ASSERT(mgr.getLoadedFontCount() == 1);
    TEST_ASSERT(mgr.getFontId("mono_primary") == id1);

    const FontFaceInfo* info = mgr.getFontInfo(id1);
    TEST_ASSERT(info != nullptr);
    TEST_ASSERT(info->name == "mono_primary");
    TEST_ASSERT(info->ftFace != nullptr);
    TEST_ASSERT(info->hbFont != nullptr);

    // Load another instance copying data
    FontId id2 = mgr.loadFontFromMemory("mono_secondary", kShareTechMonoTtf, kShareTechMonoTtfSize, true);
    TEST_ASSERT(id2 != kInvalidFontId);
    TEST_ASSERT(id2 != id1);
    TEST_ASSERT(mgr.getLoadedFontCount() == 2);

    // Test fallback chaining
    TEST_ASSERT(mgr.setFallbackFont(id1, id2));
    TEST_ASSERT(mgr.getFallbackFont(id1) == id2);
    TEST_ASSERT(mgr.getFallbackFont(id2) == kInvalidFontId);

    std::cout << "  FontManager loaded 2 fonts, chained fallback, verified handles." << std::endl;
}

static void testTextShaper() {
    std::cout << "[Test] TextShaper shaped output & advance calculations..." << std::endl;
    FontManager mgr;
    TEST_ASSERT(mgr.initialize());
    FontId fontId = mgr.loadEmbeddedFallback("mono");
    TEST_ASSERT(fontId != kInvalidFontId);

    TextShaper shaper;
    const std::string text = "Hello, World!";
    ShapedRun run;
    bool ok = shaper.shapeText(mgr, fontId, 16.0f, text, run);
    TEST_ASSERT(ok);

    TEST_ASSERT(run.glyphs.size() == text.size());
    TEST_ASSERT(run.totalAdvanceX > 0.0f);

    for (size_t i = 0; i < run.glyphs.size(); ++i) {
        TEST_ASSERT(run.glyphs[i].glyphIndex > 0);
        TEST_ASSERT(run.glyphs[i].xAdvance > 0.0f);
        TEST_ASSERT(run.glyphs[i].cluster == i);
    }

    // Verify measurement utility
    float measuredW = shaper.measureWidth(mgr, fontId, 16.0f, text);
    TEST_ASSERT(std::abs(measuredW - run.totalAdvanceX) < 0.001f);

    // Test shaping empty text
    ShapedRun emptyRun;
    bool emptyOk = shaper.shapeText(mgr, fontId, 16.0f, "", emptyRun);
    TEST_ASSERT(!emptyOk);
    TEST_ASSERT(emptyRun.glyphs.empty());
    TEST_ASSERT(emptyRun.totalAdvanceX == 0.0f);

    std::cout << "  TextShaper shaped '" << text << "' width=" << run.totalAdvanceX << " px." << std::endl;
}

static void testGlyphAtlas() {
    std::cout << "[Test] GlyphAtlas shelf packing, rasterization, subpixel bins & caching..." << std::endl;
    FontManager mgr;
    TEST_ASSERT(mgr.initialize());
    FontId fontId = mgr.loadEmbeddedFallback("mono");
    TEST_ASSERT(fontId != kInvalidFontId);

    GlyphAtlas atlas(512, 512);
    TEST_ASSERT(atlas.getWidth() == 512);
    TEST_ASSERT(atlas.getHeight() == 512);
    TEST_ASSERT(!atlas.isDirty());

    // Rasterize character 'A' (FT_Get_Char_Index)
    FT_Face face = mgr.getFtFace(fontId);
    TEST_ASSERT(face != nullptr);
    uint32_t glyphA = FT_Get_Char_Index(face, 'A');
    TEST_ASSERT(glyphA > 0);

    // 1. First fetch (subpixelX = 0.0f -> bin 0)
    const AtlasGlyph* g0 = atlas.getOrLoadGlyph(mgr, fontId, glyphA, 20.0f, 0.0f);
    TEST_ASSERT(g0 != nullptr);
    TEST_ASSERT(g0->width > 0);
    TEST_ASSERT(g0->height > 0);
    TEST_ASSERT(g0->u0 >= 0.0f && g0->u1 <= 1.0f);
    TEST_ASSERT(g0->v0 >= 0.0f && g0->v1 <= 1.0f);
    TEST_ASSERT(g0->u1 > g0->u0);
    TEST_ASSERT(g0->v1 > g0->v0);
    TEST_ASSERT(atlas.isDirty());
    TEST_ASSERT(atlas.getCacheMisses() == 1);
    TEST_ASSERT(atlas.getCacheHits() == 0);

    // Check dirty bounds
    int dx = 0, dy = 0, dw = 0, dh = 0;
    atlas.getDirtyBounds(dx, dy, dw, dh);
    TEST_ASSERT(dw >= g0->width);
    TEST_ASSERT(dh >= g0->height);
    atlas.clearDirty();
    TEST_ASSERT(!atlas.isDirty());

    // 2. Fetch same glyph with same subpixel bin -> must be cache hit!
    const AtlasGlyph* g0_cached = atlas.getOrLoadGlyph(mgr, fontId, glyphA, 20.0f, 0.05f); // 0.05 rounds to bin 0
    TEST_ASSERT(g0_cached == g0);
    TEST_ASSERT(atlas.getCacheHits() == 1);
    TEST_ASSERT(atlas.getCacheMisses() == 1);
    TEST_ASSERT(!atlas.isDirty()); // Cache hit should not dirty atlas texture

    // 3. Fetch with subpixel bin 1 (0.25f) -> cache miss, distinct rasterization
    const AtlasGlyph* g1 = atlas.getOrLoadGlyph(mgr, fontId, glyphA, 20.0f, 0.25f);
    TEST_ASSERT(g1 != nullptr);
    TEST_ASSERT(atlas.getCacheMisses() == 2);
    TEST_ASSERT(atlas.isDirty());
    TEST_ASSERT(g1->atlasX != g0->atlasX || g1->atlasY != g0->atlasY);

    // 4. Fetch with subpixel bin 2 (0.50f)
    const AtlasGlyph* g2 = atlas.getOrLoadGlyph(mgr, fontId, glyphA, 20.0f, 0.50f);
    TEST_ASSERT(g2 != nullptr);
    TEST_ASSERT(atlas.getCacheMisses() == 3);

    // 5. Fetch with subpixel bin 3 (0.75f)
    const AtlasGlyph* g3 = atlas.getOrLoadGlyph(mgr, fontId, glyphA, 20.0f, 0.75f);
    TEST_ASSERT(g3 != nullptr);
    TEST_ASSERT(atlas.getCacheMisses() == 4);

    TEST_ASSERT(atlas.getCachedGlyphCount() == 4);
    std::cout << "  GlyphAtlas cached " << atlas.getCachedGlyphCount()
              << " subpixel variations with hits=" << atlas.getCacheHits()
              << ", misses=" << atlas.getCacheMisses() << "." << std::endl;
}

int main() {
    std::cout << "===========================================" << std::endl;
    std::cout << " Running FreeType & HarfBuzz Smoke Test    " << std::endl;
    std::cout << "===========================================" << std::endl;

    testFreeTypeInitAndFontLoading();
    testHarfBuzzShaping();
    testFontManager();
    testTextShaper();
    testGlyphAtlas();

    std::cout << "===========================================" << std::endl;
    std::cout << " [PASS] All FreeType + HarfBuzz Tests OK   " << std::endl;
    std::cout << "===========================================" << std::endl;
    return 0;
}
