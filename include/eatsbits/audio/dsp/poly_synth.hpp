#ifndef EATS_POLY_SYNTH_HPP
#define EATS_POLY_SYNTH_HPP

#include <array>
#include <cstdint>
#include <algorithm>
#include "oscillator.hpp"
#include "adsr.hpp"
#include "biquad.hpp"

namespace eatsbits::dsp {

struct Voice {
    Oscillator osc;
    AdsrEnvelope ampEnv;
    BiquadFilter filter;
    uint8_t note{0};
    float velocity{0.0f};
    uint64_t noteOnTimestamp{0};
    bool active{false};

    void init(float sampleRate) noexcept {
        osc.setSampleRate(sampleRate);
        ampEnv.setSampleRate(sampleRate);
        filter.reset();
        active = false;
        note = 0;
        velocity = 0.0f;
    }
};

/**
 * Polyphonic Multi-Voice Synthesizer Engine.
 * Pre-allocates fixed voices (default 16) with zero runtime allocations.
 */
template <size_t NumVoices = 16>
class PolySynth {
public:
    PolySynth() noexcept = default;

    void setSampleRate(float sampleRate) noexcept {
        sampleRate_ = (sampleRate > 0.0f) ? sampleRate : 48000.0f;
        for (auto& v : voices_) {
            v.init(sampleRate_);
            updateVoiceParameters(v);
        }
    }

    void noteOn(uint8_t note, float velocity) noexcept {
        timestamp_++;

        // 1. Check if same note is already playing (retrigger)
        Voice* target = nullptr;
        for (auto& v : voices_) {
            if (v.active && v.note == note) {
                target = &v;
                break;
            }
        }

        // 2. Find idle voice
        if (!target) {
            for (auto& v : voices_) {
                if (!v.active) {
                    target = &v;
                    break;
                }
            }
        }

        // 3. Voice steal oldest active voice
        if (!target) {
            uint64_t oldest = UINT64_MAX;
            for (auto& v : voices_) {
                if (v.noteOnTimestamp < oldest) {
                    oldest = v.noteOnTimestamp;
                    target = &v;
                }
            }
        }

        if (target) {
            target->note = note;
            target->velocity = std::clamp(velocity, 0.0f, 1.0f);
            target->noteOnTimestamp = timestamp_;
            target->active = true;

            const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
            target->osc.setFrequency(freq);
            target->osc.resetPhase();
            target->ampEnv.reset();
            target->ampEnv.noteOn(target->velocity);
            updateVoiceParameters(*target);
        }
    }

    void noteOff(uint8_t note) noexcept {
        for (auto& v : voices_) {
            if (v.active && v.note == note) {
                v.ampEnv.noteOff();
            }
        }
    }

    void allNotesOff() noexcept {
        for (auto& v : voices_) {
            v.ampEnv.reset();
            v.active = false;
        }
    }

    void setWaveform(Waveform wave) noexcept {
        waveform_ = wave;
        for (auto& v : voices_) {
            v.osc.setWaveform(wave);
        }
    }

    static inline float getVoiceScale(size_t activeCount) noexcept {
        static const auto table = [] {
            std::array<float, NumVoices + 1> t{};
            t[0] = 1.0f;
            for (size_t i = 1; i <= NumVoices; ++i) {
                t[i] = 1.0f / std::sqrt(static_cast<float>(i));
            }
            return t;
        }();
        return (activeCount <= NumVoices) ? table[activeCount] : (1.0f / std::sqrt(static_cast<float>(activeCount)));
    }

    void setCutoff(float cutoffHz) noexcept {
        cutoffHz_ = cutoffHz;
        filterDirty_ = true;
    }

    void setResonance(float q) noexcept {
        q_ = q;
        filterDirty_ = true;
    }

    void setAdsr(float a, float d, float s, float r) noexcept {
        attack_ = a;
        decay_ = d;
        sustain_ = s;
        release_ = r;
        for (auto& v : voices_) {
            v.ampEnv.setParameters(a, d, s, r);
        }
    }

    void applyFilterParamsIfDirty() noexcept {
        if (filterDirty_) {
            for (auto& v : voices_) {
                v.filter.configure(BiquadType::LowPass, cutoffHz_, q_, 0.0f, sampleRate_);
            }
            filterDirty_ = false;
        }
    }

    [[nodiscard]] inline float processSample() noexcept {
        applyFilterParamsIfDirty();

        float mix = 0.0f;
        int activeCount = 0;

        for (auto& v : voices_) {
            if (!v.active) continue;

            const float env = v.ampEnv.process();
            if (!v.ampEnv.isActive()) {
                v.active = false;
                continue;
            }

            const float rawOsc = v.osc.process();
            const float filtered = v.filter.process(rawOsc);
            mix += filtered * env;
            activeCount++;
        }

        // Voice headroom scaling using precomputed lookup table
        if (activeCount > 1) {
            mix *= getVoiceScale(activeCount);
        }

        return std::clamp(mix, -1.0f, 1.0f);
    }

    void processBlock(float* outL, float* outR, size_t count) noexcept {
        applyFilterParamsIfDirty();
        for (size_t i = 0; i < count; ++i) {
            const float s = processSample();
            outL[i] = s;
            outR[i] = s;
        }
    }

    [[nodiscard]] size_t getActiveVoiceCount() const noexcept {
        size_t count = 0;
        for (const auto& v : voices_) {
            if (v.active) count++;
        }
        return count;
    }

private:
    void updateVoiceParameters(Voice& v) noexcept {
        v.osc.setWaveform(waveform_);
        v.ampEnv.setParameters(attack_, decay_, sustain_, release_);
        v.filter.configure(BiquadType::LowPass, cutoffHz_, q_, 0.0f, sampleRate_);
    }

    std::array<Voice, NumVoices> voices_;
    float sampleRate_{48000.0f};
    uint64_t timestamp_{0};

    Waveform waveform_{Waveform::Saw};
    float cutoffHz_{2500.0f};
    float q_{1.0f};
    float attack_{0.01f};
    float decay_{0.1f};
    float sustain_{0.7f};
    float release_{0.3f};
    bool filterDirty_{false};
};

} // namespace eatsbits::dsp

#endif // EATS_POLY_SYNTH_HPP
