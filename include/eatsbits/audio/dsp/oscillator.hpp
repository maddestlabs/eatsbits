#ifndef EATS_OSCILLATOR_HPP
#define EATS_OSCILLATOR_HPP

#include <cmath>
#include <numbers>
#include <algorithm>

namespace eatsbits::dsp {

enum class Waveform {
    Sine,
    Saw,
    Square,
    Triangle,
    Noise
};

/**
 * Multi-waveform anti-aliased oscillator using PolyBLEP
 * (Polynomial Band-Limited Step) algorithm to eliminate aliasing harmonics.
 */
class Oscillator {
public:
    Oscillator() noexcept = default;

    void setSampleRate(float sampleRate) noexcept {
        sampleRate_ = (sampleRate > 0.0f) ? sampleRate : 48000.0f;
        updateIncrement();
    }

    void setFrequency(float freqHz) noexcept {
        frequency_ = std::clamp(freqHz, 0.1f, sampleRate_ * 0.49f);
        updateIncrement();
    }

    void setWaveform(Waveform wave) noexcept {
        waveform_ = wave;
    }

    void setPulseWidth(float pw) noexcept {
        pulseWidth_ = std::clamp(pw, 0.01f, 0.99f);
    }

    void resetPhase() noexcept {
        phase_ = 0.0f;
    }

    [[nodiscard]] inline float process() noexcept {
        float sample = 0.0f;

        switch (waveform_) {
            case Waveform::Sine:
                sample = std::sin(phase_ * 2.0f * static_cast<float>(std::numbers::pi));
                break;

            case Waveform::Saw: {
                // Naive saw in [-1, 1]
                sample = 2.0f * phase_ - 1.0f;
                // Subtract polyBLEP correction at discontinuity (phase_ = 0)
                sample -= polyBlep(phase_);
                break;
            }

            case Waveform::Square: {
                sample = (phase_ < pulseWidth_) ? 1.0f : -1.0f;
                sample += polyBlep(phase_);
                sample -= polyBlep(std::fmod(phase_ + 1.0f - pulseWidth_, 1.0f));
                break;
            }

            case Waveform::Triangle: {
                // Derived from integrated square wave or folded saw
                sample = 2.0f * std::abs(2.0f * phase_ - 1.0f) - 1.0f;
                break;
            }

            case Waveform::Noise: {
                // Simple fast pseudo-random linear congruential generator
                rngState_ = rngState_ * 1664525u + 1013904223u;
                sample = static_cast<float>(static_cast<int32_t>(rngState_)) / 2147483648.0f;
                break;
            }
        }

        phase_ += phaseIncrement_;
        if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
        }

        return sample;
    }

private:
    // PolyBLEP residual calculation around a discontinuity at t=0
    [[nodiscard]] inline float polyBlep(float t) const noexcept {
        const float dt = phaseIncrement_;
        if (dt <= 0.0f) return 0.0f;

        // 0 <= t < dt: just after discontinuity
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        // 1 - dt <= t < 1: just before discontinuity
        if (t > 1.0f - dt) {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    void updateIncrement() noexcept {
        phaseIncrement_ = frequency_ / sampleRate_;
    }

    float sampleRate_{48000.0f};
    float frequency_{440.0f};
    float phase_{0.0f};
    float phaseIncrement_{0.0f};
    float pulseWidth_{0.5f};
    Waveform waveform_{Waveform::Saw};
    uint32_t rngState_{0x12345678};
};

} // namespace eatsbits::dsp

#endif // EATS_OSCILLATOR_HPP
