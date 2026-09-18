#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/eatscript/vm.hpp"
#include "eatsbits/eatscript/transpiler.hpp"

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::eatscript;

void printBanner() {
    std::cout << R"(
  ______      _       _     _ _       
 |  ____|    | |     | |   (_) |      
 | |__   __ _| |_ ___| |__  _| |_ ___ 
 |  __| / _` | __/ __| '_ \| | __/ __|
 | |___| (_| | |_\__ \ |_) | | |_\__ \
 |______\__,_|\__|___/_.__/|_|\__|___/
   Next-Generation Native C++ DAW Engine
)" << std::endl;
}

void playAcidSequence(AudioEngine& engine) {
    std::cout << "\n[Sequencer] Playing authentic TB-303 Acid Bassline sequence..." << std::endl;
    engine.setEngineMode(SynthEngineMode::Tb303Acid);
    engine.setCutoff(900.0f);
    engine.setResonance(0.85f);

    // Classic Acid pattern: (note, velocity, isSlide, isAccent)
    struct Step {
        uint8_t note;
        float velocity;
        bool isSlide;
        bool isAccent;
    };

    const std::vector<Step> pattern = {
        {36, 0.9f, false, true},   // C2 (Accent)
        {36, 0.7f, false, false},  // C2
        {48, 0.8f, true,  false},  // C3 (Slide)
        {36, 0.7f, false, false},  // C2
        {39, 0.95f, true, true},   // Eb2 (Slide + Accent)
        {41, 0.85f, true, false},  // F2 (Slide)
        {36, 0.7f, false, false},  // C2
        {46, 0.95f, false, true},  // Bb2 (Accent)
        {48, 0.8f, true, false},   // C3 (Slide)
        {36, 0.7f, false, false},  // C2
        {38, 0.85f, false, false}, // D2
        {39, 0.9f, true, true},    // Eb2 (Slide + Accent)
        {36, 0.7f, false, false},  // C2
        {43, 0.85f, false, false}, // G2
        {41, 0.8f, true, false},   // F2 (Slide)
        {39, 0.9f, false, true}    // Eb2 (Accent)
    };

    const auto stepDuration = std::chrono::milliseconds(125); // 120 BPM 16th notes

    // Play 2 loops with dynamic filter cutoff sweep
    for (int loop = 0; loop < 2; ++loop) {
        float sweepCutoff = 500.0f + loop * 800.0f;
        for (size_t s = 0; s < pattern.size(); ++s) {
            const auto& step = pattern[s];
            sweepCutoff += 40.0f;
            engine.setCutoff(sweepCutoff);
            engine.postNoteOn(step.note, step.velocity, step.isSlide, step.isAccent);

            std::this_thread::sleep_for(stepDuration);
            if (!step.isSlide) {
                engine.postNoteOff(step.note);
            }

            MeterFeedback fb;
            if (engine.pollMeterFeedback(fb)) {
                int meterBars = static_cast<int>(fb.peakLeft * 20.0f);
                std::cout << "\r[Step " << (s + 1) << "/16] Cutoff: " << static_cast<int>(sweepCutoff)
                          << " Hz | Peak: [" << std::string(meterBars, '=')
                          << std::string(20 - std::min(20, meterBars), ' ') << "] " << std::flush;
            }
        }
    }
    std::cout << "\n[Sequencer] Sequence finished.\n" << std::endl;
}

int main(int argc, char** argv) {
    printBanner();

    AudioEngine engine;
    AudioEngineConfig config;
    config.sampleRate = 48000;
    config.bufferFrameSize = 128;

    std::cout << "[Init] Starting Eatsbits Audio Engine..." << std::endl;
    if (!engine.initialize(config)) {
        std::cerr << "[Error] Could not initialize audio output. Running in headless mode." << std::endl;
    } else {
        engine.start();
        std::cout << "[Audio] Real-time engine active at " << engine.getSampleRate()
                  << " Hz (" << engine.getBufferFrameSize() << " sample buffer)." << std::endl;
    }

    std::cout << "\nAvailable Commands:" << std::endl;
    std::cout << "  1 : Play TB-303 Acid Bassline sequence" << std::endl;
    std::cout << "  2 : Trigger Polyphonic Chord (C Major)" << std::endl;
    std::cout << "  3 : Live-Code Eatscript (transpile & run VM)" << std::endl;
    std::cout << "  q : Quit" << std::endl;

    std::string input;
    bool running = true;

    // Check if run in non-interactive / automated mode
    if (argc > 1 && std::string(argv[1]) == "--test") {
        std::cout << "[CLI] Running automated verification test..." << std::endl;
        playAcidSequence(engine);
        engine.shutdown();
        return 0;
    }

    while (running) {
        std::cout << "\neatsbits> " << std::flush;
        if (!std::getline(std::cin, input)) break;

        if (input == "q" || input == "quit" || input == "exit") {
            running = false;
        } else if (input == "1") {
            playAcidSequence(engine);
        } else if (input == "2") {
            engine.setEngineMode(SynthEngineMode::PolySynth);
            engine.setCutoff(3000.0f);
            engine.setResonance(1.2f);
            std::cout << "[PolySynth] Triggering C Major 7 chord..." << std::endl;
            engine.postNoteOn(60, 0.9f); // C4
            engine.postNoteOn(64, 0.8f); // E4
            engine.postNoteOn(67, 0.8f); // G4
            engine.postNoteOn(71, 0.7f); // B4

            std::this_thread::sleep_for(std::chrono::milliseconds(1200));

            engine.postNoteOff(60);
            engine.postNoteOff(64);
            engine.postNoteOff(67);
            engine.postNoteOff(71);
            std::cout << "[PolySynth] Released notes." << std::endl;
        } else if (input == "3") {
            std::cout << "\n[Eatscript Live Coding] Enter your Eatscript code (finish with empty line):" << std::endl;
            std::string code;
            std::string line;
            while (std::getline(std::cin, line) && !line.empty()) {
                code += line + "\n";
            }
            if (code.empty()) {
                code = "def process(time, freq, note, params):\n    return math.sin(2.0 * math.pi * freq * time)\n";
                std::cout << "Using default Eatscript sine generator:\n" << code << std::endl;
            }

            VM vm;
            if (vm.compileSource(code)) {
                std::cout << "[Eatscript VM] Script compiled successfully!" << std::endl;
                double s = vm.executeProcess(0.001, 440.0, 69.0);
                std::cout << "[Eatscript VM] Sample at t=1ms, f=440Hz: " << s << std::endl;

                Transpiler transpiler;
                std::string cpp = transpiler.transpileSource(code, "live_plugin", "Live Plugin");
                std::cout << "[AOT Transpiler] Transpiled C++ plugin size: " << cpp.size() << " bytes." << std::endl;
            } else {
                std::cout << "[Eatscript VM] Failed to compile script." << std::endl;
            }
        }
    }

    std::cout << "[Shutdown] Stopping audio engine..." << std::endl;
    engine.shutdown();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
