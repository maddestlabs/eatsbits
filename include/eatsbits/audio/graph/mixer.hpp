#ifndef EATS_MIXER_HPP
#define EATS_MIXER_HPP

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>
#include "audio_node.hpp"
#include "../ringbuffer.hpp"

namespace eatsbits::audio {

/**
 * Master Stereo Mixer Channel Strip with Volume, Pan, Mute,
 * and lock-free Peak & RMS metering.
 */
class MixerStrip : public IAudioNode {
public:
    MixerStrip() noexcept = default;

    void prepare(double sampleRate, uint32_t /*maxBlockSize*/) override {
        sampleRate_ = sampleRate;
        reset();
    }

    void reset() noexcept override {
        peakL_ = 0.0f;
        peakR_ = 0.0f;
        sumSqL_ = 0.0f;
        sumSqR_ = 0.0f;
    }

    void setGainDb(float gainDb) noexcept {
        gainLinear_ = std::pow(10.0f, gainDb / 20.0f);
    }

    void setVolume(float linear) noexcept {
        gainLinear_ = std::max(0.0f, linear);
    }

    void setPan(float pan) noexcept { // -1.0 (Left) to +1.0 (Right)
        pan_ = std::clamp(pan, -1.0f, 1.0f);
        // Constant-power pan law
        constexpr float piOver4 = 0.78539816339f;
        const float angle = (pan_ + 1.0f) * 0.5f * piOver4;
        panGainL_ = std::cos(angle);
        panGainR_ = std::sin(angle);
    }

    void setMute(bool mute) noexcept {
        muted_ = mute;
    }

    void process(const EatsAudioBuffer& buffer) noexcept override {
        if (!buffer.outputs || buffer.numChannels < 2 || buffer.numSamples == 0) return;

        float* outL = buffer.outputs[0];
        float* outR = buffer.outputs[1];

        if (muted_) {
            for (uint32_t i = 0; i < buffer.numSamples; ++i) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
            }
            feedback_.peakLeft = 0.0f;
            feedback_.peakRight = 0.0f;
            feedback_.rmsLeft = 0.0f;
            feedback_.rmsRight = 0.0f;
            return;
        }

        const float gL = gainLinear_ * panGainL_;
        const float gR = gainLinear_ * panGainR_;

        float localPeakL = 0.0f;
        float localPeakR = 0.0f;
        float sumL = 0.0f;
        float sumR = 0.0f;

        for (uint32_t i = 0; i < buffer.numSamples; ++i) {
            float sL = outL[i] * gL;
            float sR = outR[i] * gR;

            // Soft saturation limiter to prevent harsh digital clipping
            sL = std::clamp(sL, -1.0f, 1.0f);
            sR = std::clamp(sR, -1.0f, 1.0f);

            outL[i] = sL;
            outR[i] = sR;

            const float absL = std::abs(sL);
            const float absR = std::abs(sR);
            if (absL > localPeakL) localPeakL = absL;
            if (absR > localPeakR) localPeakR = absR;

            sumL += sL * sL;
            sumR += sR * sR;
        }

        feedback_.peakLeft = localPeakL;
        feedback_.peakRight = localPeakR;
        feedback_.rmsLeft = std::sqrt(sumL / static_cast<float>(buffer.numSamples));
        feedback_.rmsRight = std::sqrt(sumR / static_cast<float>(buffer.numSamples));
    }

    [[nodiscard]] const MeterFeedback& getMeterFeedback() const noexcept {
        return feedback_;
    }

private:
    double sampleRate_{48000.0};
    float gainLinear_{1.0f};
    float pan_{0.0f};
    float panGainL_{0.7071f};
    float panGainR_{0.7071f};
    bool muted_{false};

    float peakL_{0.0f};
    float peakR_{0.0f};
    float sumSqL_{0.0f};
    float sumSqR_{0.0f};
    MeterFeedback feedback_{0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace eatsbits::audio

#endif // EATS_MIXER_HPP
