#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <cstddef>
#include "eatsbits/text/text_shaper.hpp"
#include "eatsbits/text/font_manager.hpp"

namespace eatsbits::text {

/**
 * @brief Layout representation of a single cell in a monospace grid row.
 * Handles single characters, multi-character HarfBuzz ligatures, continuation cells,
 * and analytical box/block primitives.
 */
struct GridCellLayout {
    uint32_t glyphIndex{0};       // HarfBuzz glyph index (0 for space, box primitives, or unassigned)
    FontId fontId{kInvalidFontId};// Font ID that produced this glyph
    uint32_t cluster{0};          // Source string UTF-8 byte offset for caret mapping
    uint16_t clusterLength{1};    // Byte length of this cluster
    uint16_t colSpan{1};          // Column span of ligature (e.g. 2 for '==', 3 for '===', 4 for '<!--')
    uint16_t colOffset{0};        // Offset within span: 0 = primary cell, 1..colSpan-1 = continuation
    bool isContinuation{false};   // True if this cell is a continuation cell (do not render glyph)
    bool isBoxOrBlock{false};     // True if Unicode box-drawing or block element
    char32_t codepoint{0};        // Original character codepoint
    float xOffset{0.0f};          // HarfBuzz subpixel X offset
    float yOffset{0.0f};          // HarfBuzz subpixel Y offset
    float xAdvance{0.0f};         // Visual advance in pixels
};

/**
 * @brief High-performance monospace grid layout helper for terminals and code editors.
 *
 * Implements:
 * 1. Ligature Cell Slicing: When HarfBuzz collapses N ASCII characters into 1 compound glyph
 *    (e.g. '==' -> 2 cells, '===' -> 3 cells, '<!--' -> 4 cells), maps primary glyph to cell 0
 *    and marks cells 1..N-1 as non-rendering continuation cells.
 * 2. Sub-Ligature Caret Navigation: Maps cursor column index directly to UTF-8 cluster byte offset
 *    and vice versa, allowing seamless sub-ligature navigation without breaking visual shaping.
 * 3. Analytical Box/Block Detection: Identifies Unicode box drawing (U+2500-U+257F) and block
 *    characters (U+2580-U+259F) for zero-seam vector quad rendering.
 */
class MonospaceGridLayout {
public:
    MonospaceGridLayout() = default;
    ~MonospaceGridLayout() = default;

    /**
     * @brief Computes cell layout for a UTF-8 text line using HarfBuzz text shaping.
     * @param utf8Line The input text for the row.
     * @param fontMgr FontManager instance.
     * @param fontId Active monospace font ID.
     * @param fontSize Font pixel size.
     * @param cellWidth Fixed monospace column width.
     * @param shaper TextShaper instance (reusable, zero allocations).
     * @param outCells Output layout array of cells, one per monospace column.
     */
    void layoutLine(std::string_view utf8Line,
                    FontManager& fontMgr,
                    FontId fontId,
                    float fontSize,
                    float cellWidth,
                    TextShaper& shaper,
                    std::vector<GridCellLayout>& outCells);

    /**
     * @brief Slices a pre-shaped HarfBuzz run into monospace grid cells.
     */
    void layoutShapedRun(std::string_view utf8Line,
                         const ShapedRun& run,
                         float cellWidth,
                         std::vector<GridCellLayout>& outCells);

    /**
     * @brief Rule-based ligature layout helper (simulates OpenType ligatures for testing
     * or environments where HarfBuzz font lacks custom ligature lookup tables).
     */
    void layoutWithLigatureRules(std::string_view utf8Line,
                                 float cellWidth,
                                 std::vector<GridCellLayout>& outCells);

    /**
     * @brief Sub-ligature caret navigation: maps a cursor column index to the UTF-8 byte offset.
     * When cursor is inside a multi-cell ligature (colSpan > 1), navigates smoothly to the
     * corresponding sub-character cluster offset.
     */
    [[nodiscard]] size_t columnToByteOffset(const std::vector<GridCellLayout>& cells,
                                            int column,
                                            size_t lineByteLength) const noexcept;

    /**
     * @brief Maps a UTF-8 byte offset to the corresponding monospace cursor column.
     */
    [[nodiscard]] int byteOffsetToColumn(const std::vector<GridCellLayout>& cells,
                                         size_t byteOffset) const noexcept;

    /**
     * @brief Hit test a screen X coordinate (relative to line start) to find cursor column.
     */
    [[nodiscard]] int hitTestColumn(float relativeX, float cellWidth, int maxColumns) const noexcept;

    /**
     * @brief Helper to test if a codepoint is a box-drawing, block element, or Braille pattern.
     */
    [[nodiscard]] static bool isBoxDrawingOrBlock(char32_t cp) noexcept;
};

} // namespace eatsbits::text
