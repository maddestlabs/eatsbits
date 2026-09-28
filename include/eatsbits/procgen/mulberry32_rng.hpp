#ifndef EATS_MULBERRY32_RNG_HPP
#define EATS_MULBERRY32_RNG_HPP

#include <cstdint>
#include <vector>
#include <algorithm>
#include <utility>

namespace eatsbits::procgen {

/**
 * Deterministic 32-bit Mulberry32 Pseudo-Random Number Generator.
 * Matches original Eatsbeats PRNG behavior for reproducible algorithmic composition.
 */
class Mulberry32Rng {
public:
    explicit Mulberry32Rng(uint32_t seed = 42) noexcept : state_(seed) {}

    void setSeed(uint32_t seed) noexcept {
        state_ = seed;
    }

    [[nodiscard]] uint32_t getSeed() const noexcept {
        return state_;
    }

    /// Generates next 32-bit integer.
    uint32_t nextU32() noexcept {
        state_ = (state_ + 0x6D2B79F5u) & 0xFFFFFFFFu;
        uint32_t t = state_;
        t = imul(t ^ (t >> 15), t | 1u);
        t ^= t + imul(t ^ (t >> 7), t | 61u);
        return (t ^ (t >> 14)) & 0xFFFFFFFFu;
    }

    /// Generates float in range [0.0f, 1.0f).
    float nextFloat() noexcept {
        return static_cast<float>(nextU32()) / 4294967296.0f;
    }

    /// Generates double in range [0.0, 1.0).
    double nextDouble() noexcept {
        return static_cast<double>(nextU32()) / 4294967296.0;
    }

    /// Generates integer in range [0, max - 1].
    int nextInt(int max) noexcept {
        if (max <= 0) return 0;
        int val = static_cast<int>(nextDouble() * max);
        return std::clamp(val, 0, max - 1);
    }

    /// Generates float in range [a, b).
    float randFloat(float a, float b) noexcept {
        return a + nextFloat() * (b - a);
    }

    /// Generates double in range [a, b).
    double randDouble(double a, double b) noexcept {
        return a + nextDouble() * (b - a);
    }

    /// Returns true with probability p (0.0 to 1.0).
    bool chance(double p) noexcept {
        return nextDouble() < p;
    }

    /// Picks a random element from a vector.
    template <typename T>
    const T& pick(const std::vector<T>& list) noexcept {
        if (list.empty()) {
            static const T emptyVal{};
            return emptyVal;
        }
        int idx = nextInt(static_cast<int>(list.size()));
        return list[static_cast<size_t>(idx)];
    }

    /// Weighted item selection.
    template <typename K>
    K weighted(const std::vector<std::pair<K, double>>& entries) noexcept {
        if (entries.empty()) return K{};
        double total = 0.0;
        for (const auto& [item, weight] : entries) {
            total += weight;
        }
        if (total <= 0.0) return entries.front().first;

        double r = nextDouble() * total;
        for (const auto& [item, weight] : entries) {
            r -= weight;
            if (r <= 0.0) return item;
        }
        return entries.back().first;
    }

private:
    static uint32_t imul(uint32_t a, uint32_t b) noexcept {
        uint32_t ah = (a >> 16) & 0xFFFFu;
        uint32_t al = a & 0xFFFFu;
        uint32_t bh = (b >> 16) & 0xFFFFu;
        uint32_t bl = b & 0xFFFFu;
        return ((al * bl) + (((ah * bl + al * bh) & 0xFFFFu) << 16)) & 0xFFFFFFFFu;
    }

    uint32_t state_{42};
};

} // namespace eatsbits::procgen

#endif // EATS_MULBERRY32_RNG_HPP
