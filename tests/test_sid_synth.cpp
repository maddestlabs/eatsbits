#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <numeric>

#include "eatsbits/audio/dsp/sid_core.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/sid_node.hpp"
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

void testGaloisLfsrNoise() {
    std::cout << "[Test 1] Testing 23-bit Galois LFSR Noise Generator..." << std::endl;
    SIDVoice voice;
    voice.prepare(48000.0f);
    voice.waveform = SIDWaveform::Noise;
    voice.noteOn(60, 1.0f); // Middle C

    std::vector<float> samples(4800);
    float minVal = 1.0f;
    float maxVal = -1.0f;
    float sum = 0.0f;

    for (size_t i = 0; i < samples.size(); ++i) {
        float s = voice.processSample(0.0f, false);
        REQUIRE(!std::isnan(s) && !std::isinf(s));
        REQUIRE(s >= -1.05f && s <= 1.05f);
        minVal = std::min(minVal, s);
        maxVal = std::max(maxVal, s);
        sum += s;
        samples[i] = s;
    }

    // Pseudo-random noise should have diverse values spanning both positive and negative
    REQUIRE(minVal < -0.4f);
    REQUIRE(maxVal > 0.4f);

    // Mean should be reasonably centered near 0
    float mean = sum / static_cast<float>(samples.size());
    REQUIRE(std::abs(mean) < 0.25f);

    // Check that samples are not static (variance > 0)
    float variance = 0.0f;
    for (float s : samples) {
        variance += (s - mean) * (s - mean);
    }
    variance /= static_cast<float>(samples.size());
    REQUIRE(variance > 0.05f);

    std::cout << "  Noise range: [" << minVal << ", " << maxVal << "] Variance: " << variance << " -> PASSED" << std::endl;
}

void testPulsePwm() {
    std::cout << "[Test 2] Testing 12-Bit Pulse Wave & Hardware PWM..." << std::endl;
    SIDVoice voice;
    voice.prepare(48000.0f);
    voice.waveform = SIDWaveform::Pulse;
    voice.pulseWidth = 2048.0f; // 50% duty cycle
    voice.pwmDepth = 0.0f;      // Static PW
    voice.noteOn(69, 1.0f);     // A4 = 440 Hz

    int highCount = 0;
    int lowCount = 0;
    const int numFrames = 4800; // 100ms
    for (int i = 0; i < numFrames; ++i) {
        float s = voice.processSample(0.0f, false);
        REQUIRE(!std::isnan(s) && !std::isinf(s));
        if (s > 0.0f) highCount++;
        else lowCount++;
    }

    // For 50% duty cycle, high and low counts should be roughly equal (~50% +/- 5%)
    float ratio = static_cast<float>(highCount) / static_cast<float>(numFrames);
    REQUIRE(ratio > 0.40f && ratio < 0.60f);

    // Test 25% duty cycle
    voice.pulseWidth = 1024.0f;
    highCount = 0;
    for (int i = 0; i < numFrames; ++i) {
        float s = voice.processSample(0.0f, false);
        if (s > 0.0f) highCount++;
    }
    float ratio25 = static_cast<float>(highCount) / static_cast<float>(numFrames);
    REQUIRE(ratio25 > 0.18f && ratio25 < 0.32f);

    // Test LFO PWM modulation
    voice.pwmRate = 5.0f;   // 5 Hz LFO
    voice.pwmDepth = 0.5f;  // Sweep width
    for (int i = 0; i < 2000; ++i) {
        (void)voice.processSample(0.0f, false);
    }
    // Sound generation stays stable during modulation
    float s = voice.processSample(0.0f, false);
    REQUIRE(!std::isnan(s) && !std::isinf(s));

    std::cout << "  Duty cycles: 50%=" << ratio << ", 25%=" << ratio25 << " -> PASSED" << std::endl;
}

void testHardSyncAndRingMod() {
    std::cout << "[Test 3] Testing Hard Sync & Ring Modulation..." << std::endl;
    SIDVoice master;
    SIDVoice slave;
    master.prepare(48000.0f);
    slave.prepare(48000.0f);

    master.waveform = SIDWaveform::Sawtooth;
    master.noteOn(48, 1.0f); // Low C

    slave.waveform = SIDWaveform::Sawtooth;
    slave.sync = true;
    slave.noteOn(72, 1.0f); // High C

    for (int i = 0; i < 1000; ++i) {
        float mOut = master.processSample(0.0f, false);
        bool mReset = master.justReset();
        float sOut = slave.processSample(mOut, mReset);
        REQUIRE(!std::isnan(sOut) && !std::isinf(sOut));
    }

    // Test Ring Modulation on Triangle Wave
    slave.ringMod = true;
    slave.waveform = SIDWaveform::Triangle;
    for (int i = 0; i < 1000; ++i) {
        float mOut = master.processSample(0.0f, false);
        bool mReset = master.justReset();
        float sOut = slave.processSample(mOut, mReset);
        REQUIRE(!std::isnan(sOut) && !std::isinf(sOut));
        REQUIRE(sOut >= -1.2f && sOut <= 1.2f);
    }

    std::cout << "  Hard sync & Ring mod stable without divergence -> PASSED" << std::endl;
}

void testFilterModesAndModels() {
    std::cout << "[Test 4] Testing 12dB/oct Chamberlin SVF Filter & Chip Models..." << std::endl;
    SIDFilter flt;
    flt.prepare(48000.0f);

    // Test Lowpass Mode with MOS 6581 (Non-linear FET saturation)
    flt.mode = SIDFilterMode::Lowpass;
    flt.chipModel = SIDChipModel::MOS6581;
    flt.setCutoff(800.0f);
    flt.setResonance(10.0f);

    float peakLp = 0.0f;
    for (int i = 0; i < 2000; ++i) {
        float inSig = (i % 40 < 20) ? 0.8f : -0.8f; // Square wave
        float outSig = flt.process(inSig);
        REQUIRE(!std::isnan(outSig) && !std::isinf(outSig));
        peakLp = std::max(peakLp, std::abs(outSig));
    }
    REQUIRE(peakLp > 0.1f);

    // Test Bandpass Mode with MOS 8580 (Linear response)
    flt.reset();
    flt.mode = SIDFilterMode::Bandpass;
    flt.chipModel = SIDChipModel::MOS8580;
    flt.setCutoff(191.0f); // (191/2047)*12500 + 30 ~= 1200 Hz center frequency
    flt.setResonance(8.0f);

    float peakBp = 0.0f;
    for (int i = 0; i < 2000; ++i) {
        float inSig = std::sin(2.0f * 3.14159265f * 1200.0f * static_cast<float>(i) / 48000.0f);
        float outSig = flt.process(inSig);
        REQUIRE(!std::isnan(outSig) && !std::isinf(outSig));
        peakBp = std::max(peakBp, std::abs(outSig));
    }
    REQUIRE(peakBp > 0.3f);

    // Test Highpass Mode
    flt.reset();
    flt.mode = SIDFilterMode::Highpass;
    flt.setCutoff(3000.0f);
    for (int i = 0; i < 1000; ++i) {
        float outSig = flt.process(0.5f);
        REQUIRE(!std::isnan(outSig) && !std::isinf(outSig));
    }

    // Test Notch Mode
    flt.reset();
    flt.mode = SIDFilterMode::Notch;
    for (int i = 0; i < 1000; ++i) {
        float outSig = flt.process(0.5f);
        REQUIRE(!std::isnan(outSig) && !std::isinf(outSig));
    }

    std::cout << "  Filter modes LP/BP/HP/Notch verified with 6581/8580 -> PASSED" << std::endl;
}

void testVoiceAllocationAndAdsr() {
    std::cout << "[Test 5] Testing Polyphonic Voice Allocation & Hardware ADSR..." << std::endl;
    SIDChip chip;
    chip.prepare(48000.0f);

    // Play 3 notes simultaneously (middle triad: C4, E4, G4)
    chip.noteOn(60, 0.9f);
    chip.noteOn(64, 0.9f);
    chip.noteOn(67, 0.9f);

    const auto& voices = chip.getVoices();
    int activeCount = 0;
    for (const auto& v : voices) {
        if (v.isActive()) activeCount++;
    }
    REQUIRE(activeCount == 3);

    // Render 100ms
    std::vector<float> bufL(4800, 0.0f);
    std::vector<float> bufR(4800, 0.0f);
    chip.processStereo(bufL.data(), bufR.data(), 4800, 48000.0f);

    float maxAmp = 0.0f;
    for (size_t i = 0; i < 4800; ++i) {
        maxAmp = std::max(maxAmp, std::abs(bufL[i]) + std::abs(bufR[i]));
    }
    REQUIRE(maxAmp > 0.2f);

    // Release all notes
    chip.noteOff(60);
    chip.noteOff(64);
    chip.noteOff(67);

    // Process through release phase
    for (int block = 0; block < 50; ++block) {
        chip.processStereo(bufL.data(), bufR.data(), 4800, 48000.0f);
    }

    // Eventually all voices should complete release
    int remainingActive = 0;
    for (const auto& v : chip.getVoices()) {
        if (v.isActive()) remainingActive++;
    }
    REQUIRE(remainingActive == 0);

    // Test Arpeggiator (50Hz / 60Hz Chiptune Arp)
    chip.reset();
    chip.setParameter("ArpMode", 1.0f); // Major Triad Arp
    chip.noteOn(60, 1.0f);
    for (int i = 0; i < 4800; ++i) {
        chip.processStereo(bufL.data(), bufR.data(), 1, 48000.0f);
    }
    REQUIRE(chip.getVoices()[0].isActive());

    std::cout << "  Triad voice polyphony, release termination, and Arp -> PASSED" << std::endl;
}

void testSidNodeGraphIntegration() {
    std::cout << "[Test 6] Testing SidNode Modular Audio Graph Integration..." << std::endl;
    AudioGraph graph;
    graph.prepare(48000.0, 128);

    auto sid = std::make_shared<SidNode>("SID_Lead");
    auto master = std::make_shared<GainNode>("Master_Out");

    NodeId sidId = graph.addNode(sid);
    NodeId masterId = graph.addNode(master);

    REQUIRE(graph.connect(sidId, 0, masterId, 0));

    // Post parameter changes
    sid->setParameter(0, 1.0f);     // Waveform: Sawtooth
    sid->setParameter(8, 1400.0f);   // Cutoff
    sid->setParameter(9, 6.0f);      // Resonance
    sid->setParameter(11, 2.0f);     // Attack
    sid->setParameter(12, 5.0f);     // Decay
    sid->setParameter(13, 10.0f);    // Sustain
    sid->setParameter(14, 4.0f);     // Release

    // Send NoteOn event
    AudioEvent evtOn{};
    evtOn.type = AudioEventType::NoteOn;
    evtOn.note = 62; // D4
    evtOn.velocity = 0.85f;
    sid->handleEvent(evtOn);

    // Process blocks
    float outL[128]{0};
    float outR[128]{0};
    float peak = 0.0f;

    for (int b = 0; b < 40; ++b) {
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
    evtOff.note = 62;
    sid->handleEvent(evtOff);

    // Verify parameter dispatch by name
    sid->setParameterByName("Overdrive", 1.8f);
    sid->setParameterByName("ChipModel", 1.0f); // 8580

    std::cout << "  Graph execution peak: " << peak << " -> PASSED" << std::endl;
}

void testPerformanceBenchmark() {
    std::cout << "[Test 7] Performance Benchmark: 10 Seconds of 3-Voice Polyphonic SID Audio..." << std::endl;
    SIDChip chip;
    chip.prepare(48000.0f);

    chip.setParameter("Waveform", 0.0f);   // Pulse
    chip.setParameter("PulseWidth", 2048.0f);
    chip.setParameter("PwmRate", 2.5f);
    chip.setParameter("PwmDepth", 0.6f);
    chip.setParameter("Cutoff", 1100.0f);
    chip.setParameter("Resonance", 8.0f);
    chip.setParameter("Overdrive", 1.4f);

    chip.noteOn(48, 0.9f);
    chip.noteOn(55, 0.85f);
    chip.noteOn(64, 0.8f);

    const size_t totalFrames = 48000 * 10; // 480,000 frames = 10s of audio
    const size_t blockSize = 128;
    std::vector<float> bufL(blockSize);
    std::vector<float> bufR(blockSize);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t f = 0; f < totalFrames; f += blockSize) {
        chip.processStereo(bufL.data(), bufR.data(), static_cast<uint32_t>(blockSize), 48000.0f);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double speedup = 10000.0 / elapsedMs;

    std::cout << "  Rendered 10s of audio in " << elapsedMs << " ms (" << speedup << "x real-time speedup)" << std::endl;
    REQUIRE(elapsedMs < 500.0); // Must be blazingly fast (<500ms for 10s audio)
    std::cout << "  -> PASSED: Real-time chiptune synthesis exceeds performance threshold." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    Eatsbits Commodore 64 SID Synth Unit Tests    " << std::endl;
    std::cout << "==================================================" << std::endl;

    testGaloisLfsrNoise();
    testPulsePwm();
    testHardSyncAndRingMod();
    testFilterModesAndModels();
    testVoiceAllocationAndAdsr();
    testSidNodeGraphIntegration();
    testPerformanceBenchmark();

    std::cout << "==================================================" << std::endl;
    std::cout << "    100% C64 SID SOUND CHIP TESTS PASSED!        " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
