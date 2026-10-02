#ifndef EATS_CORE_COLOR_HPP
#define EATS_CORE_COLOR_HPP

#include <cstdint>
#include <algorithm>

namespace eatsbits::core {

struct Color {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};

    constexpr Color() noexcept = default;
    constexpr Color(float r_, float g_, float b_, float a_ = 1.0f) noexcept
        : r(r_), g(g_), b(b_), a(a_) {}

    [[nodiscard]] constexpr uint32_t toRgba8() const noexcept {
        uint32_t ur = static_cast<uint32_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint32_t ug = static_cast<uint32_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint32_t ub = static_cast<uint32_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint32_t ua = static_cast<uint32_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        return (ua << 24) | (ub << 16) | (ug << 8) | ur;
    }

    [[nodiscard]] constexpr uint32_t toAbgr8() const noexcept {
        uint32_t ur = static_cast<uint32_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint32_t ug = static_cast<uint32_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint32_t ub = static_cast<uint32_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint32_t ua = static_cast<uint32_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        return (ua << 24) | (ub << 16) | (ug << 8) | ur;
    }

    [[nodiscard]] static constexpr Color fromHex(uint32_t hex) noexcept {
        float r = ((hex >> 24) & 0xFF) / 255.0f;
        float g = ((hex >> 16) & 0xFF) / 255.0f;
        float b = ((hex >> 8) & 0xFF) / 255.0f;
        float a = (hex & 0xFF) / 255.0f;
        return {r, g, b, a};
    }
};

} // namespace eatsbits::core

#endif // EATS_CORE_COLOR_HPP
