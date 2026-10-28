#ifndef EATS_NOTE_SCRIPT_HPP
#define EATS_NOTE_SCRIPT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "../sequencer/step_sequencer.hpp"

namespace eatsbits::eatscript {

/**
 * High-performance, zero-dependency Note Script Engine.
 * Converts between SequencerTrack step events and declarative Eatscript textual representation.
 * Allows human users and AI agents to program, inspect, and tweak music content in plain text.
 */
class NoteScriptEngine {
public:
    /// Converts a MIDI note number (0..127) to a string like "C3", "F#2", "D#4".
    [[nodiscard]] static std::string pitchToString(uint8_t midiNote) noexcept;

    /// Converts a string pitch (e.g. "C3", "C#3", "Db3", "F#2", or numeric "60") to MIDI note number.
    /// Returns -1 on failure.
    [[nodiscard]] static int stringToPitch(const std::string& str) noexcept;

    /// Serializes a SequencerTrack's active steps into declarative Eatscript syntax.
    [[nodiscard]] static std::string serializeTrackNotes(const sequencer::SequencerTrack& track);

    /// Serializes a SequencerTrack's active steps into a list of lines for text buffer editing.
    [[nodiscard]] static std::vector<std::string> serializeTrackNotesToLines(const sequencer::SequencerTrack& track);

    /// Parses declarative Eatscript note lines and applies them to a SequencerTrack.
    /// Returns true on success, or false with outError and outErrorLine populated on syntax error.
    static bool parseTrackNotes(const std::string& scriptText,
                               sequencer::SequencerTrack& track,
                               std::string* outError = nullptr,
                               int* outErrorLine = nullptr);

    /// Overload for parsing from a vector of buffer lines.
    static bool parseTrackNotesFromLines(const std::vector<std::string>& lines,
                                        sequencer::SequencerTrack& track,
                                        std::string* outError = nullptr,
                                        int* outErrorLine = nullptr);

    /// Parsed event for Eatscript clip code ({ pitch = "C4", step = 0.0, dur = 1.0, vel = 0.85, ... })
    struct ClipNoteEvent {
        uint8_t pitch{60};
        float startBeat{0.0f};      // in beats (step / 4.0f)
        float lengthBeats{1.0f};    // in beats (dur / 4.0f)
        float velocity{0.85f};
        float startStep{0.0f};      // in 16th-note steps
        float durationSteps{1.0f};  // in 16th-note steps
        int lane{0};
        bool slide{false};
        bool accent{false};
    };

    /// Parses Eatscript clip code ({ pitch = "C4", step = 0.0, dur = 1.0, vel = 0.85, ... }) into a list of ClipNoteEvents.
    [[nodiscard]] static std::vector<ClipNoteEvent> parseClipEvents(const std::string& scriptText);
};

} // namespace eatsbits::eatscript

#endif // EATS_NOTE_SCRIPT_HPP
