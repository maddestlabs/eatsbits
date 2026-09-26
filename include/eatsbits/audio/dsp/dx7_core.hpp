#ifndef EATS_DX7_CORE_HPP
#define EATS_DX7_CORE_HPP

#include <cstdint>
#include <cmath>
#include <array>
#include <algorithm>
#include <string>

namespace eatsbits::dsp {

// ─────────────────────────────────────────────────────────────────────────────
//  YAMAHA DX7 EMPIRICAL HARDWARE LOOKUP TABLES & SCALING
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr float kDX7CoarseMultipliers[32] = {
    0.5f,  1.0f,  2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,
    8.0f,  9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f,
   16.0f, 17.0f, 18.0f, 19.0f, 20.0f, 21.0f, 22.0f, 23.0f,
   24.0f, 25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 30.0f, 31.0f
};

inline constexpr uint8_t kDX7VelocityData[64] = {
    0,  70,  86,  97, 106, 114, 121, 126, 132, 138, 142, 148, 152, 156, 160, 163,
  166, 170, 173, 174, 178, 181, 184, 186, 189, 190, 194, 196, 198, 200, 202, 205,
  206, 209, 211, 214, 216, 218, 220, 222, 224, 225, 227, 229, 230, 232, 233, 235,
  237, 238, 240, 241, 242, 243, 244, 246, 246, 248, 249, 250, 251, 252, 253, 254
};

/// 32 Canonical Yamaha DX7 Operator Routing Algorithms (MSFA Bus Encoding)
/// inbus  = (flags >> 4) & 3  (0 = none, 1 = Bus 1, 2 = Bus 2)
/// outbus = flags & 3         (0 = Master Out, 1 = Bus 1, 2 = Bus 2)
/// add    = (flags & 4) != 0  (true = Add into bus, false = Overwrite)
/// fb     = (flags & 0xC0) == 0xC0 (Self-feedback loop)
inline constexpr uint8_t kDX7Algorithms[32][6] = {
    { 0xc1, 0x11, 0x11, 0x14, 0x01, 0x14 }, // Alg 1:  (6[fb]->5->4->3) + (2->1) -> Out
    { 0x01, 0x11, 0x11, 0x14, 0xc1, 0x14 }, // Alg 2:  (6->5->4->3) + (2[fb]->1) -> Out
    { 0xc1, 0x11, 0x14, 0x01, 0x11, 0x14 }, // Alg 3:  (6[fb]->5->4) + (3->2->1) -> Out
    { 0xc1, 0x11, 0x94, 0x01, 0x11, 0x14 }, // Alg 4:  (6[fb]->5->4) + (3->2->1) -> Out
    { 0xc1, 0x14, 0x01, 0x14, 0x01, 0x14 }, // Alg 5:  Classic 3-Stack E-Piano (6[fb]->5), (4->3), (2->1) -> Out
    { 0xc1, 0x94, 0x01, 0x14, 0x01, 0x14 }, // Alg 6:  (6[fb]->5), (4->3), (2->1) -> Out
    { 0xc1, 0x11, 0x05, 0x14, 0x01, 0x14 }, // Alg 7:  (6[fb]->5), (4), (3->2->1) -> Out
    { 0x01, 0x11, 0xc5, 0x14, 0x01, 0x14 }, // Alg 8:  (6->5), (4[fb]), (3->2->1) -> Out
    { 0x01, 0x11, 0x05, 0x14, 0xc1, 0x14 }, // Alg 9:  (6->5), (4), (3->2[fb]->1) -> Out
    { 0x01, 0x05, 0x14, 0xc1, 0x11, 0x14 }, // Alg 10: (6, 5 -> 4), (3[fb]->2->1) -> Out
    { 0xc1, 0x05, 0x14, 0x01, 0x11, 0x14 }, // Alg 11: (6[fb], 5 -> 4), (3->2->1) -> Out
    { 0x01, 0x05, 0x05, 0x14, 0xc1, 0x14 }, // Alg 12: (6, 5, 4 -> 3), (2[fb]->1) -> Out
    { 0xc1, 0x05, 0x05, 0x14, 0x01, 0x14 }, // Alg 13: (6[fb], 5, 4 -> 3), (2->1) -> Out
    { 0xc1, 0x05, 0x11, 0x14, 0x01, 0x14 }, // Alg 14: (6[fb], 5 -> 4 -> 3), (2->1) -> Out
    { 0x01, 0x05, 0x11, 0x14, 0xc1, 0x14 }, // Alg 15: (6, 5 -> 4 -> 3), (2[fb]->1) -> Out
    { 0xc1, 0x11, 0x02, 0x25, 0x05, 0x14 }, // Alg 16: (6[fb]->5->4), (3->1), (2->1) -> Out
    { 0x01, 0x11, 0x02, 0x25, 0xc5, 0x14 }, // Alg 17: (6->5->4), (3->1), (2[fb]->1) -> Out
    { 0x01, 0x11, 0x11, 0xc5, 0x05, 0x14 }, // Alg 18: (6->5->4->3[fb]), (2->1) -> Out
    { 0xc1, 0x14, 0x14, 0x01, 0x11, 0x14 }, // Alg 19: (6[fb]->5, 4), (3->2->1) -> Out
    { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x14 }, // Alg 20: (6, 5 -> 4), (3[fb]->2, 1) -> Out
    { 0x01, 0x14, 0x14, 0xc1, 0x14, 0x14 }, // Alg 21: (6->5, 4), (3[fb]->2, 1) -> Out
    { 0xc1, 0x14, 0x14, 0x14, 0x01, 0x14 }, // Alg 22: (6[fb]->5, 4, 3), (2->1) -> Out
    { 0xc1, 0x14, 0x14, 0x01, 0x14, 0x04 }, // Alg 23: (6[fb]->5, 4), (3->2), (1) -> Out
    { 0xc1, 0x14, 0x14, 0x14, 0x04, 0x04 }, // Alg 24: (6[fb]->5, 4, 3), (2), (1) -> Out
    { 0xc1, 0x14, 0x14, 0x04, 0x04, 0x04 }, // Alg 25: (6[fb]->5, 4), (3), (2), (1) -> Out
    { 0xc1, 0x05, 0x14, 0x01, 0x14, 0x04 }, // Alg 26: (6[fb], 5 -> 4), (3->2), (1) -> Out
    { 0x01, 0x05, 0x14, 0xc1, 0x14, 0x04 }, // Alg 27: (6, 5 -> 4), (3[fb]->2), (1) -> Out
    { 0x04, 0xc1, 0x11, 0x14, 0x01, 0x14 }, // Alg 28: (6), (5[fb]->4->3), (2->1) -> Out
    { 0xc1, 0x14, 0x01, 0x14, 0x04, 0x04 }, // Alg 29: (6[fb]->5), (4->3), (2), (1) -> Out
    { 0x04, 0xc1, 0x11, 0x14, 0x04, 0x04 }, // Alg 30: (6), (5[fb]->4->3), (2), (1) -> Out
    { 0xc1, 0x14, 0x04, 0x04, 0x04, 0x04 }, // Alg 31: (6[fb]->5), (4), (3), (2), (1) -> Out
    { 0xc4, 0x04, 0x04, 0x04, 0x04, 0x04 }  // Alg 32: 6 Parallel Sine Carriers (6[fb], 5, 4, 3, 2, 1) -> Out
};

inline constexpr float kDX7FeedbackAmounts[8] = {
    0.0f,
    0.19634954f,
    0.39269908f,
    0.78539816f,
    1.57079633f,
    3.14159265f,
    6.28318531f,
    12.56637061f
};

// High-Resolution 4096-Sample Sine Lookup Table
struct SineLUT {
    float table[4096];
    constexpr SineLUT() : table() {
        for (int i = 0; i < 4096; ++i) {
            double quadPhase = (4.0 * i) / 4096.0;
            int quad = static_cast<int>(quadPhase);
            double frac = quadPhase - quad;

            double theta = 0.0;
            if (quad == 0) {
                theta = frac * 1.5707963267948966;
            } else if (quad == 1) {
                theta = (1.0 - frac) * 1.5707963267948966;
            } else if (quad == 2) {
                theta = -frac * 1.5707963267948966;
            } else {
                theta = -(1.0 - frac) * 1.5707963267948966;
            }

            double x2 = theta * theta;
            double term = theta;
            double sum = term;
            term *= -x2 / (2.0 * 3.0);
            sum += term;
            term *= -x2 / (4.0 * 5.0);
            sum += term;
            term *= -x2 / (6.0 * 7.0);
            sum += term;
            term *= -x2 / (8.0 * 9.0);
            sum += term;
            term *= -x2 / (10.0 * 11.0);
            sum += term;
            table[i] = static_cast<float>(sum);
        }
    }
};

inline constexpr SineLUT kDX7SineTable{};

inline float lookupSine(float normalizedPhase) noexcept {
    float p = normalizedPhase - static_cast<int>(normalizedPhase);
    if (p < 0.0f) p += 1.0f;
    float idxF = p * 4096.0f;
    int i0 = static_cast<int>(idxF) & 4095;
    int i1 = (i0 + 1) & 4095;
    float frac = idxF - static_cast<float>(i0);
    return kDX7SineTable.table[i0] + frac * (kDX7SineTable.table[i1] - kDX7SineTable.table[i0]);
}

inline float scaleVelocityToLevel(float velocity, int sensitivity) noexcept {
    int clampedVel = static_cast<int>(std::clamp(velocity, 0.0f, 1.0f) * 127.0f);
    int sens = std::clamp(sensitivity, 0, 7);
    if (sens == 0) return 1.0f;
    int velVal = static_cast<int>(kDX7VelocityData[std::clamp(clampedVel >> 1, 0, 63)]) - 239;
    int scaledVel = ((sens * velVal + 7) >> 3) << 4;
    float attenDb = scaledVel * 0.05f;
    return std::clamp(std::pow(10.0f, attenDb / 20.0f), 0.0f, 1.0f);
}

inline int scaleRateKsr(int midiNote, int rateScaling) noexcept {
    int sens = std::clamp(rateScaling, 0, 7);
    if (sens == 0) return 0;
    int x = std::clamp((midiNote / 3) - 7, 0, 31);
    return (sens * x) >> 3;
}

inline float scaleLevelKls(int midiNote, int breakPt, int leftDepth, int rightDepth, int leftCurve, int rightCurve) noexcept {
    int offset = midiNote - breakPt - 17;
    int depth = (offset >= 0) ? rightDepth : leftDepth;
    int curve = (offset >= 0) ? rightCurve : leftCurve;
    if (depth <= 0) return 1.0f;

    int group = (std::abs(offset) + 1) / 3;
    float deltaDb = 0.0f;
    if (curve == 0 || curve == 3) {
        deltaDb = group * depth * 0.08f;
    } else {
        deltaDb = std::pow(group / 32.0f, 2.0f) * depth * 0.95f;
    }
    if (curve == 0 || curve == 1) {
        return std::clamp(std::pow(10.0f, -deltaDb / 20.0f), 0.0f, 1.0f);
    }
    return std::clamp(std::pow(10.0f, deltaDb / 40.0f), 1.0f, 2.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
//  DX7 OPERATOR MODEL
// ─────────────────────────────────────────────────────────────────────────────

enum class DX7EnvStage {
    Idle,
    Attack,  // R1 -> L1
    Decay1,  // R2 -> L2
    Decay2,  // R3 -> L3 (Sustain hold)
    Release  // R4 -> L4 (Release to 0)
};

class DX7Operator {
public:
    // Tuning & Multiplier
    bool isFixedFrequency{false};
    float fixedFrequencyHz{440.0f};
    float coarse{1.0f};  // 0..31
    float fine{0.0f};    // 0..99
    float multiplier{1.0f};
    float detune{0.0f};  // Detune offset in Hz (-7.0 to +7.0)

    // Output Level & Dynamics
    float outputLevel{99.0f}; // DX7 Output Level (0..99)
    int velocitySensitivity{2}; // 0..7
    int rateScaling{0};         // KSR: 0..7
    int ampModSensitivity{0};   // AMS: 0..3

    // Keyboard Level Scaling (KLS)
    int breakPoint{60};
    int leftDepth{0};
    int rightDepth{0};
    int leftCurve{0};
    int rightCurve{0};

    // 4-Rate 4-Level Envelope Generator
    float r1{99.0f}, l1{99.0f};
    float r2{75.0f}, l2{85.0f};
    float r3{45.0f}, l3{60.0f};
    float r4{55.0f}, l4{0.0f};

    // Runtime DSP State
    float phase{0.0f};
    float phaseInc{0.01f};
    float lastOutput{0.0f};
    float prevOutput{0.0f};

    // Stateful Envelope Runtime
    DX7EnvStage stage{DX7EnvStage::Idle};
    float envLevel{0.0f};
    float targetL1{1.0f};
    float targetL2{0.85f};
    float targetL3{0.60f};
    float targetL4{0.0f};
    float step1{0.01f};
    float step2{0.005f};
    float step3{0.001f};
    float step4{0.002f};
    float cachedScale{1.0f};

    DX7Operator() {
        updateMultiplier();
    }

    void updateMultiplier() noexcept {
        int c = std::clamp(static_cast<int>(std::round(coarse)), 0, 31);
        multiplier = kDX7CoarseMultipliers[c] * (1.0f + (std::clamp(fine, 0.0f, 99.0f) / 100.0f));
    }

    void reset() noexcept {
        phase = 0.0f;
        lastOutput = 0.0f;
        prevOutput = 0.0f;
        envLevel = 0.0f;
        stage = DX7EnvStage::Idle;
    }

    void prepareNote(float velocity, int midiNote, float sampleRate, float baseFreq = 440.0f) noexcept {
        updateMultiplier();
        float opFreq = isFixedFrequency ? (fixedFrequencyHz + detune) : (baseFreq * multiplier + detune);
        phaseInc = opFreq / sampleRate;

        auto rateToStep = [&](float rate) {
            int ksrDelta = scaleRateKsr(midiNote, rateScaling);
            float effRate = std::clamp(rate + ksrDelta, 0.0f, 99.0f);
            float sec = std::clamp(std::pow(10.0f, (99.0f - effRate) / 30.0f - 2.5f), 0.001f, 30.0f);
            return 1.0f / (sec * sampleRate);
        };

        step1 = rateToStep(r1);
        step2 = rateToStep(r2);
        step3 = rateToStep(r3);
        step4 = rateToStep(r4);

        targetL1 = std::clamp(l1 / 99.0f, 0.0f, 1.0f);
        targetL2 = std::clamp(l2 / 99.0f, 0.0f, 1.0f);
        targetL3 = std::clamp(l3 / 99.0f, 0.0f, 1.0f);
        targetL4 = std::clamp(l4 / 99.0f, 0.0f, 1.0f);

        float velScale = scaleVelocityToLevel(velocity, velocitySensitivity);
        float levelAtten = std::pow(10.0f, -((99.0f - std::clamp(outputLevel, 0.0f, 99.0f)) * 0.75f) / 20.0f);
        float klsScale = scaleLevelKls(midiNote, breakPoint, leftDepth, rightDepth, leftCurve, rightCurve);

        cachedScale = levelAtten * velScale * klsScale;
        stage = DX7EnvStage::Attack;
        envLevel = 0.0f;
    }

    void noteOff() noexcept {
        stage = DX7EnvStage::Release;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return stage != DX7EnvStage::Idle && (envLevel > 0.0001f || stage == DX7EnvStage::Attack);
    }

    float evaluateEnvelope() noexcept {
        switch (stage) {
            case DX7EnvStage::Attack:
                envLevel += step1;
                if (envLevel >= targetL1) {
                    envLevel = targetL1;
                    stage = DX7EnvStage::Decay1;
                }
                break;
            case DX7EnvStage::Decay1:
                if (envLevel > targetL2) {
                    envLevel = std::max(targetL2, envLevel - step2);
                } else {
                    envLevel = std::min(targetL2, envLevel + step2);
                }
                if (std::abs(envLevel - targetL2) <= step2) {
                    envLevel = targetL2;
                    stage = DX7EnvStage::Decay2;
                }
                break;
            case DX7EnvStage::Decay2:
                if (envLevel > targetL3) {
                    envLevel = std::max(targetL3, envLevel - step3);
                } else {
                    envLevel = std::min(targetL3, envLevel + step3);
                }
                break;
            case DX7EnvStage::Release:
                if (envLevel > targetL4) {
                    envLevel = std::max(targetL4, envLevel - step4);
                } else {
                    envLevel = std::min(targetL4, envLevel + step4);
                }
                if (envLevel <= 0.0001f) {
                    envLevel = 0.0f;
                    stage = DX7EnvStage::Idle;
                }
                break;
            case DX7EnvStage::Idle:
            default:
                envLevel = 0.0f;
                break;
        }
        return std::clamp(envLevel * cachedScale, 0.0f, 1.0f);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  DX7 POLYPHONIC VOICE & SYNTHESIZER
// ─────────────────────────────────────────────────────────────────────────────

class DX7Voice {
public:
    std::array<DX7Operator, 6> operators;
    int algorithm{5}; // Default: Algorithm 5 (Electric Piano 1)
    int feedback{6};  // Feedback level 0..7

    // Timbre Macros
    float brightness{1.0f};
    float tineBell{0.85f};
    float bodyWarmth{1.0f};

    // State
    float baseFreqHz{440.0f};
    int midiNote_{69};
    bool active_{false};

    void reset() noexcept {
        for (auto& op : operators) op.reset();
        active_ = false;
    }

    void noteOn(uint8_t note, float velocity, float sampleRate) noexcept {
        midiNote_ = note;
        baseFreqHz = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        active_ = true;
        for (auto& op : operators) {
            op.prepareNote(velocity, note, sampleRate, baseFreqHz);
        }
    }

    void noteOff() noexcept {
        for (auto& op : operators) {
            op.noteOff();
        }
    }

    [[nodiscard]] bool isActive() const noexcept {
        if (!active_) return false;
        for (const auto& op : operators) {
            if (op.isActive()) return true;
        }
        return false;
    }

    float processSample(float /*sampleRate*/ = 48000.0f) noexcept {
        if (!active_) return 0.0f;

        const int algIndex = std::clamp(algorithm - 1, 0, 31);
        const auto& algOps = kDX7Algorithms[algIndex];
        const float fbAmount = kDX7FeedbackAmounts[std::clamp(feedback, 0, 7)];

        float bus1 = 0.0f;
        float bus2 = 0.0f;
        float masterOut = 0.0f;

        // Process operators in MSFA order: Op 6 down to Op 1 (index 5 down to 0)
        for (int opIdx = 0; opIdx < 6; ++opIdx) {
            const uint8_t flags = algOps[opIdx];
            const int opNumber = 5 - opIdx; // Operator 5 = Op 6, ..., 0 = Op 1
            auto& op = operators[opNumber];

            float env = op.evaluateEnvelope();
            if (opNumber == 3) env *= tineBell;
            if (opNumber == 0 || opNumber == 4) env *= bodyWarmth;
            if ((flags & 3) != 0) env *= brightness; // Modulator operator

            // Phase Modulation input
            const int inbus = (flags >> 4) & 3;
            const bool hasFeedback = ((flags & 0xC0) == 0xC0);

            float modIn = 0.0f;
            if (hasFeedback && fbAmount > 0.0f) {
                modIn = ((op.lastOutput + op.prevOutput) * 0.5f) * fbAmount;
            } else if (inbus == 1) {
                modIn = bus1 * 0.5f;
            } else if (inbus == 2) {
                modIn = bus2 * 0.5f;
            }

            // Calculate sine with phase modulation
            float opOut = lookupSine(op.phase + modIn) * env;

            if (hasFeedback) {
                op.prevOutput = op.lastOutput;
                op.lastOutput = opOut;
            }

            op.phase += op.phaseInc;
            if (op.phase >= 1.0f) op.phase -= 1.0f;

            // Route to buses
            const int outbus = flags & 3;
            const bool add = ((flags & 4) != 0);

            if (outbus == 0) {
                if (add) masterOut += opOut;
                else masterOut = opOut;
            } else if (outbus == 1) {
                if (add) bus1 += opOut;
                else bus1 = opOut;
            } else if (outbus == 2) {
                if (add) bus2 += opOut;
                else bus2 = opOut;
            }
        }

        return masterOut * 0.45f;
    }
};

/**
 * 8-Voice Polyphonic Yamaha DX7 FM Synthesizer.
 */
class DX7Synth {
public:
    static constexpr size_t kNumVoices = 8;
    std::array<DX7Voice, kNumVoices> voices;

    int algorithm{5};
    int feedback{6};
    float masterVolume{0.85f};
    float sampleRate_{48000.0f};

    DX7Synth() {
        updatePanning();
        configureEPiano1();
    }

    void updatePanning() noexcept {
        for (size_t v = 0; v < kNumVoices; ++v) {
            float pan = -0.3f + 0.6f * (static_cast<float>(v) / static_cast<float>(kNumVoices - 1));
            voiceGainL_[v] = 0.7071f * std::cos((pan + 1.0f) * 0.78539816f);
            voiceGainR_[v] = 0.7071f * std::sin((pan + 1.0f) * 0.78539816f);
        }
    }

    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate;
        updatePanning();
        reset();
    }

    void reset() noexcept {
        for (auto& v : voices) v.reset();
        voiceRoundRobin_ = 0;
    }

    void noteOn(uint8_t note, float velocity = 0.85f) noexcept {
        // Find free voice or steal oldest
        int voiceIdx = -1;
        for (size_t i = 0; i < kNumVoices; ++i) {
            if (!voices[i].isActive()) {
                voiceIdx = static_cast<int>(i);
                break;
            }
        }
        if (voiceIdx < 0) {
            voiceRoundRobin_ = (voiceRoundRobin_ + 1) % kNumVoices;
            voiceIdx = static_cast<int>(voiceRoundRobin_);
        }
        voices[voiceIdx].noteOn(note, velocity, sampleRate_);
    }

    void noteOff(uint8_t note) noexcept {
        for (auto& v : voices) {
            if (v.midiNote_ == note && v.isActive()) {
                v.noteOff();
            }
        }
    }

    void setAlgorithm(int alg) noexcept {
        algorithm = std::clamp(alg, 1, 32);
        for (auto& v : voices) v.algorithm = algorithm;
    }

    void setFeedback(int fb) noexcept {
        feedback = std::clamp(fb, 0, 7);
        for (auto& v : voices) v.feedback = feedback;
    }

    void setParameter(const std::string& param, float val) noexcept {
        if (param == "Algorithm") {
            setAlgorithm(static_cast<int>(val));
        } else if (param == "Feedback") {
            setFeedback(static_cast<int>(val));
        } else if (param == "Volume" || param == "MasterVolume") {
            masterVolume = std::clamp(val, 0.0f, 2.0f);
        } else if (param == "Brightness") {
            for (auto& v : voices) v.brightness = val;
        } else if (param == "TineBell") {
            for (auto& v : voices) v.tineBell = val;
        } else if (param == "BodyWarmth") {
            for (auto& v : voices) v.bodyWarmth = val;
        } else if (param.rfind("OpLevel", 0) == 0 && param.size() >= 8) {
            int opIdx = std::clamp(param[7] - '1', 0, 5);
            for (auto& v : voices) v.operators[opIdx].outputLevel = val;
        } else if (param.rfind("OpCoarse", 0) == 0 && param.size() >= 9) {
            int opIdx = std::clamp(param[8] - '1', 0, 5);
            for (auto& v : voices) {
                v.operators[opIdx].coarse = val;
                v.operators[opIdx].updateMultiplier();
            }
        }
    }

    void processStereo(float* outL, float* outR, size_t numFrames) noexcept {
        for (size_t i = 0; i < numFrames; ++i) {
            float sumL = 0.0f;
            float sumR = 0.0f;

            for (size_t v = 0; v < kNumVoices; ++v) {
                if (voices[v].isActive()) {
                    float s = voices[v].processSample(sampleRate_);
                    sumL += s * voiceGainL_[v];
                    sumR += s * voiceGainR_[v];
                }
            }

            outL[i] = sumL * masterVolume;
            outR[i] = sumR * masterVolume;
        }
    }

    void configureEPiano1() noexcept {
        algorithm = 5;
        feedback = 6;
        for (auto& v : voices) {
            v.algorithm = 5;
            v.feedback = 6;
            // Op 1 (Carrier)
            v.operators[0].coarse = 1.0f; v.operators[0].outputLevel = 98.0f; v.operators[0].velocitySensitivity = 2;
            v.operators[0].r1 = 99.0f; v.operators[0].l1 = 98.0f; v.operators[0].r2 = 42.0f; v.operators[0].l2 = 82.0f;
            v.operators[0].r3 = 24.0f; v.operators[0].l3 = 60.0f; v.operators[0].r4 = 52.0f; v.operators[0].l4 = 0.0f;
            // Op 2 (Modulator of 1)
            v.operators[1].coarse = 1.0f; v.operators[1].outputLevel = 78.0f; v.operators[1].velocitySensitivity = 4;
            v.operators[1].r1 = 95.0f; v.operators[1].l1 = 95.0f; v.operators[1].r2 = 38.0f; v.operators[1].l2 = 65.0f;
            v.operators[1].r3 = 18.0f; v.operators[1].l3 = 0.0f;  v.operators[1].r4 = 55.0f; v.operators[1].l4 = 0.0f;
            // Op 3 (Carrier, Tine)
            v.operators[2].coarse = 1.0f; v.operators[2].outputLevel = 90.0f; v.operators[2].velocitySensitivity = 2;
            v.operators[2].r1 = 99.0f; v.operators[2].l1 = 95.0f; v.operators[2].r2 = 65.0f; v.operators[2].l2 = 60.0f;
            v.operators[2].r3 = 30.0f; v.operators[2].l3 = 35.0f; v.operators[2].r4 = 60.0f; v.operators[2].l4 = 0.0f;
            // Op 4 (Inharmonic Bell Modulator, 14x ratio)
            v.operators[3].coarse = 14.0f; v.operators[3].outputLevel = 84.0f; v.operators[3].velocitySensitivity = 5;
            v.operators[3].r1 = 99.0f; v.operators[3].l1 = 95.0f; v.operators[3].r2 = 78.0f; v.operators[3].l2 = 40.0f;
            v.operators[3].r3 = 45.0f; v.operators[3].l3 = 0.0f;  v.operators[3].r4 = 65.0f; v.operators[3].l4 = 0.0f;
            // Op 5 (Carrier, Air Shimmer)
            v.operators[4].coarse = 1.0f; v.operators[4].detune = 0.08f; v.operators[4].outputLevel = 88.0f; v.operators[4].velocitySensitivity = 3;
            v.operators[4].r1 = 99.0f; v.operators[4].l1 = 90.0f; v.operators[4].r2 = 48.0f; v.operators[4].l2 = 72.0f;
            v.operators[4].r3 = 22.0f; v.operators[4].l3 = 40.0f; v.operators[4].r4 = 50.0f; v.operators[4].l4 = 0.0f;
            // Op 6 (Modulator with feedback)
            v.operators[5].coarse = 1.0f; v.operators[5].detune = -0.06f; v.operators[5].outputLevel = 72.0f; v.operators[5].velocitySensitivity = 4;
            v.operators[5].r1 = 90.0f; v.operators[5].l1 = 92.0f; v.operators[5].r2 = 45.0f; v.operators[5].l2 = 55.0f;
            v.operators[5].r3 = 20.0f; v.operators[5].l3 = 0.0f;  v.operators[5].r4 = 55.0f; v.operators[5].l4 = 0.0f;

            for (auto& op : v.operators) op.updateMultiplier();
        }
    }

private:
    std::array<float, kNumVoices> voiceGainL_{};
    std::array<float, kNumVoices> voiceGainR_{};
    size_t voiceRoundRobin_{0};
};

} // namespace eatsbits::dsp

namespace eatsbits::audio::dsp {
    using namespace eatsbits::dsp;
}

#endif // EATS_DX7_CORE_HPP
