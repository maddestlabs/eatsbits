#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <filesystem>
#include "eatsbits/audio/track_freeze_engine.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/project/project_file.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::audio::trackfreeze;

void testDeterministicHash() {
    std::cout << "[Test 1] Testing deterministic 64-bit FNV-1a track hash..." << std::endl;
    sequencer::SequencerTrack trk("303 Acid", 1, 16);
    trk.setEatscriptCode("function process() return 0.5 end");
    sequencer::StepData s0;
    s0.active = true;
    s0.note = 60;
    s0.velocity = 0.9f;
    trk.setStep(0, s0);

    sequencer::StepData s4;
    s4.active = true;
    s4.note = 67;
    s4.velocity = 0.8f;
    trk.setStep(4, s4);

    std::string hash1 = TrackFreezeEngine::computeTrackHash(trk, 120.0, 48000);
    std::string hash2 = TrackFreezeEngine::computeTrackHash(trk, 120.0, 48000);

    assert(hash1 == hash2);
    assert(hash1.length() == 16);

    // Modify eatscript code -> hash must change
    trk.setEatscriptCode("function process() return 0.8 end");
    std::string hashModifiedCode = TrackFreezeEngine::computeTrackHash(trk, 120.0, 48000);
    assert(hashModifiedCode != hash1);

    // Restore code, modify note pitch -> hash must change
    trk.setEatscriptCode("function process() return 0.5 end");
    s4.note = 72;
    trk.setStep(4, s4);
    std::string hashModifiedNote = TrackFreezeEngine::computeTrackHash(trk, 120.0, 48000);
    assert(hashModifiedNote != hash1);

    // Modify BPM -> hash must change
    s4.note = 67;
    trk.setStep(4, s4);
    std::string hashDiffBpm = TrackFreezeEngine::computeTrackHash(trk, 135.0, 48000);
    assert(hashDiffBpm != hash1);

    // Restore BPM -> hash must match original
    std::string hashRestored = TrackFreezeEngine::computeTrackHash(trk, 120.0, 48000);
    assert(hashRestored == hash1);

    std::cout << "  -> Deterministic hash passed (hash: " << hash1 << ")" << std::endl;
}

void testOfflineRender() {
    std::cout << "[Test 2] Testing offline fast synthesis rendering..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    bool initOk = engine.initialize(cfg);
    assert(initOk);
    (void)initOk;
    engine.setupDefaultAcidBeatGraph();

    size_t tIdx = engine.getSequencer().addTrack("303 Acid", 1, 16);
    auto* trk = engine.getSequencer().getTrack(tIdx);
    assert(trk != nullptr);

    sequencer::StepData s0;
    s0.active = true;
    s0.note = 48; // C3
    s0.velocity = 0.85f;
    s0.gateLength = 0.75f;
    trk->setStep(0, s0);

    sequencer::StepData s4;
    s4.active = true;
    s4.note = 51; // Eb3
    s4.velocity = 0.90f;
    s4.slide = true;
    trk->setStep(4, s4);

    float maxReportedProg = 0.0f;
    std::string lastStatus = "";

    FreezeOptions opts{};
    opts.bpm = 120.0;
    opts.sampleRate = 48000;
    opts.numBars = 1; // 1 bar = 2.0s = 96000 frames
    opts.normalizePeaks = true;
    opts.peakLimit = 0.95f;
    opts.onProgress = [&](float p, const std::string& status) {
        if (p > maxReportedProg) maxReportedProg = p;
        lastStatus = status;
    };

    FreezeResult res = TrackFreezeEngine::renderTrackOffline(
        engine.getGraph(),
        engine.getSequencer(),
        0,
        opts
    );

    assert(res.success);
    assert(res.totalFrames == 96000);
    assert(res.bufferL.size() == 96000);
    assert(res.bufferR.size() == 96000);
    assert(maxReportedProg >= 0.99f);
    assert(res.peakL > 0.001f);
    assert(res.peakL <= 0.96f);
    assert(res.speedMultiplier > 10.0); // Offline render should be tens or hundreds of times faster than real-time

    std::cout << "  -> Rendered " << res.totalFrames << " frames (" << res.durationSeconds
              << "s) in " << res.renderTimeMs << "ms (" << res.speedMultiplier << "x real-time)" << std::endl;
}

void testFreezeAndUnfreezeAudioEngine() {
    std::cout << "[Test 3] Testing AudioEngine track freeze & node bypass..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.initialize(cfg);
    engine.setupDefaultAcidBeatGraph();

    size_t tIdx = engine.getSequencer().addTrack("303 Acid", 1, 16);
    auto* trk = engine.getSequencer().getTrack(tIdx);
    assert(trk != nullptr);

    sequencer::StepData s0;
    s0.active = true;
    s0.note = 50;
    s0.velocity = 0.8f;
    trk->setStep(0, s0);

    assert(!engine.isTrackFrozen(0));
    assert(trk->getFrozenBufferL().empty());

    // Freeze Track 0
    FreezeOptions opts{};
    opts.bpm = 120.0;
    opts.numBars = 1;
    bool freezeOk = engine.freezeTrack(0, opts);
    assert(freezeOk);
    (void)freezeOk;
    assert(engine.isTrackFrozen(0));
    assert(!trk->getFrozenBufferL().empty());
    assert(trk->getFrozenBufferL().size() == 96000);

    // Verify synth node is disabled in graph to reclaim CPU
    auto synthNode = engine.getGraph().getNode(trk->getTargetNodeId());
    assert(synthNode != nullptr);
    assert(!synthNode->isEnabled());

    // Verify freeze validity check
    assert(TrackFreezeEngine::isFreezeValid(*trk, 120.0, 48000, &engine.getGraph()));

    // Toggle freeze -> should unfreeze
    bool toggleOk = engine.toggleFreezeTrack(0, opts);
    assert(toggleOk);
    assert(!engine.isTrackFrozen(0));
    assert(synthNode->isEnabled()); // Live synth restored

    // Toggle freeze -> should freeze again
    toggleOk = engine.toggleFreezeTrack(0, opts);
    assert(toggleOk);
    (void)toggleOk;
    assert(engine.isTrackFrozen(0));
    assert(!synthNode->isEnabled());

    // Unfreeze explicitly
    bool unfreezeOk = engine.unfreezeTrack(0, false);
    assert(unfreezeOk);
    (void)unfreezeOk;
    assert(!engine.isTrackFrozen(0));
    assert(synthNode->isEnabled());

    std::cout << "  -> Freeze, unfreeze, toggle and node bypass verified successfully" << std::endl;
}

void testRealTimeFrozenPlayback() {
    std::cout << "[Test 4] Testing real-time frozen playback and mixFrozenTracks..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.initialize(cfg);
    engine.setupDefaultAcidBeatGraph();

    size_t tIdx = engine.getSequencer().addTrack("303 Acid", 1, 16);
    auto* trk = engine.getSequencer().getTrack(tIdx);
    assert(trk != nullptr);

    sequencer::StepData s0;
    s0.active = true;
    s0.note = 48;
    s0.velocity = 0.9f;
    trk->setStep(0, s0);

    // Freeze Track 0
    FreezeOptions opts{};
    opts.bpm = 120.0;
    opts.numBars = 1;
    bool ok = engine.freezeTrack(0, opts);
    assert(ok);
    (void)ok;
    assert(engine.isTrackFrozen(0));

    // Target synth node is disabled
    auto synthNode = engine.getGraph().getNode(trk->getTargetNodeId());
    assert(synthNode != nullptr && !synthNode->isEnabled());

    // Start sequencer transport
    engine.getSequencer().setBpm(120.0);
    engine.getSequencer().start();

    // Render several audio blocks via renderOfflineBlock
    constexpr uint32_t BLOCK_SIZE = 128;
    float outL[BLOCK_SIZE];
    float outR[BLOCK_SIZE];

    float totalEnergy = 0.0f;
    for (int block = 0; block < 16; ++block) {
        engine.renderOfflineBlock(outL, outR, BLOCK_SIZE);
        for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
            totalEnergy += std::abs(outL[i]) + std::abs(outR[i]);
        }
    }

    assert(totalEnergy > 0.05f); // Frozen stream produced audio!

    // Verify track mute on frozen track
    trk->setMuted(true);
    float mutedEnergy = 0.0f;
    for (int block = 0; block < 4; ++block) {
        engine.renderOfflineBlock(outL, outR, BLOCK_SIZE);
        for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
            mutedEnergy += std::abs(outL[i]) + std::abs(outR[i]);
        }
    }
    assert(mutedEnergy < 0.001f); // Muted frozen track produces silence

    trk->setMuted(false);
    engine.getSequencer().stop();
    std::cout << "  -> Real-time frozen streaming and mute handling passed" << std::endl;
}

void testProjectFreezeSerialization() {
    std::cout << "[Test 5] Testing project serialization of freeze status..." << std::endl;
    AudioGraph graph;
    sequencer::StepSequencer seq;
    size_t tIdx = seq.addTrack("Synth Lead", 1, 16);
    auto* trk = seq.getTrack(tIdx);
    assert(trk != nullptr);

    trk->setFrozen(true);
    trk->setFrozenContentHash("0123456789abcdef");

    std::string jsonStr = project::ProjectFile::serializeJson(graph, seq, "FreezeTest", 120.0, 0.50);
    assert(jsonStr.find("\"isFrozen\":true") != std::string::npos);
    assert(jsonStr.find("0123456789abcdef") != std::string::npos);

    AudioGraph loadedGraph;
    sequencer::StepSequencer loadedSeq;
    std::string title;
    double bpm = 0.0;
    double swing = 0.0;

    bool loadOk = project::ProjectFile::deserializeJson(jsonStr, loadedGraph, loadedSeq, title, bpm, swing);
    assert(loadOk);
    (void)loadOk;
    assert(loadedSeq.getNumTracks() == 1);
    const auto* loadedTrk = loadedSeq.getTrack(0);
    assert(loadedTrk != nullptr);
    (void)loadedTrk;
    assert(loadedTrk->isFrozen() == true);
    assert(loadedTrk->getFrozenContentHash() == "0123456789abcdef");

    std::cout << "  -> Freeze state and hash preserved across serialization" << std::endl;
}

void testBakeToWavFile() {
    std::cout << "[Test 6] Testing direct bounce of frozen track to WAV file..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;
    engine.initialize(cfg);
    engine.setupDefaultAcidBeatGraph();

    size_t tIdx = engine.getSequencer().addTrack("303 Acid", 1, 16);
    auto* trk = engine.getSequencer().getTrack(tIdx);
    assert(trk != nullptr);

    sequencer::StepData s0;
    s0.active = true;
    s0.note = 60;
    s0.velocity = 0.85f;
    trk->setStep(0, s0);

    std::string tempWav = "tests_scratch_track_freeze.wav";
    FreezeOptions opts{};
    opts.bpm = 120.0;
    opts.numBars = 1;

    bool wavOk = TrackFreezeEngine::bounceTrackToWav(
        engine.getGraph(),
        engine.getSequencer(),
        0,
        tempWav,
        opts
    );
    assert(wavOk);
    (void)wavOk;
    assert(std::filesystem::exists(tempWav));
    uintmax_t size = std::filesystem::file_size(tempWav);
    assert(size > 1000); // Has valid WAV header and samples

    std::filesystem::remove(tempWav);
    std::cout << "  -> Exported track to WAV (" << size << " bytes) successfully" << std::endl;
}

int main() {
    std::cout << "==========================================================" << std::endl;
    std::cout << "   RUNNING SUBSYSTEM 5: TRACK FREEZE ENGINE TEST SUITE    " << std::endl;
    std::cout << "==========================================================" << std::endl;

    testDeterministicHash();
    testOfflineRender();
    testFreezeAndUnfreezeAudioEngine();
    testRealTimeFrozenPlayback();
    testProjectFreezeSerialization();
    testBakeToWavFile();

    std::cout << "==========================================================" << std::endl;
    std::cout << "   ALL SUBSYSTEM 5 TRACK FREEZE TESTS PASSED (6/6)        " << std::endl;
    std::cout << "==========================================================" << std::endl;
    return 0;
}
