#ifndef EATS_COLOR_PICKER_DIALOG_HPP
#define EATS_COLOR_PICKER_DIALOG_HPP

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

struct ColorPaletteCategory {
    std::string name;
    std::vector<Color> colors;
};

/**
 * ColorPickerDialog: Studio track & clip accent color picker.
 * Features 4 curated Eatsbeats palette categories, HSL sliders,
 * Hex code display, and Before/After preview.
 */
class ColorPickerDialog {
public:
    ColorPickerDialog();
    ~ColorPickerDialog() = default;

    void open(const Color& initialColor, const std::string& title = "SELECT TRACK COLOR", size_t targetTrackIndex = 0);
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] const Color& getSelectedColor() const noexcept { return currentColor_; }
    void setSelectedColor(const Color& col) noexcept;

    [[nodiscard]] size_t getTargetTrackIndex() const noexcept { return targetTrackIndex_; }
    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }

    std::function<void(const Color& color, size_t targetTrackIdx)> onColorSelected;

    static Color hslToRgb(float h, float s, float l) noexcept;
    static void rgbToHsl(const Color& col, float& h, float& s, float& l) noexcept;
    static std::string colorToHex(const Color& col);

private:
    void initPaletteCategories();

    bool isOpen_{false};
    Color initialColor_{1.0f, 0.55f, 0.0f, 1.0f};
    Color currentColor_{1.0f, 0.55f, 0.0f, 1.0f};
    std::string title_{"SELECT TRACK COLOR"};
    size_t targetTrackIndex_{0};

    // HSL Components
    float hue_{33.0f};       // 0..360
    float saturation_{1.0f}; // 0..1
    float lightness_{0.5f};  // 0..1

    std::vector<ColorPaletteCategory> categories_{};

    Rect2D dialogBounds_{};
    std::vector<std::vector<Rect2D>> swatchBounds_{};

    Rect2D hueSliderBounds_{};
    Rect2D satSliderBounds_{};
    Rect2D lightSliderBounds_{};
    Rect2D beforeSwatchBounds_{};
    Rect2D afterSwatchBounds_{};
    Rect2D applyBtnBounds_{};
    Rect2D cancelBtnBounds_{};

    bool isDraggingHue_{false};
    bool isDraggingSat_{false};
    bool isDraggingLight_{false};
};

} // namespace eatsbits::ui

#endif // EATS_COLOR_PICKER_DIALOG_HPP
