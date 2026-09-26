#ifndef EATS_DX7_NODE_HPP
#define EATS_DX7_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/dx7_core.hpp"

namespace eatsbits::audio {

/**
 * Modular Yamaha DX7 6-Operator FM Synthesizer Node.
 * 32 routing algorithms, operator feedback, 4-stage envelopes, and authentic velocity scaling.
 */
class Dx7Node : public GraphNode {
public:
    explicit Dx7Node(std::string name = "YamahaDx7")
        : GraphNode(std::move(name)) {
        // Stereo generator node: 0 inputs, 1 stereo output port (2 channels)
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        dx7_.prepare(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        dx7_.reset();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        dx7_.noteOn(note, velocity);
    }

    void noteOff(uint8_t note) noexcept {
        dx7_.noteOff(note);
    }

    void setParameterByName(const std::string& name, float value) noexcept {
        dx7_.setParameter(name, value);
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: dx7_.setParameter("Algorithm", value); break;
            case 1: dx7_.setParameter("Feedback", value); break;
            case 2: dx7_.setParameter("MasterVolume", value); break;
            case 3: dx7_.setParameter("Brightness", value); break;
            case 4: dx7_.setParameter("TineBell", value); break;
            case 5: dx7_.setParameter("BodyWarmth", value); break;
            case 6: dx7_.setParameter("OpLevel1", value); break;
            case 7: dx7_.setParameter("OpLevel2", value); break;
            case 8: dx7_.setParameter("OpLevel3", value); break;
            case 9: dx7_.setParameter("OpLevel4", value); break;
            case 10: dx7_.setParameter("OpLevel5", value); break;
            case 11: dx7_.setParameter("OpLevel6", value); break;
            case 12: dx7_.setParameter("OpCoarse1", value); break;
            case 13: dx7_.setParameter("OpCoarse2", value); break;
            case 14: dx7_.setParameter("OpCoarse3", value); break;
            case 15: dx7_.setParameter("OpCoarse4", value); break;
            case 16: dx7_.setParameter("OpCoarse5", value); break;
            case 17: dx7_.setParameter("OpCoarse6", value); break;
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
                dx7_.reset();
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

        dx7_.processStereo(outL, outR, numFrames);
    }

    [[nodiscard]] dsp::DX7Synth& getDx7() noexcept { return dx7_; }
    [[nodiscard]] const dsp::DX7Synth& getDx7() const noexcept { return dx7_; }

private:
    dsp::DX7Synth dx7_;
};

} // namespace eatsbits::audio

#endif // EATS_DX7_NODE_HPP
