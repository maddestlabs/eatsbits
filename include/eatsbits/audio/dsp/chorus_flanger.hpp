#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <numbers>
#include <cstdint>

namespace eatsbits::dsp {

/**
 * High-performance circular delay line with fractional linear interpolation.
 * Sized to a power of two for fast bitwise wrapping and zero allocations.
 */
template <size_t BufferSize = 8192>
class FractionalDelayLine {
    static_assert((BufferSize & (BufferSize - 1)) == 0, "BufferSize must be a power of two");
public:
    static constexpr size_t kMask = BufferSize - 1;

    void reset() noexcept {
        buffer_.fill(0.0f);
        writeIdx_ = 0;
    }

    void write(float sample) noexcept {
        buffer_[writeIdx_] = sample;
        writeIdx_ = (writeIdx_ + 1) & kMask;
    }

    [[nodiscard]] float readFractional(float delaySamples) const noexcept {
        float readPos = static_cast<float>(writeIdx_) - delaySamples;
        while (readPos < 0.0f) {
            readPos += static_cast<float>(BufferSize);
        }
        while (readPos >= static_cast<float>(BufferSize)) {
            readPos -= static_cast<float>(BufferSize);
        }

        const size_t idx0 = static_cast<size_t>(readPos) & kMask;
        const size_t idx1 = (idx0 + 1) & kMask;
        const float frac = readPos - std::floor(readPos);

        return buffer_[idx0] * (1.0f - frac) + buffer_[idx1] * frac;
    }

private:
    std::array<float, BufferSize> buffer_{};
    size_t writeIdx_{0};
};

/**
 * Stereo Multi-Voice Chorus & Flanger.
 * Matches the EatChorus implementation from Eatsbeats with quadrature LFOs (Left 0°, Right 90°),
 * feedback regeneration, and dry/wet crossfade.
 */
class ChorusFlangerCore {
public:
    explicit ChorusFlangerCore(float sampleRate = 44100.0f)
        : sampleRate_(sampleRate) {
        reset();
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = std::max(8000.0f, sr);
        reset();
    }

    void reset() noexcept {
        delayL_.reset();
        delayR_.reset();
        phase_ = 0.0f;
    }

    void setRateHz(float rate) noexcept { rateHz_ = std::clamp(rate, 0.02f, 20.0f); }
    [[nodiscard]] float getRateHz() const noexcept { return rateHz_; }

    void setDepthMs(float depth) noexcept { depthMs_ = std::clamp(depth, 0.0f, 25.0f); }
    [[nodiscard]] float getDepthMs() const noexcept { return depthMs_; }

    void setBaseDelayMs(float delayMs) noexcept { baseDelayMs_ = std::clamp(delayMs, 0.1f, 40.0f); }
    [[nodiscard]] float getBaseDelayMs() const noexcept { return baseDelayMs_; }

    void setFeedback(float fb) noexcept { feedback_ = std::clamp(fb, -0.98f, 0.98f); }
    [[nodiscard]] float getFeedback() const noexcept { return feedback_; }

    void setMix(float mix) noexcept { mix_ = std::clamp(mix, 0.0f, 1.0f); }
    [[nodiscard]] float getMix() const noexcept { return mix_; }

    void setStereoPhaseSpreadRad(float rad) noexcept { stereoPhaseRad_ = rad; }
    [[nodiscard]] float getStereoPhaseSpreadRad() const noexcept { return stereoPhaseRad_; }

    /**
     * Process stereo channels in-place.
     */
    void processStereo(float* left, float* right, size_t numFrames) noexcept {
        if (!left || !right || numFrames == 0) return;

        constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
        const float phaseInc = (twoPi * rateHz_) / sampleRate_;
        const float msToSamples = sampleRate_ / 1000.0f;
        const float dryGain = 1.0f - mix_;
        const float wetGain = mix_;

        for (size_t i = 0; i < numFrames; ++i) {
            const float inL = left[i];
            const float inR = right[i];

            // Quadrature LFOs (Left 0°, Right with phase spread, default pi/2 = 90°)
            const float modL = (std::sin(phase_) + 1.0f) * 0.5f * depthMs_;
            const float modR = (std::sin(phase_ + stereoPhaseRad_) + 1.0f) * 0.5f * depthMs_;

            phase_ += phaseInc;
            if (phase_ >= twoPi) phase_ -= twoPi;

            const float dSamplesL = (baseDelayMs_ + modL) * msToSamples;
            const float dSamplesR = (baseDelayMs_ + modR) * msToSamples;

            const float wetL = delayL_.readFractional(dSamplesL);
            const float wetR = delayR_.readFractional(dSamplesR);

            delayL_.write(inL + wetL * feedback_);
            delayR_.write(inR + wetR * feedback_);

            left[i] = inL * dryGain + wetL * wetGain;
            right[i] = inR * dryGain + wetR * wetGain;
        }
    }

private:
    float sampleRate_{44100.0f};
    float rateHz_{1.2f};
    float depthMs_{2.5f};
    float baseDelayMs_{12.0f};
    float feedback_{0.2f};
    float mix_{0.5f};
    float stereoPhaseRad_{0.5f * std::numbers::pi_v<float>}; // 90 degrees

    float phase_{0.0f};

    FractionalDelayLine<8192> delayL_;
    FractionalDelayLine<8192> delayR_;
};

} // namespace eatsbits::dsp
