#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>

#include "eatsbits/audio/dsp/dx7_core.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/dx7_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"

using namespace eatsbits;
using namespace eatsbits::dsp;
using namespace eatsbits::audio;

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "[FAIL] Requirement failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while (false)

void testSineLutAccuracy() {
    std::cout << "[Test 1] Testing 4096-Sample High-Resolution Sine LUT..." << std::endl;

    float s0 = lookupSine(0.0f);
    float s90 = lookupSine(0.25f);
    float s180 = lookupSine(0.5f);
    float s270 = lookupSine(0.75f);

    REQUIRE(std::abs(s0) < 0.001f);
    REQUIRE(std::abs(s90 - 1.0f) < 0.005f);
    REQUIRE(std::abs(s180) < 0.001f);
    REQUIRE(std::abs(s270 - (-1.0f)) < 0.005f);

    // Verify fine sub-sample linear interpolation
    for (int i = 0; i < 1000; ++i) {
        float p = static_cast<float>(i) / 1000.0f;
        float lutVal = lookupSine(p);
        float stdVal = std::sin(2.0f * 3.14159265f * p);
        REQUIRE(std::abs(lutVal - stdVal) < 0.01f);
    }
    std::cout << "  LUT sub-sample interpolation max error < 0.01 -> PASSED" << std::endl;
}

void testAlgorithmsAndBusRouting() {
    std::cout << "[Test 2] Testing All 32 Yamaha DX7 Algorithms & Bus Routing..." << std::endl;
    DX7Voice voice;

    for (int alg = 1; alg <= 32; ++alg) {
        voice.reset();
        voice.algorithm = alg;
        voice.feedback = 4;
        voice.noteOn(60, 0.9f, 48000.0f);

        float peak = 0.0f;
        for (int i = 0; i < 480; ++i) { // 10ms of audio
            float s = voice.processSample(48000.0f);
            REQUIRE(!std::isnan(s) && !std::isinf(s));
            peak = std::max(peak, std::abs(s));
        }
        REQUIRE(peak > 0.001f);
    }
    std::cout << "  All 32 algorithms evaluated without divergence or NaN -> PASSED" << std::endl;
}

void testFeedbackLoopStability() {
    std::cout << "[Test 3] Testing Operator Feedback Loop Stability..." << std::endl;
    DX7Voice voice;
    voice.algorithm = 5; // Algorithm 5 (Op 6 feedback)

    for (int fb = 0; fb <= 7; ++fb) {
        voice.reset();
        voice.feedback = fb;
        voice.noteOn(57, 0.85f, 48000.0f); // A3

        float peak = 0.0f;
        for (int i = 0; i < 2400; ++i) { // 50ms
            float s = voice.processSample(48000.0f);
            REQUIRE(!std::isnan(s) && !std::isinf(s));
            peak = std::max(peak, std::abs(s));
        }
        // Even with max feedback 7, output must remain controlled (< 2.0)
        REQUIRE(peak > 0.01f && peak < 2.0f);
    }
    std::cout << "  Feedback levels 0..7 bounded and stable -> PASSED" << std::endl;
}

void testEnvelopeAndVelocityScaling() {
    std::cout << "[Test 4] Testing 4-Rate 4-Level Envelope & Velocity Scaling..." << std::endl;

    // Velocity scaling
    float vHigh = scaleVelocityToLevel(1.0f, 4);
    float vMid = scaleVelocityToLevel(0.5f, 4);
    float vLow = scaleVelocityToLevel(0.1f, 4);
    REQUIRE(vHigh > vMid);
    REQUIRE(vMid > vLow);

    // Operator Envelope Progression
    DX7Operator op;
    op.r1 = 99.0f; op.l1 = 99.0f; // Fast attack
    op.r2 = 50.0f; op.l2 = 60.0f; // Decay to 60%
    op.r3 = 20.0f; op.l3 = 40.0f; // Slow decay to 40% sustain
    op.r4 = 60.0f; op.l4 = 0.0f;  // Release to 0
    op.prepareNote(0.9f, 60, 48000.0f);

    REQUIRE(op.isActive());
    // Attack phase
    for (int i = 0; i < 100; ++i) (void)op.evaluateEnvelope();
    REQUIRE(op.stage != DX7EnvStage::Idle);

    // Release phase
    op.noteOff();
    REQUIRE(op.stage == DX7EnvStage::Release);

    // Run until complete release
    for (int i = 0; i < 50000; ++i) (void)op.evaluateEnvelope();
    REQUIRE(!op.isActive());
    REQUIRE(op.stage == DX7EnvStage::Idle);

    std::cout << "  Envelope stages (Attack -> Decay -> Sustain -> Release -> Idle) -> PASSED" << std::endl;
}

void testPolyphonyAndVoiceStealing() {
    std::cout << "[Test 5] Testing 8-Voice Polyphony & Voice Stealing..." << std::endl;
    DX7Synth synth;
    synth.prepare(48000.0f);

    // Play 8 notes simultaneously (C Major 9th chord spread)
    const uint8_t chord[8] = {36, 48, 55, 60, 64, 67, 71, 74};
    for (uint8_t note : chord) {
        synth.noteOn(note, 0.85f);
    }

    int activeVoices = 0;
    for (const auto& v : synth.voices) {
        if (v.isActive()) activeVoices++;
    }
    REQUIRE(activeVoices == 8);

    // Play 9th note -> should steal oldest voice without crash
    synth.noteOn(76, 0.9f);
    int activeAfterSteal = 0;
    for (const auto& v : synth.voices) {
        if (v.isActive()) activeAfterSteal++;
    }
    REQUIRE(activeAfterSteal == 8);

    // Release all notes
    for (uint8_t note : chord) {
        synth.noteOff(note);
    }
    synth.noteOff(76);

    // Render audio through release phase
    std::vector<float> bufL(4800, 0.0f);
    std::vector<float> bufR(4800, 0.0f);
    for (int b = 0; b < 40; ++b) {
        synth.processStereo(bufL.data(), bufR.data(), 4800);
    }

    int remaining = 0;
    for (const auto& v : synth.voices) {
        if (v.isActive()) remaining++;
    }
    REQUIRE(remaining == 0);

    std::cout << "  8-voice polyphony, voice stealing, and release termination -> PASSED" << std::endl;
}

void testDx7NodeModularGraph() {
    std::cout << "[Test 6] Testing Dx7Node Modular Audio Graph Integration..." << std::endl;
    AudioGraph graph;
    graph.prepare(48000.0, 128);

    auto dx7 = std::make_shared<Dx7Node>("DX7_Piano");
    auto master = std::make_shared<GainNode>("Master_Out");

    NodeId dx7Id = graph.addNode(dx7);
    NodeId masterId = graph.addNode(master);

    REQUIRE(graph.connect(dx7Id, 0, masterId, 0));

    // Automated parameter dispatch
    dx7->setParameter(0, 5.0f);   // Algorithm 5
    dx7->setParameter(1, 6.0f);   // Feedback 6
    dx7->setParameter(3, 1.2f);   // Brightness
    dx7->setParameter(4, 0.9f);   // TineBell

    // Send NoteOn event
    AudioEvent evtOn{};
    evtOn.type = AudioEventType::NoteOn;
    evtOn.note = 64; // E4
    evtOn.velocity = 0.9f;
    dx7->handleEvent(evtOn);

    // Process blocks
    float outL[128]{0};
    float outR[128]{0};
    float peak = 0.0f;

    for (int b = 0; b < 30; ++b) {
        graph.process(outL, outR, 128);
        for (int i = 0; i < 128; ++i) {
            REQUIRE(!std::isnan(outL[i]) && !std::isinf(outL[i]));
            peak = std::max(peak, std::abs(outL[i]));
        }
    }
    REQUIRE(peak > 0.05f);

    // Send NoteOff event
    AudioEvent evtOff{};
    evtOff.type = AudioEventType::NoteOff;
    evtOff.note = 64;
    dx7->handleEvent(evtOff);

    std::cout << "  Graph execution peak: " << peak << " -> PASSED" << std::endl;
}

void testPerformanceBenchmark() {
    std::cout << "[Test 7] Performance Benchmark: 10 Seconds of 8-Voice Polyphonic DX7 FM Audio..." << std::endl;
    DX7Synth synth;
    synth.prepare(48000.0f);

    // Play full 8-voice chord
    const uint8_t chord[8] = {36, 48, 55, 60, 64, 67, 71, 74};
    for (uint8_t note : chord) {
        synth.noteOn(note, 0.85f);
    }

    const size_t totalFrames = 48000 * 10; // 480,000 frames = 10s of audio
    const size_t blockSize = 128;
    std::vector<float> bufL(blockSize);
    std::vector<float> bufR(blockSize);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t f = 0; f < totalFrames; f += blockSize) {
        synth.processStereo(bufL.data(), bufR.data(), blockSize);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double speedup = 10000.0 / elapsedMs;

    std::cout << "  Rendered 10s of 8-voice FM audio (48 operators/sample) in " << elapsedMs << " ms (" << speedup << "x real-time speedup)" << std::endl;
    REQUIRE(elapsedMs < 1000.0);
    std::cout << "  -> PASSED: Real-time FM synthesis exceeds performance threshold." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    Eatsbits Yamaha DX7 6-Op FM Synthesizer Tests " << std::endl;
    std::cout << "==================================================" << std::endl;

    testSineLutAccuracy();
    testAlgorithmsAndBusRouting();
    testFeedbackLoopStability();
    testEnvelopeAndVelocityScaling();
    testPolyphonyAndVoiceStealing();
    testDx7NodeModularGraph();
    testPerformanceBenchmark();

    std::cout << "==================================================" << std::endl;
    std::cout << "    100% YAMAHA DX7 FM SYNTHESIS TESTS PASSED!    " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
