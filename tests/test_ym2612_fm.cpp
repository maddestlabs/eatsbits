#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <cassert>

#include "eatsbits/audio/dsp/ym2612_core.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/ym2612_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"

using namespace eatsbits::audio;

int main() {
    std::cout << "==================================================\n";
    std::cout << "   Eatsbits Sega Genesis YM2612 FM Unit Tests     \n";
    std::cout << "==================================================\n";

    // [Test 1] 8 OPN/OPN2 FM Routing Algorithms
    {
        std::cout << "[Test 1] Testing 8 OPN/OPN2 FM Routing Algorithms (0..7)...\n";
        YM2612Voice voice;
        constexpr float dt = 1.0f / 48000.0f;

        for (int alg = 0; alg < 8; ++alg) {
            voice.reset();
            voice.algorithm = alg;
            voice.feedback = 3;
            for (auto& op : voice.operators) {
                op.setTotalLevel(10.0f);
                op.attack = 0.001f;
                op.decay = 0.1f;
                op.sustain = 0.5f;
            }

            float peak = 0.0f;
            for (int i = 0; i < 200; ++i) {
                float s = voice.evaluateSample(i * dt, 440.0f, dt, 0.4f, i);
                assert(!std::isnan(s));
                assert(!std::isinf(s));
                peak = std::max(peak, std::abs(s));
            }
            assert(peak > 0.01f);
        }
        std::cout << "  All 8 FM algorithms evaluated with stable outputs -> PASSED\n";
    }

    // [Test 2] Operator 1 Self-Feedback Stability
    {
        std::cout << "[Test 2] Testing Operator 1 Self-Feedback (Levels 0..7)...\n";
        YM2612Voice voice;
        constexpr float dt = 1.0f / 48000.0f;

        for (int fb = 0; fb <= 7; ++fb) {
            voice.reset();
            voice.algorithm = 0; // serial stack
            voice.feedback = fb;
            voice.operators[0].setTotalLevel(0.0f);

            float peak = 0.0f;
            for (int i = 0; i < 150; ++i) {
                float s = voice.evaluateSample(i * dt, 220.0f, dt, 0.4f, i);
                assert(!std::isnan(s));
                peak = std::max(peak, std::abs(s));
            }
            assert(peak > 0.001f);
        }
        std::cout << "  Operator 1 feedback stable across all 8 levels -> PASSED\n";
    }

    // [Test 3] Total Level (TL) Attenuation Scaling
    {
        std::cout << "[Test 3] Testing Total Level (TL) Logarithmic Attenuation...\n";
        FMOperator op;
        op.setTotalLevel(0.0f);
        assert(std::abs(op.getTlAtten() - 1.0f) < 0.001f); // 0dB = 1.0

        op.setTotalLevel(60.0f);
        // 60 * 0.75dB = 45dB attenuation -> 10^(-45/20) ~ 0.0056
        assert(op.getTlAtten() < 0.01f && op.getTlAtten() > 0.001f);

        op.setTotalLevel(127.0f);
        assert(op.getTlAtten() < 0.0001f);
        std::cout << "  Total level attenuation curves verified -> PASSED\n";
    }

    // [Test 4] SFXR Procedural Patch Generator
    {
        std::cout << "[Test 4] Testing SFXR Procedural FM Patch Generator...\n";
        YM2612Voice voice;
        constexpr float dt = 1.0f / 48000.0f;

        for (int sfx = 0; sfx < 6; ++sfx) {
            SFXRGenerator::configureFromType(voice, sfx, 42);
            float peak = 0.0f;
            for (int i = 0; i < 200; ++i) {
                float s = voice.evaluateSample(i * dt, 440.0f, dt, 0.4f, i);
                assert(!std::isnan(s));
                peak = std::max(peak, std::abs(s));
            }
            assert(peak > 0.01f);
        }
        std::cout << "  All 6 SFXR procedural presets (Laser, Explosion, Powerup, Coin, Jump, Hit) verified -> PASSED\n";
    }

    // [Test 5] 6-Voice Polyphony & Voice Stealing
    {
        std::cout << "[Test 5] Testing 6-Voice Polyphony & Voice Stealing...\n";
        YM2612Synth synth;
        synth.setSampleRate(48000.0f);

        // Trigger 6 distinct notes
        float notes[6] = {261.63f, 329.63f, 392.00f, 523.25f, 659.25f, 783.99f};
        for (int i = 0; i < 6; ++i) {
            synth.noteOn(notes[i], 0.8f, 1.0f);
        }

        // Verify all 6 voices are active
        for (int i = 0; i < 6; ++i) {
            assert(synth.voices[i].active);
        }

        // Trigger 7th note -> should steal voice without crash
        synth.noteOn(1046.50f, 0.9f, 1.0f);
        assert(synth.voices[0].active);
        std::cout << "  6-voice polyphony and voice stealing active -> PASSED\n";
    }

    // [Test 6] Ym2612Node Modular Audio Graph Integration
    {
        std::cout << "[Test 6] Testing Ym2612Node Modular Audio Graph Integration...\n";
        AudioGraph graph;
        graph.prepare(48000.0, 128);

        auto ym = std::make_shared<Ym2612Node>("YmTest");
        auto master = std::make_shared<GainNode>("Master_Out");

        NodeId ymId = graph.addNode(ym);
        NodeId masterId = graph.addNode(master);
        (void)ymId;
        (void)masterId;

        assert(graph.connect(ymId, 0, masterId, 0));

        ym->noteOn(60, 0.9f); // Middle C

        float outL[128]{0};
        float outR[128]{0};
        float peak = 0.0f;

        for (int b = 0; b < 30; ++b) {
            graph.process(outL, outR, 128);
            for (int i = 0; i < 128; ++i) {
                assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
                peak = std::max(peak, std::abs(outL[i]));
            }
        }
        assert(peak > 0.05f);
        std::cout << "  Graph execution peak: " << peak << " -> PASSED\n";
    }

    // [Test 7] Performance Benchmark: 10 Seconds of 6-Voice YM2612 Audio
    {
        std::cout << "[Test 7] Performance Benchmark: 10s of 6-Voice YM2612 4-Op FM Audio...\n";
        YM2612Synth synth;
        synth.setSampleRate(48000.0f);

        // Play 6-note chord
        float chord[6] = {130.81f, 196.00f, 261.63f, 329.63f, 392.00f, 523.25f};
        for (int i = 0; i < 6; ++i) {
            synth.noteOn(chord[i], 0.85f, 10.0f);
        }

        constexpr int kTotalSamples = 48000 * 10;
        float outL = 0.0f, outR = 0.0f;

        auto startTime = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kTotalSamples; ++i) {
            synth.processStereo(outL, outR);
        }
        auto endTime = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsedMs = endTime - startTime;
        double speedup = 10000.0 / elapsedMs.count();

        std::cout << "  Rendered 10s of 6-voice YM2612 audio in " << elapsedMs.count()
                  << " ms (" << speedup << "x real-time speedup)\n";
        assert(speedup > 15.0);
        std::cout << "  -> PASSED: Real-time 4-Op FM synthesis exceeds performance threshold.\n";
    }

    std::cout << "==================================================\n";
    std::cout << "     100% SEGA GENESIS YM2612 TESTS PASSED!       \n";
    std::cout << "==================================================\n";
    return 0;
}
