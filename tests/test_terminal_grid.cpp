#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>

#include "eatsbits/text/monospace_grid_layout.hpp"
#include "eatsbits/terminal/terminal_grid.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/tui/types.hpp"

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

using namespace eatsbits;
using namespace eatsbits::text;
using namespace eatsbits::terminal;
using namespace eatsbits::ui;

// ============================================================================
// 1. Monospace Ligature Cell Slicing Tests
// ============================================================================
void testLigatureCellSlicing() {
    std::cout << "[Test 1/4] Ligature Cell Slicing (MonospaceGridLayout)..." << std::endl;

    MonospaceGridLayout layout;
    std::vector<GridCellLayout> cells;

    // Test A: 2-character ligature '=='
    layout.layoutWithLigatureRules("==", 10.0f, cells);
    REQUIRE(cells.size() == 2);
    // Primary cell
    REQUIRE(!cells[0].isContinuation);
    REQUIRE(cells[0].colSpan == 2);
    REQUIRE(cells[0].colOffset == 0);
    REQUIRE(cells[0].cluster == 0);
    // Continuation cell
    REQUIRE(cells[1].isContinuation);
    REQUIRE(cells[1].colSpan == 2);
    REQUIRE(cells[1].colOffset == 1);
    REQUIRE(cells[1].cluster == 1);

    // Test B: 3-character ligature '==='
    layout.layoutWithLigatureRules("===", 10.0f, cells);
    REQUIRE(cells.size() == 3);
    REQUIRE(!cells[0].isContinuation);
    REQUIRE(cells[0].colSpan == 3);
    REQUIRE(cells[0].colOffset == 0);
    REQUIRE(cells[1].isContinuation);
    REQUIRE(cells[1].colSpan == 3);
    REQUIRE(cells[1].colOffset == 1);
    REQUIRE(cells[2].isContinuation);
    REQUIRE(cells[2].colSpan == 3);
    REQUIRE(cells[2].colOffset == 2);

    // Test C: 4-character HTML comment ligature '<!--'
    layout.layoutWithLigatureRules("<!--", 10.0f, cells);
    REQUIRE(cells.size() == 4);
    REQUIRE(!cells[0].isContinuation);
    REQUIRE(cells[0].colSpan == 4);
    REQUIRE(cells[0].colOffset == 0);
    for (size_t i = 1; i < 4; ++i) {
        REQUIRE(cells[i].isContinuation);
        REQUIRE(cells[i].colSpan == 4);
        REQUIRE(cells[i].colOffset == static_cast<uint16_t>(i));
    }

    // Test D: Multiple ligatures and identifiers: "if (a == b && c !== d)"
    std::string text = "if (a == b && c !== d)";
    layout.layoutWithLigatureRules(text, 10.0f, cells);
    REQUIRE(cells.size() == text.size());

    // '==' starts at index 6
    REQUIRE(!cells[6].isContinuation);
    REQUIRE(cells[6].colSpan == 2);
    REQUIRE(cells[6].colOffset == 0);
    REQUIRE(cells[7].isContinuation);
    REQUIRE(cells[7].colSpan == 2);
    REQUIRE(cells[7].colOffset == 1);

    // '!==' starts at index 16
    REQUIRE(!cells[16].isContinuation);
    REQUIRE(cells[16].colSpan == 3);
    REQUIRE(cells[16].colOffset == 0);
    REQUIRE(cells[17].isContinuation);
    REQUIRE(cells[17].colSpan == 3);
    REQUIRE(cells[17].colOffset == 1);
    REQUIRE(cells[18].isContinuation);
    REQUIRE(cells[18].colSpan == 3);
    REQUIRE(cells[18].colOffset == 2);

    // Test E: HarfBuzz layoutLine with embedded fallback font
    FontManager fontMgr;
    REQUIRE(fontMgr.initialize());
    FontId fontId = fontMgr.loadEmbeddedFallback();
    REQUIRE(fontId != kInvalidFontId);
    TextShaper shaper;

    layout.layoutLine("alpha beta", fontMgr, fontId, 14.0f, 8.4f, shaper, cells);
    REQUIRE(cells.size() == 10);
    for (size_t i = 0; i < cells.size(); ++i) {
        REQUIRE(!cells[i].isContinuation);
        REQUIRE(cells[i].colSpan == 1);
        REQUIRE(cells[i].glyphIndex != 0);
    }

    // Test F: layoutShapedRun with compound multi-character cluster
    // Simulating HarfBuzz output for '==' collapsed into a single glyph at cluster 0
    ShapedRun compoundRun;
    compoundRun.fontId = fontId;
    compoundRun.fontSize = 14.0f;
    ShapedGlyph ligGlyph;
    ligGlyph.glyphIndex = 999; // Compound ligature glyph
    ligGlyph.cluster = 0;
    ligGlyph.fontId = fontId;
    ligGlyph.xAdvance = 16.8f;
    compoundRun.glyphs.push_back(ligGlyph);

    // Next token starts at cluster 2
    ShapedGlyph nextGlyph;
    nextGlyph.glyphIndex = 65; // 'A'
    nextGlyph.cluster = 2;
    nextGlyph.fontId = fontId;
    nextGlyph.xAdvance = 8.4f;
    compoundRun.glyphs.push_back(nextGlyph);

    layout.layoutShapedRun("==A", compoundRun, 8.4f, cells);
    REQUIRE(cells.size() == 3);
    // Primary cell for '=='
    REQUIRE(!cells[0].isContinuation);
    REQUIRE(cells[0].colSpan == 2);
    REQUIRE(cells[0].colOffset == 0);
    REQUIRE(cells[0].glyphIndex == 999);
    REQUIRE(cells[0].cluster == 0);
    // Continuation cell for '=='
    REQUIRE(cells[1].isContinuation);
    REQUIRE(cells[1].colSpan == 2);
    REQUIRE(cells[1].colOffset == 1);
    REQUIRE(cells[1].glyphIndex == 999);
    REQUIRE(cells[1].cluster == 1);
    // Regular cell for 'A'
    REQUIRE(!cells[2].isContinuation);
    REQUIRE(cells[2].colSpan == 1);
    REQUIRE(cells[2].colOffset == 0);
    REQUIRE(cells[2].glyphIndex == 65);
    REQUIRE(cells[2].cluster == 2);

    std::cout << "  -> Ligature Cell Slicing passed." << std::endl;
}

// ============================================================================
// 2. Sub-Ligature Caret Navigation & Hit-Testing Tests
// ============================================================================
void testSubLigatureCaretNavigation() {
    std::cout << "[Test 2/4] Sub-Ligature Caret Navigation & Hit-Testing..." << std::endl;

    MonospaceGridLayout layout;
    std::vector<GridCellLayout> cells;

    std::string text = "let x = <!-- val -->;";
    layout.layoutWithLigatureRules(text, 10.0f, cells);

    // '<!--' is at columns 8..11 (byte offsets 8, 9, 10, 11)
    // 1. Column to byte offset navigation
    REQUIRE(layout.columnToByteOffset(cells, 7, text.size()) == 7);   // ' ' before '<!--'
    REQUIRE(layout.columnToByteOffset(cells, 8, text.size()) == 8);   // '<'
    REQUIRE(layout.columnToByteOffset(cells, 9, text.size()) == 9);   // '!'
    REQUIRE(layout.columnToByteOffset(cells, 10, text.size()) == 10); // '-'
    REQUIRE(layout.columnToByteOffset(cells, 11, text.size()) == 11); // '-'
    REQUIRE(layout.columnToByteOffset(cells, 12, text.size()) == 12); // ' ' after '<!--'

    // Boundary conditions
    REQUIRE(layout.columnToByteOffset(cells, 0, text.size()) == 0);
    REQUIRE(layout.columnToByteOffset(cells, -5, text.size()) == 0);
    REQUIRE(layout.columnToByteOffset(cells, 100, text.size()) == text.size());

    // 2. Byte offset to column
    REQUIRE(layout.byteOffsetToColumn(cells, 8) == 8);
    REQUIRE(layout.byteOffsetToColumn(cells, 9) == 9);
    REQUIRE(layout.byteOffsetToColumn(cells, 10) == 10);
    REQUIRE(layout.byteOffsetToColumn(cells, 11) == 11);
    REQUIRE(layout.byteOffsetToColumn(cells, 12) == 12);
    REQUIRE(layout.byteOffsetToColumn(cells, 0) == 0);
    REQUIRE(layout.byteOffsetToColumn(cells, text.size()) == static_cast<int>(cells.size()));

    // 3. Hit testing screen coordinates
    float cellW = 8.5f;
    int maxCols = 80;
    REQUIRE(layout.hitTestColumn(0.0f, cellW, maxCols) == 0);
    REQUIRE(layout.hitTestColumn(8.4f, cellW, maxCols) == 0);
    REQUIRE(layout.hitTestColumn(8.5f, cellW, maxCols) == 1);
    REQUIRE(layout.hitTestColumn(17.0f, cellW, maxCols) == 2);
    REQUIRE(layout.hitTestColumn(25.5f, cellW, maxCols) == 3);
    REQUIRE(layout.hitTestColumn(-10.0f, cellW, maxCols) == 0);
    REQUIRE(layout.hitTestColumn(1000.0f, cellW, maxCols) == maxCols);

    std::cout << "  -> Sub-Ligature Caret Navigation passed." << std::endl;
}

// ============================================================================
// 3. TerminalGrid Dual-Buffer & Continuation Cell Tests
// ============================================================================
void testTerminalGridLigatureState() {
    std::cout << "[Test 3/4] TerminalGrid Dual-Buffer & Continuation Cells..." << std::endl;

    TerminalGrid grid(80, 24);

    // Test A: UTF-8 box drawing streaming
    grid.writeString("┌──┐\r\n│  │\r\n└──┘");
    REQUIRE(grid.getCell(0, 0).codepoint == 0x250C); // ┌
    REQUIRE(grid.getCell(1, 0).codepoint == 0x2500); // ─
    REQUIRE(grid.getCell(2, 0).codepoint == 0x2500); // ─
    REQUIRE(grid.getCell(3, 0).codepoint == 0x2510); // ┐

    REQUIRE(grid.getCell(0, 1).codepoint == 0x2502); // │
    REQUIRE(grid.getCell(3, 1).codepoint == 0x2502); // │

    REQUIRE(grid.getCell(0, 2).codepoint == 0x2514); // └
    REQUIRE(grid.getCell(1, 2).codepoint == 0x2500); // ─
    REQUIRE(grid.getCell(2, 2).codepoint == 0x2500); // ─
    REQUIRE(grid.getCell(3, 2).codepoint == 0x2518); // ┘

    // Test B: Ligature slicing in grid
    grid.setCursorPos(10, 5);
    grid.writeLigature("===");
    REQUIRE(!grid.isContinuationCell(10, 5));
    REQUIRE(grid.isContinuationCell(11, 5));
    REQUIRE(grid.isContinuationCell(12, 5));
    REQUIRE(!grid.isContinuationCell(13, 5));

    REQUIRE(grid.getCellSpan(10, 5) == 3);
    REQUIRE(grid.getCellSpan(11, 5) == 1);
    REQUIRE(grid.getCellSpan(13, 5) == 1);

    // Test C: Explicit sliceLigature
    grid.setCell(20, 5, tui::Cell{'=', tui::Color::White(), tui::Color::DefaultBg(), 0});
    grid.setCell(21, 5, tui::Cell{'=', tui::Color::White(), tui::Color::DefaultBg(), 0});
    grid.sliceLigature(20, 5, 2);
    REQUIRE(!grid.isContinuationCell(20, 5));
    REQUIRE(grid.isContinuationCell(21, 5));
    REQUIRE(grid.getCellSpan(20, 5) == 2);

    // Test D: Dual buffer swap preservation
    grid.swapBuffers();
    const auto* front = grid.getFrontBuffer();
    REQUIRE((front[5 * 80 + 11].attrs & static_cast<uint8_t>(tui::TextAttr::Continuation)) != 0);

    std::cout << "  -> TerminalGrid Dual-Buffer & Continuation passed." << std::endl;
}

// ============================================================================
// 4. Analytical Zero-Seam Box & Block Geometry Tests
// ============================================================================
void testAnalyticalZeroSeamGeometry() {
    std::cout << "[Test 4/4] Analytical Zero-Seam Box & Block Mesh Generation..." << std::endl;

    BatchRenderer2D renderer;
    bool ok = renderer.initialize(nullptr, 800, 600, RenderBackendType::Filament);
    REQUIRE(ok);
    renderer.initDefaultMonospaceAtlas();

    renderer.beginFrame(800.0f, 600.0f);

    // 1. Horizontal Box Line (U+2500: ─) Zero-Seam Test
    size_t preH = renderer.getVertexCount();
    renderer.drawMonospaceCell(10.0f, 20.0f, 10.0f, 20.0f, 0x2500, 0xFFFFFFFF, 0); // Cell (10..20)
    renderer.drawMonospaceCell(20.0f, 20.0f, 10.0f, 20.0f, 0x2500, 0xFFFFFFFF, 0); // Cell (20..30)
    REQUIRE(renderer.getVertexCount() == preH + 12); // Exactly 2 quads = 12 vertices

    const auto& verts = renderer.getVertices();
    // Vertex mode must be 0 (VertexMode::Color / OpaqueMeshNoAA) for zero-seam precision
    REQUIRE(verts[preH].mode == 0);
    REQUIRE(verts[preH + 6].mode == 0);

    // Check X coordinates: right edge of Cell 0 quad must exactly match left edge of Cell 1 quad
    float cell0_xMax = std::max({verts[preH].x, verts[preH+1].x, verts[preH+2].x, verts[preH+3].x, verts[preH+4].x, verts[preH+5].x});
    float cell1_xMin = std::min({verts[preH+6].x, verts[preH+7].x, verts[preH+8].x, verts[preH+9].x, verts[preH+10].x, verts[preH+11].x});
    REQUIRE(std::abs(cell0_xMax - 20.0f) < 0.001f);
    REQUIRE(std::abs(cell1_xMin - 20.0f) < 0.001f);
    REQUIRE(std::abs(cell0_xMax - cell1_xMin) < 0.0001f); // ZERO SEAM!

    // 2. Vertical Box Line (U+2502: │) Zero-Seam Test
    size_t preV = renderer.getVertexCount();
    renderer.drawMonospaceCell(50.0f, 10.0f, 10.0f, 20.0f, 0x2502, 0xFFFFFFFF, 0); // Cell Y (10..30)
    renderer.drawMonospaceCell(50.0f, 30.0f, 10.0f, 20.0f, 0x2502, 0xFFFFFFFF, 0); // Cell Y (30..50)
    REQUIRE(renderer.getVertexCount() == preV + 12);

    float cell0_yMax = std::max({verts[preV].y, verts[preV+1].y, verts[preV+2].y, verts[preV+3].y, verts[preV+4].y, verts[preV+5].y});
    float cell1_yMin = std::min({verts[preV+6].y, verts[preV+7].y, verts[preV+8].y, verts[preV+9].y, verts[preV+10].y, verts[preV+11].y});
    REQUIRE(std::abs(cell0_yMax - 30.0f) < 0.001f);
    REQUIRE(std::abs(cell1_yMin - 30.0f) < 0.001f);
    REQUIRE(std::abs(cell0_yMax - cell1_yMin) < 0.0001f); // ZERO SEAM!

    // 3. Full Block (U+2588: █) Zero-Seam Test
    size_t preBlock = renderer.getVertexCount();
    renderer.drawMonospaceCell(100.0f, 10.0f, 12.0f, 24.0f, 0x2588, 0xFF00FFFF, 0); // (100..112)
    renderer.drawMonospaceCell(112.0f, 10.0f, 12.0f, 24.0f, 0x2588, 0xFF00FFFF, 0); // (112..124)
    REQUIRE(renderer.getVertexCount() == preBlock + 12);

    float b0_xMax = std::max({verts[preBlock].x, verts[preBlock+1].x, verts[preBlock+2].x, verts[preBlock+3].x, verts[preBlock+4].x, verts[preBlock+5].x});
    float b1_xMin = std::min({verts[preBlock+6].x, verts[preBlock+7].x, verts[preBlock+8].x, verts[preBlock+9].x, verts[preBlock+10].x, verts[preBlock+11].x});
    REQUIRE(std::abs(b0_xMax - 112.0f) < 0.001f);
    REQUIRE(std::abs(b1_xMin - 112.0f) < 0.001f);
    REQUIRE(std::abs(b0_xMax - b1_xMin) < 0.0001f); // ZERO SEAM!

    // 4. Fractional Block Elements (Lower 1/8 to 7/8, Left 1/8 to 7/8)
    for (char32_t cp = 0x2581; cp <= 0x2587; ++cp) {
        size_t prev = renderer.getVertexCount();
        renderer.drawMonospaceCell(150.0f, 10.0f, 10.0f, 20.0f, cp, 0xFFFFFFFF, 0);
        REQUIRE(renderer.getVertexCount() == prev + 6); // 1 rect per fraction
    }

    for (char32_t cp = 0x2589; cp <= 0x258F; ++cp) {
        size_t prev = renderer.getVertexCount();
        renderer.drawMonospaceCell(170.0f, 10.0f, 10.0f, 20.0f, cp, 0xFFFFFFFF, 0);
        REQUIRE(renderer.getVertexCount() == prev + 6);
    }

    // 5. Continuation cell skipping test (non-rendering glyph invariant)
    size_t preCont = renderer.getVertexCount();
    // Cell with Continuation attribute and NO background: emits 0 vertices
    renderer.drawMonospaceCell(200.0f, 10.0f, 10.0f, 20.0f, '=', 0xFFFFFFFF, 0x00000000,
                              static_cast<uint8_t>(tui::TextAttr::Continuation));
    REQUIRE(renderer.getVertexCount() == preCont); // 0 vertices emitted!

    // Cell with Continuation attribute and background: emits exactly 6 vertices (background rect only)
    renderer.drawMonospaceCell(210.0f, 10.0f, 10.0f, 20.0f, '=', 0xFFFFFFFF, 0xFF333333,
                              static_cast<uint8_t>(tui::TextAttr::Continuation));
    REQUIRE(renderer.getVertexCount() == preCont + 6);

    std::cout << "  -> Analytical Zero-Seam Geometry passed." << std::endl;
}

// ============================================================================
// Main Test Runner
// ============================================================================
int main() {
    std::cout << "=== Monospace Terminal Grid Invariants Test Suite ===" << std::endl;

    testLigatureCellSlicing();
    testSubLigatureCaretNavigation();
    testTerminalGridLigatureState();
    testAnalyticalZeroSeamGeometry();

    std::cout << "All Terminal Grid and Monospace Layout Invariants Verified Successfully!" << std::endl;
    return 0;
}
