#ifndef EATS_AUDIO_TO_MIDI_ENGINE_HPP
#define EATS_AUDIO_TO_MIDI_ENGINE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "eatsbits/theory/chord_model.hpp"

namespace eatsbits::audio {

/**
 * Transcription algorithm mode for Audio-to-MIDI engine.
 */
enum class TranscriptionEngineMode {
    HybridDsp,          // Multi-pitch harmonic energy detection (Eatsbeats CQT + Harmonic bank)
    YinMonophonic,      // High-precision YIN autocorrelation algorithm for vocal/bass melodies
    PercussiveTransient // Multi-band spectral flux and energy diff for drum/rhythm transcription
};

/**
 * Configuration options for the transcription process.
 */
struct AudioToMidiOptions {
    TranscriptionEngineMode mode{TranscriptionEngineMode::HybridDsp};
    float onsetThreshold{0.45f};         // 0.05 to 0.95: Sensitivity of attack transient detection
    float frameThreshold{0.35f};         // 0.05 to 0.95: Sensitivity of sustained notes
    float minNoteDurationMs{70.0f};      // 10ms to 500ms: Filters out spurious noise bursts
    int minMidiPitch{24};                // C1 (default)
    int maxMidiPitch{96};                // C7 (default)
    float velocitySensitivity{1.0f};     // 0.5 to 2.0: Scales extracted velocity
    double targetBpm{120.0};             // Tempo for musical grid alignment
    bool enablePitchBend{false};         // Extract microtonal pitch bends
    bool extractChords{true};            // Automatically detect chords for Chord Track
    uint32_t quantizeSteps{4};           // Quantize to grid (1 = 1/64, 2 = 1/32, 4 = 1/16, 8 = 1/8, 0 = off)
};

/**
 * Thread-safe cancellation token for aborting long audio transcriptions.
 */
class CancellationToken {
public:
    CancellationToken() = default;
    void cancel() noexcept { cancelled_.store(true, std::memory_order_relaxed); }
    void reset() noexcept { cancelled_.store(false, std::memory_order_relaxed); }
    [[nodiscard]] bool isCancelled() const noexcept { return cancelled_.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> cancelled_{false};
};

/**
 * Decoded audio buffer containing mono or multi-channel float PCM samples.
 */
struct DecodedAudioBuffer {
    std::vector<float> samples;
    uint32_t sampleRate{44100};
    uint32_t channels{1};

    [[nodiscard]] bool empty() const noexcept { return samples.empty(); }
    [[nodiscard]] size_t frameCount() const noexcept {
        return channels > 0 ? samples.size() / channels : 0;
    }
    [[nodiscard]] double getDurationSeconds() const noexcept {
        return (sampleRate > 0 && channels > 0)
            ? static_cast<double>(samples.size()) / (static_cast<double>(sampleRate) * channels)
            : 0.0;
    }

    /// Decodes an audio file (WAV, MP3, FLAC) from disk
    static DecodedAudioBuffer decodeFromFile(const std::string& filePath);

    /// Decodes an audio file directly from memory buffer
    static DecodedAudioBuffer decodeFromMemory(const uint8_t* data, size_t size);

    /// Creates a monophonic synthetic sine wave test buffer
    static DecodedAudioBuffer createSyntheticSine(double freqHz, double durationSec,
                                                 uint32_t sampleRate = 44100, float amplitude = 0.8f);

    /// Creates a synthetic multi-tone polyphonic chord test buffer
    static DecodedAudioBuffer createSyntheticChord(const std::vector<double>& frequencies,
                                                  double durationSec,
                                                  uint32_t sampleRate = 44100, float amplitude = 0.8f);

    /// Downmixes and resamples to target sample rate
    [[nodiscard]] std::vector<float> resampleAndDownmix(uint32_t targetSampleRate) const;
};

/**
 * Precomputed min/max peaks for fast GUI waveform visualization.
 */
struct WaveformOverview {
    std::vector<float> minPeaks;
    std::vector<float> maxPeaks;

    static WaveformOverview generate(const std::vector<float>& samples, size_t numPoints = 128);
};

/**
 * A transcribed musical note.
 */
struct TranscribedNote {
    std::string id;
    uint8_t pitch{60};          // MIDI note 0..127
    float startStep{0.0f};      // Position in 16th steps (4 steps = 1 beat, 16 steps = 1 bar)
    float durationSteps{1.0f};  // Length in 16th steps
    float velocity{0.8f};       // 0.0 to 1.0

    [[nodiscard]] float getStartBeat() const noexcept { return startStep / 4.0f; }
    [[nodiscard]] float getDurationBeats() const noexcept { return durationSteps / 4.0f; }
};

/**
 * Complete transcription result representing a MIDI track.
 */
struct TranscribedMidiTrack {
    int trackIndex{0};
    std::string name{"Transcribed Audio Track"};
    std::vector<TranscribedNote> notes;
    std::vector<theory::ChordEvent> detectedChords;

    [[nodiscard]] bool empty() const noexcept { return notes.empty(); }
    [[nodiscard]] size_t noteCount() const noexcept { return notes.size(); }
};

/**
 * Sub-band transient analysis result.
 */
struct TransientOnset {
    float timeSec{0.0f};
    float strength{0.0f};
    uint8_t suggestedMidiNote{36}; // 36 = Kick, 38 = Snare, 42 = Closed Hat
    int subBand{0};                // 0 = Low, 1 = Mid, 2 = High
};

/**
 * High-performance C++20 Audio-to-MIDI Transcription Engine.
 * Features:
 * - Hybrid DSP multi-pitch harmonic correlation across MIDI notes 21..108.
 * - YIN pitch detection algorithm for clean monophonic lead/bass/vocal transcription.
 * - Sub-band energy difference & spectral flux transient detector.
 * - Non-destructive chord extraction linking directly with Chord Track.
 */
class AudioToMidiEngine {
public:
    static constexpr uint32_t kTargetSampleRate = 22050;
    static constexpr uint32_t kHopSize = 256;       // ~11.6ms per analysis frame
    static constexpr uint32_t kFftSize = 1024;      // Analysis window size

    /// Transcribes a DecodedAudioBuffer into a TranscribedMidiTrack
    static TranscribedMidiTrack transcribeAudioBuffer(
        const DecodedAudioBuffer& audio,
        const AudioToMidiOptions& options = AudioToMidiOptions{},
        CancellationToken* cancellationToken = nullptr,
        std::function<void(float progress, const std::string& status)> onProgress = nullptr);

    /// Detects percussive onsets and transients using sub-band energy differences
    static std::vector<TransientOnset> detectOnsetsAndTransients(
        const std::vector<float>& monoSamples,
        uint32_t sampleRate,
        float threshold = 0.45f);

    /// Monophonic YIN pitch detection on a single audio frame
    static float detectPitchYin(
        const float* frame,
        size_t windowSize,
        uint32_t sampleRate,
        float minFreq = 40.0f,
        float maxFreq = 2000.0f,
        float threshold = 0.15f);

    /// Correlates a tonal frequency against an audio frame using a Hann window
    static float correlateTone(
        const std::vector<float>& samples,
        size_t offset,
        size_t length,
        double frequency,
        uint32_t sampleRate,
        const std::vector<float>& window);

    /// Checks if a frequency index is a local spectral peak
    static bool isSpectralPeak(const std::vector<float>& frame, int pitch);

    /// Resamples and downmixes multi-channel audio to mono
    static std::vector<float> resampleAndDownmix(
        const std::vector<float>& samples,
        uint32_t srcSampleRate,
        uint32_t channels,
        uint32_t dstSampleRate);

private:
    static TranscribedMidiTrack transcribeHybridDsp(
        const std::vector<float>& monoSamples,
        const AudioToMidiOptions& options,
        CancellationToken* token,
        std::function<void(float, const std::string&)> onProgress);

    static TranscribedMidiTrack transcribeYin(
        const std::vector<float>& monoSamples,
        uint32_t sampleRate,
        const AudioToMidiOptions& options,
        CancellationToken* token,
        std::function<void(float, const std::string&)> onProgress);

    static TranscribedMidiTrack transcribePercussive(
        const std::vector<float>& monoSamples,
        uint32_t sampleRate,
        const AudioToMidiOptions& options,
        CancellationToken* token,
        std::function<void(float, const std::string&)> onProgress);
};

} // namespace eatsbits::audio

#endif // EATS_AUDIO_TO_MIDI_ENGINE_HPP
