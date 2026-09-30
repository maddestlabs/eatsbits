#include "eatsbits/eatscript/midi_fx_pipeline.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace eatsbits::eatscript;
using namespace eatsbits::sequencer;

void testMidiFxTypeDetection() {
    std::cout << "[Test 1] MidiFxType detection..." << std::endl;
    assert(MidiPipelineEngine::detectMidiFxType("def process(notes, ctx):", "") == MidiFxType::Eatscript);
    assert(MidiPipelineEngine::detectMidiFxType("", "Arpeggiator") == MidiFxType::Arpeggiator);
    assert(MidiPipelineEngine::detectMidiFxType("scale_snap(notes)", "") == MidiFxType::ScaleSnap);
    assert(MidiPipelineEngine::detectMidiFxType("", "Chord Follower") == MidiFxType::ChordFollow);
    assert(MidiPipelineEngine::detectMidiFxType("humanize(notes)", "") == MidiFxType::Humanize);
    assert(MidiPipelineEngine::detectMidiFxType("transpose(notes, 5)", "") == MidiFxType::Transpose);
    assert(MidiPipelineEngine::detectMidiFxType("", "Chord Voicing") == MidiFxType::ChordStabs);
    std::cout << "  Passed." << std::endl;
}

void testScaleSnap() {
    std::cout << "[Test 2] Scale Snap..." << std::endl;
    // C Major: C=60, D=62, E=64, F=65, G=67, A=69, B=71
    // 61 (C#4) snapped to C Major should snap to 60 (C4) or 62 (D4)
    int snapped1 = MidiPipelineEngine::snapToScale(61, 0, false);
    assert(snapped1 == 60 || snapped1 == 62);

    // C Natural Minor: C=60, D=62, Eb=63, F=65, G=67, Ab=68, Bb=70
    // 64 (E4) snapped to C Minor should snap to 63 (Eb4) or 65 (F4)
    int snapped2 = MidiPipelineEngine::snapToScale(64, 0, true);
    assert(snapped2 == 63 || snapped2 == 65);

    // In-scale note remains untouched
    int snappedInScale = MidiPipelineEngine::snapToScale(60, 0, false);
    assert(snappedInScale == 60);
    std::cout << "  Passed." << std::endl;
}

void testArpeggiator() {
    std::cout << "[Test 3] Arpeggiator..." << std::endl;
    std::vector<MidiNote> chord = {
        {"n1", 60, 0.0f, 4.0f, 0.9f}, // C4
        {"n2", 64, 0.0f, 4.0f, 0.9f}, // E4
        {"n3", 67, 0.0f, 4.0f, 0.9f}  // G4
    };

    // 16th note rate (1.0 step), 1 octave, "up"
    std::vector<MidiNote> arpedUp = MidiPipelineEngine::applyArpeggiator(chord, 1.0, 1, "up", 0.85, 0.0);
    assert(arpedUp.size() == 4);
    assert(arpedUp[0].pitch == 60);
    assert(arpedUp[1].pitch == 64);
    assert(arpedUp[2].pitch == 67);
    assert(arpedUp[3].pitch == 60); // Cycles back

    // 2 octaves "down"
    std::vector<MidiNote> arpedDown = MidiPipelineEngine::applyArpeggiator(chord, 1.0, 2, "down", 0.85, 0.0);
    assert(arpedDown.size() == 4);
    assert(arpedDown[0].pitch == 79); // G5 (67 + 12)
    assert(arpedDown[1].pitch == 76); // E5 (64 + 12)

    // "chord" pattern triggers all pitches simultaneously on each step
    std::vector<MidiNote> arpedChord = MidiPipelineEngine::applyArpeggiator(chord, 1.0, 1, "chord", 0.85, 0.0);
    assert(arpedChord.size() == 4 * 3); // 4 steps * 3 chord tones
    std::cout << "  Passed." << std::endl;
}

void testChordFollowAndVoicings() {
    std::cout << "[Test 4] Chord Follow & Voicings..." << std::endl;
    TimeContext ctx;
    ctx.songKeyRoot = 0; // C
    ctx.chordPitchClasses = {0, 4, 7}; // C Major (C, E, G)
    ctx.bassPitchClass = 0; // C

    std::vector<MidiNote> notes = {
        {"n1", 61, 0.0f, 1.0f, 0.8f} // C#4 (not in C Major)
    };

    std::vector<MidiNote> followed = MidiPipelineEngine::applyChordFollow(notes, ctx, "chord");
    assert(followed.size() == 1);
    assert(followed[0].pitch == 60 || followed[0].pitch == 64);

    // Voicings from single root trigger
    std::vector<MidiNote> voiced = MidiPipelineEngine::generateChordVoicings(notes, ctx);
    // Should have 1 bass note + 3 chord tones = 4 notes
    assert(voiced.size() == 4);
    std::cout << "  Passed." << std::endl;
}

void testHumanizeAndTranspose() {
    std::cout << "[Test 5] Humanize & Transpose..." << std::endl;
    std::vector<MidiNote> notes = {
        {"n1", 60, 0.0f, 1.0f, 0.8f},
        {"n2", 62, 1.0f, 1.0f, 0.8f}
    };

    // Transpose +7 semitones (fifth)
    std::vector<MidiNote> transposed = MidiPipelineEngine::applyTranspose(notes, 7);
    assert(transposed[0].pitch == 67);
    assert(transposed[1].pitch == 69);

    // Humanize
    std::vector<MidiNote> humanized = MidiPipelineEngine::applyHumanize(notes, 0.05f, 0.1f, 999);
    assert(humanized.size() == 2);
    assert(humanized[0].velocity != 0.8f || humanized[1].velocity != 0.8f);
    std::cout << "  Passed." << std::endl;
}

void testSequencerTrackInterop() {
    std::cout << "[Test 6] SequencerTrack Pipeline Interop..." << std::endl;
    SequencerTrack track("Acid", 1, 16);
    StepData s0, s4, s8;
    s0.active = true; s0.note = 36; s0.velocity = 0.9f;
    s4.active = true; s4.note = 40; s4.velocity = 0.8f;
    s8.active = true; s8.note = 43; s8.velocity = 0.85f;
    track.setStep(0, s0);
    track.setStep(4, s4);
    track.setStep(8, s8);

    std::vector<MidiNote> extracted = MidiPipelineEngine::trackToNotes(track);
    assert(extracted.size() == 3);
    assert(extracted[0].pitch == 36);
    assert(extracted[1].pitch == 40);
    assert(extracted[2].pitch == 43);

    // Apply Transpose via Rack
    std::vector<MidiFxInsert> rack = {
        {"mfx_trans", "Transpose +12", true, MidiFxType::Transpose, "", {{"Semitones", 12.0f}}}
    };
    TimeContext ctx;
    MidiPipelineEngine::applyPipelineToTrack(rack, track, ctx);

    assert(track.getStep(0).active && track.getStep(0).note == 48);
    assert(track.getStep(4).active && track.getStep(4).note == 52);
    assert(track.getStep(8).active && track.getStep(8).note == 55);
    std::cout << "  Passed." << std::endl;
}

void testStepSequencerLiveMidiFxPipeline() {
    std::cout << "[Test 7] StepSequencer Live MIDI FX Processing...\n";
    StepSequencer seq;
    seq.setBpm(135.0);
    seq.addTrack("Lead", 1, 16);

    auto* track = seq.getTrack(0);
    assert(track != nullptr);

    // Note 61 (C#4)
    StepData sd;
    sd.active = true;
    sd.note = 61;
    sd.velocity = 0.8f;
    track->setStep(0, sd);

    // Add ScaleSnap MIDI FX (C Major: snap to 60 or 62)
    std::vector<MidiFxInsert> rack = {
        {"mfx_snap", "Scale Snap", true, MidiFxType::ScaleSnap, "", {{"rootKey", 0.0f}, {"scaleMode", 0.0f}}}
    };
    track->setMidiFxRack(rack);
    assert(track->getMidiFxRack().size() == 1);

    // Execute processBlock with AudioGraph
    eatsbits::audio::AudioGraph graph;
    seq.start();
    seq.processBlock(256, graph);

    std::cout << "  Passed." << std::endl;
}

int main() {
    std::cout << "=== Running MIDI FX Pipeline Tests ===" << std::endl;
    testMidiFxTypeDetection();
    testScaleSnap();
    testArpeggiator();
    testChordFollowAndVoicings();
    testHumanizeAndTranspose();
    testSequencerTrackInterop();
    testStepSequencerLiveMidiFxPipeline();
    std::cout << "All MIDI FX Pipeline Tests Passed Successfully!" << std::endl;
    return 0;
}
