#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include <string>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace eatsbits::ui {

/**
 * Skeuomorphic vertical fader slider with recessed slot well, metallic chamfered
 * cap, volume level calibration, and linear-to-dB display.
 */
class SkeuomorphicSlider {
public:
    Rect bounds;
    std::string label{"LEVEL"};
    float normValue{0.75f}; // [0.0, 1.5], where 1.0 = 0dB unity gain
    float capHeight{32.0f};
    float capWidth{40.0f};
    bool isHovered{false};
    bool isDragging{false};

    constexpr SkeuomorphicSlider() noexcept = default;
    SkeuomorphicSlider(Rect b, std::string l = "LEVEL", float initialVal = 0.75f)
        : bounds(b), label(std::move(l)), normValue(std::clamp(initialVal, 0.0f, 1.5f)) {}

    [[nodiscard]] Rect getFaderCapBounds() const noexcept {
        float travelH = std::max(0.0f, bounds.h - capHeight);
        // Normalized 0.0 is at bottom (lowest gain), 1.5 is at top (+6dB)
        float capY = bounds.y + travelH * (1.0f - std::clamp(normValue / 1.5f, 0.0f, 1.0f));
        float capX = bounds.x + (bounds.w - capWidth) * 0.5f;
        return {capX, capY, capWidth, capHeight};
    }

    [[nodiscard]] bool hitTest(float px, float py) const noexcept {
        return bounds.contains(px, py);
    }

    [[nodiscard]] bool hitTestCap(float px, float py) const noexcept {
        return getFaderCapBounds().contains(px, py);
    }

    float defaultNormValue{1.0f}; // 1.0 = 0 dB unity gain
    double lastClickTimeMs{0.0};
    float lastClickX{0.0f};
    float lastClickY{0.0f};

    void resetToDefault() noexcept {
        normValue = defaultNormValue;
    }

    /// Process mouse drag delta (positive dy = dragged down, negative dy = dragged up)
    void handleDragDelta(float deltaY, bool isShiftDown = false) noexcept {
        float travelH = std::max(1.0f, bounds.h - capHeight);
        float sensitivity = (1.5f / travelH) * (isShiftDown ? 0.10f : 1.0f);
        normValue = std::clamp(normValue - deltaY * sensitivity, 0.0f, 1.5f);
    }

    /// Convert absolute mouse Y position along the fader travel into normalized gain
    void handleDragY(float mouseY) noexcept {
        float travelH = std::max(1.0f, bounds.h - capHeight);
        float relY = mouseY - (bounds.y + capHeight * 0.5f);
        float fraction = 1.0f - std::clamp(relY / travelH, 0.0f, 1.0f);
        normValue = fraction * 1.5f;
    }

    [[nodiscard]] std::string getFormattedDb() const {
        if (normValue <= 0.001f) {
            return "-INF dB";
        }
        float db = 20.0f * std::log10(normValue);
        std::ostringstream oss;
        if (db > 0.05f) {
            oss << "+";
        }
        oss << std::fixed << std::setprecision(1) << db << " dB";
        return oss.str();
    }
};

} // namespace eatsbits::ui
