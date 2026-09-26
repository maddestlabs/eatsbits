#ifndef EATS_MIDI_FX_PIPELINE_HPP
#define EATS_MIDI_FX_PIPELINE_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <algorithm>
#include "../sequencer/step_sequencer.hpp"

namespace eatsbits::eatscript {

/**
 * Universal MIDI Note representation for the Eatscript transform pipeline.
 * Compatible with Eatsbeats Note model.
 */
struct MidiNote {
    std::string id;
    uint8_t pitch{60};          // MIDI pitch (0..127, 60 = C4)
    float startStep{0.0f};      // Start step (0.0 = step 1, 16th-note units)
    float durationSteps{1.0f};  // Duration in 16th-note steps
    float velocity{0.85f};      // Velocity in [0.0, 1.0]
    int column{0};              // Tracker column / polyphonic lane
    bool isSlide{false};        // 303 portamento slide
    bool isAccent{false};       // 303 dynamic accent
    std::string effectCommand{""};
};

/**
 * Resolved MIDI FX type for fast enum-based dispatch without string parsing on each cycle.
 * Directly mirrors Eatsbeats MidiFxType.
 */
enum class MidiFxType {
    Eatscript,
    ChordFollow,
    ChordArp,
    ScaleSnap,
    Arpeggiator,
    Humanize,
    ChordStabs,
    Transpose,
    Passthrough
};

/**
 * Project musical and temporal context for MIDI transformations.
 */
struct TimeContext {
    double bpm{135.0};
    int songKeyRoot{0};          // 0 = C, 1 = C#, 2 = D, ..., 9 = A, 11 = B
    bool isSongKeyMinor{false};
    int activeChordRoot{0};      // Active chord root pitch class
    std::string activeChordQuality{"maj"}; // "maj", "min", "7", "min7", "maj7", "dim", "sus4"
    std::vector<int> chordPitchClasses{0, 4, 7}; // Pitch classes in current chord (e.g. {0, 4, 7})
    int bassPitchClass{0};
    double currentStep{0.0};
};

/**
 * A single MIDI FX Insert rack unit.
 */
struct MidiFxInsert {
    std::string id{"mfx_1"};
    std::string name{"Arpeggiator"};
    bool enabled{true};
    MidiFxType type{MidiFxType::Arpeggiator};
    std::string eatscriptCode{""};
    std::map<std::string, float> params;
};

/**
 * Production-grade MIDI FX Pipeline Engine.
 * Implements Arpeggiator, Scale Snapping, Chord Follow, Humanize, Transpose, Chord Stabs,
 * and custom Eatscript script execution.
 */
class MidiPipelineEngine {
public:
    /// Detects MIDI FX type from Eatscript code or name heuristics (Eatsbeats compatible).
    [[nodiscard]] static MidiFxType detectMidiFxType(const std::string& code, const std::string& name = "");

    /// Snaps a pitch to Major or Natural Minor scale.
    [[nodiscard]] static int snapToScale(int pitch, int rootKey, bool isMinor) noexcept;

    /// Production-grade Arpeggiator transformation.
    /// Supports patterns: "up", "down", "updown", "downup", "converge", "diverge", "random", "chord", "asplayed".
    /// Supports multi-octave cycling, sub-step fractional rates, gate scaling, and swing timing.
    [[nodiscard]] static std::vector<MidiNote> applyArpeggiator(
        const std::vector<MidiNote>& baseNotes,
        double stepRate = 1.0,
        int octaves = 2,
        const std::string& pattern = "up",
        double gate = 0.85,
        double swing = 0.0,
        const TimeContext* timeContext = nullptr,
        bool useChordTrackTones = false);

    /// Conforms notes dynamically to the active Chord Track / active chord.
    /// Modes: "chord", "bass", "scale", "colorLead".
    [[nodiscard]] static std::vector<MidiNote> applyChordFollow(
        const std::vector<MidiNote>& notes,
        const TimeContext& timeContext,
        const std::string& mode = "chord");

    /// Snaps notes to active song key scale.
    [[nodiscard]] static std::vector<MidiNote> applyScaleSnap(
        const std::vector<MidiNote>& notes,
        int rootKey,
        bool isMinor);

    /// Applies subtle micro-timing jitter and velocity variations.
    [[nodiscard]] static std::vector<MidiNote> applyHumanize(
        const std::vector<MidiNote>& notes,
        float timingAmount = 0.04f,
        float velocityAmount = 0.15f,
        uint32_t seed = 42);

    /// Transposes all notes by semitones offset.
    [[nodiscard]] static std::vector<MidiNote> applyTranspose(
        const std::vector<MidiNote>& notes,
        int semitones);

    /// Converts single-note triggers into full, lush polyphonic chord voicings.
    [[nodiscard]] static std::vector<MidiNote> generateChordVoicings(
        const std::vector<MidiNote>& triggerNotes,
        const TimeContext& timeContext);

    /// Evaluates a single MIDI FX insert on a note stream.
    [[nodiscard]] static std::vector<MidiNote> evaluateMidiFx(
        const MidiFxInsert& fx,
        const std::vector<MidiNote>& notes,
        const TimeContext& timeContext);

    /// Processes an entire MIDI FX chain (rack) sequentially.
    [[nodiscard]] static std::vector<MidiNote> processPipeline(
        const std::vector<MidiFxInsert>& rack,
        const std::vector<MidiNote>& notes,
        const TimeContext& timeContext);

    // =========================================================================
    // SequencerTrack Integration
    // =========================================================================

    /// Extracts active steps from a SequencerTrack as MidiNotes.
    [[nodiscard]] static std::vector<MidiNote> trackToNotes(const sequencer::SequencerTrack& track);

    /// Writes MidiNotes back into a SequencerTrack's step buffer.
    static void notesToTrack(const std::vector<MidiNote>& notes, sequencer::SequencerTrack& track);

    /// Applies a MIDI FX rack directly to a SequencerTrack.
    static void applyPipelineToTrack(
        const std::vector<MidiFxInsert>& rack,
        sequencer::SequencerTrack& track,
        const TimeContext& timeContext);
};

} // namespace eatsbits::eatscript

#endif // EATS_MIDI_FX_PIPELINE_HPP
