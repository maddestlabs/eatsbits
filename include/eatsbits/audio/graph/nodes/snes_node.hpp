#ifndef EATS_SNES_NODE_HPP
#define EATS_SNES_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/snes_dsp_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Super Nintendo S-SMP / SPC700 16-Bit S-DSP Node.
 * 12 BRR waveforms with Gaussian smoothing, 6 envelope modes, 15-bit LFSR noise, PMOD pitch modulation, and 8-tap FIR echo unit.
 */
class SnesNode : public GraphNode {
public:
    explicit SnesNode(std::string name = "SnesDsp")
        : GraphNode(std::move(name)) {
        // Stereo generator node: 0 inputs, 1 stereo output port (2 channels)
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        snes_.reset();
    }

    void reset() noexcept override {
        snes_.reset();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        snes_.noteOn(-1, freq, velocity, 0.45f);
    }

    void noteOff(uint8_t /*note*/) noexcept {
        // Release first active voice
        for (int i = 0; i < 8; ++i) {
            if (snes_.voices[i].active && !snes_.voices[i].keyOff) {
                snes_.noteOff(i);
                break;
            }
        }
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: { // Waveform (0..11)
                const int wIdx = std::clamp(static_cast<int>(value), 0, static_cast<int>(SNESWaveform::Count) - 1);
                for (auto& v : snes_.voices) v.waveform = static_cast<SNESWaveform>(wIdx);
                break;
            }
            case 1: { // Envelope Mode (0..5)
                const int mIdx = std::clamp(static_cast<int>(value), 0, 5);
                for (auto& v : snes_.voices) v.envMode = static_cast<SNESEnvelopeMode>(mIdx);
                break;
            }
            case 2: // Attack
                for (auto& v : snes_.voices) v.attack = std::clamp(value, 0.001f, 2.0f);
                break;
            case 3: // Decay
                for (auto& v : snes_.voices) v.decay = std::clamp(value, 0.01f, 2.0f);
                break;
            case 4: // Sustain
                for (auto& v : snes_.voices) v.sustain = std::clamp(value, 0.0f, 1.0f);
                break;
            case 5: // Release
                for (auto& v : snes_.voices) v.release = std::clamp(value, 0.01f, 2.0f);
                break;
            case 6: // Echo Enabled
                snes_.echo.enabled = (value > 0.5f);
                break;
            case 7: // Echo Delay Ms
                snes_.echo.setDelayMs(static_cast<int>(value));
                break;
            case 8: // Echo Feedback
                snes_.echo.feedback = std::clamp(value, -1.0f, 1.0f);
                break;
            case 9: // Echo Volume
                snes_.echo.volume = std::clamp(value, 0.0f, 1.0f);
                break;
            case 10: // Noise Enabled
                for (auto& v : snes_.voices) v.noiseEnabled = (value > 0.5f);
                break;
            case 11: // Noise Rate
                for (auto& v : snes_.voices) v.noiseRate = std::clamp(static_cast<int>(value), 0, 14);
                break;
            case 12: // Noise Mix
                for (auto& v : snes_.voices) v.noiseMix = std::clamp(value, 0.0f, 1.0f);
                break;
            case 13: // PMOD Enabled
                for (int i = 1; i < 8; ++i) snes_.voices[i].pmodEnabled = (value > 0.5f);
                break;
            case 14: // Master Volume
                snes_.masterVolume = std::clamp(value, 0.0f, 1.0f);
                break;
            case 15: { // FIR Profile (0: Surround, 1: Dark, 2: Metallic, 3: Slapback)
                const int p = static_cast<int>(value);
                if (p == 1) snes_.echo.setFIRProfile("dark_hall");
                else if (p == 2) snes_.echo.setFIRProfile("metallic_chorus");
                else if (p == 3) snes_.echo.setFIRProfile("slapback");
                else snes_.echo.setFIRProfile("surround_reverb");
                break;
            }
            default: break;
        }
    }

    void handleEvent(const AudioEvent& event) noexcept override {
        switch (event.type) {
            case AudioEventType::NoteOn:
                noteOn(event.note, event.velocity);
                break;
            case AudioEventType::NoteOff:
                noteOff(event.note);
                break;
            case AudioEventType::AllNotesOff:
                snes_.allNotesOff();
                break;
            case AudioEventType::SetParameter:
                setParameter(event.paramId, event.paramValue);
                break;
            default:
                break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);
        if (!outL || !outR) return;

        if (!enabled_) {
            std::fill_n(outL, numFrames, 0.0f);
            std::fill_n(outR, numFrames, 0.0f);
            return;
        }

        const float sr = static_cast<float>(sampleRate_);
        for (uint32_t i = 0; i < numFrames; ++i) {
            snes_.processStereo(sr, outL[i], outR[i]);
        }
    }

    [[nodiscard]] SNESDSPEngine& getEngine() noexcept { return snes_; }
    [[nodiscard]] const SNESDSPEngine& getEngine() const noexcept { return snes_; }

private:
    SNESDSPEngine snes_;
};

} // namespace eatsbits::audio

#endif // EATS_SNES_NODE_HPP
