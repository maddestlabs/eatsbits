#pragma once

#include <cstdint>

namespace eatsbits::audio {

/// Fast, deterministic pseudo-random number generator (Mulberry32).
/// Used for reproducible SFXR seeds, noise sequences, and procedural sound mutations.
class DeterministicPRNG {
public:
    explicit DeterministicPRNG(uint32_t seed = 42) noexcept
        : state_(seed == 0 ? 42 : seed) {}

    void seed(uint32_t s) noexcept {
        state_ = (s == 0 ? 42 : s);
    }

    [[nodiscard]] uint32_t nextU32() noexcept {
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }

    [[nodiscard]] float nextFloat() noexcept {
        uint32_t u = nextU32();
        return static_cast<float>(u & 0x7FFFFFFFu) / 2147483648.0f;
    }

    [[nodiscard]] int nextInt(int minVal, int maxVal) noexcept {
        if (maxVal <= minVal) return minVal;
        return minVal + static_cast<int>(nextFloat() * static_cast<float>(maxVal - minVal + 1));
    }

    [[nodiscard]] float nextRange(float minVal, float maxVal) noexcept {
        return minVal + nextFloat() * (maxVal - minVal);
    }

private:
    uint32_t state_{42};
};

} // namespace eatsbits::audio
