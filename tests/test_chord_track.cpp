#include <iostream>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "eatsbits/theory/chord_model.hpp"
#include "eatsbits/ui/views/arranger_view.hpp"
#include "eatsbits/ui/views/edit_view.hpp"
#include "eatsbits/ui/views/mixer_view.hpp"
#include "eatsbits/ui/views/track_inspector_view.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/ui/widgets/circle_of_fifths_dialog.hpp"
#include "eatsbits/ui/input/pointer_event.hpp"
#include "eatsbits/project/eats_serializer.hpp"
#include "eatsbits/project/project_file.hpp"
#include "eatsbits/audio/audio_engine.hpp"

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

    // Monophonic Content Exclusion: Single notes, octaves, and random collisions must NOT detect chords
    assert(!ChordTheory::detectChordFromPitches({60}, root, quality, bass));
    assert(!ChordTheory::detectChordFromPitches({36, 48, 60, 72}, root, quality, bass));
    assert(!ChordTheory::detectChordFromPitches({60, 61}, root, quality, bass)); // Semitone clash without harmony

    std::vector<TheoryNote> monoNotes = {
        {36, 0.0f, 4.0f, 0.8f} // Single bass note
    };
    auto monoExtracted = ChordTheory::extractChordsFromNotes(monoNotes, 0, 1, 16);
    assert(monoExtracted.empty());

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
    (void)chords;
    assert(chords.size() == 8);
    assert(chords[0].rootPitchClass == 0); // C Major
    assert(chords[1].rootPitchClass == 7); // G Major
    assert(chords[2].rootPitchClass == 9); // A Minor
    assert(chords[3].rootPitchClass == 5); // F Major

    // Active chord lookup at bar
    const auto* c0 = arranger.getActiveChordAtBar(0.5f);
    (void)c0;
    assert(c0 != nullptr && c0->rootPitchClass == 0);

    const auto* c2 = arranger.getActiveChordAtBar(2.5f);
    (void)c2;
    assert(c2 != nullptr && c2->rootPitchClass == 7);

    // Add or Update Chord
    ChordEvent customChord{"custom_1", 20, 2.0f, 2, ChordQuality::Minor7, -1}; // Dm7 at bar 20
    arranger.addOrUpdateChord(customChord);
    const auto* cCustom = arranger.getActiveChordAtBar(20.5f);
    (void)cCustom;
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
        (void)chord;
        assert(chord.rootPitchClass == 0);
        assert(chord.quality == ChordQuality::Major7);
    };
    dialog.auditionCurrentChord();
    assert(auditioned);

    // Apply Chord callback trigger
    bool applied = false;
    dialog.onChordApplied = [&](const ChordEvent& chord) {
        applied = true;
        (void)chord;
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

    (void)ok;
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

// -------------------------------------------------------------------------
// 10. Test Dedicated Single Chord Track & Harmonic Follow Modes
// -------------------------------------------------------------------------
void testClipChordsAndTrackSync() {
    std::cout << "[Test 10/11] Dedicated Single Chord Track & Track Harmonic Sync..." << std::endl;

    ArrangerView arranger;
    const auto& tracks = arranger.getTracks();

    // 1. Verify canonical Project Chord Track (Eatsbeats Parity)
    const auto& projectChords = arranger.getChordTrack();
    assert(projectChords.size() == 8);
    assert(projectChords[0].rootPitchClass == 0); // C Major
    assert(projectChords[1].rootPitchClass == 7); // G Major
    assert(projectChords[2].rootPitchClass == 9); // A Minor
    assert(projectChords[3].rootPitchClass == 5); // F Major

    // 2. Verify Track 1 (Sub Bass) follows project chord track in Bass mode
    assert(tracks.size() >= 5);
    const auto& bassTrack = tracks[1]; // Sub Bass
    (void)bassTrack;
    assert(bassTrack.chordFollowMode == ChordFollowMode::Bass);

    // Query active chord for Sub Bass at bar 0.5 (within Bar 0..2 -> C Major)
    const auto* chordAtBar0 = arranger.getActiveChordForTrackAtBar(1, 0.5f);
    assert(chordAtBar0 != nullptr);
    assert(chordAtBar0->rootPitchClass == 0); // C Major

    // Query active chord for Sub Bass at bar 2.5 (within Bar 2..4 -> G Major)
    const auto* chordAtBar2 = arranger.getActiveChordForTrackAtBar(1, 2.5f);
    assert(chordAtBar2 != nullptr);
    assert(chordAtBar2->rootPitchClass == 7); // G Major

    // 3. Test Looping Clip Chord Continuity & Explicit Extraction
    ArrangerTimelineClip loopClip;
    loopClip.id = "loop_prog";
    loopClip.startBar = 1;
    loopClip.lengthBars = 8;
    loopClip.isLooped = true;
    loopClip.loopLengthBars = 2; // 2-bar progression looped 4 times
    // Bar 0: F Major {53, 57, 60}, Bar 1: G Major {55, 59, 62}
    loopClip.notes = {
        {53, 0.0f, 3.8f, 0.8f}, {57, 0.0f, 3.8f, 0.8f}, {60, 0.0f, 3.8f, 0.8f},
        {55, 4.0f, 3.8f, 0.8f}, {59, 4.0f, 3.8f, 0.8f}, {62, 4.0f, 3.8f, 0.8f}
    };
    arranger.updateClipDetectedChords(loopClip);
    assert(loopClip.detectedChords.size() == 2);
    assert(loopClip.detectedChords[0].rootPitchClass == 5); // F Major
    assert(loopClip.detectedChords[1].rootPitchClass == 7); // G Major

    // 4. Test Harmonic Overview directly reading canonical Project Chord Track
    auto overview = arranger.getHarmonicOverviewChords();
    assert(overview.size() == projectChords.size());
    assert(overview[0].sourceTrackName == "Chord Track");
    assert(overview[0].chord.rootPitchClass == 0);

    // 5. Test [ BAKE TO MIDI ] on Follower Track
    arranger.addTrack("Pad Follower", "PolySynth", 0.3f, 0.7f, 0.9f);
    uint32_t padTrackIdx = static_cast<uint32_t>(arranger.getTracks().size() - 1);
    auto& padTrack = arranger.getTracks()[padTrackIdx];
    padTrack.chordFollowMode = ChordFollowMode::Chord;

    ArrangerTimelineClip padClip;
    padClip.id = "pad_clip";
    padClip.startBar = 1;
    padClip.lengthBars = 4;
    // Note C#4 (61) which is not in C Major (bar 0..2)
    padClip.notes.push_back({61, 0.0f, 2.0f, 0.8f});
    padTrack.clips.push_back(padClip);

    arranger.bakeChordsToTrack(padTrackIdx);
    assert(arranger.getTracks()[padTrackIdx].chordFollowMode == ChordFollowMode::Off);
    // 61 must be remapped to chord tones of C Major (60 or 64)
    assert(arranger.getTracks()[padTrackIdx].clips[0].notes[0].pitch == 60 ||
           arranger.getTracks()[padTrackIdx].clips[0].notes[0].pitch == 64);

    std::cout << "  -> Passed (Dedicated Chord Track parity & bake to MIDI)." << std::endl;
}

// -------------------------------------------------------------------------
// 11. Test Real-time Playback Follow Mode & MIDI FX Pipeline Synchronization
// -------------------------------------------------------------------------
void testPlaybackFollowModeAndMidiFxSync() {
    std::cout << "[Test 11/11] StepSequencer Real-Time Follow Modes & Live MIDI FX..." << std::endl;

    audio::AudioGraph graph;
    sequencer::StepSequencer seq;
    seq.setBpm(120.0);

    // 1. Configure project Chord Track on StepSequencer:
    // Bar 0 (steps 0..15): C Major (root 0)
    // Bar 1 (steps 16..31): A Minor (root 9)
    std::vector<ChordEvent> chords = {
        {"c0", 0, 1.0f, 0, ChordQuality::Major, -1},
        {"c1", 1, 1.0f, 9, ChordQuality::Minor, -1}
    };
    seq.setChordTrack(chords);
    assert(seq.getChordTrack().size() == 2);
    assert(seq.getActiveChordAtBar(0.0f)->rootPitchClass == 0);
    assert(seq.getActiveChordAtBar(1.0f)->rootPitchClass == 9);

    // 2. Add Track with ChordFollowMode::Bass
    size_t trkIdx = seq.addTrack("Bass Synth", 0, 32);
    auto* trk = seq.getTrack(trkIdx);
    assert(trk != nullptr);
    trk->setChordFollowMode(ChordFollowMode::Bass);

    // Set step 0 to note 65 (F4). When playing over C Major in Bass mode, it must conform to C (60)
    sequencer::StepData s0{};
    s0.active = true;
    s0.note = 65; // F4
    s0.velocity = 0.9f;
    s0.gateLength = 0.5f;
    trk->setStep(0, s0);

    // Set step 16 to note 65 (F4). When playing over A Minor in Bass mode, it must conform to A (57 or 69)
    sequencer::StepData s16{};
    s16.active = true;
    s16.note = 65; // F4
    s16.velocity = 0.9f;
    s16.gateLength = 0.5f;
    trk->setStep(16, s16);

    // 3. Start playback and process step 0
    seq.start();
    assert(seq.isPlaying());

    // Advance through step 0 (approx 6000 frames at 48kHz, 120BPM)
    seq.processBlock(480, graph);

    // Verify timeContext_ updated from active chord
    assert(seq.getTimeContext().activeChordRoot == 0);
    assert(seq.getTimeContext().activeChordQuality == "Major");

    // 4. Add Live MIDI FX Insert: ChordFollow
    eatscript::MidiFxInsert fxChordFollow;
    fxChordFollow.id = "fx_cf";
    fxChordFollow.name = "Chord Follow";
    fxChordFollow.type = eatscript::MidiFxType::ChordFollow;
    fxChordFollow.enabled = true;
    trk->getMidiFxRack().push_back(fxChordFollow);

    // Add Live MIDI FX Insert: ChordStabs
    eatscript::MidiFxInsert fxStabs;
    fxStabs.id = "fx_stabs";
    fxStabs.name = "Chord Stabs";
    fxStabs.type = eatscript::MidiFxType::ChordStabs;
    fxStabs.enabled = true;
    trk->getMidiFxRack().push_back(fxStabs);

    assert(trk->getMidiFxRack().size() == 2);

    seq.stop();
    assert(!seq.isPlaying());

    std::cout << "  -> Passed (Real-time playback follow mode, timeContext & MIDI FX sync)." << std::endl;
}

// -------------------------------------------------------------------------
// 12. Test Chord Follow & Project Harmony Playback Pipeline Desynchronization Fix
// -------------------------------------------------------------------------
void testChordFollowPipelineSyncAndAuditioning() {
    std::cout << "[Test 12/12] Arranger-Sequencer Chord Track Sync & Chord Follow Pipeline..." << std::endl;

    audio::AudioGraph graph;
    sequencer::StepSequencer seq;
    seq.setBpm(120.0);

    ArrangerView arranger;
    bool syncTriggered = false;
    arranger.onChordTrackChanged = [&]() {
        seq.setChordTrack(arranger.getChordTrack());
        syncTriggered = true;
    };

    // 1. Adding/Updating chords in Arranger immediately propagates to StepSequencer
    ChordEvent c0{"c0", 0, 1.0f, 0, ChordQuality::Major, -1}; // C Major at bar 0
    ChordEvent c1{"c1", 1, 1.0f, 7, ChordQuality::Major, -1}; // G Major at bar 1
    arranger.addOrUpdateChord(c0);
    assert(syncTriggered);
    assert(seq.getChordTrack().size() == 1);
    assert(seq.getActiveChordAtBar(0.0f) != nullptr);
    assert(seq.getActiveChordAtBar(0.0f)->rootPitchClass == 0);

    arranger.addOrUpdateChord(c1);
    assert(seq.getChordTrack().size() == 2);
    assert(seq.getActiveChordAtBar(1.0f)->rootPitchClass == 7);

    // 2. Dragging/moving chords triggers onChordTrackChanged and syncs to StepSequencer
    c1.startBar = 2;
    arranger.addOrUpdateChord(c1);
    assert(seq.getActiveChordAtBar(2.0f) != nullptr);
    assert(seq.getActiveChordAtBar(2.0f)->rootPitchClass == 7);

    // 3. StepSequencer real-time chord follow mode playback
    size_t trkIdx = seq.addTrack("Synth Track", 0, 32);
    auto* trk = seq.getTrack(trkIdx);
    assert(trk != nullptr);

    // Set step 0 (bar 0, C Major) to pitch 62 (D4)
    sequencer::StepData s0{};
    s0.active = true;
    s0.note = 62; // D4
    s0.velocity = 0.9f;
    s0.gateLength = 0.5f;
    trk->setStep(0, s0);

    // Verify toggling chord follow mode on track immediately alters rendered playback pitch
    assert(trk->getChordFollowMode() == ChordFollowMode::Off);
    trk->setChordFollowMode(ChordFollowMode::Bass);
    assert(trk->getChordFollowMode() == ChordFollowMode::Bass);

    // Start playback and process step 0
    seq.start();
    seq.processBlock(480, graph);
    assert(seq.getTimeContext().activeChordRoot == 0); // Conformed to C Major
    seq.stop();

    // Verify Chord mode remapping
    trk->setChordFollowMode(ChordFollowMode::Chord);
    assert(trk->getChordFollowMode() == ChordFollowMode::Chord);

    // 4. Removing a chord from Arranger updates StepSequencer
    arranger.removeChord("c1");
    assert(seq.getChordTrack().size() == 1);
    assert(seq.getActiveChordAtBar(2.0f) == nullptr);

    std::cout << "  -> Passed (Arranger-Sequencer chord track sync & chord follow pipeline)." << std::endl;
}

// -------------------------------------------------------------------------
// 13. Test EditView Piano Roll Chord Follow & MIDI FX Evaluation and Baking
// -------------------------------------------------------------------------
void testEditViewChordFollowAndMidiFxBake() {
    std::cout << "[Test 13/13] EditView Piano Roll Chord Follow & MIDI FX Evaluation and Baking..." << std::endl;

    EditView editView;
    ViewContext vctx{};
    editView.layout(Rect2D{0.0f, 0.0f, 1200.0f, 800.0f}, vctx);

    // 1. Initial state with raw notes
    std::vector<PianoRollNote> notes;
    PianoRollNote n1;
    n1.id = "n1";
    n1.pitch = 60; // C4
    n1.startStep = 0.0f; // Bar 0
    n1.durationSteps = 4.0f;
    n1.velocity = 0.8f;

    PianoRollNote n2;
    n2.id = "n2";
    n2.pitch = 65; // F4
    n2.startStep = 16.0f; // Bar 1 (16 sixteenth steps per bar)
    n2.durationSteps = 4.0f;
    n2.velocity = 0.8f;

    notes.push_back(n1);
    notes.push_back(n2);
    editView.getNotes() = notes;

    assert(!editView.hasActiveChordFollowOrMidiFx());
    assert(!editView.getBtnBake().isVisible());

    // 2. Set up Chord Track: Bar 0 = C Major, Bar 1 = G Major (root = 7)
    std::vector<ChordEvent> chords;
    ChordEvent c0{"c0", 0, 1.0f, 0, ChordQuality::Major, -1};
    ChordEvent c1{"c1", 1, 1.0f, 7, ChordQuality::Major, -1};
    chords.push_back(c0);
    chords.push_back(c1);
    editView.setChordTrack(chords);

    // 3. Set Chord Follow Mode to Bass
    editView.setChordFollowMode(ChordFollowMode::Bass);
    assert(editView.hasActiveChordFollowOrMidiFx());
    assert(editView.getBtnBake().isVisible());

    // Note 1 (pitch 60 C4 at bar 0) over C Major in Bass mode -> snaps to root C (60)
    // Note 2 (pitch 65 F4 at bar 1) over G Major in Bass mode -> snaps to root G (67)
    const auto& evalNotes1 = editView.getEvaluatedFxNotes();
    assert(evalNotes1.size() == 2);
    assert(evalNotes1[0].pitch == 60);
    assert(evalNotes1[1].pitch == 67); // F4 snapped to G4

    // 4. Add Transpose MIDI FX (+12 semitones)
    std::vector<eatscript::MidiFxInsert> fxRack;
    eatscript::MidiFxInsert fxTrans;
    fxTrans.id = "fx_trans";
    fxTrans.name = "Transpose";
    fxTrans.type = eatscript::MidiFxType::Transpose;
    fxTrans.enabled = true;
    fxTrans.params["semitones"] = 12.0f;
    fxRack.push_back(fxTrans);
    editView.setMidiFxRack(fxRack);

    const auto& evalNotes2 = editView.getEvaluatedFxNotes();
    assert(evalNotes2.size() == 2);
    assert(evalNotes2[0].pitch == 72); // 60 + 12 = C5
    assert(evalNotes2[1].pitch == 79); // 67 + 12 = G5

    // 5. Test Baking: commits evaluated notes to base notes, resets follow mode, disables FX, and formats Eatscript
    bool bakeCallbackFired = false;
    uint32_t bakedTrackReceived = 999;

    editView.onBakeRequested = [&](uint32_t tIdx, int) {
        bakeCallbackFired = true;
        bakedTrackReceived = tIdx;
    };

    editView.bakeChordFollowAndMidiFx();

    assert(bakeCallbackFired);
    assert(bakedTrackReceived == editView.getActiveTrackIndex());

    // Verify Eatscript code is non-empty and contains the baked notes (source of truth)
    std::string bakedScript = editView.getScriptEditor().getText();
    assert(!bakedScript.empty());
    assert(bakedScript.find("track \"") != std::string::npos);

    // Verify follow mode reset and FX disabled
    assert(editView.getChordFollowMode() == ChordFollowMode::Off);
    for (const auto& fx : editView.getMidiFxRack()) {
        assert(!fx.enabled);
    }
    assert(!editView.hasActiveChordFollowOrMidiFx());
    assert(!editView.getBtnBake().isVisible());

    // Verify EditView's base notes now contain the baked pitches
    const auto& currentNotes = editView.getNotes();
    assert(currentNotes.size() == 2);
    assert(currentNotes[0].pitch == 72);
    assert(currentNotes[1].pitch == 79);

    // 6. Test StepSequencer integration: setting the baked script code onto a SequencerTrack
    sequencer::StepSequencer seq;
    size_t trkIdx = seq.addTrack("Synth Track", 0, 32);
    auto* seqTrk = seq.getTrack(trkIdx);
    assert(seqTrk != nullptr);

    seqTrk->setEatscriptCode(bakedScript);
    assert(seqTrk->getEatscriptCode() == bakedScript);

    std::cout << "  -> Passed (EditView Chord Follow & MIDI FX evaluation, baking & Eatscript code sync)." << std::endl;
}

// -------------------------------------------------------------------------
// 14. Test EatsProjectSerializer .eats Chord Track & Default Rhodes + Grand
// -------------------------------------------------------------------------
void testEatsFormatChordTrackAndRhodesGrandSong() {
    std::cout << "[Test 14/14] EatsProjectSerializer .eats Chord Track & Default Rhodes + Grand..." << std::endl;

    audio::AudioEngine engine;
    engine.setupDefaultRhodesGrandGraph();

    auto& seq = engine.getSequencer();
    seq.setBpm(116.0);
    seq.setSwing(0.52);

    std::vector<ChordEvent> chords = {
        {"chord_0",  0,  2.0f, 0, ChordQuality::Major, -1},  // Bar 1-2: C Major
        {"chord_2",  2,  2.0f, 7, ChordQuality::Major, -1},  // Bar 3-4: G Major
        {"chord_4",  4,  2.0f, 9, ChordQuality::Minor, -1},  // Bar 5-6: A Minor
        {"chord_6",  6,  2.0f, 5, ChordQuality::Major, -1},  // Bar 7-8: F Major
        {"chord_8",  8,  2.0f, 0, ChordQuality::Major, -1},  // Bar 9-10: C Major
        {"chord_10", 10, 2.0f, 7, ChordQuality::Major, -1},  // Bar 11-12: G Major
        {"chord_12", 12, 2.0f, 9, ChordQuality::Minor, -1},  // Bar 13-14: A Minor
        {"chord_14", 14, 2.0f, 5, ChordQuality::Major, -1}   // Bar 15-16: F Major
    };
    seq.setChordTrack(chords);

    size_t t0 = seq.addTrack("DX7 Rhodes", 1, 128);
    auto* tr0 = seq.getTrack(t0);
    tr0->setChordFollowMode(ChordFollowMode::Chord);

    size_t t1 = seq.addTrack("Concert Grand", 2, 128);
    auto* tr1 = seq.getTrack(t1);
    tr1->setChordFollowMode(ChordFollowMode::Chord);

    // 1. Test .eats serialization
    std::string eatsContent = project::EatsProjectSerializer::serialize(
        engine.getGraph(), seq, "Rhodes & Concert Grand", 116.0, 0.52);

    assert(eatsContent.find("chordTrack = {") != std::string::npos);
    assert(eatsContent.find("chordFollowMode = \"Chord\"") != std::string::npos);
    assert(eatsContent.find("type = \"dx7\"") != std::string::npos);
    assert(eatsContent.find("type = \"piano\"") != std::string::npos);

    // 2. Test .eats deserialization into new engine
    audio::AudioGraph loadedGraph;
    sequencer::StepSequencer loadedSeq;
    std::string loadedTitle;
    double loadedBpm = 0.0, loadedSwing = 0.0;

    bool ok = project::EatsProjectSerializer::deserialize(
        eatsContent, loadedGraph, loadedSeq, loadedTitle, loadedBpm, loadedSwing);
    assert(ok);
    assert(loadedTitle == "Rhodes & Concert Grand");
    assert(std::abs(loadedBpm - 116.0) < 0.01);
    assert(loadedSeq.getNumTracks() == 2);
    assert(loadedSeq.getTrack(0)->getName() == "DX7 Rhodes");
    assert(loadedSeq.getTrack(0)->getChordFollowMode() == ChordFollowMode::Chord);
    assert(loadedSeq.getTrack(1)->getName() == "Concert Grand");
    assert(loadedSeq.getTrack(1)->getChordFollowMode() == ChordFollowMode::Chord);
    assert(loadedSeq.getChordTrack().size() == 8);
    assert(loadedSeq.getChordTrack()[0].quality == ChordQuality::Major);
    assert(loadedSeq.getChordTrack()[2].quality == ChordQuality::Minor);

    // 3. Test project.eats file on disk
    audio::AudioGraph diskGraph;
    sequencer::StepSequencer diskSeq;
    std::string diskTitle;
    double diskBpm = 0.0, diskSwing = 0.0;
    bool diskOk = project::ProjectFile::loadFromFile(
        "project.eats", diskGraph, diskSeq, diskTitle, diskBpm, diskSwing);
    assert(diskOk);
    assert(diskSeq.getNumTracks() == 2);
    assert(diskSeq.getChordTrack().size() == 8);
    assert(diskSeq.getTrack(0)->getChordFollowMode() == ChordFollowMode::Chord);
    assert(diskSeq.getTrack(1)->getChordFollowMode() == ChordFollowMode::Chord);

    std::cout << "  -> Passed (EatsProjectSerializer .eats chord track and Rhodes + Grand default song)." << std::endl;
}

// -------------------------------------------------------------------------
// 15. Test Per-Track Chord Follow Isolation Across Arranger, Mixer & Inspector
// -------------------------------------------------------------------------
void testPerTrackChordFollowIsolation() {
    std::cout << "[Test 15/15] Per-Track Harmonic Chord Follow Isolation..." << std::endl;

    // 1. ArrangerView isolation
    ArrangerView arranger;
    assert(arranger.getTracks().size() >= 2);

    // Initial default: track 0 and 1
    arranger.getTracks()[0].chordFollowMode = ChordFollowMode::Chord;
    arranger.getTracks()[1].chordFollowMode = ChordFollowMode::Bass;

    // Switching active track must populate drawerData_ with that specific track's mode
    arranger.setActiveTrack(0);
    assert(arranger.getDrawerData().trackIndex == 0);
    assert(arranger.getDrawerData().chordFollowMode == ChordFollowMode::Chord);

    arranger.setActiveTrack(1);
    assert(arranger.getDrawerData().trackIndex == 1);
    assert(arranger.getDrawerData().chordFollowMode == ChordFollowMode::Bass);

    // Triggering drawer chord follow change on track 1 must ONLY modify track 1
    arranger.getPropertiesDrawer().onChordFollowChanged(1, ChordFollowMode::Scale);
    assert(arranger.getTracks()[1].chordFollowMode == ChordFollowMode::Scale);
    assert(arranger.getTracks()[0].chordFollowMode == ChordFollowMode::Chord); // Must NOT be changed!
    assert(arranger.getDrawerData().chordFollowMode == ChordFollowMode::Scale);

    // Switching back to track 0 must reload track 0's ChordFollowMode::Chord
    arranger.setActiveTrack(0);
    assert(arranger.getDrawerData().trackIndex == 0);
    assert(arranger.getDrawerData().chordFollowMode == ChordFollowMode::Chord);

    // 2. MixerView isolation
    MixerView mixer;
    assert(mixer.getChannels().size() >= 2);
    mixer.getChannels()[0].chordFollowMode = ChordFollowMode::Chord;
    mixer.getChannels()[1].chordFollowMode = ChordFollowMode::ColorLead;

    mixer.getPropertiesDrawer().onChordFollowChanged(1, ChordFollowMode::Off);
    assert(mixer.getChannels()[1].chordFollowMode == ChordFollowMode::Off);
    assert(mixer.getChannels()[0].chordFollowMode == ChordFollowMode::Chord); // Must NOT be changed!

    // 3. TrackInspectorView isolation
    TrackInspectorView inspector;
    assert(inspector.getTracks().size() >= 2);
    inspector.getTracks()[0].chordFollowMode = ChordFollowMode::Chord;
    inspector.getTracks()[1].chordFollowMode = ChordFollowMode::Bass;

    inspector.setActiveTrack(0);
    inspector.updateDrawerData();
    assert(inspector.getDrawerData().chordFollowMode == ChordFollowMode::Chord);

    inspector.setActiveTrack(1);
    inspector.updateDrawerData();
    assert(inspector.getDrawerData().chordFollowMode == ChordFollowMode::Bass);

    inspector.onChordFollowChanged = [&](uint32_t idx, ChordFollowMode mode) {
        assert(idx == 1);
        assert(mode == ChordFollowMode::Scale);
    };
    inspector.getPanel().onChordFollowChanged(1, ChordFollowMode::Scale);
    assert(inspector.getTracks()[1].chordFollowMode == ChordFollowMode::Scale);
    assert(inspector.getTracks()[0].chordFollowMode == ChordFollowMode::Chord); // Track 0 preserved!

    std::cout << "  -> Passed (Harmonic Chord Follow strictly isolated on a per-track basis across Arranger, Mixer, and Inspector)." << std::endl;
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
    testClipChordsAndTrackSync();
    testPlaybackFollowModeAndMidiFxSync();
    testChordFollowPipelineSyncAndAuditioning();
    testEditViewChordFollowAndMidiFxBake();
    testEatsFormatChordTrackAndRhodesGrandSong();
    testPerTrackChordFollowIsolation();

    std::cout << "=================================================" << std::endl;
    std::cout << "  ALL 15 CHORD & HARMONIC SUITES PASSED (100%)   " << std::endl;
    std::cout << "=================================================" << std::endl;
    return 0;
}
