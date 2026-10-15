#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <cassert>

#include "eatsbits/audio/dsp/snes_dsp_core.hpp"
#include "eatsbits/audio/dsp/snes_sfxr_generator.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/snes_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/project/preset_loader.hpp"

using namespace eatsbits::audio;

int main() {
    std::cout << "==================================================\n";
    std::cout << "   Eatsbits Super Nintendo S-DSP Unit Tests      \n";
    std::cout << "==================================================\n";

    // [Test 1] 12 BRR Waveforms & Gaussian Smoothing
    {
        std::cout << "[Test 1] Testing 12 BRR Waveforms & Gaussian Smoothing...\n";
        SNESVoice voice(0);
        constexpr float kPi = 3.1415926535f;

        for (uint8_t w = 0; w < static_cast<uint8_t>(SNESWaveform::Count); ++w) {
            voice.waveform = static_cast<SNESWaveform>(w);
            float val0 = voice.evaluateWaveform(0.0f);
            float valMid = voice.evaluateWaveform(kPi * 0.5f);
            assert(!std::isnan(val0));
            assert(!std::isnan(valMid));
            assert(val0 >= -1.5f && val0 <= 1.5f);
            assert(valMid >= -1.5f && valMid <= 1.5f);
            (void)val0;
            (void)valMid;
        }

        // Test Gaussian smoothing at square wave transition
        voice.waveform = SNESWaveform::Square;
        float smoothVal = SNESVoice::gaussianSmooth(1.0f, 0.01f, 0.5f);
        assert(smoothVal < 1.0f); // Smoothed
        (void)smoothVal;
        std::cout << "  All 12 BRR waveforms and Gaussian smoothing verified -> PASSED\n";
    }

    // [Test 2] S-DSP 6 Hardware Envelope Modes
    {
        std::cout << "[Test 2] Testing S-DSP 6 Hardware Envelope Modes...\n";
        SNESVoice voice(0);
        voice.attack = 0.01f;
        voice.decay = 0.05f;
        voice.sustain = 0.5f;
        voice.release = 0.05f;
        voice.gainLevel = 0.75f;

        // ADSR
        voice.envMode = SNESEnvelopeMode::Adsr;
        float envAtk = voice.evaluateEnvelope(0.005f, 0.2f);
        assert(envAtk > 0.0f && envAtk < 1.0f);
        float envSus = voice.evaluateEnvelope(0.1f, 0.2f);
        assert(std::abs(envSus - 0.5f) < 0.01f);
        (void)envAtk;
        (void)envSus;

        // Direct GAIN
        voice.envMode = SNESEnvelopeMode::GainDirect;
        assert(std::abs(voice.evaluateEnvelope(0.05f, 0.2f) - 0.75f) < 0.001f);

        // Linear Decrease
        voice.envMode = SNESEnvelopeMode::GainLinearDecrease;
        float decMid = voice.evaluateEnvelope(0.025f, 0.2f);
        assert(decMid > 0.4f && decMid < 0.6f);
        (void)decMid;

        // Exponential Decrease
        voice.envMode = SNESEnvelopeMode::GainExpDecrease;
        float expDec = voice.evaluateEnvelope(0.05f, 0.2f);
        assert(expDec < 0.4f);
        (void)expDec;

        // Linear Increase
        voice.envMode = SNESEnvelopeMode::GainLinearIncrease;
        float linInc = voice.evaluateEnvelope(0.005f, 0.2f);
        assert(linInc > 0.4f && linInc < 0.6f);
        (void)linInc;

        // Bent Increase
        voice.envMode = SNESEnvelopeMode::GainBentIncrease;
        float bentInc = voice.evaluateEnvelope(0.005f, 0.2f);
        assert(bentInc > 0.0f && bentInc < 0.5f);
        (void)bentInc;

        std::cout << "  All 6 S-DSP envelope modes verified -> PASSED\n";
    }

    // [Test 3] 15-Bit Galois LFSR Noise Generator
    {
        std::cout << "[Test 3] Testing 15-Bit Galois LFSR Noise Generator...\n";
        SNESDSPEngine engine;
        std::vector<float> noiseSamples;
        noiseSamples.reserve(1000);

        for (int i = 0; i < 1000; ++i) {
            float n = engine.stepNoise(8);
            assert(n >= -1.0f && n <= 1.0f);
            noiseSamples.push_back(n);
        }

        // Verify variance > 0 (not stuck)
        float sum = 0.0f;
        for (float s : noiseSamples) sum += s;
        float mean = sum / 1000.0f;
        float var = 0.0f;
        for (float s : noiseSamples) var += (s - mean) * (s - mean);
        var /= 1000.0f;
        assert(var > 0.05f);
        std::cout << "  Noise variance: " << var << " -> PASSED\n";
    }

    // [Test 4] PMOD Cross-Voice Pitch Modulation
    {
        std::cout << "[Test 4] Testing Cross-Channel PMOD Pitch Modulation...\n";
        SNESDSPEngine engine;
        engine.voices[0].enabled = true;
        engine.voices[0].active = true;
        engine.voices[0].basePitchHz = 220.0f;

        engine.voices[1].enabled = true;
        engine.voices[1].active = true;
        engine.voices[1].basePitchHz = 440.0f;
        engine.voices[1].pmodEnabled = true;

        float outL = 0.0f, outR = 0.0f;
        for (int i = 0; i < 100; ++i) {
            engine.processStereo(48000.0f, outL, outR);
            assert(!std::isnan(outL) && !std::isnan(outR));
        }
        std::cout << "  PMOD cross-channel pitch modulation stable -> PASSED\n";
    }

    // [Test 5] 8-Tap FIR Stereo Echo Unit
    {
        std::cout << "[Test 5] Testing 8-Tap FIR Stereo Echo Unit...\n";
        SNESEchoUnit echo;
        echo.enabled = true;
        echo.volume = 0.5f;
        echo.feedback = 0.5f;
        echo.setDelayMs(32); // 32ms short echo

        float outL = 0.0f, outR = 0.0f;
        // Inject single stereo impulse
        echo.processStereo(1.0f, 1.0f, outL, outR);
        assert(std::abs(outL) > 0.0f);

        // Feed silence for 2000 samples and verify echoes emerge
        float maxEcho = 0.0f;
        for (int i = 0; i < 2000; ++i) {
            echo.processStereo(0.0f, 0.0f, outL, outR);
            maxEcho = std::max(maxEcho, std::abs(outL));
        }
        assert(maxEcho > 0.01f); // Echo signal arrived
        std::cout << "  8-tap FIR echo buffer response verified (peak: " << maxEcho << ") -> PASSED\n";
    }

    // [Test 6] SnesNode Modular Audio Graph Integration
    {
        std::cout << "[Test 6] Testing SnesNode Modular Audio Graph Integration...\n";
        AudioGraph graph;
        graph.prepare(48000.0, 128);

        auto snes = std::make_shared<SnesNode>("SnesTest");
        auto master = std::make_shared<GainNode>("Master_Out");

        NodeId snesId = graph.addNode(snes);
        NodeId masterId = graph.addNode(master);
        (void)snesId;
        (void)masterId;

        assert(graph.connect(snesId, 0, masterId, 0));

        snes->noteOn(60, 0.9f); // Middle C

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

    // [Test 7] Performance Benchmark: 10 Seconds of 8-Voice S-DSP Audio
    {
        std::cout << "[Test 7] Performance Benchmark: 10s of 8-Voice Polyphonic S-DSP Audio...\n";
        SNESDSPEngine engine;
        engine.echo.enabled = true;
        engine.echo.volume = 0.45f;
        engine.echo.feedback = 0.50f;

        // Activate all 8 channels
        for (int i = 0; i < 8; ++i) {
            float freq = 110.0f * (i + 1);
            engine.noteOn(i, freq, 0.8f, 10.0f);
            engine.voices[i].waveform = static_cast<SNESWaveform>(i % static_cast<int>(SNESWaveform::Count));
            if (i > 0) engine.voices[i].pmodEnabled = (i % 2 == 1);
        }

        constexpr int kTotalSamples = 48000 * 10;
        float outL = 0.0f, outR = 0.0f;

        auto startTime = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < kTotalSamples; ++i) {
            engine.processStereo(48000.0f, outL, outR);
        }
        auto endTime = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> elapsedMs = endTime - startTime;
        double speedup = 10000.0 / elapsedMs.count();

        std::cout << "  Rendered 10s of 8-voice S-DSP audio in " << elapsedMs.count()
                  << " ms (" << speedup << "x real-time speedup)\n";
        assert(speedup > 10.0);
        std::cout << "  -> PASSED: Real-time S-DSP synthesis exceeds performance threshold.\n";
    }

    // [Test 8] SNESSFXRGenerator: 11 Iconic Archetypes & Param Generation
    {
        std::cout << "[Test 8] Testing SNESSFXRGenerator Archetypes & Param Maps...\n";
        for (int type = 0; type < 11; ++type) {
            auto params = SNESSFXRGenerator::generateParamsForType(type, 42);
            assert(!params.empty());
            assert(params.find("Attack") != params.end());
            assert(params.find("Decay") != params.end());
            assert(params.find("Sustain") != params.end());
            assert(params.find("Release") != params.end());
            assert(params.find("PitchSweep") != params.end());
            assert(params.find("Waveform") != params.end());

            for (const auto& [k, v] : params) {
                assert(!std::isnan(v) && !std::isinf(v));
            }

            auto cands = SNESSFXRGenerator::getCandidateWaveformsForType(type);
            assert(!cands.empty());
        }
        std::cout << "  All 11 SFX archetypes and parameter maps verified -> PASSED\n";
    }

    // [Test 9] SNESSFXRGenerator: Configure SNESDSPEngine & Render Audio
    {
        std::cout << "[Test 9] Testing SNESSFXRGenerator::configureFromType & Audio Synthesis...\n";
        SNESDSPEngine engine;
        for (int type = 0; type < 11; ++type) {
            SNESSFXRGenerator::configureFromType(engine, type, 1234 + type);
            engine.noteOn(0, 523.25f, 0.85f, 0.5f); // C5
            float outL = 0.0f, outR = 0.0f;
            float maxAmp = 0.0f;
            for (int s = 0; s < 4800; ++s) {
                engine.processStereo(48000.0f, outL, outR);
                assert(!std::isnan(outL) && !std::isnan(outR));
                maxAmp = std::max(maxAmp, std::abs(outL));
            }
            assert(maxAmp > 0.01f);
        }
        std::cout << "  All 11 archetypes successfully rendered audio without NaNs -> PASSED\n";
    }

    // [Test 10] SnesNode: Parameter Dispatch (IDs 16-21) in Audio Graph
    {
        std::cout << "[Test 10] Testing SnesNode Parameter Dispatch (IDs 16-21)...\n";
        AudioGraph graph;
        graph.prepare(48000.0, 128);
        auto snes = std::make_shared<SnesNode>("SnesSfxrNode");
        auto master = std::make_shared<GainNode>("Master_Out");

        NodeId snesId = graph.addNode(snes);
        NodeId masterId = graph.addNode(master);
        assert(graph.connect(snesId, 0, masterId, 0));

        // Set SFXType (ID 19)
        snes->setParameter(19, 0.0f); // Laser
        // Set Seed (ID 20)
        snes->setParameter(20, 777.0f);
        // Set PitchSweep (ID 16)
        snes->setParameter(16, -1.2f);
        // Set VibratoRate (ID 17)
        snes->setParameter(17, 8.0f);
        // Set VibratoDepth (ID 18)
        snes->setParameter(18, 0.25f);
        // Trigger Randomize (ID 21)
        snes->setParameter(21, 1.0f);

        snes->noteOn(72, 0.9f); // C5

        float outL[128]{0};
        float outR[128]{0};
        float peak = 0.0f;
        for (int b = 0; b < 20; ++b) {
            graph.process(outL, outR, 128);
            for (int i = 0; i < 128; ++i) {
                assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
                peak = std::max(peak, std::abs(outL[i]));
            }
        }
        assert(peak > 0.02f);
        std::cout << "  SnesNode parameter dispatch & graph execution -> PASSED\n";
    }

    // [Test 11] PresetLoader: createSnesSfxrPreset & createSnesPreset Layouts
    {
        std::cout << "[Test 11] Testing PresetLoader SNES Sfxr & SNES Synth Layout Definitions...\n";
        auto sfxrPreset = eatsbits::project::PresetLoader::createSnesSfxrPreset();
        assert(sfxrPreset.metadata.id == "eats_sfxr");
        assert(sfxrPreset.metadata.name == "SNES Sfxr");
        assert(sfxrPreset.metadata.engineId == "snes");
        assert(sfxrPreset.guiRoot.children.size() == 2);
        // Row 1 has ListBox(SFXType), ListBox(Waveform), and Column(Nixie + Button)
        const auto& r1 = sfxrPreset.guiRoot.children[0];
        assert(r1.children.size() == 3);
        assert(r1.children[0].type == eatsbits::project::GuiNodeType::ListBox);
        assert(r1.children[1].type == eatsbits::project::GuiNodeType::ListBox);
        assert(r1.children[2].type == eatsbits::project::GuiNodeType::Column);
        // Row 2 has 9 Knobs
        const auto& r2 = sfxrPreset.guiRoot.children[1];
        assert(r2.children.size() == 9);

        auto synthPreset = eatsbits::project::PresetLoader::createSnesPreset();
        assert(synthPreset.metadata.id == "snes_console_synth");
        assert(synthPreset.metadata.name == "SNES Synth");
        assert(synthPreset.guiRoot.children.size() == 2);

        std::cout << "  Preset layouts for SNES Sfxr and SNES Synth verified -> PASSED\n";
    }

    // [Test 12] SnesNode: All 12 Waveforms Produce Audible Audio (No Silence)
    {
        std::cout << "[Test 12] Testing SnesNode: All 12 Waveforms Produce Audible Audio...\n";
        AudioGraph graph;
        graph.prepare(48000.0, 128);
        auto snes = std::make_shared<SnesNode>("SnesWaveformTest");
        auto master = std::make_shared<GainNode>("Master_Out");

        NodeId snesId = graph.addNode(snes);
        NodeId masterId = graph.addNode(master);
        assert(graph.connect(snesId, 0, masterId, 0));

        const char* waveNames[12] = {
            "Square", "Pulse 25%", "Pulse 12%", "Sawtooth", "Triangle", "Sine",
            "Organ", "Strings", "Flute", "Slap Bass", "Chime", "Noise"
        };

        for (int w = 0; w < 12; ++w) {
            // Test setting waveform parameter
            snes->setParameter(0, static_cast<float>(w));
            snes->noteOn(60, 0.85f); // Middle C

            float outL[128]{0};
            float outR[128]{0};
            float peak = 0.0f;
            for (int b = 0; b < 10; ++b) {
                graph.process(outL, outR, 128);
                for (int i = 0; i < 128; ++i) {
                    assert(!std::isnan(outL[i]) && !std::isinf(outL[i]));
                    peak = std::max(peak, std::abs(outL[i]));
                }
            }
            assert(peak > 0.01f);
            std::cout << "    Waveform [" << w << "] " << waveNames[w] << " peak: " << peak << " -> OK\n";
            snes->noteOff(60);
        }
        std::cout << "  All 12 waveforms generate audible sound without silence -> PASSED\n";
    }

    std::cout << "==================================================\n";
    std::cout << "    100% SUPER NINTENDO S-DSP TESTS PASSED!       \n";
    std::cout << "==================================================\n";
    return 0;
}
