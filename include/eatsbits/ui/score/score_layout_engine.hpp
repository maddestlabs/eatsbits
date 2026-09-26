#ifndef EATS_SCORE_LAYOUT_ENGINE_HPP
#define EATS_SCORE_LAYOUT_ENGINE_HPP

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace eatsbits::ui {

enum class ScoreClef {
    Treble,
    Bass,
    GrandStaff,
    Auto
};

enum class ScoreAccidental {
    None,
    Sharp,
    Flat,
    Natural
};

enum class ScoreNoteType {
    Whole,
    Half,
    Quarter,
    Eighth,
    Sixteenth
};

struct ScorePitchLayout {
    int midiPitch{60};
    int diatonicStep{28};
    ScoreAccidental accidental{ScoreAccidental::None};
    std::string noteName{"C4"};
    bool isTreble{true};
};

struct ScoreNoteVisual {
    ScorePitchLayout pitchLayout;
    ScoreNoteType noteType{ScoreNoteType::Quarter};
    bool isDotted{false};
    float yPos{0.0f};
    bool isStemUp{true};
    std::vector<float> ledgerLineYPositions;
};

class ScoreLayoutEngine {
public:
    // Diatonic steps from C0:
    // C=0, D=1, E=2, F=3, G=4, A=5, B=6
    // Treble Line 1 (E4) = (4 * 7) + 2 = 30
    // Treble Line 3 (B4) = (4 * 7) + 6 = 34
    // Treble Line 5 (F5) = (5 * 7) + 3 = 38
    // Bass Line 1 (G2) = (2 * 7) + 4 = 18
    // Bass Line 3 (D3) = (3 * 7) + 1 = 22
    // Bass Line 5 (A3) = (3 * 7) + 5 = 26
    // Middle C (C4, MIDI 60) = (4 * 7) + 0 = 28

    static constexpr int trebleLine1Step = 30; // E4
    static constexpr int trebleLine3Step = 34; // B4
    static constexpr int trebleLine5Step = 38; // F5

    static constexpr int bassLine1Step = 18;   // G2
    static constexpr int bassLine3Step = 22;   // D3
    static constexpr int bassLine5Step = 26;   // A3

    static constexpr int middleCStep = 28;     // C4 (MIDI 60)

    static ScorePitchLayout pitchToLayout(int midiPitch, bool useFlats = false) {
        int clamped = std::clamp(midiPitch, 0, 127);
        int octave = (clamped / 12) - 1;
        int semitone = clamped % 12;

        struct ChromaticEntry { int diatonic; ScoreAccidental accidental; };
        static constexpr ChromaticEntry chromaticMap[12] = {
            {0, ScoreAccidental::None},  // C
            {0, ScoreAccidental::Sharp}, // C#
            {1, ScoreAccidental::None},  // D
            {1, ScoreAccidental::Sharp}, // D#
            {2, ScoreAccidental::None},  // E
            {3, ScoreAccidental::None},  // F
            {3, ScoreAccidental::Sharp}, // F#
            {4, ScoreAccidental::None},  // G
            {4, ScoreAccidental::Sharp}, // G#
            {5, ScoreAccidental::None},  // A
            {5, ScoreAccidental::Sharp}, // A#
            {6, ScoreAccidental::None},  // B
        };

        int diatonic;
        ScoreAccidental accidental;

        if (useFlats && chromaticMap[semitone].accidental == ScoreAccidental::Sharp) {
            diatonic = chromaticMap[semitone].diatonic + 1;
            accidental = ScoreAccidental::Flat;
        } else {
            diatonic = chromaticMap[semitone].diatonic;
            accidental = chromaticMap[semitone].accidental;
        }

        int totalDiatonicStep = (octave * 7) + diatonic;

        static constexpr const char* diatonicNames[7] = {"C", "D", "E", "F", "G", "A", "B"};
        std::string accStr = (accidental == ScoreAccidental::Sharp) ? "#" : ((accidental == ScoreAccidental::Flat) ? "b" : "");
        std::string noteName = std::string(diatonicNames[diatonic % 7]) + accStr + std::to_string(octave);

        return ScorePitchLayout{
            clamped,
            totalDiatonicStep,
            accidental,
            noteName,
            clamped >= 60 // C4 and above goes to Treble staff by default in Grand Staff
        };
    }

    static std::pair<ScoreNoteType, bool> durationToNoteType(float durationSteps, float stepsPerBeat = 4.0f) {
        float beats = durationSteps / stepsPerBeat;
        if (beats >= 3.5f) {
            return {ScoreNoteType::Whole, false};
        } else if (beats >= 2.5f) {
            return {ScoreNoteType::Half, true}; // Dotted half
        } else if (beats >= 1.75f) {
            return {ScoreNoteType::Half, false};
        } else if (beats >= 1.25f) {
            return {ScoreNoteType::Quarter, true}; // Dotted quarter
        } else if (beats >= 0.75f) {
            return {ScoreNoteType::Quarter, false};
        } else if (beats >= 0.35f) {
            return {ScoreNoteType::Eighth, false};
        } else {
            return {ScoreNoteType::Sixteenth, false};
        }
    }

    static ScoreNoteVisual computeVisual(
        const ScorePitchLayout& pitchLayout,
        float durationSteps,
        float staffLine1Y,
        float sp,
        bool isTrebleStaff,
        float stepsPerBeat = 4.0f
    ) {
        int line1Step = isTrebleStaff ? trebleLine1Step : bassLine1Step;
        int line5Step = isTrebleStaff ? trebleLine5Step : bassLine5Step;
        int line3Step = isTrebleStaff ? trebleLine3Step : bassLine3Step;

        float halfSp = sp / 2.0f;
        int stepDelta = pitchLayout.diatonicStep - line1Step;
        float noteY = staffLine1Y - (static_cast<float>(stepDelta) * halfSp);

        // Stem direction: at or above line 3 stems point down; below line 3 stems point up
        bool isStemUp = pitchLayout.diatonicStep < line3Step;

        std::vector<float> ledgers;
        if (pitchLayout.diatonicStep < line1Step) {
            // Below staff: step delta is negative
            for (int s = line1Step - 2; s >= pitchLayout.diatonicStep; s -= 2) {
                ledgers.push_back(staffLine1Y - static_cast<float>(s - line1Step) * halfSp);
            }
        } else if (pitchLayout.diatonicStep > line5Step) {
            // Above staff: step delta is positive
            for (int s = line5Step + 2; s <= pitchLayout.diatonicStep; s += 2) {
                ledgers.push_back(staffLine1Y - static_cast<float>(s - line1Step) * halfSp);
            }
        }

        auto [noteType, isDotted] = durationToNoteType(durationSteps, stepsPerBeat);

        return ScoreNoteVisual{
            pitchLayout,
            noteType,
            isDotted,
            noteY,
            isStemUp,
            std::move(ledgers)
        };
    }

    static int yToMidiPitch(
        float y,
        float staffLine1Y,
        float sp,
        bool isTrebleStaff,
        ScoreAccidental forcedAccidental = ScoreAccidental::None
    ) {
        int line1Step = isTrebleStaff ? trebleLine1Step : bassLine1Step;
        float halfSp = sp / 2.0f;

        float stepDeltaExact = (staffLine1Y - y) / halfSp;
        int diatonicStep = std::clamp(line1Step + static_cast<int>(std::round(stepDeltaExact)), 0, 70);

        int octave = diatonicStep / 7;
        int diatonicIndex = diatonicStep % 7;

        static constexpr int naturalSemitones[7] = {0, 2, 4, 5, 7, 9, 11};
        int semitone = naturalSemitones[diatonicIndex];

        if (forcedAccidental == ScoreAccidental::Sharp) {
            semitone += 1;
        } else if (forcedAccidental == ScoreAccidental::Flat) {
            semitone -= 1;
        }

        return std::clamp((octave + 1) * 12 + semitone, 0, 127);
    }
};

} // namespace eatsbits::ui

#endif // EATS_SCORE_LAYOUT_ENGINE_HPP
