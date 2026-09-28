#ifndef EATS_PROCEDURAL_SONG_ENGINE_HPP
#define EATS_PROCEDURAL_SONG_ENGINE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "song_archetypes.hpp"
#include "procedural_ensemble_engine.hpp"
#include "../sequencer/step_sequencer.hpp"
#include "../ui/views/arranger_view.hpp"

namespace eatsbits::procgen {

/// User/AI generation configuration parameters.
struct SongGenerationParams {
    std::string style{"Lo-Fi Hip Hop"};
    int bars{16};                // 4, 8, 16, 24, or 32 bars
    std::string structure{"Full Arrangement (Intro-Verse-Chorus-Outro)"};
    double bpm{0.0};             // 0.0 = Auto (genre default)
    int rootPitchClass{-1};      // -1 = Auto (seed derived)
    uint32_t seed{42};
    double swing{-1.0};          // -1.0 = Auto (genre default)
    double humanize{0.20};
};

/// Result summary of an algorithmic song generation pass.
struct SongGenerationResult {
    bool success{true};
    std::string message;
    uint32_t affectedTracks{0};
    uint32_t affectedNotes{0};
    uint32_t affectedChords{0};
    double bpm{120.0};
    std::string songKey{"C Minor"};
    std::vector<ui::ArrangerTimelineTrack> generatedTracks;
    std::vector<theory::ChordEvent> generatedChords;
};

/**
 * Procedural Song Engine.
 * Top-level arrangement generator that creates complete 4-8 track songs
 * (Drums, Bass, Harmonic Chords, Lead Melody, Counterpoint) mapped across
 * the Arranger timeline and Step Sequencer.
 */
class ProceduralSongEngine {
public:
    [[nodiscard]] static const std::vector<std::string>& getAvailableStyles();
    [[nodiscard]] static const std::vector<std::string>& getAvailableStructures();

    /// Generates a full arrangement with timeline tracks, clips, and chord events.
    [[nodiscard]] static SongGenerationResult generateSong(const SongGenerationParams& params);

    /// Generates and loads directly into a StepSequencer instance.
    static SongGenerationResult generateToSequencer(
        sequencer::StepSequencer& sequencer,
        const SongGenerationParams& params
    );

    /// Generates a default procedural demo song into the DAW.
    [[nodiscard]] static SongGenerationResult generateDefaultDemo(
        const std::string& style = "Lo-Fi Hip Hop",
        uint32_t seed = 42
    );
};

} // namespace eatsbits::procgen

#endif // EATS_PROCEDURAL_SONG_ENGINE_HPP
