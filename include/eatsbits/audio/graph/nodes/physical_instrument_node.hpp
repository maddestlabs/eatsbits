#ifndef EATS_PHYSICAL_INSTRUMENT_NODE_HPP
#define EATS_PHYSICAL_INSTRUMENT_NODE_HPP

#include <vector>
#include <string>
#include <array>
#include <memory>
#include <cmath>
#include "../graph_node.hpp"
#include "../../dsp/piano_physical_tables.hpp"
#include "../../dsp/exciters.hpp"
#include "../../dsp/modal_resonator.hpp"
#include "../../dsp/waveguide_core.hpp"

namespace eatsbits::audio {

enum class PhysicalModelType {
    ConcertGrandPiano = 0,
    UprightBass = 1,
    SpanishGuitar = 2,
    SteelAcousticGuitar = 3
};

/**
 * High-Performance Polyphonic Physical Modeling Instrument Node.
 * Renders Stanford CCRMA / Bank-Bensa Concert Grand Piano, Upright Double Bass,
 * and Classical Spanish / Steel Plucked Acoustic Guitars with strict zero-allocation real-time safety.
 */
class PhysicalInstrumentNode : public GraphNode {
public:
    static constexpr size_t kMaxVoices = 8;
    static constexpr size_t kMaxFramesPerBlock = 2048;

    explicit PhysicalInstrumentNode(PhysicalModelType modelType = PhysicalModelType::ConcertGrandPiano,
                                   std::string name = "PhysicalInstrument")
        : GraphNode(std::move(name)), modelType_(modelType) {
        addOutputPort(2); // Stereo output (Port 0: Ch 0=L, Ch 1=R)

        // Initialize voice states
        for (size_t v = 0; v < kMaxVoices; ++v) {
            voices_[v].active = false;
            voices_[v].voiceIdx = v;
        }

        // Configure Spanish guitar modal resonator body modes
        dsp::ModalResonatorBank::ModeConfig guitarModes[4] = {
            {1.0f, 0.50f, 16.0f},
            {1.96f, 0.38f, 20.0f},
            {2.45f, 0.22f, 18.0f},
            {3.12f, 0.14f, 26.0f}
        };
        for (auto& v : voices_) {
            v.guitarBody.setModes(std::span<const dsp::ModalResonatorBank::ModeConfig>(guitarModes, 4));
            v.uprightBody.setInstrumentType(dsp::ViolinFamilyBodyResonator::InstrumentType::DoubleBass);
        }
    }

    void setModelType(PhysicalModelType type) noexcept {
        modelType_ = type;
        reset();
    }
    [[nodiscard]] PhysicalModelType getModelType() const noexcept { return modelType_; }

    void prepare(double sampleRate, uint32_t maxBlockSize) override {
        sampleRate_ = sampleRate;
        maxBlockSize_ = std::min(maxBlockSize, static_cast<uint32_t>(kMaxFramesPerBlock));
        reset();
    }

    void reset() noexcept override {
        for (auto& v : voices_) {
            v.active = false;
            v.age = 0;
            v.isNoteOff = false;
            v.pianoWaveguide.reset();
            v.pianoHammer.reset();
            v.pianoComb.reset();
            v.bassWaveguide.reset();
            v.guitarWaveguide.reset();
            v.guitarBody.reset();
            v.uprightBody.reset();
        }
    }

    void noteOn(uint8_t note, float velocity) noexcept {
        if (velocity <= 0.001f) {
            noteOff(note);
            return;
        }

        // Voice allocation: find inactive voice, or steal oldest
        size_t bestVoice = 0;
        uint32_t oldestAge = 0;
        bool foundInactive = false;

        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (!voices_[i].active) {
                bestVoice = i;
                foundInactive = true;
                break;
            }
            if (voices_[i].age > oldestAge) {
                oldestAge = voices_[i].age;
                bestVoice = i;
            }
        }

        auto& v = voices_[bestVoice];
        v.active = true;
        v.note = note;
        v.velocity = std::clamp(velocity, 0.01f, 1.0f);
        v.freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        v.age = 0;
        v.isNoteOff = false;
        v.isExcited = false; // Trigger exciter in processBlock

        v.pianoWaveguide.reset();
        v.pianoHammer.reset();
        v.pianoComb.reset();
        v.bassWaveguide.reset();
        v.guitarWaveguide.reset();
        v.guitarBody.reset();
        v.uprightBody.reset();
    }

    void noteOff(uint8_t note) noexcept {
        for (auto& v : voices_) {
            if (v.active && v.note == note && !v.isNoteOff) {
                v.isNoteOff = true;
            }
        }
    }

    void allNotesOff() noexcept {
        for (auto& v : voices_) {
            v.active = false;
        }
    }

    // Macro parameters
    void setBrightness(float b) noexcept { brightness_ = std::clamp(b, 0.0f, 1.0f); }
    void setSustain(float s) noexcept { sustain_ = std::clamp(s, 0.5f, 1.2f); }
    void setHammerHardness(float h) noexcept { hammerHardness_ = std::clamp(h, 0.1f, 3.0f); }
    void setStrumSpread(float ms) noexcept { strumSpreadMs_ = std::clamp(ms, 1.0f, 40.0f); }

    void setParameter(uint32_t paramId, float value) noexcept override {
        switch (paramId) {
            case 0: setBrightness(value); break;
            case 1: setSustain(value); break;
            case 2: setHammerHardness(value); break;
            case 3: setStrumSpread(value); break;
            default: break;
        }
    }

    void processBlock(uint32_t numFrames) noexcept override {
        float* outL = getOutputBuffer(0, 0);
        float* outR = getOutputBuffer(0, 1);

        if (!outL || !outR) return;

        std::fill_n(outL, numFrames, 0.0f);
        std::fill_n(outR, numFrames, 0.0f);

        if (!enabled_) return;

        const float sr = static_cast<float>(sampleRate_);
        const size_t frames = std::min(static_cast<size_t>(numFrames), kMaxFramesPerBlock);

        for (auto& v : voices_) {
            if (!v.active) continue;

            // 1. Synthesize excitation on note-on trigger
            if (!v.isExcited) {
                v.isExcited = true;
                switch (modelType_) {
                    case PhysicalModelType::ConcertGrandPiano:
                        dsp::CommutedSoundboardExciter::generate(
                            v.voiceBuffer.data(), frames, sr, v.note, v.velocity,
                            hammerHardness_, 0.55f, 1.0f
                        );
                        v.pianoHammer.process(v.voiceBuffer.data(), frames, v.note, v.velocity, brightness_, hammerHardness_);
                        v.pianoComb.process(v.voiceBuffer.data(), frames, sr, v.note, v.freq);
                        break;

                    case PhysicalModelType::UprightBass:
                        dsp::UprightPluckSlapExciter::generate(
                            v.voiceBuffer.data(), frames, sr, v.note, v.velocity,
                            2.2f, 0.35f, 0.70f
                        );
                        break;

                    case PhysicalModelType::SpanishGuitar:
                    case PhysicalModelType::SteelAcousticGuitar:
                        dsp::PlectrumStrumExciter::generate(
                            v.voiceBuffer.data(), frames, sr, v.note, v.velocity,
                            strumSpreadMs_, brightness_ * 2.0f
                        );
                        break;
                }
            } else {
                // Ongoing loop without new excitation
                std::fill(v.voiceBuffer.begin(), v.voiceBuffer.begin() + frames, 0.0f);
            }

            // 2. Waveguide Loop & Resonator Processing
            switch (modelType_) {
                case PhysicalModelType::ConcertGrandPiano:
                    v.pianoWaveguide.process(
                        v.voiceBuffer.data(), frames, sr, v.note, v.freq,
                        1.0f, 1.0f, sustain_, v.isNoteOff
                    );
                    break;

                case PhysicalModelType::UprightBass:
                    v.bassWaveguide.process(
                        v.voiceBuffer.data(), frames, sr, v.freq,
                        sustain_ * 0.995f, 0.28f, 0.22f, 3.2f, 0.40f
                    );
                    // Blend direct string + acoustic cavity resonator
                    v.uprightBody.process(v.voiceBuffer.data(), v.auxBuffer.data(), frames, sr, 0.75f);
                    for (size_t i = 0; i < frames; ++i) {
                        v.voiceBuffer[i] = v.voiceBuffer[i] * 0.45f + v.auxBuffer[i] * 0.55f;
                    }
                    break;

                case PhysicalModelType::SpanishGuitar:
                case PhysicalModelType::SteelAcousticGuitar:
                    v.guitarWaveguide.process(
                        v.voiceBuffer.data(), frames, sr, v.freq,
                        sustain_ * 0.995f, 0.24f
                    );
                    v.guitarBody.process(v.voiceBuffer.data(), v.auxBuffer.data(), frames, sr, v.freq);
                    for (size_t i = 0; i < frames; ++i) {
                        v.voiceBuffer[i] = v.voiceBuffer[i] * 0.75f + v.auxBuffer[i] * 0.40f;
                    }
                    break;
            }

            // 3. Stereo Pan & Accumulate into Out Buffers
            // Slight stereo spread per voice index
            const float pan = 0.5f + (static_cast<float>(v.voiceIdx) - 3.5f) * 0.06f;
            const float gainL = std::cos(pan * 1.5707963f);
            const float gainR = std::sin(pan * 1.5707963f);

            float peak = 0.0f;
            for (size_t i = 0; i < frames; ++i) {
                float s = v.voiceBuffer[i];
                outL[i] += s * gainL;
                outR[i] += s * gainR;
                peak = std::max(peak, std::abs(s));
            }

            v.age += static_cast<uint32_t>(frames);

            // Auto-deactivate voice if silent
            if (v.age > static_cast<uint32_t>(sr * 0.5f) && peak < 0.00005f) {
                v.active = false;
            }
        }
    }

private:
    struct VoiceState {
        size_t voiceIdx = 0;
        bool active = false;
        bool isNoteOff = false;
        bool isExcited = false;
        uint8_t note = 60;
        float velocity = 1.0f;
        float freq = 440.0f;
        uint32_t age = 0;

        // DSP Cores
        dsp::CommutedPianoWaveguideCore pianoWaveguide;
        dsp::CommutedHammerFilterCascade pianoHammer;
        dsp::CommutedStrikeComb pianoComb;

        dsp::UprightBassWaveguideCore bassWaveguide;
        dsp::ViolinFamilyBodyResonator uprightBody;

        dsp::DigitalWaveguideCore guitarWaveguide;
        dsp::ModalResonatorBank guitarBody;

        // Pre-allocated voice buffers (zero dynamic memory allocations during process)
        std::array<float, kMaxFramesPerBlock> voiceBuffer{};
        std::array<float, kMaxFramesPerBlock> auxBuffer{};
    };

    PhysicalModelType modelType_;
    float brightness_ = 0.5f;
    float sustain_ = 1.0f;
    float hammerHardness_ = 1.0f;
    float strumSpreadMs_ = 8.0f;

    std::array<VoiceState, kMaxVoices> voices_{};
};

} // namespace eatsbits::audio

#endif // EATS_PHYSICAL_INSTRUMENT_NODE_HPP
