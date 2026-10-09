#ifndef EATS_DAISY_DELAY_NODE_HPP
#define EATS_DAISY_DELAY_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular DaisySP Stereo Delay Graph Node with Damping & Feedback.
 * Input Port 0: Stereo Audio (L/R)
 * Output Port 0: Delayed Stereo Audio (L/R)
 */
class DaisyDelayNode : public GraphNode {
public:
    static constexpr size_t kMaxDelaySamples = 96000; // 2.0s at 48kHz

    explicit DaisyDelayNode(std::string name = "DaisyDelay")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;

        delayL_.Init();
        delayR_.Init();
        dampL_.Init(sampleRate_);
        dampR_.Init(sampleRate_);

        updateDelaySamples();
        dampL_.SetFreq(dampingFreq_);
        dampR_.SetFreq(dampingFreq_);
        reset();
    }

    void reset() noexcept override {
        delayL_.Reset();
        delayR_.Reset();
        lastFeedbackL_ = 0.0f;
        lastFeedbackR_ = 0.0f;
    }

    void setDelayTime(float seconds) noexcept {
        delayTimeSec_ = std::clamp(seconds, 0.001f, 1.95f);
        updateDelaySamples();
    }
    [[nodiscard]] float getDelayTime() const noexcept { return delayTimeSec_; }

    void setFeedback(float fb) noexcept {
        feedback_ = std::clamp(fb, 0.0f, 0.98f);
    }
    [[nodiscard]] float getFeedback() const noexcept { return feedback_; }

    void setDampingFreq(float freqHz) noexcept {
        dampingFreq_ = std::clamp(freqHz, 200.0f, sampleRate_ * 0.48f);
        dampL_.SetFreq(dampingFreq_);
        dampR_.SetFreq(dampingFreq_);
    }
    [[nodiscard]] float getDampingFreq() const noexcept { return dampingFreq_; }

    void setDryWet(float mix) noexcept {
        dryWet_ = std::clamp(mix, 0.0f, 1.0f);
    }
    [[nodiscard]] float getDryWet() const noexcept { return dryWet_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setDelayTime(value); break;
            case 1: setFeedback(value); break;
            case 2: setDampingFreq(value); break;
            case 3: setDryWet(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override;

private:
    void updateDelaySamples() noexcept {
        float samples = delayTimeSec_ * sampleRate_;
        delayL_.SetDelay(samples);
        delayR_.SetDelay(samples);
    }

    daisysp::DelayLine<float, kMaxDelaySamples> delayL_;
    daisysp::DelayLine<float, kMaxDelaySamples> delayR_;
    daisysp::Tone dampL_;
    daisysp::Tone dampR_;

    float sampleRate_{48000.0f};
    float delayTimeSec_{0.35f};
    float feedback_{0.45f};
    float dampingFreq_{12000.0f};
    float dryWet_{0.40f};

    float lastFeedbackL_{0.0f};
    float lastFeedbackR_{0.0f};
};

} // namespace eatsbits::audio

#endif // EATS_DAISY_DELAY_NODE_HPP
