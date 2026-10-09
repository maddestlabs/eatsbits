#ifndef EATS_DAISY_OSCILLATOR_NODE_HPP
#define EATS_DAISY_OSCILLATOR_NODE_HPP

#include <string>
#include <algorithm>
#include <cmath>
#include "../../graph_node.hpp"
#include "daisysp.h"

namespace eatsbits::audio {

/**
 * Modular DaisySP Oscillator Graph Node.
 * Input Port 0: CV / Pitch offset modulation (optional, in volts where 1.0 = 1 Octave)
 * Output Port 0: Stereo Audio (L/R)
 */
class DaisyOscillatorNode : public GraphNode {
public:
    enum class Waveform {
        Sine = 0,
        Triangle = 1,
        Saw = 2,
        Ramp = 3,
        Square = 4,
        PolyBlepTri = 5,
        PolyBlepSaw = 6,
        PolyBlepSquare = 7,
        VariableSaw = 8
    };

    explicit DaisyOscillatorNode(std::string name = "DaisyOscillator")
        : GraphNode(std::move(name)) {
        addInputPort(1);  // Port 0: CV Modulation
        addOutputPort(2); // Port 0: Stereo out
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = static_cast<float>(sampleRate);
        maxBlockSize_ = maxBlockSize;
        osc_.Init(sampleRate_);
        osc_.SetFreq(frequency_);
        osc_.SetAmp(amplitude_);
        osc_.SetPw(pulseWidth_);
        updateOscWaveform();

        varSaw_.Init(sampleRate_);
        varSaw_.SetFreq(frequency_);
        varSaw_.SetPW(sawWidth_);
        reset();
    }

    void reset() noexcept override {
        osc_.Reset();
        varSaw_.Init(sampleRate_);
    }

    void setFrequency(float freqHz) noexcept {
        frequency_ = std::clamp(freqHz, 5.0f, 22000.0f);
        osc_.SetFreq(frequency_);
        varSaw_.SetFreq(frequency_);
    }
    [[nodiscard]] float getFrequency() const noexcept { return frequency_; }

    void setWaveform(Waveform wf) noexcept {
        waveform_ = wf;
        updateOscWaveform();
    }
    [[nodiscard]] Waveform getWaveform() const noexcept { return waveform_; }

    void setAmplitude(float amp) noexcept {
        amplitude_ = std::clamp(amp, 0.0f, 2.0f);
        osc_.SetAmp(amplitude_);
    }
    [[nodiscard]] float getAmplitude() const noexcept { return amplitude_; }

    void setPulseWidth(float pw) noexcept {
        pulseWidth_ = std::clamp(pw, 0.01f, 0.99f);
        osc_.SetPw(pulseWidth_);
    }
    [[nodiscard]] float getPulseWidth() const noexcept { return pulseWidth_; }

    void setSawWidth(float sw) noexcept {
        sawWidth_ = std::clamp(sw, 0.0f, 1.0f);
        varSaw_.SetPW(sawWidth_);
    }
    [[nodiscard]] float getSawWidth() const noexcept { return sawWidth_; }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setFrequency(value); break;
            case 1: setWaveform(static_cast<Waveform>(static_cast<int>(value))); break;
            case 2: setAmplitude(value); break;
            case 3: setPulseWidth(value); break;
            case 4: setSawWidth(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override;

private:
    void updateOscWaveform() noexcept {
        switch (waveform_) {
            case Waveform::Sine:           osc_.SetWaveform(daisysp::Oscillator::WAVE_SIN); break;
            case Waveform::Triangle:       osc_.SetWaveform(daisysp::Oscillator::WAVE_TRI); break;
            case Waveform::Saw:            osc_.SetWaveform(daisysp::Oscillator::WAVE_SAW); break;
            case Waveform::Ramp:           osc_.SetWaveform(daisysp::Oscillator::WAVE_RAMP); break;
            case Waveform::Square:         osc_.SetWaveform(daisysp::Oscillator::WAVE_SQUARE); break;
            case Waveform::PolyBlepTri:    osc_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_TRI); break;
            case Waveform::PolyBlepSaw:    osc_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SAW); break;
            case Waveform::PolyBlepSquare: osc_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_SQUARE); break;
            case Waveform::VariableSaw:    break;
        }
    }

    daisysp::Oscillator osc_;
    daisysp::VariableSawOscillator varSaw_;

    float sampleRate_{48000.0f};
    float frequency_{440.0f};
    float amplitude_{0.8f};
    float pulseWidth_{0.5f};
    float sawWidth_{0.5f};
    Waveform waveform_{Waveform::PolyBlepSaw};
};

} // namespace eatsbits::audio

#endif // EATS_DAISY_OSCILLATOR_NODE_HPP
