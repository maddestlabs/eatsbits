#ifndef EATS_PARAMETRIC_EQ_NODE_HPP
#define EATS_PARAMETRIC_EQ_NODE_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"
#include "../../dsp/parametric_eq.hpp"

namespace eatsbits::audio {

/**
 * Modular 5-Band Studio Parametric Equalizer Graph Node.
 * Input Port 0: Stereo Audio (L/R)
 * Output Port 0: Filtered Stereo Audio (L/R)
 */
class ParametricEqNode : public GraphNode {
public:
    explicit ParametricEqNode(std::string name = "ParametricEQ")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        eq_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        eq_.reset();
    }

    void configureBand(size_t bandIdx, bool enabled, dsp::BiquadType type, float freqHz, float q, float gainDb) noexcept {
        eq_.configureBand(bandIdx, enabled, type, freqHz, q, gainDb);
    }

    void setBandGain(size_t bandIdx, float gainDb) noexcept {
        eq_.setBandGain(bandIdx, gainDb);
    }

    void setBandFrequency(size_t bandIdx, float freqHz) noexcept {
        eq_.setBandFrequency(bandIdx, freqHz);
    }

    void setBandQ(size_t bandIdx, float q) noexcept {
        eq_.setBandQ(bandIdx, q);
    }

    void setBandBypass(size_t bandIdx, bool bypass) noexcept {
        eq_.setBandBypass(bandIdx, bypass);
    }

    void setMasterGain(float gainDb) noexcept {
        eq_.setMasterGain(gainDb);
    }

    [[nodiscard]] float getMasterGain() const noexcept {
        return eq_.getMasterGain();
    }

    [[nodiscard]] const dsp::EqBandSettings& getBandSettings(size_t bandIdx) const noexcept {
        return eq_.getBandSettings(bandIdx);
    }

    [[nodiscard]] float evalMagnitudeDb(float freqHz) const noexcept {
        return eq_.evalMagnitudeDb(freqHz);
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        if (paramId < dsp::ParametricEqCore::kNumBands) {
            // Param 0..4: Band gain in dB
            setBandGain(paramId, value);
        } else if (paramId == 5) {
            // Param 5: Master gain in dB
            setMasterGain(value);
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

        eq_.processStereo(outL, outR, numFrames);
    }

private:
    dsp::ParametricEqCore eq_;
};

} // namespace eatsbits::audio

#endif // EATS_PARAMETRIC_EQ_NODE_HPP
