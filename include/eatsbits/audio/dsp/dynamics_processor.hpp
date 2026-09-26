#pragma once

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>

namespace eatsbits::dsp {

/**
 * Studio Dynamic Range Compressor with logarithmic decibel ballistics,
 * soft-knee polynomial smoothing, auto-makeup gain, and external sidechain input.
 */
class CompressorCore {
public:
    explicit CompressorCore(float sampleRate = 44100.0f)
        : sampleRate_(sampleRate),
          rmsWindow_(std::clamp(static_cast<size_t>(sampleRate * 0.01f), static_cast<size_t>(16), static_cast<size_t>(1024))) {
        reset();
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = sr;
        rmsWindow_ = std::clamp(static_cast<size_t>(sr * 0.01f), static_cast<size_t>(16), static_cast<size_t>(1024));
        reset();
    }

    void reset() noexcept {
        detectorDb_ = -96.0f;
        gainReductionDb_ = 0.0f;
        rmsSum_ = 0.0f;
        rmsCount_ = 0;
    }

    void setThreshold(float db) noexcept { thresholdDb_ = std::clamp(db, -60.0f, 0.0f); }
    [[nodiscard]] float getThreshold() const noexcept { return thresholdDb_; }

    void setRatio(float r) noexcept { ratio_ = std::clamp(r, 1.0f, 30.0f); }
    [[nodiscard]] float getRatio() const noexcept { return ratio_; }

    void setKnee(float db) noexcept { kneeDb_ = std::clamp(db, 0.0f, 24.0f); }
    [[nodiscard]] float getKnee() const noexcept { return kneeDb_; }

    void setAttack(float ms) noexcept { attackMs_ = std::clamp(ms, 0.1f, 200.0f); }
    [[nodiscard]] float getAttack() const noexcept { return attackMs_; }

    void setRelease(float ms) noexcept { releaseMs_ = std::clamp(ms, 1.0f, 2000.0f); }
    [[nodiscard]] float getRelease() const noexcept { return releaseMs_; }

    void setMakeupGain(float db) noexcept { makeupGainDb_ = std::clamp(db, -18.0f, 36.0f); }
    [[nodiscard]] float getMakeupGain() const noexcept { return makeupGainDb_; }

    void setMix(float mix) noexcept { mix_ = std::clamp(mix, 0.0f, 1.0f); }
    [[nodiscard]] float getMix() const noexcept { return mix_; }

    void setRmsMode(bool rms) noexcept { isRms_ = rms; }
    [[nodiscard]] bool isRmsMode() const noexcept { return isRms_; }

    [[nodiscard]] float getCurrentGainReductionDb() const noexcept { return gainReductionDb_; }

    /**
     * Process stereo channels in-place.
     * Optional sidechain buffers drive the envelope detector if non-null.
     */
    void processStereo(float* bufferL, float* bufferR, size_t numFrames,
                       const float* scL = nullptr, const float* scR = nullptr) noexcept {
        if (!bufferL || !bufferR || numFrames == 0) return;

        const float attackCoeff = std::exp(-1.0f / (std::max(0.1f, attackMs_) * sampleRate_ / 1000.0f));
        const float releaseCoeff = std::exp(-1.0f / (std::max(1.0f, releaseMs_) * sampleRate_ / 1000.0f));
        const float makeupLin = std::pow(10.0f, makeupGainDb_ / 20.0f);
        const float halfKnee = kneeDb_ * 0.5f;

        for (size_t i = 0; i < numFrames; ++i) {
            const float dryL = bufferL[i];
            const float dryR = bufferR[i];

            // 1. Level Detection (Peak or RMS from sidechain or main input)
            const float detInL = scL ? scL[i] : dryL;
            const float detInR = scR ? scR[i] : dryR;
            const float detMax = std::max(std::abs(detInL), std::abs(detInR));

            float inputLevelDb;
            if (isRms_) {
                rmsSum_ += (detInL * detInL + detInR * detInR) * 0.5f;
                rmsCount_++;
                if (rmsCount_ >= rmsWindow_) {
                    float rms = std::sqrt(rmsSum_ / static_cast<float>(rmsCount_));
                    detectorDb_ = 20.0f * std::log10(std::max(1e-6f, rms));
                    rmsSum_ = 0.0f;
                    rmsCount_ = 0;
                }
                inputLevelDb = detectorDb_;
            } else {
                inputLevelDb = 20.0f * std::log10(std::max(1e-6f, detMax));
            }

            // 2. Static Characteristic with Soft-Knee Polynomial
            float targetDb = inputLevelDb;
            const float delta = inputLevelDb - thresholdDb_;

            if (kneeDb_ > 0.1f && std::abs(delta) <= halfKnee) {
                // Soft-knee transition range
                float kneeFactor = (delta + halfKnee) / kneeDb_;
                float cDb = thresholdDb_ + delta / ratio_;
                targetDb = inputLevelDb * (1.0f - kneeFactor) + cDb * kneeFactor;
            } else if (delta > 0.0f) {
                // Above knee: full ratio compression
                targetDb = thresholdDb_ + delta / ratio_;
            }

            const float desiredGrDb = targetDb - inputLevelDb; // Always <= 0.0

            // 3. Ballistics (Attack / Release filter on gain reduction in dB)
            if (desiredGrDb < gainReductionDb_) {
                // Signal increasing / compressing: Attack
                gainReductionDb_ = attackCoeff * gainReductionDb_ + (1.0f - attackCoeff) * desiredGrDb;
            } else {
                // Signal decaying / returning to 0dB: Release
                gainReductionDb_ = releaseCoeff * gainReductionDb_ + (1.0f - releaseCoeff) * desiredGrDb;
            }

            // 4. Linear Gain Multiplication & Makeup
            const float gainLin = std::pow(10.0f, gainReductionDb_ / 20.0f) * makeupLin;
            const float wetL = dryL * gainLin;
            const float wetR = dryR * gainLin;

            // 5. Dry / Wet Parallel Mix
            bufferL[i] = std::clamp(dryL * (1.0f - mix_) + wetL * mix_, -2.5f, 2.5f);
            bufferR[i] = std::clamp(dryR * (1.0f - mix_) + wetR * mix_, -2.5f, 2.5f);
        }
    }

private:
    float sampleRate_ = 44100.0f;
    float thresholdDb_ = -18.0f;
    float ratio_ = 4.0f;
    float kneeDb_ = 6.0f;
    float attackMs_ = 15.0f;
    float releaseMs_ = 100.0f;
    float makeupGainDb_ = 0.0f;
    float mix_ = 1.0f;
    bool isRms_ = false;

    float detectorDb_ = -96.0f;
    float gainReductionDb_ = 0.0f;
    float rmsSum_ = 0.0f;
    size_t rmsCount_ = 0;
    size_t rmsWindow_ = 441;
};

/**
 * Zero-Overshoot Brickwall Peak Limiter with pre-allocated circular lookahead delay buffer.
 * Guarantees zero transient overshoot past the configured ceiling.
 */
class LimiterCore {
public:
    static constexpr size_t kMaxLookaheadSamples = 1024; // Supports up to >20ms at 48kHz

    explicit LimiterCore(float sampleRate = 44100.0f, float lookaheadMs = 2.0f)
        : sampleRate_(sampleRate), lookaheadMs_(lookaheadMs) {
        initBuffer();
    }

    void setSampleRate(float sr) noexcept {
        sampleRate_ = sr;
        initBuffer();
    }

    void reset() noexcept {
        std::fill(delayBufL_.begin(), delayBufL_.end(), 0.0f);
        std::fill(delayBufR_.begin(), delayBufR_.end(), 0.0f);
        writeIdx_ = 0;
        gainReduction_ = 1.0f;
    }

    void setCeiling(float db) noexcept { ceilingDb_ = std::clamp(db, -18.0f, 0.0f); }
    [[nodiscard]] float getCeiling() const noexcept { return ceilingDb_; }

    void setCeilingDb(float db) noexcept { setCeiling(db); }
    [[nodiscard]] float getCeilingDb() const noexcept { return getCeiling(); }

    void setRelease(float ms) noexcept { releaseMs_ = std::clamp(ms, 1.0f, 1000.0f); }
    [[nodiscard]] float getRelease() const noexcept { return releaseMs_; }

    void setLookaheadMs(float ms) noexcept {
        lookaheadMs_ = std::clamp(ms, 0.1f, 20.0f);
        initBuffer();
    }
    [[nodiscard]] float getLookaheadMs() const noexcept { return lookaheadMs_; }

    [[nodiscard]] float getCurrentGainReductionLinear() const noexcept { return gainReduction_; }

    void processStereo(float* bufferL, float* bufferR, size_t numFrames) noexcept {
        if (!bufferL || !bufferR || numFrames == 0) return;

        const float ceilingLin = std::clamp(std::pow(10.0f, ceilingDb_ / 20.0f), 0.01f, 1.0f);
        const float releaseCoeff = std::exp(-1.0f / (std::max(1.0f, releaseMs_) * sampleRate_ / 1000.0f));

        for (size_t i = 0; i < numFrames; ++i) {
            const float inL = bufferL[i];
            const float inR = bufferR[i];

            // 1. Read delayed sample from lookahead ring buffer
            const size_t readIdx = (writeIdx_ + 1) % delayLength_;
            const float delayedL = delayBufL_[readIdx];
            const float delayedR = delayBufR_[readIdx];

            // 2. Store incoming sample into lookahead ring buffer
            delayBufL_[writeIdx_] = inL;
            delayBufR_[writeIdx_] = inR;
            writeIdx_ = (writeIdx_ + 1) % delayLength_;

            // 3. Lookahead Peak Detection
            const float peak = std::max(std::abs(inL), std::abs(inR));
            float targetGain = 1.0f;
            if (peak > ceilingLin) {
                targetGain = ceilingLin / peak;
            }

            // 4. Instant Attack / Exponential Release Ballistics
            if (targetGain < gainReduction_) {
                gainReduction_ = targetGain; // Instant lookahead attack
            } else {
                gainReduction_ = releaseCoeff * gainReduction_ + (1.0f - releaseCoeff) * targetGain;
            }

            // 5. Apply lookahead gain to delayed sample with brickwall clamp
            bufferL[i] = std::clamp(delayedL * gainReduction_, -ceilingLin, ceilingLin);
            bufferR[i] = std::clamp(delayedR * gainReduction_, -ceilingLin, ceilingLin);
        }
    }

private:
    void initBuffer() noexcept {
        delayLength_ = std::clamp(static_cast<size_t>(sampleRate_ * (lookaheadMs_ / 1000.0f)), static_cast<size_t>(8), kMaxLookaheadSamples);
        reset();
    }

    float sampleRate_ = 44100.0f;
    float ceilingDb_ = -0.1f;
    float releaseMs_ = 50.0f;
    float lookaheadMs_ = 2.0f;

    size_t delayLength_ = 88;
    size_t writeIdx_ = 0;
    float gainReduction_ = 1.0f;

    std::array<float, kMaxLookaheadSamples> delayBufL_{};
    std::array<float, kMaxLookaheadSamples> delayBufR_{};
};

} // namespace eatsbits::dsp
