#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <array>
#include <memory>
#include "prng.hpp"

namespace eatsbits::audio::dsp {

enum class AcousticMaterialType {
    BirchPlywood,
    PineWood,
    AcousticFoam,
    Concrete,
    StudioWood,
    VelvetDrapes,
    SheetMetal,
    Carpet
};

struct AcousticMaterial {
    AcousticMaterialType type;
    const char* displayName;
    float alphaLow;   // 125 - 250 Hz absorption (0.0 to 1.0)
    float alphaMid;   // 500 - 1000 Hz absorption (0.0 to 1.0)
    float alphaHigh;  // 2000 - 4000 Hz absorption (0.0 to 1.0)
    float diffusion;  // Scattering coefficient (0.0 to 1.0)

    static AcousticMaterial get(AcousticMaterialType t) {
        switch (t) {
            case AcousticMaterialType::BirchPlywood:
                return { t, "18mm Birch Plywood (Cab)", 0.28f, 0.15f, 0.10f, 0.25f };
            case AcousticMaterialType::PineWood:
                return { t, "Pine Wood (Vintage Cab)", 0.24f, 0.18f, 0.12f, 0.30f };
            case AcousticMaterialType::AcousticFoam:
                return { t, "Acoustic Foam / Studio", 0.15f, 0.70f, 0.95f, 0.85f };
            case AcousticMaterialType::Concrete:
                return { t, "Hard Concrete / Marble", 0.01f, 0.02f, 0.03f, 0.10f };
            case AcousticMaterialType::StudioWood:
                return { t, "Wood Paneling (Live Room)", 0.20f, 0.12f, 0.08f, 0.45f };
            case AcousticMaterialType::VelvetDrapes:
                return { t, "Heavy Velvet Drapes", 0.05f, 0.35f, 0.75f, 0.60f };
            case AcousticMaterialType::SheetMetal:
                return { t, "Sheet Metal / Plate", 0.02f, 0.03f, 0.04f, 0.05f };
            case AcousticMaterialType::Carpet:
                return { t, "Thin Carpet on Concrete", 0.02f, 0.15f, 0.50f, 0.40f };
            default:
                return { AcousticMaterialType::StudioWood, "Wood Paneling", 0.20f, 0.12f, 0.08f, 0.45f };
        }
    }
};

struct AcousticSpaceParams {
    std::string name;
    float width = 8.0f;       // meters (Lx)
    float length = 12.0f;     // meters (Ly)
    float height = 4.0f;      // meters (Lz)
    float sourceX = 0.5f;     // normalized (0..1)
    float sourceY = 0.5f;
    float sourceZ = 0.5f;
    float listenerX = 0.5f;
    float listenerY = 0.8f;
    float listenerZ = 0.5f;
    AcousticMaterialType material = AcousticMaterialType::StudioWood;
    float rt60 = 1.8f;        // seconds
    float damping = 0.5f;     // 0.0 (bright) to 1.0 (dark)
    bool isCabinetMode = false;
    float micDistance = 0.05f;// meters
    float micAngleDeg = 0.0f; // degrees off-axis
    bool isOpenBack = false;  // dipole cancellation
    float stereoWidth = 0.20f;// meters ear/mic spacing
};

struct StereoIRBuffer {
    std::vector<float> left;
    std::vector<float> right;

    size_t size() const { return left.size(); }
    bool empty() const { return left.empty(); }
    void clear() { left.clear(); right.clear(); }
};

class ProceduralIRGenerator {
public:
    static constexpr float kSpeedOfSound = 343.0f; // m/s
    static constexpr float kPi = 3.14159265358979323846f;

    static const std::vector<AcousticSpaceParams>& getStockPresets() {
        static const std::vector<AcousticSpaceParams> kPresets = {
            // Room & Hall Presets
            { "Stone Cathedral", 24.0f, 45.0f, 18.0f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::Concrete, 3.8f, 0.15f, false, 0.05f, 0.0f, false, 0.20f },
            { "Great Hall", 15.0f, 25.0f, 10.0f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 1.6f, 0.25f, false, 0.05f, 0.0f, false, 0.20f },
            { "Studio Live Room", 6.5f, 9.0f, 3.5f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 0.65f, 0.40f, false, 0.05f, 0.0f, false, 0.20f },
            { "Warm Room", 5.0f, 6.5f, 3.0f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::StudioWood, 0.60f, 0.35f, false, 0.05f, 0.0f, false, 0.20f },
            { "Small Vocal Booth", 1.8f, 2.0f, 2.3f, 0.5f, 0.4f, 0.5f, 0.5f, 0.7f, 0.5f, AcousticMaterialType::AcousticFoam, 0.18f, 0.85f, false, 0.05f, 0.0f, false, 0.20f },
            { "Tile Bathroom", 2.5f, 3.0f, 2.6f, 0.5f, 0.3f, 0.5f, 0.5f, 0.8f, 0.5f, AcousticMaterialType::Concrete, 1.1f, 0.10f, false, 0.05f, 0.0f, false, 0.20f },
            { "Plate Reverb", 3.0f, 4.0f, 2.0f, 0.4f, 0.4f, 0.5f, 0.6f, 0.6f, 0.5f, AcousticMaterialType::SheetMetal, 1.5f, 0.08f, false, 0.05f, 0.0f, false, 0.20f },
            { "Spring Tank", 1.2f, 0.6f, 0.4f, 0.2f, 0.5f, 0.5f, 0.8f, 0.5f, 0.5f, AcousticMaterialType::SheetMetal, 1.2f, 0.35f, false, 0.05f, 0.0f, false, 0.20f },

            // Amp Cabinet & Enclosure Presets
            { "4x12 Vintage Stack (Closed)", 0.76f, 0.76f, 0.36f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::BirchPlywood, 0.035f, 0.55f, true, 0.05f, 0.0f, false, 0.20f },
            { "2x12 British Celestion", 0.70f, 0.52f, 0.30f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::BirchPlywood, 0.030f, 0.50f, true, 0.08f, 25.0f, false, 0.20f },
            { "1x12 Tweed Combo (Open-Back)", 0.50f, 0.42f, 0.24f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::PineWood, 0.025f, 0.45f, true, 0.03f, 15.0f, true, 0.20f },
            { "Bass 8x10 Fridge", 0.66f, 1.22f, 0.41f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::BirchPlywood, 0.045f, 0.60f, true, 0.10f, 0.0f, false, 0.20f },
            { "Small Radio Speaker", 0.20f, 0.15f, 0.12f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, AcousticMaterialType::PineWood, 0.020f, 0.75f, true, 0.04f, 0.0f, true, 0.20f }
        };
        return kPresets;
    }

    static const AcousticSpaceParams* findPreset(const std::string& name) {
        const auto& presets = getStockPresets();
        for (const auto& p : presets) {
            if (p.name == name) return &p;
        }
        for (const auto& p : presets) {
            if (p.name.find(name) != std::string::npos || name.find(p.name) != std::string::npos) {
                return &p;
            }
        }
        return presets.empty() ? nullptr : &presets[1]; // Default to Great Hall
    }

    static StereoIRBuffer generateStereo(const AcousticSpaceParams& p, int sampleRate = 44100, int maxSamples = 8192) {
        if (p.isCabinetMode) {
            return generateCabinetIRStereo(p, sampleRate, maxSamples);
        } else {
            return generateRoomIRStereo(p, sampleRate, maxSamples);
        }
    }

private:
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        static Biquad highPass(float fs, float fc, float q) {
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

        static Biquad lowPass(float fs, float fc, float q) {
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

        static Biquad peakingEQ(float fs, float fc, float q, float gainDb) {
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

        void processInPlace(std::vector<float>& buffer) {
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
    };

    static StereoIRBuffer generateRoomIRStereo(const AcousticSpaceParams& p, int sampleRate, int maxSamples) {
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

        // Phase A: Image Source Method (Order <= 3) for Early Reflections
        constexpr int kOrder = 3;
        float avgReflect = 1.0f - ((mat.alphaLow + mat.alphaMid + mat.alphaHigh) / 3.0f);
        float userDampFactor = 1.0f - std::clamp(p.damping * 0.4f, 0.0f, 0.8f);

        for (int u = -kOrder; u <= kOrder; ++u) {
            for (int v = -kOrder; v <= kOrder; ++v) {
                for (int w = -kOrder; w <= kOrder; ++w) {
                    if (u == 0 && v == 0 && w == 0) continue; // Direct path below

                    float ix = ((u % 2 == 0) ? u * p.width + sx : u * p.width + (p.width - sx));
                    float iy = ((v % 2 == 0) ? v * p.length + sy : v * p.length + (p.length - sy));
                    float iz = ((w % 2 == 0) ? w * p.height + sz : w * p.height + (p.height - sz));

                    int bounces = std::abs(u) + std::abs(v) + std::abs(w);
                    float sign = (bounces % 2 == 0) ? 1.0f : -1.0f;
                    float bounceAtten = std::pow(avgReflect * userDampFactor, static_cast<float>(bounces));

                    // Left Ear
                    float dxL = ix - rxL;
                    float dyL = iy - ry;
                    float dzL = iz - rz;
                    float distL = std::sqrt(dxL * dxL + dyL * dyL + dzL * dzL);
                    int delaySamplesL = static_cast<int>(distL / kSpeedOfSound * sampleRate);
                    if (delaySamplesL < totalSamples) {
                        float attenL = (1.0f / (distL + 1.0f)) * bounceAtten;
                        out.left[delaySamplesL] += sign * attenL;
                    }

                    // Right Ear
                    float dxR = ix - rxR;
                    float dyR = iy - ry;
                    float dzR = iz - rz;
                    float distR = std::sqrt(dxR * dxR + dyR * dyR + dzR * dzR);
                    int delaySamplesR = static_cast<int>(distR / kSpeedOfSound * sampleRate);
                    if (delaySamplesR < totalSamples) {
                        float attenR = (1.0f / (distR + 1.0f)) * bounceAtten;
                        out.right[delaySamplesR] += sign * attenR;
                    }
                }
            }
        }

        // Direct Arrival Spikes
        float directDistL = std::sqrt((sx - rxL) * (sx - rxL) + (sy - ry) * (sy - ry) + (sz - rz) * (sz - rz));
        int directDelayL = std::clamp(static_cast<int>(directDistL / kSpeedOfSound * sampleRate), 0, totalSamples - 1);
        out.left[directDelayL] += 1.0f / (directDistL + 1.0f);

        float directDistR = std::sqrt((sx - rxR) * (sx - rxR) + (sy - ry) * (sy - ry) + (sz - rz) * (sz - rz));
        int directDelayR = std::clamp(static_cast<int>(directDistR / kSpeedOfSound * sampleRate), 0, totalSamples - 1);
        out.right[directDelayR] += 1.0f / (directDistR + 1.0f);

        // Phase B: Dual-Seed Velvet Noise Late Diffuse Tail
        float decayCoeff = 6.907755f / (p.rt60 * sampleRate); // ln(1000) / (rt60 * fs)
        constexpr int kVelvetGrid = 4;
        DeterministicPRNG prngL(1337);
        DeterministicPRNG prngR(7331);

        float lpAlpha = std::clamp(1.0f - (p.damping * 0.6f + mat.alphaHigh * 0.3f), 0.05f, 0.98f);
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

        normalizeStereo(out.left, out.right, 0.95f);
        return out;
    }

    static StereoIRBuffer generateCabinetIRStereo(const AcousticSpaceParams& p, int sampleRate, int maxSamples) {
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

    static void normalizeStereo(std::vector<float>& left, std::vector<float>& right, float maxPeak = 0.95f) {
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
};

} // namespace eatsbits::audio::dsp
