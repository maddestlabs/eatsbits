#ifndef EATS_CONVOLVER_NODE_HPP
#define EATS_CONVOLVER_NODE_HPP

#include <vector>
#include <string>
#include <memory>
#include "../graph_node.hpp"
#include "../../dsp/convolver_core.hpp"
#include "../../procedural_ir_generator.hpp"

namespace eatsbits::audio {

/**
 * Modular Convolution Reverb Node for the AudioGraph.
 * Embeds the real-time ConvolverCore with built-in procedural physics simulation,
 * dynamic room resizing, frequency-dependent damping, and gated reverb shaping.
 */
class ConvolverNode : public GraphNode {
public:
    explicit ConvolverNode(std::string name = "ConvolverReverb")
        : GraphNode(std::move(name)),
          convolver_(44100.0f) {
        addInputPort(2);  // Stereo input (Port 0: Ch 0=L, Ch 1=R)
        addOutputPort(2); // Stereo output (Port 0: Ch 0=L, Ch 1=R)
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        convolver_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        convolver_.reset();
    }

    void setMix(float mix) noexcept { convolver_.setMix(mix); }
    [[nodiscard]] float getMix() const noexcept { return convolver_.getMix(); }

    void setPreDelay(float ms) noexcept { convolver_.setPreDelay(ms); }
    [[nodiscard]] float getPreDelay() const noexcept { return convolver_.getPreDelay(); }

    void setDecay(float decay) { convolver_.setDecay(decay); }
    [[nodiscard]] float getDecay() const noexcept { return convolver_.getDecay(); }

    void setRoomSize(float scale) { convolver_.setRoomSize(scale); }
    [[nodiscard]] float getRoomSize() const noexcept { return convolver_.getRoomSize(); }

    void setDamping(float damping) { convolver_.setDamping(damping); }
    [[nodiscard]] float getDamping() const noexcept { return convolver_.getDamping(); }

    void setGated(bool gated, float holdMs = 180.0f, float releaseMs = 20.0f) {
        convolver_.setGated(gated, holdMs, releaseMs);
    }
    [[nodiscard]] bool isGated() const noexcept { return convolver_.isGated(); }

    void setHighCut(float hz) { convolver_.setHighCut(hz); }
    [[nodiscard]] float getHighCut() const noexcept { return convolver_.getHighCut(); }

    void setLowCut(float hz) { convolver_.setLowCut(hz); }
    [[nodiscard]] float getLowCut() const noexcept { return convolver_.getLowCut(); }

    void loadPreset(const std::string& name) { convolver_.loadPreset(name); }
    void loadPresetIndex(size_t index) { convolver_.loadPresetIndex(index); }
    [[nodiscard]] const std::string& getCurrentPresetName() const noexcept { return convolver_.getCurrentPresetName(); }

    void setAcousticSpace(const AcousticSpaceParams& params) { convolver_.setAcousticSpace(params); }
    [[nodiscard]] const AcousticSpaceParams& getAcousticSpace() const noexcept { return convolver_.getCurrentSpaceParams(); }

    StereoIRBuffer bakeCustomSpace(const AcousticSpaceParams& params) {
        return convolver_.bakeCustomSpace(params);
    }

    dsp::ConvolverCore& getCore() noexcept { return convolver_; }
    const dsp::ConvolverCore& getCore() const noexcept { return convolver_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setMix(value); break;
            case 1: loadPresetIndex(static_cast<size_t>(value)); break;
            case 2: setPreDelay(value); break;
            case 3: setDecay(value); break;
            case 4: setHighCut(value); break;
            case 5: setLowCut(value); break;
            case 6: setRoomSize(value); break;
            case 7: setDamping(value); break;
            case 8: setGated(value > 0.5f); break;
            default: break;
        }
    }

    [[nodiscard]] float getParameter(uint32_t paramId) const noexcept {
        switch (paramId) {
            case 0: return getMix();
            case 1: return 0.0f;
            case 2: return getPreDelay();
            case 3: return getDecay();
            case 4: return getHighCut();
            case 5: return getLowCut();
            case 6: return getRoomSize();
            case 7: return getDamping();
            case 8: return isGated() ? 1.0f : 0.0f;
            default: return 0.0f;
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

        convolver_.processStereo(srcL, srcR, outL, outR, numFrames);
    }

private:
    dsp::ConvolverCore convolver_;
};

} // namespace eatsbits::audio

#endif // EATS_CONVOLVER_NODE_HPP
