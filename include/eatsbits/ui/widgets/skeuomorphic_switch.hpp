#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include <string>
#include <vector>

namespace eatsbits::ui {

/**
 * Skeuomorphic hardware toggle switch supporting 2-position (e.g. Saw/Square)
 * or 3-position (e.g. Lowpass/Bandpass/Highpass) switches with tactile bat handle.
 */
class SkeuomorphicSwitch {
public:
    Rect bounds;
    std::string paramName{"WAVE"};
    int state{0}; // 0 = Down, 1 = Up (or 0, 1, 2 for 3-way)
    int numPositions{2};
    std::vector<std::string> labels{"SAW", "SQR"};
    bool isHovered{false};

    constexpr SkeuomorphicSwitch() noexcept = default;
    SkeuomorphicSwitch(Rect b, std::string name, std::vector<std::string> optionLabels, int initialPos = 0)
        : bounds(b), paramName(std::move(name)),
          numPositions(static_cast<int>(optionLabels.size())),
          labels(std::move(optionLabels)),
          state(std::clamp(initialPos, 0, numPositions - 1)) {}

    [[nodiscard]] bool hitTest(float px, float py) const noexcept {
        return bounds.contains(px, py);
    }

    void toggle() noexcept {
        state = (state + 1) % numPositions;
    }

    [[nodiscard]] const std::string& getActiveLabel() const noexcept {
        static const std::string emptyStr;
        if (state >= 0 && state < static_cast<int>(labels.size())) {
            return labels[state];
        }
        return emptyStr;
    }
};

} // namespace eatsbits::ui
