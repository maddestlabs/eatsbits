#include "eatsbits/tui/ansi_diff_renderer.hpp"
#include <cstdio>

namespace eatsbits::tui {

AnsiDiffRenderer::AnsiDiffRenderer()
    : activeFg_(), activeBg_(), activeAttrs_(0), cursorX_(-1), cursorY_(-1) {}

void AnsiDiffRenderer::resetTerminalState(std::string& out) {
    out.append("\x1b[0m\x1b[39m\x1b[49m");
    activeFg_ = Color();
    activeBg_ = Color();
    activeAttrs_ = 0;
    cursorX_ = -1;
    cursorY_ = -1;
}

void AnsiDiffRenderer::appendCursorMove(std::string& out, int x, int y) {
    if (cursorX_ == x && cursorY_ == y) return;
    // ANSI cursor positioning is 1-indexed: \x1b[<row>;<col>H
    out.append("\x1b[");
    out.append(std::to_string(y + 1));
    out.push_back(';');
    out.append(std::to_string(x + 1));
    out.push_back('H');
    cursorX_ = x;
    cursorY_ = y;
}

void AnsiDiffRenderer::appendColorAndAttrs(std::string& out, const Cell& cell) {
    // Attributes
    if (cell.attrs != activeAttrs_) {
        // Reset and re-apply if attributes differ
        out.append("\x1b[0m");
        activeAttrs_ = cell.attrs;
        activeFg_ = Color();
        activeBg_ = Color();

        if (cell.attrs & static_cast<uint8_t>(TextAttr::Bold)) out.append("\x1b[1m");
        if (cell.attrs & static_cast<uint8_t>(TextAttr::Dim)) out.append("\x1b[2m");
        if (cell.attrs & static_cast<uint8_t>(TextAttr::Underline)) out.append("\x1b[4m");
        if (cell.attrs & static_cast<uint8_t>(TextAttr::Reverse)) out.append("\x1b[7m");
        if (cell.attrs & static_cast<uint8_t>(TextAttr::Blink)) out.append("\x1b[5m");
    }

    // Foreground TrueColor
    if (cell.fg != activeFg_) {
        activeFg_ = cell.fg;
        if (cell.fg.isDefault) {
            out.append("\x1b[39m");
        } else {
            out.append("\x1b[38;2;");
            out.append(std::to_string(cell.fg.r));
            out.push_back(';');
            out.append(std::to_string(cell.fg.g));
            out.push_back(';');
            out.append(std::to_string(cell.fg.b));
            out.push_back('m');
        }
    }

    // Background TrueColor
    if (cell.bg != activeBg_) {
        activeBg_ = cell.bg;
        if (cell.bg.isDefault) {
            out.append("\x1b[49m");
        } else {
            out.append("\x1b[48;2;");
            out.append(std::to_string(cell.bg.r));
            out.push_back(';');
            out.append(std::to_string(cell.bg.g));
            out.push_back(';');
            out.append(std::to_string(cell.bg.b));
            out.push_back('m');
        }
    }
}

void AnsiDiffRenderer::appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

size_t AnsiDiffRenderer::renderDiff(CellSurface& surface, std::string& out, bool forceFull) {
    const int w = surface.getWidth();
    const int h = surface.getHeight();
    if (w <= 0 || h <= 0) return 0;

    const Cell* front = surface.getFrontBuffer();
    const Cell* back = surface.getBackBuffer();
    size_t dirtyCellCount = 0;

    if (forceFull) {
        // Clear screen and home cursor
        out.append("\x1b[2J\x1b[H");
        cursorX_ = 0;
        cursorY_ = 0;
        resetTerminalState(out);
    }

    for (int y = 0; y < h; ++y) {
        if (!forceFull && !surface.isRowDirty(y)) {
            continue;
        }

        const size_t rowOffset = static_cast<size_t>(y * w);

        for (int x = 0; x < w; ++x) {
            const size_t idx = rowOffset + static_cast<size_t>(x);
            const Cell& newCell = back[idx];

            if (forceFull || front[idx] != newCell) {
                dirtyCellCount++;

                // Move cursor if not adjacent to current cursor
                appendCursorMove(out, x, y);

                // Update styling & color
                appendColorAndAttrs(out, newCell);

                // Write character
                appendUtf8(out, newCell.codepoint);
                cursorX_++;
            }
        }
    }

    surface.swapBuffers();
    return dirtyCellCount;
}

} // namespace eatsbits::tui
