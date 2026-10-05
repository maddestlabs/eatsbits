#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <memory>
#include <numeric>
#include <numbers>
#include <algorithm>

#include "eatsbits/audio/dsp/dynamics_processor.hpp"
#include "eatsbits/audio/dsp/parametric_eq.hpp"
#include "eatsbits/audio/dsp/chorus_flanger.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/parametric_eq_node.hpp"
#include "eatsbits/audio/graph/nodes/compressor_node.hpp"
#include "eatsbits/audio/graph/nodes/chorus_node.hpp"
#include "eatsbits/audio/graph/nodes/limiter_node.hpp"
#include "eatsbits/audio/graph/nodes/waveshaper_node.hpp"
#include "eatsbits/audio/audio_engine.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::dsp;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

// Generate a sine wave buffer
static void generateSine(std::vector<float>& buf, float freqHz, float sampleRate, float amplitude = 1.0f) {
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    for (size_t i = 0; i < buf.size(); ++i) {
        buf[i] = amplitude * std::sin(twoPi * freqHz * static_cast<float>(i) / sampleRate);
    }
}

// 1. Compressor Core Tests
void testCompressorGainReduction() {
    std::cout << "[Test 1] Testing Compressor Core Gain Reduction...\n";

    dsp::CompressorCore comp(44100.0f);
    comp.setThreshold(-20.0f); // -20 dB threshold
    comp.setRatio(4.0f);       // 4:1 ratio
    comp.setKnee(0.0f);        // hard knee
    comp.setAttack(5.0f);      // fast attack
    comp.setRelease(50.0f);    // 50ms release
    comp.setMakeupGain(0.0f);  // no makeup

    // Generate 100ms of 0 dBFS sine (amplitude 1.0)
    const size_t numFrames = 4410;
    std::vector<float> left(numFrames);
    std::vector<float> right(numFrames);
    generateSine(left, 1000.0f, 44100.0f, 1.0f);
    right = left;

    comp.processStereo(left.data(), right.data(), numFrames);

    float grDb = comp.getCurrentGainReductionDb();
    std::cout << "  Compressor Gain Reduction: " << grDb << " dB\n";
    TEST_ASSERT(grDb < -10.0f); // Substantial gain reduction occurred

    float tailMax = 0.0f;
    for (size_t i = numFrames - 500; i < numFrames; ++i) {
        tailMax = std::max(tailMax, std::abs(left[i]));
    }
    std::cout << "  Attenuated signal peak: " << tailMax << "\n";
    TEST_ASSERT(tailMax < 0.35f);
    TEST_ASSERT(tailMax > 0.10f);

    std::cout << "  -> PASSED\n";
}

void testCompressorSidechainDucking() {
    std::cout << "[Test 2] Testing Compressor Sidechain Ducking...\n";

    dsp::CompressorCore comp(44100.0f);
    comp.setThreshold(-18.0f);
    comp.setRatio(8.0f);
    comp.setAttack(1.0f);
    comp.setRelease(100.0f);

    const size_t numFrames = 4410;
    // Main audio is a quiet signal (amplitude 0.1, approx -20 dBFS, below threshold)
    std::vector<float> mainL(numFrames, 0.1f);
    std::vector<float> mainR(numFrames, 0.1f);

    // Process without sidechain: no gain reduction should occur
    comp.processStereo(mainL.data(), mainR.data(), numFrames);
    TEST_ASSERT(std::abs(comp.getCurrentGainReductionDb()) < 0.5f);

    // Reset and now provide a loud sidechain kick (+3 dBFS / 1.41 amplitude)
    comp.reset();
    std::vector<float> scL(numFrames, 1.41f);
    std::vector<float> scR(numFrames, 1.41f);
    std::vector<float> duckedL(numFrames, 0.1f);
    std::vector<float> duckedR(numFrames, 0.1f);

    comp.processStereo(duckedL.data(), duckedR.data(), numFrames, scL.data(), scR.data());

    std::cout << "  Sidechain Gain Reduction: " << comp.getCurrentGainReductionDb() << " dB\n";
    TEST_ASSERT(comp.getCurrentGainReductionDb() < -10.0f);
    TEST_ASSERT(duckedL.back() < 0.05f);

    std::cout << "  -> PASSED\n";
}

// 2. Limiter Core Tests
void testLimiterZeroOvershoot() {
    std::cout << "[Test 3] Testing Brickwall Limiter Zero-Overshoot...\n";

    dsp::LimiterCore limiter(44100.0f);
    limiter.setCeilingDb(-0.5f); // Ceiling is ~0.944
    limiter.setRelease(30.0f);
    limiter.setLookaheadMs(2.0f);

    const float ceilingLin = std::pow(10.0f, -0.5f / 20.0f);

    // Create a signal with severe clipping spikes (amplitude 4.0 = +12 dBFS)
    const size_t numFrames = 8820;
    std::vector<float> left(numFrames);
    std::vector<float> right(numFrames);
    generateSine(left, 440.0f, 44100.0f, 4.0f);
    right = left;

    limiter.processStereo(left.data(), right.data(), numFrames);

    float maxObserved = 0.0f;
    for (size_t i = 0; i < numFrames; ++i) {
        maxObserved = std::max(maxObserved, std::abs(left[i]));
        maxObserved = std::max(maxObserved, std::abs(right[i]));
        TEST_ASSERT(!std::isnan(left[i]));
        TEST_ASSERT(!std::isnan(right[i]));
        TEST_ASSERT(std::abs(left[i]) <= ceilingLin + 1e-4f);
        TEST_ASSERT(std::abs(right[i]) <= ceilingLin + 1e-4f);
    }
    std::cout << "  Ceiling linear: " << ceilingLin << ", Max limited sample: " << maxObserved << "\n";
    std::cout << "  -> PASSED\n";
}

// 3. Parametric EQ Core Tests
void testParametricEqFrequencyResponse() {
    std::cout << "[Test 4] Testing 5-Band Parametric EQ Frequency Response...\n";

    dsp::ParametricEqCore eq(44100.0f);

    // Highpass at 30Hz: 10Hz should be heavily attenuated
    float mag10Hz = eq.evalMagnitudeDb(10.0f);
    float mag1000Hz = eq.evalMagnitudeDb(1000.0f);
    std::cout << "  Magnitude at 10Hz: " << mag10Hz << " dB (expected < -10 dB)\n";
    std::cout << "  Magnitude at 1000Hz: " << mag1000Hz << " dB (expected ~ 0 dB)\n";
    TEST_ASSERT(mag10Hz < -10.0f);
    TEST_ASSERT(std::abs(mag1000Hz) < 0.5f);

    // Boost Band 2 (Mid Bell at 1000 Hz) by +6 dB
    eq.configureBand(2, true, dsp::BiquadType::Peaking, 1000.0f, 2.0f, 6.0f);
    float boosted1k = eq.evalMagnitudeDb(1000.0f);
    std::cout << "  Boosted Magnitude at 1000Hz: " << boosted1k << " dB (expected ~ +6 dB)\n";
    TEST_ASSERT(std::abs(boosted1k - 6.0f) < 0.2f);

    // Process a test block and verify stability
    std::vector<float> bufL(1024, 0.5f);
    std::vector<float> bufR(1024, 0.5f);
    eq.processStereo(bufL.data(), bufR.data(), 1024);
    for (float s : bufL) {
        TEST_ASSERT(!std::isnan(s));
        TEST_ASSERT(!std::isinf(s));
    }
    std::cout << "  -> PASSED\n";
}

// 4. Chorus / Flanger Core Tests
void testChorusQuadratureModulation() {
    std::cout << "[Test 5] Testing Chorus Quadrature Modulation...\n";

    dsp::ChorusFlangerCore chorus(44100.0f);
    chorus.setRateHz(1.5f);
    chorus.setDepthMs(3.0f);
    chorus.setBaseDelayMs(10.0f);
    chorus.setMix(1.0f); // 100% wet to measure modulation

    const size_t numFrames = 4410; // 100ms
    std::vector<float> left(numFrames, 0.0f);
    std::vector<float> right(numFrames, 0.0f);

    // Single impulse at sample 0
    left[0] = 1.0f;
    right[0] = 1.0f;

    chorus.processStereo(left.data(), right.data(), numFrames);

    // Signal early in delay line should be silent
    float sumEarly = 0.0f;
    for (size_t i = 0; i < 200; ++i) {
        sumEarly += std::abs(left[i]);
    }
    TEST_ASSERT(sumEarly < 1e-4f);

    // Signal in delay region should have energy in both channels
    float sumLateL = 0.0f;
    float sumLateR = 0.0f;
    for (size_t i = 300; i < 800; ++i) {
        sumLateL += std::abs(left[i]);
        sumLateR += std::abs(right[i]);
    }
    std::cout << "  Chorus Wet Delay Sums (L=" << sumLateL << ", R=" << sumLateR << ")\n";
    TEST_ASSERT(sumLateL > 0.1f);
    TEST_ASSERT(sumLateR > 0.1f);

    std::cout << "  -> PASSED\n";
}

// 5. AudioGraph Integration Chain
void testAudioGraphChannelStripProcessing() {
    std::cout << "[Test 6] Testing AudioGraph Full Channel Strip (Synth -> EQ -> Comp -> Chorus -> Limiter)...\n";

    audio::AudioGraph graph;
    graph.prepare(44100.0, 512);

    auto synth = std::make_shared<audio::PolySynthNode>("Synth");
    auto eq = std::make_shared<audio::ParametricEqNode>("StudioEQ");
    auto comp = std::make_shared<audio::CompressorNode>("StudioComp");
    auto chorus = std::make_shared<audio::ChorusNode>("StudioChorus");
    auto limiter = std::make_shared<audio::LimiterNode>("MasterLimiter");

    auto synthId = graph.addNode(synth);
    auto eqId = graph.addNode(eq);
    auto compId = graph.addNode(comp);
    auto chorusId = graph.addNode(chorus);
    auto limiterId = graph.addNode(limiter);

    // Connect in series: Synth -> EQ -> Comp -> Chorus -> Limiter
    bool c1 = graph.connect(synthId, 0, eqId, 0);
    bool c2 = graph.connect(eqId, 0, compId, 0);
    bool c3 = graph.connect(compId, 0, chorusId, 0);
    bool c4 = graph.connect(chorusId, 0, limiterId, 0);
    TEST_ASSERT(c1 && c2 && c3 && c4);
    graph.setOutputNode(limiterId, 0);

    // Trigger note on poly synth
    synth->noteOn(60, 0.9f);

    const size_t blockSize = 512;
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    // Process 20 blocks (~230 ms)
    float peak = 0.0f;
    for (int block = 0; block < 20; ++block) {
        graph.process(outL.data(), outR.data(), static_cast<uint32_t>(blockSize));
        for (size_t i = 0; i < blockSize; ++i) {
            TEST_ASSERT(!std::isnan(outL[i]));
            TEST_ASSERT(!std::isnan(outR[i]));
            peak = std::max(peak, std::abs(outL[i]));
            peak = std::max(peak, std::abs(outR[i]));
        }
    }

    std::cout << "  Channel strip processed cleanly, output peak: " << peak << "\n";
    TEST_ASSERT(peak > 0.01f);
    TEST_ASSERT(peak <= 1.0f); // Master limiter ceiling

    std::cout << "  -> PASSED\n";
}

// 6. Real-time Benchmark: 10 Seconds of Audio through Full Strip
void testBenchmarkFullChannelStrip() {
    std::cout << "[Test 7] Benchmarking Full Studio Channel Strip (10s audio rendering)...\n";

    audio::AudioGraph graph;
    graph.prepare(44100.0, 512);

    auto synth = std::make_shared<audio::PolySynthNode>("Synth");
    auto eq = std::make_shared<audio::ParametricEqNode>("StudioEQ");
    auto comp = std::make_shared<audio::CompressorNode>("StudioComp");
    auto chorus = std::make_shared<audio::ChorusNode>("StudioChorus");
    auto limiter = std::make_shared<audio::LimiterNode>("MasterLimiter");

    auto synthId = graph.addNode(synth);
    auto eqId = graph.addNode(eq);
    auto compId = graph.addNode(comp);
    auto chorusId = graph.addNode(chorus);
    auto limiterId = graph.addNode(limiter);

    bool c1 = graph.connect(synthId, 0, eqId, 0);
    bool c2 = graph.connect(eqId, 0, compId, 0);
    bool c3 = graph.connect(compId, 0, chorusId, 0);
    bool c4 = graph.connect(chorusId, 0, limiterId, 0);
    TEST_ASSERT(c1 && c2 && c3 && c4);
    graph.setOutputNode(limiterId, 0);

    synth->noteOn(60, 0.8f);
    synth->noteOn(64, 0.8f);
    synth->noteOn(67, 0.8f);

    const size_t blockSize = 512;
    const size_t totalFrames = 441000; // 10 seconds
    const size_t numBlocks = totalFrames / blockSize;

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t b = 0; b < numBlocks; ++b) {
        graph.process(outL.data(), outR.data(), static_cast<uint32_t>(blockSize));
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsedMs = end - start;

    double realtimeMs = 10000.0;
    double speedup = realtimeMs / elapsedMs.count();

    std::cout << "[BENCHMARK] Full Studio Channel Strip (3-Voice Synth + 5-Band EQ + Comp + Chorus + Limiter):\n"
              << "            Rendered 10.0s in " << elapsedMs.count() << " ms ("
              << speedup << "x faster than real-time)\n";

    TEST_ASSERT(speedup > 25.0); // Real-time safety requirement (> 25x real-time)
    std::cout << "  -> PASSED\n";
}

void testAudioEngineDynamicFxRouting() {
    std::cout << "[Test 8] Testing AudioEngine Dynamic Audio FX Insert & Parameter Modulation...\n";
    AudioEngineConfig cfg;
    cfg.sampleRate = 44100;
    cfg.bufferFrameSize = 256;
    AudioEngine engine;
    engine.initialize(cfg);
    engine.setupDefaultAcidBeatGraph();

    // Verify initial state has Track 0 FX
    auto fx0 = engine.getTrackAudioFx(0);
    TEST_ASSERT(fx0.size() >= 1);

    // Rebuild with 2 inserts: Tube Distortion and Studio Dynamics
    std::vector<AudioEngine::TrackAudioFxItem> newFx = {
        {"Acid Distortion", "TUBE_DISTORTION", 0.75f, 0.90f, true},
        {"Acid Dynamics", "DYNAMICS_COMP", 0.50f, 0.80f, true}
    };
    bool ok = engine.rebuildTrackAudioFx(0, newFx);
    TEST_ASSERT(ok);

    auto updatedFx = engine.getTrackAudioFx(0);
    TEST_ASSERT(updatedFx.size() == 2);
    TEST_ASSERT(updatedFx[0].name == "Acid Distortion");
    TEST_ASSERT(updatedFx[1].name == "Acid Dynamics");

    // Modulate parameters in-place without glitching
    bool pOk1 = engine.setTrackAudioFxParam(0, 0, "drive", 0.85f);
    bool pOk2 = engine.setTrackAudioFxParam(0, 0, "mix", 0.60f);
    TEST_ASSERT(pOk1 && pOk2);

    // Bypass first FX
    newFx[0].enabled = false;
    bool bypassOk = engine.rebuildTrackAudioFx(0, newFx);
    TEST_ASSERT(bypassOk);
    TEST_ASSERT(!engine.getTrackAudioFx(0)[0].enabled);

    // Process a block through graph
    std::vector<float> bufL(256, 0.0f);
    std::vector<float> bufR(256, 0.0f);
    engine.getGraph().process(bufL.data(), bufR.data(), 256);

    std::cout << "  -> PASSED\n";
}

void testWaveShaperCalibrationAndDspFeatures() {
    std::cout << "[Test 9] Testing WaveShaper Calibration, Saturation Makeup & Tone/Bias DSP...\n";

    WaveShaperNode ws("TubeSaturation");
    ws.prepare(44100.0, 512);

    TEST_ASSERT(std::abs(ws.getDrive() - 2.0f) < 0.001f);
    TEST_ASSERT(std::abs(ws.getTone() - 0.75f) < 0.001f);
    TEST_ASSERT(std::abs(ws.getBias() - 0.5f) < 0.001f);

    ws.setParameter(0, 4.0f);
    TEST_ASSERT(std::abs(ws.getDrive() - 4.0f) < 0.001f);
    ws.setParameter(3, 0.40f);
    TEST_ASSERT(std::abs(ws.getTone() - 0.40f) < 0.001f);
    ws.setParameter(4, 0.70f);
    TEST_ASSERT(std::abs(ws.getBias() - 0.70f) < 0.001f);
    ws.setParameter(2, 1.0f);
    TEST_ASSERT(ws.getShape() == 1);

    // Test makeup gain calibration: drive=1.0 vs drive=9.0
    const size_t numFrames = 512;
    std::vector<float> inL(numFrames);
    std::vector<float> inR(numFrames);
    std::vector<float> outL(numFrames, 0.0f);
    std::vector<float> outR(numFrames, 0.0f);
    generateSine(inL, 1000.0f, 44100.0f, 0.3f);
    inR = inL;

    auto wsNode = std::make_shared<WaveShaperNode>("TestWS");
    wsNode->prepare(44100.0, numFrames);
    wsNode->setDrive(1.0f);
    wsNode->setMix(1.0f);
    wsNode->setBias(0.5f);
    wsNode->setTone(1.0f);

    wsNode->setInputBufferPtr(0, 0, inL.data());
    wsNode->setInputBufferPtr(0, 1, inR.data());
    wsNode->setOutputBufferPtr(0, 0, outL.data());
    wsNode->setOutputBufferPtr(0, 1, outR.data());
    wsNode->processBlock(numFrames);

    float peakAtDrive1 = 0.0f;
    for (size_t i = 0; i < numFrames; ++i) {
        peakAtDrive1 = std::max(peakAtDrive1, std::abs(outL[i]));
    }

    // Now test with drive = 9.0f
    wsNode->reset();
    wsNode->setDrive(9.0f);
    wsNode->setInputBufferPtr(0, 0, inL.data());
    wsNode->setInputBufferPtr(0, 1, inR.data());
    wsNode->setOutputBufferPtr(0, 0, outL.data());
    wsNode->setOutputBufferPtr(0, 1, outR.data());
    wsNode->processBlock(numFrames);

    float peakAtDrive9 = 0.0f;
    for (size_t i = 0; i < numFrames; ++i) {
        peakAtDrive9 = std::max(peakAtDrive9, std::abs(outL[i]));
    }

    std::cout << "  Peak at drive 1.0: " << peakAtDrive1 << ", Peak at drive 9.0: " << peakAtDrive9 << "\n";
    TEST_ASSERT(peakAtDrive9 <= 0.65f);
    TEST_ASSERT(peakAtDrive9 >= 0.20f);

    // Test tone filtering: dark tone (0.05) vs open tone (1.0) on high frequency (8 kHz)
    std::vector<float> highFreqL(numFrames);
    generateSine(highFreqL, 8000.0f, 44100.0f, 0.4f);
    wsNode->setDrive(1.0f);
    wsNode->setTone(0.05f);
    wsNode->reset();
    wsNode->setInputBufferPtr(0, 0, highFreqL.data());
    wsNode->setInputBufferPtr(0, 1, highFreqL.data());
    wsNode->setOutputBufferPtr(0, 0, outL.data());
    wsNode->setOutputBufferPtr(0, 1, outR.data());
    wsNode->processBlock(numFrames);

    float peakDark = 0.0f;
    for (size_t i = numFrames / 2; i < numFrames; ++i) {
        peakDark = std::max(peakDark, std::abs(outL[i]));
    }

    wsNode->setTone(1.0f);
    wsNode->reset();
    wsNode->setInputBufferPtr(0, 0, highFreqL.data());
    wsNode->setInputBufferPtr(0, 1, highFreqL.data());
    wsNode->setOutputBufferPtr(0, 0, outL.data());
    wsNode->setOutputBufferPtr(0, 1, outR.data());
    wsNode->processBlock(numFrames);

    float peakBright = 0.0f;
    for (size_t i = numFrames / 2; i < numFrames; ++i) {
        peakBright = std::max(peakBright, std::abs(outL[i]));
    }
    std::cout << "  Tone dark peak: " << peakDark << ", Tone bright peak: " << peakBright << "\n";
    TEST_ASSERT(peakDark < peakBright * 0.5f);

    std::cout << "  -> PASSED\n";
}

void testAudioEngineParametricEqAndLimiterDispatch() {
    std::cout << "[Test 10] Testing Audio Engine EQ, Limiter & WaveShaper Parameter Dispatch...\n";

    AudioEngineConfig cfg;
    cfg.sampleRate = 44100;
    cfg.bufferFrameSize = 256;
    AudioEngine engine;
    engine.initialize(cfg);
    engine.setupDefaultAcidBeatGraph();

    std::vector<AudioEngine::TrackAudioFxItem> inserts = {
        {"Studio Parametric EQ", "PARAMETRIC_EQ", 0.5f, 1.0f, true},
        {"Master Brickwall Limiter", "BRICKWALL_LIMITER", 0.5f, 1.0f, true},
        {"Dynamic Tube Distortion", "TUBE_DISTORTION", 0.5f, 0.8f, true}
    };
    bool ok = engine.rebuildTrackAudioFx(0, inserts);
    TEST_ASSERT(ok);

    auto fxList = engine.getTrackAudioFx(0);
    TEST_ASSERT(fxList.size() == 3);

    // Parametric EQ parameter modulation
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 0, "low", 0.75f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 0, "mid", 0.35f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 0, "high", 0.80f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 0, "q", 0.40f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 0, "gain", 0.60f));

    // Limiter parameter modulation
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 1, "ceiling", -0.5f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 1, "release", 75.0f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 1, "lookahead", 5.0f));

    // WaveShaper parameter modulation
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 2, "drive", 0.70f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 2, "tone", 0.45f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 2, "bias", 0.60f));
    TEST_ASSERT(engine.setTrackAudioFxParam(0, 2, "mix", 0.90f));

    // Process a block
    std::vector<float> bufL(256, 0.0f);
    std::vector<float> bufR(256, 0.0f);
    engine.getGraph().process(bufL.data(), bufR.data(), 256);

    std::cout << "  -> PASSED\n";
}

int main() {
    std::cout << "====================================================\n";
    std::cout << "   EATSBITS STUDIO DYNAMICS & INSERT FX SUITE TESTS \n";
    std::cout << "====================================================\n";

    testCompressorGainReduction();
    testCompressorSidechainDucking();
    testLimiterZeroOvershoot();
    testParametricEqFrequencyResponse();
    testChorusQuadratureModulation();
    testAudioGraphChannelStripProcessing();
    testBenchmarkFullChannelStrip();
    testAudioEngineDynamicFxRouting();
    testWaveShaperCalibrationAndDspFeatures();
    testAudioEngineParametricEqAndLimiterDispatch();

    std::cout << "====================================================\n";
    std::cout << " ALL 10 STUDIO FX & DYNAMICS TESTS PASSED SUCCESSFULLY \n";
    std::cout << "====================================================\n";
    return 0;
}
