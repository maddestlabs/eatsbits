#include "eatsbits/text/monospace_grid_layout.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::text {

namespace {

inline char32_t decodeNextUtf8(const char*& ptr, const char* end, size_t& byteLen) noexcept {
    if (ptr >= end) {
        byteLen = 0;
        return 0;
    }
    const unsigned char c = static_cast<unsigned char>(*ptr);
    if (c < 0x80) {
        byteLen = 1;
        ptr++;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && (ptr + 1 < end)) {
        byteLen = 2;
        char32_t cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(ptr[1]) & 0x3F);
        ptr += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && (ptr + 2 < end)) {
        byteLen = 3;
        char32_t cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(ptr[1]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(ptr[2]) & 0x3F);
        ptr += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && (ptr + 3 < end)) {
        byteLen = 4;
        char32_t cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(ptr[1]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(ptr[2]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(ptr[3]) & 0x3F);
        ptr += 4;
        return cp;
    }
    byteLen = 1;
    ptr++;
    return c;
}

struct ParsedCodepoint {
    char32_t cp{0};
    size_t byteOffset{0};
    size_t byteLength{1};
};

} // namespace

bool MonospaceGridLayout::isBoxDrawingOrBlock(char32_t cp) noexcept {
    return (cp >= 0x2500 && cp <= 0x257F) || // Box Drawing
           (cp >= 0x2580 && cp <= 0x259F) || // Block Elements
           (cp >= 0x2800 && cp <= 0x28FF);   // Braille Patterns
}

void MonospaceGridLayout::layoutShapedRun(std::string_view utf8Line,
                                         const ShapedRun& run,
                                         float cellWidth,
                                         std::vector<GridCellLayout>& outCells) {
    (void)cellWidth;
    outCells.clear();
    if (utf8Line.empty() || run.glyphs.empty()) {
        return;
    }

    const size_t textLen = utf8Line.size();
    const size_t glyphCount = run.glyphs.size();

    for (size_t i = 0; i < glyphCount; ++i) {
        const auto& glyph = run.glyphs[i];
        size_t startCluster = glyph.cluster;
        if (startCluster > textLen) {
            startCluster = textLen;
        }

        size_t endCluster = textLen;
        if (i + 1 < glyphCount) {
            endCluster = run.glyphs[i + 1].cluster;
            if (endCluster > textLen) {
                endCluster = textLen;
            }
            if (endCluster < startCluster) {
                endCluster = startCluster;
            }
        }

        // Parse all codepoints within this cluster span
        std::vector<ParsedCodepoint> codepoints;
        const char* p = utf8Line.data() + startCluster;
        const char* end = utf8Line.data() + endCluster;
        while (p < end) {
            size_t currentOffset = static_cast<size_t>(p - utf8Line.data());
            size_t bLen = 0;
            char32_t cp = decodeNextUtf8(p, end, bLen);
            codepoints.push_back({cp, currentOffset, bLen});
        }

        size_t span = codepoints.size();
        if (span == 0) {
            // Empty cluster fallback
            span = 1;
            GridCellLayout cell;
            cell.glyphIndex = glyph.glyphIndex;
            cell.fontId = glyph.fontId;
            cell.cluster = static_cast<uint32_t>(startCluster);
            cell.clusterLength = static_cast<uint16_t>(endCluster - startCluster);
            cell.colSpan = 1;
            cell.colOffset = 0;
            cell.isContinuation = false;
            cell.xOffset = glyph.xOffset;
            cell.yOffset = glyph.yOffset;
            cell.xAdvance = glyph.xAdvance;
            outCells.push_back(cell);
            continue;
        }

        // 1. Primary cell (index 0 of the cluster / ligature)
        GridCellLayout primary;
        primary.glyphIndex = glyph.glyphIndex;
        primary.fontId = glyph.fontId;
        primary.cluster = static_cast<uint32_t>(codepoints[0].byteOffset);
        primary.clusterLength = static_cast<uint16_t>(codepoints[0].byteLength);
        primary.colSpan = static_cast<uint16_t>(span);
        primary.colOffset = 0;
        primary.isContinuation = false;
        primary.codepoint = codepoints[0].cp;
        primary.isBoxOrBlock = isBoxDrawingOrBlock(primary.codepoint);
        primary.xOffset = glyph.xOffset;
        primary.yOffset = glyph.yOffset;
        primary.xAdvance = glyph.xAdvance;
        outCells.push_back(primary);

        // 2. Continuation cells (indices 1..span-1)
        for (size_t k = 1; k < span; ++k) {
            GridCellLayout cont;
            cont.glyphIndex = glyph.glyphIndex; // Reference to same shaped compound glyph
            cont.fontId = glyph.fontId;
            cont.cluster = static_cast<uint32_t>(codepoints[k].byteOffset);
            cont.clusterLength = static_cast<uint16_t>(codepoints[k].byteLength);
            cont.colSpan = static_cast<uint16_t>(span);
            cont.colOffset = static_cast<uint16_t>(k);
            cont.isContinuation = true; // Mark non-rendering continuation cell
            cont.codepoint = codepoints[k].cp;
            cont.isBoxOrBlock = isBoxDrawingOrBlock(cont.codepoint);
            cont.xOffset = 0.0f;
            cont.yOffset = 0.0f;
            cont.xAdvance = 0.0f;
            outCells.push_back(cont);
        }
    }
}

void MonospaceGridLayout::layoutLine(std::string_view utf8Line,
                                     FontManager& fontMgr,
                                     FontId fontId,
                                     float fontSize,
                                     float cellWidth,
                                     TextShaper& shaper,
                                     std::vector<GridCellLayout>& outCells) {
    ShapedRun run;
    run.glyphs.reserve(utf8Line.size() + 16);
    if (shaper.shapeText(fontMgr, fontId, fontSize, utf8Line, run)) {
        layoutShapedRun(utf8Line, run, cellWidth, outCells);
    } else {
        // Fallback to rule-based or standard ASCII layout if shaping fails
        layoutWithLigatureRules(utf8Line, cellWidth, outCells);
    }
}

void MonospaceGridLayout::layoutWithLigatureRules(std::string_view utf8Line,
                                                 float cellWidth,
                                                 std::vector<GridCellLayout>& outCells) {
    (void)cellWidth;
    outCells.clear();
    if (utf8Line.empty()) return;

    // Known common monospace programming ligatures
    static const struct LigatureDef {
        std::string_view seq;
        uint32_t syntheticGlyph;
    } kLigatures[] = {
        {"<!--", 0xE100},
        {"-->",  0xE101},
        {"===",  0xE102},
        {"!==",  0xE103},
        {"==",   0xE104},
        {"!=",   0xE105},
        {"<=",   0xE106},
        {">=",   0xE107},
        {"=>",   0xE108},
        {"->",   0xE109},
        {"::",   0xE10A},
        {"/*",   0xE10B},
        {"*/",   0xE10C},
        {"///",  0xE10D},
        {"//",   0xE10E}
    };

    const char* ptr = utf8Line.data();
    const char* end = ptr + utf8Line.size();

    while (ptr < end) {
        size_t currentByteOffset = static_cast<size_t>(ptr - utf8Line.data());
        std::string_view remaining(ptr, static_cast<size_t>(end - ptr));

        // Check for ligature match
        const LigatureDef* matched = nullptr;
        for (const auto& lig : kLigatures) {
            if (remaining.starts_with(lig.seq)) {
                matched = &lig;
                break;
            }
        }

        if (matched) {
            size_t ligSpan = matched->seq.size();
            for (size_t k = 0; k < ligSpan; ++k) {
                GridCellLayout cell;
                cell.glyphIndex = matched->syntheticGlyph;
                cell.cluster = static_cast<uint32_t>(currentByteOffset + k);
                cell.clusterLength = 1;
                cell.colSpan = static_cast<uint16_t>(ligSpan);
                cell.colOffset = static_cast<uint16_t>(k);
                cell.isContinuation = (k > 0);
                cell.codepoint = static_cast<char32_t>(static_cast<unsigned char>(matched->seq[k]));
                cell.isBoxOrBlock = false;
                outCells.push_back(cell);
            }
            ptr += ligSpan;
        } else {
            size_t bLen = 0;
            char32_t cp = decodeNextUtf8(ptr, end, bLen);
            GridCellLayout cell;
            cell.glyphIndex = static_cast<uint32_t>(cp);
            cell.cluster = static_cast<uint32_t>(currentByteOffset);
            cell.clusterLength = static_cast<uint16_t>(bLen);
            cell.colSpan = 1;
            cell.colOffset = 0;
            cell.isContinuation = false;
            cell.codepoint = cp;
            cell.isBoxOrBlock = isBoxDrawingOrBlock(cp);
            outCells.push_back(cell);
        }
    }
}

size_t MonospaceGridLayout::columnToByteOffset(const std::vector<GridCellLayout>& cells,
                                              int column,
                                              size_t lineByteLength) const noexcept {
    if (column <= 0 || cells.empty()) {
        return 0;
    }
    if (column >= static_cast<int>(cells.size())) {
        return lineByteLength;
    }
    return cells[static_cast<size_t>(column)].cluster;
}

int MonospaceGridLayout::byteOffsetToColumn(const std::vector<GridCellLayout>& cells,
                                            size_t byteOffset) const noexcept {
    if (cells.empty() || byteOffset == 0) {
        return 0;
    }

    const auto& last = cells.back();
    if (byteOffset >= static_cast<size_t>(last.cluster + last.clusterLength)) {
        return static_cast<int>(cells.size());
    }

    int bestCol = 0;
    for (size_t i = 0; i < cells.size(); ++i) {
        if (cells[i].cluster <= byteOffset) {
            bestCol = static_cast<int>(i);
        } else {
            break;
        }
    }
    return bestCol;
}

int MonospaceGridLayout::hitTestColumn(float relativeX, float cellWidth, int maxColumns) const noexcept {
    if (cellWidth <= 0.001f || relativeX <= 0.0f) {
        return 0;
    }
    int col = static_cast<int>(std::floor(relativeX / cellWidth));
    if (col < 0) col = 0;
    if (col > maxColumns) col = maxColumns;
    return col;
}

} // namespace eatsbits::text
