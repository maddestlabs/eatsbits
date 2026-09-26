#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

#include "prng.hpp"

namespace eatsbits::audio {

/// Operator Waveforms supported across OPN2 (sine) and extended OPL3 / SFXR modes.
enum class FMWaveform : uint8_t {
    Sine = 0,
    Square,
    Saw,
    Triangle,
    HalfSine,
    AbsSine,
    Noise
};

/// Represents one operator in a 4-operator FM sound chip (YM2612 / OPN2).
class FMOperator {
public:
    float multiplier{1.0f}; // Frequency multiplier (0.5, 1.0..15.0)
    float attack{0.005f};    // Attack time in seconds
    float decay{0.30f};     // Decay time in seconds
    float sustain{0.60f};   // Sustain gain level (0.0 to 1.0)
    float release{0.40f};   // Release time in seconds
    float detune{0.0f};     // Pitch detune in Hz
    FMWaveform waveform{FMWaveform::Sine};

    // DSP state
    float phase{0.0f};
    float lastOutput{0.0f};
    float prevOutput{0.0f};

    FMOperator() noexcept {
        setTotalLevel(0.0f);
    }

    void setTotalLevel(float val) noexcept {
        totalLevel_ = std::clamp(val, 0.0f, 127.0f);
        tlAtten_ = std::pow(10.0f, -(totalLevel_ * 0.75f) / 20.0f);
    }

    [[nodiscard]] float getTotalLevel() const noexcept { return totalLevel_; }
    [[nodiscard]] float getTlAtten() const noexcept { return tlAtten_; }

    [[nodiscard]] float evaluateEnvelope(float time, float duration) const noexcept {
        const float a = std::max(0.0001f, attack);
        const float d = std::max(0.001f, decay);
        const float s = std::clamp(sustain, 0.0f, 1.0f);
        const float r = std::max(0.001f, release);
        const float gate = std::max(a + d, duration);

        float env = 0.0f;
        if (time < a) {
            env = std::clamp(time / a, 0.0f, 1.0f);
        } else if (time < a + d) {
            const float decProg = (time - a) / d;
            env = 1.0f - (decProg * (1.0f - s));
        } else if (time < gate) {
            env = s;
        } else {
            const float relProg = (time - gate) / r;
            env = s * std::max(0.0f, 1.0f - relProg);
        }

        return std::clamp(env * tlAtten_, 0.0f, 1.0f);
    }

    [[nodiscard]] float evaluateWaveform(float ph) const noexcept {
        constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
        const float normPhase = std::fmod(ph, kTwoPi);
        const float posPhase = normPhase < 0.0f ? normPhase + kTwoPi : normPhase;

        switch (waveform) {
            case FMWaveform::Square:
                return std::sin(ph) >= 0.0f ? 1.0f : -1.0f;
            case FMWaveform::Triangle:
                return (2.0f / std::numbers::pi_v<float>) * std::asin(std::clamp(std::sin(ph), -1.0f, 1.0f));
            case FMWaveform::Saw:
                return 2.0f * (posPhase / kTwoPi) - 1.0f;
            case FMWaveform::HalfSine: {
                const float s = std::sin(ph);
                return s > 0.0f ? s : 0.0f;
            }
            case FMWaveform::AbsSine:
                return std::abs(std::sin(ph));
            case FMWaveform::Noise:
            case FMWaveform::Sine:
            default:
                return std::sin(ph);
        }
    }

    void reset() noexcept {
        phase = 0.0f;
        lastOutput = 0.0f;
        prevOutput = 0.0f;
        waveform = FMWaveform::Sine;
    }

private:
    float totalLevel_{0.0f};
    float tlAtten_{1.0f};
};

/// High-accuracy 4-Operator Hardware FM Voice (Modelled after YM2612 & OPN2).
class YM2612Voice {
public:
    std::array<FMOperator, 4> operators;
    int algorithm{4}; // 0..7
    int feedback{4};  // 0..7 (Operator 1 self-feedback)

    float startFreqMult{1.0f};
    float endFreqMult{1.0f};
    float sweepDuration{0.0f};

    bool noiseMode{false};
    float noiseMix{0.0f};

    bool active{false};
    float basePitchHz{440.0f};
    float noteTime{0.0f};
    float noteDuration{0.4f};
    float velocity{0.8f};

    DeterministicPRNG prng;

    explicit YM2612Voice(int alg = 4, int fb = 4, uint32_t seed = 42) noexcept
        : algorithm(std::clamp(alg, 0, 7)), feedback(std::clamp(fb, 0, 7)), prng(seed) {}

    void setSeed(uint32_t s) noexcept {
        prng.seed(s);
    }

    void reset() noexcept {
        active = false;
        noteTime = 0.0f;
        startFreqMult = 1.0f;
        endFreqMult = 1.0f;
        sweepDuration = 0.0f;
        noiseMode = false;
        noiseMix = 0.0f;
        for (auto& op : operators) {
            op.reset();
        }
    }

    void writeRegister(int /*port*/, int reg, int value) noexcept {
        const int v = value & 0xFF;

        if ((reg & 0xF0) == 0xB0) {
            algorithm = v & 0x07;
            feedback = (v >> 3) & 0x07;
            return;
        }

        const int opIndex = (reg >> 2) & 0x03;
        auto& op = operators[opIndex];
        const int regGroup = reg & 0xF0;

        switch (regGroup) {
            case 0x30: { // DT & MULT
                const int multRaw = v & 0x0F;
                op.multiplier = multRaw == 0 ? 0.5f : static_cast<float>(multRaw);
                const int dtRaw = (v >> 4) & 0x07;
                op.detune = static_cast<float>(dtRaw - 3) * 1.5f;
                break;
            }
            case 0x40: // TL
                op.setTotalLevel(static_cast<float>(v & 0x7F));
                break;
            case 0x50: { // AR
                const int arRaw = v & 0x1F;
                op.attack = arRaw == 0 ? 2.0f : std::max(0.001f, static_cast<float>(31 - arRaw) * 0.05f);
                break;
            }
            case 0x60: { // DR
                const int drRaw = v & 0x1F;
                op.decay = drRaw == 0 ? 4.0f : std::max(0.01f, static_cast<float>(31 - drRaw) * 0.1f);
                break;
            }
            case 0x70: { // SR
                const int srRaw = v & 0x1F;
                op.release = srRaw == 0 ? 4.0f : std::max(0.01f, static_cast<float>(31 - srRaw) * 0.15f);
                break;
            }
            case 0x80: { // SL & RR
                const int slRaw = (v >> 4) & 0x0F;
                op.sustain = 1.0f - (static_cast<float>(slRaw) / 15.0f);
                const int rrRaw = v & 0x0F;
                op.release = rrRaw == 0 ? 2.0f : std::max(0.01f, static_cast<float>(15 - rrRaw) * 0.15f);
                break;
            }
            default: break;
        }
    }

    [[nodiscard]] float evaluateSample(float time, float baseFreq, float dt, float duration = 0.4f, int sampleIndex = 0) noexcept {
        if (baseFreq <= 0.0f) return 0.0f;
        constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;

        // 1. Instantaneous pitch sweep
        float currentFreq = baseFreq;
        if (sweepDuration > 0.001f && time < sweepDuration) {
            const float progress = std::clamp(time / sweepDuration, 0.0f, 1.0f);
            const float mult = startFreqMult + (endFreqMult - startFreqMult) * progress;
            currentFreq = baseFreq * mult;
        }

        // 2. Envelopes
        const float env1 = operators[0].evaluateEnvelope(time, duration);
        const float env2 = operators[1].evaluateEnvelope(time, duration);
        const float env3 = operators[2].evaluateEnvelope(time, duration);
        const float env4 = operators[3].evaluateEnvelope(time, duration);

        // Check if voice expired
        if (env1 <= 0.0001f && env2 <= 0.0001f && env3 <= 0.0001f && env4 <= 0.0001f && time > 0.05f) {
            active = false;
            return 0.0f;
        }

        // 3. Op 1 with feedback
        const float op1Freq = (currentFreq + operators[0].detune) * operators[0].multiplier;
        operators[0].phase += (kTwoPi * op1Freq) * dt;
        if (operators[0].phase >= kTwoPi) operators[0].phase -= kTwoPi;

        float fbMod = 0.0f;
        if (feedback > 0) {
            const float fbAmount = std::pow(2.0f, static_cast<float>(feedback - 1)) * 0.5f;
            fbMod = ((operators[0].lastOutput + operators[0].prevOutput) * 0.5f) * fbAmount;
        }

        const float op1Out = operators[0].evaluateWaveform(operators[0].phase + fbMod) * env1;
        operators[0].prevOutput = operators[0].lastOutput;
        operators[0].lastOutput = op1Out;

        // 4. Op 2, 3, 4 phases
        const float op2Freq = (currentFreq + operators[1].detune) * operators[1].multiplier;
        const float op3Freq = (currentFreq + operators[2].detune) * operators[2].multiplier;
        const float op4Freq = (currentFreq + operators[3].detune) * operators[3].multiplier;

        operators[1].phase += (kTwoPi * op2Freq) * dt;
        operators[2].phase += (kTwoPi * op3Freq) * dt;
        operators[3].phase += (kTwoPi * op4Freq) * dt;

        if (operators[1].phase >= kTwoPi) operators[1].phase -= kTwoPi;
        if (operators[2].phase >= kTwoPi) operators[2].phase -= kTwoPi;
        if (operators[3].phase >= kTwoPi) operators[3].phase -= kTwoPi;

        // 5. FM Algorithm Matrix Routing (OPN / OPN2 0..7)
        float output = 0.0f;
        constexpr float kModScale = 4.0f;

        switch (algorithm) {
            case 0: { // Op1 -> Op2 -> Op3 -> Op4 -> Out (Full Serial Stack)
                const float op2 = operators[1].evaluateWaveform(operators[1].phase + op1Out * kModScale) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase + op2 * kModScale) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase + op3 * kModScale) * env4;
                output = op4;
                break;
            }
            case 1: { // (Op1 + Op2) -> Op3 -> Op4 -> Out
                const float op2 = operators[1].evaluateWaveform(operators[1].phase) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase + (op1Out + op2) * kModScale) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase + op3 * kModScale) * env4;
                output = op4;
                break;
            }
            case 2: { // Op1 + (Op2 -> Op3) -> Op4 -> Out
                const float op2 = operators[1].evaluateWaveform(operators[1].phase) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase + op2 * kModScale) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase + (op1Out + op3) * kModScale) * env4;
                output = op4;
                break;
            }
            case 3: { // (Op1 -> Op2) + Op3 -> Op4 -> Out
                const float op2 = operators[1].evaluateWaveform(operators[1].phase + op1Out * kModScale) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase + (op2 + op3) * kModScale) * env4;
                output = op4;
                break;
            }
            case 4: { // (Op1 -> Op2) + (Op3 -> Op4) -> Out (Classic Dual 2-Op)
                const float op2 = operators[1].evaluateWaveform(operators[1].phase + op1Out * kModScale) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase + op3 * kModScale) * env4;
                output = (op2 + op4) * 0.7f;
                break;
            }
            case 5: { // Op1 -> (Op2 + Op3 + Op4) -> Out
                const float op2 = operators[1].evaluateWaveform(operators[1].phase + op1Out * kModScale) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase + op1Out * kModScale) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase + op1Out * kModScale) * env4;
                output = (op2 + op3 + op4) * 0.5f;
                break;
            }
            case 6: { // (Op1 -> Op2) + Op3 + Op4 -> Out
                const float op2 = operators[1].evaluateWaveform(operators[1].phase + op1Out * kModScale) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase) * env4;
                output = (op2 + op3 + op4) * 0.5f;
                break;
            }
            case 7: // Op1 + Op2 + Op3 + Op4 -> Out (Additive 4-Op)
            default: {
                const float op2 = operators[1].evaluateWaveform(operators[1].phase) * env2;
                const float op3 = operators[2].evaluateWaveform(operators[2].phase) * env3;
                const float op4 = operators[3].evaluateWaveform(operators[3].phase) * env4;
                output = (op1Out + op2 + op3 + op4) * 0.35f;
                break;
            }
        }

        // 6. Noise modulation
        if (noiseMode && noiseMix > 0.0f) {
            const float noise = static_cast<float>((sampleIndex * 1664525u + 1013904223u) & 0x7FFFFFFFu) / 2147483647.0f * 2.0f - 1.0f;
            output = output * (1.0f - noiseMix) + noise * env4 * noiseMix;
        }

        return std::clamp(output, -1.0f, 1.0f);
    }
};

/// Procedural SFXR sound generator library built on 4-Op FM hardware.
class SFXRGenerator {
public:
    static void configureLaser(YM2612Voice& voice, DeterministicPRNG* prng = nullptr) noexcept {
        voice.reset();
        voice.algorithm = 4; // Dual 2-op
        voice.feedback = 6;
        voice.startFreqMult = prng ? prng->nextRange(2.0f, 3.2f) : 2.4f;
        voice.endFreqMult = prng ? prng->nextRange(0.1f, 0.3f) : 0.2f;
        voice.sweepDuration = prng ? prng->nextRange(0.12f, 0.24f) : 0.18f;
        voice.noiseMode = false;
        voice.noiseMix = 0.0f;

        voice.operators[0].multiplier = prng ? static_cast<float>(prng->nextInt(2, 4)) : 3.0f;
        voice.operators[0].detune = prng ? prng->nextRange(-2.0f, 2.0f) : 0.0f;
        voice.operators[0].setTotalLevel(8.0f);
        voice.operators[0].attack = 0.001f;
        voice.operators[0].decay = 0.12f;
        voice.operators[0].sustain = 0.0f;

        voice.operators[1].multiplier = 1.0f;
        voice.operators[1].setTotalLevel(0.0f);
        voice.operators[1].attack = 0.001f;
        voice.operators[1].decay = 0.18f;
        voice.operators[1].sustain = 0.0f;

        voice.operators[2].multiplier = prng ? static_cast<float>(prng->nextInt(4, 7)) : 5.0f;
        voice.operators[2].detune = prng ? prng->nextRange(-3.0f, 3.0f) : 0.0f;
        voice.operators[2].setTotalLevel(18.0f);
        voice.operators[2].attack = 0.001f;
        voice.operators[2].decay = 0.08f;
        voice.operators[2].sustain = 0.0f;

        voice.operators[3].multiplier = 2.0f;
        voice.operators[3].setTotalLevel(6.0f);
        voice.operators[3].attack = 0.001f;
        voice.operators[3].decay = 0.15f;
        voice.operators[3].sustain = 0.0f;
    }

    static void configureExplosion(YM2612Voice& voice, DeterministicPRNG* prng = nullptr) noexcept {
        voice.reset();
        voice.algorithm = 0; // Stacked serial
        voice.feedback = 7;
        voice.startFreqMult = prng ? prng->nextRange(1.2f, 1.8f) : 1.5f;
        voice.endFreqMult = prng ? prng->nextRange(0.15f, 0.35f) : 0.25f;
        voice.sweepDuration = prng ? prng->nextRange(0.35f, 0.55f) : 0.45f;
        voice.noiseMode = true;
        voice.noiseMix = prng ? prng->nextRange(0.5f, 0.8f) : 0.65f;

        voice.operators[0].multiplier = 0.5f;
        voice.operators[0].setTotalLevel(0.0f);
        voice.operators[0].attack = 0.005f;
        voice.operators[0].decay = 0.40f;
        voice.operators[0].sustain = 0.10f;

        voice.operators[1].multiplier = 1.0f;
        voice.operators[1].setTotalLevel(10.0f);
        voice.operators[1].attack = 0.01f;
        voice.operators[1].decay = 0.35f;
        voice.operators[1].sustain = 0.0f;

        voice.operators[2].multiplier = 0.5f;
        voice.operators[2].setTotalLevel(5.0f);
        voice.operators[2].attack = 0.01f;
        voice.operators[2].decay = 0.45f;
        voice.operators[2].sustain = 0.0f;

        voice.operators[3].multiplier = 0.5f;
        voice.operators[3].setTotalLevel(0.0f);
        voice.operators[3].attack = 0.005f;
        voice.operators[3].decay = 0.50f;
        voice.operators[3].sustain = 0.0f;
    }

    static void configurePowerup(YM2612Voice& voice, DeterministicPRNG* prng = nullptr) noexcept {
        voice.reset();
        voice.algorithm = 5; // Multi-carrier
        voice.feedback = 3;
        voice.startFreqMult = prng ? prng->nextRange(0.3f, 0.6f) : 0.4f;
        voice.endFreqMult = prng ? prng->nextRange(1.5f, 2.2f) : 1.8f;
        voice.sweepDuration = prng ? prng->nextRange(0.18f, 0.3f) : 0.22f;
        voice.noiseMode = false;
        voice.noiseMix = 0.0f;

        voice.operators[0].multiplier = 1.0f;
        voice.operators[0].setTotalLevel(12.0f);
        voice.operators[0].attack = 0.002f;
        voice.operators[0].decay = 0.20f;
        voice.operators[0].sustain = 0.10f;

        voice.operators[1].multiplier = 2.0f;
        voice.operators[1].setTotalLevel(0.0f);
        voice.operators[1].attack = 0.001f;
        voice.operators[1].decay = 0.25f;
        voice.operators[1].sustain = 0.0f;

        voice.operators[2].multiplier = 3.0f;
        voice.operators[2].setTotalLevel(4.0f);
        voice.operators[2].attack = 0.001f;
        voice.operators[2].decay = 0.20f;
        voice.operators[2].sustain = 0.0f;

        voice.operators[3].multiplier = 4.0f;
        voice.operators[3].setTotalLevel(8.0f);
        voice.operators[3].attack = 0.001f;
        voice.operators[3].decay = 0.15f;
        voice.operators[3].sustain = 0.0f;
    }

    static void configureCoin(YM2612Voice& voice, DeterministicPRNG* prng = nullptr) noexcept {
        voice.reset();
        voice.algorithm = 2;
        voice.feedback = 4;
        voice.startFreqMult = 1.0f;
        voice.endFreqMult = prng ? prng->nextRange(1.3f, 1.6f) : 1.45f;
        voice.sweepDuration = 0.08f;
        voice.noiseMode = false;
        voice.noiseMix = 0.0f;

        voice.operators[0].multiplier = 1.0f;
        voice.operators[0].setTotalLevel(15.0f);
        voice.operators[0].attack = 0.001f;
        voice.operators[0].decay = 0.30f;
        voice.operators[0].sustain = 0.0f;

        voice.operators[1].multiplier = 2.0f;
        voice.operators[1].setTotalLevel(10.0f);
        voice.operators[1].attack = 0.001f;
        voice.operators[1].decay = 0.20f;
        voice.operators[1].sustain = 0.0f;

        voice.operators[2].multiplier = 4.0f;
        voice.operators[2].setTotalLevel(6.0f);
        voice.operators[2].attack = 0.001f;
        voice.operators[2].decay = 0.25f;
        voice.operators[2].sustain = 0.0f;

        voice.operators[3].multiplier = 2.0f;
        voice.operators[3].setTotalLevel(0.0f);
        voice.operators[3].attack = 0.001f;
        voice.operators[3].decay = 0.35f;
        voice.operators[3].sustain = 0.0f;
    }

    static void configureJump(YM2612Voice& voice, DeterministicPRNG* prng = nullptr) noexcept {
        voice.reset();
        voice.algorithm = 1;
        voice.feedback = 2;
        voice.startFreqMult = prng ? prng->nextRange(0.6f, 0.8f) : 0.7f;
        voice.endFreqMult = prng ? prng->nextRange(1.6f, 2.0f) : 1.8f;
        voice.sweepDuration = 0.15f;
        voice.noiseMode = false;
        voice.noiseMix = 0.0f;

        voice.operators[0].multiplier = 1.0f;
        voice.operators[0].setTotalLevel(8.0f);
        voice.operators[0].attack = 0.005f;
        voice.operators[0].decay = 0.15f;
        voice.operators[0].sustain = 0.0f;

        voice.operators[1].multiplier = 1.5f;
        voice.operators[1].setTotalLevel(12.0f);
        voice.operators[1].attack = 0.005f;
        voice.operators[1].decay = 0.15f;
        voice.operators[1].sustain = 0.0f;

        voice.operators[2].multiplier = 1.0f;
        voice.operators[2].setTotalLevel(6.0f);
        voice.operators[2].attack = 0.005f;
        voice.operators[2].decay = 0.18f;
        voice.operators[2].sustain = 0.0f;

        voice.operators[3].multiplier = 1.0f;
        voice.operators[3].setTotalLevel(0.0f);
        voice.operators[3].attack = 0.005f;
        voice.operators[3].decay = 0.20f;
        voice.operators[3].sustain = 0.0f;
    }

    static void configureHit(YM2612Voice& voice, DeterministicPRNG* /*prng*/ = nullptr) noexcept {
        voice.reset();
        voice.algorithm = 3;
        voice.feedback = 5;
        voice.startFreqMult = 1.2f;
        voice.endFreqMult = 0.4f;
        voice.sweepDuration = 0.10f;
        voice.noiseMode = true;
        voice.noiseMix = 0.40f;

        voice.operators[0].multiplier = 0.5f;
        voice.operators[0].setTotalLevel(0.0f);
        voice.operators[0].attack = 0.001f;
        voice.operators[0].decay = 0.12f;
        voice.operators[0].sustain = 0.0f;

        voice.operators[1].multiplier = 2.0f;
        voice.operators[1].setTotalLevel(14.0f);
        voice.operators[1].attack = 0.001f;
        voice.operators[1].decay = 0.10f;
        voice.operators[1].sustain = 0.0f;

        voice.operators[2].multiplier = 3.0f;
        voice.operators[2].setTotalLevel(10.0f);
        voice.operators[2].attack = 0.001f;
        voice.operators[2].decay = 0.08f;
        voice.operators[2].sustain = 0.0f;

        voice.operators[3].multiplier = 1.0f;
        voice.operators[3].setTotalLevel(0.0f);
        voice.operators[3].attack = 0.001f;
        voice.operators[3].decay = 0.14f;
        voice.operators[3].sustain = 0.0f;
    }

    static void configureFromType(YM2612Voice& voice, int sfxType, uint32_t seed = 42) noexcept {
        voice.setSeed(seed);
        auto& prng = voice.prng;
        switch (sfxType) {
            case 0: configureLaser(voice, &prng); break;
            case 1: configureExplosion(voice, &prng); break;
            case 2: configurePowerup(voice, &prng); break;
            case 3: configureCoin(voice, &prng); break;
            case 4: configureJump(voice, &prng); break;
            case 5: configureHit(voice, &prng); break;
            default: break;
        }
    }
};

/// 6-Voice Sega Genesis / Mega Drive YM2612 Sound Engine.
class YM2612Synth {
public:
    std::array<YM2612Voice, 6> voices;
    float masterVolume{0.80f};

    YM2612Synth() noexcept {
        updatePanning();
        configureDefaultPatch();
    }

    void configureDefaultPatch() noexcept {
        // Classic Sega Genesis FM Slap Bass / Brass patch
        for (auto& v : voices) {
            v.algorithm = 4; // Dual 2-op
            v.feedback = 5;

            // Op 1 (Modulator 1)
            v.operators[0].multiplier = 1.0f;
            v.operators[0].setTotalLevel(24.0f);
            v.operators[0].attack = 0.002f;
            v.operators[0].decay = 0.25f;
            v.operators[0].sustain = 0.20f;
            v.operators[0].release = 0.20f;

            // Op 2 (Carrier 1)
            v.operators[1].multiplier = 1.0f;
            v.operators[1].setTotalLevel(0.0f);
            v.operators[1].attack = 0.002f;
            v.operators[1].decay = 0.40f;
            v.operators[1].sustain = 0.40f;
            v.operators[1].release = 0.25f;

            // Op 3 (Modulator 2)
            v.operators[2].multiplier = 2.0f;
            v.operators[2].setTotalLevel(18.0f);
            v.operators[2].attack = 0.001f;
            v.operators[2].decay = 0.15f;
            v.operators[2].sustain = 0.10f;
            v.operators[2].release = 0.15f;

            // Op 4 (Carrier 2)
            v.operators[3].multiplier = 1.0f;
            v.operators[3].setTotalLevel(6.0f);
            v.operators[3].attack = 0.002f;
            v.operators[3].decay = 0.35f;
            v.operators[3].sustain = 0.30f;
            v.operators[3].release = 0.25f;
        }
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = sr > 1000.0f ? sr : 48000.0f;
        dt_ = 1.0f / sampleRate_;
    }

    void noteOn(float freq, float velocity = 0.8f, float duration = 0.5f) noexcept {
        int targetIdx = -1;
        for (int i = 0; i < 6; ++i) {
            if (!voices[i].active) {
                targetIdx = i;
                break;
            }
        }
        if (targetIdx < 0) {
            // Steal round-robin voice
            targetIdx = nextVoiceSteal_;
            nextVoiceSteal_ = (nextVoiceSteal_ + 1) % 6;
        }

        auto& v = voices[targetIdx];
        v.active = true;
        v.basePitchHz = freq;
        v.velocity = velocity;
        v.noteTime = 0.0f;
        v.noteDuration = duration;
        for (auto& op : v.operators) {
            op.phase = 0.0f;
            op.lastOutput = 0.0f;
            op.prevOutput = 0.0f;
        }
    }

    void noteOff(float freq) noexcept {
        for (auto& v : voices) {
            if (v.active && std::abs(v.basePitchHz - freq) < 1.0f) {
                // Shorten note duration to release quickly
                v.noteDuration = v.noteTime;
            }
        }
    }

    void allNotesOff() noexcept {
        for (auto& v : voices) {
            v.active = false;
        }
    }

    void processStereo(float& outL, float& outR) noexcept {
        float mixL = 0.0f;
        float mixR = 0.0f;

        for (int i = 0; i < 6; ++i) {
            auto& v = voices[i];
            if (!v.active) continue;

            v.noteTime += dt_;
            const float sample = v.evaluateSample(v.noteTime, v.basePitchHz, dt_, v.noteDuration, sampleCounter_) * v.velocity;
            mixL += sample * voiceGainL_[i];
            mixR += sample * voiceGainR_[i];
        }

        sampleCounter_++;
        outL = std::clamp(mixL * masterVolume, -1.0f, 1.0f);
        outR = std::clamp(mixR * masterVolume, -1.0f, 1.0f);
    }

private:
    void updatePanning() noexcept {
        for (int i = 0; i < 6; ++i) {
            const float pan = (static_cast<float>(i) - 2.5f) / 3.0f * 0.4f; // -0.4 to +0.4
            const float angle = (pan + 1.0f) * 0.25f * std::numbers::pi_v<float>;
            voiceGainL_[i] = std::cos(angle);
            voiceGainR_[i] = std::sin(angle);
        }
    }

    float sampleRate_{48000.0f};
    float dt_{1.0f / 48000.0f};
    int sampleCounter_{0};
    int nextVoiceSteal_{0};
    std::array<float, 6> voiceGainL_{};
    std::array<float, 6> voiceGainR_{};
};

} // namespace eatsbits::audio
