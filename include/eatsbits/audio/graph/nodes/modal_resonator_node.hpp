#ifndef EATS_MODAL_RESONATOR_NODE_HPP
#define EATS_MODAL_RESONATOR_NODE_HPP

#include <vector>
#include <string>
#include <memory>
#include "../graph_node.hpp"
#include "../../dsp/modal_resonator.hpp"

namespace eatsbits::audio {

/**
 * Modular Modal Resonator Node for the AudioGraph.
 * Embeds parallel 2nd-order bandpass resonators for acoustic body emulation.
 */
class ModalResonatorNode : public GraphNode {
public:
    explicit ModalResonatorNode(std::string name = "ModalResonator")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Stereo input (Port 0: Ch 0=L, Ch 1=R)
        addOutputPort(2); // Stereo output (Port 0: Ch 0=L, Ch 1=R)

        // Default Spanish guitar body mode configuration
        dsp::ModalResonatorBank::ModeConfig modes[4] = {
            {1.0f, 0.50f, 16.0f},
            {1.96f, 0.38f, 20.0f},
            {2.45f, 0.22f, 18.0f},
            {3.12f, 0.14f, 26.0f}
        };
        bankL_.setModes(std::span<const dsp::ModalResonatorBank::ModeConfig>(modes, 4));
        bankR_.setModes(std::span<const dsp::ModalResonatorBank::ModeConfig>(modes, 4));
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        reset();
    }

    void reset() noexcept override {
        bankL_.reset();
        bankR_.reset();
    }

    void setBaseFrequency(float hz) noexcept { baseFreq_ = hz; }
    [[nodiscard]] float getBaseFrequency() const noexcept { return baseFreq_; }

    void setMix(float mix) noexcept { mix_ = std::clamp(mix, 0.0f, 1.0f); }
    [[nodiscard]] float getMix() const noexcept { return mix_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setBaseFrequency(value); break;
            case 1: setMix(value); break;
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

        scratchL_.resize(numFrames);
        scratchR_.resize(numFrames);

        bankL_.process(srcL, scratchL_.data(), numFrames, static_cast<float>(sampleRate_), baseFreq_);
        bankR_.process(srcR, scratchR_.data(), numFrames, static_cast<float>(sampleRate_), baseFreq_);

        for (uint32_t i = 0; i < numFrames; ++i) {
            outL[i] = srcL[i] * (1.0f - mix_) + scratchL_[i] * mix_;
            outR[i] = srcR[i] * (1.0f - mix_) + scratchR_[i] * mix_;
        }
    }

private:
    float baseFreq_ = 196.0f;
    float mix_ = 0.5f;

    dsp::ModalResonatorBank bankL_;
    dsp::ModalResonatorBank bankR_;
    std::vector<float> scratchL_;
    std::vector<float> scratchR_;
};

} // namespace eatsbits::audio

#endif // EATS_MODAL_RESONATOR_NODE_HPP
