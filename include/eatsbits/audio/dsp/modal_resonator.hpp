#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <span>
#include <array>

namespace eatsbits::dsp {

/// Parallel Modal Resonator Bank.
/// Emulates the mechanical resonant modes of acoustic instrument bodies, soundboards,
/// tone bars, and wooden plates using parallel 2nd-order bandpass resonators.
class ModalResonatorBank {
public:
    static constexpr size_t kMaxModes = 8;

    struct ModeConfig {
        float freqRatio = 1.0f; // Multiplier relative to fundamental or base frequency
        float gain = 0.5f;      // Relative amplitude per mode
        float q = 15.0f;        // Q factor (decay sharpness) per mode
    };

    ModalResonatorBank() = default;

    void setModes(std::span<const ModeConfig> configs) {
        numModes_ = std::min(configs.size(), kMaxModes);
        for (size_t m = 0; m < numModes_; ++m) {
            modes_[m] = configs[m];
        }
    }

    void reset() {
        for (size_t m = 0; m < kMaxModes; ++m) {
            z1_[m] = 0.0;
            z2_[m] = 0.0;
        }
    }

    /// Process in-place: outBuffer contains input signal initially, and is replaced by
    /// the summed output of the modal resonator bank.
    void process(const float* inBuffer, float* outBuffer, size_t numFrames, float sampleRate, float baseFreq) {
        if (!inBuffer || !outBuffer || numFrames == 0 || numModes_ == 0) return;

        const double sr = sampleRate;
        const double baseF = baseFreq > 10.0f ? static_cast<double>(baseFreq) : 440.0;
        const double pi = 3.14159265358979323846;

        std::fill(outBuffer, outBuffer + numFrames, 0.0f);

        for (size_t m = 0; m < numModes_; ++m) {
            const double ratio = static_cast<double>(modes_[m].freqRatio);
            const double g = static_cast<double>(modes_[m].gain);
            const double q = static_cast<double>(modes_[m].q);

            const double f = std::clamp(baseF * ratio, 20.0, sr * 0.48);
            const double w0 = 2.0 * pi * (f / sr);
            const double alpha = std::sin(w0) / (2.0 * q);

            const double a0 = 1.0 + alpha;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha;

            const double normB0 = (alpha / a0) * g;
            const double normB2 = (-alpha / a0) * g;
            const double normA1 = a1 / a0;
            const double normA2 = a2 / a0;

            double z1 = z1_[m];
            double z2 = z2_[m];

            for (size_t i = 0; i < numFrames; ++i) {
                const double inSample = static_cast<double>(inBuffer[i]);
                const double outSample = normB0 * inSample + z1;
                z1 = -normA1 * outSample + z2;
                z2 = normB2 * inSample - normA2 * outSample;
                outBuffer[i] += static_cast<float>(outSample);
            }

            z1_[m] = z1;
            z2_[m] = z2;
        }
    }

private:
    std::array<ModeConfig, kMaxModes> modes_{};
    size_t numModes_ = 0;
    std::array<double, kMaxModes> z1_{};
    std::array<double, kMaxModes> z2_{};
};

/// Body Resonator for the Violin Family (Violin, Viola, Cello, Double Bass).
/// Accurately models the Helmholtz air cavity resonance and wood plate resonances (top and back).
class ViolinFamilyBodyResonator {
public:
    enum class InstrumentType {
        Violin = 0,
        Viola = 1,
        Cello = 2,
        DoubleBass = 3
    };

    ViolinFamilyBodyResonator() {
        setInstrumentType(InstrumentType::DoubleBass);
    }

    void setInstrumentType(InstrumentType type) {
        type_ = type;
        ModalResonatorBank::ModeConfig configs[3];
        switch (type) {
            case InstrumentType::Violin:
                // Air cavity: 280Hz, Top plate: 460Hz, Back plate: 560Hz
                configs[0] = {280.0f, 0.45f, 14.0f};
                configs[1] = {460.0f, 0.55f, 22.0f};
                configs[2] = {560.0f, 0.35f, 18.0f};
                break;
            case InstrumentType::Viola:
                // Air cavity: 215Hz, Top plate: 350Hz, Back plate: 440Hz
                configs[0] = {215.0f, 0.48f, 15.0f};
                configs[1] = {350.0f, 0.58f, 20.0f};
                configs[2] = {440.0f, 0.38f, 18.0f};
                break;
            case InstrumentType::Cello:
                // Air cavity: 110Hz, Top plate: 175Hz, Back plate: 230Hz
                configs[0] = {110.0f, 0.50f, 16.0f};
                configs[1] = {175.0f, 0.60f, 24.0f};
                configs[2] = {230.0f, 0.40f, 20.0f};
                break;
            case InstrumentType::DoubleBass:
            default:
                // Air cavity: 58Hz, Wood top: 98Hz, Spruce body plate: 145Hz
                configs[0] = {58.0f, 0.65f, 16.0f};
                configs[1] = {98.0f, 0.55f, 22.0f};
                configs[2] = {145.0f, 0.42f, 26.0f};
                break;
        }
        bank_.setModes(std::span<const ModalResonatorBank::ModeConfig>(configs, 3));
    }

    void reset() {
        bank_.reset();
    }

    void process(const float* inBuffer, float* outBuffer, size_t numFrames, float sampleRate, float woodWarmth = 0.75f) {
        // Base frequency 1.0 so freqRatio acts as absolute Hz for body cavity modes
        bank_.process(inBuffer, outBuffer, numFrames, sampleRate, 1.0f);
        const float warmth = std::clamp(woodWarmth, 0.1f, 2.0f);
        for (size_t i = 0; i < numFrames; ++i) {
            outBuffer[i] *= warmth;
        }
    }

private:
    InstrumentType type_ = InstrumentType::DoubleBass;
    ModalResonatorBank bank_;
};

/// Asymmetric Magnetic Pickup Saturation.
/// Models electromagnetic guitar & tine pickup behavior where vibrating strings swing
/// closer to magnetic pole pieces, generating warm 2nd-order even harmonics and soft compression.
class PickupSaturation {
public:
    static void process(float* inOutBuffer, size_t numFrames, float distance = 1.0f, float symmetry = 0.65f) {
        if (!inOutBuffer || numFrames == 0) return;

        const float dist = std::clamp(distance, 0.1f, 3.0f);
        const float sym = std::clamp(symmetry, 0.0f, 1.0f);

        const float gain = 1.0f / std::sqrt(dist);
        const float alpha = 0.35f * sym * (1.5f / dist); // 2nd harmonic quadratic term
        const float beta = 0.15f * (1.0f / dist);        // 3rd harmonic cubic term

        for (size_t i = 0; i < numFrames; ++i) {
            float x = inOutBuffer[i] * gain;
            // Asymmetric magnetic reluctance polynomial: y = x + alpha*x*|x| - beta*x^3
            float y = x + (alpha * x * std::abs(x)) - (beta * x * x * x);
            // Soft-clip boundary
            if (y > 1.2f) y = 1.2f + 0.1f * std::tanh(y - 1.2f);
            if (y < -1.2f) y = -1.2f + 0.1f * std::tanh(y + 1.2f);
            inOutBuffer[i] = y * 0.9f;
        }
    }
};

} // namespace eatsbits::dsp
