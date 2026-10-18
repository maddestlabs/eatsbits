#include "eatsbits/terminal/terminal_grid.hpp"
#include <stdexcept>
#include <algorithm>

namespace eatsbits::terminal {

static std::string codepointToUtf8(char32_t cp) {
    std::string out;
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back('?');
    }
    return out;
}

TerminalGrid::TerminalGrid(int cols, int rows, size_t maxScrollback)
    : cols_(std::max(1, cols)),
      rows_(std::max(1, rows)),
      maxScrollback_(maxScrollback),
      scrollTop_(0),
      scrollBottom_(std::max(1, rows) - 1),
      dirtyRows_(static_cast<size_t>(rows_), true),
      anyDirty_(true) {
    initBuffer(primaryScreen_);
    initBuffer(altScreen_);
}

void TerminalGrid::initBuffer(ScreenBuffer& buf) {
    size_t count = static_cast<size_t>(cols_) * rows_;
    tui::Cell blank{' ', tui::Color::DefaultFg(), tui::Color::DefaultBg(), 0};
    buf.front.assign(count, blank);
    buf.back.assign(count, blank);
    buf.cursor = {0, 0, true, true};
    buf.savedCursor = {0, 0, true, true};
    buf.scrollTop = 0;
    buf.scrollBottom = rows_ - 1;
    buf.mouseTrackingMode = 0;
    buf.sgrMouse = false;
}

void TerminalGrid::resize(int cols, int rows) {
    if (cols <= 0 || rows <= 0) return;
    if (cols == cols_ && rows == rows_) return;

    int oldCols = cols_;
    int oldRows = rows_;
    cols_ = cols;
    rows_ = rows;
    dirtyRows_.assign(static_cast<size_t>(rows_), true);
    anyDirty_ = true;

    auto resizeBuf = [this, oldCols, oldRows](ScreenBuffer& buf) {
        size_t newCount = static_cast<size_t>(cols_) * rows_;
        tui::Cell blank{' ', activeFg_, activeBg_, 0};
        std::vector<tui::Cell> newFront(newCount, blank);
        std::vector<tui::Cell> newBack(newCount, blank);

        int copyRows = std::min(oldRows, rows_);
        int copyCols = std::min(oldCols, cols_);
        for (int r = 0; r < copyRows; ++r) {
            for (int c = 0; c < copyCols; ++c) {
                newFront[r * cols_ + c] = buf.front[r * oldCols + c];
                newBack[r * cols_ + c] = buf.back[r * oldCols + c];
            }
        }
        buf.front = std::move(newFront);
        buf.back = std::move(newBack);
        buf.scrollTop = 0;
        buf.scrollBottom = rows_ - 1;
    };

    resizeBuf(primaryScreen_);
    resizeBuf(altScreen_);

    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;

    cursor_.x = std::clamp(cursor_.x, 0, cols_ - 1);
    cursor_.y = std::clamp(cursor_.y, 0, rows_ - 1);
}

const tui::Cell& TerminalGrid::getCell(int col, int row) const {
    static const tui::Cell kOutOfBounds{' ', tui::Color::DefaultFg(), tui::Color::DefaultBg(), 0};
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) {
        return kOutOfBounds;
    }
    return currentScreen().back[static_cast<size_t>(row * cols_ + col)];
}

tui::Cell& TerminalGrid::getCell(int col, int row) {
    static tui::Cell kDummy{' ', tui::Color::DefaultFg(), tui::Color::DefaultBg(), 0};
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) {
        return kDummy;
    }
    markRowDirty(row);
    return currentScreen().back[static_cast<size_t>(row * cols_ + col)];
}

void TerminalGrid::setCell(int col, int row, const tui::Cell& cell) {
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return;
    size_t idx = static_cast<size_t>(row * cols_ + col);
    auto& screen = currentScreen();
    if (screen.back[idx] != cell) {
        screen.back[idx] = cell;
        markRowDirty(row);
    }
}

const tui::Cell* TerminalGrid::getFrontBuffer() const noexcept {
    return currentScreen().front.data();
}

const tui::Cell* TerminalGrid::getBackBuffer() const noexcept {
    return currentScreen().back.data();
}

tui::Cell* TerminalGrid::getBackBuffer() noexcept {
    return currentScreen().back.data();
}

void TerminalGrid::swapBuffers() {
    auto& screen = currentScreen();
    for (int r = 0; r < rows_; ++r) {
        if (dirtyRows_[static_cast<size_t>(r)]) {
            size_t startIdx = static_cast<size_t>(r * cols_);
            for (int c = 0; c < cols_; ++c) {
                screen.front[startIdx + c] = screen.back[startIdx + c];
            }
        }
    }
    clearDirty();
}

bool TerminalGrid::isRowDirty(int row) const noexcept {
    if (row < 0 || row >= rows_) return false;
    return dirtyRows_[static_cast<size_t>(row)];
}

void TerminalGrid::markAllDirty() noexcept {
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), true);
    anyDirty_ = true;
}

void TerminalGrid::clearDirty() noexcept {
    std::fill(dirtyRows_.begin(), dirtyRows_.end(), false);
    anyDirty_ = false;
}

void TerminalGrid::markRowDirty(int row) noexcept {
    if (row >= 0 && row < rows_) {
        dirtyRows_[static_cast<size_t>(row)] = true;
        anyDirty_ = true;
    }
}

void TerminalGrid::setCursorPos(int col, int row) {
    cursor_.x = std::clamp(col, 0, cols_ - 1);
    cursor_.y = std::clamp(row, 0, rows_ - 1);
}

void TerminalGrid::moveCursor(int dx, int dy) {
    setCursorPos(cursor_.x + dx, cursor_.y + dy);
}

void TerminalGrid::saveCursor() noexcept {
    savedCursor_ = cursor_;
}

void TerminalGrid::restoreCursor() noexcept {
    cursor_ = savedCursor_;
}

void TerminalGrid::resetAttributes() noexcept {
    activeFg_ = tui::Color::DefaultFg();
    activeBg_ = tui::Color::DefaultBg();
    activeAttrs_ = 0;
}

void TerminalGrid::writeCodepoint(char32_t cp) {
    if (cursor_.x >= cols_) {
        cursor_.x = 0;
        newline();
    }

    tui::Cell cell{cp, activeFg_, activeBg_, activeAttrs_};
    setCell(cursor_.x, cursor_.y, cell);
    cursor_.x++;
}

static inline char32_t decodeNextUtf8Codepoint(const char*& ptr, const char* end) noexcept {
    if (ptr >= end) return 0;
    const unsigned char c = static_cast<unsigned char>(*ptr);
    if (c < 0x80) {
        ptr++;
        return c;
    }
    if ((c & 0xE0) == 0xC0 && (ptr + 1 < end)) {
        char32_t cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(ptr[1]) & 0x3F);
        ptr += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && (ptr + 2 < end)) {
        char32_t cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(ptr[1]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(ptr[2]) & 0x3F);
        ptr += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && (ptr + 3 < end)) {
        char32_t cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(ptr[1]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(ptr[2]) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(ptr[3]) & 0x3F);
        ptr += 4;
        return cp;
    }
    ptr++;
    return c;
}

void TerminalGrid::writeString(std::string_view str) {
    const char* ptr = str.data();
    const char* end = ptr + str.size();
    while (ptr < end) {
        char ch = *ptr;
        if (ch == '\r') {
            ptr++;
            carriageReturn();
        } else if (ch == '\n') {
            ptr++;
            newline();
        } else if (ch == '\b') {
            ptr++;
            backspace();
        } else if (ch == '\t') {
            ptr++;
            tab();
        } else {
            char32_t cp = decodeNextUtf8Codepoint(ptr, end);
            writeCodepoint(cp);
        }
    }
}

void TerminalGrid::writeLigature(std::string_view str) {
    if (str.empty()) return;
    std::vector<char32_t> cps;
    const char* p = str.data();
    const char* end = p + str.size();
    while (p < end) {
        cps.push_back(decodeNextUtf8Codepoint(p, end));
    }
    if (cps.empty()) return;

    for (size_t i = 0; i < cps.size(); ++i) {
        uint8_t oldAttrs = activeAttrs_;
        if (i > 0) {
            activeAttrs_ |= static_cast<uint8_t>(tui::TextAttr::Continuation);
        }
        writeCodepoint(cps[i]);
        activeAttrs_ = oldAttrs;
    }
}

void TerminalGrid::sliceLigature(int startCol, int row, int colSpan) {
    if (colSpan <= 1 || row < 0 || row >= rows_ || startCol < 0 || startCol >= cols_) return;
    int endCol = std::min(cols_, startCol + colSpan);
    for (int c = startCol + 1; c < endCol; ++c) {
        tui::Cell cell = getCell(c, row);
        cell.attrs |= static_cast<uint8_t>(tui::TextAttr::Continuation);
        setCell(c, row, cell);
    }
}

bool TerminalGrid::isContinuationCell(int col, int row) const noexcept {
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return false;
    return (getCell(col, row).attrs & static_cast<uint8_t>(tui::TextAttr::Continuation)) != 0;
}

int TerminalGrid::getCellSpan(int col, int row) const noexcept {
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return 1;
    if (isContinuationCell(col, row)) return 1;
    int span = 1;
    while (col + span < cols_ && isContinuationCell(col + span, row)) {
        span++;
    }
    return span;
}

void TerminalGrid::newline() {
    if (cursor_.y == scrollBottom_) {
        scrollUp(1);
    } else if (cursor_.y < rows_ - 1) {
        cursor_.y++;
    }
}

void TerminalGrid::carriageReturn() noexcept {
    cursor_.x = 0;
}

void TerminalGrid::backspace() noexcept {
    if (cursor_.x > 0) {
        cursor_.x--;
    }
}

void TerminalGrid::tab() noexcept {
    cursor_.x = std::min(cols_ - 1, ((cursor_.x / 8) + 1) * 8);
}

void TerminalGrid::scrollUp(int lines) {
    if (lines <= 0) return;
    int regionHeight = scrollBottom_ - scrollTop_ + 1;
    if (regionHeight <= 0) return;
    lines = std::min(lines, regionHeight);

    auto& screen = currentScreen();

    // Push evicted top lines into scrollback buffer (primary screen only, when scrolling full screen)
    if (!usingAltScreen_ && maxScrollback_ > 0 && scrollTop_ == 0 && scrollBottom_ == rows_ - 1) {
        for (int l = 0; l < lines; ++l) {
            std::vector<tui::Cell> rowCells(cols_);
            size_t srcOffset = static_cast<size_t>(l * cols_);
            std::copy_n(&screen.back[srcOffset], cols_, rowCells.begin());

            if (scrollback_.size() >= maxScrollback_) {
                scrollback_.erase(scrollback_.begin());
            }
            scrollback_.push_back(std::move(rowCells));
        }
    }

    // Shift rows within [scrollTop_, scrollBottom_] upward
    int rowsToMove = regionHeight - lines;
    if (rowsToMove > 0) {
        size_t dst = static_cast<size_t>(scrollTop_ * cols_);
        size_t src = static_cast<size_t>((scrollTop_ + lines) * cols_);
        size_t count = static_cast<size_t>(rowsToMove * cols_);
        std::copy_n(&screen.back[src], count, &screen.back[dst]);
    }

    // Clear newly opened bottom rows within [scrollBottom_ - lines + 1, scrollBottom_]
    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    size_t blankStart = static_cast<size_t>((scrollBottom_ - lines + 1) * cols_);
    size_t blankCount = static_cast<size_t>(lines * cols_);
    std::fill_n(&screen.back[blankStart], blankCount, blank);

    for (int r = scrollTop_; r <= scrollBottom_; ++r) {
        markRowDirty(r);
    }
}

void TerminalGrid::scrollDown(int lines) {
    if (lines <= 0) return;
    int regionHeight = scrollBottom_ - scrollTop_ + 1;
    if (regionHeight <= 0) return;
    lines = std::min(lines, regionHeight);

    auto& screen = currentScreen();

    // Shift rows downward within [scrollTop_, scrollBottom_]
    int rowsToMove = regionHeight - lines;
    if (rowsToMove > 0) {
        size_t srcStart = static_cast<size_t>(scrollTop_ * cols_);
        size_t srcEnd = static_cast<size_t>((scrollTop_ + rowsToMove) * cols_);
        size_t dstEnd = static_cast<size_t>((scrollBottom_ + 1) * cols_);
        std::copy_backward(&screen.back[srcStart], &screen.back[srcEnd], &screen.back[dstEnd]);
    }

    // Clear newly opened top rows within [scrollTop_, scrollTop_ + lines - 1]
    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    size_t blankStart = static_cast<size_t>(scrollTop_ * cols_);
    size_t blankCount = static_cast<size_t>(lines * cols_);
    std::fill_n(&screen.back[blankStart], blankCount, blank);

    for (int r = scrollTop_; r <= scrollBottom_; ++r) {
        markRowDirty(r);
    }
}

const std::vector<tui::Cell>& TerminalGrid::getScrollbackRow(size_t index) const {
    static const std::vector<tui::Cell> kEmpty;
    if (index >= scrollback_.size()) return kEmpty;
    return scrollback_[index];
}

void TerminalGrid::eraseInDisplay(int mode) {
    auto& screen = currentScreen();
    tui::Cell blank{' ', activeFg_, activeBg_, 0};

    if (mode == 0) {
        // Cursor to end of screen
        size_t start = static_cast<size_t>(cursor_.y * cols_ + cursor_.x);
        size_t total = static_cast<size_t>(rows_ * cols_);
        if (start < total) {
            std::fill(&screen.back[start], &screen.back[total], blank);
            for (int r = cursor_.y; r < rows_; ++r) markRowDirty(r);
        }
    } else if (mode == 1) {
        // Beginning of screen to cursor
        size_t end = static_cast<size_t>(cursor_.y * cols_ + cursor_.x + 1);
        size_t total = static_cast<size_t>(rows_ * cols_);
        end = std::min(end, total);
        std::fill(&screen.back[0], &screen.back[end], blank);
        for (int r = 0; r <= cursor_.y; ++r) markRowDirty(r);
    } else if (mode == 2 || mode == 3) {
        // Entire display
        std::fill(screen.back.begin(), screen.back.end(), blank);
        markAllDirty();
        if (mode == 3) {
            clearScrollback();
        }
    }
}

void TerminalGrid::eraseInLine(int mode) {
    auto& screen = currentScreen();
    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    size_t rowOffset = static_cast<size_t>(cursor_.y * cols_);
    int curX = std::clamp(cursor_.x, 0, cols_ - 1);

    if (mode == 0) {
        // Cursor to end of line
        size_t start = rowOffset + static_cast<size_t>(curX);
        size_t end = rowOffset + static_cast<size_t>(cols_);
        std::fill(&screen.back[start], &screen.back[end], blank);
        markRowDirty(cursor_.y);
    } else if (mode == 1) {
        // Start of line to cursor
        size_t start = rowOffset;
        size_t end = rowOffset + static_cast<size_t>(std::min(cols_, curX + 1));
        std::fill(&screen.back[start], &screen.back[end], blank);
        markRowDirty(cursor_.y);
    } else if (mode == 2) {
        // Entire line
        size_t start = rowOffset;
        size_t end = rowOffset + static_cast<size_t>(cols_);
        std::fill(&screen.back[start], &screen.back[end], blank);
        markRowDirty(cursor_.y);
    }
}

void TerminalGrid::eraseCharacters(int count) {
    if (count <= 0 || cursor_.y < 0 || cursor_.y >= rows_) return;
    int startCol = std::clamp(cursor_.x, 0, cols_ - 1);
    int endCol = std::min(cols_, startCol + count);
    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    auto& screen = currentScreen();
    size_t rowOffset = static_cast<size_t>(cursor_.y * cols_);
    std::fill(&screen.back[rowOffset + static_cast<size_t>(startCol)],
              &screen.back[rowOffset + static_cast<size_t>(endCol)],
              blank);
    markRowDirty(cursor_.y);
}

void TerminalGrid::deleteCharacters(int count) {
    if (count <= 0 || cursor_.y < 0 || cursor_.y >= rows_) return;
    int startCol = std::clamp(cursor_.x, 0, cols_ - 1);
    count = std::min(count, cols_ - startCol);
    auto& screen = currentScreen();
    size_t rowOffset = static_cast<size_t>(cursor_.y * cols_);

    int moveCount = cols_ - startCol - count;
    if (moveCount > 0) {
        std::copy_n(&screen.back[rowOffset + static_cast<size_t>(startCol + count)],
                    static_cast<size_t>(moveCount),
                    &screen.back[rowOffset + static_cast<size_t>(startCol)]);
    }
    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    std::fill_n(&screen.back[rowOffset + static_cast<size_t>(cols_ - count)],
                static_cast<size_t>(count),
                blank);
    markRowDirty(cursor_.y);
}

void TerminalGrid::insertCharacters(int count) {
    if (count <= 0 || cursor_.y < 0 || cursor_.y >= rows_) return;
    int startCol = std::clamp(cursor_.x, 0, cols_ - 1);
    count = std::min(count, cols_ - startCol);
    auto& screen = currentScreen();
    size_t rowOffset = static_cast<size_t>(cursor_.y * cols_);

    int moveCount = cols_ - startCol - count;
    if (moveCount > 0) {
        std::copy_backward(&screen.back[rowOffset + static_cast<size_t>(startCol)],
                           &screen.back[rowOffset + static_cast<size_t>(startCol + moveCount)],
                           &screen.back[rowOffset + static_cast<size_t>(cols_)]);
    }
    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    std::fill_n(&screen.back[rowOffset + static_cast<size_t>(startCol)],
                static_cast<size_t>(count),
                blank);
    markRowDirty(cursor_.y);
}

void TerminalGrid::deleteLines(int count) {
    if (count <= 0 || cursor_.y < scrollTop_ || cursor_.y > scrollBottom_) return;
    int regionHeight = scrollBottom_ - cursor_.y + 1;
    count = std::min(count, regionHeight);

    auto& screen = currentScreen();
    int rowsToMove = regionHeight - count;
    if (rowsToMove > 0) {
        size_t dst = static_cast<size_t>(cursor_.y * cols_);
        size_t src = static_cast<size_t>((cursor_.y + count) * cols_);
        size_t numCells = static_cast<size_t>(rowsToMove * cols_);
        std::copy_n(&screen.back[src], numCells, &screen.back[dst]);
    }

    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    size_t blankStart = static_cast<size_t>((scrollBottom_ - count + 1) * cols_);
    size_t blankCount = static_cast<size_t>(count * cols_);
    std::fill_n(&screen.back[blankStart], blankCount, blank);

    for (int r = cursor_.y; r <= scrollBottom_; ++r) {
        markRowDirty(r);
    }
}

void TerminalGrid::insertLines(int count) {
    if (count <= 0 || cursor_.y < scrollTop_ || cursor_.y > scrollBottom_) return;
    int regionHeight = scrollBottom_ - cursor_.y + 1;
    count = std::min(count, regionHeight);

    auto& screen = currentScreen();
    int rowsToMove = regionHeight - count;
    if (rowsToMove > 0) {
        size_t srcStart = static_cast<size_t>(cursor_.y * cols_);
        size_t srcEnd = static_cast<size_t>((cursor_.y + rowsToMove) * cols_);
        size_t dstEnd = static_cast<size_t>((scrollBottom_ + 1) * cols_);
        std::copy_backward(&screen.back[srcStart], &screen.back[srcEnd], &screen.back[dstEnd]);
    }

    tui::Cell blank{' ', activeFg_, activeBg_, 0};
    size_t blankStart = static_cast<size_t>(cursor_.y * cols_);
    size_t blankCount = static_cast<size_t>(count * cols_);
    std::fill_n(&screen.back[blankStart], blankCount, blank);

    for (int r = cursor_.y; r <= scrollBottom_; ++r) {
        markRowDirty(r);
    }
}

void TerminalGrid::setScrollRegion(int top, int bottom) {
    if (top < 0) top = 0;
    if (bottom >= rows_ || bottom < 0) bottom = rows_ - 1;
    if (top >= bottom) {
        top = 0;
        bottom = rows_ - 1;
    }
    scrollTop_ = top;
    scrollBottom_ = bottom;
    currentScreen().scrollTop = top;
    currentScreen().scrollBottom = bottom;
    setCursorPos(0, 0); // VT100 standard: setting DECSTBM moves cursor to home (0, 0)
}

void TerminalGrid::setAlternateScreen(bool enable) {
    if (enable == usingAltScreen_) return;

    // Save active state into the leaving screen buffer
    currentScreen().cursor = cursor_;
    currentScreen().savedCursor = savedCursor_;
    currentScreen().scrollTop = scrollTop_;
    currentScreen().scrollBottom = scrollBottom_;
    currentScreen().mouseTrackingMode = mouseTrackingMode_;
    currentScreen().sgrMouse = sgrMouse_;

    usingAltScreen_ = enable;

    // Restore active state from entering screen buffer
    cursor_ = currentScreen().cursor;
    savedCursor_ = currentScreen().savedCursor;
    scrollTop_ = currentScreen().scrollTop;
    scrollBottom_ = currentScreen().scrollBottom;
    mouseTrackingMode_ = currentScreen().mouseTrackingMode;
    sgrMouse_ = currentScreen().sgrMouse;

    markAllDirty();
}

std::string TerminalGrid::dumpRowText(int row) const {
    if (row < 0 || row >= rows_) return "";
    std::string out;
    const auto& screen = currentScreen();
    size_t offset = static_cast<size_t>(row * cols_);
    for (int c = 0; c < cols_; ++c) {
        char32_t cp = screen.back[offset + c].codepoint;
        if (cp == 0) cp = ' ';
        out += codepointToUtf8(cp);
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string TerminalGrid::dumpScreenText() const {
    std::string out;
    for (int r = 0; r < rows_; ++r) {
        out += dumpRowText(r);
        if (r < rows_ - 1) {
            out += "\n";
        }
    }
    return out;
}

} // namespace eatsbits::terminal
