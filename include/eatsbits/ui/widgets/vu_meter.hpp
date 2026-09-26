#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include <algorithm>

namespace eatsbits::ui {

/**
 * Skeuomorphic pro-audio stereo VU meter with peak hold, ballistic exponential decay,
 * and overload / clip LED indicator pips.
 */
class VuMeter {
public:
    Rect bounds;
    float peakLeft{0.0f};  // [0.0, 1.0+]
    float peakRight{0.0f}; // [0.0, 1.0+]
    float holdLeft{0.0f};
    float holdRight{0.0f};
    bool clipLeft{false};
    bool clipRight{false};

    constexpr VuMeter() noexcept = default;
    explicit VuMeter(Rect b) noexcept : bounds(b) {}

    void update(float inL, float inR, float decayFactor = 0.05f) noexcept {
        peakLeft = inL;
        peakRight = inR;

        if (inL >= 1.0f) clipLeft = true;
        if (inR >= 1.0f) clipRight = true;

        if (inL > holdLeft) {
            holdLeft = inL;
        } else {
            holdLeft = std::max(0.0f, holdLeft - decayFactor);
        }

        if (inR > holdRight) {
            holdRight = inR;
        } else {
            holdRight = std::max(0.0f, holdRight - decayFactor);
        }
    }

    void resetClip() noexcept {
        clipLeft = false;
        clipRight = false;
    }
};

} // namespace eatsbits::ui
