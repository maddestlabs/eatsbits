#ifndef EATS_TB303_NODE_HPP
#define EATS_TB303_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/tb303_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Roland TB-303 Acid Bassline Source Node.
 * Emits stereo acid bass audio, accepts slides, accents, and cutoff sweeps.
 */
class Tb303Node : public GraphNode {
public:
    explicit Tb303Node(std::string name = "Tb303Acid")
        : GraphNode(std::move(name)) {
        // Source node: 0 input ports, 1 stereo output port
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        tb303_.setSampleRate(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        tb303_.reset();
    }

    void noteOn(uint8_t note, float velocity, bool isSlide = false, bool isAccent = false) noexcept {
        tb303_.noteOn(note, velocity, isSlide, isAccent);
    }

    void noteOff() noexcept {
        tb303_.noteOff();
    }

    void setCutoff(float cutoffHz) noexcept {
        tb303_.setCutoff(cutoffHz);
    }

    void setResonance(float res) noexcept {
        tb303_.setResonance(res);
    }

    void setEnvMod(float mod) noexcept {
        tb303_.setEnvMod(mod);
    }

    void setDecay(float decay) noexcept {
        tb303_.setDecay(decay);
    }

    void setAccent(float accent) noexcept {
        tb303_.setAccent(accent);
    }

    void setWaveform(float wave) noexcept {
        tb303_.setWaveform(wave);
    }

    void setOverdrive(float drive) noexcept {
        tb303_.setOverdrive(drive);
    }

    void handleEvent(const AudioEvent& event) noexcept override {
        switch (event.type) {
            case AudioEventType::NoteOn: {
                // Check slide/accent encoded or from param fields
                bool isSlide = (event.channel & 0x01) != 0;
                bool isAccent = (event.channel & 0x02) != 0;
                noteOn(event.note, event.velocity, isSlide, isAccent);
                break;
            }
            case AudioEventType::NoteOff:
                noteOff();
                break;
            case AudioEventType::AllNotesOff:
                noteOff();
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
            case 0: setCutoff(value); break;
            case 1: setResonance(value); break;
            case 2: setEnvMod(value); break;
            case 3: setDecay(value); break;
            case 4: setAccent(value); break;
            case 5: setWaveform(value); break;
            case 6: setOverdrive(value); break;
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

        for (uint32_t i = 0; i < numFrames; ++i) {
            const float s = tb303_.processSample();
            outL[i] = s;
            outR[i] = s;
        }
    }

    dsp::Tb303Core& getTb303() noexcept { return tb303_; }

private:
    dsp::Tb303Core tb303_;
};

} // namespace eatsbits::audio

#endif // EATS_TB303_NODE_HPP
