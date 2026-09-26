#ifndef EATS_POLY_SYNTH_NODE_HPP
#define EATS_POLY_SYNTH_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/poly_synth.hpp"

namespace eatsbits::audio {

/**
 * Modular Polyphonic Multi-Voice Synthesizer Source Node.
 * Emits stereo audio, handles MIDI events, and offers parameter modulation.
 */
class PolySynthNode : public GraphNode {
public:
    explicit PolySynthNode(std::string name = "PolySynth")
        : GraphNode(std::move(name)) {
        // Source node: 0 input ports, 1 stereo output port
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        synth_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        synth_.allNotesOff();
    }

    void noteOn(uint8_t note, float velocity) noexcept {
        synth_.noteOn(note, velocity);
    }

    void noteOff(uint8_t note) noexcept {
        synth_.noteOff(note);
    }

    void allNotesOff() noexcept {
        synth_.allNotesOff();
    }

    void setWaveform(::eatsbits::dsp::Waveform wave) noexcept {
        synth_.setWaveform(wave);
    }

    void setFilterCutoff(float cutoffHz) noexcept {
        synth_.setCutoff(cutoffHz);
    }

    void setFilterResonance(float q) noexcept {
        synth_.setResonance(q);
    }

    void setAdsr(float a, float d, float s, float r) noexcept {
        synth_.setAdsr(a, d, s, r);
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
                allNotesOff();
                break;
            case AudioEventType::SetParameter:
                setParameter(event.paramId, event.paramValue);
                break;
            default:
                break;
        }
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setFilterCutoff(value); break;
            case 1: setFilterResonance(value); break;
            case 2: setWaveform(static_cast<::eatsbits::dsp::Waveform>(static_cast<int>(value))); break;
            default: break;
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

        synth_.processBlock(outL, outR, numFrames);
    }

    ::eatsbits::dsp::PolySynth<16>& getSynth() noexcept { return synth_; }

private:
    ::eatsbits::dsp::PolySynth<16> synth_;
};

} // namespace eatsbits::audio

#endif // EATS_POLY_SYNTH_NODE_HPP
