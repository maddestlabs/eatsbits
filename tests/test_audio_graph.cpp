#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/biquad_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "[FAIL] Requirement failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while (false)

void testBasicGraphTopology() {
    std::cout << "[Test] Running testBasicGraphTopology..." << std::endl;
    AudioGraph graph;
    graph.prepare(48000.0, 128);

    auto synth = std::make_shared<PolySynthNode>("Synth");
    auto filter = std::make_shared<BiquadNode>("Filter");
    auto gain = std::make_shared<GainNode>("MasterGain");

    NodeId synthId = graph.addNode(synth);
    NodeId filterId = graph.addNode(filter);
    NodeId gainId = graph.addNode(gain);

    REQUIRE(synthId != INVALID_NODE_ID);
    REQUIRE(filterId != INVALID_NODE_ID);
    REQUIRE(gainId != INVALID_NODE_ID);
    REQUIRE(graph.getNodeCount() == 3);

    // Connect: Synth (port 0) -> Filter (port 0) -> Gain (port 0)
    REQUIRE(graph.connect(synthId, 0, filterId, 0));
    REQUIRE(graph.connect(filterId, 0, gainId, 0));

    graph.setOutputNode(gainId, 0);
    REQUIRE(graph.getOutputNodeId() == gainId);

    std::cout << "[Test] testBasicGraphTopology PASSED." << std::endl;
}

void testCycleDetection() {
    std::cout << "[Test] Running testCycleDetection..." << std::endl;
    AudioGraph graph;
    graph.prepare(48000.0, 128);

    auto nodeA = std::make_shared<GainNode>("NodeA");
    auto nodeB = std::make_shared<GainNode>("NodeB");
    auto nodeC = std::make_shared<GainNode>("NodeC");

    NodeId idA = graph.addNode(nodeA);
    NodeId idB = graph.addNode(nodeB);
    NodeId idC = graph.addNode(nodeC);

    // Self loop must be rejected
    REQUIRE(!graph.connect(idA, 0, idA, 0));

    // Linear chain: A -> B -> C
    REQUIRE(graph.connect(idA, 0, idB, 0));
    REQUIRE(graph.connect(idB, 0, idC, 0));

    // Closing the cycle: C -> A must be detected and rejected!
    REQUIRE(!graph.connect(idC, 0, idA, 0));

    // Verify graph is still valid and has exactly 2 connections
    REQUIRE(graph.getConnections().size() == 2);

    std::cout << "[Test] testCycleDetection PASSED." << std::endl;
}

void testAudioRenderingThroughGraph() {
    std::cout << "[Test] Running testAudioRenderingThroughGraph..." << std::endl;
    AudioGraph graph;
    const double sampleRate = 48000.0;
    const uint32_t blockSize = 128;
    graph.prepare(sampleRate, blockSize);

    auto synth = std::make_shared<PolySynthNode>("Synth");
    auto filter = std::make_shared<BiquadNode>("Filter");
    auto delay = std::make_shared<DelayNode>("Delay");
    auto gain = std::make_shared<GainNode>("Gain");

    filter->setCutoff(2000.0f);
    filter->setResonance(1.5f);
    delay->setDelayTimeMs(100.0f);
    delay->setFeedback(0.3f);
    delay->setDryWet(0.5f);
    gain->setVolume(0.8f);

    NodeId synthId = graph.addNode(synth);
    NodeId filterId = graph.addNode(filter);
    NodeId delayId = graph.addNode(delay);
    NodeId gainId = graph.addNode(gain);

    REQUIRE(graph.connect(synthId, 0, filterId, 0));
    REQUIRE(graph.connect(filterId, 0, delayId, 0));
    REQUIRE(graph.connect(delayId, 0, gainId, 0));
    graph.setOutputNode(gainId, 0);

    // Trigger NoteOn (Middle C = 60)
    synth->noteOn(60, 0.9f);

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    float maxAmpL = 0.0f;
    float maxAmpR = 0.0f;

    // Process 20 blocks (~53ms)
    for (int block = 0; block < 20; ++block) {
        graph.process(outL.data(), outR.data(), blockSize);
        for (uint32_t i = 0; i < blockSize; ++i) {
            REQUIRE(!std::isnan(outL[i]) && !std::isinf(outL[i]));
            REQUIRE(!std::isnan(outR[i]) && !std::isinf(outR[i]));
            maxAmpL = std::max(maxAmpL, std::abs(outL[i]));
            maxAmpR = std::max(maxAmpR, std::abs(outR[i]));
        }
    }

    REQUIRE(maxAmpL > 0.01f);
    REQUIRE(maxAmpR > 0.01f);
    std::cout << "  Rendered with Peak Amp L: " << maxAmpL << ", R: " << maxAmpR << std::endl;
    std::cout << "[Test] testAudioRenderingThroughGraph PASSED." << std::endl;
}

void testAcidBasslineInModularGraph() {
    std::cout << "[Test] Running testAcidBasslineInModularGraph..." << std::endl;
    AudioGraph graph;
    const double sampleRate = 48000.0;
    const uint32_t blockSize = 128;
    graph.prepare(sampleRate, blockSize);

    auto tb303 = std::make_shared<Tb303Node>("Tb303Acid");
    auto delay = std::make_shared<DelayNode>("AcidEcho");
    auto gain = std::make_shared<GainNode>("MasterOutput");

    tb303->setCutoff(1200.0f);
    tb303->setResonance(0.8f);
    delay->setDelayTimeMs(125.0f); // 16th note echo
    delay->setFeedback(0.4f);
    delay->setDryWet(0.35f);

    NodeId tbId = graph.addNode(tb303);
    NodeId delayId = graph.addNode(delay);
    NodeId gainId = graph.addNode(gain);

    REQUIRE(graph.connect(tbId, 0, delayId, 0));
    REQUIRE(graph.connect(delayId, 0, gainId, 0));
    graph.setOutputNode(gainId, 0);

    // Play an accented note
    tb303->noteOn(36, 0.95f, false, true);

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);
    float peak = 0.0f;

    for (int b = 0; b < 10; ++b) {
        graph.process(outL.data(), outR.data(), blockSize);
        for (uint32_t i = 0; i < blockSize; ++i) {
            peak = std::max(peak, std::abs(outL[i]));
        }
    }

    REQUIRE(peak > 0.05f);
    std::cout << "  TB-303 Modular Peak: " << peak << std::endl;
    std::cout << "[Test] testAcidBasslineInModularGraph PASSED." << std::endl;
}

void testGraphPerformanceBenchmark() {
    std::cout << "[Benchmark] Stress-testing AudioGraph execution budget..." << std::endl;
    AudioGraph graph;
    const double sampleRate = 48000.0;
    const uint32_t blockSize = 64; // 1.33 ms real-time budget per block
    graph.prepare(sampleRate, blockSize);

    auto synth = std::make_shared<PolySynthNode>("Synth");
    auto filter = std::make_shared<BiquadNode>("Filter");
    auto delay = std::make_shared<DelayNode>("Delay");
    auto gain = std::make_shared<GainNode>("MasterGain");

    NodeId sId = graph.addNode(synth);
    NodeId fId = graph.addNode(filter);
    NodeId dId = graph.addNode(delay);
    NodeId gId = graph.addNode(gain);

    REQUIRE(graph.connect(sId, 0, fId, 0));
    REQUIRE(graph.connect(fId, 0, dId, 0));
    REQUIRE(graph.connect(dId, 0, gId, 0));
    graph.setOutputNode(gId, 0);

    // Play 4-note chord
    synth->noteOn(48, 0.8f);
    synth->noteOn(52, 0.8f);
    synth->noteOn(55, 0.8f);
    synth->noteOn(59, 0.8f);

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

    std::cout << "  Rendered " << totalAudioSeconds << " seconds of modular graph audio in "
              << elapsedMs << " ms (" << speedup << "x faster than real-time budget)" << std::endl;
    REQUIRE(speedup > 20.0);
    std::cout << "[Benchmark] AudioGraph benchmark PASSED." << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " Eatsbits AudioGraph & DAG Evaluator Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    testBasicGraphTopology();
    testCycleDetection();
    testAudioRenderingThroughGraph();
    testAcidBasslineInModularGraph();
    testGraphPerformanceBenchmark();

    std::cout << "\n>>> ALL AUDIOGRAPH TESTS PASSED SUCCESSFULLY! <<<\n" << std::endl;
    return 0;
}
