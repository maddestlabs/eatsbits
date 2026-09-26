#ifndef EATS_LIMITER_NODE_HPP
#define EATS_LIMITER_NODE_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"
#include "../../dsp/dynamics_processor.hpp"

namespace eatsbits::audio {

/**
 * Modular Studio Brickwall Lookahead Limiter Graph Node.
 * Ensures zero-overshoot peak control with lookahead delay buffer.
 * Input Port 0: Stereo Audio (L/R)
 * Output Port 0: Peak-Limited Stereo Audio (L/R)
 */
class LimiterNode : public GraphNode {
public:
    explicit LimiterNode(std::string name = "Limiter")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        limiter_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        limiter_.reset();
    }

    void setCeilingDb(float db) noexcept { limiter_.setCeilingDb(db); }
    [[nodiscard]] float getCeilingDb() const noexcept { return limiter_.getCeilingDb(); }

    void setRelease(float ms) noexcept { limiter_.setRelease(ms); }
    [[nodiscard]] float getRelease() const noexcept { return limiter_.getRelease(); }

    void setLookaheadMs(float ms) noexcept { limiter_.setLookaheadMs(ms); }
    [[nodiscard]] float getLookaheadMs() const noexcept { return limiter_.getLookaheadMs(); }

    [[nodiscard]] float getCurrentGainReductionLinear() const noexcept {
        return limiter_.getCurrentGainReductionLinear();
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setCeilingDb(value); break;
            case 1: setRelease(value); break;
            case 2: setLookaheadMs(value); break;
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

        limiter_.processStereo(outL, outR, numFrames);
    }

private:
    dsp::LimiterCore limiter_;
};

} // namespace eatsbits::audio

#endif // EATS_LIMITER_NODE_HPP
