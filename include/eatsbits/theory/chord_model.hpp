#ifndef EATS_CHORD_MODEL_HPP
#define EATS_CHORD_MODEL_HPP

#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <optional>

namespace eatsbits::theory {

/// Chord qualities supported by the harmonic engine (Eatsbeats compatible).
enum class ChordQuality {
    Major,
    Minor,
    Dominant7,
    Major7,
    Minor7,
    Diminished,
    Augmented,
    HalfDiminished7,
    Sus2,
    Sus4,
    Add9,
    Min9,
    Maj9,
    Dom9
};

/// Harmonic remapping / scale conforming mode for tracks following the Chord Track.
enum class ChordFollowMode {
    Off,
    Chord,
    Bass,
    Scale,
    ColorLead
};

[[nodiscard]] const char* getChordQualityDisplayName(ChordQuality q) noexcept;
[[nodiscard]] const char* getChordQualitySymbol(ChordQuality q) noexcept;
[[nodiscard]] std::vector<int> getChordQualityIntervals(ChordQuality q);
[[nodiscard]] const char* getChordFollowModeName(ChordFollowMode m) noexcept;
[[nodiscard]] ChordFollowMode parseChordFollowMode(const std::string& name) noexcept;

/// A chord event positioned on the Chord Track timeline.
struct ChordEvent {
    std::string id{"chord_0"};
    uint32_t startBar{0};          // 0-indexed bar number
    float barLength{1.0f};         // Duration in bars (e.g. 1.0, 2.0, 0.5, 4.0)
    int rootPitchClass{0};         // 0 = C, 1 = C#, 2 = D, ..., 11 = B
    ChordQuality quality{ChordQuality::Major};
    int bassPitchClass{-1};        // -1 = Default Root, or 0..11 for slash chords (e.g. C/E -> bass = 4)

    [[nodiscard]] std::string getDisplayName() const;
    [[nodiscard]] std::string getRootName() const;
    [[nodiscard]] std::string getBassName() const;
    [[nodiscard]] std::vector<int> getPitchClasses() const;
};

/// Preset entry in the curated chord progression library.
struct ChordProgressionPreset {
    struct Step {
        int rootOffset{0};         // Semitone offset from song key tonic (e.g. 0 = I, 5 = IV, 7 = V, 9 = vi)
        ChordQuality quality{ChordQuality::Major};
        float barLength{1.0f};     // Duration in bars
    };

    std::string id;
    std::string name;
    std::string genre;
    std::string description;
    std::vector<std::string> tags;
    std::vector<Step> chords;

    [[nodiscard]] std::string getRomanSummary(int keyRoot = 0, bool isMinor = false) const;
};

/// Note input descriptor for chord extraction and analysis.
struct TheoryNote {
    uint8_t pitch{60};
    float startStep{0.0f};
    float durationSteps{1.0f};
    float velocity{0.8f};
};

/// Music Theory Utilities, Circle of Fifths Data & Harmonic Remapping Engine.
class ChordTheory {
public:
    static const std::array<const char*, 12> pitchClassNames;
    static const std::array<const char*, 12> pitchClassFlatNames;

    /// Circle of Fifths: 12 Major sectors starting at C (top, 12 o'clock) clockwise.
    /// C (0), G (7), D (2), A (9), E (4), B (11), F#/Gb (6), Db (1), Ab (8), Eb (3), Bb (10), F (5)
    static const std::array<int, 12> circleOfFifthsMajor;

    /// Circle of Fifths relative minor order:
    /// Am (9), Em (4), Bm (11), F#m (6), C#m (1), G#m (8), D#m (3), Bbm (10), Fm (5), Cm (0), Gm (7), Dm (2)
    static const std::array<int, 12> circleOfFifthsMinor;

    static const std::array<const char*, 12> circleMajorLabels;
    static const std::array<const char*, 12> circleMinorLabels;

    /// Curated library of 28 iconic chord progressions across genres.
    [[nodiscard]] static const std::vector<ChordProgressionPreset>& getProgressionPresets();

    /// Formats a chord name nicely (e.g. "Cmaj7", "G/B", "F#m7b5")
    [[nodiscard]] static std::string formatChordName(int rootPitchClass, ChordQuality quality, int bassPitchClass = -1);

    /// Calculates all unique pitch classes (0..11) for a given chord.
    [[nodiscard]] static std::vector<int> getPitchClasses(int rootPitchClass, ChordQuality quality, int bassPitchClass = -1);

    /// Returns MIDI notes for auditioning a chord around octave 3-4 (playable on any instrument)
    [[nodiscard]] static std::vector<int> getAuditionMidiNotes(const ChordEvent& chord);

    /// Returns diatonic Roman numeral relative to Song Key (e.g. C Major -> I, ii, iii, IV, V, vi, vii°)
    [[nodiscard]] static std::string getRomanNumeral(int keyRootPitchClass, bool isKeyMinor, int chordRootPitchClass, ChordQuality quality);

    /// Scale degrees / Parent scale pitch classes for a chord
    [[nodiscard]] static std::vector<int> getScalePitchClassesForChord(const ChordEvent& chord);

    /// Non-destructive realtime pitch snapping / harmonic remapping algorithm
    [[nodiscard]] static int remapPitchForChord(int originalPitch, const ChordEvent& chord, ChordFollowMode mode);
    [[nodiscard]] static int remapPitchForChord(int originalPitch, const ChordEvent& chord, const std::string& followModeStr);

    /// Given a list of active MIDI pitches, detects the best matching root pitch class (0..11), ChordQuality, and optional bass note.
    [[nodiscard]] static bool detectChordFromPitches(
        const std::vector<int>& midiPitches,
        int& outRoot,
        ChordQuality& outQuality,
        int& outBass);

    /// Analyzes note events across bars and creates a sequence of ChordEvents
    [[nodiscard]] static std::vector<ChordEvent> extractChordsFromNotes(
        const std::vector<TheoryNote>& notes,
        uint32_t startBar = 0,
        uint32_t totalBars = 0,
        uint32_t stepsPerBar = 16);
};

} // namespace eatsbits::theory

#endif // EATS_CHORD_MODEL_HPP
