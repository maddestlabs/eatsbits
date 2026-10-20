#ifndef EATS_DX_CORE_HPP
#define EATS_DX_CORE_HPP

#include <cstdint>
#include <cmath>
#include <array>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>

namespace eatsbits::dsp {

// ─────────────────────────────────────────────────────────────────────────────
//  DXVII / 6-OPERATOR FM EMPIRICAL HARDWARE LOOKUP TABLES & SCALING
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

/// 32 Canonical 6-Operator Routing Algorithms (MSFA Bus Encoding)
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
//  PATCH ARCHITECTURE & SYSEX PARSER
// ─────────────────────────────────────────────────────────────────────────────

struct EatsDxPatch {
    std::string name{"INIT VOICE"};
    int algorithm{5}; // 1..32
    int feedback{6};  // 0..7
    bool oscSync{true};
    int8_t transpose{0};

    struct OperatorData {
        float r1{99.0f}, r2{75.0f}, r3{45.0f}, r4{55.0f};
        float l1{99.0f}, l2{85.0f}, l3{60.0f}, l4{0.0f};
        int breakPoint{60};
        int leftDepth{0}, rightDepth{0};
        int leftCurve{0}, rightCurve{0};
        float detune{0.0f}; // Detune in Hz (-7..+7)
        int rateScaling{0}; // 0..7
        int ampModSens{0};  // 0..3
        int velocitySens{2}; // 0..7
        float outputLevel{99.0f}; // 0..99
        bool fixedFreq{false};
        float coarse{1.0f}; // 0..31
        float fine{0.0f};   // 0..99
    };
    std::array<OperatorData, 6> operators{}; // Op 0 = Op 1 ... Op 5 = Op 6
};

class EatsDxSysEx {
public:
    static EatsDxPatch unpackPacked128(const uint8_t* data) {
        EatsDxPatch patch;
        if (!data) return patch;

        // Operators stored in reverse order: Op 6 down to Op 1 (indices 5 down to 0)
        for (int i = 0; i < 6; ++i) {
            int opIdx = 5 - i;
            const uint8_t* p = data + (i * 17);
            auto& op = patch.operators[opIdx];

            op.r1 = static_cast<float>(p[0] & 0x7F);
            op.r2 = static_cast<float>(p[1] & 0x7F);
            op.r3 = static_cast<float>(p[2] & 0x7F);
            op.r4 = static_cast<float>(p[3] & 0x7F);

            op.l1 = static_cast<float>(p[4] & 0x7F);
            op.l2 = static_cast<float>(p[5] & 0x7F);
            op.l3 = static_cast<float>(p[6] & 0x7F);
            op.l4 = static_cast<float>(p[7] & 0x7F);

            op.breakPoint = p[8] & 0x7F;
            op.leftDepth  = p[9] & 0x7F;
            op.rightDepth = p[10] & 0x7F;
            op.leftCurve  = p[11] & 0x03;
            op.rightCurve = (p[11] >> 2) & 0x03;

            int rawDetune = p[12] & 0x07;
            op.detune = static_cast<float>(rawDetune - 7) * 0.1f;
            op.rateScaling = (p[12] >> 3) & 0x07;

            op.ampModSens = p[13] & 0x03;
            op.velocitySens = (p[13] >> 2) & 0x07;

            op.outputLevel = static_cast<float>(p[14] & 0x7F);
            op.fixedFreq = ((p[15] & 0x01) != 0);
            op.coarse = static_cast<float>((p[15] >> 1) & 0x1F);
            op.fine = static_cast<float>(p[16] & 0x7F);
        }

        // Global Parameters
        patch.algorithm = std::clamp((data[110] & 0x1F) + 1, 1, 32);
        patch.feedback = data[111] & 0x07;
        patch.oscSync  = ((data[111] >> 3) & 0x01) != 0;
        patch.transpose = static_cast<int8_t>(static_cast<int>(data[117] & 0x7F) - 24);

        // 10-Character Voice Name
        char nameBuf[11] = {};
        for (int c = 0; c < 10; ++c) {
            char ch = static_cast<char>(data[118 + c] & 0x7F);
            nameBuf[c] = (ch >= 32 && ch <= 126) ? ch : ' ';
        }
        nameBuf[10] = '\0';
        std::string rawName(nameBuf);
        while (!rawName.empty() && rawName.back() == ' ') rawName.pop_back();
        patch.name = rawName.empty() ? "INIT VOICE" : rawName;

        return patch;
    }

    static EatsDxPatch unpackUnpacked155(const uint8_t* data) {
        EatsDxPatch patch;
        if (!data) return patch;

        for (int i = 0; i < 6; ++i) {
            int opIdx = 5 - i;
            const uint8_t* p = data + (i * 21);
            auto& op = patch.operators[opIdx];

            op.r1 = static_cast<float>(p[0] & 0x7F);
            op.r2 = static_cast<float>(p[1] & 0x7F);
            op.r3 = static_cast<float>(p[2] & 0x7F);
            op.r4 = static_cast<float>(p[3] & 0x7F);
            op.l1 = static_cast<float>(p[4] & 0x7F);
            op.l2 = static_cast<float>(p[5] & 0x7F);
            op.l3 = static_cast<float>(p[6] & 0x7F);
            op.l4 = static_cast<float>(p[7] & 0x7F);

            op.breakPoint = p[8] & 0x7F;
            op.leftDepth = p[9] & 0x7F;
            op.rightDepth = p[10] & 0x7F;
            op.leftCurve = p[11] & 0x03;
            op.rightCurve = p[12] & 0x03;
            op.rateScaling = p[13] & 0x07;
            op.ampModSens = p[14] & 0x03;
            op.velocitySens = p[15] & 0x07;
            op.outputLevel = static_cast<float>(p[16] & 0x7F);
            op.fixedFreq = (p[17] != 0);
            op.coarse = static_cast<float>(p[18] & 0x1F);
            op.fine = static_cast<float>(p[19] & 0x7F);
            op.detune = static_cast<float>(static_cast<int>(p[20] & 0x0F) - 7) * 0.1f;
        }

        patch.algorithm = std::clamp((data[134] & 0x1F) + 1, 1, 32);
        patch.feedback = data[135] & 0x07;
        patch.oscSync = (data[136] != 0);
        patch.transpose = static_cast<int8_t>(static_cast<int>(data[144] & 0x7F) - 24);

        char nameBuf[11] = {};
        for (int c = 0; c < 10; ++c) {
            char ch = static_cast<char>(data[145 + c] & 0x7F);
            nameBuf[c] = (ch >= 32 && ch <= 126) ? ch : ' ';
        }
        nameBuf[10] = '\0';
        std::string rawName(nameBuf);
        while (!rawName.empty() && rawName.back() == ' ') rawName.pop_back();
        patch.name = rawName.empty() ? "INIT VOICE" : rawName;

        return patch;
    }

    static std::vector<EatsDxPatch> parseBank(const uint8_t* data, size_t size) {
        std::vector<EatsDxPatch> bank;
        if (!data || size < 128) return bank;

        // 4104-byte Bank Dump (Standard SysEx with F0 43 ... F7)
        if (size >= 4104 && data[0] == 0xF0 && data[1] == 0x43) {
            const uint8_t* voiceStart = data + 6;
            bank.reserve(32);
            for (int v = 0; v < 32; ++v) {
                bank.push_back(unpackPacked128(voiceStart + (v * 128)));
            }
            return bank;
        }

        // 4096-byte Bank Dump (Raw 32 voices packed)
        if (size >= 4096) {
            bank.reserve(32);
            for (int v = 0; v < 32; ++v) {
                bank.push_back(unpackPacked128(data + (v * 128)));
            }
            return bank;
        }

        // 163-byte Single Voice with SysEx header
        if (size >= 163 && data[0] == 0xF0 && data[1] == 0x43) {
            bank.push_back(unpackUnpacked155(data + 6));
            return bank;
        }

        // 155-byte Unpacked Single Voice
        if (size >= 155 && size < 4096) {
            bank.push_back(unpackUnpacked155(data));
            return bank;
        }

        // 128-byte Packed Single Voice (128..154 bytes)
        if (size >= 128 && size < 155) {
            bank.push_back(unpackPacked128(data));
            return bank;
        }

        return bank;
    }

    static std::vector<uint8_t> decodeBase64Payload(std::string_view input) {
        std::vector<uint8_t> out;
        if (input.empty()) return out;

        size_t comma = input.find(',');
        if (comma != std::string_view::npos && input.find("base64") != std::string_view::npos) {
            input = input.substr(comma + 1);
        }

        static const int8_t b64Lut[256] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63,
            52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
            15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
            41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
        };

        uint32_t buf = 0;
        int bits = 0;
        for (char c : input) {
            uint8_t uc = static_cast<uint8_t>(c);
            int8_t val = b64Lut[uc];
            if (val < 0) continue;
            buf = (buf << 6) | static_cast<uint32_t>(val);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
            }
        }
        return out;
    }

    static std::vector<EatsDxPatch> parseBase64Bank(std::string_view base64OrUri) {
        auto bytes = decodeBase64Payload(base64OrUri);
        return parseBank(bytes.data(), bytes.size());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  DXVII OPERATOR MODEL
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
    float outputLevel{99.0f}; // Output Level (0..99)
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

using EatsDxOperator = DX7Operator;

// ─────────────────────────────────────────────────────────────────────────────
//  DXVII POLYPHONIC VOICE & SYNTHESIZER
// ─────────────────────────────────────────────────────────────────────────────

class DX7Voice {
public:
    std::array<DX7Operator, 6> operators;
    int algorithm{5}; // Default: Algorithm 5 (Electric Piano 1)
    int feedback{6};  // Feedback level 0..7

    // Timbre & Expressive Macros
    float brightness{1.0f};
    float tineBell{1.0f};
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
            // Apply macros if tweaked from nominal 1.0f
            if (opNumber == 3 && tineBell != 1.0f) env *= tineBell;
            if ((opNumber == 0 || opNumber == 4) && bodyWarmth != 1.0f) env *= bodyWarmth;
            if ((flags & 3) != 0 && brightness != 1.0f) env *= brightness; // Modulator operator

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

using EatsDxVoice = DX7Voice;

/**
 * 8-Voice Polyphonic DXVII / 6-Operator FM Synthesizer with Cartridge / SysEx Support.
 */
class DX7Synth {
public:
    static constexpr size_t kNumVoices = 8;
    std::array<DX7Voice, kNumVoices> voices;

    int algorithm{5};
    int feedback{6};
    float masterVolume{0.85f};
    float sampleRate_{48000.0f};

    std::string cartridgeName_{"FACTORY ROM"};
    size_t activePatchIndex_{0};
    std::vector<EatsDxPatch> cartridgePatches_;

    DX7Synth() {
        updatePanning();
        initFactoryBank();
        loadPatch(0);
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
        } else if (param == "Patch") {
            loadPatch(static_cast<size_t>(std::max(0.0f, val)));
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
        std::fill_n(outL, numFrames, 0.0f);
        std::fill_n(outR, numFrames, 0.0f);

        for (size_t v = 0; v < kNumVoices; ++v) {
            if (voices[v].isActive()) {
                const float gL = voiceGainL_[v] * masterVolume;
                const float gR = voiceGainR_[v] * masterVolume;
                for (size_t i = 0; i < numFrames; ++i) {
                    float s = voices[v].processSample(sampleRate_);
                    outL[i] += s * gL;
                    outR[i] += s * gR;
                }
            }
        }
    }

    // ── Cartridge & Patch Management ──────────────────────────────────────────

    void applyPatch(const EatsDxPatch& patch) noexcept {
        algorithm = patch.algorithm;
        feedback = patch.feedback;
        for (auto& v : voices) {
            v.algorithm = patch.algorithm;
            v.feedback = patch.feedback;
            for (size_t opIdx = 0; opIdx < 6; ++opIdx) {
                const auto& src = patch.operators[opIdx];
                auto& dst = v.operators[opIdx];

                dst.coarse = src.coarse;
                dst.fine = src.fine;
                dst.detune = src.detune;
                dst.isFixedFrequency = src.fixedFreq;
                dst.outputLevel = src.outputLevel;
                dst.velocitySensitivity = src.velocitySens;
                dst.rateScaling = src.rateScaling;
                dst.ampModSensitivity = src.ampModSens;
                dst.breakPoint = src.breakPoint;
                dst.leftDepth = src.leftDepth;
                dst.rightDepth = src.rightDepth;
                dst.leftCurve = src.leftCurve;
                dst.rightCurve = src.rightCurve;

                dst.r1 = src.r1; dst.l1 = src.l1;
                dst.r2 = src.r2; dst.l2 = src.l2;
                dst.r3 = src.r3; dst.l3 = src.l3;
                dst.r4 = src.r4; dst.l4 = src.l4;
                dst.updateMultiplier();
            }
        }
    }

    bool loadPatch(size_t index) noexcept {
        if (cartridgePatches_.empty()) return false;
        activePatchIndex_ = index % cartridgePatches_.size();
        applyPatch(cartridgePatches_[activePatchIndex_]);
        return true;
    }

    bool loadBank(const uint8_t* data, size_t size, std::string name = "CUSTOM BANK") {
        auto patches = EatsDxSysEx::parseBank(data, size);
        if (patches.empty()) return false;
        cartridgePatches_ = std::move(patches);
        cartridgeName_ = std::move(name);
        loadPatch(0);
        return true;
    }

    bool loadBankFromBase64(std::string_view base64OrUri, std::string name = "EMBEDDED BANK") {
        auto patches = EatsDxSysEx::parseBase64Bank(base64OrUri);
        if (patches.empty()) return false;
        cartridgePatches_ = std::move(patches);
        cartridgeName_ = std::move(name);
        loadPatch(0);
        return true;
    }

    [[nodiscard]] const std::string& getCartridgeName() const noexcept { return cartridgeName_; }
    [[nodiscard]] size_t getActivePatchIndex() const noexcept { return activePatchIndex_; }
    [[nodiscard]] size_t getNumPatches() const noexcept { return cartridgePatches_.size(); }

    [[nodiscard]] std::string getPatchName(size_t index) const {
        if (index < cartridgePatches_.size()) return cartridgePatches_[index].name;
        return "EMPTY";
    }

    [[nodiscard]] std::string getActivePatchName() const {
        return getPatchName(activePatchIndex_);
    }

    // Classic 1983 E.Piano 1 preset configuration
    void configureEPiano1() noexcept {
        if (!cartridgePatches_.empty()) {
            loadPatch(0);
        } else {
            algorithm = 5;
            feedback = 6;
            for (auto& v : voices) {
                v.algorithm = 5;
                v.feedback = 6;
                v.operators[0].coarse = 1.0f; v.operators[0].outputLevel = 98.0f; v.operators[0].velocitySensitivity = 2;
                v.operators[0].r1 = 99.0f; v.operators[0].l1 = 98.0f; v.operators[0].r2 = 42.0f; v.operators[0].l2 = 82.0f;
                v.operators[0].r3 = 24.0f; v.operators[0].l3 = 60.0f; v.operators[0].r4 = 52.0f; v.operators[0].l4 = 0.0f;

                v.operators[1].coarse = 1.0f; v.operators[1].outputLevel = 78.0f; v.operators[1].velocitySensitivity = 4;
                v.operators[1].r1 = 95.0f; v.operators[1].l1 = 95.0f; v.operators[1].r2 = 38.0f; v.operators[1].l2 = 65.0f;
                v.operators[1].r3 = 18.0f; v.operators[1].l3 = 0.0f;  v.operators[1].r4 = 55.0f; v.operators[1].l4 = 0.0f;

                v.operators[2].coarse = 1.0f; v.operators[2].outputLevel = 90.0f; v.operators[2].velocitySensitivity = 2;
                v.operators[2].r1 = 99.0f; v.operators[2].l1 = 95.0f; v.operators[2].r2 = 65.0f; v.operators[2].l2 = 60.0f;
                v.operators[2].r3 = 30.0f; v.operators[2].l3 = 35.0f; v.operators[2].r4 = 60.0f; v.operators[2].l4 = 0.0f;

                v.operators[3].coarse = 14.0f; v.operators[3].outputLevel = 84.0f; v.operators[3].velocitySensitivity = 5;
                v.operators[3].r1 = 99.0f; v.operators[3].l1 = 95.0f; v.operators[3].r2 = 78.0f; v.operators[3].l2 = 40.0f;
                v.operators[3].r3 = 45.0f; v.operators[3].l3 = 0.0f;  v.operators[3].r4 = 65.0f; v.operators[3].l4 = 0.0f;

                v.operators[4].coarse = 1.0f; v.operators[4].detune = 0.08f; v.operators[4].outputLevel = 88.0f; v.operators[4].velocitySensitivity = 3;
                v.operators[4].r1 = 99.0f; v.operators[4].l1 = 90.0f; v.operators[4].r2 = 48.0f; v.operators[4].l2 = 72.0f;
                v.operators[4].r3 = 22.0f; v.operators[4].l3 = 40.0f; v.operators[4].r4 = 50.0f; v.operators[4].l4 = 0.0f;

                v.operators[5].coarse = 1.0f; v.operators[5].detune = -0.06f; v.operators[5].outputLevel = 72.0f; v.operators[5].velocitySensitivity = 4;
                v.operators[5].r1 = 90.0f; v.operators[5].l1 = 92.0f; v.operators[5].r2 = 45.0f; v.operators[5].l2 = 55.0f;
                v.operators[5].r3 = 20.0f; v.operators[5].l3 = 0.0f;  v.operators[5].r4 = 55.0f; v.operators[5].l4 = 0.0f;

                for (auto& op : v.operators) op.updateMultiplier();
            }
        }
    }

private:
    std::array<float, kNumVoices> voiceGainL_{};
    std::array<float, kNumVoices> voiceGainR_{};
    size_t voiceRoundRobin_{0};

    void initFactoryBank() {
        cartridgePatches_.clear();
        cartridgePatches_.reserve(8);

        // Patch 0: E.PIANO 1 (FullTines)
        {
            EatsDxPatch p;
            p.name = "E.PIANO 1";
            p.algorithm = 5;
            p.feedback = 6;
            p.operators[0] = {99,42,24,52, 98,82,60,0, 60,0,0,0,0, 0.0f,0,0,2, 98.0f,false,1.0f,0.0f};
            p.operators[1] = {95,38,18,55, 95,65,0,0,  60,0,0,0,0, 0.0f,0,0,4, 78.0f,false,1.0f,0.0f};
            p.operators[2] = {99,65,30,60, 95,60,35,0, 60,0,0,0,0, 0.0f,0,0,2, 90.0f,false,1.0f,0.0f};
            p.operators[3] = {99,78,45,65, 95,40,0,0,  60,0,0,0,0, 0.0f,0,0,5, 84.0f,false,14.0f,0.0f};
            p.operators[4] = {99,48,22,50, 90,72,40,0, 60,0,0,0,0, 0.08f,0,0,3, 88.0f,false,1.0f,0.0f};
            p.operators[5] = {90,45,20,55, 92,55,0,0,  60,0,0,0,0, -0.06f,0,0,4, 72.0f,false,1.0f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 1: SOLID BASS (Classic 80s FM Punch Bass)
        {
            EatsDxPatch p;
            p.name = "SOLID BASS";
            p.algorithm = 1;
            p.feedback = 7;
            p.operators[0] = {99,60,30,50, 99,85,75,0, 60,0,0,0,0, 0.0f,0,0,2, 99.0f,false,0.5f,0.0f};
            p.operators[1] = {99,45,20,45, 95,70,0,0,  60,0,0,0,0, 0.0f,0,0,4, 82.0f,false,0.5f,0.0f};
            p.operators[2] = {99,50,25,50, 99,80,0,0,  60,0,0,0,0, 0.0f,0,0,3, 75.0f,false,1.0f,0.0f};
            p.operators[3] = {99,40,15,40, 90,65,0,0,  60,0,0,0,0, 0.0f,0,0,4, 68.0f,false,1.0f,0.0f};
            p.operators[4] = {99,55,30,55, 95,75,0,0,  60,0,0,0,0, 0.0f,0,0,3, 60.0f,false,2.0f,0.0f};
            p.operators[5] = {99,40,10,40, 95,60,0,0,  60,0,0,0,0, 0.0f,0,0,5, 78.0f,false,0.5f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 2: TUB BELLS (Chimes / Tubular Bells)
        {
            EatsDxPatch p;
            p.name = "TUB BELLS";
            p.algorithm = 5;
            p.feedback = 5;
            p.operators[0] = {99,35,20,40, 99,80,40,0, 60,0,0,0,0, 0.0f,0,0,2, 95.0f,false,1.0f,0.0f};
            p.operators[1] = {99,45,25,45, 99,75,0,0,  60,0,0,0,0, 0.0f,0,0,4, 86.0f,false,3.5f,0.0f};
            p.operators[2] = {99,30,15,35, 95,70,30,0, 60,0,0,0,0, 0.0f,0,0,2, 92.0f,false,1.0f,0.0f};
            p.operators[3] = {99,50,30,50, 95,60,0,0,  60,0,0,0,0, 0.0f,0,0,4, 80.0f,false,9.0f,0.0f};
            p.operators[4] = {99,40,20,40, 90,65,25,0, 60,0,0,0,0, 0.05f,0,0,3, 85.0f,false,1.0f,0.0f};
            p.operators[5] = {95,40,15,45, 90,50,0,0,  60,0,0,0,0, -0.05f,0,0,4, 75.0f,false,5.25f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 3: DX BRASS 1 (Synth Brass Section)
        {
            EatsDxPatch p;
            p.name = "DX BRASS 1";
            p.algorithm = 22;
            p.feedback = 6;
            p.operators[0] = {85,60,40,55, 99,85,75,0, 60,0,0,0,0, 0.0f,0,0,2, 99.0f,false,1.0f,0.0f};
            p.operators[1] = {80,55,35,50, 99,80,65,0, 60,0,0,0,0, 0.0f,0,0,3, 76.0f,false,1.0f,0.0f};
            p.operators[2] = {88,62,42,55, 99,85,70,0, 60,0,0,0,0, 0.05f,0,0,2, 94.0f,false,1.0f,0.0f};
            p.operators[3] = {85,58,38,50, 99,80,60,0, 60,0,0,0,0, -0.05f,0,0,3, 74.0f,false,1.0f,0.0f};
            p.operators[4] = {82,55,35,45, 99,85,70,0, 60,0,0,0,0, 0.08f,0,0,2, 92.0f,false,1.0f,0.0f};
            p.operators[5] = {75,50,30,45, 95,75,55,0, 60,0,0,0,0, -0.08f,0,0,4, 70.0f,false,1.0f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 4: STRINGS 1 (Lush FM Ensemble Pad)
        {
            EatsDxPatch p;
            p.name = "STRINGS 1";
            p.algorithm = 22;
            p.feedback = 5;
            p.operators[0] = {75,50,40,60, 99,90,85,0, 60,0,0,0,0, 0.0f,0,0,2, 98.0f,false,1.0f,0.0f};
            p.operators[1] = {70,45,35,55, 90,80,70,0, 60,0,0,0,0, 0.0f,0,0,3, 70.0f,false,2.0f,0.0f};
            p.operators[2] = {78,52,42,60, 99,90,85,0, 60,0,0,0,0, 0.06f,0,0,2, 96.0f,false,1.0f,0.0f};
            p.operators[3] = {72,48,38,55, 88,78,68,0, 60,0,0,0,0, -0.06f,0,0,3, 68.0f,false,2.0f,0.0f};
            p.operators[4] = {76,50,40,58, 98,88,80,0, 60,0,0,0,0, 0.10f,0,0,2, 94.0f,false,2.0f,0.0f};
            p.operators[5] = {68,42,32,50, 85,75,65,0, 60,0,0,0,0, -0.10f,0,0,4, 65.0f,false,2.0f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 5: MARIMBA (Percussive Mallet)
        {
            EatsDxPatch p;
            p.name = "MARIMBA";
            p.algorithm = 5;
            p.feedback = 4;
            p.operators[0] = {99,75,40,65, 99,70,0,0,  60,0,0,0,0, 0.0f,0,0,3, 99.0f,false,1.0f,0.0f};
            p.operators[1] = {99,80,45,70, 99,60,0,0,  60,0,0,0,0, 0.0f,0,0,5, 80.0f,false,4.0f,0.0f};
            p.operators[2] = {99,70,35,60, 95,65,0,0,  60,0,0,0,0, 0.0f,0,0,3, 92.0f,false,1.0f,0.0f};
            p.operators[3] = {99,85,50,75, 95,55,0,0,  60,0,0,0,0, 0.0f,0,0,5, 78.0f,false,10.0f,0.0f};
            p.operators[4] = {99,72,38,62, 90,60,0,0,  60,0,0,0,0, 0.0f,0,0,3, 88.0f,false,1.0f,0.0f};
            p.operators[5] = {99,82,48,72, 90,50,0,0,  60,0,0,0,0, 0.0f,0,0,5, 74.0f,false,1.0f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 6: SYN-LEAD 1 (80s Cutting Poly Lead)
        {
            EatsDxPatch p;
            p.name = "SYN-LEAD 1";
            p.algorithm = 10;
            p.feedback = 6;
            p.operators[0] = {99,65,35,55, 99,85,75,0, 60,0,0,0,0, 0.0f,0,0,2, 99.0f,false,1.0f,0.0f};
            p.operators[1] = {95,60,30,50, 99,80,65,0, 60,0,0,0,0, 0.04f,0,0,3, 82.0f,false,1.0f,0.0f};
            p.operators[2] = {92,55,25,45, 95,75,60,0, 60,0,0,0,0, -0.04f,0,0,4, 76.0f,false,2.0f,0.0f};
            p.operators[3] = {99,70,40,60, 99,88,80,0, 60,0,0,0,0, 0.0f,0,0,2, 94.0f,false,1.0f,0.0f};
            p.operators[4] = {95,62,32,52, 95,78,65,0, 60,0,0,0,0, 0.06f,0,0,3, 80.0f,false,1.0f,0.0f};
            p.operators[5] = {90,58,28,48, 92,72,55,0, 60,0,0,0,0, -0.06f,0,0,4, 75.0f,false,3.0f,0.0f};
            cartridgePatches_.push_back(p);
        }

        // Patch 7: CLAV 1 (Funky FM Clavinet)
        {
            EatsDxPatch p;
            p.name = "CLAV 1";
            p.algorithm = 5;
            p.feedback = 6;
            p.operators[0] = {99,70,30,60, 99,75,0,0,  60,0,0,0,0, 0.0f,0,0,3, 98.0f,false,1.0f,0.0f};
            p.operators[1] = {99,75,35,65, 99,70,0,0,  60,0,0,0,0, 0.0f,0,0,4, 85.0f,false,3.0f,0.0f};
            p.operators[2] = {99,68,28,58, 95,70,0,0,  60,0,0,0,0, 0.0f,0,0,3, 92.0f,false,2.0f,0.0f};
            p.operators[3] = {99,72,32,62, 95,65,0,0,  60,0,0,0,0, 0.0f,0,0,4, 80.0f,false,7.0f,0.0f};
            p.operators[4] = {99,65,25,55, 90,65,0,0,  60,0,0,0,0, 0.0f,0,0,3, 88.0f,false,1.0f,0.0f};
            p.operators[5] = {99,70,30,60, 90,60,0,0,  60,0,0,0,0, 0.0f,0,0,4, 75.0f,false,1.0f,0.0f};
            cartridgePatches_.push_back(p);
        }
    }
};

using EatsDxSynth = DX7Synth;

} // namespace eatsbits::dsp

namespace eatsbits::audio::dsp {
    using namespace eatsbits::dsp;
}

#endif // EATS_DX_CORE_HPP
