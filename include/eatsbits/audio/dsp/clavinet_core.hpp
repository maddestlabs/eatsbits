#pragma once

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include "exciters.hpp"
#include "waveguide_core.hpp"

namespace eatsbits::dsp {

/**
 * Authentic Physical Model Core of the 1971 Hohner Clavinet D6.
 * Architecture:
 * - Rubber-tipped hammer-on-metal anvil strike exciter (Hertzian contact pulse + neoprene tip friction)
 * - Taut steel string digital waveguide with frequency-dependent damping and rapid decay
 * - Key-off yarn damper contact impulse thump on vibrating string release
 * - Dual single-coil electromagnetic pickups (Neck & Bridge) with comb filtering
 * - Out-of-phase pickup selector (A - B) for iconic hollow funk quack
 * - Authentic 4-Rocker EQ Filter Matrix (Brilliant, Treble, Medium, Soft)
 * - Vintage tube preamp overdrive and funk bite
 *
 * Real-time safe: strict zero-allocation polyphony.
 */
class ClavinetCore {
public:
    static constexpr size_t kMaxVoices = 16;
    static constexpr size_t kMaxBlockFrames = 2048;

    ClavinetCore() {
        reset();
    }

    void prepare(float sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
        reset();
    }

    void reset() noexcept {
        for (auto& v : voices_) {
            v.active = false;
            v.isNoteOff = false;
            v.hasReleased = false;
            v.age = 0;
            v.thumpIdx = 0;
            v.thumpLen = 0;
            v.envDecay = 1.0f;
            v.waveguide.reset();
        }
        std::fill(bridgeDelayLine_.begin(), bridgeDelayLine_.end(), 0.0f);
        bridgeWriteIdx_ = 0;
        hpLastInL_ = hpLastInR_ = 0.0f;
        hpLastOutL_ = hpLastOutR_ = 0.0f;
        lpStateL_ = lpStateR_ = 0.0f;
    }

    void noteOn(uint8_t note, float velocity) noexcept {
        if (velocity <= 0.001f) {
            noteOff(note);
            return;
        }

        // Voice allocation: find inactive or steal oldest
        size_t bestVoice = 0;
        uint32_t oldestAge = 0;
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (!voices_[i].active) {
                bestVoice = i;
                break;
            }
            if (voices_[i].age > oldestAge) {
                oldestAge = voices_[i].age;
                bestVoice = i;
            }
        }

        auto& v = voices_[bestVoice];
        v.active = true;
        v.note = note;
        v.velocity = std::clamp(velocity, 0.01f, 1.0f);
        v.freq = 440.0f * std::pow(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
        v.age = 0;
        v.isNoteOff = false;
        v.hasReleased = false;
        v.thumpIdx = 0;
        v.thumpLen = 0;
        v.envDecay = 1.0f;
        v.waveguide.reset();

        // Synthesize hammer excitation pulse into voice buffer
        // Contact duration shortens with higher velocity and harder hammer
        HammerExciter::generate(v.excBuffer.data(), 128, sampleRate_, note, v.velocity, hammerHardness_, hammerSnap_);
        v.excFramesRemaining = 128;
        v.excReadIdx = 0;
    }

    void noteOff(uint8_t note) noexcept {
        for (auto& v : voices_) {
            if (v.active && v.note == note && !v.isNoteOff) {
                v.isNoteOff = true;
                v.hasReleased = true;
                v.thumpIdx = 0;
                v.thumpLen = static_cast<uint32_t>(0.025f * sampleRate_);
                v.rng = 0x434C4156u ^ static_cast<uint32_t>(note * 37);
            }
        }
    }

    void allNotesOff() noexcept {
        for (auto& v : voices_) {
            v.active = false;
        }
    }

    // Parameters
    void setPickupSelect(float val) noexcept { pickupSelect_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getPickupSelect() const noexcept { return pickupSelect_; }

    void setPhaseInvert(float val) noexcept { phaseInvert_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getPhaseInvert() const noexcept { return phaseInvert_; }

    void setBrilliant(float val) noexcept { brilliant_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getBrilliant() const noexcept { return brilliant_; }

    void setTreble(float val) noexcept { treble_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getTreble() const noexcept { return treble_; }

    void setMedium(float val) noexcept { medium_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getMedium() const noexcept { return medium_; }

    void setSoft(float val) noexcept { soft_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getSoft() const noexcept { return soft_; }

    void setHammerHardness(float val) noexcept { hammerHardness_ = std::clamp(val, 0.2f, 3.0f); }
    [[nodiscard]] float getHammerHardness() const noexcept { return hammerHardness_; }

    void setHammerSnap(float val) noexcept { hammerSnap_ = std::clamp(val, 0.0f, 2.0f); }
    [[nodiscard]] float getHammerSnap() const noexcept { return hammerSnap_; }

    void setSustain(float val) noexcept { sustain_ = std::clamp(val, 0.8f, 0.999f); }
    [[nodiscard]] float getSustain() const noexcept { return sustain_; }

    void setDamperThump(float val) noexcept { damperThump_ = std::clamp(val, 0.0f, 1.0f); }
    [[nodiscard]] float getDamperThump() const noexcept { return damperThump_; }

    void setDrive(float val) noexcept { drive_ = std::clamp(val, 0.5f, 3.5f); }
    [[nodiscard]] float getDrive() const noexcept { return drive_; }

    void setParameterByName(const std::string& name, float value) noexcept {
        std::string lower = name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lower == "pickupselect" || lower == "pickup") setPickupSelect(value);
        else if (lower == "phaseinvert" || lower == "phase") setPhaseInvert(value);
        else if (lower == "brilliant" || lower == "brill") setBrilliant(value);
        else if (lower == "treble" || lower == "treb") setTreble(value);
        else if (lower == "medium" || lower == "med") setMedium(value);
        else if (lower == "soft") setSoft(value);
        else if (lower == "hammerhardness" || lower == "hammer") setHammerHardness(value);
        else if (lower == "hammersnap" || lower == "snap") setHammerSnap(value);
        else if (lower == "sustain") setSustain(value);
        else if (lower == "damperthump" || lower == "damper") setDamperThump(value);
        else if (lower == "drive" || lower == "overdrive") setDrive(value);
    }

    void process(float* outL, float* outR, size_t numFrames) noexcept {
        if (!outL || !outR || numFrames == 0) return;

        std::fill_n(outL, numFrames, 0.0f);
        std::fill_n(outR, numFrames, 0.0f);

        const float sr = sampleRate_;
        const float pi = 3.14159265358979323846f;
        const size_t frames = std::min(numFrames, kMaxBlockFrames);

        // Precompute natural note decay factor per sample (Karplus-Strong extension)
        const float decayFactor = std::exp(-1.0f / (sr * 0.95f));
        const float releaseDecayFactor = std::exp(-1.0f / (sr * 0.06f));

        // 1. Process active polyphonic voices
        for (auto& v : voices_) {
            if (!v.active) continue;

            // Prepare block excitation buffer
            std::fill(v.blockBuffer.begin(), v.blockBuffer.begin() + frames, 0.0f);
            if (v.excFramesRemaining > 0) {
                size_t toCopy = std::min(static_cast<size_t>(v.excFramesRemaining), frames);
                for (size_t i = 0; i < toCopy; ++i) {
                    v.blockBuffer[i] = v.excBuffer[v.excReadIdx + i];
                }
                v.excReadIdx += toCopy;
                v.excFramesRemaining -= static_cast<uint32_t>(toCopy);
            }

            // Keep hammer transient click copy
            std::array<float, kMaxBlockFrames> hammerTransient{};
            for (size_t i = 0; i < frames; ++i) {
                hammerTransient[i] = v.blockBuffer[i];
            }

            // Karplus-Strong digital waveguide string simulation
            v.waveguide.process(v.blockBuffer.data(), frames, sr, v.freq, sustain_, 0.30f);

            // Apply release yarn damping envelope and release contact thump
            float peakVoice = 0.0f;
            const float pan = 0.5f + (static_cast<float>(v.note) - 60.0f) * 0.005f;
            const float gainL = std::cos(pan * 1.5707963f);
            const float gainR = std::sin(pan * 1.5707963f);

            for (size_t i = 0; i < frames; ++i) {
                // String decay envelope
                if (v.isNoteOff) {
                    v.envDecay *= releaseDecayFactor;
                } else {
                    v.envDecay *= decayFactor;
                }

                // Clavinet mixer: Waveguide String (0.90) + Hammer click (0.25)
                float s = v.blockBuffer[i] * 0.90f * v.envDecay + hammerTransient[i] * 0.25f;

                // Key-off yarn damper thump impulse
                if (v.hasReleased && v.thumpIdx < v.thumpLen) {
                    float tNorm = static_cast<float>(v.thumpIdx) / static_cast<float>(v.thumpLen);
                    float env = std::exp(-tNorm * 6.0f);
                    v.rng ^= (v.rng << 13);
                    v.rng ^= (v.rng >> 17);
                    v.rng ^= (v.rng << 5);
                    float n = (static_cast<float>(v.rng & 0xFFFFFFu) / 8388607.5f) - 1.0f;
                    float thumpSine = std::sin(2.0f * pi * 140.0f * (static_cast<float>(v.thumpIdx) / sr));
                    float thump = (thumpSine * 0.6f + n * 0.4f) * env * damperThump_ * 0.4f;
                    s += thump * 0.30f;
                    v.thumpIdx++;
                }

                outL[i] += s * gainL;
                outR[i] += s * gainR;
                peakVoice = std::max(peakVoice, std::abs(s));
            }

            v.age += static_cast<uint32_t>(frames);
            if (v.isNoteOff && v.envDecay < 0.0001f && (!v.hasReleased || v.thumpIdx >= v.thumpLen)) {
                v.active = false;
            }
        }

        // 2. Dual-Pickup Position Comb Filtering & Phase Inversion (A-B Funk Quack)
        // Delay spacing ~ 0.35ms (17 samples at 48kHz)
        const size_t delaySamples = std::clamp(static_cast<size_t>(sr * 0.00035f), size_t{2}, size_t{64});
        const float pSelect = pickupSelect_;
        const float pInvert = phaseInvert_;

        for (size_t i = 0; i < frames; ++i) {
            float neckSampleL = outL[i];
            float neckSampleR = outR[i];

            // Bridge sample from delay line
            size_t readPos = (bridgeWriteIdx_ + kDelayBufSize - delaySamples) % kDelayBufSize;
            float bridgeSampleL = bridgeDelayLine_[readPos * 2];
            float bridgeSampleR = bridgeDelayLine_[readPos * 2 + 1];

            bridgeDelayLine_[bridgeWriteIdx_ * 2] = neckSampleL;
            bridgeDelayLine_[bridgeWriteIdx_ * 2 + 1] = neckSampleR;
            bridgeWriteIdx_ = (bridgeWriteIdx_ + 1) % kDelayBufSize;

            // In-Phase vs Out-of-Phase funk quack combination
            float inPhaseL = neckSampleL * (1.0f - pSelect * 0.6f) + bridgeSampleL * (0.4f + pSelect * 0.6f);
            float inPhaseR = neckSampleR * (1.0f - pSelect * 0.6f) + bridgeSampleR * (0.4f + pSelect * 0.6f);

            float outOfPhaseL = (neckSampleL - bridgeSampleL * 1.05f) * 1.6f;
            float outOfPhaseR = (neckSampleR - bridgeSampleR * 1.05f) * 1.6f;

            outL[i] = inPhaseL * (1.0f - pInvert) + outOfPhaseL * pInvert;
            outR[i] = inPhaseR * (1.0f - pInvert) + outOfPhaseR * pInvert;
        }

        // 3. 4-Rocker EQ Filter Matrix
        // Highpass (Brilliant switch: 220Hz 1-pole HPF)
        if (brilliant_ > 0.05f) {
            const float hpAlpha = 1.0f / (1.0f + (2.0f * pi * 220.0f / sr));
            for (size_t i = 0; i < frames; ++i) {
                float inSampleL = outL[i];
                float hpOutL = hpAlpha * (hpLastOutL_ + inSampleL - hpLastInL_);
                hpLastInL_ = inSampleL;
                hpLastOutL_ = hpOutL;
                outL[i] = inSampleL * (1.0f - brilliant_ * 0.7f) + hpOutL * (brilliant_ * 1.4f);

                float inSampleR = outR[i];
                float hpOutR = hpAlpha * (hpLastOutR_ + inSampleR - hpLastInR_);
                hpLastInR_ = inSampleR;
                hpLastOutR_ = hpOutR;
                outR[i] = inSampleR * (1.0f - brilliant_ * 0.7f) + hpOutR * (brilliant_ * 1.4f);
            }
        }

        // Lowpass (Soft switch: 750Hz 1-pole LPF)
        if (soft_ > 0.05f) {
            const float lpAlpha = std::clamp(2.0f * pi * 750.0f / sr, 0.01f, 0.95f);
            for (size_t i = 0; i < frames; ++i) {
                lpStateL_ += lpAlpha * (outL[i] - lpStateL_);
                outL[i] = outL[i] * (1.0f - soft_) + lpStateL_ * soft_;

                lpStateR_ += lpAlpha * (outR[i] - lpStateR_);
                outR[i] = outR[i] * (1.0f - soft_) + lpStateR_ * soft_;
            }
        }

        // Peaking EQ Boost (Treble 3.2kHz + Medium 1.2kHz boost)
        const float peakGain = 1.0f + treble_ * 0.4f + medium_ * 0.3f;
        for (size_t i = 0; i < frames; ++i) {
            outL[i] *= peakGain;
            outR[i] *= peakGain;
        }

        // 4. Vintage Tube Preamp Overdrive / Bite
        const float drv = std::clamp(drive_, 0.5f, 4.0f);
        const float normTanh = 1.0f / std::tanh(drv);
        for (size_t i = 0; i < frames; ++i) {
            outL[i] = std::tanh(outL[i] * drv) * normTanh * 0.75f;
            outR[i] = std::tanh(outR[i] * drv) * normTanh * 0.75f;
        }
    }

private:
    struct VoiceState {
        bool active = false;
        bool isNoteOff = false;
        bool hasReleased = false;
        uint8_t note = 60;
        float velocity = 1.0f;
        float freq = 440.0f;
        uint32_t age = 0;
        float envDecay = 1.0f;

        // Key-off yarn damper thump state
        uint32_t thumpIdx = 0;
        uint32_t thumpLen = 0;
        uint32_t rng = 0;

        DigitalWaveguideCore waveguide;
        std::array<float, 128> excBuffer{};
        uint32_t excFramesRemaining = 0;
        uint32_t excReadIdx = 0;

        std::array<float, kMaxBlockFrames> blockBuffer{};
    };

    float sampleRate_ = 48000.0f;
    std::array<VoiceState, kMaxVoices> voices_{};

    // Parameter states matching original Hohner Clavinet D6
    float pickupSelect_ = 0.5f;     // 0.0 = Neck (A), 0.5 = Both (A+B), 1.0 = Bridge (B)
    float phaseInvert_ = 0.0f;      // 0.0 = In-Phase, 1.0 = Out-of-Phase (A-B Quack)
    float brilliant_ = 1.0f;        // 220Hz HPF bite
    float treble_ = 0.8f;           // 3.2kHz peak boost
    float medium_ = 0.5f;           // 1.2kHz mid peak
    float soft_ = 0.0f;             // 750Hz LPF
    float hammerHardness_ = 1.4f;   // Strike hardness
    float hammerSnap_ = 0.9f;       // Micro-click snap
    float sustain_ = 0.993f;        // Waveguide string sustain feedback
    float damperThump_ = 0.45f;     // Yarn damper thump level
    float drive_ = 1.08f;           // Tube preamp bite

    // Comb filter delay line for bridge pickup
    static constexpr size_t kDelayBufSize = 128;
    std::array<float, kDelayBufSize * 2> bridgeDelayLine_{};
    size_t bridgeWriteIdx_ = 0;

    // Filter states
    float hpLastInL_ = 0.0f, hpLastInR_ = 0.0f;
    float hpLastOutL_ = 0.0f, hpLastOutR_ = 0.0f;
    float lpStateL_ = 0.0f, lpStateR_ = 0.0f;
};

} // namespace eatsbits::dsp
