#ifndef EATS_DRUM_SYNTHS_HPP
#define EATS_DRUM_SYNTHS_HPP

#include <cmath>
#include <numbers>
#include <array>
#include <algorithm>
#include <cstdint>
#include "biquad.hpp"

namespace eatsbits::dsp {

/**
 * Fast real-time safe linear congruential pseudo-random noise generator.
 */
class FastNoise {
public:
    inline float next() noexcept {
        seed_ = (seed_ * 1103515245u + 12345u) & 0x7FFFFFFFu;
        return (static_cast<float>(seed_) / 1073741824.0f) - 1.0f; // -1.0 to +1.0
    }
private:
    uint32_t seed_{0x1A2B3C4Du};
};

/**
 * Authentic Roland TR-808 Bass Drum Synthesizer.
 * Bridged-T resonant network with exponential pitch sweep and soft saturation.
 */
class Analog808Kick {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        reset();
    }

    void reset() noexcept {
        phase_ = 0.0f;
        currentFreq_ = baseFreq_;
        ampEnv_ = 0.0f;
        pitchEnv_ = 0.0f;
        active_ = false;
    }

    void trigger(float velocity, float tune = 50.0f, float decay = 0.45f, float punch = 1.0f, float drive = 0.25f) noexcept {
        velocity_ = std::clamp(velocity, 0.05f, 1.0f);
        baseFreq_ = std::clamp(tune, 30.0f, 120.0f);
        decayTime_ = std::clamp(decay, 0.05f, 2.5f);
        punch_ = std::clamp(punch, 0.0f, 2.0f);
        drive_ = std::clamp(drive, 0.0f, 1.0f);

        ampEnv_ = velocity_;
        pitchEnv_ = 1.0f;
        currentFreq_ = baseFreq_ + 140.0f * punch_;
        phase_ = 0.0f;
        active_ = true;

        // Exponential decay coefficients per sample
        ampDecayCoeff_ = std::exp(-1.0f / (decayTime_ * sampleRate_));
        pitchDecayCoeff_ = std::exp(-1.0f / (0.035f * sampleRate_)); // Fast pitch drop (~35ms)
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        // Advance pitch envelope
        pitchEnv_ *= pitchDecayCoeff_;
        currentFreq_ = baseFreq_ + 140.0f * punch_ * pitchEnv_;

        // Advance oscillator phase
        phase_ += currentFreq_ / sampleRate_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        // Sine waveform
        const float osc = std::sin(phase_ * 2.0f * static_cast<float>(std::numbers::pi));

        // Advance amplitude envelope
        ampEnv_ *= ampDecayCoeff_;
        if (ampEnv_ < 0.002f) {
            active_ = false;
            return 0.0f;
        }

        float out = osc * ampEnv_;

        // Soft-clipping saturation / overdrive
        if (drive_ > 0.01f) {
            const float driven = out * (1.0f + drive_ * 3.0f);
            out = std::tanh(driven);
        }

        return out;
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }

private:
    float sampleRate_{48000.0f};
    float phase_{0.0f};
    float baseFreq_{50.0f};
    float currentFreq_{50.0f};
    float velocity_{1.0f};
    float decayTime_{0.45f};
    float punch_{1.0f};
    float drive_{0.25f};

    float ampEnv_{0.0f};
    float pitchEnv_{0.0f};
    float ampDecayCoeff_{0.999f};
    float pitchDecayCoeff_{0.99f};
    bool active_{false};
};

/**
 * Authentic Roland TR-909 Bass Drum Synthesizer.
 * Punchy state-variable swept pitch with transient beater click and heavy sub-weight.
 */
class Analog909Kick {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        reset();
    }

    void reset() noexcept {
        phase_ = 0.0f;
        ampEnv_ = 0.0f;
        pitchEnv_ = 0.0f;
        clickEnv_ = 0.0f;
        active_ = false;
    }

    void trigger(float velocity, float tune = 54.0f, float attack = 1.0f, float decay = 0.38f, float drive = 0.35f) noexcept {
        velocity_ = std::clamp(velocity, 0.05f, 1.0f);
        baseFreq_ = std::clamp(tune, 35.0f, 130.0f);
        decayTime_ = std::clamp(decay, 0.05f, 2.0f);
        attack_ = std::clamp(attack, 0.0f, 2.0f);
        drive_ = std::clamp(drive, 0.0f, 1.0f);

        ampEnv_ = velocity_;
        pitchEnv_ = 1.0f;
        clickEnv_ = 1.0f;
        currentFreq_ = baseFreq_ + 220.0f * attack_;
        phase_ = 0.0f;
        active_ = true;

        ampDecayCoeff_ = std::exp(-1.0f / (decayTime_ * sampleRate_));
        pitchDecayCoeff_ = std::exp(-1.0f / (0.045f * sampleRate_));
        clickDecayCoeff_ = std::exp(-1.0f / (0.005f * sampleRate_)); // 5ms attack click
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        pitchEnv_ *= pitchDecayCoeff_;
        currentFreq_ = baseFreq_ + 220.0f * attack_ * pitchEnv_;

        phase_ += currentFreq_ / sampleRate_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        const float osc = std::sin(phase_ * 2.0f * static_cast<float>(std::numbers::pi));

        // Attack beater click impulse
        clickEnv_ *= clickDecayCoeff_;
        const float click = (noise_.next() * 0.4f + 0.6f) * clickEnv_ * 0.35f;

        ampEnv_ *= ampDecayCoeff_;
        if (ampEnv_ < 0.0005f) {
            active_ = false;
            return 0.0f;
        }

        float out = (osc + click) * ampEnv_;

        // 909 analog diode drive
        const float gain = 1.0f + drive_ * 4.0f;
        out = std::tanh(out * gain);
        return out;
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }

private:
    float sampleRate_{48000.0f};
    float phase_{0.0f};
    float baseFreq_{54.0f};
    float currentFreq_{54.0f};
    float velocity_{1.0f};
    float decayTime_{0.38f};
    float attack_{1.0f};
    float drive_{0.35f};

    float ampEnv_{0.0f};
    float pitchEnv_{0.0f};
    float clickEnv_{0.0f};
    float ampDecayCoeff_{0.999f};
    float pitchDecayCoeff_{0.99f};
    float clickDecayCoeff_{0.95f};
    bool active_{false};

    FastNoise noise_;
};

/**
 * Authentic Roland TR-808 Snare Drum Synthesizer.
 * Dual resonant tone oscillators (shell resonance) + filtered noise burst (snappy wires).
 */
class Analog808Snare {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        filter_.configure(BiquadType::BandPass, 2200.0f, 1.8f, 0.0f, sampleRate_);
        reset();
    }

    void reset() noexcept {
        phase1_ = 0.0f;
        phase2_ = 0.0f;
        bodyEnv_ = 0.0f;
        snappyEnv_ = 0.0f;
        filter_.reset();
        active_ = false;
    }

    void trigger(float velocity, float snappy = 0.65f, float tone = 200.0f, float decay = 0.22f) noexcept {
        velocity_ = std::clamp(velocity, 0.05f, 1.0f);
        snappy_ = std::clamp(snappy, 0.0f, 1.0f);
        toneFreq_ = std::clamp(tone, 100.0f, 400.0f);
        decayTime_ = std::clamp(decay, 0.05f, 1.0f);

        bodyEnv_ = velocity_;
        snappyEnv_ = velocity_ * snappy_;
        phase1_ = 0.0f;
        phase2_ = 0.0f;
        active_ = true;

        bodyDecayCoeff_ = std::exp(-1.0f / (decayTime_ * 0.6f * sampleRate_));
        snappyDecayCoeff_ = std::exp(-1.0f / (decayTime_ * sampleRate_));
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        // Dual shell tone oscillators (e.g. 180Hz and 330Hz)
        phase1_ += toneFreq_ / sampleRate_;
        if (phase1_ >= 1.0f) phase1_ -= 1.0f;
        phase2_ += (toneFreq_ * 1.65f) / sampleRate_;
        if (phase2_ >= 1.0f) phase2_ -= 1.0f;

        const float body = (std::sin(phase1_ * 2.0f * static_cast<float>(std::numbers::pi)) * 0.6f +
                            std::sin(phase2_ * 2.0f * static_cast<float>(std::numbers::pi)) * 0.4f) * bodyEnv_;

        // Snappy wire noise burst through bandpass filter
        const float rawNoise = noise_.next();
        const float filteredNoise = filter_.process(rawNoise) * snappyEnv_;

        bodyEnv_ *= bodyDecayCoeff_;
        snappyEnv_ *= snappyDecayCoeff_;

        if (bodyEnv_ < 0.0005f && snappyEnv_ < 0.0005f) {
            active_ = false;
            return 0.0f;
        }

        return std::clamp(body * 0.6f + filteredNoise * 0.8f, -1.0f, 1.0f);
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }

private:
    float sampleRate_{48000.0f};
    float phase1_{0.0f};
    float phase2_{0.0f};
    float toneFreq_{200.0f};
    float snappy_{0.65f};
    float decayTime_{0.22f};
    float velocity_{1.0f};

    float bodyEnv_{0.0f};
    float snappyEnv_{0.0f};
    float bodyDecayCoeff_{0.999f};
    float snappyDecayCoeff_{0.999f};
    bool active_{false};

    FastNoise noise_;
    BiquadFilter filter_;
};

/**
 * Authentic Roland TR-808 Hi-Hat Synthesizer.
 * 6 inharmonic square wave oscillators + HighPass filter with choke logic.
 */
class Analog808HiHat {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        filter_.configure(BiquadType::HighPass, 7500.0f, 2.5f, 0.0f, sampleRate_);
        reset();
    }

    void reset() noexcept {
        phases_.fill(0.0f);
        ampEnv_ = 0.0f;
        filter_.reset();
        active_ = false;
    }

    void trigger(float velocity, bool open = false, float decay = 0.0f) noexcept {
        velocity_ = std::clamp(velocity, 0.05f, 1.0f);
        isOpen_ = open;

        float effectiveDecay = decay > 0.0f ? decay : (open ? 0.45f : 0.065f);
        ampEnv_ = velocity_;
        active_ = true;

        decayCoeff_ = std::exp(-1.0f / (effectiveDecay * sampleRate_));
    }

    // Choke: instantly suppresses ringing when closed hat is triggered
    void choke() noexcept {
        if (active_ && isOpen_) {
            decayCoeff_ = std::exp(-1.0f / (0.015f * sampleRate_)); // Fast 15ms clamp
        }
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        // 6 inharmonic square wave frequencies: 245, 306, 384, 423, 532, 622 Hz
        static constexpr std::array<float, 6> freqs = {245.0f, 306.0f, 384.0f, 423.0f, 532.0f, 622.0f};

        float cluster = 0.0f;
        for (size_t i = 0; i < 6; ++i) {
            phases_[i] += freqs[i] / sampleRate_;
            if (phases_[i] >= 1.0f) phases_[i] -= 1.0f;
            cluster += (phases_[i] < 0.5f) ? 1.0f : -1.0f;
        }
        cluster *= (1.0f / 6.0f);

        // High-pass filter cluster to extract metallic sizzle
        const float filtered = filter_.process(cluster);

        ampEnv_ *= decayCoeff_;
        if (ampEnv_ < 0.0005f) {
            active_ = false;
            return 0.0f;
        }

        return filtered * ampEnv_;
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

private:
    float sampleRate_{48000.0f};
    std::array<float, 6> phases_{};
    float ampEnv_{0.0f};
    float decayCoeff_{0.999f};
    float velocity_{1.0f};
    bool isOpen_{false};
    bool active_{false};

    BiquadFilter filter_;
};

/**
 * Authentic Roland TR-808 Hand Clap Synthesizer.
 * 4 staggered micro-burst impulses spaced ~11ms apart followed by filtered reverb tail.
 */
class Analog808Clap {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        filter_.configure(BiquadType::BandPass, 1200.0f, 1.2f, 0.0f, sampleRate_);
        reset();
    }

    void reset() noexcept {
        sampleIndex_ = 0;
        tailEnv_ = 0.0f;
        filter_.reset();
        active_ = false;
    }

    void trigger(float velocity) noexcept {
        velocity_ = std::clamp(velocity, 0.05f, 1.0f);
        sampleIndex_ = 0;
        tailEnv_ = velocity_;
        active_ = true;
        tailDecayCoeff_ = std::exp(-1.0f / (0.28f * sampleRate_)); // 280ms tail
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        const float t = static_cast<float>(sampleIndex_) / sampleRate_;
        sampleIndex_++;

        float burstGain = 0.0f;
        // 4 burst impulses at t=0ms, 11ms, 22ms, 33ms
        static constexpr std::array<float, 4> burstTimes = {0.0f, 0.011f, 0.022f, 0.033f};
        for (float bt : burstTimes) {
            if (t >= bt && t < bt + 0.007f) { // 7ms burst pulse
                burstGain += 1.0f - ((t - bt) / 0.007f);
            }
        }

        // Long tail after the 4 bursts
        if (t >= 0.033f) {
            tailEnv_ *= tailDecayCoeff_;
            burstGain += tailEnv_ * 0.7f;
        }

        if (burstGain <= 0.0005f && t > 0.05f) {
            active_ = false;
            return 0.0f;
        }

        const float rawNoise = noise_.next();
        const float filtered = filter_.process(rawNoise);
        return filtered * burstGain * velocity_;
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }

private:
    float sampleRate_{48000.0f};
    uint32_t sampleIndex_{0};
    float velocity_{1.0f};
    float tailEnv_{0.0f};
    float tailDecayCoeff_{0.999f};
    bool active_{false};

    FastNoise noise_;
    BiquadFilter filter_;
};

/**
 * Authentic Roland TR-808 Cowbell Synthesizer.
 * Dual tuned square waves (540Hz & 800Hz) through bandpass filter.
 */
class Analog808Cowbell {
public:
    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        filter_.configure(BiquadType::BandPass, 800.0f, 2.0f, 0.0f, sampleRate_);
        reset();
    }

    void reset() noexcept {
        phase1_ = 0.0f;
        phase2_ = 0.0f;
        ampEnv_ = 0.0f;
        filter_.reset();
        active_ = false;
    }

    void trigger(float velocity) noexcept {
        velocity_ = std::clamp(velocity, 0.05f, 1.0f);
        ampEnv_ = velocity_;
        phase1_ = 0.0f;
        phase2_ = 0.0f;
        active_ = true;
        decayCoeff_ = std::exp(-1.0f / (0.25f * sampleRate_)); // 250ms decay
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        // 540Hz and 800Hz
        phase1_ += 540.0f / sampleRate_;
        if (phase1_ >= 1.0f) phase1_ -= 1.0f;
        phase2_ += 800.0f / sampleRate_;
        if (phase2_ >= 1.0f) phase2_ -= 1.0f;

        const float sq1 = (phase1_ < 0.5f) ? 1.0f : -1.0f;
        const float sq2 = (phase2_ < 0.5f) ? 1.0f : -1.0f;

        const float mixed = (sq1 + sq2) * 0.5f;
        const float filtered = filter_.process(mixed);

        ampEnv_ *= decayCoeff_;
        if (ampEnv_ < 0.0005f) {
            active_ = false;
            return 0.0f;
        }

        return filtered * ampEnv_;
    }

    [[nodiscard]] bool isActive() const noexcept { return active_; }

private:
    float sampleRate_{48000.0f};
    float phase1_{0.0f};
    float phase2_{0.0f};
    float ampEnv_{0.0f};
    float decayCoeff_{0.999f};
    float velocity_{1.0f};
    bool active_{false};

    BiquadFilter filter_;
};

} // namespace eatsbits::dsp

#endif // EATS_DRUM_SYNTHS_HPP
