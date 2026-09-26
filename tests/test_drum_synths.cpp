#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "eatsbits/audio/dsp/drum_synths.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
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

void testAnalog808Kick() {
    std::cout << "[Test] Running testAnalog808Kick..." << std::endl;
    Analog808Kick kick;
    kick.prepare(48000.0f);

    kick.trigger(0.9f, 50.0f, 0.4f, 1.0f, 0.2f);
    REQUIRE(kick.isActive());

    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i) { // 100ms
        float s = kick.processSample();
        REQUIRE(!std::isnan(s) && !std::isinf(s));
        peak = std::max(peak, std::abs(s));
    }
    REQUIRE(peak > 0.4f);

    // After 3 seconds, kick should be inactive
    for (int i = 0; i < 48000 * 3; ++i) {
        (void)kick.processSample();
    }
    REQUIRE(!kick.isActive());
    std::cout << "  808 Kick Peak: " << peak << " | testAnalog808Kick PASSED." << std::endl;
}

void testAnalog909Kick() {
    std::cout << "[Test] Running testAnalog909Kick..." << std::endl;
    Analog909Kick kick;
    kick.prepare(48000.0f);

    kick.trigger(0.95f, 54.0f, 1.2f, 0.35f, 0.4f);
    REQUIRE(kick.isActive());

    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i) {
        float s = kick.processSample();
        REQUIRE(!std::isnan(s) && !std::isinf(s));
        peak = std::max(peak, std::abs(s));
    }
    REQUIRE(peak > 0.5f);
    std::cout << "  909 Kick Peak: " << peak << " | testAnalog909Kick PASSED." << std::endl;
}

void testAnalog808Snare() {
    std::cout << "[Test] Running testAnalog808Snare..." << std::endl;
    Analog808Snare snare;
    snare.prepare(48000.0f);

    snare.trigger(0.85f, 0.7f, 210.0f, 0.25f);
    REQUIRE(snare.isActive());

    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i) {
        float s = snare.processSample();
        REQUIRE(!std::isnan(s) && !std::isinf(s));
        peak = std::max(peak, std::abs(s));
    }
    REQUIRE(peak > 0.2f);
    std::cout << "  808 Snare Peak: " << peak << " | testAnalog808Snare PASSED." << std::endl;
}

void testAnalog808HiHatAndChoke() {
    std::cout << "[Test] Running testAnalog808HiHatAndChoke..." << std::endl;
    Analog808HiHat hat;
    hat.prepare(48000.0f);

    // 1. Trigger Open Hat
    hat.trigger(0.9f, true); // Open
    REQUIRE(hat.isActive());
    REQUIRE(hat.isOpen());

    // Render 50ms of open hat
    float preChokeAmp = 0.0f;
    for (int i = 0; i < 2400; ++i) {
        preChokeAmp = std::abs(hat.processSample());
    }
    REQUIRE(preChokeAmp > 0.05f);

    // 2. Choke with Closed Hat
    hat.choke();
    // Render another 25ms
    for (int i = 0; i < 1200; ++i) {
        (void)hat.processSample();
    }
    float postChokeAmp = std::abs(hat.processSample());
    REQUIRE(postChokeAmp < preChokeAmp * 0.25f);

    std::cout << "  Pre-choke amp: " << preChokeAmp << ", Post-choke amp: " << postChokeAmp
              << " | testAnalog808HiHatAndChoke PASSED." << std::endl;
}

void testAnalog808ClapAndCowbell() {
    std::cout << "[Test] Running testAnalog808ClapAndCowbell..." << std::endl;
    Analog808Clap clap;
    Analog808Cowbell cowbell;
    clap.prepare(48000.0f);
    cowbell.prepare(48000.0f);

    clap.trigger(0.9f);
    cowbell.trigger(0.85f);
    REQUIRE(clap.isActive());
    REQUIRE(cowbell.isActive());

    float clapPeak = 0.0f;
    float cbPeak = 0.0f;
    for (int i = 0; i < 4800; ++i) {
        clapPeak = std::max(clapPeak, std::abs(clap.processSample()));
        cbPeak = std::max(cbPeak, std::abs(cowbell.processSample()));
    }
    REQUIRE(clapPeak > 0.1f);
    REQUIRE(cbPeak > 0.1f);

    std::cout << "  Clap Peak: " << clapPeak << ", Cowbell Peak: " << cbPeak
              << " | testAnalog808ClapAndCowbell PASSED." << std::endl;
}

void testModularDrumKitNode() {
    std::cout << "[Test] Running testModularDrumKitNode in AudioGraph..." << std::endl;
    AudioGraph graph;
    const double sampleRate = 48000.0;
    const uint32_t blockSize = 128;
    graph.prepare(sampleRate, blockSize);

    auto drums = std::make_shared<DrumKitNode>("DrumKit");
    auto gain = std::make_shared<GainNode>("MasterOutput");

    NodeId dId = graph.addNode(drums);
    NodeId gId = graph.addNode(gain);

    REQUIRE(graph.connect(dId, 0, gId, 0));
    graph.setOutputNode(gId, 0);

    // Trigger Kick (36) and Closed Hat (42) simultaneously
    AudioEvent evtKick{AudioEventType::NoteOn, 10, 36, 0.95f, 0, 0.0f};
    AudioEvent evtHat{AudioEventType::NoteOn, 10, 42, 0.80f, 0, 0.0f};
    graph.sendNodeEvent(dId, evtKick);
    graph.sendNodeEvent(dId, evtHat);

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);
    float maxAmp = 0.0f;

    for (int b = 0; b < 10; ++b) {
        graph.process(outL.data(), outR.data(), blockSize);
        for (uint32_t i = 0; i < blockSize; ++i) {
            maxAmp = std::max(maxAmp, std::max(std::abs(outL[i]), std::abs(outR[i])));
        }
    }

    REQUIRE(maxAmp > 0.1f);
    std::cout << "  DrumKit Graph Output Peak: " << maxAmp << " | testModularDrumKitNode PASSED." << std::endl;
}

void testDrumPerformanceBenchmark() {
    std::cout << "[Benchmark] Stress-testing DrumKitNode throughput..." << std::endl;
    AudioGraph graph;
    const double sampleRate = 48000.0;
    const uint32_t blockSize = 64;
    graph.prepare(sampleRate, blockSize);

    auto drums = std::make_shared<DrumKitNode>("DrumKit");
    NodeId dId = graph.addNode(drums);
    graph.setOutputNode(dId, 0);

    // Trigger all drum voices simultaneously
    drums->triggerNote(36, 0.95f); // 909 Kick
    drums->triggerNote(38, 0.85f); // 808 Snare
    drums->triggerNote(46, 0.80f); // 808 Open Hat
    drums->triggerNote(39, 0.90f); // 808 Clap
    drums->triggerNote(56, 0.85f); // 808 Cowbell

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    constexpr int NUM_BLOCKS = 5000;
    const double totalAudioSeconds = (NUM_BLOCKS * blockSize) / sampleRate;

    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_BLOCKS; ++i) {
        graph.process(outL.data(), outR.data(), blockSize);
    }
    const auto end = std::chrono::high_resolution_clock::now();

    const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    const double elapsedSeconds = elapsedMs / 1000.0;
    const double speedup = totalAudioSeconds / elapsedSeconds;

    std::cout << "  Rendered " << totalAudioSeconds << " seconds of 5-voice analog drum synthesis in "
              << elapsedMs << " ms (" << speedup << "x faster than real-time budget)" << std::endl;
    REQUIRE(speedup > 25.0);
    std::cout << "[Benchmark] DrumKit benchmark PASSED." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " Eatsbits TR-808 & TR-909 Drum Tests    " << std::endl;
    std::cout << "========================================" << std::endl;

    testAnalog808Kick();
    testAnalog909Kick();
    testAnalog808Snare();
    testAnalog808HiHatAndChoke();
    testAnalog808ClapAndCowbell();
    testModularDrumKitNode();
    testDrumPerformanceBenchmark();

    std::cout << "\n>>> ALL DRUM SYNTHESIS TESTS PASSED! <<<\n" << std::endl;
    return 0;
}
