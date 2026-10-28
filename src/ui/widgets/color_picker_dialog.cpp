#include "eatsbits/ui/widgets/color_picker_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

Color ColorPickerDialog::hslToRgb(float h, float s, float l) noexcept {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    l = std::clamp(l, 0.0f, 1.0f);

    float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c * 0.5f;

    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 60.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h < 120.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h < 180.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h < 240.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h < 300.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }

    return Color(r + m, g + m, b + m, 1.0f);
}

void ColorPickerDialog::rgbToHsl(const Color& col, float& h, float& s, float& l) noexcept {
    float r = std::clamp(col.r, 0.0f, 1.0f);
    float g = std::clamp(col.g, 0.0f, 1.0f);
    float b = std::clamp(col.b, 0.0f, 1.0f);

    float maxV = std::max({r, g, b});
    float minV = std::min({r, g, b});
    float delta = maxV - minV;

    l = (maxV + minV) * 0.5f;

    if (delta < 1e-4f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    s = (l > 0.5f) ? (delta / (2.0f - maxV - minV)) : (delta / (maxV + minV));

    if (maxV == r) {
        h = 60.0f * (std::fmod((g - b) / delta, 6.0f));
    } else if (maxV == g) {
        h = 60.0f * (((b - r) / delta) + 2.0f);
    } else {
        h = 60.0f * (((r - g) / delta) + 4.0f);
    }

    if (h < 0.0f) h += 360.0f;
}

std::string ColorPickerDialog::colorToHex(const Color& col) {
    int r = std::clamp(static_cast<int>(std::round(col.r * 255.0f)), 0, 255);
    int g = std::clamp(static_cast<int>(std::round(col.g * 255.0f)), 0, 255);
    int b = std::clamp(static_cast<int>(std::round(col.b * 255.0f)), 0, 255);

    std::ostringstream ss;
    ss << "#"
       << std::hex << std::uppercase << std::setfill('0')
       << std::setw(2) << r
       << std::setw(2) << g
       << std::setw(2) << b;
    return ss.str();
}

ColorPickerDialog::ColorPickerDialog() {
    initPaletteCategories();
}

void ColorPickerDialog::open(const Color& initialColor, const std::string& title, size_t targetTrackIndex) {
    initialColor_ = initialColor;
    currentColor_ = initialColor;
    title_ = title;
    targetTrackIndex_ = targetTrackIndex;
    isOpen_ = true;
    isDraggingHue_ = false;
    isDraggingSat_ = false;
    isDraggingLight_ = false;

    rgbToHsl(currentColor_, hue_, saturation_, lightness_);
}

void ColorPickerDialog::setSelectedColor(const Color& col) noexcept {
    currentColor_ = col;
    rgbToHsl(currentColor_, hue_, saturation_, lightness_);
}

void ColorPickerDialog::initPaletteCategories() {
    categories_.clear();

    // 1. VIBRANT PALETTE
    categories_.push_back({
        "VIBRANT PALETTE",
        {
            Color(1.00f, 0.09f, 0.27f), // Crimson #FF1744
            Color(1.00f, 0.32f, 0.32f), // Light Red #FF5252
            Color(1.00f, 0.43f, 0.00f), // Deep Orange #FF6D00
            Color(1.00f, 0.67f, 0.00f), // Amber #FFAB00
            Color(1.00f, 0.84f, 0.00f), // Gold #FFD700
            Color(0.68f, 0.92f, 0.00f), // Lime #AEEA00
            Color(0.46f, 1.00f, 0.01f), // Bright Green #76FF03
            Color(0.00f, 0.90f, 0.46f), // Emerald #00E676
            Color(0.11f, 0.91f, 0.71f), // Mint #1DE9B6
            Color(0.00f, 0.69f, 1.00f), // Light Sky #00B0FF
            Color(0.16f, 0.47f, 1.00f), // Vivid Blue #2979FF
            Color(0.40f, 0.12f, 1.00f), // Deep Violet #651FFF
            Color(0.84f, 0.00f, 0.98f), // Magenta Violet #D500F9
            Color(0.96f, 0.00f, 0.34f), // Rose Pink #F50057
            Color(0.88f, 0.25f, 0.98f), // Orchid #E040FB
            Color(0.20f, 0.60f, 1.00f), // Sky Blue #3399FF
        }
    });

    // 2. CLASSIC SYNTH & STUDIO
    categories_.push_back({
        "CLASSIC SYNTH & STUDIO",
        {
            Color(0.97f, 0.50f, 0.00f), // Amber Glow #F77F00
            Color(0.90f, 0.22f, 0.27f), // Drum Red #E63946
            Color(0.00f, 0.47f, 0.71f), // Indigo Blue #0077B6
            Color(0.16f, 0.62f, 0.56f), // Analog Teal #2A9D8F
            Color(0.88f, 0.66f, 0.43f), // Brass Gold #E0A96D
            Color(0.45f, 0.04f, 0.72f), // Deep Violet #7209B7
            Color(0.27f, 0.48f, 0.62f), // Steel Slate #457B9D
            Color(0.83f, 0.64f, 0.45f), // Warm Wood #D4A373
        }
    });

    // 3. PASTELS & SUBTLE
    categories_.push_back({
        "PASTELS & SUBTLE",
        {
            Color(1.00f, 0.71f, 0.64f), // Soft Peach #FFB4A2
            Color(0.89f, 0.73f, 0.85f), // Lavender Dust #E2BAE1
            Color(0.70f, 0.82f, 0.89f), // Powder Blue #B3D1E3
            Color(0.74f, 0.88f, 0.77f), // Sage Green #BDDFC5
            Color(1.00f, 0.92f, 0.65f), // Pale Lemon #FFEB99
            Color(0.98f, 0.78f, 0.76f), // Blush Pink #FAC7C2
            Color(0.80f, 0.77f, 0.93f), // Lilac Mist #CCBEEE
            Color(0.72f, 0.88f, 0.86f), // Soft Aqua #B8E0DB
        }
    });

    // 4. GRAYSCALE
    categories_.push_back({
        "GRAYSCALE",
        {
            Color(1.00f, 1.00f, 1.00f), // Pure White #FFFFFF
            Color(0.86f, 0.86f, 0.88f), // Platinum #DCDCE0
            Color(0.70f, 0.71f, 0.74f), // Silver #B3B5BD
            Color(0.52f, 0.54f, 0.58f), // Steel #858A94
            Color(0.36f, 0.38f, 0.42f), // Slate #5C616B
            Color(0.24f, 0.25f, 0.28f), // Charcoal #3D4047
            Color(0.14f, 0.15f, 0.17f), // Onyx #24262B
            Color(0.06f, 0.06f, 0.08f), // Carbon Black #0F0F14
        }
    });
}

void ColorPickerDialog::layout(float screenW, float screenH) {
    const float w = std::min(560.0f, screenW - 24.0f);
    const float h = std::min(520.0f, screenH - 24.0f);
    const float x = (screenW - w) * 0.5f;
    const float y = (screenH - h) * 0.5f;

    if (dialogBounds_.x == x && dialogBounds_.y == y &&
        dialogBounds_.w == w && dialogBounds_.h == h &&
        !swatchBounds_.empty()) {
        return;
    }

    dialogBounds_ = Rect2D(x, y, w, h);

    // Calculate layout for palette swatches
    float curY = y + 54.0f;
    if (swatchBounds_.size() != categories_.size()) {
        swatchBounds_.resize(categories_.size());
    }

    for (size_t c = 0; c < categories_.size(); ++c) {
        const auto& cat = categories_[c];
        curY += 20.0f; // Category header label space

        size_t numCols = cat.colors.size() > 8 ? 8 : cat.colors.size();
        const float swatchGapX = 5.0f;
        const float swatchGapY = 5.0f;
        const float swatchW = (w - 48.0f - (numCols - 1) * swatchGapX) / static_cast<float>(numCols);
        const float swatchH = std::clamp(swatchW * 0.36f, 16.0f, 20.0f);

        if (swatchBounds_[c].size() != cat.colors.size()) {
            swatchBounds_[c].resize(cat.colors.size());
        }

        for (size_t i = 0; i < cat.colors.size(); ++i) {
            size_t col = i % numCols;
            size_t row = i / numCols;
            float sx = x + 24.0f + col * (swatchW + swatchGapX);
            float sy = curY + row * (swatchH + swatchGapY);
            swatchBounds_[c][i] = Rect2D(sx, sy, swatchW, swatchH);
        }

        size_t totalRows = (cat.colors.size() + numCols - 1) / numCols;
        curY += totalRows * (swatchH + swatchGapY) + 6.0f;
    }

    // HSL Sliders & Preview Swatches
    float hslStartY = curY + 14.0f;
    const float sliderW = w - 48.0f - 110.0f;
    const float sliderH = 14.0f;
    hueSliderBounds_ = Rect2D(x + 24.0f, hslStartY + 16.0f, sliderW, sliderH);
    satSliderBounds_ = Rect2D(x + 24.0f, hueSliderBounds_.bottom() + 24.0f, sliderW, sliderH);
    lightSliderBounds_ = Rect2D(x + 24.0f, satSliderBounds_.bottom() + 24.0f, sliderW, sliderH);

    // Before / After Preview Swatches
    const float prevX = x + w - 24.0f - 96.0f;
    beforeSwatchBounds_ = Rect2D(prevX, hslStartY + 16.0f, 44.0f, 44.0f);
    afterSwatchBounds_ = Rect2D(prevX + 48.0f, hslStartY + 16.0f, 48.0f, 44.0f);

    // Bottom Action Buttons
    const float botY = y + h - 42.0f;
    applyBtnBounds_ = Rect2D(x + w - 134.0f, botY, 110.0f, 30.0f);
    cancelBtnBounds_ = Rect2D(applyBtnBounds_.x - 90.0f, botY, 80.0f, 30.0f);
}

void ColorPickerDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, 4000.0f, 4000.0f, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Dialog Chassis with current color glow border
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 12.0f, currentColor_, 1.8f);

    // Header Title with current color chip
    drawRoundedRect(r, dialogBounds_.x + 24.0f, dialogBounds_.y + 22.0f, 12.0f, 12.0f, 2.0f, currentColor_);
    drawText(r, title_, dialogBounds_.x + 44.0f, dialogBounds_.y + 20.0f, 13.0f, theme.textPrimary);
    drawText(r, "Choose from curated studio palettes or tweak custom HSL color values", dialogBounds_.x + 44.0f, dialogBounds_.y + 36.0f, 9.5f, theme.textMuted);

    // 3. Swatch Palettes
    for (size_t c = 0; c < categories_.size() && c < swatchBounds_.size(); ++c) {
        const auto& cat = categories_[c];
        const auto& swatches = swatchBounds_[c];
        if (swatches.empty()) continue;

        drawText(r, cat.name, dialogBounds_.x + 24.0f, swatches[0].y - 15.0f, 8.5f, theme.textMuted);

        for (size_t i = 0; i < cat.colors.size() && i < swatches.size(); ++i) {
            const auto& sb = swatches[i];
            const auto& col = cat.colors[i];

            bool isSelected = (std::abs(col.r - currentColor_.r) < 0.02f &&
                               std::abs(col.g - currentColor_.g) < 0.02f &&
                               std::abs(col.b - currentColor_.b) < 0.02f);

            drawRoundedRect(r, sb.x, sb.y, sb.w, sb.h, 3.5f, col);
            if (isSelected) {
                drawRoundedRectOutline(r, sb.x - 2.0f, sb.y - 2.0f, sb.w + 4.0f, sb.h + 4.0f, 4.5f, Color(1.0f, 1.0f, 1.0f, 0.95f), 2.0f);
            } else {
                drawRoundedRectOutline(r, sb.x, sb.y, sb.w, sb.h, 3.5f, Color(0.0f, 0.0f, 0.0f, 0.35f), 1.0f);
            }
        }
    }

    // 4. HSL Sliders
    auto drawHslSlider = [&](const Rect2D& b, const std::string& name, float val, float maxV, const Color& thumbCol) {
        drawText(r, name, b.x, b.y - 14.0f, 8.5f, theme.textMuted);

        // Track background
        drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, Color(0.10f, 0.14f, 0.20f, 1.0f));
        drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, theme.borderSubtle, 1.0f);

        // Fill bar
        float norm = std::clamp(val / maxV, 0.0f, 1.0f);
        drawRoundedRect(r, b.x, b.y, b.w * norm, b.h, 4.0f, thumbCol);

        // Thumb indicator
        float tx = b.x + b.w * norm;
        drawCircle(r, tx, b.y + b.h * 0.5f, 6.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
        drawCircleOutline(r, tx, b.y + b.h * 0.5f, 6.0f, Color(0.1f, 0.1f, 0.1f, 1.0f), 1.2f);
    };

    drawHslSlider(hueSliderBounds_, "HUE (0 - 360)", hue_, 360.0f, hslToRgb(hue_, 1.0f, 0.5f));
    drawHslSlider(satSliderBounds_, "SATURATION (0 - 100%)", saturation_ * 100.0f, 100.0f, hslToRgb(hue_, saturation_, 0.5f));
    drawHslSlider(lightSliderBounds_, "LIGHTNESS (0 - 100%)", lightness_ * 100.0f, 100.0f, hslToRgb(hue_, saturation_, lightness_));

    // 5. Before / After Preview Swatches
    drawText(r, "BEFORE", beforeSwatchBounds_.x, beforeSwatchBounds_.y - 14.0f, 8.0f, theme.textMuted);
    drawRoundedRect(r, beforeSwatchBounds_.x, beforeSwatchBounds_.y, beforeSwatchBounds_.w, beforeSwatchBounds_.h, 4.0f, initialColor_);
    drawRoundedRectOutline(r, beforeSwatchBounds_.x, beforeSwatchBounds_.y, beforeSwatchBounds_.w, beforeSwatchBounds_.h, 4.0f, theme.borderSubtle, 1.0f);

    drawText(r, "AFTER", afterSwatchBounds_.x, afterSwatchBounds_.y - 14.0f, 8.0f, theme.textMuted);
    drawRoundedRect(r, afterSwatchBounds_.x, afterSwatchBounds_.y, afterSwatchBounds_.w, afterSwatchBounds_.h, 4.0f, currentColor_);
    drawRoundedRectOutline(r, afterSwatchBounds_.x, afterSwatchBounds_.y, afterSwatchBounds_.w, afterSwatchBounds_.h, 4.0f, Color(1.0f, 1.0f, 1.0f, 0.8f), 1.5f);

    // Hex code readout below after swatch
    std::string hexStr = colorToHex(currentColor_);
    drawText(r, hexStr, afterSwatchBounds_.x - 12.0f, afterSwatchBounds_.bottom() + 7.0f, 9.0f, theme.textPrimary);

    // 6. Action Buttons
    drawButton(r, cancelBtnBounds_, "CANCEL", theme.panelHeader, theme.borderSubtle, theme.textSecondary, 10.0f);
    drawButton(r, applyBtnBounds_, "SELECT COLOR", currentColor_, theme.borderFocus, Color(0.05f, 0.08f, 0.12f, 1.0f), 10.0f);
}

bool ColorPickerDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    // Handle ongoing slider dragging regardless of whether cursor left dialog bounds
    if (ev.action == PointerAction::Move && (isDraggingHue_ || isDraggingSat_ || isDraggingLight_)) {
        if (isDraggingHue_) {
            float norm = std::clamp((ev.x - hueSliderBounds_.x) / hueSliderBounds_.w, 0.0f, 1.0f);
            hue_ = norm * 360.0f;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
        if (isDraggingSat_) {
            float norm = std::clamp((ev.x - satSliderBounds_.x) / satSliderBounds_.w, 0.0f, 1.0f);
            saturation_ = norm;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
        if (isDraggingLight_) {
            float norm = std::clamp((ev.x - lightSliderBounds_.x) / lightSliderBounds_.w, 0.0f, 1.0f);
            lightness_ = norm;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
    }

    if ((ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) &&
        (isDraggingHue_ || isDraggingSat_ || isDraggingLight_)) {
        isDraggingHue_ = false;
        isDraggingSat_ = false;
        isDraggingLight_ = false;
        return true;
    }

    if (!dialogBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            close();
            return true;
        }
        return true; // Absorb events behind modal while modal is open
    }

    if (ev.action == PointerAction::Down) {
        // Swatch clicks
        for (size_t c = 0; c < categories_.size() && c < swatchBounds_.size(); ++c) {
            const auto& swatches = swatchBounds_[c];
            for (size_t i = 0; i < swatches.size() && i < categories_[c].colors.size(); ++i) {
                if (swatches[i].contains(ev.x, ev.y)) {
                    setSelectedColor(categories_[c].colors[i]);
                    return true;
                }
            }
        }

        // HSL Sliders
        if (hueSliderBounds_.contains(ev.x, ev.y)) {
            isDraggingHue_ = true;
            float norm = std::clamp((ev.x - hueSliderBounds_.x) / hueSliderBounds_.w, 0.0f, 1.0f);
            hue_ = norm * 360.0f;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
        if (satSliderBounds_.contains(ev.x, ev.y)) {
            isDraggingSat_ = true;
            float norm = std::clamp((ev.x - satSliderBounds_.x) / satSliderBounds_.w, 0.0f, 1.0f);
            saturation_ = norm;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
        if (lightSliderBounds_.contains(ev.x, ev.y)) {
            isDraggingLight_ = true;
            float norm = std::clamp((ev.x - lightSliderBounds_.x) / lightSliderBounds_.w, 0.0f, 1.0f);
            lightness_ = norm;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }

        // Buttons
        if (applyBtnBounds_.contains(ev.x, ev.y)) {
            if (onColorSelected) onColorSelected(currentColor_, targetTrackIndex_);
            close();
            return true;
        }
        if (cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingHue_) {
            float norm = std::clamp((ev.x - hueSliderBounds_.x) / hueSliderBounds_.w, 0.0f, 1.0f);
            hue_ = norm * 360.0f;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
        if (isDraggingSat_) {
            float norm = std::clamp((ev.x - satSliderBounds_.x) / satSliderBounds_.w, 0.0f, 1.0f);
            saturation_ = norm;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
        if (isDraggingLight_) {
            float norm = std::clamp((ev.x - lightSliderBounds_.x) / lightSliderBounds_.w, 0.0f, 1.0f);
            lightness_ = norm;
            currentColor_ = hslToRgb(hue_, saturation_, lightness_);
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        isDraggingHue_ = false;
        isDraggingSat_ = false;
        isDraggingLight_ = false;
        return true;
    }

    return true;
}

bool ColorPickerDialog::handleKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (!isOpen_) return false;
    if (action == 1) { // GLFW_PRESS
        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            return true;
        }
        if (key == 257) { // GLFW_KEY_ENTER
            if (onColorSelected) onColorSelected(currentColor_, targetTrackIndex_);
            close();
            return true;
        }
    }
    return true;
}

} // namespace eatsbits::ui
