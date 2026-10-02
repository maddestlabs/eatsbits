#pragma once

#include "eatsbits/tui/types.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace eatsbits::terminal {

struct CursorState {
    int x{0};
    int y{0};
    bool visible{true};
    bool blink{true};
};

/**
 * @brief Dual-buffered, hardware-independent terminal cell grid.
 * Decoupled from raw I/O and windowing systems.
 * Provides double buffering, scrollback history, alternate screen buffer,
 * cursor management, and dirty tracking for high-performance GPU/WebGPU rendering.
 */
class TerminalGrid {
public:
    TerminalGrid(int cols = 80, int rows = 24, size_t maxScrollback = 1000);
    ~TerminalGrid() = default;

    // Resizing & Metrics
    void resize(int cols, int rows);
    [[nodiscard]] int getCols() const noexcept { return cols_; }
    [[nodiscard]] int getRows() const noexcept { return rows_; }
    [[nodiscard]] size_t getScrollbackCount() const noexcept { return scrollback_.size(); }
    [[nodiscard]] size_t getMaxScrollback() const noexcept { return maxScrollback_; }
    void setMaxScrollback(size_t maxLines) noexcept { maxScrollback_ = maxLines; }

    // Direct Cell Access
    [[nodiscard]] const tui::Cell& getCell(int col, int row) const;
    [[nodiscard]] tui::Cell& getCell(int col, int row);
    void setCell(int col, int row, const tui::Cell& cell);

    // Dual-Buffering & Dirty State
    [[nodiscard]] const tui::Cell* getFrontBuffer() const noexcept;
    [[nodiscard]] const tui::Cell* getBackBuffer() const noexcept;
    [[nodiscard]] tui::Cell* getBackBuffer() noexcept;
    void swapBuffers();
    [[nodiscard]] bool isRowDirty(int row) const noexcept;
    [[nodiscard]] bool hasAnyDirty() const noexcept { return anyDirty_; }
    void markAllDirty() noexcept;
    void clearDirty() noexcept;

    // Cursor Management
    [[nodiscard]] int getCursorX() const noexcept { return cursor_.x; }
    [[nodiscard]] int getCursorY() const noexcept { return cursor_.y; }
    void setCursorPos(int col, int row);
    void moveCursor(int dx, int dy);
    [[nodiscard]] bool isCursorVisible() const noexcept { return cursor_.visible; }
    void setCursorVisible(bool visible) noexcept { cursor_.visible = visible; }
    void saveCursor() noexcept;
    void restoreCursor() noexcept;

    // Active Color and Attribute State
    void setFgColor(const tui::Color& fg) noexcept { activeFg_ = fg; }
    void setBgColor(const tui::Color& bg) noexcept { activeBg_ = bg; }
    [[nodiscard]] const tui::Color& getFgColor() const noexcept { return activeFg_; }
    [[nodiscard]] const tui::Color& getBgColor() const noexcept { return activeBg_; }
    void setAttributes(uint8_t attrs) noexcept { activeAttrs_ = attrs; }
    void addAttributes(uint8_t attrs) noexcept { activeAttrs_ |= attrs; }
    void removeAttributes(uint8_t attrs) noexcept { activeAttrs_ &= ~attrs; }
    [[nodiscard]] uint8_t getAttributes() const noexcept { return activeAttrs_; }
    void resetAttributes() noexcept;

    // Text Streaming & Cursor Flow
    void writeCodepoint(char32_t cp);
    void writeString(std::string_view str);
    void newline();
    void carriageReturn() noexcept;
    void backspace() noexcept;
    void tab() noexcept;

    // Scrolling & Scrollback
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    void clearScrollback() noexcept { scrollback_.clear(); }
    [[nodiscard]] const std::vector<tui::Cell>& getScrollbackRow(size_t index) const;

    // Erase Modes
    void eraseInDisplay(int mode); // 0 = cursor to end, 1 = start to cursor, 2 = all, 3 = all + scrollback
    void eraseInLine(int mode);    // 0 = cursor to end, 1 = start to cursor, 2 = all

    // Screen Buffers (Primary vs Alternate)
    void setAlternateScreen(bool enable);
    [[nodiscard]] bool isAlternateScreen() const noexcept { return usingAltScreen_; }

    // Bracketed Paste Mode (DEC private 2004)
    void setBracketedPaste(bool enable) noexcept { bracketedPaste_ = enable; }
    [[nodiscard]] bool isBracketedPaste() const noexcept { return bracketedPaste_; }

    // Window Title (OSC 0 / 2)
    void setTitle(std::string title) { title_ = std::move(title); }
    [[nodiscard]] const std::string& getTitle() const noexcept { return title_; }

    // Headless / Testing Inspection
    [[nodiscard]] std::string dumpScreenText() const;
    [[nodiscard]] std::string dumpRowText(int row) const;

private:
    struct ScreenBuffer {
        std::vector<tui::Cell> front;
        std::vector<tui::Cell> back;
        CursorState cursor;
        CursorState savedCursor;
    };

    int cols_{80};
    int rows_{24};
    size_t maxScrollback_{1000};

    ScreenBuffer primaryScreen_;
    ScreenBuffer altScreen_;
    bool usingAltScreen_{false};

    ScreenBuffer& currentScreen() noexcept {
        return usingAltScreen_ ? altScreen_ : primaryScreen_;
    }
    const ScreenBuffer& currentScreen() const noexcept {
        return usingAltScreen_ ? altScreen_ : primaryScreen_;
    }

    CursorState cursor_{0, 0, true, true};
    CursorState savedCursor_{0, 0, true, true};

    tui::Color activeFg_{tui::Color::DefaultFg()};
    tui::Color activeBg_{tui::Color::DefaultBg()};
    uint8_t activeAttrs_{0};

    std::vector<std::vector<tui::Cell>> scrollback_;
    std::vector<bool> dirtyRows_;
    bool anyDirty_{true};

    bool bracketedPaste_{false};
    std::string title_{"Eatscript Terminal"};

    void initBuffer(ScreenBuffer& buf);
    void markRowDirty(int row) noexcept;
};

} // namespace eatsbits::terminal
