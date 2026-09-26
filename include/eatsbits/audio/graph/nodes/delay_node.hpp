#ifndef EATS_DELAY_NODE_HPP
#define EATS_DELAY_NODE_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"

namespace eatsbits::audio {

/**
 * High-performance Stereo Delay / Echo Node with feedback and dry/wet mix.
 * Pre-allocated delay ring buffers guarantee zero allocation during real-time processing.
 */
class DelayNode : public GraphNode {
public:
    explicit DelayNode(std::string name = "StereoDelay")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Stereo input
        addOutputPort(2); // Stereo output
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        // Max delay 2.0 seconds
        maxDelayFrames_ = static_cast<size_t>(sampleRate * 2.0);
        delayBufL_.assign(maxDelayFrames_, 0.0f);
        delayBufR_.assign(maxDelayFrames_, 0.0f);
        updateDelayFrames();
        reset();
    }

    void reset() noexcept override {
        std::fill(delayBufL_.begin(), delayBufL_.end(), 0.0f);
        std::fill(delayBufR_.begin(), delayBufR_.end(), 0.0f);
        writeIndex_ = 0;
    }

    void setDelayTimeMs(float delayTimeMs) noexcept {
        delayTimeMs_ = std::clamp(delayTimeMs, 1.0f, 2000.0f);
        updateDelayFrames();
    }

    [[nodiscard]] float getDelayTimeMs() const noexcept { return delayTimeMs_; }

    void setFeedback(float feedback) noexcept {
        feedback_ = std::clamp(feedback, 0.0f, 0.98f);
    }

    [[nodiscard]] float getFeedback() const noexcept { return feedback_; }

    void setDryWet(float dryWet) noexcept {
        dryWet_ = std::clamp(dryWet, 0.0f, 1.0f);
    }

    [[nodiscard]] float getDryWet() const noexcept { return dryWet_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setDelayTimeMs(value); break;
            case 1: setFeedback(value); break;
            case 2: setDryWet(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        const float* inL = getInputBuffer(0, 0);
        const float* inR = getInputBuffer(0, 1);
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);

        if (!outL || !outR) return;

        if (!enabled_ || (!inL && !inR)) {
            std::fill_n(outL, numFrames, 0.0f);
            std::fill_n(outR, numFrames, 0.0f);
            return;
        }

        const float* srcL = inL ? inL : inR;
        const float* srcR = inR ? inR : inL;

        const float dryGain = 1.0f - dryWet_;
        const float wetGain = dryWet_;
        const size_t bufSize = delayBufL_.size();
        if (bufSize == 0) return;

        for (uint32_t i = 0; i < numFrames; ++i) {
            // Calculate read index
            size_t readIndex = (writeIndex_ + bufSize - delayFrames_) % bufSize;

            float delayedL = delayBufL_[readIndex];
            float delayedR = delayBufR_[readIndex];

            // Mix wet with dry for output
            outL[i] = srcL[i] * dryGain + delayedL * wetGain;
            outR[i] = srcR[i] * dryGain + delayedR * wetGain;

            // Write into delay buffer with feedback
            delayBufL_[writeIndex_] = srcL[i] + delayedL * feedback_;
            delayBufR_[writeIndex_] = srcR[i] + delayedR * feedback_;

            writeIndex_ = (writeIndex_ + 1) % bufSize;
        }
    }

private:
    void updateDelayFrames() noexcept {
        delayFrames_ = std::clamp(
            static_cast<size_t>(sampleRate_ * (delayTimeMs_ / 1000.0f)),
            size_t{1},
            maxDelayFrames_ > 1 ? maxDelayFrames_ - 1 : size_t{1}
        );
    }

    float delayTimeMs_{375.0f}; // 1/8th note at 120 BPM
    float feedback_{0.45f};
    float dryWet_{0.35f};

    size_t maxDelayFrames_{96000};
    size_t delayFrames_{18000};
    size_t writeIndex_{0};

    std::vector<float> delayBufL_;
    std::vector<float> delayBufR_;
};

} // namespace eatsbits::audio

#endif // EATS_DELAY_NODE_HPP
