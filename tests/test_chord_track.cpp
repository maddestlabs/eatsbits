#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "eatsbits/theory/chord_model.hpp"
#include "eatsbits/ui/views/arranger_view.hpp"
#include "eatsbits/ui/widgets/circle_of_fifths_dialog.hpp"
#include "eatsbits/ui/input/pointer_event.hpp"

using namespace eatsbits;
using namespace eatsbits::theory;
using namespace eatsbits::ui;

static PointerEvent makePointer(float x, float y, PointerAction action = PointerAction::Down,
                                PointerType type = PointerType::Mouse,
                                PointerButton button = PointerButton::Left) {
    PointerEvent ev;
    ev.x = x;
    ev.y = y;
    ev.rawX = x;
    ev.rawY = y;
    ev.action = action;
    ev.type = type;
    ev.timestampMs = 0.0;
    ev.button = button;
    return ev;
}

// -------------------------------------------------------------------------
// 1. Test Chord Qualities and Interval Definitions
// -------------------------------------------------------------------------
void testChordQualities() {
    std::cout << "[Test 1/8] Chord Qualities and Theory Intervals..." << std::endl;

    // Major Triad: [0, 4, 7]
    auto majIntervals = getChordQualityIntervals(ChordQuality::Major);
    assert(majIntervals.size() == 3);
    assert(majIntervals[0] == 0 && majIntervals[1] == 4 && majIntervals[2] == 7);

    // Minor Triad: [0, 3, 7]
    auto minIntervals = getChordQualityIntervals(ChordQuality::Minor);
    assert(minIntervals.size() == 3);
    assert(minIntervals[0] == 0 && minIntervals[1] == 3 && minIntervals[2] == 7);

    // Dominant 7th: [0, 4, 7, 10]
    auto dom7Intervals = getChordQualityIntervals(ChordQuality::Dominant7);
    assert(dom7Intervals.size() == 4);
    assert(dom7Intervals[3] == 10);

    // Major 7th: [0, 4, 7, 11]
    auto maj7Intervals = getChordQualityIntervals(ChordQuality::Major7);
    assert(maj7Intervals.size() == 4);
    assert(maj7Intervals[3] == 11);

    // Diminished: [0, 3, 6]
    auto dimIntervals = getChordQualityIntervals(ChordQuality::Diminished);
    assert(dimIntervals.size() == 3);
    assert(dimIntervals[1] == 3 && dimIntervals[2] == 6);

    // Sus4: [0, 5, 7]
    auto sus4Intervals = getChordQualityIntervals(ChordQuality::Sus4);
    assert(sus4Intervals.size() == 3);
    assert(sus4Intervals[1] == 5 && sus4Intervals[2] == 7);

    // Follow Mode strings
    assert(std::string(getChordFollowModeName(ChordFollowMode::Off)) == "Off");
    assert(std::string(getChordFollowModeName(ChordFollowMode::Chord)) == "Chord");
    assert(std::string(getChordFollowModeName(ChordFollowMode::Bass)) == "Bass");
    assert(std::string(getChordFollowModeName(ChordFollowMode::Scale)) == "Scale");
    assert(std::string(getChordFollowModeName(ChordFollowMode::ColorLead)) == "ColorLead");

    assert(parseChordFollowMode("Bass") == ChordFollowMode::Bass);
    assert(parseChordFollowMode("Scale") == ChordFollowMode::Scale);
    assert(parseChordFollowMode("ColorLead") == ChordFollowMode::ColorLead);

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 2. Test Circle of Fifths Math and Sector Order
// -------------------------------------------------------------------------
void testCircleOfFifths() {
    std::cout << "[Test 2/8] Circle of Fifths Clockwise Order & Geometry..." << std::endl;

    // 12 Major sectors starting at 12 o'clock clockwise:
    // C, G, D, A, E, B, F#, Db, Ab, Eb, Bb, F
    const auto& maj = ChordTheory::circleOfFifthsMajor;
    assert(maj[0] == 0);   // C
    assert(maj[1] == 7);   // G
    assert(maj[2] == 2);   // D
    assert(maj[3] == 9);   // A
    assert(maj[4] == 4);   // E
    assert(maj[5] == 11);  // B
    assert(maj[6] == 6);   // F# / Gb
    assert(maj[7] == 1);   // Db
    assert(maj[8] == 8);   // Ab
    assert(maj[9] == 3);   // Eb
    assert(maj[10] == 10); // Bb
    assert(maj[11] == 5);  // F

    // Relative minor of C is Am (9), of G is Em (4), etc.
    const auto& min = ChordTheory::circleOfFifthsMinor;
    assert(min[0] == 9);   // Am
    assert(min[1] == 4);   // Em
    assert(min[2] == 11);  // Bm
    assert(min[11] == 2);  // Dm

    (void)maj;
    (void)min;

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 3. Test Roman Numeral Analysis
// -------------------------------------------------------------------------
void testRomanNumerals() {
    std::cout << "[Test 3/8] Diatonic Roman Numeral Analysis..." << std::endl;

    // Key: C Major (root = 0, isMinor = false)
    assert(ChordTheory::getRomanNumeral(0, false, 0, ChordQuality::Major) == "I");
    assert(ChordTheory::getRomanNumeral(0, false, 2, ChordQuality::Minor) == "ii");
    assert(ChordTheory::getRomanNumeral(0, false, 4, ChordQuality::Minor) == "iii");
    assert(ChordTheory::getRomanNumeral(0, false, 5, ChordQuality::Major) == "IV");
    assert(ChordTheory::getRomanNumeral(0, false, 7, ChordQuality::Major) == "V");
    assert(ChordTheory::getRomanNumeral(0, false, 9, ChordQuality::Minor) == "vi");
    assert(ChordTheory::getRomanNumeral(0, false, 11, ChordQuality::Diminished) == "vii°");

    // Key: A Minor (root = 9, isMinor = true)
    assert(ChordTheory::getRomanNumeral(9, true, 9, ChordQuality::Minor) == "i");
    assert(ChordTheory::getRomanNumeral(9, true, 0, ChordQuality::Major) == "III");
    assert(ChordTheory::getRomanNumeral(9, true, 2, ChordQuality::Minor) == "iv");
    assert(ChordTheory::getRomanNumeral(9, true, 4, ChordQuality::Minor) == "v");
    assert(ChordTheory::getRomanNumeral(9, true, 5, ChordQuality::Major) == "VI");
    assert(ChordTheory::getRomanNumeral(9, true, 7, ChordQuality::Major) == "VII");

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 4. Test Curated Progression Presets (28 Presets)
// -------------------------------------------------------------------------
void testProgressionPresets() {
    std::cout << "[Test 4/8] Curated Progression Preset Library..." << std::endl;

    const auto& presets = ChordTheory::getProgressionPresets();
    assert(presets.size() == 28);

    bool foundPopClassic = false;
    bool foundSynthwave = false;
    bool foundJazzAutumn = false;

    for (const auto& p : presets) {
        assert(!p.id.empty());
        assert(!p.name.empty());
        assert(!p.genre.empty());
        assert(!p.chords.empty());

        if (p.id == "pop_classic") foundPopClassic = true;
        if (p.id == "synthwave_retro") foundSynthwave = true;
        if (p.id == "jazz_autumn_leaves") foundJazzAutumn = true;
    }

    assert(foundPopClassic);
    assert(foundSynthwave);
    assert(foundJazzAutumn);

    std::cout << "  -> Passed (28 verified presets)." << std::endl;
}

// -------------------------------------------------------------------------
// 5. Test Pitch Chord Detection and Extraction
// -------------------------------------------------------------------------
void testChordDetection() {
    std::cout << "[Test 5/8] Pitch Chord Detection from MIDI..." << std::endl;

    int root = -1, bass = -1;
    ChordQuality quality = ChordQuality::Major;

    // Detect C Major: [60, 64, 67]
    bool ok = ChordTheory::detectChordFromPitches({60, 64, 67}, root, quality, bass);
    assert(ok);
    assert(root == 0); // C
    assert(quality == ChordQuality::Major);

    // Detect A Minor: [57, 60, 64]
    ok = ChordTheory::detectChordFromPitches({57, 60, 64}, root, quality, bass);
    assert(ok);
    assert(root == 9); // A
    assert(quality == ChordQuality::Minor);

    // Detect G Dominant 7th: [55, 59, 62, 65]
    ok = ChordTheory::detectChordFromPitches({55, 59, 62, 65}, root, quality, bass);
    assert(ok);
    assert(root == 7); // G
    assert(quality == ChordQuality::Dominant7);

    // Detect Slash Chord C/E: [52, 60, 64, 67] (E in bass)
    ok = ChordTheory::detectChordFromPitches({52, 60, 64, 67}, root, quality, bass);
    assert(ok);
    assert(root == 0); // C
    assert(quality == ChordQuality::Major);
    assert(bass == 4); // E

    // Extract Chords From Notes
    std::vector<TheoryNote> notes = {
        {60, 0.0f, 4.0f, 0.8f},  // C in bar 0
        {64, 0.0f, 4.0f, 0.8f},  // E in bar 0
        {67, 0.0f, 4.0f, 0.8f},  // G in bar 0
        {57, 16.0f, 4.0f, 0.8f}, // A in bar 1
        {60, 16.0f, 4.0f, 0.8f}, // C in bar 1
        {64, 16.0f, 4.0f, 0.8f}  // E in bar 1
    };

    auto extracted = ChordTheory::extractChordsFromNotes(notes, 0, 2, 16);
    assert(extracted.size() == 2);
    assert(extracted[0].rootPitchClass == 0 && extracted[0].quality == ChordQuality::Major);
    assert(extracted[1].rootPitchClass == 9 && extracted[1].quality == ChordQuality::Minor);

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 6. Test Harmonic Remapping
// -------------------------------------------------------------------------
void testHarmonicRemapping() {
    std::cout << "[Test 6/8] Harmonic Remapping & Follow Modes..." << std::endl;

    ChordEvent cMaj{"c", 0, 1.0f, 0, ChordQuality::Major, -1}; // C Major: C(0), E(4), G(7)

    // Mode: Off -> pitch remains unchanged
    assert(ChordTheory::remapPitchForChord(61, cMaj, ChordFollowMode::Off) == 61);

    // Mode: Chord -> snaps C# (61) to closest chord tone (C = 60)
    int remapped = ChordTheory::remapPitchForChord(61, cMaj, ChordFollowMode::Chord);
    assert(remapped == 60 || remapped == 64);

    // Mode: Bass -> snaps note to bass note C in appropriate octave
    int bassRemapped = ChordTheory::remapPitchForChord(65, cMaj, ChordFollowMode::Bass);
    assert((bassRemapped % 12) == 0); // Root C

    // Slash Chord C/E -> Bass mode snaps to E (4)
    ChordEvent cSlashE{"c_e", 0, 1.0f, 0, ChordQuality::Major, 4};
    int slashBass = ChordTheory::remapPitchForChord(65, cSlashE, ChordFollowMode::Bass);
    assert((slashBass % 12) == 4); // Bass E

    (void)remapped;
    (void)bassRemapped;
    (void)slashBass;

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 7. Test ArrangerView Chord Track Integration & Bake
// -------------------------------------------------------------------------
void testArrangerChordTrack() {
    std::cout << "[Test 7/8] ArrangerView Chord Track Integration..." << std::endl;

    ArrangerView arranger;
    arranger.layout(Rect2D(0, 0, 1280, 800), ViewContext{});

    // Initial progression: 8 chords
    const auto& chords = arranger.getChordTrack();
    assert(chords.size() == 8);
    assert(chords[0].rootPitchClass == 0); // C Major
    assert(chords[1].rootPitchClass == 7); // G Major
    assert(chords[2].rootPitchClass == 9); // A Minor
    assert(chords[3].rootPitchClass == 5); // F Major

    // Active chord lookup at bar
    const auto* c0 = arranger.getActiveChordAtBar(0.5f);
    assert(c0 != nullptr && c0->rootPitchClass == 0);

    const auto* c2 = arranger.getActiveChordAtBar(2.5f);
    assert(c2 != nullptr && c2->rootPitchClass == 7);

    // Add or Update Chord
    ChordEvent customChord{"custom_1", 20, 2.0f, 2, ChordQuality::Minor7, -1}; // Dm7 at bar 20
    arranger.addOrUpdateChord(customChord);
    const auto* cCustom = arranger.getActiveChordAtBar(20.5f);
    assert(cCustom != nullptr && cCustom->rootPitchClass == 2);

    // Remove Chord
    arranger.removeChord("custom_1");
    assert(arranger.getActiveChordAtBar(20.5f) == nullptr);

    // Song Key Setting
    arranger.setSongKey(9, true); // A Minor
    assert(arranger.getSongKeyRoot() == 9);
    assert(arranger.isSongKeyMinor() == true);
    assert(arranger.getSongKeyName() == "A Minor");

    // Apply progression preset
    const auto& presets = ChordTheory::getProgressionPresets();
    arranger.applyChordProgressionPreset(presets[0], 0); // Pop classic
    assert(!arranger.getChordTrack().empty());

    // Bake to MIDI
    arranger.addTrack("Synth Lead", "PolySynth", 0.9f, 0.4f, 0.2f);
    auto& trk = arranger.getTracks().back();
    trk.chordFollowMode = ChordFollowMode::Chord;
    trk.clips.clear();

    ArrangerTimelineClip cl;
    cl.id = "test_clip";
    cl.startBar = 1;
    cl.lengthBars = 4;
    // Note C#4 (61) which is not in C Major
    ArrangerClipNote note;
    note.pitch = 61;
    note.startBeat = 0.0f;
    note.lengthBeats = 1.0f;
    note.velocity = 0.8f;
    cl.notes.push_back(note);
    trk.clips.push_back(cl);

    arranger.bakeChordsToTrack(static_cast<uint32_t>(arranger.getTracks().size() - 1));
    // Verify follow mode reset to Off and note remapped
    assert(arranger.getTracks().back().chordFollowMode == ChordFollowMode::Off);
    assert(arranger.getTracks().back().clips[0].notes[0].pitch != 61); // Conformed to chord tone

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 8. Test CircleOfFifthsDialog Modal
// -------------------------------------------------------------------------
void testCircleOfFifthsDialog() {
    std::cout << "[Test 8/8] CircleOfFifthsDialog Modal Interactions..." << std::endl;

    CircleOfFifthsDialog dialog;
    dialog.layout(1280.0f, 800.0f);
    assert(!dialog.isOpen());

    // Open at bar 0 in C Major
    dialog.open(0, false, 0);
    assert(dialog.isOpen());
    assert(dialog.getSelectedRoot() == 0);
    assert(!dialog.isMinorRingSelected());

    // Set Quality
    dialog.setSelectedQuality(ChordQuality::Major7);
    assert(dialog.getSelectedQuality() == ChordQuality::Major7);

    // Audition callback trigger
    bool auditioned = false;
    dialog.onAuditionChord = [&](const ChordEvent& chord) {
        auditioned = true;
        assert(chord.rootPitchClass == 0);
        assert(chord.quality == ChordQuality::Major7);
    };
    dialog.auditionCurrentChord();
    assert(auditioned);

    // Apply Chord callback trigger
    bool applied = false;
    dialog.onChordApplied = [&](const ChordEvent& chord) {
        applied = true;
        assert(chord.rootPitchClass == 0);
        assert(chord.startBar == 0);
    };

    // Click Apply button
    const auto& b = dialog.getBounds();
    float applyBtnX = b.x + b.w - 110.0f;
    float applyBtnY = b.y + b.h - 38.0f;
    dialog.handlePointer(makePointer(applyBtnX + 5.0f, applyBtnY + 5.0f, PointerAction::Down));
    assert(applied);
    assert(!dialog.isOpen()); // Modal closes after apply

    std::cout << "  -> Passed." << std::endl;
}

// -------------------------------------------------------------------------
// 9. Test Project File Serialization Roundtrip with Chord Track
// -------------------------------------------------------------------------
#include "eatsbits/project/project_file.hpp"

void testProjectSerializationRoundtrip() {
    std::cout << "[Test 9/9] Project Serialization (.eats JSON) Roundtrip..." << std::endl;

    audio::AudioGraph origGraph;
    sequencer::StepSequencer origSeq;

    std::vector<ChordEvent> origChords = {
        {"c0", 0, 2.0f, 0, ChordQuality::Major, -1},  // C Major
        {"c1", 2, 2.0f, 7, ChordQuality::Major, -1},  // G Major
        {"c2", 4, 2.0f, 9, ChordQuality::Minor, -1},  // A Minor
        {"c3", 6, 2.0f, 5, ChordQuality::Major, -1}   // F Major
    };

    std::string jsonStr = project::ProjectFile::serializeJson(
        origGraph, origSeq, "Chords Anthem", 128.0, 0.54, 0, false, origChords);

    assert(jsonStr.find("\"chordTrack\"") != std::string::npos);
    assert(jsonStr.find("\"songKeyRoot\"") != std::string::npos);

    audio::AudioGraph loadedGraph;
    sequencer::StepSequencer loadedSeq;
    std::string loadedTitle;
    double loadedBpm = 0.0;
    double loadedSwing = 0.0;
    int loadedKeyRoot = -1;
    bool loadedIsMinor = true;
    std::vector<ChordEvent> loadedChords;

    bool ok = project::ProjectFile::deserializeJson(
        jsonStr, loadedGraph, loadedSeq, loadedTitle, loadedBpm, loadedSwing,
        loadedKeyRoot, loadedIsMinor, loadedChords);

    assert(ok);
    assert(loadedTitle == "Chords Anthem");
    assert(std::abs(loadedBpm - 128.0) < 0.01);
    assert(loadedKeyRoot == 0);
    assert(!loadedIsMinor);
    assert(loadedChords.size() == 4);
    assert(loadedChords[0].rootPitchClass == 0 && loadedChords[0].quality == ChordQuality::Major);
    assert(loadedChords[1].rootPitchClass == 7 && loadedChords[1].quality == ChordQuality::Major);
    assert(loadedChords[2].rootPitchClass == 9 && loadedChords[2].quality == ChordQuality::Minor);
    assert(loadedChords[3].rootPitchClass == 5 && loadedChords[3].quality == ChordQuality::Major);

    std::cout << "  -> Passed (Complete lossless chord track serialization)." << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "  EATSBITS CHORD TRACK & HARMONIC ENGINE TESTS   " << std::endl;
    std::cout << "=================================================" << std::endl;

    testChordQualities();
    testCircleOfFifths();
    testRomanNumerals();
    testProgressionPresets();
    testChordDetection();
    testHarmonicRemapping();
    testArrangerChordTrack();
    testCircleOfFifthsDialog();
    testProjectSerializationRoundtrip();

    std::cout << "=================================================" << std::endl;
    std::cout << "  ALL 9 CHORD TRACK TEST SUITES PASSED (100%)    " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
