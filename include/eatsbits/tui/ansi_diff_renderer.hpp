#pragma once

#include "eatsbits/tui/cell_surface.hpp"
#include <string>

namespace eatsbits::tui {

class AnsiDiffRenderer {
public:
    AnsiDiffRenderer();
    ~AnsiDiffRenderer() = default;

    // Renders dirty differences between front and back buffers into outBuffer
    // Returns number of modified cells rendered
    size_t renderDiff(CellSurface& surface, std::string& outBuffer, bool forceFull = false);

    void resetTerminalState(std::string& outBuffer);

private:
    Color activeFg_;
    Color activeBg_;
    uint8_t activeAttrs_{0};
    int cursorX_{-1};
    int cursorY_{-1};

    void appendCursorMove(std::string& out, int x, int y);
    void appendColorAndAttrs(std::string& out, const Cell& cell);
    static void appendUtf8(std::string& out, char32_t cp);
};

} // namespace eatsbits::tui
