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

    constexpr SkeuomorphicKnob() noexcept = default;
    SkeuomorphicKnob(Rect b, std::string name, float initialVal = 0.5f,
                     float minVal = 0.0f, float maxVal = 1.0f, std::string u = "",
                     KnobStyle s = KnobStyle::Standard)
        : bounds(b), paramName(std::move(name)), unit(std::move(u)),
          normValue(std::clamp(initialVal, 0.0f, 1.0f)),
          minDisplay(minVal), maxDisplay(maxVal), style(s) {}

    [[nodiscard]] float getDisplayValue() const noexcept {
        if (isBipolar) {
            return minDisplay + (normValue * (maxDisplay - minDisplay));
        }
        return minDisplay + normValue * (maxDisplay - minDisplay);
    }

    [[nodiscard]] std::string getFormattedValue() const {
        std::ostringstream oss;
        float val = getDisplayValue();
        if (std::abs(val) >= 1000.0f) {
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

    /// Process mouse drag delta (positive dy = dragged down, negative dy = dragged up)
    void handleDrag(float deltaY, bool isShiftDown = false) noexcept {
        // Dragging upward increases value; dragging downward decreases value
        float sensitivity = isShiftDown ? 0.001f : 0.005f;
        normValue = std::clamp(normValue - deltaY * sensitivity, 0.0f, 1.0f);
    }

    void resetToCenter() noexcept {
        normValue = isBipolar ? 0.5f : 0.0f;
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
