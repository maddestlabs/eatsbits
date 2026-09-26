#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include <string>

namespace eatsbits::ui {

/**
 * Skeuomorphic illuminated Nixie / LCD display well with beveled metallic borders,
 * dark recessed glass, dot grid texture, glowing filaments, and ambient bloom.
 */
class GlowingNixie {
public:
    Rect bounds;
    std::string title{"TEMPO"};
    std::string valueText{"120 BPM"};
    bool isHovered{false};

    constexpr GlowingNixie() noexcept = default;
    GlowingNixie(Rect b, std::string t, std::string v)
        : bounds(b), title(std::move(t)), valueText(std::move(v)) {}

    [[nodiscard]] bool hitTest(float px, float py) const noexcept {
        return bounds.contains(px, py);
    }
};

} // namespace eatsbits::ui
