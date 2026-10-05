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

    void reset() noexcept override {
        filterStateL_ = 0.0f;
        filterStateR_ = 0.0f;
    }

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

    void setTone(float tone) noexcept {
        tone_ = std::clamp(tone, 0.0f, 1.0f);
    }
    [[nodiscard]] float getTone() const noexcept { return tone_; }

    void setBias(float bias) noexcept {
        bias_ = std::clamp(bias, 0.0f, 1.0f);
    }
    [[nodiscard]] float getBias() const noexcept { return bias_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setDrive(value); break;
            case 1: setMix(value); break;
            case 2: setShape(static_cast<int>(value)); break;
            case 3: setTone(value); break;
            case 4: setBias(value); break;
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
            if (inL) std::copy_n(inL, numFrames, outL); else std::fill_n(outL, numFrames, 0.0f);
            if (inR) std::copy_n(inR, numFrames, outR); else std::fill_n(outR, numFrames, 0.0f);
            return;
        }

        const float* srcL = inL ? inL : inR;
        const float* srcR = inR ? inR : inL;

        const float dry = 1.0f - mix_;
        const float wet = mix_;
        const float drive = drive_;

        // Dynamic makeup gain compensation to prevent volume blasting while adding rich harmonics
        const float makeup = 1.0f / std::sqrt(std::max(1.0f, drive));

        // Asymmetric bias DC offset
        const float biasOffset = (bias_ - 0.5f) * 0.4f;

        // Transfer curve mapping
        auto transferCurve = [this, drive](float sample) -> float {
            if (shape_ == 1) {
                // Symmetric soft saturation
                return std::tanh(sample * drive);
            } else if (shape_ == 2) {
                // Sine wavefolding
                return std::sin(sample * drive * 1.57079632679f);
            } else {
                // Asymmetric tube distortion (shape 0 default)
                float d = sample * drive;
                return (d > 0.0f) ? std::tanh(d) : (d / (1.0f + d * d));
            }
        };

        const float dcComp = transferCurve(biasOffset);

        // 1-pole lowpass tone smoothing: tone=1.0 is full open, tone=0 is dark/rolled off
        const float alpha = std::clamp(0.05f + 0.95f * tone_, 0.05f, 1.0f);

        for (uint32_t i = 0; i < numFrames; ++i) {
            float xL = srcL[i];
            float xR = srcR[i];

            float satL = (transferCurve(xL + biasOffset) - dcComp) * makeup;
            float satR = (transferCurve(xR + biasOffset) - dcComp) * makeup;

            // Tone filter stage
            filterStateL_ += alpha * (satL - filterStateL_);
            filterStateR_ += alpha * (satR - filterStateR_);
            satL = filterStateL_;
            satR = filterStateR_;

            outL[i] = xL * dry + satL * wet;
            outR[i] = xR * dry + satR * wet;
        }
    }

private:
    float drive_{2.0f};
    float mix_{0.8f};
    int shape_{0};
    float tone_{0.75f};
    float bias_{0.5f};
    float filterStateL_{0.0f};
    float filterStateR_{0.0f};
};

} // namespace eatsbits::audio

#endif // EATS_WAVESHAPER_NODE_HPP
