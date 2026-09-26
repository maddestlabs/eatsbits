#ifndef EATS_GAIN_NODE_HPP
#define EATS_GAIN_NODE_HPP

#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"

namespace eatsbits::audio {

class GainNode : public GraphNode {
public:
    explicit GainNode(std::string name = "GainNode")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Stereo input
        addOutputPort(2); // Stereo output
        setPan(0.0f);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        reset();
    }

    void reset() noexcept override {
        peakL_.store(0.0f, std::memory_order_relaxed);
        peakR_.store(0.0f, std::memory_order_relaxed);
    }

    void getPeakLevels(float& outL, float& outR) const noexcept {
        outL = peakL_.load(std::memory_order_relaxed);
        outR = peakR_.load(std::memory_order_relaxed);
    }

    void setGainDb(float gainDb) noexcept {
        gainLinear_ = std::pow(10.0f, gainDb / 20.0f);
    }

    void setVolume(float linear) noexcept {
        gainLinear_ = std::max(0.0f, linear);
    }

    [[nodiscard]] float getVolume() const noexcept { return gainLinear_; }

    void setPan(float pan) noexcept {
        pan_ = std::clamp(pan, -1.0f, 1.0f);
        constexpr float piOver4 = 0.78539816339f;
        const float angle = (pan_ + 1.0f) * 0.5f * piOver4;
        panGainL_ = std::cos(angle);
        panGainR_ = std::sin(angle);
    }

    [[nodiscard]] float getPan() const noexcept { return pan_; }

    void setMute(bool mute) noexcept {
        muted_ = mute;
    }

    [[nodiscard]] bool isMuted() const noexcept { return muted_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setVolume(value); break;
            case 1: setPan(value); break;
            case 2: setMute(value > 0.5f); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        const float* inL = getInputBuffer(0, 0);
        const float* inR = getInputBuffer(0, 1);
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);

        if (!outL || !outR) return;

        if (muted_ || !enabled_ || (!inL && !inR)) {
            std::fill_n(outL, numFrames, 0.0f);
            std::fill_n(outR, numFrames, 0.0f);
            peakL_.store(0.0f, std::memory_order_relaxed);
            peakR_.store(0.0f, std::memory_order_relaxed);
            return;
        }

        const float multL = gainLinear_ * panGainL_;
        const float multR = gainLinear_ * panGainR_;

        // If inR is absent, duplicate mono inL to stereo
        const float* srcL = inL;
        const float* srcR = inR ? inR : inL;

        float maxL = 0.0f;
        float maxR = 0.0f;

        for (uint32_t i = 0; i < numFrames; ++i) {
            float sL = srcL[i] * multL;
            float sR = srcR[i] * multR;
            outL[i] = sL;
            outR[i] = sR;

            float absL = std::abs(sL);
            float absR = std::abs(sR);
            if (absL > maxL) maxL = absL;
            if (absR > maxR) maxR = absR;
        }

        peakL_.store(maxL, std::memory_order_relaxed);
        peakR_.store(maxR, std::memory_order_relaxed);
    }

private:
    float gainLinear_{1.0f};
    float pan_{0.0f};
    float panGainL_{0.70710678f};
    float panGainR_{0.70710678f};
    bool muted_{false};
    std::atomic<float> peakL_{0.0f};
    std::atomic<float> peakR_{0.0f};
};

} // namespace eatsbits::audio

#endif // EATS_GAIN_NODE_HPP
