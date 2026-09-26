#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <numbers>
#include "biquad.hpp"

namespace eatsbits::dsp {

struct EqBandSettings {
    bool enabled = true;
    BiquadType type = BiquadType::Peaking;
    float frequency = 1000.0f;
    float q = 1.0f;
    float gainDb = 0.0f;
};

/**
 * 5-Band Studio Parametric Equalizer.
 * Consists of cascaded Direct Form II Transposed stereo biquads:
 * - Band 0: Highpass / Low-Cut (30 Hz default)
 * - Band 1: Low Shelf (120 Hz, +/-18dB)
 * - Band 2: Low-Mid Peaking Bell (600 Hz, +/-18dB, variable Q)
 * - Band 3: High-Mid Peaking Bell (2500 Hz, +/-18dB, variable Q)
 * - Band 4: High Shelf (8000 Hz, +/-18dB)
 */
class ParametricEqCore {
public:
    static constexpr size_t kNumBands = 5;

    explicit ParametricEqCore(float sampleRate = 44100.0f) : sampleRate_(sampleRate) {
        // Default 5-band configuration
        bands_[0] = {true, BiquadType::HighPass, 30.0f, 0.707f, 0.0f};
        bands_[1] = {true, BiquadType::LowShelf, 120.0f, 0.707f, 0.0f};
        bands_[2] = {true, BiquadType::Peaking, 600.0f, 1.0f, 0.0f};
        bands_[3] = {true, BiquadType::Peaking, 2500.0f, 1.0f, 0.0f};
        bands_[4] = {true, BiquadType::HighShelf, 8000.0f, 0.707f, 0.0f};

        updateAllBands();
        reset();
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = sr;
        updateAllBands();
        reset();
    }

    void reset() noexcept {
        for (size_t b = 0; b < kNumBands; ++b) {
            filtersL_[b].reset();
            filtersR_[b].reset();
        }
    }

    void configureBand(size_t bandIdx, bool enabled, BiquadType type, float freqHz, float q, float gainDb) noexcept {
        if (bandIdx >= kNumBands) return;
        bands_[bandIdx] = {enabled, type, freqHz, q, gainDb};
        filtersL_[bandIdx].configure(type, freqHz, q, gainDb, sampleRate_);
        filtersR_[bandIdx].configure(type, freqHz, q, gainDb, sampleRate_);
    }

    void setBandGain(size_t bandIdx, float gainDb) noexcept {
        if (bandIdx >= kNumBands) return;
        bands_[bandIdx].gainDb = gainDb;
        filtersL_[bandIdx].configure(bands_[bandIdx].type, bands_[bandIdx].frequency, bands_[bandIdx].q, gainDb, sampleRate_);
        filtersR_[bandIdx].configure(bands_[bandIdx].type, bands_[bandIdx].frequency, bands_[bandIdx].q, gainDb, sampleRate_);
    }

    void setBandFrequency(size_t bandIdx, float freqHz) noexcept {
        if (bandIdx >= kNumBands) return;
        bands_[bandIdx].frequency = freqHz;
        filtersL_[bandIdx].configure(bands_[bandIdx].type, freqHz, bands_[bandIdx].q, bands_[bandIdx].gainDb, sampleRate_);
        filtersR_[bandIdx].configure(bands_[bandIdx].type, freqHz, bands_[bandIdx].q, bands_[bandIdx].gainDb, sampleRate_);
    }

    void setBandQ(size_t bandIdx, float q) noexcept {
        if (bandIdx >= kNumBands) return;
        bands_[bandIdx].q = q;
        filtersL_[bandIdx].configure(bands_[bandIdx].type, bands_[bandIdx].frequency, q, bands_[bandIdx].gainDb, sampleRate_);
        filtersR_[bandIdx].configure(bands_[bandIdx].type, bands_[bandIdx].frequency, q, bands_[bandIdx].gainDb, sampleRate_);
    }

    void setBandEnabled(size_t bandIdx, bool enabled) noexcept {
        if (bandIdx < kNumBands) bands_[bandIdx].enabled = enabled;
    }

    void setBandBypass(size_t bandIdx, bool bypass) noexcept {
        setBandEnabled(bandIdx, !bypass);
    }

    void setMasterGain(float gainDb) noexcept {
        masterGainDb_ = std::clamp(gainDb, -36.0f, 36.0f);
    }

    [[nodiscard]] float getMasterGain() const noexcept {
        return masterGainDb_;
    }

    [[nodiscard]] const EqBandSettings& getBandSettings(size_t bandIdx) const noexcept {
        return bands_[bandIdx < kNumBands ? bandIdx : 0];
    }

    void processStereo(float* bufferL, float* bufferR, size_t numFrames) noexcept {
        if (!bufferL || !bufferR || numFrames == 0) return;

        for (size_t b = 0; b < kNumBands; ++b) {
            if (!bands_[b].enabled) continue;
            // In-place block filtering
            filtersL_[b].process_block(bufferL, bufferL, numFrames);
            filtersR_[b].process_block(bufferR, bufferR, numFrames);
        }

        if (std::abs(masterGainDb_) > 1e-4f) {
            const float masterGainLin = std::pow(10.0f, masterGainDb_ / 20.0f);
            for (size_t i = 0; i < numFrames; ++i) {
                bufferL[i] *= masterGainLin;
                bufferR[i] *= masterGainLin;
            }
        }
    }

    /**
     * Evaluates composite magnitude response in dB at a given frequency.
     * Ideal for rendering graphical EQ curves in NanoVG or Filament canvases.
     */
    [[nodiscard]] float evalMagnitudeDb(float freqHz) const noexcept {
        const float w = 2.0f * static_cast<float>(std::numbers::pi) * freqHz / sampleRate_;
        float totalDb = 0.0f;

        for (size_t b = 0; b < kNumBands; ++b) {
            if (!bands_[b].enabled) continue;
            // RBJ biquad magnitude computation
            const auto& band = bands_[b];
            const float w0 = 2.0f * static_cast<float>(std::numbers::pi) * band.frequency / sampleRate_;
            const float cosW0 = std::cos(w0);
            const float sinW0 = std::sin(w0);
            const float alpha = sinW0 / (2.0f * std::max(0.01f, band.q));
            const float A = std::pow(10.0f, band.gainDb / 40.0f);

            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
            switch (band.type) {
                case BiquadType::HighPass:
                    b0 = (1.0f + cosW0) * 0.5f;
                    b1 = -(1.0f + cosW0);
                    b2 = (1.0f + cosW0) * 0.5f;
                    a0 = 1.0f + alpha;
                    a1 = -2.0f * cosW0;
                    a2 = 1.0f - alpha;
                    break;
                case BiquadType::LowShelf: {
                    const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
                    b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha);
                    b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
                    b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha);
                    a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha;
                    a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
                    a2 = (A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha;
                    break;
                }
                case BiquadType::Peaking:
                    b0 = 1.0f + alpha * A;
                    b1 = -2.0f * cosW0;
                    b2 = 1.0f - alpha * A;
                    a0 = 1.0f + alpha / A;
                    a1 = -2.0f * cosW0;
                    a2 = 1.0f - alpha / A;
                    break;
                case BiquadType::HighShelf: {
                    const float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
                    b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha);
                    b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW0);
                    b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha);
                    a0 = (A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha;
                    a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW0);
                    a2 = (A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha;
                    break;
                }
                default: break;
            }

            // Complex evaluation: H(e^jw) = (b0 + b1*e^-jw + b2*e^-2jw) / (a0 + a1*e^-jw + a2*e^-2jw)
            const float cosW = std::cos(w);
            const float sinW = std::sin(w);
            const float cos2W = std::cos(2.0f * w);
            const float sin2W = std::sin(2.0f * w);

            const float numR = b0 + b1 * cosW + b2 * cos2W;
            const float numI = -b1 * sinW - b2 * sin2W;
            const float denR = a0 + a1 * cosW + a2 * cos2W;
            const float denI = -a1 * sinW - a2 * sin2W;

            const float numMagSq = numR * numR + numI * numI;
            const float denMagSq = denR * denR + denI * denI;

            if (denMagSq > 1e-12f) {
                totalDb += 10.0f * std::log10(numMagSq / denMagSq);
            }
        }

        return totalDb;
    }

private:
    void updateAllBands() noexcept {
        for (size_t b = 0; b < kNumBands; ++b) {
            filtersL_[b].configure(bands_[b].type, bands_[b].frequency, bands_[b].q, bands_[b].gainDb, sampleRate_);
            filtersR_[b].configure(bands_[b].type, bands_[b].frequency, bands_[b].q, bands_[b].gainDb, sampleRate_);
        }
    }

    float sampleRate_ = 44100.0f;
    float masterGainDb_ = 0.0f;
    std::array<EqBandSettings, kNumBands> bands_{};
    std::array<BiquadFilter, kNumBands> filtersL_{};
    std::array<BiquadFilter, kNumBands> filtersR_{};
};

} // namespace eatsbits::dsp
