#ifndef EATS_NOTE_SPLITTER_ENGINE_HPP
#define EATS_NOTE_SPLITTER_ENGINE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "../ui/theme.hpp"
#include "step_sequencer.hpp"
#include "../eatscript/midi_fx_pipeline.hpp"

namespace eatsbits::sequencer {

enum class SplitMode {
    ThreeWayVoice,   // Bassline, Harmony/Chords, Skyline Lead Melody
    BassTrebleClefs, // Left Hand (< pivot), Right Hand (>= pivot)
    FourVoiceSatb,   // Soprano, Alto, Tenor, Bass polyphonic distribution
    DrumDemux        // GM Drum Demuxer (Kick, Snare/Clap, Hats/Cymbals, Toms/Perc)
};

struct SplitTrackResult {
    std::string name;
    ui::Color color;
    std::string iconRef{"preset:inst_synth"};
    std::vector<eatscript::MidiNote> notes;
    uint8_t minPitch{127};
    uint8_t maxPitch{0};
    size_t noteCount{0};
};

struct SplitParams {
    uint8_t bassSplitPitch{48};      // C3 for 3-way
    uint8_t leadThresholdPitch{64};   // E4 for 3-way
    uint8_t pivotPitch{60};          // Middle C for 2-way piano split
};

/**
 * Algorithmic chord voice separator and polyphonic note distributor.
 * Splits composite MIDI and step tracks into melodic stems.
 */
class NoteSplitterEngine {
public:
    static bool isSkyline(const eatscript::MidiNote& target, const std::vector<eatscript::MidiNote>& allNotes) noexcept;
    static bool isBassNote(const eatscript::MidiNote& target, const std::vector<eatscript::MidiNote>& allNotes) noexcept;

    static std::vector<SplitTrackResult> split3WayVoice(
        const std::vector<eatscript::MidiNote>& notes,
        uint8_t bassSplitPitch = 48,
        uint8_t leadThresholdPitch = 64
    );

    static std::vector<SplitTrackResult> splitBassTreble(
        const std::vector<eatscript::MidiNote>& notes,
        uint8_t splitPitch = 60
    );

    static std::vector<SplitTrackResult> split4VoicePolyphony(
        const std::vector<eatscript::MidiNote>& notes
    );

    static std::vector<SplitTrackResult> splitDrumPercussion(
        const std::vector<eatscript::MidiNote>& notes
    );

    static std::vector<SplitTrackResult> splitNotes(
        const std::vector<eatscript::MidiNote>& notes,
        SplitMode mode,
        const SplitParams& params = {}
    );

    static std::vector<SplitTrackResult> splitTrack(
        const SequencerTrack& track,
        SplitMode mode,
        const SplitParams& params = {}
    );

    /// Applies the split to StepSequencer: generates new tracks and populates them!
    static std::vector<size_t> applySplitToSequencer(
        StepSequencer& seq,
        size_t sourceTrackIdx,
        SplitMode mode,
        const SplitParams& params = {},
        bool removeSourceTrack = false
    );
};

} // namespace eatsbits::sequencer

#endif // EATS_NOTE_SPLITTER_ENGINE_HPP
