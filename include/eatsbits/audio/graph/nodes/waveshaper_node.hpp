#ifndef EATS_WAVESHAPER_NODE_HPP
#define EATS_WAVESHAPER_NODE_HPP

#include <vector>
#include <cmath>
#include <algorithm>
#include "../graph_node.hpp"

namespace eatsbits::audio {

/**
 * Modular Dynamic WaveShaper / Tube Distortion Graph Node.
 * Supports tube saturation, soft clipping, foldback, and asymmetric warmth.
 */
class WaveShaperNode : public GraphNode {
public:
    explicit WaveShaperNode(std::string name = "TubeDistortion")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        reset();
    }

    void reset() noexcept override {}

    void setDrive(float drive) noexcept {
        drive_ = std::clamp(drive, 0.05f, 20.0f);
    }
    [[nodiscard]] float getDrive() const noexcept { return drive_; }

    void setMix(float mix) noexcept {
        mix_ = std::clamp(mix, 0.0f, 1.0f);
    }
    [[nodiscard]] float getMix() const noexcept { return mix_; }

    void setShape(int shape) noexcept {
        shape_ = shape;
    }
    [[nodiscard]] int getShape() const noexcept { return shape_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setDrive(value); break;
            case 1: setMix(value); break;
            case 2: setShape(static_cast<int>(value)); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        const float* inL = getInputBuffer(0, 0);
        const float* inR = getInputBuffer(0, 1);
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);

        if (!outL || !outR) return;

        if (!inL && !inR) {
            std::fill_n(outL, numFrames, 0.0f);
            std::fill_n(outR, numFrames, 0.0f);
            return;
        }

        const float* srcL = inL ? inL : inR;
        const float* srcR = inR ? inR : inL;

        const float dry = 1.0f - mix_;
        const float wet = mix_;
        const float drive = drive_;

        for (uint32_t i = 0; i < numFrames; ++i) {
            float xL = srcL[i];
            float xR = srcR[i];

            // Asymmetric soft tube distortion
            float drivenL = xL * drive;
            float satL = (drivenL > 0.0f) ? std::tanh(drivenL) : (drivenL / (1.0f + drivenL * drivenL));
            outL[i] = xL * dry + satL * wet;

            float drivenR = xR * drive;
            float satR = (drivenR > 0.0f) ? std::tanh(drivenR) : (drivenR / (1.0f + drivenR * drivenR));
            outR[i] = xR * dry + satR * wet;
        }
    }

private:
    float drive_{2.0f};
    float mix_{0.8f};
    int shape_{0};
};

} // namespace eatsbits::audio

#endif // EATS_WAVESHAPER_NODE_HPP
