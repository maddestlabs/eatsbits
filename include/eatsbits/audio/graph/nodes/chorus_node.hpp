#ifndef EATS_CHORUS_NODE_HPP
#define EATS_CHORUS_NODE_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"
#include "../../dsp/chorus_flanger.hpp"

namespace eatsbits::audio {

/**
 * Modular Stereo Chorus & Flanger Graph Node.
 * Input Port 0: Stereo Audio (L/R)
 * Output Port 0: Modulated Stereo Audio (L/R)
 */
class ChorusNode : public GraphNode {
public:
    explicit ChorusNode(std::string name = "ChorusFlanger")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        chorus_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        chorus_.reset();
    }

    void setRateHz(float rate) noexcept { chorus_.setRateHz(rate); }
    [[nodiscard]] float getRateHz() const noexcept { return chorus_.getRateHz(); }

    void setDepthMs(float depth) noexcept { chorus_.setDepthMs(depth); }
    [[nodiscard]] float getDepthMs() const noexcept { return chorus_.getDepthMs(); }

    void setBaseDelayMs(float delayMs) noexcept { chorus_.setBaseDelayMs(delayMs); }
    [[nodiscard]] float getBaseDelayMs() const noexcept { return chorus_.getBaseDelayMs(); }

    void setFeedback(float fb) noexcept { chorus_.setFeedback(fb); }
    [[nodiscard]] float getFeedback() const noexcept { return chorus_.getFeedback(); }

    void setMix(float mix) noexcept { chorus_.setMix(mix); }
    [[nodiscard]] float getMix() const noexcept { return chorus_.getMix(); }

    void setStereoPhaseSpreadRad(float rad) noexcept { chorus_.setStereoPhaseSpreadRad(rad); }
    [[nodiscard]] float getStereoPhaseSpreadRad() const noexcept { return chorus_.getStereoPhaseSpreadRad(); }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setRateHz(value); break;
            case 1: setDepthMs(value); break;
            case 2: setBaseDelayMs(value); break;
            case 3: setFeedback(value); break;
            case 4: setMix(value); break;
            case 5: setStereoPhaseSpreadRad(value); break;
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

        std::copy_n(srcL, numFrames, outL);
        std::copy_n(srcR, numFrames, outR);

        chorus_.processStereo(outL, outR, numFrames);
    }

private:
    dsp::ChorusFlangerCore chorus_;
};

} // namespace eatsbits::audio

#endif // EATS_CHORUS_NODE_HPP
