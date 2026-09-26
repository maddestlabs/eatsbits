#ifndef EATS_YM2612_NODE_HPP
#define EATS_YM2612_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/ym2612_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Sega Genesis / Mega Drive Yamaha YM2612 (OPN2) 4-Operator FM Synthesizer Node.
 * 8 routing algorithms, Operator 1 self-feedback, authentic Total Level attenuation, and SFXR procedural presets.
 */
class Ym2612Node : public GraphNode {
public:
    explicit Ym2612Node(std::string name = "Ym2612Fm")
        : GraphNode(std::move(name)) {
        // Stereo generator node: 0 inputs, 1 stereo output port (2 channels)
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        ym2612_.setSampleRate(static_cast<float>(sampleRate));
    }

    void reset() noexcept override {
        ym2612_.allNotesOff();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        ym2612_.noteOn(freq, velocity);
    }

    void noteOff(uint8_t note) noexcept {
        const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        ym2612_.noteOff(freq);
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: { // Algorithm (0..7)
                const int alg = std::clamp(static_cast<int>(value), 0, 7);
                for (auto& v : ym2612_.voices) v.algorithm = alg;
                break;
            }
            case 1: { // Feedback (0..7)
                const int fb = std::clamp(static_cast<int>(value), 0, 7);
                for (auto& v : ym2612_.voices) v.feedback = fb;
                break;
            }
            case 2: // Master Volume
                ym2612_.masterVolume = std::clamp(value, 0.0f, 1.0f);
                break;
            case 3: // Op1 Multiplier
                for (auto& v : ym2612_.voices) v.operators[0].multiplier = std::max(0.5f, value);
                break;
            case 4: // Op1 Total Level (0..127)
                for (auto& v : ym2612_.voices) v.operators[0].setTotalLevel(value);
                break;
            case 5: // Op2 Multiplier
                for (auto& v : ym2612_.voices) v.operators[1].multiplier = std::max(0.5f, value);
                break;
            case 6: // Op2 Total Level
                for (auto& v : ym2612_.voices) v.operators[1].setTotalLevel(value);
                break;
            case 7: // Op3 Multiplier
                for (auto& v : ym2612_.voices) v.operators[2].multiplier = std::max(0.5f, value);
                break;
            case 8: // Op3 Total Level
                for (auto& v : ym2612_.voices) v.operators[2].setTotalLevel(value);
                break;
            case 9: // Op4 Multiplier
                for (auto& v : ym2612_.voices) v.operators[3].multiplier = std::max(0.5f, value);
                break;
            case 10: // Op4 Total Level
                for (auto& v : ym2612_.voices) v.operators[3].setTotalLevel(value);
                break;
            case 11: // Global Attack
                for (auto& v : ym2612_.voices) {
                    for (auto& op : v.operators) op.attack = std::clamp(value, 0.001f, 2.0f);
                }
                break;
            case 12: // Global Decay
                for (auto& v : ym2612_.voices) {
                    for (auto& op : v.operators) op.decay = std::clamp(value, 0.01f, 2.0f);
                }
                break;
            case 13: // Global Sustain
                for (auto& v : ym2612_.voices) {
                    for (auto& op : v.operators) op.sustain = std::clamp(value, 0.0f, 1.0f);
                }
                break;
            case 14: // Global Release
                for (auto& v : ym2612_.voices) {
                    for (auto& op : v.operators) op.release = std::clamp(value, 0.01f, 2.0f);
                }
                break;
            case 15: { // SFXR Preset Apply (0: Laser, 1: Explosion, 2: Powerup, 3: Coin, 4: Jump, 5: Hit)
                const int sfx = static_cast<int>(value);
                for (auto& v : ym2612_.voices) {
                    SFXRGenerator::configureFromType(v, sfx);
                }
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
                ym2612_.allNotesOff();
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

        for (uint32_t i = 0; i < numFrames; ++i) {
            ym2612_.processStereo(outL[i], outR[i]);
        }
    }

    [[nodiscard]] YM2612Synth& getSynth() noexcept { return ym2612_; }
    [[nodiscard]] const YM2612Synth& getSynth() const noexcept { return ym2612_; }

private:
    YM2612Synth ym2612_;
};

} // namespace eatsbits::audio

#endif // EATS_YM2612_NODE_HPP
