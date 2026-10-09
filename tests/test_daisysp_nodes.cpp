#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <memory>

#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/daisy/daisy_nodes.hpp"
#include "eatsbits/audio/audio_engine.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

static bool isBufferFinite(const float* buf, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (std::isnan(buf[i]) || std::isinf(buf[i])) {
            return false;
        }
    }
    return true;
}

// 1. DaisyOscillatorNode Tests
void testDaisyOscillator() {
    std::cout << "[Test 1] Testing DaisyOscillatorNode...\n";

    AudioGraph graph;
    auto osc = std::make_shared<DaisyOscillatorNode>("TestOsc");
    NodeId oscId = graph.addNode(osc);
    graph.setOutputNode(oscId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    // Test each waveform mode
    for (int wf = 0; wf <= 8; ++wf) {
        osc->setWaveform(static_cast<DaisyOscillatorNode::Waveform>(wf));
        osc->setFrequency(440.0f);
        osc->setAmplitude(0.75f);
        osc->setPulseWidth(0.5f);
        osc->setSawWidth(0.5f);

        graph.process(outL.data(), outR.data(), 256);
        TEST_ASSERT(isBufferFinite(outL.data(), 256));
        TEST_ASSERT(isBufferFinite(outR.data(), 256));

        // Check for non-silent output
        float energy = 0.0f;
        for (int i = 0; i < 256; ++i) {
            energy += std::abs(outL[i]);
        }
        TEST_ASSERT(energy > 1.0f);
    }

    // Test extreme parameter sweeps (low/high frequencies)
    float testFreqs[] = { 10.0f, 100.0f, 1000.0f, 10000.0f, 20000.0f };
    for (float f : testFreqs) {
        osc->setFrequency(f);
        graph.process(outL.data(), outR.data(), 256);
        TEST_ASSERT(isBufferFinite(outL.data(), 256));
    }

    std::cout << "  [SUCCESS] DaisyOscillatorNode verified across waveforms and parameter sweeps!\n";
}

// 2. DaisyFilterNode Tests
void testDaisyFilter() {
    std::cout << "[Test 2] Testing DaisyFilterNode (MoogLadder & SVF)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<DaisyOscillatorNode>("SourceOsc");
    osc->setWaveform(DaisyOscillatorNode::Waveform::PolyBlepSaw);
    osc->setFrequency(120.0f);
    osc->setAmplitude(0.8f);

    auto filter = std::make_shared<DaisyFilterNode>("TestFilter");
    NodeId oscId = graph.addNode(osc);
    NodeId fltId = graph.addNode(filter);

    graph.connect(oscId, 0, fltId, 0); // L/R from osc port 0 to filter port 0
    graph.setOutputNode(fltId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    // Test MoogLadder
    filter->setMode(DaisyFilterNode::FilterMode::MoogLadder);
    filter->setCutoff(500.0f);
    filter->setResonance(0.7f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));
    TEST_ASSERT(isBufferFinite(outR.data(), 256));

    // Test SVF Modes: Low, High, Band, Notch
    DaisyFilterNode::FilterMode svfModes[] = {
        DaisyFilterNode::FilterMode::SvfLowpass,
        DaisyFilterNode::FilterMode::SvfHighpass,
        DaisyFilterNode::FilterMode::SvfBandpass,
        DaisyFilterNode::FilterMode::SvfNotch
    };

    for (auto m : svfModes) {
        filter->setMode(m);
        filter->setCutoff(2000.0f);
        filter->setResonance(0.5f);
        filter->setDrive(0.2f);
        graph.process(outL.data(), outR.data(), 256);
        TEST_ASSERT(isBufferFinite(outL.data(), 256));
        TEST_ASSERT(isBufferFinite(outR.data(), 256));
    }

    // Cutoff frequency sweep
    for (float cf = 20.0f; cf <= 18000.0f; cf *= 2.0f) {
        filter->setCutoff(cf);
        graph.process(outL.data(), outR.data(), 256);
        TEST_ASSERT(isBufferFinite(outL.data(), 256));
    }

    std::cout << "  [SUCCESS] DaisyFilterNode verified for MoogLadder and SVF modes!\n";
}

// 3. DaisyReverbNode Tests
void testDaisyReverb() {
    std::cout << "[Test 3] Testing DaisyReverbNode (ReverbSc)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<DaisyOscillatorNode>("VerbSource");
    osc->setFrequency(330.0f);
    osc->setAmplitude(0.5f);

    auto verb = std::make_shared<DaisyReverbNode>("TestVerb");
    verb->setFeedback(0.85f);
    verb->setDampingFreq(8000.0f);
    verb->setDryWet(0.5f);

    NodeId oscId = graph.addNode(osc);
    NodeId verbId = graph.addNode(verb);
    graph.connect(oscId, 0, verbId, 0);
    graph.setOutputNode(verbId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));
    TEST_ASSERT(isBufferFinite(outR.data(), 256));

    // Sweep feedback and damping
    verb->setFeedback(0.95f);
    verb->setDampingFreq(2000.0f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    std::cout << "  [SUCCESS] DaisyReverbNode verified!\n";
}

// 4. DaisyDelayNode Tests
void testDaisyDelay() {
    std::cout << "[Test 4] Testing DaisyDelayNode...\n";

    AudioGraph graph;
    auto osc = std::make_shared<DaisyOscillatorNode>("PulseSource");
    osc->setFrequency(200.0f);
    auto delay = std::make_shared<DaisyDelayNode>("TestDelay");
    delay->setDelayTime(0.05f); // 50ms delay
    delay->setFeedback(0.5f);
    delay->setDampingFreq(10000.0f);
    delay->setDryWet(0.5f);

    NodeId oscId = graph.addNode(osc);
    NodeId delId = graph.addNode(delay);
    graph.connect(oscId, 0, delId, 0);
    graph.setOutputNode(delId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));
    TEST_ASSERT(isBufferFinite(outR.data(), 256));

    std::cout << "  [SUCCESS] DaisyDelayNode verified!\n";
}

// 5. DaisyModulationNode Tests
void testDaisyModulation() {
    std::cout << "[Test 5] Testing DaisyModulationNode (Chorus, Flanger, Overdrive)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<DaisyOscillatorNode>("ModOsc");
    auto mod = std::make_shared<DaisyModulationNode>("TestMod");

    NodeId oscId = graph.addNode(osc);
    NodeId modId = graph.addNode(mod);
    graph.connect(oscId, 0, modId, 0);
    graph.setOutputNode(modId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    // Chorus
    mod->setType(DaisyModulationNode::ModType::Chorus);
    mod->setRate(1.5f);
    mod->setDepth(0.6f);
    mod->setDryWet(0.5f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    // Flanger
    mod->setType(DaisyModulationNode::ModType::Flanger);
    mod->setRate(0.8f);
    mod->setDepth(0.7f);
    mod->setFeedback(0.4f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    // Overdrive
    mod->setType(DaisyModulationNode::ModType::Overdrive);
    mod->setDrive(0.8f);
    mod->setDryWet(0.7f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    std::cout << "  [SUCCESS] DaisyModulationNode verified across all modes!\n";
}

// 6. DaisyCompressorNode Tests
void testDaisyCompressor() {
    std::cout << "[Test 6] Testing DaisyCompressorNode...\n";

    AudioGraph graph;
    auto osc = std::make_shared<DaisyOscillatorNode>("LoudOsc");
    osc->setAmplitude(1.5f);
    auto comp = std::make_shared<DaisyCompressorNode>("TestComp");
    comp->setThreshold(-12.0f);
    comp->setRatio(6.0f);
    comp->setAttack(0.005f);
    comp->setRelease(0.050f);
    comp->setMakeupGain(3.0f);

    NodeId oscId = graph.addNode(osc);
    NodeId compId = graph.addNode(comp);
    graph.connect(oscId, 0, compId, 0);
    graph.setOutputNode(compId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));
    TEST_ASSERT(isBufferFinite(outR.data(), 256));

    std::cout << "  [SUCCESS] DaisyCompressorNode verified!\n";
}

// 7. AudioEngine Track FX Integration Tests
void testAudioEngineDaisyFxIntegration() {
    std::cout << "[Test 7] Testing AudioEngine DaisySP FX Rack Integration...\n";

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    engine.initialize(cfg);

    uint32_t tIdx = engine.addTrack("SynthTrack");

    // Build a multi-insert FX chain containing all DaisySP effects
    std::vector<AudioEngine::TrackAudioFxItem> fxChain = {
        { "DaisyMoogLadder", "MOOG_FILTER", 0.4f, 0.7f, true, "fx_moog" },
        { "DaisyStereoChorus", "DAISY_CHORUS", 0.3f, 0.5f, true, "fx_chorus" },
        { "DaisyTapeDelay", "DAISY_DELAY", 0.35f, 0.4f, true, "fx_delay" },
        { "DaisyPlatespace", "DAISY_REVERB", 0.8f, 0.35f, true, "fx_verb" },
        { "DaisyStudioComp", "DAISY_COMPRESSOR", 0.5f, 1.0f, true, "fx_comp" }
    };

    bool rebuildOk = engine.rebuildTrackAudioFx(tIdx, fxChain);
    TEST_ASSERT(rebuildOk);

    // Test parameter updates
    bool p1 = engine.setTrackAudioFxParam(tIdx, 0, "cutoff", 1500.0f);
    bool p2 = engine.setTrackAudioFxParam(tIdx, 0, "resonance", 0.6f);
    bool p3 = engine.setTrackAudioFxParam(tIdx, 1, "rate", 1.8f);
    bool p4 = engine.setTrackAudioFxParam(tIdx, 2, "time", 0.25f);
    bool p5 = engine.setTrackAudioFxParam(tIdx, 3, "decay", 0.90f);
    bool p6 = engine.setTrackAudioFxParam(tIdx, 4, "threshold", -15.0f);

    TEST_ASSERT(p1 && p2 && p3 && p4 && p5 && p6);

    // Process a block
    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);
    engine.getGraph().process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));
    TEST_ASSERT(isBufferFinite(outR.data(), 256));

    std::cout << "  [SUCCESS] AudioEngine DaisySP FX rack rebuild & parameter dispatch verified!\n";
}

int main() {
    std::cout << "=== Running DaisySP Modular DSP & FX Tests ===\n";
    testDaisyOscillator();
    testDaisyFilter();
    testDaisyReverb();
    testDaisyDelay();
    testDaisyModulation();
    testDaisyCompressor();
    testAudioEngineDaisyFxIntegration();
    std::cout << "=== All DaisySP Tests Passed Cleanly (100%) ===\n";
    return 0;
}
