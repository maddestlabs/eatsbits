#ifndef EATS_COMPRESSOR_NODE_HPP
#define EATS_COMPRESSOR_NODE_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"
#include "../../dsp/dynamics_processor.hpp"

namespace eatsbits::audio {

/**
 * Modular Studio Compressor Graph Node.
 * Input Port 0: Main Stereo Audio (L/R)
 * Input Port 1: Sidechain Audio (L/R, optional)
 * Output Port 0: Processed Stereo Audio (L/R)
 */
class CompressorNode : public GraphNode {
public:
    explicit CompressorNode(std::string name = "Compressor")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Main stereo in
        addInputPort(2);  // Port 1: Sidechain in
        addOutputPort(2); // Port 0: Main stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        compressor_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        compressor_.reset();
    }

    // Parameters
    void setThreshold(float db) noexcept { compressor_.setThreshold(db); }
    [[nodiscard]] float getThreshold() const noexcept { return compressor_.getThreshold(); }

    void setRatio(float r) noexcept { compressor_.setRatio(r); }
    [[nodiscard]] float getRatio() const noexcept { return compressor_.getRatio(); }

    void setKnee(float db) noexcept { compressor_.setKnee(db); }
    [[nodiscard]] float getKnee() const noexcept { return compressor_.getKnee(); }

    void setAttack(float ms) noexcept { compressor_.setAttack(ms); }
    [[nodiscard]] float getAttack() const noexcept { return compressor_.getAttack(); }

    void setRelease(float ms) noexcept { compressor_.setRelease(ms); }
    [[nodiscard]] float getRelease() const noexcept { return compressor_.getRelease(); }

    void setMakeupGain(float db) noexcept { compressor_.setMakeupGain(db); }
    [[nodiscard]] float getMakeupGain() const noexcept { return compressor_.getMakeupGain(); }

    void setMix(float mix) noexcept { compressor_.setMix(mix); }
    [[nodiscard]] float getMix() const noexcept { return compressor_.getMix(); }

    void setRmsMode(bool rms) noexcept { compressor_.setRmsMode(rms); }
    [[nodiscard]] bool isRmsMode() const noexcept { return compressor_.isRmsMode(); }

    [[nodiscard]] float getCurrentGainReductionDb() const noexcept {
        return compressor_.getCurrentGainReductionDb();
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setThreshold(value); break;
            case 1: setRatio(value); break;
            case 2: setKnee(value); break;
            case 3: setAttack(value); break;
            case 4: setRelease(value); break;
            case 5: setMakeupGain(value); break;
            case 6: setMix(value); break;
            case 7: setRmsMode(value > 0.5f); break;
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

        // Copy input to output buffers for in-place processing
        std::copy_n(srcL, numFrames, outL);
        std::copy_n(srcR, numFrames, outR);

        // Check sidechain input on Port 1
        const float* scL = getInputBuffer(1, 0);
        const float* scR = getInputBuffer(1, 1);

        compressor_.processStereo(outL, outR, numFrames, scL, scR);
    }

private:
    dsp::CompressorCore compressor_;
};

} // namespace eatsbits::audio

#endif // EATS_COMPRESSOR_NODE_HPP
