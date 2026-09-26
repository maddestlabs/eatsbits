#pragma once

#include "eatsbits/tui/types.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <cmath>

namespace eatsbits::tui {

class CellSurface {
public:
    CellSurface(int width = 80, int height = 24);
    ~CellSurface() = default;

    void resize(int width, int height);

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    int getBrailleWidth() const { return width_ * 2; }
    int getBrailleHeight() const { return height_ * 4; }

    void clear(Color bg = Color::DefaultBg());
    void fill(const Cell& cell);

    // Cell-level rendering
    void setCell(int x, int y, const Cell& cell);
    const Cell& getCell(int x, int y) const;
    Cell& getBackCell(int x, int y);

    // Text and UI rendering
    void drawText(int x, int y, std::string_view text, Color fg = Color::DefaultFg(), Color bg = Color::DefaultBg(), uint8_t attrs = 0);
    void drawBox(const Rect& rect, Color borderFg = Color::Gray(), Color bg = Color::DefaultBg(), bool doubleLine = false, std::string_view title = "");
    void fillRect(const Rect& rect, Color bg, char32_t fillChar = ' ');

    // Sub-pixel Braille Graphics (2x4 dots per character cell)
    void setBrailleDot(int subX, int subY, bool state, Color fg = Color::AcidAmber(), Color bg = Color::DefaultBg());
    void drawBrailleLine(int x0, int y0, int x1, int y1, Color fg = Color::AcidAmber(), Color bg = Color::DefaultBg());
    void clearBrailleArea(const Rect& cellRect, Color bg = Color::DefaultBg());

    // Audio Visualizer Helpers
    void drawHorizontalMeter(int x, int y, int width, float value, float peak = -1.0f);
    void drawVerticalMeter(int x, int y, int height, float value, float peak = -1.0f);

    // Buffer swapping & dirty state
    void swapBuffers();
    bool isRowDirty(int y) const;
    bool hasAnyDirty() const { return anyDirty_; }
    void markAllDirty();

    // Direct buffer access for diff renderer & Wasm WebGPU zero-copy memory mapping
    const Cell* getFrontBuffer() const { return frontBuffer_.data(); }
    const Cell* getBackBuffer() const { return backBuffer_.data(); }
    Cell* getBackBuffer() { return backBuffer_.data(); }
    size_t getBufferCellCount() const { return frontBuffer_.size(); }

private:
    int width_{0};
    int height_{0};
    std::vector<Cell> frontBuffer_;
    std::vector<Cell> backBuffer_;
    std::vector<bool> dirtyRows_;
    bool anyDirty_{true};

    static uint8_t getBrailleDotMask(int col, int row);
};

} // namespace eatsbits::tui
