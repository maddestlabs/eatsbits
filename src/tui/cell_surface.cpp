#include "eatsbits/tui/cell_surface.hpp"
#include <cstdlib>
#include <algorithm>

namespace eatsbits::tui {

namespace {

// Helper to decode a UTF-8 string into UTF-32 codepoints
void decodeUtf8(std::string_view text, std::vector<char32_t>& outCodepoints) {
    outCodepoints.clear();
    size_t i = 0;
    while (i < text.size()) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        char32_t cp = 0;
        size_t len = 0;

        if (c < 0x80) {
            cp = c;
            len = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            len = 4;
        } else {
            // Invalid UTF-8 start byte, fallback to replacement
            cp = 0xFFFD;
            len = 1;
        }

        if (i + len > text.size()) {
            break;
        }

        for (size_t j = 1; j < len; ++j) {
            uint8_t follow = static_cast<uint8_t>(text[i + j]);
            if ((follow & 0xC0) != 0x80) {
                cp = 0xFFFD;
                break;
            }
            cp = (cp << 6) | (follow & 0x3F);
        }

        outCodepoints.push_back(cp);
        i += len;
    }
}

} // namespace

CellSurface::CellSurface(int width, int height) {
    resize(width, height);
}

void CellSurface::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    width_ = width;
    height_ = height;
    const size_t total = static_cast<size_t>(width_ * height_);
    frontBuffer_.assign(total, Cell{});
    backBuffer_.assign(total, Cell{});
    dirtyRows_.assign(static_cast<size_t>(height_), true);
    anyDirty_ = true;
}

void CellSurface::clear(Color bg) {
    Cell emptyCell{' ', Color::DefaultFg(), bg, 0};
    std::fill(backBuffer_.begin(), backBuffer_.end(), emptyCell);
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), true);
    anyDirty_ = true;
}

void CellSurface::fill(const Cell& cell) {
    std::fill(backBuffer_.begin(), backBuffer_.end(), cell);
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), true);
    anyDirty_ = true;
}

void CellSurface::setCell(int x, int y, const Cell& cell) {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
    const size_t idx = static_cast<size_t>(y * width_ + x);
    if (backBuffer_[idx] != cell) {
        backBuffer_[idx] = cell;
        dirtyRows_[static_cast<size_t>(y)] = true;
        anyDirty_ = true;
    }
}

const Cell& CellSurface::getCell(int x, int y) const {
    static const Cell kEmpty{};
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return kEmpty;
    return backBuffer_[static_cast<size_t>(y * width_ + x)];
}

Cell& CellSurface::getBackCell(int x, int y) {
    static Cell kFallback{};
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return kFallback;
    dirtyRows_[static_cast<size_t>(y)] = true;
    anyDirty_ = true;
    return backBuffer_[static_cast<size_t>(y * width_ + x)];
}

void CellSurface::drawText(int x, int y, std::string_view text, Color fg, Color bg, uint8_t attrs) {
    if (y < 0 || y >= height_ || x >= width_) return;

    std::vector<char32_t> codepoints;
    decodeUtf8(text, codepoints);

    int curX = x;
    for (char32_t cp : codepoints) {
        if (curX >= width_) break;
        if (curX >= 0) {
            setCell(curX, y, Cell{cp, fg, bg, attrs});
        }
        curX++;
    }
}

void CellSurface::drawBox(const Rect& rect, Color borderFg, Color bg, bool doubleLine, std::string_view title) {
    if (rect.width < 2 || rect.height < 2) return;

    // Unicode box characters
    const char32_t tl = doubleLine ? 0x2554 : 0x250C; // ╔ or ┌
    const char32_t tr = doubleLine ? 0x2557 : 0x2510; // ╗ or ┐
    const char32_t bl = doubleLine ? 0x255A : 0x2514; // ╚ or └
    const char32_t br = doubleLine ? 0x255D : 0x2518; // ╝ or ┘
    const char32_t hz = doubleLine ? 0x2550 : 0x2500; // ═ or ─
    const char32_t vt = doubleLine ? 0x2551 : 0x2502; // ║ or │

    const int x1 = rect.x;
    const int y1 = rect.y;
    const int x2 = rect.x + rect.width - 1;
    const int y2 = rect.y + rect.height - 1;

    // Corners
    setCell(x1, y1, Cell{tl, borderFg, bg, 0});
    setCell(x2, y1, Cell{tr, borderFg, bg, 0});
    setCell(x1, y2, Cell{bl, borderFg, bg, 0});
    setCell(x2, y2, Cell{br, borderFg, bg, 0});

    // Horizontal edges
    for (int x = x1 + 1; x < x2; ++x) {
        setCell(x, y1, Cell{hz, borderFg, bg, 0});
        setCell(x, y2, Cell{hz, borderFg, bg, 0});
    }

    // Vertical edges
    for (int y = y1 + 1; y < y2; ++y) {
        setCell(x1, y, Cell{vt, borderFg, bg, 0});
        setCell(x2, y, Cell{vt, borderFg, bg, 0});
    }

    // Interior fill
    for (int y = y1 + 1; y < y2; ++y) {
        for (int x = x1 + 1; x < x2; ++x) {
            setCell(x, y, Cell{' ', Color::DefaultFg(), bg, 0});
        }
    }

    // Optional title
    if (!title.empty() && rect.width > 4) {
        std::string titleStr = " ";
        titleStr.append(title);
        titleStr.append(" ");
        int titleX = x1 + 2;
        drawText(titleX, y1, titleStr, Color::White(), bg, static_cast<uint8_t>(TextAttr::Bold));
    }
}

void CellSurface::fillRect(const Rect& rect, Color bg, char32_t fillChar) {
    const int xEnd = std::min(rect.x + rect.width, width_);
    const int yEnd = std::min(rect.y + rect.height, height_);
    const int xStart = std::max(rect.x, 0);
    const int yStart = std::max(rect.y, 0);

    for (int y = yStart; y < yEnd; ++y) {
        for (int x = xStart; x < xEnd; ++x) {
            setCell(x, y, Cell{fillChar, Color::DefaultFg(), bg, 0});
        }
    }
}

uint8_t CellSurface::getBrailleDotMask(int col, int row) {
    // Unicode Braille dot mapping (U+2800..U+28FF)
    // col: 0..1, row: 0..3
    static const uint8_t masks[2][4] = {
        { 0x01, 0x02, 0x04, 0x40 }, // col 0: dot 1, 2, 3, 7
        { 0x08, 0x10, 0x20, 0x80 }  // col 1: dot 4, 5, 6, 8
    };
    if (col >= 0 && col < 2 && row >= 0 && row < 4) {
        return masks[col][row];
    }
    return 0;
}

void CellSurface::setBrailleDot(int subX, int subY, bool state, Color fg, Color bg) {
    if (subX < 0 || subX >= getBrailleWidth() || subY < 0 || subY >= getBrailleHeight()) return;

    const int cellX = subX / 2;
    const int cellY = subY / 4;
    const int dotCol = subX % 2;
    const int dotRow = subY % 4;
    const uint8_t mask = getBrailleDotMask(dotCol, dotRow);

    Cell c = getCell(cellX, cellY);
    if (c.codepoint < 0x2800 || c.codepoint > 0x28FF) {
        c.codepoint = 0x2800;
        c.bg = bg;
    }

    if (state) {
        c.codepoint |= mask;
        c.fg = fg;
    } else {
        c.codepoint &= ~mask;
    }

    setCell(cellX, cellY, c);
}

void CellSurface::drawBrailleLine(int x0, int y0, int x1, int y1, Color fg, Color bg) {
    // Bresenham's line algorithm in Braille sub-pixel space
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setBrailleDot(x0, y0, true, fg, bg);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void CellSurface::clearBrailleArea(const Rect& cellRect, Color bg) {
    for (int y = cellRect.y; y < cellRect.y + cellRect.height; ++y) {
        for (int x = cellRect.x; x < cellRect.x + cellRect.width; ++x) {
            setCell(x, y, Cell{0x2800, Color::DefaultFg(), bg, 0});
        }
    }
}

void CellSurface::drawHorizontalMeter(int x, int y, int width, float value, float peak) {
    if (width <= 0) return;
    value = std::clamp(value, 0.0f, 1.0f);
    float totalBlocks = value * static_cast<float>(width);
    int fullBlocks = static_cast<int>(totalBlocks);
    float remainder = totalBlocks - static_cast<float>(fullBlocks);

    // Horizontal partial block characters: ' ', '▏', '▎', '▍', '▌', '▋', '▊', '▉', '█'
    static const char32_t partials[9] = {
        ' ', 0x258F, 0x258E, 0x258D, 0x258C, 0x258B, 0x258A, 0x2589, 0x2588
    };

    for (int i = 0; i < width; ++i) {
        float pos = static_cast<float>(i) / static_cast<float>(width);
        Color fg = (pos < 0.7f) ? Color::MeterGreen() : (pos < 0.9f) ? Color::MeterYellow() : Color::MeterRed();

        if (i < fullBlocks) {
            setCell(x + i, y, Cell{0x2588, fg, Color::DarkGray(), 0});
        } else if (i == fullBlocks && remainder > 0.05f) {
            int partIdx = std::clamp(static_cast<int>(remainder * 8.0f), 1, 8);
            setCell(x + i, y, Cell{partials[partIdx], fg, Color::DarkGray(), 0});
        } else {
            setCell(x + i, y, Cell{' ', fg, Color::DarkGray(), 0});
        }
    }

    if (peak >= 0.0f) {
        int peakIdx = std::clamp(static_cast<int>(peak * static_cast<float>(width - 1)), 0, width - 1);
        Color peakCol = (peak < 0.7f) ? Color::MeterGreen() : (peak < 0.9f) ? Color::MeterYellow() : Color::MeterRed();
        setCell(x + peakIdx, y, Cell{0x2502, peakCol, Color::DarkGray(), static_cast<uint8_t>(TextAttr::Bold)});
    }
}

void CellSurface::drawVerticalMeter(int x, int y, int height, float value, float peak) {
    if (height <= 0) return;
    value = std::clamp(value, 0.0f, 1.0f);
    float totalBlocks = value * static_cast<float>(height);
    int fullBlocks = static_cast<int>(totalBlocks);
    float remainder = totalBlocks - static_cast<float>(fullBlocks);

    // Vertical partial block characters: ' ', ' ', '▂', '▃', '▄', '▅', '▆', '▇', '█'
    static const char32_t partials[9] = {
        ' ', 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588
    };

    for (int i = 0; i < height; ++i) {
        int row = y + height - 1 - i; // draw from bottom up
        float pos = static_cast<float>(i) / static_cast<float>(height);
        Color fg = (pos < 0.7f) ? Color::MeterGreen() : (pos < 0.9f) ? Color::MeterYellow() : Color::MeterRed();

        if (i < fullBlocks) {
            setCell(x, row, Cell{0x2588, fg, Color::DarkGray(), 0});
        } else if (i == fullBlocks && remainder > 0.05f) {
            int partIdx = std::clamp(static_cast<int>(remainder * 8.0f), 1, 8);
            setCell(x, row, Cell{partials[partIdx], fg, Color::DarkGray(), 0});
        } else {
            setCell(x, row, Cell{' ', fg, Color::DarkGray(), 0});
        }
    }

    if (peak >= 0.0f) {
        int peakI = std::clamp(static_cast<int>(peak * static_cast<float>(height - 1)), 0, height - 1);
        int peakRow = y + height - 1 - peakI;
        setCell(x, peakRow, Cell{0x2500, Color::White(), Color::DarkGray(), static_cast<uint8_t>(TextAttr::Bold)});
    }
}

void CellSurface::swapBuffers() {
    frontBuffer_ = backBuffer_;
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), false);
    anyDirty_ = false;
}

bool CellSurface::isRowDirty(int y) const {
    if (y < 0 || y >= static_cast<int>(dirtyRows_.size())) return false;
    return dirtyRows_[static_cast<size_t>(y)];
}

void CellSurface::markAllDirty() {
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), true);
    anyDirty_ = true;
}

} // namespace eatsbits::tui
