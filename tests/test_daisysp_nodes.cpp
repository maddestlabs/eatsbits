#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <memory>

#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/modular_nodes.hpp"
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

// 1. OscillatorNode Tests (PolyBLEP bandlimited oscillator)
void testOscillator() {
    std::cout << "[Test 1] Testing canonical OscillatorNode & DaisyOscillator alias...\n";

    AudioGraph graph;
    auto osc = std::make_shared<OscillatorNode>("TestOsc");
    NodeId oscId = graph.addNode(osc);
    graph.setOutputNode(oscId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    // Test each waveform mode
    for (int wf = 0; wf <= 8; ++wf) {
        osc->setWaveform(static_cast<OscillatorNode::Waveform>(wf));
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

    // Verify DaisyOscillatorNode alias type equality
    static_assert(std::is_same_v<OscillatorNode, DaisyOscillatorNode>, "DaisyOscillatorNode must alias OscillatorNode");

    std::cout << "  [SUCCESS] OscillatorNode verified across waveforms and parameter sweeps!\n";
}

// 2. MoogFilterNode Tests (MoogLadder & SVF)
void testMoogFilter() {
    std::cout << "[Test 2] Testing canonical MoogFilterNode (MoogLadder & SVF)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<OscillatorNode>("SourceOsc");
    osc->setWaveform(OscillatorNode::Waveform::PolyBlepSaw);
    osc->setFrequency(120.0f);
    osc->setAmplitude(0.8f);

    auto filter = std::make_shared<MoogFilterNode>("TestFilter");
    NodeId oscId = graph.addNode(osc);
    NodeId fltId = graph.addNode(filter);

    graph.connect(oscId, 0, fltId, 0); // L/R from osc port 0 to filter port 0
    graph.setOutputNode(fltId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    // Test MoogLadder
    filter->setMode(MoogFilterNode::FilterMode::MoogLadder);
    filter->setCutoff(500.0f);
    filter->setResonance(0.7f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));
    TEST_ASSERT(isBufferFinite(outR.data(), 256));

    // Test SVF Modes: Low, High, Band, Notch
    MoogFilterNode::FilterMode svfModes[] = {
        MoogFilterNode::FilterMode::SvfLowpass,
        MoogFilterNode::FilterMode::SvfHighpass,
        MoogFilterNode::FilterMode::SvfBandpass,
        MoogFilterNode::FilterMode::SvfNotch
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

    // Verify DaisyFilterNode and FilterNode aliases
    static_assert(std::is_same_v<MoogFilterNode, DaisyFilterNode>, "DaisyFilterNode must alias MoogFilterNode");
    static_assert(std::is_same_v<MoogFilterNode, FilterNode>, "FilterNode must alias MoogFilterNode");

    std::cout << "  [SUCCESS] MoogFilterNode verified for MoogLadder and SVF modes!\n";
}

// 3. ReverbNode Tests (ReverbSc)
void testReverb() {
    std::cout << "[Test 3] Testing canonical ReverbNode (ReverbSc algorithmic reverb)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<OscillatorNode>("VerbSource");
    osc->setFrequency(330.0f);
    osc->setAmplitude(0.5f);

    auto verb = std::make_shared<ReverbNode>("TestVerb");
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

    // Verify DaisyReverbNode alias
    static_assert(std::is_same_v<ReverbNode, DaisyReverbNode>, "DaisyReverbNode must alias ReverbNode");

    std::cout << "  [SUCCESS] ReverbNode verified!\n";
}

// 4. DelayNode Tests
void testDelay() {
    std::cout << "[Test 4] Testing canonical DelayNode (DaisySP DelayLine + Tone damping)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<OscillatorNode>("PulseSource");
    osc->setFrequency(200.0f);
    auto delay = std::make_shared<DelayNode>("TestDelay");
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

    // Test millisecond interface
    delay->setDelayTimeMs(120.0f);
    TEST_ASSERT(std::abs(delay->getDelayTimeMs() - 120.0f) < 0.1f);

    // Verify DaisyDelayNode alias
    static_assert(std::is_same_v<DelayNode, DaisyDelayNode>, "DaisyDelayNode must alias DelayNode");

    std::cout << "  [SUCCESS] DelayNode verified!\n";
}

// 5. ChorusNode Tests (Chorus, Flanger, Overdrive)
void testChorus() {
    std::cout << "[Test 5] Testing canonical ChorusNode (Chorus, Flanger, Overdrive)...\n";

    AudioGraph graph;
    auto osc = std::make_shared<OscillatorNode>("ModOsc");
    auto mod = std::make_shared<ChorusNode>("TestMod");

    NodeId oscId = graph.addNode(osc);
    NodeId modId = graph.addNode(mod);
    graph.connect(oscId, 0, modId, 0);
    graph.setOutputNode(modId, 0);
    graph.compile();
    graph.prepare(48000.0, 256);

    std::vector<float> outL(256, 0.0f);
    std::vector<float> outR(256, 0.0f);

    // Chorus
    mod->setType(ChorusNode::ModType::Chorus);
    mod->setRate(1.5f);
    mod->setDepth(0.6f);
    mod->setDryWet(0.5f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    // Flanger
    mod->setType(ChorusNode::ModType::Flanger);
    mod->setRate(0.8f);
    mod->setDepth(0.7f);
    mod->setFeedback(0.4f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    // Overdrive
    mod->setType(ChorusNode::ModType::Overdrive);
    mod->setDrive(0.8f);
    mod->setDryWet(0.7f);
    graph.process(outL.data(), outR.data(), 256);
    TEST_ASSERT(isBufferFinite(outL.data(), 256));

    // Backward compatibility methods
    mod->setRateHz(2.0f);
    TEST_ASSERT(std::abs(mod->getRateHz() - 2.0f) < 0.01f);
    mod->setDepthMs(3.0f);
    TEST_ASSERT(mod->getDepthMs() > 0.0f);

    // Verify DaisyModulationNode alias
    static_assert(std::is_same_v<ChorusNode, DaisyModulationNode>, "DaisyModulationNode must alias ChorusNode");

    std::cout << "  [SUCCESS] ChorusNode verified across all modes!\n";
}

// 6. CompressorNode Tests
void testCompressor() {
    std::cout << "[Test 6] Testing canonical CompressorNode...\n";

    AudioGraph graph;
    auto osc = std::make_shared<OscillatorNode>("LoudOsc");
    osc->setAmplitude(1.5f);
    auto comp = std::make_shared<CompressorNode>("TestComp");
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

    // Verify DaisyCompressorNode alias
    static_assert(std::is_same_v<CompressorNode, DaisyCompressorNode>, "DaisyCompressorNode must alias CompressorNode");

    std::cout << "  [SUCCESS] CompressorNode verified!\n";
}

// 7. AudioEngine Track FX Integration Tests (Prefix-Free & Legacy Brand Aliases)
void testAudioEngineModularFxIntegration() {
    std::cout << "[Test 7] Testing AudioEngine Prefix-Free Modular FX Rack Integration...\n";

    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    engine.initialize(cfg);

    uint32_t tIdx = engine.addTrack("SynthTrack");

    // Build a multi-insert FX chain containing canonical prefix-free effects
    std::vector<AudioEngine::TrackAudioFxItem> fxChain = {
        { "MoogLadder", "MOOG_FILTER", 0.4f, 0.7f, true, "fx_moog" },
        { "StereoChorus", "CHORUS", 0.3f, 0.5f, true, "fx_chorus" },
        { "TapeDelay", "DELAY", 0.35f, 0.4f, true, "fx_delay" },
        { "Platespace", "REVERB", 0.8f, 0.35f, true, "fx_verb" },
        { "StudioComp", "COMPRESSOR", 0.5f, 1.0f, true, "fx_comp" }
    };

    bool rebuildOk = engine.rebuildTrackAudioFx(tIdx, fxChain);
    TEST_ASSERT(rebuildOk);

    // Test parameter updates with prefix-free names
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

    // Also test rebuild with legacy DAISY_* prefixed types
    std::vector<AudioEngine::TrackAudioFxItem> legacyChain = {
        { "DaisyMoogLadder", "DAISY_FILTER", 0.4f, 0.7f, true, "fx_moog" },
        { "DaisyStereoChorus", "DAISY_CHORUS", 0.3f, 0.5f, true, "fx_chorus" },
        { "DaisyTapeDelay", "DAISY_DELAY", 0.35f, 0.4f, true, "fx_delay" },
        { "DaisyPlatespace", "DAISY_REVERB", 0.8f, 0.35f, true, "fx_verb" },
        { "DaisyStudioComp", "DAISY_COMPRESSOR", 0.5f, 1.0f, true, "fx_comp" }
    };
    bool legacyOk = engine.rebuildTrackAudioFx(tIdx, legacyChain);
    TEST_ASSERT(legacyOk);

    std::cout << "  [SUCCESS] AudioEngine modular FX rack rebuild & parameter dispatch verified for both prefix-free & legacy names!\n";
}

int main() {
    std::cout << "=== Running Modular DSP & FX Tests (Prefix-Free & DaisySP) ===\n";
    testOscillator();
    testMoogFilter();
    testReverb();
    testDelay();
    testChorus();
    testCompressor();
    testAudioEngineModularFxIntegration();
    std::cout << "=== All Modular DSP & FX Tests Passed Cleanly (100%) ===\n";
    return 0;
}
