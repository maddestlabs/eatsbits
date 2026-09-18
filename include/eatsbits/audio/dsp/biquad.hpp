#ifndef EATS_BIQUAD_HPP
#define EATS_BIQUAD_HPP

#include <cmath>
#include <cstddef>
#include <numbers>

namespace eatsbits::dsp {

enum class BiquadType {
    LowPass,
    HighPass,
    BandPass,
    Notch,
    Peaking,
    LowShelf,
    HighShelf
};

/**
 * High-performance, Direct Form II Transposed Biquad Filter.
 * Vectorizer-friendly with sample-by-sample and block processing methods.
 */
class BiquadFilter {
public:
    BiquadFilter() noexcept {
        reset();
    }

    void reset() noexcept {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

    void configure(BiquadType type, float cutoffHz, float q, float gainDb, float sampleRate) noexcept {
        if (sampleRate <= 0.0f) sampleRate = 48000.0f;
        // Clamp cutoff within Nyquist limit
        const float nyquist = sampleRate * 0.495f;
        cutoffHz = std::fmax(10.0f, std::fmin(cutoffHz, nyquist));
        q = std::fmax(0.01f, q);

        const float w0 = 2.0f * static_cast<float>(std::numbers::pi) * cutoffHz / sampleRate;
        const float cosW0 = std::cos(w0);
        const float sinW0 = std::sin(w0);
        const float alpha = sinW0 / (2.0f * q);
        const float A = std::pow(10.0f, gainDb / 40.0f);

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        switch (type) {
            case BiquadType::LowPass:
                b0 = (1.0f - cosW0) * 0.5f;
                b1 = 1.0f - cosW0;
                b2 = (1.0f - cosW0) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW0;
                a2 = 1.0f - alpha;
                break;
            case BiquadType::HighPass:
                b0 = (1.0f + cosW0) * 0.5f;
                b1 = -(1.0f + cosW0);
                b2 = (1.0f + cosW0) * 0.5f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW0;
                a2 = 1.0f - alpha;
                break;
            case BiquadType::BandPass:
                b0 = alpha;
                b1 = 0.0f;
                b2 = -alpha;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW0;
                a2 = 1.0f - alpha;
                break;
            case BiquadType::Notch:
                b0 = 1.0f;
                b1 = -2.0f * cosW0;
                b2 = 1.0f;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosW0;
                a2 = 1.0f - alpha;
                break;
            case BiquadType::Peaking:
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosW0;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosW0;
                a2 = 1.0f - alpha / A;
                break;
            case BiquadType::LowShelf: {
                const float sqrtA = std::sqrt(A);
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
                a2 = (A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
                break;
            }
            case BiquadType::HighShelf: {
                const float sqrtA = std::sqrt(A);
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + 2.0f * sqrtA * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
                a2 = (A + 1.0f) - (A - 1.0f) * cosW0 - 2.0f * sqrtA * alpha;
                break;
            }
        }

        const float invA0 = 1.0f / a0;
        b0_ = b0 * invA0;
        b1_ = b1 * invA0;
        b2_ = b2 * invA0;
        a1_ = a1 * invA0;
        a2_ = a2 * invA0;
    }

    [[nodiscard]] inline float process(float in) noexcept {
        const float out = b0_ * in + z1_;
        z1_ = b1_ * in - a1_ * out + z2_;
        z2_ = b2_ * in - a2_ * out;
        return out;
    }

    void process_block(const float* in, float* out, size_t count) noexcept {
        float z1 = z1_;
        float z2 = z2_;
        const float b0 = b0_, b1 = b1_, b2 = b2_;
        const float a1 = a1_, a2 = a2_;

        for (size_t i = 0; i < count; ++i) {
            const float x = in[i];
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            out[i] = y;
        }

        z1_ = z1;
        z2_ = z2;
    }

private:
    float b0_{1.0f}, b1_{0.0f}, b2_{0.0f};
    float a1_{0.0f}, a2_{0.0f};
    float z1_{0.0f}, z2_{0.0f};
};

} // namespace eatsbits::dsp

#endif // EATS_BIQUAD_HPP
