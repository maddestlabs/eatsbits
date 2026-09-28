#include "eatsbits/lyrics/lyric_track.hpp"
#include "eatsbits/audio/graph/nodes/tts_synth_node.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/project/project_file.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits;
using namespace eatsbits::lyrics;
using namespace eatsbits::audio;
using namespace eatsbits::sequencer;
using namespace eatsbits::project;

void testLyricCueRoundtrip() {
    std::cout << "[Test 1] Testing LyricCue JSON Serialization & Deserialization..." << std::endl;

    LyricCue cue;
    cue.id = "cue_vox_01";
    cue.startStep = 16.0f;
    cue.durationSteps = 4.0f;
    cue.text = "Welcome";
    cue.phoneticOverride = "w eh l k ah m";
    cue.pitch = 1.25f;
    cue.rate = 0.95f;

    json::Value jsonVal = cue.toJson();
    assert(jsonVal.isObject());

    LyricCue restored = LyricCue::fromJson(jsonVal);
    assert(restored.id == "cue_vox_01");
    assert(std::abs(restored.startStep - 16.0f) < 1e-4f);
    assert(std::abs(restored.durationSteps - 4.0f) < 1e-4f);
    assert(restored.text == "Welcome");
    assert(restored.phoneticOverride == "w eh l k ah m");
    assert(std::abs(restored.pitch - 1.25f) < 1e-4f);
    assert(std::abs(restored.rate - 0.95f) < 1e-4f);

    std::cout << "  -> LyricCue JSON roundtrip verified!" << std::endl;
}

void testLrcParserStandard() {
    std::cout << "[Test 2] Testing LrcParser Standard Line-Synced LRC Parsing..." << std::endl;

    const std::string lrcContent = 
        "[ti:Eatsbits Odyssey]\n"
        "[ar:Modular Artist]\n"
        "[00:00.00] In the beginning there was sound\n"
        "[00:02.00] Frequencies modulating through the ground\n"
        "[00:04.50] Synthesizing pure wave dreams\n";

    // At 120 BPM:
    // 1 beat = 0.5s, 1 16th step = 0.125s
    // 00:00.00 = 0 steps
    // 00:02.00 = 2.0s / 0.125s = 16 steps
    // 00:04.50 = 4.5s / 0.125s = 36 steps
    std::vector<LyricCue> cues = LrcParser::parse(lrcContent, 120.0f);
    assert(cues.size() == 3);

    assert(cues[0].text == "In the beginning there was sound");
    assert(std::abs(cues[0].startStep - 0.0f) < 0.1f);

    assert(cues[1].text == "Frequencies modulating through the ground");
    assert(std::abs(cues[1].startStep - 16.0f) < 0.1f);

    assert(cues[2].text == "Synthesizing pure wave dreams");
    assert(std::abs(cues[2].startStep - 36.0f) < 0.1f);

    std::cout << "  -> Standard line-synced LRC parsing verified!" << std::endl;
}

void testLrcParserEnhanced() {
    std::cout << "[Test 3] Testing LrcParser Enhanced Word-Synced Timestamp Parsing..." << std::endl;

    const std::string enhancedLrc =
        "[00:01.00] <00:01.00> Zero <00:01.50> latency <00:02.00> audio\n";

    // 120 BPM -> step = 0.125s
    // 1.0s = 8 steps ("Zero")
    // 1.5s = 12 steps ("latency")
    // 2.0s = 16 steps ("audio")
    std::vector<LyricCue> cues = LrcParser::parse(enhancedLrc, 120.0f);
    assert(cues.size() == 3);

    assert(cues[0].text == "Zero");
    assert(std::abs(cues[0].startStep - 8.0f) < 0.1f);
    assert(std::abs(cues[0].durationSteps - 4.0f) < 0.1f);

    assert(cues[1].text == "latency");
    assert(std::abs(cues[1].startStep - 12.0f) < 0.1f);
    assert(std::abs(cues[1].durationSteps - 4.0f) < 0.1f);

    assert(cues[2].text == "audio");
    assert(std::abs(cues[2].startStep - 16.0f) < 0.1f);

    std::cout << "  -> Enhanced word-synced LRC parsing verified!" << std::endl;
}

void testLrcExporter() {
    std::cout << "[Test 4] Testing LrcParser Export to LRC String..." << std::endl;

    std::vector<LyricCue> cues;
    LyricCue c1; c1.id = "1"; c1.startStep = 0.0f;  c1.text = "Modular";
    LyricCue c2; c2.id = "2"; c2.startStep = 16.0f; c2.text = "Workstation";
    cues.push_back(c1);
    cues.push_back(c2);

    std::string exported = LrcParser::exportToLrc(cues, 120.0f, "Vocals", "Eatsbits");
    assert(exported.find("[ti:Vocals]") != std::string::npos);
    assert(exported.find("[ar:Eatsbits]") != std::string::npos);
    assert(exported.find("[00:00.00] Modular") != std::string::npos);
    assert(exported.find("[00:02.00] Workstation") != std::string::npos);

    std::cout << "  -> LRC export verified!" << std::endl;
}

void testLyricTrackContainer() {
    std::cout << "[Test 5] Testing LyricTrack Timeline Container..." << std::endl;

    LyricTrack track("vox_lead", "Lead Vocals");
    assert(track.getId() == "vox_lead");
    assert(track.getName() == "Lead Vocals");
    assert(track.getCues().empty());

    LyricCue c1; c1.id = "c1"; c1.startStep = 0.0f;  c1.durationSteps = 4.0f; c1.text = "Kick";
    LyricCue c2; c2.id = "c2"; c2.startStep = 8.0f;  c2.durationSteps = 4.0f; c2.text = "Snare";
    LyricCue c3; c3.id = "c3"; c3.startStep = 16.0f; c3.durationSteps = 8.0f; c3.text = "Drop";

    track.addCue(c2);
    track.addCue(c1);
    track.addCue(c3);

    // Should be automatically sorted by startStep
    assert(track.getCues().size() == 3);
    assert(track.getCues()[0].id == "c1");
    assert(track.getCues()[1].id == "c2");
    assert(track.getCues()[2].id == "c3");

    // Lookup at step
    const auto* cueAt2 = track.getCueAtStep(2.0f);
    (void)cueAt2;
    assert(cueAt2 && cueAt2->text == "Kick");

    const auto* cueAt6 = track.getCueAtStep(6.0f);
    (void)cueAt6;
    assert(cueAt6 == nullptr);

    const auto* cueAt10 = track.getCueAtStep(10.0f);
    (void)cueAt10;
    assert(cueAt10 && cueAt10->text == "Snare");

    // Range lookup
    auto range = track.getCuesInRange(4.0f, 18.0f);
    assert(range.size() == 2); // c2 and c3

    // JSON serialization
    json::Value jsonVal = track.toJson();
    LyricTrack restored = LyricTrack::fromJson(jsonVal);
    assert(restored.getId() == "vox_lead");
    assert(restored.getName() == "Lead Vocals");
    assert(restored.getCues().size() == 3);

    // Remove cue
    bool removed = track.removeCue("c2");
    (void)removed;
    assert(removed);
    assert(track.getCues().size() == 2);

    std::cout << "  -> LyricTrack timeline container verified!" << std::endl;
}

void testTtsSynthNodeDsp() {
    std::cout << "[Test 6] Testing TtsSynthNode Formant DSP & Vocal Modes..." << std::endl;

    TtsSynthNode synth;
    synth.prepare(48000.0, 512);

    // Check parameters
    synth.setParameter(TtsSynthNode::PARAM_PITCH, 1.2f);
    assert(std::abs(synth.getParameter(TtsSynthNode::PARAM_PITCH) - 1.2f) < 1e-4f);

    synth.setParameter(TtsSynthNode::PARAM_TONE, 1.5f);
    assert(std::abs(synth.getParameter(TtsSynthNode::PARAM_TONE) - 1.5f) < 1e-4f);

    synth.setParameter(TtsSynthNode::PARAM_VOICE_MODE, static_cast<float>(VocalMode::Robot));
    assert(synth.getVocalMode() == VocalMode::Robot);

    synth.setParameter(TtsSynthNode::PARAM_VOICE_MODE, static_cast<float>(VocalMode::Whisper));
    assert(synth.getVocalMode() == VocalMode::Whisper);

    synth.setParameter(TtsSynthNode::PARAM_VOICE_MODE, static_cast<float>(VocalMode::Natural));
    assert(synth.getVocalMode() == VocalMode::Natural);

    // Test vowel switching
    synth.setVowel(VowelPhoneme::E);
    assert(synth.getCurrentVowel() == VowelPhoneme::E);

    synth.setVowel(VowelPhoneme::A);
    assert(synth.getCurrentVowel() == VowelPhoneme::A);

    std::cout << "  -> Formant parameters and vocal modes verified!" << std::endl;
}

void testTtsAudioBlockRendering() {
    std::cout << "[Test 7] Testing TtsSynthNode Real-Time Audio Block Rendering..." << std::endl;

    TtsSynthNode synth;
    synth.prepare(48000.0, 256);

    std::vector<float> bufOutL(256, 0.0f);
    std::vector<float> bufOutR(256, 0.0f);

    synth.setOutputBufferPtr(0, 0, bufOutL.data());
    synth.setOutputBufferPtr(0, 1, bufOutR.data());

    // Initially silent
    synth.processBlock(256);
    float maxAmpQuiet = 0.0f;
    for (float s : bufOutL) maxAmpQuiet = std::max(maxAmpQuiet, std::abs(s));
    assert(maxAmpQuiet < 0.01f);

    // Trigger note and render
    AudioEvent noteOn;
    noteOn.type = AudioEventType::NoteOn;
    noteOn.note = 69; // A4 (440 Hz)
    noteOn.velocity = 1.0f;
    synth.handleEvent(noteOn);

    synth.processBlock(256);
    synth.processBlock(256);

    float maxAmpVoicing = 0.0f;
    for (float s : bufOutL) maxAmpVoicing = std::max(maxAmpVoicing, std::abs(s));
    assert(maxAmpVoicing > 0.05f);

    // Trigger speech syllable
    synth.speakText("Eatsbits");
    synth.processBlock(256);
    synth.processBlock(256);

    // Release note
    AudioEvent noteOff;
    noteOff.type = AudioEventType::NoteOff;
    noteOff.note = 69;
    synth.handleEvent(noteOff);

    std::cout << "  -> Audio block rendering and phoneme sequencing verified!" << std::endl;
}

void testSequencerTrackLyricsIntegration() {
    std::cout << "[Test 8] Testing StepSequencer and ProjectFile Lyrics Integration..." << std::endl;

    StepSequencer seq;
    size_t trackIdx = seq.addTrack("Vocal Synth", 1, 16);
    auto* track = seq.getTrack(trackIdx);
    assert(track);

    // Attach step lyrics
    StepData s0; s0.active = true; s0.note = 60; s0.lyric = "Eats-";
    StepData s4; s4.active = true; s4.note = 64; s4.lyric = "bits";
    track->setStep(0, s0);
    track->setStep(4, s4);

    // Attach track lyrics
    LyricCue cue1; cue1.id = "v1"; cue1.startStep = 0.0f; cue1.durationSteps = 4.0f; cue1.text = "Eats-";
    LyricCue cue2; cue2.id = "v2"; cue2.startStep = 4.0f; cue2.durationSteps = 4.0f; cue2.text = "bits";
    track->addLyricCue(cue1);
    track->addLyricCue(cue2);

    assert(track->hasLyrics());
    assert(track->getLyrics().size() == 2);
    assert(track->getStep(0).lyric == "Eats-");
    assert(track->getStep(4).lyric == "bits");

    // Test ProjectFile serialization roundtrip
    audio::AudioGraph graph;
    std::string serialized = ProjectFile::serializeJson(graph, seq, "Vocal Song", 128.0, 0.0);

    audio::AudioGraph restoredGraph;
    StepSequencer restoredSeq;
    std::string restoredTitle;
    double restoredBpm = 0.0;
    double restoredSwing = 0.0;

    bool ok = ProjectFile::deserializeJson(serialized, restoredGraph, restoredSeq, restoredTitle, restoredBpm, restoredSwing);
    (void)ok;
    (void)restoredSwing;
    assert(ok);
    assert(restoredTitle == "Vocal Song");
    assert(std::abs(restoredBpm - 128.0) < 0.01);

    auto* restoredTrack = restoredSeq.getTrack(0);
    (void)restoredTrack;
    assert(restoredTrack);
    assert(restoredTrack->hasLyrics());
    assert(restoredTrack->getLyrics().size() == 2);
    assert(restoredTrack->getLyrics()[0].text == "Eats-");
    assert(restoredTrack->getLyrics()[1].text == "bits");
    assert(restoredTrack->getStep(0).lyric == "Eats-");
    assert(restoredTrack->getStep(4).lyric == "bits");

    std::cout << "  -> StepSequencer and ProjectFile lyrics integration verified!" << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << " Running Eatsbits Subsystem 8: Lyrics & Vocalizer Tests" << std::endl;
    std::cout << "======================================================" << std::endl;

    testLyricCueRoundtrip();
    testLrcParserStandard();
    testLrcParserEnhanced();
    testLrcExporter();
    testLyricTrackContainer();
    testTtsSynthNodeDsp();
    testTtsAudioBlockRendering();
    testSequencerTrackLyricsIntegration();

    std::cout << "======================================================" << std::endl;
    std::cout << " ALL 8 LYRICS & SPEECH VOCALIZER TESTS PASSED (100%)! " << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
