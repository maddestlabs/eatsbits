#ifndef EATS_DRUM_KIT_NODE_HPP
#define EATS_DRUM_KIT_NODE_HPP

#include "../graph_node.hpp"
#include "../../dsp/drum_synths.hpp"

namespace eatsbits::audio {

/**
 * Modular TR-808 & TR-909 Drum Machine Kit Node.
 * Maps standard General MIDI percussion note numbers to authentic analog synthesis models.
 */
class DrumKitNode : public GraphNode {
public:
    explicit DrumKitNode(std::string name = "DrumKit")
        : GraphNode(std::move(name)) {
        // Source node: 0 input ports, 1 stereo output port
        addOutputPort(2);
    }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = maxBlockSize;
        const float sr = static_cast<float>(sampleRate);
        kick808_.prepare(sr);
        kick909_.prepare(sr);
        snare808_.prepare(sr);
        hihat808_.prepare(sr);
        clap808_.prepare(sr);
        cowbell808_.prepare(sr);
        reset();
    }

    void reset() noexcept override {
        kick808_.reset();
        kick909_.reset();
        snare808_.reset();
        hihat808_.reset();
        clap808_.reset();
        cowbell808_.reset();
    }

    void triggerNote(uint8_t note, float velocity) noexcept {
        const float kickTune = 35.0f + tune_ * 50.0f;
        const float kickDecay = 0.15f + decay_ * 1.5f;
        const float snareDecay = 0.1f + decay_ * 0.4f;

        switch (note) {
            case 35: // Acoustic / 808 Bass Drum
                kick808_.trigger(velocity, kickTune, kickDecay, 1.0f, overdrive_);
                break;
            case 36: // Bass Drum 1 / 909 Kick
                kick909_.trigger(velocity, kickTune, 1.0f, kickDecay, overdrive_);
                break;
            case 38: // Acoustic / 808 Snare
            case 40: // Electric Snare
                snare808_.trigger(velocity, 0.65f, 150.0f + tune_ * 100.0f, snareDecay);
                break;
            case 39: // Hand Clap
                clap808_.trigger(velocity);
                break;
            case 42: // Closed Hi-Hat
            case 44: // Pedal Hi-Hat
                hihat808_.choke();
                hihat808_.trigger(velocity, false);
                break;
            case 46: // Open Hi-Hat
                hihat808_.trigger(velocity, true);
                break;
            case 56: // Cowbell
                cowbell808_.trigger(velocity);
                break;
            default:
                if (note < 37) kick808_.trigger(velocity, kickTune, kickDecay, 1.0f, overdrive_);
                else if (note < 42) snare808_.trigger(velocity, 0.65f, 150.0f + tune_ * 100.0f, snareDecay);
                else hihat808_.trigger(velocity, false);
                break;
        }
    }

    void handleEvent(const AudioEvent& event) noexcept override {
        if (event.type == AudioEventType::NoteOn) {
            triggerNote(event.note, event.velocity);
        } else if (event.type == AudioEventType::AllNotesOff) {
            reset();
        } else if (event.type == AudioEventType::SetParameter) {
            setParameter(event.paramId, event.paramValue);
        }
    }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: masterGain_ = std::clamp(value, 0.0f, 2.0f); break;
            case 1: tune_ = std::clamp(value, 0.0f, 1.0f); break;
            case 2: decay_ = std::clamp(value, 0.0f, 1.0f); break;
            case 3: overdrive_ = std::clamp(value, 0.0f, 1.0f); break;
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
            float k808 = kick808_.processSample();
            float k909 = kick909_.processSample();
            float sn = snare808_.processSample();
            float hh = hihat808_.processSample();
            float cl = clap808_.processSample();
            float cb = cowbell808_.processSample();

            // Panning:
            // Kick & Snare: Center
            // Clap & Cowbell: Slight Right
            // HiHat: Slight Left
            float sampleL = (k808 + k909 + sn) + (hh * 0.9f) + (cl * 0.6f) + (cb * 0.6f);
            float sampleR = (k808 + k909 + sn) + (hh * 0.6f) + (cl * 0.9f) + (cb * 0.8f);

            outL[i] = sampleL * masterGain_;
            outR[i] = sampleR * masterGain_;
        }
    }

    // Direct access to sub-engines for fine parameter tweaking
    ::eatsbits::dsp::Analog808Kick& get808Kick() noexcept { return kick808_; }
    ::eatsbits::dsp::Analog909Kick& get909Kick() noexcept { return kick909_; }
    ::eatsbits::dsp::Analog808Snare& get808Snare() noexcept { return snare808_; }
    ::eatsbits::dsp::Analog808HiHat& get808HiHat() noexcept { return hihat808_; }

private:
    ::eatsbits::dsp::Analog808Kick kick808_;
    ::eatsbits::dsp::Analog909Kick kick909_;
    ::eatsbits::dsp::Analog808Snare snare808_;
    ::eatsbits::dsp::Analog808HiHat hihat808_;
    ::eatsbits::dsp::Analog808Clap clap808_;
    ::eatsbits::dsp::Analog808Cowbell cowbell808_;

    float masterGain_{1.0f};
    float tune_{0.5f};
    float decay_{0.5f};
    float overdrive_{0.2f};
};

} // namespace eatsbits::audio

#endif // EATS_DRUM_KIT_NODE_HPP
