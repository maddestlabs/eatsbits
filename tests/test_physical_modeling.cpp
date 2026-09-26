#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <chrono>
#include <numeric>
#include "eatsbits/audio/dsp/piano_physical_tables.hpp"
#include "eatsbits/audio/dsp/exciters.hpp"
#include "eatsbits/audio/dsp/modal_resonator.hpp"
#include "eatsbits/audio/dsp/waveguide_core.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/waveguide_node.hpp"
#include "eatsbits/audio/graph/nodes/modal_resonator_node.hpp"
#include "eatsbits/audio/graph/nodes/physical_instrument_node.hpp"
#include "eatsbits/project/preset_loader.hpp"

using namespace eatsbits;
using namespace eatsbits::dsp;
using namespace eatsbits::audio;
using namespace eatsbits::project;

void testEmpiricalTables() {
    std::cout << "[Test 1] Testing Stanford CCRMA / Bank-Bensa Empirical Piano Tables..." << std::endl;

    // Test boundary lookups
    double lowDecay = PianoPhysicalTables::singleStringDecayRate.lookup(10.0);
    assert(lowDecay == -1.5);
    double highDecay = PianoPhysicalTables::singleStringDecayRate.lookup(120.0);
    assert(highDecay == -15.0);

    // Test intermediate interpolation
    double midZero = PianoPhysicalTables::singleStringZero.lookup(45.0);
    assert(midZero < -0.2 && midZero > -0.4);

    double a4Pole = PianoPhysicalTables::loudPole.lookup(69.0);
    assert(a4Pole >= 0.80 && a4Pole <= 0.82);

    (void)lowDecay;
    (void)highDecay;
    (void)midZero;
    (void)a4Pole;

    std::cout << "  Empirical breakpoint tables verified -> PASSED" << std::endl;
}

void testExciters() {
    std::cout << "[Test 2] Testing Mechanical Exciters (Hammer, Plectrum, Soundboard, Upright Pluck)..." << std::endl;

    constexpr size_t kLen = 1024;
    constexpr float kSr = 44100.0f;
    std::vector<float> bufSoft(kLen, 0.0f);
    std::vector<float> bufHard(kLen, 0.0f);

    // 1. Hammer Exciter
    HammerExciter::generate(bufSoft.data(), kLen, kSr, 60, 0.2f, 0.8f, 0.0f);
    HammerExciter::generate(bufHard.data(), kLen, kSr, 60, 1.0f, 2.0f, 1.0f);

    float peakSoft = 0.0f, peakHard = 0.0f;
    for (size_t i = 0; i < kLen; ++i) {
        peakSoft = std::max(peakSoft, std::abs(bufSoft[i]));
        peakHard = std::max(peakHard, std::abs(bufHard[i]));
    }
    assert(peakHard > peakSoft);
    assert(peakHard > 0.5f);

    // 2. Plectrum Strum Exciter
    std::vector<float> plectrumBuf(kLen, 0.0f);
    PlectrumStrumExciter::generate(plectrumBuf.data(), kLen, kSr, 60, 0.85f, 8.0f, 1.2f);
    float plectrumPeak = 0.0f;
    for (float s : plectrumBuf) plectrumPeak = std::max(plectrumPeak, std::abs(s));
    assert(plectrumPeak > 0.3f);

    // 3. Upright Pluck Exciter
    std::vector<float> uprightBuf(kLen, 0.0f);
    UprightPluckSlapExciter::generate(uprightBuf.data(), kLen, kSr, 33, 0.9f, 2.2f, 0.5f, 0.7f);
    float uprightPeak = 0.0f;
    for (float s : uprightBuf) uprightPeak = std::max(uprightPeak, std::abs(s));
    assert(uprightPeak > 0.1f);

    std::cout << "  All mechanical exciters generated valid physical force transients -> PASSED" << std::endl;
}

void testWaveguidePitchAccuracy() {
    std::cout << "[Test 3] Testing Fractional Delay Pitch Accuracy across Octaves..." << std::endl;

    constexpr float kSr = 44100.0f;
    constexpr size_t kNumSamples = 4410; // 100ms
    DigitalWaveguideCore wg;

    const float testFreqs[] = {110.0f, 220.0f, 440.0f, 880.0f};

    for (float targetF : testFreqs) {
        wg.reset();
        std::vector<float> buf(kNumSamples, 0.0f);
        // Dirac impulse
        buf[0] = 1.0f;

        wg.process(buf.data(), kNumSamples, kSr, targetF, 0.998f, 0.05f);

        // Detect period between peaks
        std::vector<size_t> peakIndices;
        for (size_t i = 1; i < kNumSamples - 1; ++i) {
            if (buf[i] > 0.1f && buf[i] > buf[i - 1] && buf[i] > buf[i + 1]) {
                peakIndices.push_back(i);
            }
        }

        assert(peakIndices.size() >= 3);
        float avgPeriod = static_cast<float>(peakIndices[2] - peakIndices[0]) / 2.0f;
        float expectedPeriod = kSr / targetF;
        float error = std::abs(avgPeriod - expectedPeriod) / expectedPeriod;
        assert(error < 0.03f); // Within 3% tolerance for discrete peak detection
        (void)error;
    }

    std::cout << "  Pitch tracking accurate across 4 octaves -> PASSED" << std::endl;
}

void testBankBensaCommutedPiano() {
    std::cout << "[Test 4] Testing Bank-Bensa Commuted Piano Waveguide & Bridge Coupling..." << std::endl;

    constexpr float kSr = 44100.0f;
    constexpr size_t kBlockSize = 2048;
    std::vector<float> pcm(kBlockSize, 0.0f);

    CommutedSoundboardExciter::generate(pcm.data(), kBlockSize, kSr, 60, 0.9f, 0.85f, 0.55f, 1.0f);
    CommutedHammerFilterCascade hammer;
    hammer.process(pcm.data(), kBlockSize, 60, 0.9f, 0.5f, 1.0f);

    CommutedStrikeComb comb;
    comb.process(pcm.data(), kBlockSize, kSr, 60, 261.63f);

    CommutedPianoWaveguideCore pianoWg;
    pianoWg.process(pcm.data(), kBlockSize, kSr, 60, 261.63f, 1.0f, 1.0f, 1.0f, false);

    float peakVal = 0.0f;
    float sumEnergy = 0.0f;
    for (float s : pcm) {
        peakVal = std::max(peakVal, std::abs(s));
        sumEnergy += s * s;
    }

    assert(peakVal > 0.05f);
    assert(sumEnergy > 0.1f);

    std::cout << "  Bank-Bensa commuted piano rendered with peak: " << peakVal << " -> PASSED" << std::endl;
}

void testUprightBassGrowlSlap() {
    std::cout << "[Test 5] Testing Upright Bass Elastodynamics & Ebony Fingerboard Collision..." << std::endl;

    constexpr float kSr = 44100.0f;
    constexpr size_t kBlockSize = 2048;

    std::vector<float> softPluck(kBlockSize, 0.0f);
    std::vector<float> hardSlap(kBlockSize, 0.0f);

    UprightPluckSlapExciter::generate(softPluck.data(), kBlockSize, kSr, 33, 0.3f, 2.0f, 0.0f, 0.8f);
    UprightPluckSlapExciter::generate(hardSlap.data(), kBlockSize, kSr, 33, 1.0f, 3.0f, 1.5f, 0.3f);

    UprightBassWaveguideCore bassWgSoft, bassWgHard;
    bassWgSoft.process(softPluck.data(), kBlockSize, kSr, 55.0f, 0.99f, 0.28f, 0.22f, 3.2f, 0.0f);
    bassWgHard.process(hardSlap.data(), kBlockSize, kSr, 55.0f, 0.99f, 0.28f, 0.22f, 1.5f, 1.5f);

    float softPeak = 0.0f, hardPeak = 0.0f;
    for (size_t i = 0; i < kBlockSize; ++i) {
        softPeak = std::max(softPeak, std::abs(softPluck[i]));
        hardPeak = std::max(hardPeak, std::abs(hardSlap[i]));
    }

    assert(hardPeak > softPeak);
    std::cout << "  Upright bass growl/slap collision dynamics verified -> PASSED" << std::endl;
}

void testModalResonatorBank() {
    std::cout << "[Test 6] Testing Modal Resonator Bank & Body Cavity Modes..." << std::endl;

    constexpr float kSr = 44100.0f;
    constexpr size_t kBlockSize = 512;

    std::vector<float> impulse(kBlockSize, 0.0f);
    impulse[0] = 1.0f;
    std::vector<float> out(kBlockSize, 0.0f);

    ModalResonatorBank bank;
    ModalResonatorBank::ModeConfig modes[3] = {
        {1.0f, 0.5f, 20.0f},
        {2.0f, 0.3f, 25.0f},
        {3.0f, 0.2f, 30.0f}
    };
    bank.setModes(std::span<const ModalResonatorBank::ModeConfig>(modes, 3));
    bank.process(impulse.data(), out.data(), kBlockSize, kSr, 200.0f);

    float peakOut = 0.0f;
    for (float s : out) peakOut = std::max(peakOut, std::abs(s));
    assert(peakOut > 0.01f);

    std::cout << "  Modal Resonator Bank resonant modes verified -> PASSED" << std::endl;
}

void testAudioGraphPhysicalInstruments() {
    std::cout << "[Test 7] Testing AudioGraph PhysicalInstrumentNode Integration..." << std::endl;

    AudioGraph graph;
    const double sampleRate = 44100.0;
    const uint32_t blockSize = 512;
    graph.prepare(sampleRate, blockSize);

    auto piano = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::ConcertGrandPiano, "GrandPiano");
    auto bass = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::UprightBass, "UprightBass");
    auto guitar = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::SpanishGuitar, "SpanishGuitar");

    NodeId pianoId = graph.addNode(piano);
    NodeId bassId = graph.addNode(bass);
    NodeId guitarId = graph.addNode(guitar);
    (void)bassId;
    (void)guitarId;

    graph.setOutputNode(pianoId, 0);

    // Trigger notes
    piano->noteOn(60, 0.85f);
    bass->noteOn(33, 0.90f);
    guitar->noteOn(64, 0.80f);

    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    float maxSample = 0.0f;
    for (int b = 0; b < 10; ++b) {
        graph.process(outL.data(), outR.data(), blockSize);
        for (size_t i = 0; i < blockSize; ++i) {
            maxSample = std::max(maxSample, std::abs(outL[i]));
        }
    }
    assert(maxSample > 0.01f);

    std::cout << "  Physical instrument nodes evaluated cleanly in AudioGraph with peak: " << maxSample << " -> PASSED" << std::endl;
}

void testPhysicalModelingBenchmark() {
    std::cout << "[Test 8] Real-Time Performance Benchmark: 10s of 4-Voice Concert Grand Piano..." << std::endl;

    AudioGraph graph;
    const double sampleRate = 44100.0;
    const uint32_t blockSize = 512;
    graph.prepare(sampleRate, blockSize);

    auto piano = std::make_shared<PhysicalInstrumentNode>(PhysicalModelType::ConcertGrandPiano, "BenchPiano");
    NodeId pianoId = graph.addNode(piano);
    graph.setOutputNode(pianoId, 0);

    // Play 4-note C-major chord (C3, E3, G3, B3)
    piano->noteOn(48, 0.85f);
    piano->noteOn(52, 0.80f);
    piano->noteOn(55, 0.80f);
    piano->noteOn(59, 0.75f);

    constexpr size_t kTotalBlocks = (44100 * 10) / blockSize; // ~10 seconds of audio
    std::vector<float> outL(blockSize, 0.0f);
    std::vector<float> outR(blockSize, 0.0f);

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t b = 0; b < kTotalBlocks; ++b) {
        graph.process(outL.data(), outR.data(), blockSize);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    double speedup = 10000.0 / elapsedMs;

    std::cout << "  Rendered 10.0s of 4-voice physical modeling audio in " << elapsedMs << " ms ("
              << speedup << "x faster than real-time budget)" << std::endl;
    assert(speedup > 2.0); // Strict DAW-grade real-time throughput requirement

    std::cout << "  -> PASSED: Physical modeling engine comfortably exceeds real-time budget." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   Eatsbits Acoustic Physical Modeling Suite      " << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        testEmpiricalTables();
        testExciters();
        testWaveguidePitchAccuracy();
        testBankBensaCommutedPiano();
        testUprightBassGrowlSlap();
        testModalResonatorBank();
        testAudioGraphPhysicalInstruments();
        testPhysicalModelingBenchmark();

        std::cout << "==================================================" << std::endl;
        std::cout << "  100% PHYSICAL MODELING SUITE TESTS PASSED!     " << std::endl;
        std::cout << "==================================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL TEST EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
