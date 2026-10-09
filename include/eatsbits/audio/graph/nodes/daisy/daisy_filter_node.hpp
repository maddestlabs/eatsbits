#ifndef EATS_DAISY_FILTER_NODE_HPP
#define EATS_DAISY_FILTER_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular DaisySP Filter Graph Node.
 * Modes: MoogLadder (4-pole 24dB LP), SVF Lowpass, SVF Highpass, SVF Bandpass, SVF Notch.
 * Input Port 0: Stereo Audio (L/R)
 * Input Port 1: CV Cutoff modulation (optional, in volts where 1.0 = 1 Octave)
 * Output Port 0: Filtered Stereo Audio (L/R)
 */
class DaisyFilterNode : public GraphNode {
public:
    enum class FilterMode {
        MoogLadder = 0,
        SvfLowpass = 1,
        SvfHighpass = 2,
        SvfBandpass = 3,
        SvfNotch = 4
    };

    explicit DaisyFilterNode(std::string name = "DaisyFilter")
        : GraphNode(std::move(name)) {
        addInputPort(2);  // Port 0: Stereo in
        addInputPort(1);  // Port 1: CV Cutoff mod (optional)
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;

        moogL_.Init(sampleRate_);
        moogR_.Init(sampleRate_);
        moogL_.SetFreq(cutoff_);
        moogR_.SetFreq(cutoff_);
        moogL_.SetRes(resonance_);
        moogR_.SetRes(resonance_);

        svfL_.Init(sampleRate_);
        svfR_.Init(sampleRate_);
        svfL_.SetFreq(cutoff_);
        svfR_.SetFreq(cutoff_);
        svfL_.SetRes(resonance_);
        svfR_.SetRes(resonance_);
        svfL_.SetDrive(drive_);
        svfR_.SetDrive(drive_);

        reset();
    }

    void reset() noexcept override {
        // MoogLadder & Svf hold internal sample history
        moogL_.Init(sampleRate_);
        moogR_.Init(sampleRate_);
        svfL_.Init(sampleRate_);
        svfR_.Init(sampleRate_);
        applyParameters();
    }

    void setCutoff(float freqHz) noexcept {
        cutoff_ = std::clamp(freqHz, 10.0f, (sampleRate_ > 0.0f ? sampleRate_ * 0.48f : 20000.0f));
        moogL_.SetFreq(cutoff_);
        moogR_.SetFreq(cutoff_);
        svfL_.SetFreq(std::min(cutoff_, sampleRate_ / 3.0f));
        svfR_.SetFreq(std::min(cutoff_, sampleRate_ / 3.0f));
    }
    [[nodiscard]] float getCutoff() const noexcept { return cutoff_; }

    void setResonance(float res) noexcept {
        resonance_ = std::clamp(res, 0.0f, 0.99f);
        moogL_.SetRes(resonance_);
        moogR_.SetRes(resonance_);
        svfL_.SetRes(resonance_);
        svfR_.SetRes(resonance_);
    }
    [[nodiscard]] float getResonance() const noexcept { return resonance_; }

    void setDrive(float drive) noexcept {
        drive_ = std::clamp(drive, 0.0f, 1.0f);
        svfL_.SetDrive(drive_);
        svfR_.SetDrive(drive_);
    }
    [[nodiscard]] float getDrive() const noexcept { return drive_; }

    void setMode(FilterMode mode) noexcept {
        mode_ = mode;
    }
    [[nodiscard]] FilterMode getMode() const noexcept { return mode_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setCutoff(value); break;
            case 1: setResonance(value); break;
            case 2: setDrive(value); break;
            case 3: setMode(static_cast<FilterMode>(static_cast<int>(value))); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override;

private:
    void applyParameters() noexcept {
        setCutoff(cutoff_);
        setResonance(resonance_);
        setDrive(drive_);
    }

    daisysp::MoogLadder moogL_, moogR_;
    daisysp::Svf svfL_, svfR_;

    float sampleRate_{48000.0f};
    float cutoff_{2000.0f};
    float resonance_{0.2f};
    float drive_{0.0f};
    FilterMode mode_{FilterMode::MoogLadder};
};

} // namespace eatsbits::audio

#endif // EATS_DAISY_FILTER_NODE_HPP
