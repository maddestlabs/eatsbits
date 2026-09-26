#ifndef EATS_BIQUAD_NODE_HPP
#define EATS_BIQUAD_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/biquad.hpp"

namespace eatsbits::audio {

class BiquadNode : public GraphNode {
public:
    explicit BiquadNode(std::string name = "BiquadFilter")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Stereo input
        addOutputPort(2); // Stereo output
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        updateCoefficients();
        reset();
    }

    void reset() noexcept override {
        filterL_.reset();
        filterR_.reset();
    }

    void setFilterType(dsp::BiquadType type) noexcept {
        type_ = type;
        updateCoefficients();
    }

    void setCutoff(float cutoffHz) noexcept {
        cutoffHz_ = cutoffHz;
        updateCoefficients();
    }

    [[nodiscard]] float getCutoff() const noexcept { return cutoffHz_; }

    void setResonance(float q) noexcept {
        q_ = q;
        updateCoefficients();
    }

    [[nodiscard]] float getResonance() const noexcept { return q_; }

    void setGainDb(float gainDb) noexcept {
        gainDb_ = gainDb;
        updateCoefficients();
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setCutoff(value); break;
            case 1: setResonance(value); break;
            case 2: setGainDb(value); break;
            case 3: setFilterType(static_cast<dsp::BiquadType>(static_cast<int>(value))); break;
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

        filterL_.process_block(srcL, outL, numFrames);
        filterR_.process_block(srcR, outR, numFrames);
    }

private:
    void updateCoefficients() noexcept {
        filterL_.configure(type_, cutoffHz_, q_, gainDb_, static_cast<float>(sampleRate_));
        filterR_.configure(type_, cutoffHz_, q_, gainDb_, static_cast<float>(sampleRate_));
    }

    dsp::BiquadFilter filterL_;
    dsp::BiquadFilter filterR_;
    dsp::BiquadType type_{dsp::BiquadType::LowPass};
    float cutoffHz_{1000.0f};
    float q_{0.7071f};
    float gainDb_{0.0f};
};

} // namespace eatsbits::audio

#endif // EATS_BIQUAD_NODE_HPP
