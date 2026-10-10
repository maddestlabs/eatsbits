#ifndef EATS_REVERB_NODE_HPP
#define EATS_REVERB_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular Stereo Plate & Hall Reverb Graph Node.
 * Powered by DaisySP's 8-delay stereo algorithmic ReverbSc engine with feedback and LP damping.
 * Input Port 0: Stereo Audio (L/R)
 * Output Port 0: Reverberated Stereo Audio (L/R)
 */
class ReverbNode : public GraphNode {
public:
    explicit ReverbNode(std::string name = "Reverb")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;
        verb_.Init(sampleRate_);
        verb_.SetFeedback(feedback_);
        verb_.SetLpFreq(dampingFreq_);
        reset();
    }

    void reset() noexcept override {
        verb_.Init(sampleRate_);
        verb_.SetFeedback(feedback_);
        verb_.SetLpFreq(dampingFreq_);
    }

    void setFeedback(float fb) noexcept {
        feedback_ = std::clamp(fb, 0.0f, 0.999f);
        verb_.SetFeedback(feedback_);
    }
    [[nodiscard]] float getFeedback() const noexcept { return feedback_; }

    void setDampingFreq(float freqHz) noexcept {
        dampingFreq_ = std::clamp(freqHz, 100.0f, sampleRate_ * 0.49f);
        verb_.SetLpFreq(dampingFreq_);
    }
    [[nodiscard]] float getDampingFreq() const noexcept { return dampingFreq_; }

    void setDryWet(float mix) noexcept {
        dryWet_ = std::clamp(mix, 0.0f, 1.0f);
    }
    [[nodiscard]] float getDryWet() const noexcept { return dryWet_; }

    void setMix(float mix) noexcept { setDryWet(mix); }
    [[nodiscard]] float getMix() const noexcept { return getDryWet(); }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setFeedback(value); break;
            case 1: setDampingFreq(value); break;
            case 2: setDryWet(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override;

private:
    daisysp::ReverbSc verb_;

    float sampleRate_{48000.0f};
    float feedback_{0.85f};
    float dampingFreq_{10000.0f};
    float dryWet_{0.35f};
};

// Aliases for prefix-free and backward compatibility
using DaisyReverbNode = ReverbNode;

} // namespace eatsbits::audio

#endif // EATS_REVERB_NODE_HPP
