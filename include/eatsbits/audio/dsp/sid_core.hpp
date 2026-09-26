#ifndef EATS_SID_CORE_HPP
#define EATS_SID_CORE_HPP

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>

namespace eatsbits::dsp {

/**
 * Fast Padé approximant soft-saturation for analog FET overdrive.
 */
inline float fastTanh(float x) noexcept {
    if (std::isnan(x)) return 0.0f;
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

enum class SIDWaveform {
    Pulse = 0,
    Sawtooth = 1,
    Triangle = 2,
    Noise = 3,
    CombinedSawPulse = 4,
    CombinedTriSaw = 5
};

enum class SIDFilterMode {
    Lowpass = 0,
    Bandpass = 1,
    Highpass = 2,
    Notch = 3,
    Off = 4
};

enum class SIDChipModel {
    MOS6581 = 0, // 1982 C64 Breadbin: warm non-linear FET saturation, dark cutoff slope
    MOS8580 = 1  // 1986 C64C / C128: clean, linear cutoff, sharp resonance
};

enum class SIDArpMode {
    Off = 0,
    Hz50 = 1,   // European PAL 50Hz V-Blank (~20.0ms)
    Hz60 = 2,   // North American NTSC 60Hz (~16.6ms)
    Hz100 = 3,  // 2x Multi-speed Tracker (~10.0ms)
    Fast32 = 4, // 1/32 note tempo-synced
    Fast16 = 5  // 1/16 note tempo-synced
};

constexpr std::array<float, 16> kSIDAttackRatesMs = {
    2.0f, 8.0f, 16.0f, 24.0f, 38.0f, 56.0f, 68.0f, 80.0f,
    100.0f, 250.0f, 500.0f, 800.0f, 1000.0f, 3000.0f, 5000.0f, 8000.0f
};

constexpr std::array<float, 16> kSIDDecayReleaseRatesMs = {
    6.0f, 24.0f, 48.0f, 72.0f, 114.0f, 168.0f, 204.0f, 240.0f,
    300.0f, 750.0f, 1500.0f, 2400.0f, 3000.0f, 9000.0f, 15000.0f, 24000.0f
};

/**
 * Authentic MOS 6581 / 8580 23-bit Galois Linear Feedback Shift Register (LFSR).
 * Taps at bit 22 and bit 17 with 8-bit DAC output tapping bits 20, 18, 14, 11, 9, 5, 2, 0.
 */
class SIDNoiseGenerator {
public:
    void reset() noexcept {
        lfsr_ = 0x7FFFF8;
    }

    void clock() noexcept {
        const uint32_t bit22 = (lfsr_ >> 22) & 1;
        const uint32_t bit17 = (lfsr_ >> 17) & 1;
        const uint32_t feedback = bit22 ^ bit17;
        lfsr_ = ((lfsr_ << 1) | feedback) & 0x7FFFFF;
    }

    [[nodiscard]] float getOutput() const noexcept {
        const uint32_t b7 = (lfsr_ >> 20) & 1;
        const uint32_t b6 = (lfsr_ >> 18) & 1;
        const uint32_t b5 = (lfsr_ >> 14) & 1;
        const uint32_t b4 = (lfsr_ >> 11) & 1;
        const uint32_t b3 = (lfsr_ >> 9) & 1;
        const uint32_t b2 = (lfsr_ >> 5) & 1;
        const uint32_t b1 = (lfsr_ >> 2) & 1;
        const uint32_t b0 = lfsr_ & 1;
        const uint32_t out8 = (b7 << 7) | (b6 << 6) | (b5 << 5) | (b4 << 4) | (b3 << 3) | (b2 << 2) | (b1 << 1) | b0;
        return (static_cast<float>(out8) / 127.5f) - 1.0f;
    }

private:
    uint32_t lfsr_{0x7FFFF8};
};

/**
 * Single Voice channel (Voice 1, 2, or 3) on the SID chip.
 */
class SIDVoice {
public:
    SIDWaveform waveform{SIDWaveform::Pulse};

    // Frequency & Pitch
    float baseFreqHz{440.0f};
    float currentFreqHz{440.0f};
    float targetFreqHz{440.0f};
    float detuneHz{0.0f};
    float phase{0.0f};
    float lastOutput{0.0f};

    // 12-bit Pulse Width (0..4095; 2048 = 50% square)
    float pulseWidth{2048.0f};
    float pwmRate{1.6f};
    float pwmRateHz{1.6f};
    float pwmDepth{0.45f};
    float pwmPhase{0.0f};
    float sampleRate_{48000.0f};

    // Routing & Modulation
    bool sync{false};
    bool ringMod{false};
    bool testBit{false};
    bool routeToFilter{true};

    // Glissando / Portamento
    float glideSpeed{0.0f}; // Slide duration in seconds

    // Hardware Chiptune Arpeggiator
    SIDArpMode arpMode{SIDArpMode::Off};
    std::array<int, 4> arpSemitones{0, 3, 7, 12}; // Minor 7th
    size_t arpCount{4};
    float arpSpeedSec{0.020f}; // 50Hz = 20ms
    size_t arpStep{0};
    float arpTimer_{0.0f};

    // Hardware ADSR (0..15 register levels)
    int attackRate{1};
    int decayRate{6};
    int sustainLevel{12};
    int releaseRate{5};

    // Envelope runtime state
    float envLevel_{0.0f};
    float attackTime_{0.008f};
    float decayTime_{0.204f};
    float sustainLevelNorm_{0.80f};
    float releaseTime_{0.168f};
    float timeSinceTrigger_{0.0f};
    bool isNoteActive_{false};

    SIDNoiseGenerator noiseGen;

    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate;
        reset();
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        (void)velocity;
        const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        triggerNote(freq, 0.5f);
    }

    void noteOff(uint8_t note) noexcept {
        (void)note;
        releaseNote();
    }

    [[nodiscard]] bool isActive() const noexcept {
        return isNoteActive_ || envLevel_ > 0.0001f;
    }

    [[nodiscard]] bool justReset() const noexcept {
        return phase < 0.05f;
    }

    float processSample(float syncSample = 0.0f, bool syncReset = false) noexcept {
        (void)syncSample;
        if (syncReset && sync) {
            phase = 0.0f;
        }
        const float dt = 1.0f / sampleRate_;
        return evaluateSample(dt, sampleRate_, nullptr);
    }

    void reset() noexcept {
        phase = 0.0f;
        lastOutput = 0.0f;
        pwmPhase = 0.0f;
        arpTimer_ = 0.0f;
        arpStep = 0;
        envLevel_ = 0.0f;
        timeSinceTrigger_ = 0.0f;
        isNoteActive_ = false;
        noiseGen.reset();
    }

    void triggerNote(float freq, float durationSec = 0.4f, float slideTo = 0.0f) noexcept {
        (void)durationSec;
        baseFreqHz = freq;
        currentFreqHz = freq;
        targetFreqHz = (slideTo > 0.0f) ? slideTo : freq;
        timeSinceTrigger_ = 0.0f;
        isNoteActive_ = true;

        attackTime_ = kSIDAttackRatesMs[std::clamp(attackRate, 0, 15)] / 1000.0f;
        decayTime_ = kSIDDecayReleaseRatesMs[std::clamp(decayRate, 0, 15)] / 1000.0f;
        sustainLevelNorm_ = static_cast<float>(std::clamp(sustainLevel, 0, 15)) / 15.0f;
        releaseTime_ = kSIDDecayReleaseRatesMs[std::clamp(releaseRate, 0, 15)] / 1000.0f;

        switch (arpMode) {
            case SIDArpMode::Hz50: arpSpeedSec = 0.020f; break;
            case SIDArpMode::Hz60: arpSpeedSec = 0.0166f; break;
            case SIDArpMode::Hz100: arpSpeedSec = 0.010f; break;
            case SIDArpMode::Fast32: arpSpeedSec = 0.03125f; break;
            case SIDArpMode::Fast16: arpSpeedSec = 0.0625f; break;
            case SIDArpMode::Off: default: arpSpeedSec = 1.0f; break;
        }

        arpTimer_ = 0.0f;
        arpStep = 0;
    }

    void releaseNote() noexcept {
        isNoteActive_ = false;
    }

    [[nodiscard]] float evaluateEnvelope(float dt) noexcept {
        timeSinceTrigger_ += dt;
        if (isNoteActive_) {
            if (timeSinceTrigger_ < attackTime_) {
                envLevel_ = (attackTime_ > 0.0001f) ? (timeSinceTrigger_ / attackTime_) : 1.0f;
            } else if (timeSinceTrigger_ < attackTime_ + decayTime_) {
                const float decayProg = (timeSinceTrigger_ - attackTime_) / decayTime_;
                envLevel_ = 1.0f - decayProg * (1.0f - sustainLevelNorm_);
            } else {
                envLevel_ = sustainLevelNorm_;
            }
        } else {
            // Releasing
            if (envLevel_ > 0.0001f && releaseTime_ > 0.0001f) {
                const float relStep = dt / releaseTime_;
                envLevel_ = std::max(0.0f, envLevel_ - relStep * sustainLevelNorm_);
            } else {
                envLevel_ = 0.0f;
            }
        }
        return std::clamp(envLevel_, 0.0f, 1.0f);
    }

    float evaluateSample(float dt, float sampleRate, const SIDVoice* syncSource = nullptr) noexcept {
        if (testBit) return 0.0f;

        // 1. Portamento / Glissando Glide
        if (glideSpeed > 0.001f && std::abs(targetFreqHz - currentFreqHz) > 0.1f) {
            const float glideStep = (targetFreqHz - currentFreqHz) * (dt / glideSpeed);
            currentFreqHz += glideStep;
            if (std::abs(targetFreqHz - currentFreqHz) < 0.5f) currentFreqHz = targetFreqHz;
        }

        // 2. Hardware Arpeggiator
        float arpPitchMult = 1.0f;
        if (arpMode != SIDArpMode::Off && arpCount > 0) {
            arpTimer_ += dt;
            if (arpTimer_ >= arpSpeedSec) {
                arpTimer_ -= arpSpeedSec;
                arpStep = (arpStep + 1) % arpCount;
            }
            const int semi = arpSemitones[arpStep];
            arpPitchMult = std::pow(2.0f, static_cast<float>(semi) / 12.0f);
        }

        // 3. Phase Accumulator
        const float effectiveFreq = (currentFreqHz * arpPitchMult) + detuneHz;
        const float phaseInc = effectiveFreq / sampleRate;

        if (sync && syncSource) {
            if (syncSource->phase < phaseInc * 1.5f) {
                phase = 0.0f;
            }
        }

        phase += phaseInc;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            noiseGen.clock();
        }

        // 4. Waveform Calculation
        float rawWave = 0.0f;
        switch (waveform) {
            case SIDWaveform::Triangle: {
                float triPhase = phase;
                if (ringMod && syncSource) {
                    if (syncSource->phase >= 0.5f) {
                        triPhase = 1.0f - triPhase;
                    }
                }
                rawWave = (triPhase < 0.5f) ? (4.0f * triPhase - 1.0f) : (3.0f - 4.0f * triPhase);
                break;
            }
            case SIDWaveform::Sawtooth:
                rawWave = 2.0f * phase - 1.0f;
                break;
            case SIDWaveform::Pulse: {
                pwmPhase += pwmRate / sampleRate;
                if (pwmPhase >= 1.0f) pwmPhase -= 1.0f;
                const float pwmLfo = std::sin(pwmPhase * 6.2831853f);
                const float effPw = std::clamp((static_cast<float>(pulseWidth) / 4095.0f) + pwmLfo * pwmDepth * 0.45f, 0.02f, 0.98f);
                rawWave = (phase < effPw) ? 1.0f : -1.0f;
                break;
            }
            case SIDWaveform::Noise:
                rawWave = noiseGen.getOutput();
                break;
            case SIDWaveform::CombinedSawPulse: {
                const float saw = 2.0f * phase - 1.0f;
                const float pulse = (phase < (static_cast<float>(pulseWidth) / 4095.0f)) ? 1.0f : -1.0f;
                rawWave = (saw + pulse) * 0.5f;
                break;
            }
            case SIDWaveform::CombinedTriSaw: {
                const float tri = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
                const float saw = 2.0f * phase - 1.0f;
                rawWave = (tri + saw) * 0.5f;
                break;
            }
        }

        // 5. Envelope Application
        const float env = evaluateEnvelope(dt);
        lastOutput = rawWave * env;
        return lastOutput;
    }
};

/**
 * 12 dB/Octave Multi-Mode Resonant State-Variable Filter (Chamberlin SVF).
 * Switches between MOS 6581 (non-linear saturation, dark slope) and MOS 8580 (linear clean).
 */
class SIDFilter {
public:
    SIDChipModel chipModel{SIDChipModel::MOS6581};
    SIDFilterMode mode{SIDFilterMode::Lowpass};

    int cutoffReg{1350};    // 0..2047 (11-bit)
    int resonanceReg{9};     // 0..15 (4-bit)
    float sampleRate_{48000.0f};

    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate;
        reset();
    }

    void setCutoff(float cutoffVal) noexcept {
        cutoffReg = static_cast<int>(std::clamp(cutoffVal, 0.0f, 2047.0f));
    }

    void setResonance(float resVal) noexcept {
        resonanceReg = static_cast<int>(std::clamp(resVal, 0.0f, 15.0f));
    }

    void reset() noexcept {
        low_ = 0.0f;
        band_ = 0.0f;
    }

    float process(float inSample, float sampleRate = 0.0f) noexcept {
        if (mode == SIDFilterMode::Off) return inSample;

        const float sr = (sampleRate > 0.0f) ? sampleRate : sampleRate_;

        float cutoffHz;
        if (chipModel == SIDChipModel::MOS6581) {
            cutoffHz = 30.0f + std::pow(std::clamp(cutoffReg, 0, 2047) / 2047.0f, 2.2f) * 9500.0f;
        } else {
            cutoffHz = 30.0f + (std::clamp(cutoffReg, 0, 2047) / 2047.0f) * 12500.0f;
        }

        const float q = 0.707f + (std::clamp(resonanceReg, 0, 15) / 15.0f) * 7.293f;
        const float damping = 1.0f / q;

        // 2x oversampled Chamberlin SVF
        float out = 0.0f;
        const float f = 2.0f * std::sin(3.14159265f * (cutoffHz / (sr * 2.0f)));

        for (int i = 0; i < 2; ++i) {
            float high = inSample - low_ - damping * band_;
            if (chipModel == SIDChipModel::MOS6581) {
                // Non-linear FET saturation
                high = fastTanh(high * 1.25f);
            }
            band_ += f * high;
            low_ += f * band_;

            switch (mode) {
                case SIDFilterMode::Lowpass: out = low_; break;
                case SIDFilterMode::Bandpass: out = band_; break;
                case SIDFilterMode::Highpass: out = high; break;
                case SIDFilterMode::Notch: out = low_ + high; break;
                case SIDFilterMode::Off: default: out = inSample; break;
            }
        }

        return out;
    }

private:
    float low_{0.0f};
    float band_{0.0f};
};

/**
 * Complete Commodore 64 MOS 6581 / 8580 Sound Chip Emulation.
 * Integrates 3 voices, filter, voice allocation, and overdrive stage.
 */
class SIDChip {
public:
    SIDVoice voice1;
    SIDVoice voice2;
    SIDVoice voice3;
    SIDFilter filter;

    float overdrive{1.2f};
    float masterVolume{0.85f};
    float sampleRate_{48000.0f};

    SIDChip() {
        voice1.pulseWidth = 2048.0f;
        voice2.pulseWidth = 2048.0f;
        voice3.pulseWidth = 2048.0f;
    }

    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate;
        voice1.prepare(sampleRate);
        voice2.prepare(sampleRate);
        voice3.prepare(sampleRate);
        filter.prepare(sampleRate);
        reset();
    }

    [[nodiscard]] std::array<SIDVoice, 3> getVoices() const noexcept {
        return {voice1, voice2, voice3};
    }

    void reset() noexcept {
        voice1.reset();
        voice2.reset();
        voice3.reset();
        filter.reset();
        voiceRoundRobin_ = 0;
    }

    void noteOn(uint8_t midiNote, float velocity = 0.85f) noexcept {
        (void)velocity;
        const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
        // Simple round-robin polyphony across the 3 voices
        if (!voice1.isNoteActive_) {
            voice1.triggerNote(freq, 0.5f);
        } else if (!voice2.isNoteActive_) {
            voice2.triggerNote(freq, 0.5f);
        } else if (!voice3.isNoteActive_) {
            voice3.triggerNote(freq, 0.5f);
        } else {
            // Steal oldest voice
            voiceRoundRobin_ = (voiceRoundRobin_ + 1) % 3;
            if (voiceRoundRobin_ == 0) voice1.triggerNote(freq, 0.5f);
            else if (voiceRoundRobin_ == 1) voice2.triggerNote(freq, 0.5f);
            else voice3.triggerNote(freq, 0.5f);
        }
    }

    void noteOff(uint8_t midiNote) noexcept {
        const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
        if (std::abs(voice1.baseFreqHz - freq) < 1.0f) voice1.releaseNote();
        if (std::abs(voice2.baseFreqHz - freq) < 1.0f) voice2.releaseNote();
        if (std::abs(voice3.baseFreqHz - freq) < 1.0f) voice3.releaseNote();
    }

    void setParameter(const std::string& param, float val) noexcept {
        if (param == "Waveform") {
            auto wf = static_cast<SIDWaveform>(std::clamp(static_cast<int>(val), 0, 5));
            voice1.waveform = wf; voice2.waveform = wf; voice3.waveform = wf;
        } else if (param == "PulseWidth") {
            int pw = std::clamp(static_cast<int>(val), 100, 4000);
            voice1.pulseWidth = pw; voice2.pulseWidth = pw; voice3.pulseWidth = pw;
        } else if (param == "PwmRate") {
            voice1.pwmRate = val; voice2.pwmRate = val; voice3.pwmRate = val;
            voice1.pwmRateHz = val; voice2.pwmRateHz = val; voice3.pwmRateHz = val;
        } else if (param == "PwmDepth") {
            voice1.pwmDepth = val; voice2.pwmDepth = val; voice3.pwmDepth = val;
        } else if (param == "ArpMode") {
            auto arp = static_cast<SIDArpMode>(std::clamp(static_cast<int>(val), 0, 5));
            voice1.arpMode = arp; voice2.arpMode = arp; voice3.arpMode = arp;
        } else if (param == "GlideSpeed") {
            voice1.glideSpeed = val; voice2.glideSpeed = val; voice3.glideSpeed = val;
        } else if (param == "ChipModel") {
            filter.chipModel = (val >= 0.5f) ? SIDChipModel::MOS8580 : SIDChipModel::MOS6581;
        } else if (param == "FilterMode") {
            filter.mode = static_cast<SIDFilterMode>(std::clamp(static_cast<int>(val), 0, 4));
        } else if (param == "Cutoff") {
            filter.cutoffReg = std::clamp(static_cast<int>(val), 100, 2047);
        } else if (param == "Resonance") {
            filter.resonanceReg = std::clamp(static_cast<int>(val), 0, 15);
        } else if (param == "Overdrive") {
            overdrive = val;
        } else if (param == "Attack") {
            int a = std::clamp(static_cast<int>(val), 0, 15);
            voice1.attackRate = a; voice2.attackRate = a; voice3.attackRate = a;
        } else if (param == "Decay") {
            int d = std::clamp(static_cast<int>(val), 0, 15);
            voice1.decayRate = d; voice2.decayRate = d; voice3.decayRate = d;
        } else if (param == "Sustain") {
            int s = std::clamp(static_cast<int>(val), 0, 15);
            voice1.sustainLevel = s; voice2.sustainLevel = s; voice3.sustainLevel = s;
        } else if (param == "Release") {
            int r = std::clamp(static_cast<int>(val), 0, 15);
            voice1.releaseRate = r; voice2.releaseRate = r; voice3.releaseRate = r;
        }
    }

    void processStereo(float* outL, float* outR, size_t numFrames, float sampleRate) noexcept {
        const float dt = 1.0f / sampleRate;

        for (size_t i = 0; i < numFrames; ++i) {
            // Voice 1 syncs with Voice 3; Voice 2 syncs with Voice 1; Voice 3 syncs with Voice 2
            const float s1 = voice1.evaluateSample(dt, sampleRate, &voice3);
            const float s2 = voice2.evaluateSample(dt, sampleRate, &voice1);
            const float s3 = voice3.evaluateSample(dt, sampleRate, &voice2);

            float filteredSum = 0.0f;
            float unfilteredSum = 0.0f;

            if (voice1.routeToFilter) filteredSum += s1; else unfilteredSum += s1;
            if (voice2.routeToFilter) filteredSum += s2; else unfilteredSum += s2;
            if (voice3.routeToFilter) filteredSum += s3; else unfilteredSum += s3;

            const float filtered = filter.process(filteredSum, sampleRate);
            float mixed = (filtered + unfilteredSum) * (1.0f / 3.0f);

            // Analog Overdrive saturation
            mixed = fastTanh(mixed * overdrive) * masterVolume;

            outL[i] = mixed;
            outR[i] = mixed;
        }
    }

private:
    size_t voiceRoundRobin_{0};
};

} // namespace eatsbits::dsp

namespace eatsbits::audio::dsp {
    using namespace eatsbits::dsp;
}

#endif // EATS_SID_CORE_HPP
