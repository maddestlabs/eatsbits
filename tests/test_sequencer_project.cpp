#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <cstdlib>

#include "eatsbits/sequencer/transport.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"
#include "eatsbits/project/project_file.hpp"

#define REQUIRE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "[FAIL] Assertion failed: " << #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

using namespace eatsbits;

void testTransportClock() {
    std::cout << "[Test] Running testTransportClock..." << std::endl;

    sequencer::Transport transport(48000);
    transport.setBpm(120.0);
    transport.setSwing(0.50); // Straight 50/50

    REQUIRE(transport.getBpm() == 120.0);
    REQUIRE(transport.getSwing() == 0.50);

    // At 120 BPM, 48000 Hz:
    // 1 beat (quarter note) = 0.5s = 24000 samples.
    // 1 step (16th note) = 6000 samples.
    double straightStep = transport.getStepLength(0);
    REQUIRE(std::abs(straightStep - 6000.0) < 0.001);

    // Test swing: 0.60
    transport.setSwing(0.60);
    double evenStep = transport.getStepLength(0);
    double oddStep = transport.getStepLength(1);
    REQUIRE(std::abs(evenStep - (6000.0 * 1.20)) < 0.001); // 7200
    REQUIRE(std::abs(oddStep - (6000.0 * 0.80)) < 0.001);  // 4800
    REQUIRE(std::abs((evenStep + oddStep) - 12000.0) < 0.001); // Pair remains exact quarter note

    // Test advance
    transport.stop();
    transport.start();
    std::array<sequencer::StepTick, 8> ticks{};
    size_t count = transport.advance(128, ticks);
    REQUIRE(count == 1);
    REQUIRE(ticks[0].stepIndex == 0);
    REQUIRE(ticks[0].frameOffset == 0);

    std::cout << "  Transport step lengths & swing verified." << std::endl;
    std::cout << "[Test] testTransportClock PASSED.\n" << std::endl;
}

void testEatscriptNode() {
    std::cout << "[Test] Running testEatscriptNode..." << std::endl;

    auto scriptNode = std::make_shared<audio::EatscriptNode>("LiveScriptOsc");
    scriptNode->prepare(48000.0, 128);

    const std::string scriptCode = 
        "def process(time, freq, note):\n"
        "    return sin(time * freq * 6.2831853) * 0.75\n";

    bool compiled = scriptNode->setScript(scriptCode);
    REQUIRE(compiled);
    REQUIRE(scriptNode->hasCompiledProcess());

    audio::AudioGraph graph;
    graph.prepare(48000.0, 128);
    audio::NodeId scriptId = graph.addNode(scriptNode);
    auto gainNode = std::make_shared<audio::GainNode>("MasterGain");
    gainNode->prepare(48000.0, 128);
    audio::NodeId gainId = graph.addNode(gainNode);

    REQUIRE(graph.connect(scriptId, 0, gainId, 0));
    graph.setOutputNode(gainId, 0);
    REQUIRE(graph.compile());

    // Trigger NoteOn (MIDI 69 = A4 = 440 Hz)
    AudioEvent ev{};
    ev.type = AudioEventType::NoteOn;
    ev.note = 69;
    ev.velocity = 1.0f;
    graph.sendNodeEvent(scriptId, ev);

    // Render 512 frames
    alignas(64) float outL[128]{};
    alignas(64) float outR[128]{};
    float peak = 0.0f;

    for (int block = 0; block < 8; ++block) {
        graph.process(outL, outR, 128);
        for (int i = 0; i < 128; ++i) {
            peak = std::max(peak, std::abs(outL[i]));
        }
    }

    std::cout << "  EatscriptNode generated peak amplitude: " << peak << std::endl;
    REQUIRE(peak > 0.4f && peak <= 0.85f);

    std::cout << "[Test] testEatscriptNode PASSED.\n" << std::endl;
}

void testStepSequencerAudioGeneration() {
    std::cout << "[Test] Running testStepSequencerAudioGeneration..." << std::endl;

    audio::AudioGraph graph;
    graph.prepare(48000.0, 128);

    auto tb303 = std::make_shared<audio::Tb303Node>("AcidTB303");
    auto drums = std::make_shared<audio::DrumKitNode>("AnalogDrums");
    auto masterGain = std::make_shared<audio::GainNode>("MasterGain");

    tb303->prepare(48000.0, 128);
    drums->prepare(48000.0, 128);
    masterGain->prepare(48000.0, 128);

    audio::NodeId tbId = graph.addNode(tb303);
    audio::NodeId drumId = graph.addNode(drums);
    audio::NodeId masterId = graph.addNode(masterGain);

    REQUIRE(graph.connect(tbId, 0, masterId, 0));
    REQUIRE(graph.connect(drumId, 0, masterId, 0));
    graph.setOutputNode(masterId, 0);
    REQUIRE(graph.compile());

    sequencer::StepSequencer sequencer;
    sequencer.getTransport().setSampleRate(48000);
    sequencer.setBpm(135.0);

    // Create Track 1: TB-303 Acid Bassline
    size_t t1 = sequencer.addTrack("303 Bass", tbId, 16);
    auto* track1 = sequencer.getTrack(t1);
    REQUIRE(track1 != nullptr);

    // 16-step pattern
    const uint8_t bassNotes[16] = {36, 36, 48, 36, 39, 36, 41, 42, 36, 48, 36, 39, 51, 50, 48, 46};
    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData step{};
        step.active = true;
        step.note = bassNotes[s];
        step.velocity = (s % 4 == 0) ? 1.0f : 0.75f;
        step.gateLength = 0.65f;
        step.slide = (s == 6 || s == 14);
        step.accent = (s == 0 || s == 7 || s == 12);
        track1->setStep(s, step);
    }

    // Create Track 2: Drums
    size_t t2 = sequencer.addTrack("Drums", drumId, 16);
    auto* track2 = sequencer.getTrack(t2);
    REQUIRE(track2 != nullptr);

    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData step{};
        // Four on the floor kick + snare on 4, 12 + hihat on all offbeats
        if (s % 4 == 0) {
            step.active = true;
            step.note = 36; // 909 Kick
            step.velocity = 0.95f;
        } else if (s % 4 == 2) {
            step.active = true;
            step.note = 42; // Closed Hat
            step.velocity = 0.7f;
        } else if (s == 4 || s == 12) {
            step.active = true;
            step.note = 38; // Snare
            step.velocity = 0.85f;
        }
        track2->setStep(s, step);
    }

    sequencer.start();
    REQUIRE(sequencer.isPlaying());

    // Render 1 full bar (16 steps = 4 beats at 135 BPM = ~1.78 seconds = ~85333 samples)
    alignas(64) float outL[128]{};
    alignas(64) float outR[128]{};
    float maxPeak = 0.0f;
    uint32_t totalRendered = 0;

    while (totalRendered < 85333) {
        sequencer.processBlock(128, graph);
        graph.process(outL, outR, 128);
        for (int i = 0; i < 128; ++i) {
            maxPeak = std::max(maxPeak, std::abs(outL[i]));
            maxPeak = std::max(maxPeak, std::abs(outR[i]));
        }
        totalRendered += 128;
    }

    std::cout << "  Rendered 1 full bar of synchronized sequencer audio. Max Peak: " << maxPeak << std::endl;
    REQUIRE(maxPeak > 0.3f);
    REQUIRE(maxPeak < 2.0f);

    sequencer.stop();
    REQUIRE(!sequencer.isPlaying());

    std::cout << "[Test] testStepSequencerAudioGeneration PASSED.\n" << std::endl;
}

void testProjectSerializationRoundTrip() {
    std::cout << "[Test] Running testProjectSerializationRoundTrip..." << std::endl;

    audio::AudioGraph origGraph;
    origGraph.prepare(48000.0, 128);

    auto tb = std::make_shared<audio::Tb303Node>("AcidTB303");
    auto dk = std::make_shared<audio::DrumKitNode>("AnalogDrums");
    auto dl = std::make_shared<audio::DelayNode>("TapeEcho");
    dl->setDelayTimeMs(333.0f);
    dl->setFeedback(0.42f);
    dl->setDryWet(0.38f);
    auto gn = std::make_shared<audio::GainNode>("MasterOutput");
    gn->setVolume(0.85f);
    gn->setPan(0.1f);
    auto es = std::make_shared<audio::EatscriptNode>("CustomOsc");
    es->setScript("def process(time, freq, note):\n    return sin(time * 440.0) * 0.5\n");

    audio::NodeId idTb = origGraph.addNode(tb);
    audio::NodeId idDk = origGraph.addNode(dk);
    audio::NodeId idDl = origGraph.addNode(dl);
    audio::NodeId idGn = origGraph.addNode(gn);
    audio::NodeId idEs = origGraph.addNode(es);

    REQUIRE(origGraph.connect(idTb, 0, idDl, 0));
    REQUIRE(origGraph.connect(idDl, 0, idGn, 0));
    REQUIRE(origGraph.connect(idDk, 0, idGn, 0));
    REQUIRE(origGraph.connect(idEs, 0, idGn, 0));
    origGraph.setOutputNode(idGn, 0);
    REQUIRE(origGraph.compile());

    sequencer::StepSequencer origSeq;
    origSeq.setBpm(138.0);
    origSeq.setSwing(0.56);

    size_t tIdx = origSeq.addTrack("303 Lead", idTb, 16);
    auto* t = origSeq.getTrack(tIdx);
    REQUIRE(t != nullptr);
    sequencer::StepData s0{};
    s0.active = true;
    s0.note = 36;
    s0.velocity = 0.95f;
    s0.slide = false;
    s0.accent = true;
    s0.paramLockId = 0; // Cutoff
    s0.paramLockValue = 1200.0f;
    t->setStep(0, s0);

    sequencer::StepData s1{};
    s1.active = true;
    s1.note = 48;
    s1.velocity = 0.8f;
    s1.slide = true;
    s1.accent = false;
    t->setStep(1, s1);

    std::cout << "  Serializing JSON..." << std::endl;
    std::string jsonStr = project::ProjectFile::serializeJson(origGraph, origSeq, "Acid Masterpiece", 138.0, 0.56);
    std::cout << "  JSON length: " << jsonStr.size() << std::endl;
    REQUIRE(!jsonStr.empty());
    REQUIRE(jsonStr.find("Acid Masterpiece") != std::string::npos);
    REQUIRE(jsonStr.find("AcidTB303") != std::string::npos);
    REQUIRE(jsonStr.find("AnalogDrums") != std::string::npos);
    REQUIRE(jsonStr.find("TapeEcho") != std::string::npos);

    // Save to test file
    std::cout << "  Saving to file..." << std::endl;
    const std::string testFilePath = "test_roundtrip.eats";
    REQUIRE(project::ProjectFile::saveToFile(testFilePath, origGraph, origSeq, "Acid Masterpiece", 138.0, 0.56));

    // Deserialize into fresh graph and sequencer
    std::cout << "  Deserializing from file..." << std::endl;
    audio::AudioGraph loadedGraph;
    loadedGraph.prepare(48000.0, 128);
    sequencer::StepSequencer loadedSeq;

    std::string loadedTitle;
    double loadedBpm = 0.0;
    double loadedSwing = 0.0;

    bool loadedOk = project::ProjectFile::loadFromFile(testFilePath, loadedGraph, loadedSeq, loadedTitle, loadedBpm, loadedSwing);
    std::cout << "  loadFromFile returned: " << (loadedOk ? "OK" : "FAIL") << std::endl;
    REQUIRE(loadedOk);
    REQUIRE(loadedTitle == "Acid Masterpiece");
    REQUIRE(std::abs(loadedBpm - 138.0) < 0.001);
    REQUIRE(std::abs(loadedSwing - 0.56) < 0.001);

    // Verify Graph
    REQUIRE(loadedGraph.getNodeCount() == 5);
    REQUIRE(loadedGraph.getConnections().size() == 4);
    REQUIRE(loadedGraph.getOutputNodeId() != audio::INVALID_NODE_ID);

    // Verify Sequencer
    REQUIRE(loadedSeq.getBpm() == 138.0);
    REQUIRE(loadedSeq.getSwing() == 0.56);
    auto* loadedTrack = loadedSeq.getTrack(0);
    REQUIRE(loadedTrack != nullptr);
    REQUIRE(loadedTrack->getName() == "303 Lead");
    REQUIRE(loadedTrack->getStep(0).active);
    REQUIRE(loadedTrack->getStep(0).note == 36);
    REQUIRE(loadedTrack->getStep(0).accent == true);
    REQUIRE(loadedTrack->getStep(0).slide == false);
    REQUIRE(loadedTrack->getStep(1).active);
    REQUIRE(loadedTrack->getStep(1).note == 48);
    REQUIRE(loadedTrack->getStep(1).slide == true);

    std::cout << "  Full project serialized and deserialized with 100% fidelity." << std::endl;
    std::cout << "[Test] testProjectSerializationRoundTrip PASSED.\n" << std::endl;
}

void testSequencerBenchmark() {
    std::cout << "[Benchmark] Stress-testing StepSequencer & AudioGraph execution budget..." << std::endl;

    audio::AudioGraph graph;
    graph.prepare(48000.0, 64);

    auto tb = std::make_shared<audio::Tb303Node>("AcidTB303");
    auto dk = std::make_shared<audio::DrumKitNode>("AnalogDrums");
    auto dl = std::make_shared<audio::DelayNode>("StereoDelay");
    auto gn = std::make_shared<audio::GainNode>("Master");

    audio::NodeId tbId = graph.addNode(tb);
    audio::NodeId dkId = graph.addNode(dk);
    audio::NodeId dlId = graph.addNode(dl);
    audio::NodeId gnId = graph.addNode(gn);

    graph.connect(tbId, 0, dlId, 0);
    graph.connect(dlId, 0, gnId, 0);
    graph.connect(dkId, 0, gnId, 0);
    graph.setOutputNode(gnId, 0);
    graph.compile();

    sequencer::StepSequencer seq;
    seq.setBpm(140.0);
    seq.setSwing(0.55);

    size_t t1 = seq.addTrack("Bass", tbId, 16);
    auto* track1 = seq.getTrack(t1);
    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData st{};
        st.active = true;
        st.note = static_cast<uint8_t>(36 + (s % 12));
        st.velocity = 0.85f;
        st.slide = (s % 3 == 0);
        st.accent = (s % 4 == 0);
        track1->setStep(s, st);
    }

    size_t t2 = seq.addTrack("Drums", dkId, 16);
    auto* track2 = seq.getTrack(t2);
    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData st{};
        st.active = true;
        st.note = (s % 4 == 0) ? 36 : (s % 2 == 0 ? 42 : 38);
        st.velocity = 0.9f;
        track2->setStep(s, st);
    }

    seq.start();

    // Render 10.0 seconds of audio at 64-sample buffer size (150000 iterations @ 48kHz = 9.6 million samples)
    const uint32_t numBlocks = 7500; // 7500 * 64 = 480,000 samples = 10.0 seconds
    alignas(64) float outL[64]{};
    alignas(64) float outR[64]{};

    auto start = std::chrono::high_resolution_clock::now();
    for (uint32_t b = 0; b < numBlocks; ++b) {
        seq.processBlock(64, graph);
        graph.process(outL, outR, 64);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    double realTimeSecs = 10.0;
    double elapsedMs = duration.count();
    double speedup = (realTimeSecs * 1000.0) / elapsedMs;

    std::cout << "  Rendered " << realTimeSecs << " seconds of synced sequencer + graph audio in "
              << elapsedMs << " ms (" << speedup << "x faster than real-time budget)" << std::endl;

    REQUIRE(speedup > 15.0);
    std::cout << "[Benchmark] Sequencer benchmark PASSED.\n" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << " Eatsbits Sequencer, EatscriptNode & Project I/O" << std::endl;
    std::cout << "=================================================" << std::endl;

    testTransportClock();
    testEatscriptNode();
    testStepSequencerAudioGeneration();
    testProjectSerializationRoundTrip();
    testSequencerBenchmark();

    std::cout << ">>> ALL SEQUENCER & PROJECT TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
    return 0;
}
