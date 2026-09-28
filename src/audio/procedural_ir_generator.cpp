#include "eatsbits/audio/procedural_ir_generator.hpp"
#include <cmath>
#include <cstring>
#include <mutex>

namespace eatsbits::audio {

namespace {

// Fast, deterministic 32-bit PRNG for audio synthesis
class FastPRNG {
public:
    explicit FastPRNG(uint32_t seed = 1337) : state_(seed) {}

    uint32_t nextU32() noexcept {
        uint32_t z = (state_ += 0x6D2B79F5u);
        z = (z ^ (z >> 15)) * (z | 1u);
        z ^= z + (z ^ (z >> 7)) * (z | 61u);
        return z ^ (z >> 14);
    }

    float nextFloat() noexcept {
        return static_cast<float>(nextU32()) / 4294967296.0f;
    }

private:
    uint32_t state_;
};

static std::vector<AcousticSpaceParams> sCustomPresets;
static std::mutex sPresetMutex;

} // anonymous namespace

ProceduralIRGenerator::Biquad ProceduralIRGenerator::Biquad::highPass(float fs, float fc, float q) noexcept {
    Biquad f;
    float w0 = 2.0f * kPi * fc / fs;
    float alpha = std::sin(w0) / (2.0f * q);
    float cosw0 = std::cos(w0);
    float a0 = 1.0f + alpha;

    f.b0 = ((1.0f + cosw0) * 0.5f) / a0;
    f.b1 = (-(1.0f + cosw0)) / a0;
    f.b2 = ((1.0f + cosw0) * 0.5f) / a0;
    f.a1 = (-2.0f * cosw0) / a0;
    f.a2 = (1.0f - alpha) / a0;
    return f;
}

ProceduralIRGenerator::Biquad ProceduralIRGenerator::Biquad::lowPass(float fs, float fc, float q) noexcept {
    Biquad f;
    float w0 = 2.0f * kPi * fc / fs;
    float alpha = std::sin(w0) / (2.0f * q);
    float cosw0 = std::cos(w0);
    float a0 = 1.0f + alpha;

    f.b0 = ((1.0f - cosw0) * 0.5f) / a0;
    f.b1 = (1.0f - cosw0) / a0;
    f.b2 = ((1.0f - cosw0) * 0.5f) / a0;
    f.a1 = (-2.0f * cosw0) / a0;
    f.a2 = (1.0f - alpha) / a0;
    return f;
}

ProceduralIRGenerator::Biquad ProceduralIRGenerator::Biquad::peakingEQ(float fs, float fc, float q, float gainDb) noexcept {
    Biquad f;
    float a = std::pow(10.0f, gainDb / 40.0f);
    float w0 = 2.0f * kPi * fc / fs;
    float alpha = std::sin(w0) / (2.0f * q);
    float cosw0 = std::cos(w0);
    float a0 = 1.0f + alpha / a;

    f.b0 = (1.0f + alpha * a) / a0;
    f.b1 = (-2.0f * cosw0) / a0;
    f.b2 = (1.0f - alpha * a) / a0;
    f.a1 = (-2.0f * cosw0) / a0;
    f.a2 = (1.0f - alpha / a) / a0;
    return f;
}

void ProceduralIRGenerator::Biquad::processInPlace(std::vector<float>& buffer) noexcept {
    for (size_t i = 0; i < buffer.size(); ++i) {
        float x = buffer[i];
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        buffer[i] = y;
    }
}

const std::vector<AcousticSpaceParams>& ProceduralIRGenerator::getStockPresets() {
    static const std::vector<AcousticSpaceParams> kPresets = {
        // --- Acoustic Spaces & Reverb Chambers ---
        { "Stone Cathedral", 24.0f, 45.0f, 18.0f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::Concrete, 3.8f, 0.15f, 0.60f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Great Hall", 15.0f, 25.0f, 10.0f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 1.6f, 0.25f, 0.50f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Studio Live Room", 6.5f, 9.0f, 3.5f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 0.65f, 0.40f, 0.45f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Warm Room", 5.0f, 6.5f, 3.0f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 0.60f, 0.35f, 0.40f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Small Vocal Booth", 1.8f, 2.0f, 2.3f, 0.5f, 0.4f, 0.5f, 0.5f, 0.7f, 0.5f, AcousticMaterialType::AcousticFoam, 0.18f, 0.85f, 0.80f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Tile Bathroom", 2.5f, 3.0f, 2.6f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::Concrete, 1.1f, 0.10f, 0.20f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Plate Reverb", 3.0f, 4.0f, 2.0f, 0.4f, 0.4f, 0.5f, 0.6f, 0.6f, 0.5f, AcousticMaterialType::SheetMetal, 1.5f, 0.08f, 0.75f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Spring Tank", 1.2f, 0.6f, 0.4f, 0.2f, 0.5f, 0.5f, 0.8f, 0.5f, 0.5f, AcousticMaterialType::SheetMetal, 1.2f, 0.35f, 0.65f, false, 0.0f, 0.0f, false, false, 0.05f, 0.0f, false, 0.20f },

        // --- Non-Linear & Gated Reverbs ---
        { "80s Gated Chamber", 10.0f, 14.0f, 6.0f, 0.5f, 0.4f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::Concrete, 1.2f, 0.20f, 0.70f, true, 180.0f, 20.0f, false, false, 0.05f, 0.0f, false, 0.20f },
        { "Non-Linear Reverse Snare", 8.0f, 10.0f, 5.0f, 0.5f, 0.4f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 0.9f, 0.25f, 0.80f, false, 200.0f, 10.0f, true, false, 0.05f, 0.0f, false, 0.20f },

        // --- Amp Cabinets & Enclosures ---
        { "4x12 Vintage Stack (Closed)", 0.76f, 0.76f, 0.36f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::BirchPlywood, 0.035f, 0.55f, 0.30f, false, 0.0f, 0.0f, false, true, 0.05f, 0.0f, false, 0.20f },
        { "2x12 British Celestion", 0.70f, 0.52f, 0.30f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::BirchPlywood, 0.030f, 0.50f, 0.35f, false, 0.0f, 0.0f, false, true, 0.08f, 25.0f, false, 0.20f },
        { "1x12 Tweed Combo (Open-Back)", 0.50f, 0.42f, 0.24f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::PineWood, 0.025f, 0.45f, 0.40f, false, 0.0f, 0.0f, false, true, 0.03f, 15.0f, true, 0.20f },
        { "Bass 8x10 Fridge", 0.66f, 1.22f, 0.41f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::BirchPlywood, 0.045f, 0.60f, 0.25f, false, 0.0f, 0.0f, false, true, 0.10f, 0.0f, false, 0.20f },
        { "Small Radio Speaker", 0.20f, 0.15f, 0.12f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::PineWood, 0.020f, 0.75f, 0.15f, false, 0.0f, 0.0f, false, true, 0.04f, 0.0f, true, 0.20f },
        { "Acoustic Resonator Box", 0.40f, 0.30f, 0.20f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::StudioWood, 0.040f, 0.30f, 0.45f, false, 0.0f, 0.0f, false, true, 0.05f, 10.0f, false, 0.20f }
    };
    return kPresets;
}

const AcousticSpaceParams* ProceduralIRGenerator::findPreset(const std::string& name) {
    const auto& stock = getStockPresets();
    for (const auto& p : stock) {
        if (p.name == name) return &p;
    }
    {
        std::lock_guard<std::mutex> lock(sPresetMutex);
        for (const auto& p : sCustomPresets) {
            if (p.name == name) return &p;
        }
    }
    for (const auto& p : stock) {
        if (p.name.find(name) != std::string::npos || name.find(p.name) != std::string::npos) {
            return &p;
        }
    }
    return stock.empty() ? nullptr : &stock[1]; // Default to Great Hall
}

bool ProceduralIRGenerator::registerCustomPreset(const AcousticSpaceParams& params) {
    std::lock_guard<std::mutex> lock(sPresetMutex);
    for (auto& p : sCustomPresets) {
        if (p.name == params.name) {
            p = params;
            return true;
        }
    }
    sCustomPresets.push_back(params);
    return true;
}

std::vector<std::string> ProceduralIRGenerator::getAvailablePresetNames() {
    std::vector<std::string> names;
    const auto& stock = getStockPresets();
    names.reserve(stock.size() + 8);
    for (const auto& p : stock) {
        names.push_back(p.name);
    }
    {
        std::lock_guard<std::mutex> lock(sPresetMutex);
        for (const auto& p : sCustomPresets) {
            names.push_back(p.name);
        }
    }
    return names;
}

StereoIRBuffer ProceduralIRGenerator::generateStereo(
    const AcousticSpaceParams& p,
    int sampleRate,
    int maxSamples
) {
    if (p.isCabinetMode) {
        return generateCabinetIRStereo(p, sampleRate, maxSamples);
    } else {
        return generateRoomIRStereo(p, sampleRate, maxSamples);
    }
}

std::vector<float> ProceduralIRGenerator::generateMono(
    const AcousticSpaceParams& p,
    int sampleRate,
    int maxSamples
) {
    auto stereo = generateStereo(p, sampleRate, maxSamples);
    std::vector<float> mono(stereo.left.size());
    for (size_t i = 0; i < mono.size(); ++i) {
        mono[i] = (stereo.left[i] + stereo.right[i]) * 0.5f;
    }
    return mono;
}

void ProceduralIRGenerator::normalizeStereo(
    std::vector<float>& left,
    std::vector<float>& right,
    float maxPeak
) noexcept {
    float peak = 0.0f;
    for (float s : left) {
        float a = std::abs(s);
        if (a > peak) peak = a;
    }
    for (float s : right) {
        float a = std::abs(s);
        if (a > peak) peak = a;
    }

    if (peak > 1e-6f) {
        float gain = maxPeak / peak;
        for (size_t i = 0; i < left.size(); ++i) {
            left[i] *= gain;
            right[i] *= gain;
        }
    }
}

StereoIRBuffer ProceduralIRGenerator::generateRoomIRStereo(
    const AcousticSpaceParams& p,
    int sampleRate,
    int maxSamples
) {
    int totalSamples = std::clamp(static_cast<int>(p.rt60 * sampleRate), 512, maxSamples);
    StereoIRBuffer out;
    out.left.assign(totalSamples, 0.0f);
    out.right.assign(totalSamples, 0.0f);

    auto mat = AcousticMaterial::get(p.material);

    float sx = std::clamp(p.sourceX * p.width, 0.01f, p.width - 0.01f);
    float sy = std::clamp(p.sourceY * p.length, 0.01f, p.length - 0.01f);
    float sz = std::clamp(p.sourceZ * p.height, 0.01f, p.height - 0.01f);

    float earOffset = std::clamp(p.stereoWidth * 0.5f, 0.02f, p.width * 0.4f);
    float rxCenter = std::clamp(p.listenerX * p.width, 0.05f, p.width - 0.05f);
    float ry = std::clamp(p.listenerY * p.length, 0.01f, p.length - 0.01f);
    float rz = std::clamp(p.listenerZ * p.height, 0.01f, p.height - 0.01f);

    float rxL = std::clamp(rxCenter - earOffset, 0.01f, p.width - 0.01f);
    float rxR = std::clamp(rxCenter + earOffset, 0.01f, p.width - 0.01f);

    // --- Phase A: 3D Image Source Method (ISM, Order <= 3) with Multiband Damping & Scattering ---
    constexpr int kOrder = 3;
    float avgReflect = 1.0f - ((mat.alphaLow * 0.25f + mat.alphaMid * 0.45f + mat.alphaHigh * 0.30f));
    float userDampFactor = 1.0f - std::clamp(p.damping * 0.45f, 0.0f, 0.85f);
    float effectiveDiffusion = std::clamp(p.diffusion * 0.8f + mat.diffusion * 0.2f, 0.0f, 1.0f);

    FastPRNG ismPrng(4321);

    for (int u = -kOrder; u <= kOrder; ++u) {
        for (int v = -kOrder; v <= kOrder; ++v) {
            for (int w = -kOrder; w <= kOrder; ++w) {
                if (u == 0 && v == 0 && w == 0) continue; // Direct path handled separately

                float ix = ((u % 2 == 0) ? u * p.width + sx : u * p.width + (p.width - sx));
                float iy = ((v % 2 == 0) ? v * p.length + sy : v * p.length + (p.length - sy));
                float iz = ((w % 2 == 0) ? w * p.height + sz : w * p.height + (p.height - sz));

                int bounces = std::abs(u) + std::abs(v) + std::abs(w);
                float sign = (bounces % 2 == 0) ? 1.0f : -1.0f;
                float bounceAtten = std::pow(avgReflect * userDampFactor, static_cast<float>(bounces));

                // Left Ear Ray
                float dxL = ix - rxL;
                float dyL = iy - ry;
                float dzL = iz - rz;
                float distL = std::sqrt(dxL * dxL + dyL * dyL + dzL * dzL);

                // Scattering diffusion jitter: higher diffusion scatters arrival times slightly
                float scatterOffsetL = (effectiveDiffusion > 0.05f)
                    ? (ismPrng.nextFloat() - 0.5f) * effectiveDiffusion * 0.003f * static_cast<float>(sampleRate)
                    : 0.0f;

                int delaySamplesL = static_cast<int>(distL / kSpeedOfSound * sampleRate + scatterOffsetL);
                if (delaySamplesL >= 0 && delaySamplesL < totalSamples) {
                    float attenL = (1.0f / (distL + 1.0f)) * bounceAtten;
                    out.left[delaySamplesL] += sign * attenL;
                }

                // Right Ear Ray
                float dxR = ix - rxR;
                float dyR = iy - ry;
                float dzR = iz - rz;
                float distR = std::sqrt(dxR * dxR + dyR * dyR + dzR * dzR);

                float scatterOffsetR = (effectiveDiffusion > 0.05f)
                    ? (ismPrng.nextFloat() - 0.5f) * effectiveDiffusion * 0.003f * static_cast<float>(sampleRate)
                    : 0.0f;

                int delaySamplesR = static_cast<int>(distR / kSpeedOfSound * sampleRate + scatterOffsetR);
                if (delaySamplesR >= 0 && delaySamplesR < totalSamples) {
                    float attenR = (1.0f / (distR + 1.0f)) * bounceAtten;
                    out.right[delaySamplesR] += sign * attenR;
                }
            }
        }
    }

    // Direct Arrival Spikes for Left and Right Ears
    float directDistL = std::sqrt((sx - rxL) * (sx - rxL) + (sy - ry) * (sy - ry) + (sz - rz) * (sz - rz));
    int directDelayL = std::clamp(static_cast<int>(directDistL / kSpeedOfSound * sampleRate), 0, totalSamples - 1);
    out.left[directDelayL] += 1.0f / (directDistL + 1.0f);

    float directDistR = std::sqrt((sx - rxR) * (sx - rxR) + (sy - ry) * (sy - ry) + (sz - rz) * (sz - rz));
    int directDelayR = std::clamp(static_cast<int>(directDistR / kSpeedOfSound * sampleRate), 0, totalSamples - 1);
    out.right[directDelayR] += 1.0f / (directDistR + 1.0f);

    // --- Phase B: Dual-Seed Velvet Noise Stochastic Late Diffuse Tail ---
    float decayCoeff = 6.907755f / (p.rt60 * sampleRate); // ln(1000) / (rt60 * fs)
    constexpr int kVelvetGrid = 4;
    FastPRNG prngL(1337);
    FastPRNG prngR(7331); // Decorrelated seed for Right Channel

    float lpAlpha = std::clamp(1.0f - (p.damping * 0.55f + mat.alphaHigh * 0.35f), 0.05f, 0.98f);
    float lpStateL = 0.0f;
    float lpStateR = 0.0f;

    float decayFactor = std::exp(-decayCoeff);
    float env = 1.0f;

    for (int n = 0; n < totalSamples; ++n) {
        float pulseL = 0.0f;
        if (n % kVelvetGrid == 0) {
            uint32_t r = prngL.nextU32() % 3;
            pulseL = (r == 1) ? 1.0f : (r == 2) ? -1.0f : 0.0f;
        }
        lpStateL = (1.0f - lpAlpha) * (pulseL * env) + lpAlpha * lpStateL;
        out.left[n] += lpStateL * 0.35f;

        float pulseR = 0.0f;
        if (n % kVelvetGrid == 0) {
            uint32_t r = prngR.nextU32() % 3;
            pulseR = (r == 1) ? 1.0f : (r == 2) ? -1.0f : 0.0f;
        }
        lpStateR = (1.0f - lpAlpha) * (pulseR * env) + lpAlpha * lpStateR;
        out.right[n] += lpStateR * 0.35f;

        env *= decayFactor;
    }

    // --- Phase C: Non-Linear & Gated Reverb Envelope Shaping ---
    if (p.isGated) {
        // Hold steady reverberation density, then gate sharply
        int holdSamples = static_cast<int>((p.gateHoldMs * 0.001f) * sampleRate);
        int releaseSamples = std::max(8, static_cast<int>((p.gateReleaseMs * 0.001f) * sampleRate));

        for (int n = 0; n < totalSamples; ++n) {
            if (n < holdSamples) {
                // Boost sustained body slightly to simulate explosive gated drum plate
                out.left[n] *= 1.25f;
                out.right[n] *= 1.25f;
            } else if (n < holdSamples + releaseSamples) {
                float t = static_cast<float>(n - holdSamples) / static_cast<float>(releaseSamples);
                float gateFade = 0.5f * (1.0f + std::cos(kPi * t)); // Half-cosine window
                out.left[n] *= gateFade;
                out.right[n] *= gateFade;
            } else {
                out.left[n] = 0.0f;
                out.right[n] = 0.0f;
            }
        }
    } else if (p.isReverse) {
        // Reverse envelope swell
        float swellNorm = 1.0f / static_cast<float>(totalSamples);
        for (int n = 0; n < totalSamples; ++n) {
            float t = static_cast<float>(n) * swellNorm;
            float swell = std::pow(t, 2.5f); // Exponential rise
            out.left[n] *= swell;
            out.right[n] *= swell;
        }
    }

    normalizeStereo(out.left, out.right, 0.95f);
    return out;
}

StereoIRBuffer ProceduralIRGenerator::generateCabinetIRStereo(
    const AcousticSpaceParams& p,
    int sampleRate,
    int maxSamples
) {
    int totalSamples = std::clamp(static_cast<int>(p.rt60 * sampleRate), 256, maxSamples);
    StereoIRBuffer out;
    out.left.assign(totalSamples, 0.0f);
    out.right.assign(totalSamples, 0.0f);

    float bassFreq = (p.name.find("Bass") != std::string::npos) ? 55.0f : 85.0f;
    float highCutFreq = (p.name.find("Radio") != std::string::npos) ? 3500.0f : 5000.0f;
    float presenceFreq = 3200.0f;

    float angleRadL = std::clamp(p.micAngleDeg, 0.0f, 90.0f) * (kPi / 180.0f);
    float angleRadR = std::clamp(p.micAngleDeg + 8.0f, 0.0f, 90.0f) * (kPi / 180.0f);
    float offAxisDampingL = std::clamp(std::cos(angleRadL), 0.2f, 1.0f);
    float offAxisDampingR = std::clamp(std::cos(angleRadR), 0.2f, 1.0f);

    out.left[0] = 1.0f;
    out.right[0] = 1.0f;

    float enclosureDims[3] = { p.width, p.length, p.height };
    auto mat = AcousticMaterial::get(p.material);
    float boxReflect = 1.0f - ((mat.alphaLow + mat.alphaMid) * 0.5f);

    for (int dim = 0; dim < 3; ++dim) {
        float dimDist = enclosureDims[dim];
        float roundTripTime = (dimDist * 2.0f) / kSpeedOfSound;
        int roundTripSamples = static_cast<int>(roundTripTime * sampleRate);

        if (roundTripSamples > 0 && roundTripSamples < totalSamples) {
            out.left[roundTripSamples] += 0.45f * boxReflect;
            out.right[roundTripSamples] += 0.42f * boxReflect;
            if (roundTripSamples * 2 < totalSamples) {
                out.left[roundTripSamples * 2] -= 0.25f * boxReflect * boxReflect;
                out.right[roundTripSamples * 2] -= 0.23f * boxReflect * boxReflect;
            }
        }
    }

    if (p.isOpenBack) {
        float rearPathDist = p.length + p.micDistance;
        int rearDelaySamples = static_cast<int>(rearPathDist / kSpeedOfSound * sampleRate);
        if (rearDelaySamples < totalSamples) {
            out.left[rearDelaySamples] -= 0.65f;
            out.right[rearDelaySamples] -= 0.60f;
        }
    }

    // Apply Speaker Filter Chain (HPF bass resonance, peaking EQ presence, LPF cone roll-off)
    auto hpfL = Biquad::highPass(static_cast<float>(sampleRate), bassFreq, 1.8f);
    auto peakL = Biquad::peakingEQ(static_cast<float>(sampleRate), presenceFreq, 1.4f, 4.5f);
    auto lpfL = Biquad::lowPass(static_cast<float>(sampleRate), highCutFreq * offAxisDampingL, 0.8f);
    hpfL.processInPlace(out.left);
    peakL.processInPlace(out.left);
    lpfL.processInPlace(out.left);

    auto hpfR = Biquad::highPass(static_cast<float>(sampleRate), bassFreq * 1.02f, 1.8f);
    auto peakR = Biquad::peakingEQ(static_cast<float>(sampleRate), presenceFreq * 0.98f, 1.4f, 4.5f);
    auto lpfR = Biquad::lowPass(static_cast<float>(sampleRate), highCutFreq * offAxisDampingR, 0.8f);
    hpfR.processInPlace(out.right);
    peakR.processInPlace(out.right);
    lpfR.processInPlace(out.right);

    float cabDecayStep = std::exp(-1.0f / (totalSamples * 0.4f));
    float cabDecay = 1.0f;
    for (size_t i = 0; i < out.left.size(); ++i) {
        out.left[i] *= cabDecay;
        out.right[i] *= cabDecay;
        cabDecay *= cabDecayStep;
    }

    normalizeStereo(out.left, out.right, 0.35f);
    return out;
}

} // namespace eatsbits::audio
