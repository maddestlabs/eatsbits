#pragma once

#include <string>
#include <string_view>
#include "geometry.hpp"
#include "batch_renderer_2d.hpp"
#include "theme.hpp"

namespace eatsbits::ui {

void drawVectorString(BatchRenderer2D& r, std::string_view str, float x, float y, float scale, float red, float green, float blue, float alpha = 1.0f);
inline void drawVectorString(BatchRenderer2D& r, std::string_view str, float x, float y, float scale, const Color& c) {
    drawVectorString(r, str, x, y, scale, c.r, c.g, c.b, c.a);
}

void drawVectorString(std::string_view str, float x, float y, float scale, float r, float g, float b, float a = 1.0f);
inline void drawVectorString(std::string_view str, float x, float y, float scale, const Color& c) {
    drawVectorString(str, x, y, scale, c.r, c.g, c.b, c.a);
}

void drawVectorStringCentered(BatchRenderer2D& r, std::string_view str, float cx, float cy, float scale, float red, float green, float blue, float alpha = 1.0f);
inline void drawVectorStringCentered(BatchRenderer2D& r, std::string_view str, float cx, float cy, float scale, const Color& c) {
    drawVectorStringCentered(r, str, cx, cy, scale, c.r, c.g, c.b, c.a);
}

void drawVectorStringCentered(std::string_view str, float cx, float cy, float scale, float r, float g, float b, float a = 1.0f);
inline void drawVectorStringCentered(std::string_view str, float cx, float cy, float scale, const Color& c) {
    drawVectorStringCentered(str, cx, cy, scale, c.r, c.g, c.b, c.a);
}

void drawMonoString(BatchRenderer2D& r, std::string_view str, float x, float y, float scale, float red, float green, float blue, float alpha = 1.0f);
inline void drawMonoString(BatchRenderer2D& r, std::string_view str, float x, float y, float scale, const Color& c) {
    drawMonoString(r, str, x, y, scale, c.r, c.g, c.b, c.a);
}

void drawMonoString(std::string_view str, float x, float y, float scale, float r, float g, float b, float a = 1.0f);
inline void drawMonoString(std::string_view str, float x, float y, float scale, const Color& c) {
    drawMonoString(str, x, y, scale, c.r, c.g, c.b, c.a);
}

void drawText(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
              float red, float green, float blue, float alpha = 1.0f);

inline void drawText(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                     const Color& col, float alpha = 1.0f) {
    drawText(r, str, x, y, size, col.r, col.g, col.b, alpha * col.a);
}

void drawCenteredText(BatchRenderer2D& r, std::string_view str, const Rect2D& bounds,
                      float size, const Color& col, float alpha = 1.0f);

inline void drawCenteredText(BatchRenderer2D& r, std::string_view str, float x, float y, float w, float h,
                             float size, const Color& col, float alpha = 1.0f) {
    drawCenteredText(r, str, Rect2D{x, y, w, h}, size, col, alpha);
}

inline void drawCenteredText(BatchRenderer2D& r, std::string_view str, float x, float y, float w, float h,
                             float size, float red, float green, float blue, float alpha = 1.0f) {
    drawCenteredText(r, str, Rect2D{x, y, w, h}, size, Color{red, green, blue, alpha}, 1.0f);
}

void drawVectorStringLeftAlignedY(BatchRenderer2D& r, std::string_view str, float x, float cy, float scale, float red, float green, float blue, float alpha = 1.0f);
inline void drawVectorStringLeftAlignedY(BatchRenderer2D& r, std::string_view str, float x, float cy, float scale, const Color& c) {
    drawVectorStringLeftAlignedY(r, str, x, cy, scale, c.r, c.g, c.b, c.a);
}

void drawVectorStringLeftAlignedY(std::string_view str, float x, float cy, float scale, float r, float g, float b, float a = 1.0f);
inline void drawVectorStringLeftAlignedY(std::string_view str, float x, float cy, float scale, const Color& c) {
    drawVectorStringLeftAlignedY(str, x, cy, scale, c.r, c.g, c.b, c.a);
}

void drawTextLeftAlignedY(BatchRenderer2D& r, std::string_view str, float x, float cy, float size,
                          float red, float green, float blue, float alpha = 1.0f);

inline void drawTextLeftAlignedY(BatchRenderer2D& r, std::string_view str, float x, float cy, float size,
                                 const Color& col, float alpha = 1.0f) {
    drawTextLeftAlignedY(r, str, x, cy, size, col.r, col.g, col.b, alpha * col.a);
}

void drawRotatedText(BatchRenderer2D& r, std::string_view str, float cx, float cy,
                     float angleDegrees, float size, const Color& col, float alpha = 1.0f);

void drawButton(BatchRenderer2D& r, const Rect2D& bounds, std::string_view label,
                const Color& bgCol, const Color& borderCol, const Color& textCol,
                float size = 10.5f, float cornerRadius = 4.0f, float borderWidth = 1.0f);

inline void drawButton(BatchRenderer2D& r, float x, float y, float w, float h, std::string_view label,
                       const Color& bgCol, const Color& borderCol, const Color& textCol,
                       float size = 10.5f, float cornerRadius = 4.0f, float borderWidth = 1.0f) {
    drawButton(r, Rect2D{x, y, w, h}, label, bgCol, borderCol, textCol, size, cornerRadius, borderWidth);
}

void drawPianoIcon(BatchRenderer2D& r, float x, float y, float w, float h, const Color& accentCol);

void drawPropertiesIcon(BatchRenderer2D& r, float x, float y, float w, float h, const Color& accentCol);

void drawScrewCloseButton(BatchRenderer2D& r, float cx, float cy, float radius, bool hovered, const Color& highlightColor);

void drawIconEdit(BatchRenderer2D& r, float x, float y, float size, const Color& c);

void drawDesignChipIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c);

void drawSlidersTuneIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c);

void drawFullscreenIcon(BatchRenderer2D& r, float x, float y, float size, const Color& c, float thickness = 1.5f);

void drawChevronUp(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness = 1.6f);

void drawChevronDown(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness = 1.6f);

void drawTrashIcon(BatchRenderer2D& r, float cx, float cy, float size, const Color& c, float thickness = 1.4f);

void drawTooltipBadge(BatchRenderer2D& r, std::string_view text, float targetX, float targetY,
                      const ThemeTokens& theme, bool placeBelow = false, float screenW = 0.0f,
                      float screenH = 0.0f, float minY = -1.0f, float maxY = -1.0f);

void drawMonoText(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                  float red, float green, float blue, float alpha = 1.0f);

inline void drawMonoText(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                         const Color& col, float alpha = 1.0f) {
    drawMonoText(r, str, x, y, size, col.r, col.g, col.b, alpha * col.a);
}

void drawMonoTextClipped(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                         float clipMinX, float clipMaxX,
                         float red, float green, float blue, float alpha = 1.0f);

inline void drawMonoTextClipped(BatchRenderer2D& r, std::string_view str, float x, float y, float size,
                                float clipMinX, float clipMaxX,
                                const Color& col, float alpha = 1.0f) {
    drawMonoTextClipped(r, str, x, y, size, clipMinX, clipMaxX, col.r, col.g, col.b, alpha * col.a);
}

inline void drawRect(BatchRenderer2D& r, float x, float y, float w, float h,
                     float red, float green, float blue, float alpha = 1.0f) {
    r.drawRect(x, y, w, h, red, green, blue, alpha);
}

inline void drawRect(BatchRenderer2D& r, float x, float y, float w, float h,
                     const Color& col, float alpha = 1.0f) {
    r.drawRect(x, y, w, h, col.r, col.g, col.b, alpha * col.a);
}

inline void drawRectGradient(BatchRenderer2D& r, float x, float y, float w, float h,
                             float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f) {
    r.drawRectGradient(x, y, w, h, r0, g0, b0, r1, g1, b1, a);
}

inline void drawRectGradient(BatchRenderer2D& r, float x, float y, float w, float h,
                             const Color& c0, const Color& c1) {
    r.drawRectGradient(x, y, w, h, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a);
}

inline void drawRoundedRect(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                            float red, float green, float blue, float alpha = 1.0f) {
    r.drawRoundedRect(x, y, w, h, radius, red, green, blue, alpha);
}

inline void drawRoundedRect(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                            const Color& col, float alpha = 1.0f) {
    r.drawRoundedRect(x, y, w, h, radius, col.r, col.g, col.b, alpha * col.a);
}

inline void drawRoundedRectGradient(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                    float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f) {
    r.drawRoundedRectGradient(x, y, w, h, radius, r0, g0, b0, r1, g1, b1, a);
}

inline void drawRoundedRectGradient(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                    const Color& c0, const Color& c1) {
    r.drawRoundedRectGradient(x, y, w, h, radius, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a);
}

inline void drawRoundedRectOutline(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                   float red, float green, float blue, float alpha = 1.0f, float lineWidth = 1.0f) {
    r.drawRoundedRectOutline(x, y, w, h, radius, red, green, blue, alpha, lineWidth);
}

inline void drawRoundedRectOutline(BatchRenderer2D& r, float x, float y, float w, float h, float radius,
                                   const Color& col, float alpha = 1.0f, float lineWidth = 1.0f) {
    r.drawRoundedRectOutline(x, y, w, h, radius, col.r, col.g, col.b, alpha * col.a, lineWidth);
}

inline void drawLine(BatchRenderer2D& r, float x0, float y0, float x1, float y1,
                     float red, float green, float blue, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawLine(x0, y0, x1, y1, red, green, blue, alpha, lineWidth);
}

inline void drawLine(BatchRenderer2D& r, float x0, float y0, float x1, float y1,
                     const Color& col, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawLine(x0, y0, x1, y1, col.r, col.g, col.b, alpha * col.a, lineWidth);
}

inline void drawCircle(BatchRenderer2D& r, float cx, float cy, float radius,
                       float red, float green, float blue, float alpha = 1.0f) {
    r.drawCircle(cx, cy, radius, red, green, blue, alpha);
}

inline void drawCircle(BatchRenderer2D& r, float cx, float cy, float radius,
                       const Color& col, float alpha = 1.0f) {
    r.drawCircle(cx, cy, radius, col.r, col.g, col.b, alpha * col.a);
}

inline void drawCircleRadialGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                     float innerR, float innerG, float innerB, float innerA,
                                     float outerR, float outerG, float outerB, float outerA,
                                     float offX = 0.0f, float offY = 0.0f, int segments = 36) {
    r.drawCircleRadialGradient(cx, cy, radius, innerR, innerG, innerB, innerA, outerR, outerG, outerB, outerA, offX, offY, segments);
}

inline void drawCircleRadialGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                     const Color& innerCol, const Color& outerCol,
                                     float offX = 0.0f, float offY = 0.0f, int segments = 36) {
    r.drawCircleRadialGradient(cx, cy, radius, innerCol.r, innerCol.g, innerCol.b, innerCol.a,
                               outerCol.r, outerCol.g, outerCol.b, outerCol.a, offX, offY, segments);
}

inline void drawCircleRadial3StopGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                          const Color& innerCol, const Color& midCol, const Color& outerCol,
                                          float offX = 0.0f, float offY = 0.0f, float midStop = 0.50f, int segments = 36) {
    r.drawCircleRadial3StopGradient(cx, cy, radius,
                                    innerCol.r, innerCol.g, innerCol.b, innerCol.a,
                                    midCol.r, midCol.g, midCol.b, midCol.a,
                                    outerCol.r, outerCol.g, outerCol.b, outerCol.a,
                                    offX, offY, midStop, segments);
}

inline void drawCircleLinearGradient(BatchRenderer2D& r, float cx, float cy, float radius,
                                     const Color& c0, const Color& c1,
                                     float angleRad = 1.5707963f, int segments = 36) {
    r.drawCircleLinearGradient(cx, cy, radius, c0.r, c0.g, c0.b, c0.a, c1.r, c1.g, c1.b, c1.a, angleRad, segments);
}

inline void drawCircleOutline(BatchRenderer2D& r, float cx, float cy, float radius,
                              float red, float green, float blue, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawCircleOutline(cx, cy, radius, red, green, blue, alpha, lineWidth);
}

inline void drawCircleOutline(BatchRenderer2D& r, float cx, float cy, float radius,
                              const Color& col, float alpha = 1.0f, float lineWidth = 1.5f) {
    r.drawCircleOutline(cx, cy, radius, col.r, col.g, col.b, alpha * col.a, lineWidth);
}

inline void drawTriangle(BatchRenderer2D& r, float x0, float y0, float x1, float y1, float x2, float y2,
                         float red, float green, float blue, float alpha = 1.0f) {
    r.drawTriangle(x0, y0, x1, y1, x2, y2, red, green, blue, alpha);
}

inline void drawTriangle(BatchRenderer2D& r, float x0, float y0, float x1, float y1, float x2, float y2,
                         const Color& col, float alpha = 1.0f) {
    r.drawTriangle(x0, y0, x1, y1, x2, y2, col.r, col.g, col.b, alpha * col.a);
}

float getMonoCharAdvance(float size);
void drawDebugText(BatchRenderer2D& r, float x, float y, float size, std::string_view text, const Color& col);

// =========================================================================
// Unified Composable Control Core (Knobs & Sliders)
// =========================================================================

enum class KnobLayoutMode : uint8_t {
    StackedVertical = 0, // Label top, knob center, value bottom (gui-minimal / standard)
    InlineRight = 1,     // SMAOLAB style: knob on left, value & label stacked on the right
    MicroInline = 2,     // Compact 16-24px dial for dense track headers and mixers
    OverlayOnHover = 3,  // Value swaps with label inside dial on hover
    BottomStacked = 4,   // gui-astral style: knob on top, label below, value pill badge below
    InlineLeft = 5       // Value & label stacked on left, knob on right
};

enum class KnobArcType : uint8_t {
    None = 0,
    Unipolar = 1,          // 0.0 -> 1.0 (Amount, Retune, Filter Cutoff)
    BipolarCenterZero = 2, // -1.0 -> +1.0 or 0.5 center (Pan, EQ Cut/Boost, Pitch)
    SteppedGraduations = 3 // Discrete ticks around perimeter
};

enum class RotorSurface : uint8_t {
    FlatVector = 0,      // Pure minimalist flat circle (gui-minimal, Bitwig, Ableton)
    SubtleGradient = 1,  // Soft modern 3-stop studio lighting (gui-variations, SMAOLAB)
    BrushedAluminum = 2, // Lathe metallic finish
    FlutedHardware = 3   // Vintage fluted ridges
};

struct KnobStyleCore {
    KnobLayoutMode layout{KnobLayoutMode::StackedVertical};
    KnobArcType arcType{KnobArcType::Unipolar};
    RotorSurface surface{RotorSurface::SubtleGradient};

    float arcThickness{2.2f};
    float arcRadiusOffset{3.5f};
    float pointerLengthRatio{0.75f};
    float pointerWidth{1.8f};

    int tickCount{0};
    bool hasCenterDetent{false};
    float centerDetentVal{0.5f};

    uint32_t filmstripTexture{0};
    int filmstripFrames{0};
    bool filmstripHorizontal{false};

    std::optional<Color> trackColor{std::nullopt};
    std::optional<Color> arcColor{std::nullopt};
    std::optional<Color> bodyColor{std::nullopt};
    std::optional<Color> pointerColor{std::nullopt};
    std::optional<Color> labelColor{std::nullopt};
    std::optional<Color> valueColor{std::nullopt};

    static KnobStyleCore minimal(const Color& accent, bool bipolar = false) {
        KnobStyleCore k;
        k.layout = KnobLayoutMode::StackedVertical;
        k.arcType = bipolar ? KnobArcType::BipolarCenterZero : KnobArcType::Unipolar;
        k.surface = RotorSurface::FlatVector;
        k.arcThickness = 2.2f;
        k.arcRadiusOffset = 3.5f;
        k.pointerLengthRatio = 0.72f;
        k.pointerWidth = 1.6f;
        k.arcColor = accent;
        k.hasCenterDetent = bipolar;
        return k;
    }

    static KnobStyleCore studio(const Color& accent, bool bipolar = false) {
        KnobStyleCore k;
        k.layout = KnobLayoutMode::InlineRight;
        k.arcType = bipolar ? KnobArcType::BipolarCenterZero : KnobArcType::Unipolar;
        k.surface = RotorSurface::SubtleGradient;
        k.arcThickness = 2.5f;
        k.arcRadiusOffset = 3.5f;
        k.pointerLengthRatio = 0.80f;
        k.pointerWidth = 2.0f;
        k.arcColor = accent;
        k.hasCenterDetent = bipolar;
        return k;
    }

    static KnobStyleCore micro(const Color& accent, bool bipolar = false) {
        KnobStyleCore k;
        k.layout = KnobLayoutMode::MicroInline;
        k.arcType = bipolar ? KnobArcType::BipolarCenterZero : KnobArcType::Unipolar;
        k.surface = RotorSurface::FlatVector;
        k.arcThickness = 1.8f;
        k.arcRadiusOffset = 2.0f;
        k.pointerLengthRatio = 0.70f;
        k.pointerWidth = 1.5f;
        k.arcColor = accent;
        k.hasCenterDetent = bipolar;
        return k;
    }
};

enum class SliderOrientation : uint8_t {
    Horizontal = 0,
    Vertical = 1
};

enum class SliderThumbStyle : uint8_t {
    None = 0,       // Fill without thumb
    PillThumb = 1,  // Rounded modern pill (Arranger track style)
    HardwareCap = 2 // Studio fader cap
};

struct SliderStyleCore {
    SliderOrientation orientation{SliderOrientation::Horizontal};
    SliderThumbStyle thumbStyle{SliderThumbStyle::PillThumb};
    bool bipolar{false}; // Center-zero for pan sliders
    float trackHeight{4.0f};
    float thumbRadius{5.0f};
    float cornerRadius{2.0f};
    std::optional<Color> trackColor{std::nullopt};
    std::optional<Color> fillColor{std::nullopt};
    std::optional<Color> thumbColor{std::nullopt};

    uint32_t filmstripTexture{0};
    int filmstripFrames{0};
    bool filmstripHorizontal{false};

    static SliderStyleCore horizontalTrack(const Color& accent, float trackH = 4.0f) {
        SliderStyleCore s;
        s.orientation = SliderOrientation::Horizontal;
        s.thumbStyle = SliderThumbStyle::PillThumb;
        s.trackHeight = trackH;
        s.fillColor = accent;
        return s;
    }

    static SliderStyleCore verticalFader(const Color& accent) {
        SliderStyleCore s;
        s.orientation = SliderOrientation::Vertical;
        s.thumbStyle = SliderThumbStyle::HardwareCap;
        s.trackHeight = 6.0f;
        s.fillColor = accent;
        return s;
    }
};

void drawUnifiedKnob(BatchRenderer2D& r, float cx, float cy, float radius, float normVal,
                     const KnobStyleCore& style,
                     std::string_view label = {}, std::string_view valueText = {},
                     const ThemeTokens* theme = nullptr,
                     bool isActive = false, bool isHovered = false);

void drawUnifiedSlider(BatchRenderer2D& r, const Rect2D& bounds, float normVal,
                      const SliderStyleCore& style,
                      const Color& accent,
                      const ThemeTokens* theme = nullptr,
                      bool isActive = false, bool isHovered = false);

} // namespace eatsbits::ui

