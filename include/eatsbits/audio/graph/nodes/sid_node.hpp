#ifndef EATS_SID_NODE_HPP
#define EATS_SID_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/sid_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Commodore 64 SID (MOS 6581 / 8580) Audio Source Node.
 * 3-voice chiptune synthesis, hardware PWM, arpeggios, and 12dB/oct resonant filter.
 */
class SidNode : public GraphNode {
public:
    explicit SidNode(std::string name = "C64SidSynth")
        : GraphNode(std::move(name)) {
        // Source node: 0 input ports, 1 stereo output port (2 channels)
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        sid_.prepare(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        sid_.reset();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        sid_.noteOn(note, velocity);
    }

    void noteOff(uint8_t note) noexcept {
        sid_.noteOff(note);
    }

    void setParameterByName(const std::string& name, float value) noexcept {
        sid_.setParameter(name, value);
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: sid_.setParameter("Waveform", value); break;
            case 1: sid_.setParameter("PulseWidth", value); break;
            case 2: sid_.setParameter("PwmRate", value); break;
            case 3: sid_.setParameter("PwmDepth", value); break;
            case 4: sid_.setParameter("ArpMode", value); break;
            case 5: sid_.setParameter("GlideSpeed", value); break;
            case 6: sid_.setParameter("ChipModel", value); break;
            case 7: sid_.setParameter("FilterMode", value); break;
            case 8: sid_.setParameter("Cutoff", value); break;
            case 9: sid_.setParameter("Resonance", value); break;
            case 10: sid_.setParameter("Overdrive", value); break;
            case 11: sid_.setParameter("Attack", value); break;
            case 12: sid_.setParameter("Decay", value); break;
            case 13: sid_.setParameter("Sustain", value); break;
            case 14: sid_.setParameter("Release", value); break;
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
                sid_.reset();
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

        sid_.processStereo(outL, outR, numFrames, static_cast<float>(sampleRate_));
    }

    [[nodiscard]] dsp::SIDChip& getSidChip() noexcept { return sid_; }
    [[nodiscard]] const dsp::SIDChip& getSidChip() const noexcept { return sid_; }

private:
    dsp::SIDChip sid_;
};

} // namespace eatsbits::audio

#endif // EATS_SID_NODE_HPP
