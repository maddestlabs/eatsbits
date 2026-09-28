#pragma once

#include "../graph_node.hpp"
#include "eatsbits/lyrics/lyric_track.hpp"
#include <string>
#include <vector>
#include <array>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace eatsbits::audio {

enum class VocalMode : uint32_t {
    Natural = 0,
    Robot = 1,
    Whisper = 2
};

enum class VowelPhoneme : uint32_t {
    A = 0, // "father" (730, 1090, 2440 Hz)
    E = 1, // "bed"    (530, 1840, 2480 Hz)
    I = 2, // "see"    (270, 2290, 3010 Hz)
    O = 3, // "boat"   (570, 840,  2410 Hz)
    U = 4  // "boot"   (300, 870,  2240 Hz)
};

/**
 * @brief Resonant 2-pole Bandpass Filter (SVF) for vocal tract formant modeling.
 */
struct FormantFilter {
    float f{1000.0f}; // Resonant frequency (Hz)
    float q{6.0f};    // Bandwidth / Q factor
    float gain{1.0f}; // Formant relative gain

    // State variable filter states
    float s1{0.0f};
    float s2{0.0f};

    void reset() noexcept {
        s1 = 0.0f;
        s2 = 0.0f;
    }

    [[nodiscard]] inline float process(float in, float sampleRate) noexcept {
        if (sampleRate <= 0.0f) return in;
        float g = std::tan(static_cast<float>(M_PI) * (f / sampleRate));
        float k = 1.0f / q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        float v3 = in - s2;
        float v1 = a1 * s1 + a2 * v3;
        float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;

        return v1 * gain; // Bandpass output
    }
};

/**
 * @brief Hardware-styled vocal formant synthesizer & speech engine node.
 * Features:
 * - 3-pole physical acoustic vocal tract formant resonators (F1, F2, F3).
 * - Glottal pulse oscillator with variable prosody and subtle human vibrato.
 * - Sibilance & breath turbulence noise generator.
 * - 3 vocal modes: Natural (human singing), Robot (vocoded carrier), Whisper (unvoiced breath).
 * - Real-time zero-allocation processing loop.
 */
class TtsSynthNode : public GraphNode {
public:
    static constexpr uint32_t PARAM_PITCH = 0;
    static constexpr uint32_t PARAM_TONE = 1;
    static constexpr uint32_t PARAM_VOLUME = 2;
    static constexpr uint32_t PARAM_SPACE = 3;
    static constexpr uint32_t PARAM_AIR = 4;
    static constexpr uint32_t PARAM_VOICE_MODE = 5;
    static constexpr uint32_t PARAM_SPEED = 6;
    static constexpr uint32_t PARAM_VOWEL = 7;
    static constexpr uint32_t PARAM_GATE = 8;

    explicit TtsSynthNode(std::string name = "TTS Voice Synth");
    ~TtsSynthNode() override = default;

    void prepare(double sampleRate, uint32_t maxBlockSize) override;
    void reset() noexcept override;
    void processBlock(uint32_t numFrames) noexcept override;

    void setParameter(uint32_t paramId, float value) noexcept override;
    [[nodiscard]] float getParameter(uint32_t paramId) const noexcept;

    void handleEvent(const AudioEvent& event) noexcept override;

    // Speech and Lyric sequencing
    void speakText(const std::string& text);
    void triggerCue(const lyrics::LyricCue& cue);
    void setVowel(VowelPhoneme vowel) noexcept;
    void setPitchHz(float hz) noexcept;
    void setGate(bool active) noexcept;

    [[nodiscard]] VocalMode getVocalMode() const noexcept { return vocalMode_; }
    void setVocalMode(VocalMode mode) noexcept;

    [[nodiscard]] VowelPhoneme getCurrentVowel() const noexcept { return currentVowel_; }
    [[nodiscard]] bool isVoicing() const noexcept { return gateActive_ || noteOnCount_ > 0; }

private:
    void updateFormantTargets() noexcept;

    double sampleRate_{48000.0};
    uint32_t maxBlockSize_{512};

    // Parameters
    float pitchMultiplier_{1.0f};
    float tone_{1.0f};
    float volume_{1.0f};
    float space_{0.3f};
    float air_{0.15f};
    VocalMode vocalMode_{VocalMode::Natural};
    float speedMultiplier_{1.0f};
    VowelPhoneme currentVowel_{VowelPhoneme::A};
    bool gateActive_{false};

    // Pitch & Phase
    float currentPitchHz_{220.0f}; // A3 fundamental
    float targetPitchHz_{220.0f};
    float phase_{0.0f};
    float vibratoPhase_{0.0f};
    uint32_t noteOnCount_{0};

    // Formant filter banks: Left and Right
    std::array<FormantFilter, 3> formantsL_;
    std::array<FormantFilter, 3> formantsR_;
    std::array<float, 3> targetF_{{730.0f, 1090.0f, 2440.0f}};
    std::array<float, 3> currentF_{{730.0f, 1090.0f, 2440.0f}};

    // Noise Generator state (Linear Congruential)
    uint32_t noiseSeed_{22222};

    // Sibilance / Envelope
    float ampEnv_{0.0f};
    float attackCoeff_{0.005f};
    float releaseCoeff_{0.001f};

    // Queued speech phoneme structure
    struct QueuedPhoneme {
        VowelPhoneme vowel{VowelPhoneme::A};
        float pitchHz{220.0f};
        uint32_t durationFrames{9600}; // ~200ms at 48k
        bool isConsonantNoise{false};
    };

    static constexpr size_t MAX_PHONEME_QUEUE = 64;
    std::array<QueuedPhoneme, MAX_PHONEME_QUEUE> phonemeQueue_{};
    size_t queueReadIdx_{0};
    size_t queueWriteIdx_{0};
    uint32_t currentPhonemeFramesLeft_{0};
};

} // namespace eatsbits::audio
