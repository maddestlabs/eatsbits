#ifndef EATS_COMPRESSOR_NODE_HPP
#define EATS_COMPRESSOR_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular Studio Stereo Compressor Graph Node with Sidechain Keying.
 * Powered by dual DaisySP Compressor engines with true zero-allocation processing.
 * Input Port 0: Stereo Audio (L/R)
 * Input Port 1: Sidechain Key (optional, mono or stereo)
 * Output Port 0: Compressed Stereo Audio (L/R)
 */
class CompressorNode : public GraphNode {
public:
    explicit CompressorNode(std::string name = "Compressor")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addInputPort(2);  // Port 1: Sidechain in (optional, accepts 1 or 2 channels)
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
        gainReductionDb_ = 0.0f;
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

    void setKnee(float db) noexcept {
        kneeDb_ = std::clamp(db, 0.0f, 24.0f);
    }
    [[nodiscard]] float getKnee() const noexcept { return kneeDb_; }

    void setAttack(float val) noexcept {
        // Automatically handle ms (> 1.0f) or seconds (<= 1.0f)
        attackSec_ = std::clamp(val > 1.0f ? val / 1000.0f : val, 0.0001f, 1.0f);
        compL_.SetAttack(attackSec_);
        compR_.SetAttack(attackSec_);
    }
    [[nodiscard]] float getAttack() const noexcept { return attackSec_; }

    void setRelease(float val) noexcept {
        // Automatically handle ms (> 3.0f) or seconds (<= 3.0f)
        releaseSec_ = std::clamp(val > 3.0f ? val / 1000.0f : val, 0.001f, 3.0f);
        compL_.SetRelease(releaseSec_);
        compR_.SetRelease(releaseSec_);
    }
    [[nodiscard]] float getRelease() const noexcept { return releaseSec_; }

    void setMakeupGain(float db) noexcept {
        makeupGainDb_ = std::clamp(db, -24.0f, 36.0f);
        compL_.SetMakeup(makeupGainDb_);
        compR_.SetMakeup(makeupGainDb_);
    }
    [[nodiscard]] float getMakeupGain() const noexcept { return makeupGainDb_; }

    void setDryWet(float mix) noexcept {
        dryWet_ = std::clamp(mix, 0.0f, 1.0f);
    }
    [[nodiscard]] float getDryWet() const noexcept { return dryWet_; }

    void setMix(float mix) noexcept { setDryWet(mix); }
    [[nodiscard]] float getMix() const noexcept { return getDryWet(); }

    void setRmsMode(bool rms) noexcept { rmsMode_ = rms; }
    [[nodiscard]] bool isRmsMode() const noexcept { return rmsMode_; }

    [[nodiscard]] float getCurrentGainReductionDb() const noexcept {
        return gainReductionDb_;
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setThreshold(value); break;
            case 1: setRatio(value); break;
            case 2:
                if (value <= 24.0f && value >= 0.0f) setKnee(value);
                setAttack(value);
                break;
            case 3: setAttack(value); break;
            case 4: setRelease(value); break;
            case 5: setMakeupGain(value); break;
            case 6: setDryWet(value); break;
            case 7: setRmsMode(value > 0.5f); break;
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
    float kneeDb_{0.0f};
    float attackSec_{0.010f};
    float releaseSec_{0.100f};
    float makeupGainDb_{0.0f};
    float dryWet_{1.0f};
    float gainReductionDb_{0.0f};
    bool rmsMode_{false};
};

// Aliases for prefix-free and backward compatibility
using DaisyCompressorNode = CompressorNode;

} // namespace eatsbits::audio

#endif // EATS_COMPRESSOR_NODE_HPP
