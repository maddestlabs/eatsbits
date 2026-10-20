#ifndef EATS_DX_NODE_HPP
#define EATS_DX_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/eats_dx_core.hpp"

namespace eatsbits::audio {

/**
 * Modular DXVII 6-Operator FM Synthesizer Node.
 * 32 routing algorithms, operator feedback, 4-stage envelopes, authentic velocity scaling,
 * built-in factory ROM bank, and drag-and-drop SysEx (.syx) cartridge loading.
 */
class EatsDxNode : public GraphNode {
public:
    explicit EatsDxNode(std::string name = "DXVII_Synth")
        : GraphNode(std::move(name)) {
        // Stereo generator node: 0 inputs, 1 stereo output port (2 channels)
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        synth_.prepare(static_cast<float>(sampleRate));
        reset();
    }

    void reset() noexcept override {
        synth_.reset();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        synth_.noteOn(note, velocity);
    }

    void noteOff(uint8_t note) noexcept {
        synth_.noteOff(note);
    }

    void setParameterByName(const std::string& name, float value) noexcept {
        synth_.setParameter(name, value);
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: synth_.setParameter("Algorithm", value); break;
            case 1: synth_.setParameter("Feedback", value); break;
            case 2: synth_.setParameter("MasterVolume", value); break;
            case 3: synth_.setParameter("Brightness", value); break;
            case 4: synth_.setParameter("TineBell", value); break;
            case 5: synth_.setParameter("BodyWarmth", value); break;
            case 6: synth_.setParameter("OpLevel1", value); break;
            case 7: synth_.setParameter("OpLevel2", value); break;
            case 8: synth_.setParameter("OpLevel3", value); break;
            case 9: synth_.setParameter("OpLevel4", value); break;
            case 10: synth_.setParameter("OpLevel5", value); break;
            case 11: synth_.setParameter("OpLevel6", value); break;
            case 12: synth_.setParameter("OpCoarse1", value); break;
            case 13: synth_.setParameter("OpCoarse2", value); break;
            case 14: synth_.setParameter("OpCoarse3", value); break;
            case 15: synth_.setParameter("OpCoarse4", value); break;
            case 16: synth_.setParameter("OpCoarse5", value); break;
            case 17: synth_.setParameter("OpCoarse6", value); break;
            case 18: synth_.loadPatch(static_cast<size_t>(std::max(0.0f, value))); break;
            default: break;
        }
    }

    bool loadPatch(size_t index) noexcept {
        return synth_.loadPatch(index);
    }

    bool loadSysEx(const uint8_t* data, size_t size, std::string name = "CUSTOM BANK") {
        return synth_.loadBank(data, size, std::move(name));
    }

    bool loadBankFromBase64(std::string_view base64OrUri, std::string name = "EMBEDDED BANK") {
        return synth_.loadBankFromBase64(base64OrUri, std::move(name));
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
                synth_.reset();
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

        synth_.processStereo(outL, outR, numFrames);
    }

    [[nodiscard]] dsp::EatsDxSynth& getSynth() noexcept { return synth_; }
    [[nodiscard]] const dsp::EatsDxSynth& getSynth() const noexcept { return synth_; }

    // Legacy backwards compatibility methods
    [[nodiscard]] dsp::EatsDxSynth& getDx7() noexcept { return synth_; }
    [[nodiscard]] const dsp::EatsDxSynth& getDx7() const noexcept { return synth_; }

private:
    dsp::EatsDxSynth synth_;
};

using Dx7Node = EatsDxNode;

} // namespace eatsbits::audio

#endif // EATS_DX_NODE_HPP
