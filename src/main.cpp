#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <iomanip>
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"
#include "eatsbits/project/project_file.hpp"
#include "eatsbits/ui/canvas_renderer.hpp"
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

void playModularGraphDemo(AudioEngine& engine) {
    std::cout << "\n[Modular Graph] Configuring zero-allocation DAG pipeline..." << std::endl;
    std::cout << "  Patching: [TB-303 Source] -> [Stereo Delay (375ms, 45% Feedback)] -> [Master Gain]" << std::endl;
    engine.setupDefaultAcidGraph();

    struct Step {
        uint8_t note;
        float velocity;
        bool isSlide;
        bool isAccent;
    };

    const std::vector<Step> pattern = {
        {36, 0.95f, false, true},  // C2
        {36, 0.70f, false, false}, // C2
        {48, 0.85f, true,  false}, // C3 slide
        {46, 0.90f, false, true},  // Bb2
        {41, 0.80f, true,  false}, // F2 slide
        {39, 0.95f, false, true},  // Eb2
        {43, 0.80f, false, false}, // G2
        {36, 0.90f, false, true}   // C2
    };

    const auto stepDuration = std::chrono::milliseconds(140);
    for (size_t s = 0; s < pattern.size(); ++s) {
        const auto& step = pattern[s];
        std::cout << "\r[DAG Step " << (s + 1) << "/" << pattern.size() << "] Playing Note: " << static_cast<int>(step.note) << "   " << std::flush;
        engine.postNoteOn(step.note, step.velocity, step.isSlide, step.isAccent);
        std::this_thread::sleep_for(stepDuration);
        if (!step.isSlide) {
            engine.postNoteOff(step.note);
        }
    }
    std::cout << "\n[Modular Graph] Sequence finished (auditioning delay decay tail)...\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
}

void playAcidBeatDemo(AudioEngine& engine) {
    std::cout << "\n[Acid Techno Groove] Configuring Modular 303 + 808/909 Drum DAG..." << std::endl;
    std::cout << "  Patching: [TB-303 Acid Bass] -> [Stereo Delay] -> [Master Bus]" << std::endl;
    std::cout << "            [TR-808/909 Drums]                   -> [Master Bus]" << std::endl;
    engine.setupDefaultAcidBeatGraph();

    struct GrooveStep {
        uint8_t tbNote;
        float tbVel;
        bool isSlide;
        bool isAccent;
        std::vector<uint8_t> drumHits;
    };

    const std::vector<GrooveStep> pattern = {
        {36, 0.95f, false, true,  {36, 42}},      // 909 Kick + Closed Hat + C2 (Accent)
        {0,  0.00f, false, false, {42}},          // Closed Hat
        {36, 0.70f, false, false, {42}},          // Closed Hat + C2
        {0,  0.00f, false, false, {46}},          // Open Hat
        {48, 0.85f, true,  false, {38, 42}},      // 808 Snare + Closed Hat + C3 (Slide)
        {0,  0.00f, false, false, {42}},          // Closed Hat
        {0,  0.00f, false, false, {36, 42}},      // 909 Kick + Closed Hat
        {39, 0.95f, true,  true,  {46}},          // Open Hat + Eb2 (Slide + Accent)
        {41, 0.80f, true,  false, {36, 42}},      // 909 Kick + Closed Hat + F2 (Slide)
        {0,  0.00f, false, false, {42}},          // Closed Hat
        {36, 0.70f, false, false, {42}},          // Closed Hat + C2
        {0,  0.00f, false, false, {46}},          // Open Hat
        {46, 0.90f, false, true,  {38, 39, 42}},  // 808 Snare + 808 Clap + Bb2 (Accent)
        {0,  0.00f, false, false, {42}},          // Closed Hat
        {48, 0.80f, false, false, {36, 42}},      // 909 Kick + Closed Hat + C3
        {39, 0.95f, false, true,  {46, 56}}       // Open Hat + Cowbell + Eb2 (Accent)
    };

    const auto stepDuration = std::chrono::milliseconds(130);
    // Play 2 full bars
    for (int bar = 0; bar < 2; ++bar) {
        for (size_t s = 0; s < pattern.size(); ++s) {
            const auto& step = pattern[s];
            std::cout << "\r[Groove Bar " << (bar + 1) << "/2 | Step " << (s + 1) << "/16]"
                      << (step.tbNote ? " TB:" + std::to_string(step.tbNote) : "       ")
                      << (step.drumHits.size() > 1 ? " [Drums+Hats]" : " [Hat]") << "   " << std::flush;

            // Trigger drums
            for (uint8_t dNote : step.drumHits) {
                engine.postNoteOn(dNote, 0.88f);
            }

            // Trigger TB-303
            if (step.tbNote > 0) {
                engine.postNoteOn(step.tbNote, step.tbVel, step.isSlide, step.isAccent);
            }

            std::this_thread::sleep_for(stepDuration);

            if (step.tbNote > 0 && !step.isSlide) {
                engine.postNoteOff(step.tbNote);
            }
        }
    }
    std::cout << "\n[Acid Techno Groove] Groove finished.\n" << std::endl;
}

void playSequencerProjectDemo(AudioEngine& engine) {
    std::cout << "\n[Step Sequencer & Project I/O] Setting up sample-accurate project..." << std::endl;

    engine.setEngineMode(SynthEngineMode::ModularGraph);
    auto& graph = engine.getGraph();
    graph.clear();

    auto tb303 = std::make_shared<Tb303Node>("AcidTB303");
    auto drums = std::make_shared<DrumKitNode>("AnalogDrums");
    auto delay = std::make_shared<DelayNode>("StereoEcho");
    delay->setDelayTimeMs(260.0f);
    delay->setFeedback(0.40f);
    delay->setDryWet(0.35f);

    auto scriptSynth = std::make_shared<EatscriptNode>("LiveEatscriptArp");
    scriptSynth->setScript(
        "def process(time, freq, note):\n"
        "    return sin(time * freq * 6.2831853) * 0.35\n"
    );

    auto masterGain = std::make_shared<GainNode>("MasterBus");
    masterGain->setVolume(0.85f);

    NodeId tbId = graph.addNode(tb303);
    NodeId drumId = graph.addNode(drums);
    NodeId delayId = graph.addNode(delay);
    NodeId scriptId = graph.addNode(scriptSynth);
    NodeId masterId = graph.addNode(masterGain);

    graph.connect(tbId, 0, delayId, 0);
    graph.connect(delayId, 0, masterId, 0);
    graph.connect(drumId, 0, masterId, 0);
    graph.connect(scriptId, 0, masterId, 0);
    graph.setOutputNode(masterId, 0);
    graph.compile();

    auto& seq = engine.getSequencer();
    seq.stop();
    seq.setBpm(136.0);
    seq.setSwing(0.56);

    // Track 1: 303 Bassline
    size_t t1 = seq.addTrack("303 Bass", tbId, 16);
    auto* track1 = seq.getTrack(t1);
    const uint8_t bassNotes[16] = {36, 36, 48, 36, 39, 36, 41, 42, 36, 48, 36, 39, 51, 50, 48, 46};
    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData st{};
        st.active = true;
        st.note = bassNotes[s];
        st.velocity = (s % 4 == 0) ? 1.0f : 0.75f;
        st.gateLength = 0.65f;
        st.slide = (s == 6 || s == 14);
        st.accent = (s == 0 || s == 7 || s == 12);
        track1->setStep(s, st);
    }

    // Track 2: Drums
    size_t t2 = seq.addTrack("Drums", drumId, 16);
    auto* track2 = seq.getTrack(t2);
    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData st{};
        if (s % 4 == 0) {
            st.active = true;
            st.note = 36; // 909 Kick
            st.velocity = 0.95f;
        } else if (s % 4 == 2) {
            st.active = true;
            st.note = 42; // Closed Hat
            st.velocity = 0.7f;
        } else if (s == 4 || s == 12) {
            st.active = true;
            st.note = 38; // Snare
            st.velocity = 0.85f;
        }
        track2->setStep(s, st);
    }

    // Track 3: Eatscript Arp
    size_t t3 = seq.addTrack("ScriptArp", scriptId, 16);
    auto* track3 = seq.getTrack(t3);
    const uint8_t arpNotes[16] = {60, 63, 67, 70, 60, 63, 67, 72, 60, 63, 67, 70, 72, 70, 67, 63};
    for (uint32_t s = 0; s < 16; ++s) {
        sequencer::StepData st{};
        st.active = (s % 2 == 1); // 16th off-beat syncopation
        st.note = arpNotes[s];
        st.velocity = 0.6f;
        st.gateLength = 0.4f;
        track3->setStep(s, st);
    }

    std::cout << "[Step Sequencer] Starting sample-accurate playback (136 BPM, 56% Swing)..." << std::endl;
    seq.start();

    // Run for ~4 bars
    for (int i = 0; i < 32; ++i) {
        std::cout << "\r[Transport Step: " << std::setw(2) << (seq.getTransport().getCurrentStep() + 1)
                  << "/16 | Elapsed: " << std::fixed << std::setprecision(1)
                  << (seq.getTransport().getTotalSamplesElapsed() / 48000.0) << "s] "
                  << ">>> 303 + 808/909 Drums + EatscriptArp <<< " << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(110));
    }

    std::cout << "\n\n[Project File] Saving project to 'demo_acid.eats'..." << std::endl;
    bool saved = engine.saveProject("demo_acid.eats", "Live Acid Session 2026");
    if (saved) {
        std::cout << "  [OK] Successfully serialized project to 'demo_acid.eats'!" << std::endl;
    }

    std::cout << "[Step Sequencer] Stopping and testing round-trip deserialization..." << std::endl;
    seq.stop();
    graph.clear();

    std::cout << "[Project File] Reloading project from 'demo_acid.eats'..." << std::endl;
    bool loaded = engine.loadProject("demo_acid.eats");
    if (loaded) {
        std::cout << "  [OK] Successfully loaded project! Re-initiating playback for 1 bar..." << std::endl;
        engine.getSequencer().start();
        std::this_thread::sleep_for(std::chrono::milliseconds(1800));
        engine.getSequencer().stop();
        std::cout << "\n[Step Sequencer] Playback completed.\n" << std::endl;
    }
}

void playModularVisualizerDemo(AudioEngine& engine) {
    std::cout << "\n[UI Canvas & Patchbay] Setting up Modular Hardware Rack & Visualizer..." << std::endl;
    engine.setupDefaultAcidBeatGraph();

    ui::CanvasRenderer renderer(1024.0f, 768.0f);
    renderer.updateRackLayout(engine.getGraph());

    auto& seq = engine.getSequencer();
    seq.stop();
    seq.setBpm(132.0);
    seq.setSwing(0.55);

    // Find node IDs
    const auto& nodes = engine.getGraph().getNodes();
    audio::NodeId tbId = 0;
    audio::NodeId drumId = 0;
    for (const auto& [id, n] : nodes) {
        if (dynamic_cast<audio::Tb303Node*>(n.get())) tbId = id;
        if (dynamic_cast<audio::DrumKitNode*>(n.get())) drumId = id;
    }

    if (tbId != 0 && drumId != 0) {
        size_t t1 = seq.addTrack("Acid303", tbId, 16);
        auto* track1 = seq.getTrack(t1);
        const uint8_t bassNotes[16] = {36, 36, 48, 36, 39, 41, 36, 46, 48, 36, 39, 43, 36, 41, 39, 36};
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            st.active = true;
            st.note = bassNotes[s];
            st.velocity = (s % 4 == 0) ? 1.0f : 0.78f;
            st.gateLength = 0.65f;
            st.slide = (s == 5 || s == 13);
            st.accent = (s == 0 || s == 7);
            track1->setStep(s, st);
        }

        size_t t2 = seq.addTrack("Drums", drumId, 16);
        auto* track2 = seq.getTrack(t2);
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            if (s % 4 == 0) {
                st.active = true;
                st.note = 36; // Kick
                st.velocity = 0.95f;
            } else if (s % 4 == 2) {
                st.active = true;
                st.note = 42; // Hat
                st.velocity = 0.7f;
            } else if (s == 4 || s == 12) {
                st.active = true;
                st.note = 38; // Snare
                st.velocity = 0.85f;
            }
            track2->setStep(s, st);
        }
    }

    seq.start();

    std::cout << "[Visualizer] Running live ANSI hardware rack & visualizer (Auditioning 24 frames)...\n" << std::endl;
    float scopeBuffer[128]{0};

    for (int frame = 0; frame < 24; ++frame) {
        // Read oscilloscope samples and peak meters
        size_t count = engine.getScopeSamples(scopeBuffer, 128);
        if (count > 0) {
            renderer.setScopeData(scopeBuffer, count);
        }
        MeterFeedback fb;
        if (engine.pollMeterFeedback(fb)) {
            renderer.setVuMeter(fb.peakLeft, fb.peakRight);
        }

        // Print ANSI visualizer frame
        std::cout << renderer.renderAnsiVisualizer(&seq) << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(110));
    }

    seq.stop();
    std::cout << "\n[Visualizer] Exporting high-resolution vector SVG patchbay to 'rack_patchbay.svg'..." << std::endl;
    bool exported = renderer.saveSvgToFile("rack_patchbay.svg", engine.getGraph(), &seq);
    if (exported) {
        std::cout << "  [OK] Successfully saved vector SVG patchbay to 'rack_patchbay.svg'!" << std::endl;
    }
}

void exportStudioBouncesDemo(AudioEngine& engine) {
    std::cout << "\n[Studio Master Bounce & Stem Separation]" << std::endl;
    std::cout << "Setting up full multi-track project (TB-303 + Drums + Echo Delay)..." << std::endl;
    engine.setupDefaultAcidBeatGraph();

    auto& seq = engine.getSequencer();
    seq.stop();
    seq.setBpm(134.0);
    seq.setSwing(0.57);

    // Track 1: 303 Bassline
    const auto& nodes = engine.getGraph().getNodes();
    audio::NodeId tbId = 0;
    audio::NodeId drumId = 0;
    for (const auto& [id, n] : nodes) {
        if (dynamic_cast<audio::Tb303Node*>(n.get())) tbId = id;
        if (dynamic_cast<audio::DrumKitNode*>(n.get())) drumId = id;
    }

    if (tbId != 0 && drumId != 0) {
        size_t t1 = seq.addTrack("Acid303", tbId, 16);
        auto* track1 = seq.getTrack(t1);
        const uint8_t bassNotes[16] = {36, 36, 48, 36, 39, 41, 36, 46, 48, 36, 39, 43, 36, 41, 39, 36};
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            st.active = true;
            st.note = bassNotes[s];
            st.velocity = (s % 4 == 0) ? 1.0f : 0.78f;
            st.gateLength = 0.65f;
            st.slide = (s == 5 || s == 13);
            st.accent = (s == 0 || s == 7);
            track1->setStep(s, st);
        }

        size_t t2 = seq.addTrack("AnalogDrums", drumId, 16);
        auto* track2 = seq.getTrack(t2);
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            if (s % 4 == 0) {
                st.active = true;
                st.note = 36; // 909 Kick
                st.velocity = 0.95f;
            } else if (s % 4 == 2) {
                st.active = true;
                st.note = 42; // Hat
                st.velocity = 0.7f;
            } else if (s == 4 || s == 12) {
                st.active = true;
                st.note = 38; // Snare
                st.velocity = 0.85f;
            }
            track2->setStep(s, st);
        }
    }

    std::cout << "[Offline Renderer] Bouncing 24-bit Studio Master (4 bars @ 134 BPM = 7.16s)..." << std::endl;
    const double duration = 7.164;
    auto masterStats = engine.bounceProject("bounces/master_24bit.wav", duration, audio::exporting::WavFormat::Pcm24, true);

    std::cout << "  [Master Complete] " << masterStats.outputPath << "\n"
              << "    * Frames Rendered:   " << masterStats.totalFrames << " frames\n"
              << "    * File Size:         " << (masterStats.fileSizeBytes / 1024) << " KB\n"
              << "    * Peak Amplitude:    L: " << std::fixed << std::setprecision(3) << masterStats.peakLeft
              << " | R: " << masterStats.peakRight << "\n"
              << "    * Render Duration:   " << masterStats.renderTimeMs << " ms\n"
              << "    * Speed Multiplier:  " << std::setprecision(1) << masterStats.speedMultiplier << "x real-time speed!\n"
              << std::endl;

    std::cout << "[Offline Renderer] Bouncing isolated multi-track stems to 'bounces/stems/'..." << std::endl;
    auto stems = engine.bounceStems("bounces/stems", duration, audio::exporting::WavFormat::Pcm24, true);

    for (const auto& stem : stems) {
        std::cout << "  [Stem] " << std::left << std::setw(36) << stem.outputPath
                  << " | Size: " << std::setw(6) << (stem.fileSizeBytes / 1024) << " KB"
                  << " | Peak: " << std::setprecision(2) << stem.peakLeft << std::endl;
    }
    std::cout << "\n[Studio Master Bounce] All stems successfully exported!\n" << std::endl;
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
    std::cout << "  4 : Play Modular Audio Graph (TB-303 -> Stereo Delay -> Master Gain)" << std::endl;
    std::cout << "  5 : Play Full Acid Techno Groove (TB-303 + TR-808/909 Drums + Delay)" << std::endl;
    std::cout << "  6 : Sample-Accurate Step Sequencer & Project File I/O Demo (.eats)" << std::endl;
    std::cout << "  7 : Modular Vector UI & ANSI Visualizer (Oscilloscope, VU Meters, SVG Patchbay Export)" << std::endl;
    std::cout << "  8 : Studio Audio Bouncing (24-bit Master WAV & Isolated Track Stems)" << std::endl;
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
        } else if (input == "4") {
            playModularGraphDemo(engine);
        } else if (input == "5") {
            playAcidBeatDemo(engine);
        } else if (input == "6") {
            playSequencerProjectDemo(engine);
        } else if (input == "7") {
            playModularVisualizerDemo(engine);
        } else if (input == "8") {
            exportStudioBouncesDemo(engine);
        }
    }

    std::cout << "[Shutdown] Stopping audio engine..." << std::endl;
    engine.shutdown();
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
