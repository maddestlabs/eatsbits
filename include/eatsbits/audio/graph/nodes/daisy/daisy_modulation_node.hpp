#ifndef EATS_DAISY_MODULATION_NODE_HPP
#define EATS_DAISY_MODULATION_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular DaisySP Modulation & Saturation Graph Node.
 * Modes: Chorus, Flanger, Overdrive.
 * Input Port 0: Stereo Audio (L/R)
 * Output Port 0: Modulated/Saturated Stereo Audio (L/R)
 */
class DaisyModulationNode : public GraphNode {
public:
    enum class ModType {
        Chorus = 0,
        Flanger = 1,
        Overdrive = 2
    };

    explicit DaisyModulationNode(std::string name = "DaisyModulation")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;

        chorus_.Init(sampleRate_);
        chorus_.SetPan(0.0f, 1.0f); // Wide stereo spread

        flangerL_.Init(sampleRate_);
        flangerR_.Init(sampleRate_);
        flangerL_.SetLfoFreq(rate_);
        flangerR_.SetLfoFreq(rate_ * 1.05f); // Subtle stereo offset
        flangerL_.SetLfoDepth(depth_);
        flangerR_.SetLfoDepth(depth_);
        flangerL_.SetFeedback(feedback_);
        flangerR_.SetFeedback(feedback_);

        overdriveL_.Init();
        overdriveR_.Init();
        overdriveL_.SetDrive(drive_);
        overdriveR_.SetDrive(drive_);

        reset();
    }

    void reset() noexcept override {
        chorus_.Init(sampleRate_);
        chorus_.SetPan(0.0f, 1.0f);
        flangerL_.Init(sampleRate_);
        flangerR_.Init(sampleRate_);
        overdriveL_.Init();
        overdriveR_.Init();
        applyParameters();
    }

    void setType(ModType type) noexcept { type_ = type; }
    [[nodiscard]] ModType getType() const noexcept { return type_; }

    void setRate(float rateHz) noexcept {
        rate_ = std::clamp(rateHz, 0.05f, 20.0f);
        flangerL_.SetLfoFreq(rate_);
        flangerR_.SetLfoFreq(rate_ * 1.05f);
    }
    [[nodiscard]] float getRate() const noexcept { return rate_; }

    void setDepth(float depth) noexcept {
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        flangerL_.SetLfoDepth(depth_);
        flangerR_.SetLfoDepth(depth_);
    }
    [[nodiscard]] float getDepth() const noexcept { return depth_; }

    void setFeedback(float fb) noexcept {
        feedback_ = std::clamp(fb, 0.0f, 0.95f);
        flangerL_.SetFeedback(feedback_);
        flangerR_.SetFeedback(feedback_);
    }
    [[nodiscard]] float getFeedback() const noexcept { return feedback_; }

    void setDrive(float drv) noexcept {
        drive_ = std::clamp(drv, 0.0f, 1.0f);
        overdriveL_.SetDrive(drive_);
        overdriveR_.SetDrive(drive_);
    }
    [[nodiscard]] float getDrive() const noexcept { return drive_; }

    void setDryWet(float mix) noexcept {
        dryWet_ = std::clamp(mix, 0.0f, 1.0f);
    }
    [[nodiscard]] float getDryWet() const noexcept { return dryWet_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setRate(value); break;
            case 1: setDepth(value); break;
            case 2: setFeedback(value); break;
            case 3: setDrive(value); break;
            case 4: setDryWet(value); break;
            case 5: setType(static_cast<ModType>(static_cast<int>(value))); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override;

private:
    void applyParameters() noexcept {
        setRate(rate_);
        setDepth(depth_);
        setFeedback(feedback_);
        setDrive(drive_);
    }

    daisysp::Chorus chorus_;
    daisysp::Flanger flangerL_, flangerR_;
    daisysp::Overdrive overdriveL_, overdriveR_;

    float sampleRate_{48000.0f};
    float rate_{1.2f};
    float depth_{0.5f};
    float feedback_{0.3f};
    float drive_{0.4f};
    float dryWet_{0.5f};
    ModType type_{ModType::Chorus};
};

} // namespace eatsbits::audio

#endif // EATS_DAISY_MODULATION_NODE_HPP
