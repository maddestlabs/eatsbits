#ifndef EATS_WAVEGUIDE_NODE_HPP
#define EATS_WAVEGUIDE_NODE_HPP

#include <vector>
#include <string>
#include <memory>
#include "../graph_node.hpp"
#include "../../dsp/waveguide_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Digital Waveguide Node for the AudioGraph.
 * Embeds stereo DigitalWaveguideCore string resonators with frequency-dependent damping.
 */
class WaveguideNode : public GraphNode {
public:
    explicit WaveguideNode(std::string name = "DigitalWaveguide")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Stereo input (Port 0: Ch 0=L, Ch 1=R)
        addOutputPort(2); // Stereo output (Port 0: Ch 0=L, Ch 1=R)
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        reset();
    }

    void reset() noexcept override {
        wgL_.reset();
        wgR_.reset();
    }

    void setFrequency(float hz) noexcept { freqHz_ = hz; }
    [[nodiscard]] float getFrequency() const noexcept { return freqHz_; }

    void setFeedback(float fb) noexcept { feedback_ = fb; }
    [[nodiscard]] float getFeedback() const noexcept { return feedback_; }

    void setDamping(float damp) noexcept { damping_ = damp; }
    [[nodiscard]] float getDamping() const noexcept { return damping_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setFrequency(value); break;
            case 1: setFeedback(value); break;
            case 2: setDamping(value); break;
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

        wgL_.process(outL, numFrames, static_cast<float>(sampleRate_), freqHz_, feedback_, damping_);
        wgR_.process(outR, numFrames, static_cast<float>(sampleRate_), freqHz_ * 1.002f, feedback_, damping_);
    }

private:
    float freqHz_ = 440.0f;
    float feedback_ = 0.995f;
    float damping_ = 0.25f;

    dsp::DigitalWaveguideCore wgL_;
    dsp::DigitalWaveguideCore wgR_;
};

} // namespace eatsbits::audio

#endif // EATS_WAVEGUIDE_NODE_HPP
