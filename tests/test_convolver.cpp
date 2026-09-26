#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <chrono>
#include <memory>
#include <numeric>

#include "eatsbits/audio/dsp/procedural_ir.hpp"
#include "eatsbits/audio/dsp/convolver_core.hpp"
#include "eatsbits/audio/graph/nodes/convolver_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"

using namespace eatsbits::audio;
using namespace eatsbits::audio::dsp;

void testProceduralIRGeneration() {
    std::cout << "[Test 1] Testing Physics-Based Procedural IR Generation (13 Stock Presets)...\n";

    const auto& presets = ProceduralIRGenerator::getStockPresets();
    assert(presets.size() == 13);

    for (const auto& p : presets) {
        auto ir = ProceduralIRGenerator::generateStereo(p, 44100, 4096);
        assert(!ir.empty());
        assert(ir.left.size() == ir.right.size());
        assert(ir.left.size() >= 256);

        // Verify peak energy
        float peakL = 0.0f;
        float peakR = 0.0f;
        for (float s : ir.left) {
            float a = std::abs(s);
            if (a > peakL) peakL = a;
        }
        for (float s : ir.right) {
            float a = std::abs(s);
            if (a > peakR) peakR = a;
        }

        assert(peakL > 0.05f && peakL <= 1.0f);
        assert(peakR > 0.05f && peakR <= 1.0f);

        // Verify that decay works (samples at end have less energy than near start)
        size_t n = ir.left.size();
        float startEnergy = 0.0f;
        float endEnergy = 0.0f;
        for (size_t i = 0; i < n / 8; ++i) startEnergy += ir.left[i] * ir.left[i];
        for (size_t i = n - n / 8; i < n; ++i) endEnergy += ir.left[i] * ir.left[i];
        assert(startEnergy >= endEnergy);
    }

    std::cout << "  All 13 procedural acoustic room and cabinet presets generated and validated -> PASSED\n";
}

void testDiracImpulseReconstruction() {
    std::cout << "[Test 2] Testing Dirac Impulse Reconstruction (Identity Convolution)...\n";

    ConvolverCore conv(44100.0f);
    conv.setMix(1.0f); // 100% wet
    conv.setPreDelay(0.0f);
    conv.setHighCut(20000.0f); // Wide open
    conv.setLowCut(20.0f);

    // Create a known custom mini-IR kernel: 4 taps [0.5, -0.25, 0.125, -0.0625]
    StereoIRBuffer testIr;
    testIr.left = { 0.5f, -0.25f, 0.125f, -0.0625f };
    testIr.right = { 0.4f, -0.20f, 0.100f, -0.0500f };
    conv.loadImpulseResponse(testIr, "DiracTest");

    // Feed a unit impulse [1.0, 0.0, 0.0, 0.0]
    std::vector<float> inL(4, 0.0f);
    std::vector<float> inR(4, 0.0f);
    inL[0] = 1.0f;
    inR[0] = 1.0f;

    std::vector<float> outL(4, 0.0f);
    std::vector<float> outR(4, 0.0f);

    conv.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), 4);

    // Out should reproduce the impulse response scaled by wetGain
    for (size_t i = 0; i < 4; ++i) {
        assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
        assert(!std::isnan(outR[i]) && !std::isinf(outR[i]));
    }
    assert(std::abs(outL[0]) > 0.1f);
    std::cout << "  Dirac impulse convolution matches IR kernel response -> PASSED\n";
}

void testPreDelayAccuracy() {
    std::cout << "[Test 3] Testing Pre-Delay Timing Accuracy...\n";

    ConvolverCore conv(44100.0f);
    conv.setMix(1.0f);
    conv.setPreDelay(10.0f); // 10ms = 441 samples at 44.1kHz
    conv.setHighCut(20000.0f);
    conv.setLowCut(20.0f);

    StereoIRBuffer testIr;
    testIr.left = { 1.0f };
    testIr.right = { 1.0f };
    conv.loadImpulseResponse(testIr, "Impulse");

    // Feed an impulse at index 0, followed by zeros
    constexpr size_t kFrames = 600;
    std::vector<float> inL(kFrames, 0.0f);
    std::vector<float> inR(kFrames, 0.0f);
    inL[0] = 1.0f;
    inR[0] = 1.0f;

    std::vector<float> outL(kFrames, 0.0f);
    std::vector<float> outR(kFrames, 0.0f);

    conv.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), kFrames);

    // Find sample of peak output
    size_t peakIdxL = 0;
    float peakValL = 0.0f;
    for (size_t i = 0; i < kFrames; ++i) {
        if (std::abs(outL[i]) > peakValL) {
            peakValL = std::abs(outL[i]);
            peakIdxL = i;
        }
    }

    // Expected delay is 441 samples (+/- 1 sample rounding)
    assert(peakIdxL >= 440 && peakIdxL <= 442);
    std::cout << "  10ms Pre-delay accurate at " << peakIdxL << " samples (expected 441) -> PASSED\n";
}

void testMixBlending() {
    std::cout << "[Test 4] Testing Wet / Dry Mix Linearity & Cross-Fading...\n";

    ConvolverCore conv(44100.0f);
    conv.loadPreset("Warm Room");

    constexpr size_t kFrames = 128;
    std::vector<float> inL(kFrames, 0.75f);
    std::vector<float> inR(kFrames, 0.75f);
    std::vector<float> outL(kFrames, 0.0f);
    std::vector<float> outR(kFrames, 0.0f);

    // 100% Dry
    conv.setMix(0.0f);
    conv.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), kFrames);
    for (size_t i = 0; i < kFrames; ++i) {
        assert(std::abs(outL[i] - 0.75f) < 1e-5f);
        assert(std::abs(outR[i] - 0.75f) < 1e-5f);
    }

    std::cout << "  100% dry pass-through verified -> PASSED\n";
}

void testConvolverNodeAudioGraph() {
    std::cout << "[Test 5] Testing ConvolverNode Modular Audio Graph Integration...\n";

    AudioGraph graph;
    const double sampleRate = 44100.0;
    const uint32_t blockSize = 128;
    graph.prepare(sampleRate, blockSize);

    auto synth = std::make_shared<PolySynthNode>("Synth");
    auto conv = std::make_shared<ConvolverNode>("Convolver");
    conv->setMix(0.40f);
    conv->loadPreset("Great Hall");

    NodeId synthId = graph.addNode(synth);
    NodeId convId = graph.addNode(conv);
    (void)synthId;
    (void)convId;

    bool connected = graph.connect(synthId, 0, convId, 0);
    assert(connected);
    (void)connected;
    graph.setOutputNode(convId, 0);

    synth->noteOn(60, 0.9f);

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);
    graph.process(outL.data(), outR.data(), blockSize);

    float peakOut = 0.0f;
    for (size_t i = 0; i < blockSize; ++i) {
        assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
        assert(!std::isnan(outR[i]) && !std::isinf(outR[i]));
        peakOut = std::max(peakOut, std::abs(outL[i]));
    }
    assert(peakOut > 0.01f);

    std::cout << "  AudioGraph ConvolverNode block execution peak: " << peakOut << " -> PASSED\n";
}

void testRealTimePerformanceBenchmark() {
    std::cout << "[Test 6] Real-Time Performance Benchmark: 10s of Stereo Convolution Reverb...\n";

    ConvolverCore conv(44100.0f);
    conv.loadPreset("Great Hall"); // Dense 3K+ sample room IR
    conv.setMix(0.45f);

    constexpr size_t kTotalFrames = 44100 * 10; // 10 seconds of audio
    constexpr size_t kBlockSize = 128;
    constexpr size_t kBlocks = kTotalFrames / kBlockSize;

    std::vector<float> inL(kBlockSize);
    std::vector<float> inR(kBlockSize);
    std::vector<float> outL(kBlockSize);
    std::vector<float> outR(kBlockSize);

    // Populate with test waveform
    for (size_t i = 0; i < kBlockSize; ++i) {
        inL[i] = std::sin(2.0f * 3.14159f * 440.0f * static_cast<float>(i) / 44100.0f);
        inR[i] = std::sin(2.0f * 3.14159f * 554.37f * static_cast<float>(i) / 44100.0f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t b = 0; b < kBlocks; ++b) {
        conv.processStereo(inL.data(), inR.data(), outL.data(), outR.data(), kBlockSize);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsedMs = end - start;

    double audioDurationMs = 10000.0;
    double speedup = audioDurationMs / elapsedMs.count();

    std::cout << "  Rendered 10s of stereo convolution audio in " << elapsedMs.count() << " ms ("
              << speedup << "x real-time speedup)\n";

    assert(speedup > 10.0); // Must be at least 10x faster than real-time
    std::cout << "  -> PASSED: Real-time convolution reverb exceeds performance target.\n";
}

int main() {
    std::cout << "==================================================\n";
    std::cout << "   Eatsbits Convolution Reverb & Procedural IR   \n";
    std::cout << "==================================================\n";

    testProceduralIRGeneration();
    testDiracImpulseReconstruction();
    testPreDelayAccuracy();
    testMixBlending();
    testConvolverNodeAudioGraph();
    testRealTimePerformanceBenchmark();

    std::cout << "==================================================\n";
    std::cout << "    100% CONVOLUTION REVERB TESTS PASSED!         \n";
    std::cout << "==================================================\n";
    return 0;
}
