#include <iostream>
#include <chrono>
#include <vector>
#include <cassert>
#include <thread>
#include "eatsbits/audio/ringbuffer.hpp"
#include "eatsbits/audio/dsp/biquad.hpp"
#include "eatsbits/audio/dsp/oscillator.hpp"
#include "eatsbits/audio/dsp/adsr.hpp"
#include "eatsbits/audio/dsp/poly_synth.hpp"
#include "eatsbits/audio/dsp/tb303_core.hpp"
#include "eatsbits/audio/audio_engine.hpp"

using namespace eatsbits;
using namespace eatsbits::dsp;
using namespace eatsbits::audio;

void testRingBufferConcurrency() {
    std::cout << "[Test] Running SPSC lock-free ringbuffer multi-threaded concurrency test..." << std::endl;
    SpscRingBuffer<AudioEvent, 1024> queue;
    constexpr size_t NUM_EVENTS = 50000;

    std::atomic<bool> producerDone{false};
    size_t receivedCount = 0;

    // Producer Thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_EVENTS; ++i) {
            AudioEvent evt;
            evt.type = AudioEventType::NoteOn;
            evt.note = static_cast<uint8_t>(i % 128);
            evt.velocity = 0.8f;
            while (!queue.push(evt)) {
                std::this_thread::yield();
            }
        }
        producerDone = true;
    });

    // Consumer Thread
    std::thread consumer([&]() {
        AudioEvent evt;
        while (!producerDone || !queue.empty()) {
            if (queue.pop(evt)) {
                receivedCount++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    assert(receivedCount == NUM_EVENTS);
    std::cout << "  -> Passed: " << receivedCount << " events delivered wait-free and loss-free." << std::endl;
}

void testDspBiquad() {
    std::cout << "[Test] Testing Biquad lowpass filter..." << std::endl;
    BiquadFilter filter;
    filter.configure(BiquadType::LowPass, 1000.0f, 0.707f, 0.0f, 48000.0f);

    float in[128];
    float out[128];
    for (int i = 0; i < 128; ++i) in[i] = (i % 2 == 0) ? 1.0f : -1.0f; // High frequency input

    filter.process_block(in, out, 128);

    // High frequencies should be heavily attenuated
    float maxAmp = 0.0f;
    for (int i = 32; i < 128; ++i) {
        maxAmp = std::max(maxAmp, std::abs(out[i]));
    }
    assert(maxAmp < 0.2f);
    std::cout << "  -> Passed: 24kHz Nyquist pulse attenuated to " << maxAmp << " (< 0.2)" << std::endl;
}

void testBufferStressTest(uint32_t bufferSize) {
    std::cout << "[Stress Test] Testing buffer size: " << bufferSize << " samples..." << std::endl;

    AudioEngine engine;
    AudioEngineConfig cfg;
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = bufferSize;

    // Offline buffer test
    std::vector<float> bufL(bufferSize);
    std::vector<float> bufR(bufferSize);

    // Trigger polyphonic chord
    engine.postNoteOn(60, 0.9f); // C4
    engine.postNoteOn(64, 0.8f); // E4
    engine.postNoteOn(67, 0.8f); // G4

    constexpr size_t TOTAL_BLOCKS = 2000; // ~5.3 seconds of audio
    const auto start = std::chrono::high_resolution_clock::now();

    for (size_t b = 0; b < TOTAL_BLOCKS; ++b) {
        engine.renderOfflineBlock(bufL.data(), bufR.data(), bufferSize);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    const double totalAudioSeconds = static_cast<double>(TOTAL_BLOCKS * bufferSize) / 48000.0;
    const double realTimeBudgetMs = totalAudioSeconds * 1000.0;
    const double speedupFactor = realTimeBudgetMs / elapsedMs;

    std::cout << "  -> Rendered " << (TOTAL_BLOCKS * bufferSize) << " samples (" << totalAudioSeconds << "s audio) in "
              << elapsedMs << "ms (Speed: " << speedupFactor << "x real-time)" << std::endl;

    assert(speedupFactor > 10.0); // Must be at least 10x faster than real-time budget
}

void testTb303DspStressTest() {
    std::cout << "[Stress Test] Testing TB-303 diode ladder 4x oversampled synthesis..." << std::endl;
    Tb303Core tb303;
    tb303.setSampleRate(48000.0f);
    tb303.setCutoff(1200.0f);
    tb303.setResonance(0.85f);
    tb303.setOverdrive(0.4f);

    tb303.noteOn(36, 0.95f, false, true); // C2 with accent

    constexpr size_t SAMPLES = 48000 * 2; // 2 seconds
    std::vector<float> output(SAMPLES);

    const auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < SAMPLES; ++i) {
        output[i] = tb303.processSample();
    }
    const auto end = std::chrono::high_resolution_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "  -> 2.0s 4x-oversampled 303 rendered in " << elapsedMs << "ms ("
              << (2000.0 / elapsedMs) << "x real-time)" << std::endl;

    assert(elapsedMs < 200.0); // Real-time margin
}

void testTb303CutoffSweepStability() {
    std::cout << "[Test] TB-303 Cutoff Sweep Stability & Non-Linear Diode Saturation..." << std::endl;
    Tb303Core tb303;
    tb303.setSampleRate(48000.0f);
    tb303.setResonance(0.95f);
    tb303.setEnvMod(0.85f);
    tb303.setOverdrive(0.25f);

    // Trigger note with slide & accent
    tb303.noteOn(38, 0.90f, true, true);

    // Sweep cutoff across entire range: 100 Hz to 8000 Hz
    float peak = 0.0f;
    for (int step = 0; step < 500; ++step) {
        float sweepCutoff = 100.0f + (step / 500.0f) * 7900.0f;
        tb303.setCutoff(sweepCutoff);
        for (int s = 0; s < 64; ++s) {
            float smp = tb303.processSample();
            assert(!std::isnan(smp) && !std::isinf(smp));
            peak = std::max(peak, std::abs(smp));
        }
    }

    std::cout << "  -> Peak amplitude across 100Hz-8000Hz sweep: " << peak << std::endl;
    assert(peak <= 1.0f);
    assert(peak > 0.05f); // Must produce healthy non-zero signal
    std::cout << "  [PASS] TB-303 Cutoff sweep remains strictly bounded within [-1.0, 1.0]." << std::endl;
}

void testAudioEnginePanic() {
    std::cout << "[Test] AudioEngine::panic() hard reset and buffer clearing..." << std::endl;
    AudioEngine engine;
    engine.initialize();
    engine.setupDefaultAcidBeatGraph();
    engine.getSequencer().start();
    assert(engine.getSequencer().isPlaying());

    // Render a block to populate delay and synth buffers
    float outL[256], outR[256];
    engine.renderOfflineBlock(outL, outR, 256);

    // Initiate Panic
    engine.panic();

    // Verify sequencer is stopped
    assert(!engine.getSequencer().isPlaying());

    // Verify that rendering immediately after panic yields silent/bounded output
    engine.renderOfflineBlock(outL, outR, 256);
    float postPanicPeak = 0.0f;
    for (int i = 0; i < 256; ++i) {
        postPanicPeak = std::max(postPanicPeak, std::max(std::abs(outL[i]), std::abs(outR[i])));
    }
    std::cout << "  -> Post-panic output peak: " << postPanicPeak << std::endl;
    assert(postPanicPeak < 0.001f);
    std::cout << "  [PASS] AudioEngine::panic() halted sequencer and flushed all audio buffers." << std::endl;
}

int main() {
    std::cout << "=== Eatsbits Audio & Real-Time DSP Test Suite ===" << std::endl;
    testRingBufferConcurrency();
    testDspBiquad();
    testBufferStressTest(32);
    testBufferStressTest(64);
    testBufferStressTest(128);
    testTb303DspStressTest();
    testTb303CutoffSweepStability();
    testAudioEnginePanic();
    std::cout << "=== ALL AUDIO TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
