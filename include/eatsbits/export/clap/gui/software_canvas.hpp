#ifndef EATS_EXPORT_CLAP_GUI_SOFTWARE_CANVAS_HPP
#define EATS_EXPORT_CLAP_GUI_SOFTWARE_CANVAS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

namespace eatsbits::export_clap::gui {

/**
 * High-performance 2D vector software rasterizer rendering into a 32-bit
 * BGRA pixel buffer with sub-pixel antialiasing and zero host GPU context collisions.
 *
 * Designed specifically for micro-runtime CLAP plugins:
 * - Standalone: Zero dependencies on external graphics libraries (no OpenGL/Vulkan/DirectX/FreeType).
 * - Resolution-Independent: All primitives (circles, arcs, bevels, lines, vector glyphs)
 *   are evaluated analytically against the pixel grid.
 * - Platform Native: Pixels are stored in 32-bit little-endian BGRA (0x00RRGGBB / 0xAARRGGBB),
 *   allowing single-pass blitting via Win32 SetDIBitsToDevice, macOS CGContextDrawImage,
 *   or Linux XPutImage.
 */
class SoftwareCanvas {
public:
    SoftwareCanvas() = default;
    SoftwareCanvas(uint32_t width, uint32_t height, double scale = 1.0);

    void resize(uint32_t width, uint32_t height, double scale = 1.0);

    [[nodiscard]] uint32_t width() const noexcept { return width_; }
    [[nodiscard]] uint32_t height() const noexcept { return height_; }
    [[nodiscard]] double scale() const noexcept { return scale_; }
    [[nodiscard]] const uint32_t* pixels() const noexcept { return pixels_.data(); }
    [[nodiscard]] uint32_t* pixels() noexcept { return pixels_.data(); }
    [[nodiscard]] size_t pixelCount() const noexcept { return pixels_.size(); }

    // --- Color Helpers ---

    static constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) noexcept {
        // Little-endian BGRA memory layout
        return static_cast<uint32_t>(b) |
               (static_cast<uint32_t>(g) << 8) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(a) << 24);
    }

    static inline uint32_t rgbaf(float r, float g, float b, float a = 1.0f) noexcept {
        const auto clampByte = [](float v) -> uint8_t {
            return static_cast<uint8_t>(std::clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
        };
        return rgba(clampByte(r), clampByte(g), clampByte(b), clampByte(a));
    }

    // --- Drawing Operations ---

    void clear(uint32_t color) noexcept;

    /// Procedural brushed aluminum texture with fine horizontal grain.
    void drawBrushedMetal(int x, int y, int w, int h, uint32_t baseCol, uint32_t grainCol, float intensity) noexcept;

    void drawRect(int x, int y, int w, int h, uint32_t color) noexcept;
    void drawRectGradientV(int x, int y, int w, int h, uint32_t colTop, uint32_t colBottom) noexcept;
    void drawRectOutline(int x, int y, int w, int h, uint32_t color, float strokeWidth = 1.0f) noexcept;

    void drawRoundedRect(float x, float y, float w, float h, float radius, uint32_t color) noexcept;
    void drawRoundedRectOutline(float x, float y, float w, float h, float radius, uint32_t color, float strokeWidth = 1.0f) noexcept;

    void drawCircle(float cx, float cy, float radius, uint32_t color) noexcept;
    void drawCircleOutline(float cx, float cy, float radius, uint32_t color, float strokeWidth = 1.0f) noexcept;
    void drawCircleRadialGradient(float cx, float cy, float radius,
                                 uint32_t innerCol, uint32_t outerCol,
                                 float offX = 0.0f, float offY = 0.0f) noexcept;

    void drawArc(float cx, float cy, float radius, float startAngle, float endAngle,
                 uint32_t color, float strokeWidth = 1.0f) noexcept;

    void drawLine(float x0, float y0, float x1, float y1, uint32_t color, float strokeWidth = 1.0f) noexcept;

    void drawCircleDropShadow(float cx, float cy, float radius, float blurRadius,
                             float lightAngle, float alpha = 0.5f) noexcept;

    /// Fluted metallic knob perimeter with N·L specular lighting.
    void drawKnurledRing(float cx, float cy, float rInner, float rOuter, int numTeeth,
                        float currentAngle, float lightAngle,
                        uint32_t baseCol, uint32_t specCol, uint32_t shadowCol) noexcept;

    // --- Vector Typography ---

    void drawString(std::string_view text, float x, float y, float scale, uint32_t color, float strokeWidth = 1.2f) noexcept;
    void drawStringCentered(std::string_view text, float cx, float cy, float scale, uint32_t color, float strokeWidth = 1.2f) noexcept;
    [[nodiscard]] float measureStringWidth(std::string_view text, float scale) const noexcept;

    // Pixel access with alpha blending
    inline void blendPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) noexcept {
        if (x < 0 || x >= static_cast<int>(width_) || y < 0 || y >= static_cast<int>(height_) || a == 0) {
            return;
        }
        uint32_t& dest = pixels_[static_cast<size_t>(y) * width_ + static_cast<size_t>(x)];
        if (a == 255) {
            dest = rgba(r, g, b, 255);
            return;
        }
        const uint8_t db = dest & 0xFF;
        const uint8_t dg = (dest >> 8) & 0xFF;
        const uint8_t dr = (dest >> 16) & 0xFF;
        const uint32_t invA = 255 - a;
        const uint8_t outR = static_cast<uint8_t>((r * a + dr * invA) / 255);
        const uint8_t outG = static_cast<uint8_t>((g * a + dg * invA) / 255);
        const uint8_t outB = static_cast<uint8_t>((b * a + db * invA) / 255);
        dest = rgba(outR, outG, outB, 255);
    }

private:
    uint32_t width_{800};
    uint32_t height_{480};
    double scale_{1.0};
    std::vector<uint32_t> pixels_;
};

} // namespace eatsbits::export_clap::gui

#endif // EATS_EXPORT_CLAP_GUI_SOFTWARE_CANVAS_HPP
