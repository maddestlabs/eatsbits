#ifndef EATS_DAISY_COMPRESSOR_NODE_HPP
#define EATS_DAISY_COMPRESSOR_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular DaisySP Stereo Dynamics Compressor Graph Node with Sidechain Input.
 * Input Port 0: Stereo Audio (L/R)
 * Input Port 1: Sidechain Key (optional, mono)
 * Output Port 0: Compressed Stereo Audio (L/R)
 */
class DaisyCompressorNode : public GraphNode {
public:
    explicit DaisyCompressorNode(std::string name = "DaisyCompressor")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addInputPort(1);  // Port 1: Sidechain key (optional)
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;

        compL_.Init(sampleRate_);
        compR_.Init(sampleRate_);
        updateCompressorParams();
        reset();
    }

    void reset() noexcept override {
        compL_.Init(sampleRate_);
        compR_.Init(sampleRate_);
        updateCompressorParams();
    }

    void setThreshold(float db) noexcept {
        thresholdDb_ = std::clamp(db, -80.0f, 0.0f);
        compL_.SetThreshold(thresholdDb_);
        compR_.SetThreshold(thresholdDb_);
    }
    [[nodiscard]] float getThreshold() const noexcept { return thresholdDb_; }

    void setRatio(float ratio) noexcept {
        ratio_ = std::clamp(ratio, 1.0f, 40.0f);
        compL_.SetRatio(ratio_);
        compR_.SetRatio(ratio_);
    }
    [[nodiscard]] float getRatio() const noexcept { return ratio_; }

    void setAttack(float sec) noexcept {
        attackSec_ = std::clamp(sec, 0.0001f, 1.0f);
        compL_.SetAttack(attackSec_);
        compR_.SetAttack(attackSec_);
    }
    [[nodiscard]] float getAttack() const noexcept { return attackSec_; }

    void setRelease(float sec) noexcept {
        releaseSec_ = std::clamp(sec, 0.001f, 3.0f);
        compL_.SetRelease(releaseSec_);
        compR_.SetRelease(releaseSec_);
    }
    [[nodiscard]] float getRelease() const noexcept { return releaseSec_; }

    void setMakeupGain(float db) noexcept {
        makeupGainDb_ = std::clamp(db, -24.0f, 36.0f);
        makeupGainLinear_ = std::pow(10.0f, makeupGainDb_ / 20.0f);
        compL_.SetMakeup(makeupGainDb_);
        compR_.SetMakeup(makeupGainDb_);
    }
    [[nodiscard]] float getMakeupGain() const noexcept { return makeupGainDb_; }

    void setDryWet(float mix) noexcept {
        dryWet_ = std::clamp(mix, 0.0f, 1.0f);
    }
    [[nodiscard]] float getDryWet() const noexcept { return dryWet_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setThreshold(value); break;
            case 1: setRatio(value); break;
            case 2: setAttack(value); break;
            case 3: setRelease(value); break;
            case 4: setMakeupGain(value); break;
            case 5: setDryWet(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override;

private:
    void updateCompressorParams() noexcept {
        compL_.SetThreshold(thresholdDb_);
        compR_.SetThreshold(thresholdDb_);
        compL_.SetRatio(ratio_);
        compR_.SetRatio(ratio_);
        compL_.SetAttack(attackSec_);
        compR_.SetAttack(attackSec_);
        compL_.SetRelease(releaseSec_);
        compR_.SetRelease(releaseSec_);
        compL_.SetMakeup(makeupGainDb_);
        compR_.SetMakeup(makeupGainDb_);
    }

    daisysp::Compressor compL_, compR_;

    float sampleRate_{48000.0f};
    float thresholdDb_{-18.0f};
    float ratio_{4.0f};
    float attackSec_{0.010f};
    float releaseSec_{0.100f};
    float makeupGainDb_{0.0f};
    float makeupGainLinear_{1.0f};
    float dryWet_{1.0f};
};

} // namespace eatsbits::audio

#endif // EATS_DAISY_COMPRESSOR_NODE_HPP
