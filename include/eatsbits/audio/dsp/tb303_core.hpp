#ifndef EATS_TB303_CORE_HPP
#define EATS_TB303_CORE_HPP

#include <cmath>
#include <numbers>
#include <algorithm>
#include <cstdint>

namespace eatsbits::dsp {

/**
 * Voice state and diode ladder filter registers for TB-303 synthesis.
 */
struct Tb303VoiceState {
    float phase{0.0f};
    float subPhase{0.0f};
    float lastFreq{0.0f};
    float startFreq{0.0f};
    float lastEnv{1.0f};
    float mainEnv{1.0f};
    float ampEnv{1.0f};
    float rc1{0.0f};
    float rc2{0.0f};
    float stage1{0.0f};
    float stage2{0.0f};
    float stage3{0.0f};
    float stage4{0.0f};
    float feedbackHpX1{0.0f};
    float feedbackHpY1{0.0f};
    float preHpX1{0.0f};
    float preHpY1{0.0f};
    float postHpX1{0.0f};
    float postHpY1{0.0f};

    void reset() noexcept {
        phase = 0.0f;
        subPhase = 0.0f;
        lastFreq = 0.0f;
        startFreq = 0.0f;
        lastEnv = 1.0f;
        mainEnv = 1.0f;
        ampEnv = 1.0f;
        rc1 = 0.0f;
        rc2 = 0.0f;
        stage1 = 0.0f;
        stage2 = 0.0f;
        stage3 = 0.0f;
        stage4 = 0.0f;
        feedbackHpX1 = 0.0f;
        feedbackHpY1 = 0.0f;
        preHpX1 = 0.0f;
        preHpY1 = 0.0f;
        postHpX1 = 0.0f;
        postHpY1 = 0.0f;
    }
};

/**
 * Authentic Roland TB-303 Acid Bassline Core.
 * Based on Robin Schmidt Open303 and Mystran diode ladder differential circuit equations.
 * Matches original Eatsbeats DSP implementation (pure linear stage coupling, full envelope sweep,
 * authentic accent punch, 60ms 1V/Oct semitone slide with configurable glide curves).
 * Completely allocation-free and real-time thread safe.
 */
class Tb303Core {
public:
    static constexpr float C0 = 313.8152786059267f;
    static constexpr float C1 = 2394.411986817546f;
    static constexpr float OF = 0.048292930943553f;
    static constexpr float OC = 0.294391201442418f;
    static constexpr float S_LO_F = 3.773996325111173f;
    static constexpr float S_LO_C = 0.736965594166206f;
    static constexpr float S_HI_F = 4.194548788411135f;
    static constexpr float S_HI_C = 0.864344900642434f;
    static constexpr int OVERSAMPLING = 4;

    Tb303Core() noexcept {
        reset();
        updateCoefficients();
        updateDynamicParameters();
    }

    void setSampleRate(float sampleRate) noexcept {
        sampleRate_ = (sampleRate > 0.0f) ? sampleRate : 48000.0f;
        filterRate_ = sampleRate_ * static_cast<float>(OVERSAMPLING);
        updateCoefficients();
        updateDynamicParameters();
    }

    void reset() noexcept {
        state_.reset();
        active_ = false;
        gate_ = false;
        sampleIndex_ = 0;
    }

    void noteOn(uint8_t midiNote, float velocity, bool isSlide = false, bool isAccent = false, int fromMidiNote = -1) noexcept {
        active_ = true;
        gate_ = true;
        midiNote_ = midiNote;
        velocity_ = std::clamp(velocity, 0.0f, 1.0f);
        isSlide_ = isSlide;
        isAccent_ = isAccent || (velocity > 0.75f);
        sampleIndex_ = 0;

        const float noteFreq = 440.0f * std::pow(2.0f, (static_cast<float>(midiNote) - 69.0f) / 12.0f);
        targetFreq_ = noteFreq * std::pow(2.0f, static_cast<float>(octave_) + (tuning_ / 12.0f));

        // Retrigger envelopes on articulated note boundaries; preserve continuity for authentic 303 slides
        if (!isSlide) {
            state_.mainEnv = 1.0f;
            state_.ampEnv = 1.0f;
            state_.rc1 = 0.0f;
            state_.rc2 = 0.0f;
            state_.phase = 0.0f;
            state_.subPhase = 0.0f;

            if (fromMidiNote > 0) {
                state_.startFreq = 440.0f * std::pow(2.0f, ((static_cast<float>(fromMidiNote) + octave_ * 12.0f) - 69.0f + tuning_) / 12.0f);
            } else {
                state_.startFreq = targetFreq_;
            }
        } else {
            if (fromMidiNote > 0) {
                state_.startFreq = 440.0f * std::pow(2.0f, ((static_cast<float>(fromMidiNote) + octave_ * 12.0f) - 69.0f + tuning_) / 12.0f);
            } else {
                state_.startFreq = (state_.lastFreq > 0.0f) ? state_.lastFreq : targetFreq_;
            }
        }

        currentFreq_ = targetFreq_;
        updateDynamicParameters();
    }

    void noteOff() noexcept {
        gate_ = false;
    }

    // Parameter setters (normalized 0.0 .. 1.0 or raw values, matching Eatsbeats)
    void setCutoff(float cutoffHz) noexcept {
        cutoffHz_ = cutoffHz;
        updateDynamicParameters();
    }

    void setResonance(float res) noexcept {
        resonance_ = res;
        updateDynamicParameters();
    }

    void setEnvMod(float envNorm) noexcept {
        envMod_ = envNorm;
        updateDynamicParameters();
    }

    void setDecay(float decayNorm) noexcept {
        decayNorm_ = decayNorm;
        updateDynamicParameters();
    }

    void setAccent(float accentNorm) noexcept {
        accent_ = accentNorm;
        updateDynamicParameters();
    }

    void setOverdrive(float driveNorm) noexcept {
        drive_ = std::clamp(driveNorm, 0.0f, 1.0f);
        driveGain_ = (drive_ > 0.02f) ? (1.0f + drive_ * 3.5f) : 1.0f;
    }

    void setDrive(float driveNorm) noexcept {
        setOverdrive(driveNorm);
    }

    void setWaveform(float wave) noexcept { // 0.0 = Saw, 1.0 = Square
        waveform_ = std::clamp(wave, 0.0f, 1.0f);
    }

    void setTuning(float semitones) noexcept {
        tuning_ = std::clamp(semitones, -24.0f, 24.0f);
        updatePitch();
    }

    void setPitch(float semitones) noexcept {
        setTuning(semitones);
    }

    void setOctave(int octaves) noexcept {
        octave_ = std::clamp(octaves, -3, 3);
        updatePitch();
    }

    void setSubVolume(float vol) noexcept {
        subVolume_ = std::clamp(vol, 0.0f, 1.0f);
    }

    void setSubWaveform(float wave) noexcept { // 0.0 = Sine, 1.0 = Square
        subWaveform_ = std::clamp(wave, 0.0f, 1.0f);
    }

    void setGlideCurve(float curve) noexcept { // 0.0 = RC Exponential, 1.0 = Linear, 2.0 = S-Curve
        glideCurve_ = std::clamp(curve, 0.0f, 2.0f);
    }

    [[nodiscard]] inline float processSample() noexcept {
        if (!active_) return 0.0f;

        const float time = static_cast<float>(sampleIndex_) / sampleRate_;
        sampleIndex_++;

        // Pitch glide for slide step in 1V/Oct semitone domain (matching original Eatsbeats)
        float currentFreq = targetFreq_;
        if (isSlide_ || (state_.startFreq != targetFreq_)) {
            constexpr float glideTime = 0.060f; // Authentic 60ms TB-303 analog slide time
            const float progress = std::clamp(time / glideTime, 0.0f, 1.0f);
            float factor = 0.0f;
            if (glideCurve_ >= 1.9f) {
                // S-Curve (cosine ease-in / ease-out)
                factor = 0.5f * (1.0f + std::cos(static_cast<float>(std::numbers::pi) * progress));
            } else {
                // 0.0 = Exponential analog RC curve, 1.0 = Linear semitone slew
                const float expFactor = std::exp(-time / (glideTime * 0.35f));
                const float linFactor = 1.0f - progress;
                const float blend = std::clamp(glideCurve_, 0.0f, 1.0f);
                factor = (1.0f - blend) * expFactor + blend * linFactor;
            }

            // Semitone pitch domain interpolation for musical 1V/Oct glide across octaves
            const float startPitch = 69.0f + 12.0f * (std::log(state_.startFreq / 440.0f) / std::numbers::ln2_v<float>);
            const float targetPitch = 69.0f + 12.0f * (std::log(targetFreq_ / 440.0f) / std::numbers::ln2_v<float>);
            const float curPitch = targetPitch + (startPitch - targetPitch) * factor;
            currentFreq = 440.0f * std::pow(2.0f, (curPitch - 69.0f) / 12.0f);
        }
        state_.lastFreq = currentFreq;

        // Envelope stepping
        state_.mainEnv *= decayCoeff_;
        state_.lastEnv = state_.mainEnv;
        state_.rc1 = state_.mainEnv + rc1Coeff_ * (state_.rc1 - state_.mainEnv);

        const float tmp1 = envScaler_ * (state_.rc1 - envOffset_);
        float tmp2 = 0.0f;
        if (isAccent_) {
            state_.rc2 = state_.mainEnv + rc2Coeff_ * (state_.rc2 - state_.mainEnv);
            tmp2 = normAccent_ * state_.rc2;
        }

        const float instCutoff = std::clamp(nominalCutoff_ * std::pow(2.0f, tmp1 + tmp2), 60.0f, 18000.0f);

        // Mystran & Kunn Diode Ladder coefficient calculations
        const float wc = 2.0f * static_cast<float>(std::numbers::pi) * instCutoff / filterRate_;
        const float fx = wc * (1.0f / static_cast<float>(std::numbers::sqrt2)) / (2.0f * static_cast<float>(std::numbers::pi));
        const float b0 = (0.00045522346f + 6.1922189f * fx) /
                         (1.0f + 12.358354f * fx + 4.4156345f * (fx * fx));
        float k = fx *
                  (fx *
                          (fx *
                                  (fx *
                                          (fx * (fx + 7198.6997f) - 5837.7917f) -
                                      476.47308f) +
                              614.95611f) +
                      213.87126f) +
                  16.998792f;
        float g = k * (1.0f / 17.0f);
        g = (g - 1.0f) * r_ + 1.0f;
        g = g * (1.0f + r_);
        k = k * r_;

        // Amp envelope
        if (gate_ || isSlide_) {
            state_.ampEnv *= ampDecayCoeff_;
        } else {
            state_.ampEnv *= releaseDecayCoeff_;
        }
        const float totalAmp = state_.ampEnv + 0.45f * state_.mainEnv + (isAccent_ ? 2.5f * normAccent_ * state_.mainEnv : 0.0f);

        // 4x Oversampled diode ladder loop (pure linear coupled states per Robin Schmidt / Mystran)
        const float phaseInc = currentFreq / filterRate_;
        float filtered = 0.0f;

        for (int step = 0; step < OVERSAMPLING; ++step) {
            state_.phase += phaseInc;
            if (state_.phase >= 1.0f) state_.phase -= 1.0f;
            const float normPhase = state_.phase;

            // 303 Saw: Inverted ramp wave
            const float sawRaw = 2.0f * normPhase - 1.0f;
            // 303 Square: Asymmetric pulse wave
            const float sqrRaw = (normPhase < 0.48f) ? 0.85f : -0.85f;
            const float osc = (1.0f - waveform_) * sawRaw + waveform_ * sqrRaw;

            // Sub-oscillator (-1 octave)
            float subOsc = 0.0f;
            if (subVolume_ > 0.01f) {
                state_.subPhase += phaseInc * 0.5f;
                if (state_.subPhase >= 1.0f) state_.subPhase -= 1.0f;
                subOsc = (subWaveform_ > 0.5f)
                    ? ((state_.subPhase < 0.5f) ? 0.7f : -0.7f)
                    : std::sin(2.0f * static_cast<float>(std::numbers::pi) * state_.subPhase);
            }
            const float rawInput = osc * (1.0f - subVolume_ * 0.4f) + subOsc * (subVolume_ * 0.6f);

            // Pre-filter Highpass
            const float preHpOut = preHpB0_ * (-rawInput) + preHpB1_ * state_.preHpX1 + preHpA1_ * state_.preHpY1;
            state_.preHpX1 = -rawInput;
            state_.preHpY1 = preHpOut;

            // Feedback Highpass
            const float fbHpIn = k * state_.stage4;
            const float fbHpOut = fbHpB0_ * fbHpIn + fbHpB1_ * state_.feedbackHpX1 + fbHpA1_ * state_.feedbackHpY1;
            state_.feedbackHpX1 = fbHpIn;
            state_.feedbackHpY1 = fbHpOut;

            // Diode ladder stage coupling
            const float y0 = preHpOut - fbHpOut;
            state_.stage1 += 2.0f * b0 * (y0 - state_.stage1 + state_.stage2);
            state_.stage2 += b0 * (state_.stage1 - 2.0f * state_.stage2 + state_.stage3);
            state_.stage3 += b0 * (state_.stage2 - 2.0f * state_.stage3 + state_.stage4);
            state_.stage4 += b0 * (state_.stage3 - 2.0f * state_.stage4);

            filtered = 2.0f * g * state_.stage4;
        }

        // Post-filter DC blocking highpass (24 Hz)
        const float postHpOut = 0.985f * (state_.postHpY1 + filtered - state_.postHpX1);
        state_.postHpX1 = filtered;
        state_.postHpY1 = postHpOut;

        // VCA Stage & Saturation
        float output = postHpOut * totalAmp * 0.45f;
        if (drive_ > 0.02f) {
            output = fastTanh(output * driveGain_);
        }

        if (std::isnan(output) || std::isinf(output)) {
            output = 0.0f;
        }

        // Auto-cutoff when sound drops to silence
        if (state_.ampEnv < 0.00005f && state_.mainEnv < 0.00005f) {
            active_ = false;
        }

        return std::clamp(output, -1.0f, 1.0f);
    }

    void processBlock(float* output, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            output[i] = processSample();
        }
    }

    [[nodiscard]] bool isActive() const noexcept {
        return active_;
    }

    [[nodiscard]] const Tb303VoiceState& getState() const noexcept {
        return state_;
    }

private:
    [[nodiscard]] static inline float fastTanh(float x) noexcept {
        if (x < -3.0f) return -1.0f;
        if (x > 3.0f) return 1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    void updateCoefficients() noexcept {
        // 150 Hz feedback highpass filter coefficient
        const float fbHpX = std::exp(-2.0f * static_cast<float>(std::numbers::pi) * 150.0f / filterRate_);
        fbHpB0_ = 0.5f * (1.0f + fbHpX);
        fbHpB1_ = -0.5f * (1.0f + fbHpX);
        fbHpA1_ = fbHpX;

        // 44.486 Hz pre-filter highpass filter coefficient
        const float preHpX = std::exp(-2.0f * static_cast<float>(std::numbers::pi) * 44.486f / filterRate_);
        preHpB0_ = 0.5f * (1.0f + preHpX);
        preHpB1_ = -0.5f * (1.0f + preHpX);
        preHpA1_ = preHpX;

        ampDecayCoeff_ = std::exp(-1.0f / (sampleRate_ * 0.001f * 1230.0f));
        releaseDecayCoeff_ = std::exp(-1.0f / (sampleRate_ * 0.001f * 40.0f));
        rc1Coeff_ = std::exp(-1.0f / (sampleRate_ * 0.001f * 3.0f));
        rc2Coeff_ = std::exp(-1.0f / (sampleRate_ * 0.001f * 3.0f));
    }

    void updatePitch() noexcept {
        const float noteFreq = 440.0f * std::pow(2.0f, (static_cast<float>(midiNote_) - 69.0f) / 12.0f);
        targetFreq_ = noteFreq * std::pow(2.0f, static_cast<float>(octave_) + (tuning_ / 12.0f));
        if (!isSlide_) {
            state_.startFreq = targetFreq_;
        }
        currentFreq_ = targetFreq_;
    }

    void updateDynamicParameters() noexcept {
        // Map cutoff: 314Hz to 2394Hz nominal
        const float normCutoff = (cutoffHz_ > 10.0f)
            ? std::clamp(std::log(std::clamp(cutoffHz_, 314.0f, 3500.0f) / 314.0f) / std::log(2394.0f / 314.0f), 0.0f, 1.2f)
            : std::clamp(cutoffHz_, 0.0f, 1.2f);
        nominalCutoff_ = 314.0f * std::pow(2394.0f / 314.0f, normCutoff);

        // Open303 decay range: 200ms to 2000ms
        const float normDecay = (decayNorm_ > 10.0f)
            ? std::clamp(std::log(std::clamp(decayNorm_, 200.0f, 2000.0f) / 200.0f) / std::log(2000.0f / 200.0f), 0.0f, 1.0f)
            : std::clamp(decayNorm_, 0.0f, 1.0f);
        const float decayMs = 200.0f * std::pow(2000.0f / 200.0f, normDecay);
        const float activeDecayMs = isAccent_ ? 200.0f : decayMs;
        decayCoeff_ = std::exp(-1.0f / (sampleRate_ * 0.001f * activeDecayMs));

        // Resonance skewing
        const float normRes = (resonance_ > 16.0f)
            ? (resonance_ / 100.0f)
            : ((resonance_ > 1.0f) ? (resonance_ / 16.0f) : resonance_);
        const float clampedRes = std::clamp(normRes, 0.0f, 1.0f);
        r_ = (1.0f - std::exp(-3.0f * clampedRes)) / (1.0f - std::exp(-3.0f));

        // Open303 Scaler and Offset
        const float normEnv = (envMod_ > 1.0f) ? std::clamp(envMod_ / 100.0f, 0.0f, 1.0f) : std::clamp(envMod_, 0.0f, 1.0f);
        const float e = normEnv * normEnv;
        const float c = std::clamp(std::log(nominalCutoff_ / C0) / std::log(C1 / C0), 0.0f, 1.0f);
        const float sLo = S_LO_F * e + S_LO_C;
        const float sHi = S_HI_F * e + S_HI_C;
        envScaler_ = (1.0f - c) * sLo + c * sHi;
        envOffset_ = OF * c + OC;

        // Accent scaling
        normAccent_ = (accent_ > 1.0f) ? std::clamp(accent_ / 100.0f, 0.0f, 1.0f) : std::clamp(accent_, 0.0f, 1.0f);
    }

    Tb303VoiceState state_;
    float sampleRate_{48000.0f};
    float filterRate_{192000.0f};

    float cutoffHz_{1400.0f};
    float nominalCutoff_{1400.0f};
    float resonance_{0.5f};
    float envMod_{0.75f};
    float decayNorm_{0.5f};
    float accent_{0.0f};
    float normAccent_{0.0f};
    float drive_{0.25f};
    float driveGain_{1.875f};
    float waveform_{0.0f}; // 0.0 = Saw, 1.0 = Square
    float tuning_{0.0f};   // Semitones offset
    int octave_{0};        // Octave shift (-3 .. +3)
    float subVolume_{0.0f};
    float subWaveform_{0.0f}; // 0.0 = Sine, 1.0 = Square
    float glideCurve_{0.0f};  // 0.0 = RC, 1.0 = Linear, 2.0 = S-curve
    float velocity_{0.9f};
    float targetFreq_{110.0f};
    float currentFreq_{110.0f};

    float r_{0.0f};
    float envScaler_{1.0f};
    float envOffset_{0.0f};

    float decayCoeff_{0.999f};
    float ampDecayCoeff_{0.999f};
    float releaseDecayCoeff_{0.999f};
    float rc1Coeff_{0.99f};
    float rc2Coeff_{0.99f};

    float fbHpB0_{0.0f}, fbHpB1_{0.0f}, fbHpA1_{0.0f};
    float preHpB0_{0.0f}, preHpB1_{0.0f}, preHpA1_{0.0f};

    uint8_t midiNote_{36};
    bool isSlide_{false};
    bool isAccent_{false};
    bool gate_{false};
    bool active_{false};
    uint64_t sampleIndex_{0};
};

} // namespace eatsbits::dsp

#endif // EATS_TB303_CORE_HPP
