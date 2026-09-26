#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

#include "prng.hpp"

#include <vector>

namespace eatsbits::audio {

/// Authentic SNES 16-Bit S-DSP Waveform Types (BRR encoded single-cycle & multi-cycle wavetables).
enum class SNESWaveform : uint8_t {
    Sine = 0,
    Square,
    Pulse25,
    Pulse12,
    Sawtooth,
    Triangle,
    Organ,
    Strings,
    Flute,
    SlapBass,
    Chime,
    Noise,
    Count
};

/// S-DSP Envelope Modes
enum class SNESEnvelopeMode : uint8_t {
    Adsr = 0,
    GainDirect,
    GainLinearDecrease,
    GainExpDecrease,
    GainLinearIncrease,
    GainBentIncrease
};

/// 8-Tap FIR Echo & Reverb DSP Unit (Emulates hardware S-DSP Echo).
class SNESEchoUnit {
public:
    static constexpr int kMaxEchoDelaySamples = 131072; // Power of 2 (2^17) ring buffer
    static constexpr int kMaxEchoDelayMask = 131071;

    SNESEchoUnit() {
        bufferL_.resize(kMaxEchoDelaySamples, 0.0f);
        bufferR_.resize(kMaxEchoDelaySamples, 0.0f);
        setDelayMs(120);
        setFIRProfile("surround_reverb");
    }

    void reset() noexcept {
        std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
        std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
        writeIndex_ = 0;
        enabled = false;
        volume = 0.0f;
    }

    void setDelayMs(int val) noexcept {
        delayMs_ = std::clamp(val, 16, 480);
        cachedDelaySamples_ = std::clamp(static_cast<int>((delayMs_ / 1000.0f) * 48000.0f), 64, kMaxEchoDelaySamples - 64);
    }

    [[nodiscard]] int getDelayMs() const noexcept { return delayMs_; }

    void setFIRProfile(std::string_view profileName) noexcept {
        if (profileName == "dark_reverb" || profileName == "dark_hall") {
            firCoefficients_ = {0.50f, 0.35f, 0.15f, 0.05f, -0.02f, 0.02f, -0.01f, 0.01f};
        } else if (profileName == "metallic_chorus" || profileName == "metallic") {
            firCoefficients_ = {0.25f, -0.35f, 0.45f, -0.25f, 0.15f, -0.10f, 0.05f, -0.02f};
        } else if (profileName == "slapback") {
            firCoefficients_ = {1.00f, 0.00f, 0.0f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f};
        } else {
            // surround_reverb default
            firCoefficients_ = {0.34f, 0.45f, -0.12f, 0.10f, -0.05f, 0.08f, -0.04f, 0.02f};
        }
    }

    void processStereo(float leftDry, float rightDry, float& outL, float& outR) noexcept {
        if (!enabled || volume <= 0.001f) {
            outL = leftDry;
            outR = rightDry;
            return;
        }

        const int baseReadIdx = writeIndex_ - cachedDelaySamples_;
        float leftFir = 0.0f;
        float rightFir = 0.0f;

        for (int tap = 0; tap < 8; ++tap) {
            const int tapIndex = (baseReadIdx - (tap << 2)) & kMaxEchoDelayMask;
            const float coeff = firCoefficients_[tap];
            leftFir += bufferL_[tapIndex] * coeff;
            rightFir += bufferR_[tapIndex] * coeff;
        }

        bufferL_[writeIndex_] = std::clamp(leftDry + (leftFir * feedback), -1.5f, 1.5f);
        bufferR_[writeIndex_] = std::clamp(rightDry + (rightFir * feedback), -1.5f, 1.5f);
        writeIndex_ = (writeIndex_ + 1) & kMaxEchoDelayMask;

        outL = std::clamp((leftDry * (1.0f - volume * 0.5f)) + (leftFir * volume), -1.0f, 1.0f);
        outR = std::clamp((rightDry * (1.0f - volume * 0.5f)) + (rightFir * volume), -1.0f, 1.0f);
    }

    bool enabled{true};
    float feedback{0.45f}; // -1.0 to 1.0
    float volume{0.40f};   // 0.0 to 1.0

private:
    std::vector<float> bufferL_{};
    std::vector<float> bufferR_{};
    std::array<float, 8> firCoefficients_{0.34f, 0.45f, -0.12f, 0.10f, -0.05f, 0.08f, -0.04f, 0.02f};
    int writeIndex_{0};
    int delayMs_{120};
    int cachedDelaySamples_{5760};
};

/// Represents a single voice channel (Channel 0..7) in the 16-Bit S-DSP.
class SNESVoice {
public:
    int index{0};

    // Pitch & Sample Playback
    float pitch{1.0f};
    float basePitchHz{440.0f};
    SNESWaveform waveform{SNESWaveform::Square};
    float phase{0.0f};
    float lastOutput{0.0f};

    // Volume & Panning (-1.0 to 1.0)
    float volumeLeft{0.7f};
    float volumeRight{0.7f};

    // Envelope Generator
    SNESEnvelopeMode envMode{SNESEnvelopeMode::Adsr};
    float attack{0.005f};
    float decay{0.25f};
    float sustain{0.4f};
    float release{0.2f};
    float gainLevel{1.0f};

    // Pitch Sweeps & Arpeggios
    float startFreqMult{1.0f};
    float endFreqMult{1.0f};
    float sweepDuration{0.0f};
    std::array<int, 8> arpeggioNotes{};
    int arpeggioCount{0};
    float arpeggioSpeed{0.05f};

    // Vibrato / Pitch LFO
    float vibratoRate{0.0f};
    float vibratoDepth{0.0f};
    float vibratoPhase{0.0f};

    // Noise Mode
    bool noiseEnabled{false};
    int noiseRate{8};
    float noiseMix{0.0f};

    // Cross-Channel Pitch Modulation (PMOD: Voice n-1 modulates Voice n)
    bool pmodEnabled{false};

    // Echo Enable
    bool echoEnabled{true};

    // Voice Enabled / Active
    bool enabled{true};
    bool active{false};
    float noteTime{0.0f};
    float noteDuration{0.4f};
    bool keyOff{false};
    float keyOffTime{0.0f};

    explicit SNESVoice(int idx = 0) noexcept : index(idx) {
        enabled = (idx == 0);
    }

    void reset() noexcept {
        enabled = (index == 0);
        active = false;
        phase = 0.0f;
        lastOutput = 0.0f;
        startFreqMult = 1.0f;
        endFreqMult = 1.0f;
        sweepDuration = 0.0f;
        arpeggioCount = 0;
        vibratoRate = 0.0f;
        vibratoDepth = 0.0f;
        vibratoPhase = 0.0f;
        noiseEnabled = false;
        noiseMix = 0.0f;
        pmodEnabled = false;
        echoEnabled = true;
        waveform = SNESWaveform::Square;
        envMode = SNESEnvelopeMode::Adsr;
        attack = 0.005f;
        decay = 0.25f;
        sustain = 0.4f;
        release = 0.2f;
        gainLevel = 1.0f;
        noteTime = 0.0f;
        keyOff = false;
    }

    [[nodiscard]] float evaluateEnvelope(float time, float duration) const noexcept {
        switch (envMode) {
            case SNESEnvelopeMode::GainDirect:
                return std::clamp(gainLevel, 0.0f, 1.0f);

            case SNESEnvelopeMode::GainLinearDecrease: {
                const float d = std::max(0.01f, decay);
                const float prog = std::clamp(time / d, 0.0f, 1.0f);
                return std::clamp(1.0f - prog, 0.0f, 1.0f);
            }

            case SNESEnvelopeMode::GainExpDecrease: {
                const float d = std::max(0.01f, decay);
                return std::clamp(std::exp(-time / d), 0.0f, 1.0f);
            }

            case SNESEnvelopeMode::GainLinearIncrease: {
                const float a = std::max(0.001f, attack);
                return std::clamp(time / a, 0.0f, 1.0f);
            }

            case SNESEnvelopeMode::GainBentIncrease: {
                const float a = std::max(0.001f, attack);
                const float prog = std::clamp(time / a, 0.0f, 1.0f);
                return prog < 0.75f ? (prog * 0.5f) : (0.375f + (prog - 0.75f) * 2.5f);
            }

            case SNESEnvelopeMode::Adsr:
            default: {
                const float a = std::max(0.0001f, attack);
                const float d = std::max(0.001f, decay);
                const float s = std::clamp(sustain, 0.0f, 1.0f);
                const float r = std::max(0.001f, release);
                const float gate = std::max(a + d, duration);

                if (time < a) {
                    return std::clamp(time / a, 0.0f, 1.0f);
                } else if (time < a + d) {
                    const float decProg = (time - a) / d;
                    return 1.0f - (decProg * (1.0f - s));
                } else if (time < gate) {
                    return s;
                } else {
                    const float relProg = (time - gate) / r;
                    return std::clamp(s * std::max(0.0f, 1.0f - relProg), 0.0f, 1.0f);
                }
            }
        }
    }

    [[nodiscard]] float evaluateWaveform(float ph) const noexcept {
        constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
        constexpr float kInvTwoPi = 1.0f / kTwoPi;

        float normPhase = std::fmod(ph, kTwoPi);
        if (normPhase < 0.0f) normPhase += kTwoPi;
        const float normPos = normPhase * kInvTwoPi; // 0.0 to 1.0

        switch (waveform) {
            case SNESWaveform::Sine:
                return std::sin(normPhase);

            case SNESWaveform::Square: {
                const float sqr = normPos < 0.5f ? 1.0f : -1.0f;
                return gaussianSmooth(sqr, normPos, 0.5f);
            }

            case SNESWaveform::Pulse25: {
                const float sqr = normPos < 0.25f ? 1.0f : -1.0f;
                return gaussianSmooth(sqr, normPos, 0.25f);
            }

            case SNESWaveform::Pulse12: {
                const float sqr = normPos < 0.125f ? 1.0f : -1.0f;
                return gaussianSmooth(sqr, normPos, 0.125f);
            }

            case SNESWaveform::Sawtooth:
                return (2.0f * normPos - 1.0f) * 0.9f;

            case SNESWaveform::Triangle:
                return (2.0f / std::numbers::pi_v<float>) * std::asin(std::clamp(std::sin(normPhase), -1.0f, 1.0f));

            case SNESWaveform::Organ: {
                const float s1 = std::sin(normPhase);
                const float s2 = std::sin(normPhase * 2.0f) * 0.5f;
                const float s4 = std::sin(normPhase * 4.0f) * 0.25f;
                return (s1 + s2 + s4) * 0.57f;
            }

            case SNESWaveform::Strings: {
                const float saw1 = 2.0f * normPos - 1.0f;
                const float saw2 = 2.0f * std::fmod(normPos * 2.0f, 1.0f) - 1.0f;
                return (saw1 * 0.6f + saw2 * 0.4f);
            }

            case SNESWaveform::Flute:
                return std::sin(normPhase) * 0.85f + std::sin(normPhase * 3.0f) * 0.15f;

            case SNESWaveform::SlapBass: {
                const float b1 = std::sin(normPhase);
                const float b2 = std::sin(normPhase * 3.0f) * 0.4f;
                return (b1 + b2) * 0.7f;
            }

            case SNESWaveform::Chime: {
                const float c1 = std::sin(normPhase);
                const float c2 = std::sin(normPhase * 2.76f) * 0.4f;
                const float c3 = std::sin(normPhase * 5.4f) * 0.25f;
                return (c1 + c2 + c3) * 0.6f;
            }

            case SNESWaveform::Noise:
            default:
                return 0.0f;
        }
    }

    [[nodiscard]] static float gaussianSmooth(float rawVal, float normPos, float transitionPoint) noexcept {
        constexpr float kEdgeWidth = 0.03f;
        const float dist1 = std::abs(normPos - 0.0f);
        const float dist2 = std::abs(normPos - transitionPoint);
        const float dist3 = std::abs(normPos - 1.0f);

        if (dist1 < kEdgeWidth || dist2 < kEdgeWidth || dist3 < kEdgeWidth) {
            return rawVal * 0.85f;
        }
        return rawVal;
    }
};

/// Comprehensive SPC700 / S-DSP Sound Chip Engine (SNES Sound Emulation).
class SNESDSPEngine {
public:
    std::array<SNESVoice, 8> voices;
    SNESEchoUnit echo;

    explicit SNESDSPEngine(uint32_t seed = 42) noexcept : prng(seed) {
        for (int i = 0; i < 8; ++i) {
            voices[i] = SNESVoice(i);
        }
    }

    void reset() noexcept {
        for (auto& v : voices) {
            v.reset();
        }
        echo.reset();
        noiseLfsr_ = 0x4000;
        noiseClockCounter_ = 0;
    }

    void setSeed(uint32_t s) noexcept {
        seed_ = s;
        prng.seed(s);
    }

    void writeRegister(int reg, int value) noexcept {
        const int v = value & 0xFF;
        const int voiceIdx = (reg >> 4) & 0x07;
        const int regType = reg & 0x0F;

        if (voiceIdx < 8 && regType < 0x08) {
            auto& voice = voices[voiceIdx];
            switch (regType) {
                case 0x00: // VOL_L
                    voice.volumeLeft = (v >= 128 ? v - 256 : v) / 127.0f;
                    break;
                case 0x01: // VOL_R
                    voice.volumeRight = (v >= 128 ? v - 256 : v) / 127.0f;
                    break;
                case 0x02: // P_LOW
                    voice.pitch = static_cast<float>((static_cast<int>(voice.pitch) & 0x3F00) | v) / 4096.0f;
                    break;
                case 0x03: // P_HIGH
                    voice.pitch = static_cast<float>(((v & 0x3F) << 8) | (static_cast<int>(voice.pitch * 4096.0f) & 0xFF)) / 4096.0f;
                    break;
                case 0x04: // SCRN (Source Number)
                    voice.waveform = static_cast<SNESWaveform>(v % static_cast<int>(SNESWaveform::Count));
                    break;
                case 0x05: // ADSR1
                    voice.attack = std::max(0.001f, (15 - ((v >> 4) & 0x0F)) * 0.03f);
                    voice.decay = std::max(0.01f, (7 - (v & 0x07)) * 0.1f);
                    voice.envMode = (v & 0x80) != 0 ? SNESEnvelopeMode::Adsr : SNESEnvelopeMode::GainDirect;
                    break;
                case 0x06: // ADSR2
                    voice.sustain = ((v >> 5) & 0x07) / 7.0f;
                    voice.release = std::max(0.005f, (31 - (v & 0x1F)) * 0.05f);
                    break;
                case 0x07: // GAIN
                    if ((v & 0x80) == 0) {
                        voice.envMode = SNESEnvelopeMode::GainDirect;
                        voice.gainLevel = (v & 0x7F) / 127.0f;
                    } else {
                        const int mode = (v >> 5) & 0x03;
                        if (mode == 0) voice.envMode = SNESEnvelopeMode::GainLinearDecrease;
                        else if (mode == 1) voice.envMode = SNESEnvelopeMode::GainExpDecrease;
                        else if (mode == 2) voice.envMode = SNESEnvelopeMode::GainLinearIncrease;
                        else if (mode == 3) voice.envMode = SNESEnvelopeMode::GainBentIncrease;
                        voice.decay = std::max(0.01f, (31 - (v & 0x1F)) * 0.08f);
                    }
                    break;
                default: break;
            }
            return;
        }

        switch (reg) {
            case 0x0D: // EFB (Echo Feedback)
                echo.feedback = (v >= 128 ? v - 256 : v) / 128.0f;
                break;
            case 0x2D: // PMOD
                for (int i = 1; i < 8; ++i) {
                    voices[i].pmodEnabled = (v & (1 << i)) != 0;
                }
                break;
            case 0x3D: // NON
                for (int i = 0; i < 8; ++i) {
                    voices[i].noiseEnabled = (v & (1 << i)) != 0;
                }
                break;
            case 0x4D: // EON
                for (int i = 0; i < 8; ++i) {
                    voices[i].echoEnabled = (v & (1 << i)) != 0;
                }
                break;
            case 0x6C: // FLG
                echo.enabled = (v & 0x20) == 0;
                {
                    const int nClock = std::clamp(v & 0x1F, 0, 14);
                    for (auto& voice : voices) {
                        voice.noiseRate = nClock;
                    }
                }
                break;
            case 0x7D: // EDL
                echo.setDelayMs((v & 0x0F) * 16 + 16);
                break;
            default: break;
        }
    }

    [[nodiscard]] float stepNoise(int clockRate) noexcept {
        const int stepInterval = std::max(1, 16 - clockRate);
        noiseClockCounter_++;
        if (noiseClockCounter_ >= stepInterval) {
            noiseClockCounter_ = 0;
            const uint16_t feedbackBit = static_cast<uint16_t>((noiseLfsr_ & 0x01) ^ ((noiseLfsr_ >> 1) & 0x01));
            noiseLfsr_ = static_cast<uint16_t>(((noiseLfsr_ >> 1) | (feedbackBit << 14)) & 0x7FFF);
        }
        return (static_cast<float>(noiseLfsr_) / 16384.0f) - 1.0f;
    }

    void noteOn(int voiceIdx, float freq, float velocity = 0.8f, float duration = 0.4f) noexcept {
        if (voiceIdx < 0 || voiceIdx >= 8) {
            // Allocate lowest active voice
            voiceIdx = 0;
            for (int i = 0; i < 8; ++i) {
                if (!voices[i].active) {
                    voiceIdx = i;
                    break;
                }
            }
        }

        auto& v = voices[voiceIdx];
        v.enabled = true;
        v.active = true;
        v.basePitchHz = freq;
        v.noteTime = 0.0f;
        v.noteDuration = duration;
        v.keyOff = false;
        v.volumeLeft = 0.7f * velocity;
        v.volumeRight = 0.7f * velocity;
    }

    void noteOff(int voiceIdx) noexcept {
        if (voiceIdx >= 0 && voiceIdx < 8) {
            voices[voiceIdx].keyOff = true;
            voices[voiceIdx].keyOffTime = voices[voiceIdx].noteTime;
        }
    }

    void allNotesOff() noexcept {
        for (auto& v : voices) {
            v.active = false;
            v.keyOff = true;
        }
    }

    void processStereo(float sampleRate, float& outL, float& outR) noexcept {
        constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
        const float dt = 1.0f / sampleRate;

        float dryL = 0.0f;
        float dryR = 0.0f;
        float echoL = 0.0f;
        float echoR = 0.0f;
        float prevVoiceOut = 0.0f;

        for (int i = 0; i < 8; ++i) {
            auto& voice = voices[i];
            if (!voice.enabled || !voice.active) {
                prevVoiceOut = 0.0f;
                continue;
            }

            voice.noteTime += dt;
            const float t = voice.noteTime;

            float curFreq = (voice.basePitchHz > 0.0f ? voice.basePitchHz : 440.0f) * voice.pitch;

            // Pitch sweep
            if (voice.sweepDuration > 0.001f && t < voice.sweepDuration) {
                const float prog = std::clamp(t / voice.sweepDuration, 0.0f, 1.0f);
                const float curve = voice.startFreqMult > voice.endFreqMult
                    ? std::pow(1.0f - prog, 2.2f)
                    : std::pow(prog, 1.4f);
                const float mult = voice.endFreqMult + (voice.startFreqMult - voice.endFreqMult) * curve;
                curFreq *= mult;
            }

            // Arpeggio
            if (voice.arpeggioCount > 0) {
                const int arpIdx = static_cast<int>(t / std::max(0.01f, voice.arpeggioSpeed)) % voice.arpeggioCount;
                const int semi = voice.arpeggioNotes[arpIdx];
                curFreq *= std::pow(2.0f, semi / 12.0f);
            }

            // Vibrato
            if (voice.vibratoRate > 0.01f && voice.vibratoDepth > 0.001f) {
                voice.vibratoPhase += (kTwoPi * voice.vibratoRate) * dt;
                if (voice.vibratoPhase >= kTwoPi) voice.vibratoPhase -= kTwoPi;
                curFreq *= std::pow(2.0f, (std::sin(voice.vibratoPhase) * voice.vibratoDepth) / 12.0f);
            }

            // Cross-channel pitch modulation (PMOD)
            if (voice.pmodEnabled && i > 0) {
                curFreq *= (1.0f + (prevVoiceOut * 0.5f));
            }

            // Phase progression
            voice.phase += (kTwoPi * curFreq) * dt;
            if (voice.phase >= kTwoPi) voice.phase -= kTwoPi;

            // Envelope evaluation
            const float env = voice.evaluateEnvelope(t, voice.noteDuration);
            if (env <= 0.0001f && t > voice.attack + voice.decay) {
                voice.active = false;
                prevVoiceOut = 0.0f;
                continue;
            }

            float sample = voice.evaluateWaveform(voice.phase);

            // S-DSP Noise
            if (voice.noiseEnabled) {
                const float noise = stepNoise(voice.noiseRate);
                sample = voice.waveform == SNESWaveform::Noise
                    ? noise
                    : (sample * (1.0f - voice.noiseMix) + noise * voice.noiseMix);
            }

            sample *= env;
            voice.lastOutput = sample;
            prevVoiceOut = sample;

            const float vL = sample * voice.volumeLeft;
            const float vR = sample * voice.volumeRight;

            dryL += vL;
            dryR += vR;

            if (voice.echoEnabled) {
                echoL += vL;
                echoR += vR;
            }
        }

        // Process through 8-tap FIR echo unit
        float wetL = 0.0f;
        float wetR = 0.0f;
        echo.processStereo(echoL, echoR, wetL, wetR);

        outL = std::clamp((dryL + wetL * 0.5f) * masterVolume, -1.0f, 1.0f);
        outR = std::clamp((dryR + wetR * 0.5f) * masterVolume, -1.0f, 1.0f);
    }

    float masterVolume{0.85f};
    DeterministicPRNG prng;

private:
    uint32_t seed_{42};
    uint16_t noiseLfsr_{0x4000};
    int noiseClockCounter_{0};
};

} // namespace eatsbits::audio
