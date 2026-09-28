#ifndef EATS_TRACK_FREEZE_ENGINE_HPP
#define EATS_TRACK_FREEZE_ENGINE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <memory>
#include "graph/audio_graph.hpp"
#include "../sequencer/step_sequencer.hpp"

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::audio::trackfreeze {

/// Callback for reporting freeze progress from 0.0 to 1.0.
using FreezeProgressCallback = std::function<void(float progress, const std::string& status)>;

/// Configuration options for offline track audio baking.
struct FreezeOptions {
    double bpm{120.0};
    uint32_t sampleRate{48000};
    uint32_t numBars{0};         // 0 = automatic from track length
    bool includeFx{true};        // Include track FX rack / connected nodes in bake
    bool normalizePeaks{true};   // Soft peak normalize to ceiling
    float peakLimit{0.98f};      // Normalized peak limit ceiling
    FreezeProgressCallback onProgress{nullptr};
};

/// Resulting status and stereo Float32 audio buffers from an offline bake.
struct FreezeResult {
    bool success{false};
    std::vector<float> bufferL{};
    std::vector<float> bufferR{};
    std::string contentHash{""};
    uint32_t sampleRate{48000};
    uint64_t totalFrames{0};
    double durationSeconds{0.0};
    double renderTimeMs{0.0};
    double speedMultiplier{0.0};
    float peakL{0.0f};
    float peakR{0.0f};
    std::string errorMessage{""};
};

/**
 * TrackFreezeEngine: Offline DSP synthesis and audio bouncing engine.
 * Bakes individual tracks into contiguous stereo Float32 PCM audio buffers,
 * computes deterministic 64-bit FNV-1a content hashes, and dynamically
 * bypasses heavy synth and FX nodes to reclaim 100% real-time CPU.
 */
class TrackFreezeEngine {
public:
    /// Computes a deterministic 64-bit FNV-1a content hash (16-char hex)
    /// of track notes, steps, clips, synth settings, and FX parameters.
    static std::string computeTrackHash(
        const sequencer::SequencerTrack& track,
        double bpm = 120.0,
        uint32_t sampleRate = 48000,
        const AudioGraph* graph = nullptr
    );

    /// Checks if a track's existing frozen buffer is valid and up-to-date with current settings.
    static bool isFreezeValid(
        const sequencer::SequencerTrack& track,
        double bpm = 120.0,
        uint32_t sampleRate = 48000,
        const AudioGraph* graph = nullptr
    );

    /// Fast-renders the track offline into contiguous stereo Float32 memory buffers.
    static FreezeResult renderTrackOffline(
        AudioGraph& graph,
        sequencer::StepSequencer& seq,
        size_t trackIndex,
        const FreezeOptions& options = {}
    );

    /// Freezes the track on the given AudioEngine: renders offline, saves buffers to track,
    /// sets isFrozen=true, and disables the live synth node in graph to reclaim CPU.
    static bool freezeTrack(
        AudioEngine& engine,
        size_t trackIndex,
        const FreezeOptions& options = {}
    );

    /// Unfreezes the track: sets isFrozen=false, re-enables the live synth node in graph.
    static bool unfreezeTrack(
        AudioEngine& engine,
        size_t trackIndex,
        bool clearBuffers = false
    );

    /// Toggles freeze status on the track.
    static bool toggleFreezeTrack(
        AudioEngine& engine,
        size_t trackIndex,
        const FreezeOptions& options = {}
    );

    /// Direct background bounce of a track to a WAV file.
    static bool bounceTrackToWav(
        AudioGraph& graph,
        sequencer::StepSequencer& seq,
        size_t trackIndex,
        const std::string& wavFilePath,
        const FreezeOptions& options = {}
    );

    /// Asynchronous background track freeze (returns std::future).
    static std::future<FreezeResult> renderTrackAsync(
        std::shared_ptr<AudioGraph> graphCopy,
        std::shared_ptr<sequencer::StepSequencer> seqCopy,
        size_t trackIndex,
        FreezeOptions options = {}
    );
};

} // namespace eatsbits::audio::trackfreeze

#endif // EATS_TRACK_FREEZE_ENGINE_HPP
