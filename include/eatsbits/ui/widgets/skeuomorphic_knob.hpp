#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include <string>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace eatsbits::ui {

/**
 * Aesthetic hardware knob rendering style.
 */
enum class KnobStyle {
    Standard,
    Tb303Potentiometer,
    Tb303Selector,
    ChromeFluted,
    VintageBakelite
};

/**
 * Skeuomorphic rotary hardware knob with arc indicator, center rotor,
 * pointer needle, linear vertical mouse drag handling, fine-tuning, and parameter readouts.
 */
class SkeuomorphicKnob {
public:
    Rect bounds;
    std::string paramName{"CUTOFF"};
    std::string unit{"Hz"};
    float normValue{0.5f}; // [0.0, 1.0]
    float minDisplay{20.0f};
    float maxDisplay{20000.0f};
    bool isBipolar{false};   // True for PAN (-1 to +1) or EQ gain (-12dB to +12dB)
    bool isHovered{false};
    bool isDragging{false};
    KnobStyle style{KnobStyle::Standard};
    float step{0.0f}; // 0.0f = continuous; > 0.0f = discrete step increment (e.g. 1.0f for octaves 0..10)

    constexpr SkeuomorphicKnob() noexcept = default;
    SkeuomorphicKnob(Rect b, std::string name, float initialVal = 0.5f,
                     float minVal = 0.0f, float maxVal = 1.0f, std::string u = "",
                     KnobStyle s = KnobStyle::Standard, float stepVal = 0.0f)
        : bounds(b), paramName(std::move(name)), unit(std::move(u)),
          normValue(std::clamp(initialVal, 0.0f, 1.0f)),
          minDisplay(minVal), maxDisplay(maxVal), style(s), step(stepVal) {
        if (step > 0.0f && maxDisplay > minDisplay) {
            float raw = minDisplay + normValue * (maxDisplay - minDisplay);
            raw = minDisplay + std::round((raw - minDisplay) / step) * step;
            normValue = std::clamp((raw - minDisplay) / (maxDisplay - minDisplay), 0.0f, 1.0f);
        }
    }

    [[nodiscard]] float getDisplayValue() const noexcept {
        float raw = minDisplay + normValue * (maxDisplay - minDisplay);
        if (step > 0.0f && maxDisplay > minDisplay) {
            raw = minDisplay + std::round((raw - minDisplay) / step) * step;
            raw = std::clamp(raw, minDisplay, maxDisplay);
        }
        return raw;
    }

    [[nodiscard]] std::string getFormattedValue() const {
        std::ostringstream oss;
        float val = getDisplayValue();
        if (step >= 1.0f || std::abs(val - std::round(val)) < 0.001f) {
            oss << static_cast<int>(std::round(val));
        } else if (std::abs(val) >= 1000.0f) {
            oss << std::fixed << std::setprecision(1) << (val / 1000.0f) << "k";
        } else if (std::abs(val) >= 10.0f) {
            oss << std::fixed << std::setprecision(0) << val;
        } else {
            oss << std::fixed << std::setprecision(2) << val;
        }
        if (!unit.empty()) {
            oss << " " << unit;
        }
        return oss.str();
    }

    [[nodiscard]] bool hitTest(float px, float py) const noexcept {
        return bounds.contains(px, py);
    }

    float defaultNormValue{0.5f};
    double lastClickTimeMs{0.0};
    float lastClickX{0.0f};
    float lastClickY{0.0f};

    /// Process mouse drag delta (positive dy = dragged down, negative dy = dragged up)
    void handleDrag(float deltaY, bool isShiftDown = false) noexcept {
        // Dragging upward increases value; dragging downward decreases value
        // Shift modifier applies industry standard 0.1x sensitivity multiplier
        float sensitivity = isShiftDown ? 0.0005f : 0.005f;
        normValue = std::clamp(normValue - deltaY * sensitivity, 0.0f, 1.0f);
        if (step > 0.0f && maxDisplay > minDisplay) {
            float raw = minDisplay + normValue * (maxDisplay - minDisplay);
            raw = minDisplay + std::round((raw - minDisplay) / step) * step;
            raw = std::clamp(raw, minDisplay, maxDisplay);
            normValue = (raw - minDisplay) / (maxDisplay - minDisplay);
        }
    }

    void resetToCenter() noexcept {
        normValue = isBipolar ? 0.5f : 0.0f;
    }

    void resetToDefault() noexcept {
        normValue = isBipolar ? 0.5f : defaultNormValue;
    }

    /// Outer knob angle in degrees: starts at -135 deg (bottom-left) to +135 deg (bottom-right)
    [[nodiscard]] float getPointerAngleDegrees() const noexcept {
        return -135.0f + normValue * 270.0f;
    }

    /// Outer knob angle in radians: starts at -3*pi/4 (-135 deg) to +3*pi/4 (+135 deg)
    [[nodiscard]] float getPointerAngleRadians() const noexcept {
        constexpr float minAngle = -2.35619449f;
        constexpr float maxAngle = 2.35619449f;
        return minAngle + normValue * (maxAngle - minAngle);
    }
};

} // namespace eatsbits::ui
