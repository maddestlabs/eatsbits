#include <iostream>
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/clap_audio_node.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include "eatsbits/ui/gui_window.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#endif

using namespace eatsbits;
using namespace eatsbits::audio;
using namespace eatsbits::ui;

#if defined(__EMSCRIPTEN__)
static std::unique_ptr<AudioEngine> g_webEngine;
static std::unique_ptr<GuiWindow> g_webWindow;
#endif

int main(int argc, char** argv);

#if defined(_WIN32) && defined(_MSC_VER)
int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) {
    return main(__argc, __argv);
}
#endif

int main(int /*argc*/, char** /*argv*/) {
#if !defined(__EMSCRIPTEN__)
    ClapAudioNode::setMainThreadId(std::this_thread::get_id());
#endif
#if defined(_WIN32)
    // If launched from an existing terminal/PowerShell/cmd prompt, attach to it so stdout/stderr is visible.
    // If launched by double-clicking in Explorer or from a desktop shortcut, no terminal window is spawned.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        std::ios::sync_with_stdio(true);
    }
#endif

    std::cout << "=====================================================" << std::endl;
    std::cout << " Eatsbits Modular Workstation (GLFW + WebGPU / BatchRenderer2D)" << std::endl;
    std::cout << "=====================================================" << std::endl;

#if defined(__EMSCRIPTEN__)
    g_webEngine = std::make_unique<AudioEngine>();
    AudioEngine& engine = *g_webEngine;
#else
    AudioEngine engine;
#endif
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
#if defined(__EMSCRIPTEN__)
    cfg.bufferFrameSize = 1024; // 21.3ms buffer prevents WebAudio scheduler underruns
#else
    cfg.bufferFrameSize = 128;
#endif

    std::cout << "[Audio] Initializing real-time audio engine..." << std::endl;
    bool audioOk = engine.initialize(cfg);
    if (!audioOk) {
        std::cerr << "[Audio] Warning: Failed to open hardware audio output. Running in headless audio mode." << std::endl;
    }

    std::cout << "[Modular] Configuring DX7 Rhodes + Concert Grand base song..." << std::endl;
    engine.setupDefaultRhodesGrandGraph();
    std::cout << "[Modular] DX7 Rhodes + Concert Grand graph configured." << std::endl;

    auto& seq = engine.getSequencer();
    seq.setBpm(116.0);
    seq.setSwing(0.52);

    // 16-bar project chord progression: C - G - Am - F - C - G - Am - F
    std::vector<theory::ChordEvent> chords = {
        {"chord_0",  0,  2.0f, 0, theory::ChordQuality::Major, -1},  // Bar 1-2: C Major
        {"chord_2",  2,  2.0f, 7, theory::ChordQuality::Major, -1},  // Bar 3-4: G Major
        {"chord_4",  4,  2.0f, 9, theory::ChordQuality::Minor, -1},  // Bar 5-6: A Minor
        {"chord_6",  6,  2.0f, 5, theory::ChordQuality::Major, -1},  // Bar 7-8: F Major
        {"chord_8",  8,  2.0f, 0, theory::ChordQuality::Major, -1},  // Bar 9-10: C Major
        {"chord_10", 10, 2.0f, 7, theory::ChordQuality::Major, -1},  // Bar 11-12: G Major
        {"chord_12", 12, 2.0f, 9, theory::ChordQuality::Minor, -1},  // Bar 13-14: A Minor
        {"chord_14", 14, 2.0f, 5, theory::ChordQuality::Major, -1}   // Bar 15-16: F Major
    };
    seq.setChordTrack(chords);

    // Locate node IDs
    const auto& nodes = engine.getGraph().getNodes();
    audio::NodeId dx7Id = 0;
    audio::NodeId grandId = 0;
    for (const auto& [id, n] : nodes) {
        if (!n) continue;
        if (n->getName() == "Dx7Rhodes") dx7Id = id;
        else if (n->getName() == "ConcertGrand") grandId = id;
    }

    // Track 1: DX7 Rhodes (128 steps / 8 bars, polyphonic comping chords synced to chord track)
    if (dx7Id != 0) {
        size_t t0 = seq.addTrack("DX7 Rhodes", dx7Id, 128);
        auto* tr0 = seq.getTrack(t0);
        tr0->setChordFollowMode(theory::ChordFollowMode::Chord);

        // 8 bars of lush comping chords (16 steps per bar)
        // Bar 1 (steps 0..15): Comping hits on steps 0, 6, 10
        sequencer::StepData h0{};
        h0.active = true; h0.note = 48; h0.extraNotes = {60, 64, 67, 71}; h0.velocity = 0.85f; h0.gateLength = 5.5f;
        tr0->setStep(0, h0);
        sequencer::StepData h1{};
        h1.active = true; h1.note = 60; h1.extraNotes = {64, 67, 71}; h1.velocity = 0.70f; h1.gateLength = 3.5f;
        tr0->setStep(6, h1);
        sequencer::StepData h2{};
        h2.active = true; h2.note = 60; h2.extraNotes = {64, 67}; h2.velocity = 0.75f; h2.gateLength = 5.5f;
        tr0->setStep(10, h2);

        // Bar 2 (steps 16..31): Comping hits on steps 16, 22, 28
        sequencer::StepData h3{};
        h3.active = true; h3.note = 48; h3.extraNotes = {60, 64, 67}; h3.velocity = 0.80f; h3.gateLength = 5.5f;
        tr0->setStep(16, h3);
        sequencer::StepData h4{};
        h4.active = true; h4.note = 64; h4.extraNotes = {67, 71}; h4.velocity = 0.70f; h4.gateLength = 5.5f;
        tr0->setStep(22, h4);
        sequencer::StepData h5{};
        h5.active = true; h5.note = 60; h5.extraNotes = {64, 67, 71}; h5.velocity = 0.75f; h5.gateLength = 3.5f;
        tr0->setStep(28, h5);

        // Bar 3 (steps 32..47)
        sequencer::StepData h6{};
        h6.active = true; h6.note = 48; h6.extraNotes = {60, 64, 67, 71}; h6.velocity = 0.85f; h6.gateLength = 5.5f;
        tr0->setStep(32, h6);
        sequencer::StepData h7{};
        h7.active = true; h7.note = 60; h7.extraNotes = {64, 67, 71}; h7.velocity = 0.72f; h7.gateLength = 3.5f;
        tr0->setStep(38, h7);
        sequencer::StepData h8{};
        h8.active = true; h8.note = 60; h8.extraNotes = {64, 67}; h8.velocity = 0.78f; h8.gateLength = 5.5f;
        tr0->setStep(42, h8);

        // Bar 4 (steps 48..63)
        sequencer::StepData h9{};
        h9.active = true; h9.note = 48; h9.extraNotes = {60, 64, 67}; h9.velocity = 0.80f; h9.gateLength = 5.5f;
        tr0->setStep(48, h9);
        sequencer::StepData h10{};
        h10.active = true; h10.note = 64; h10.extraNotes = {67, 71}; h10.velocity = 0.70f; h10.gateLength = 5.5f;
        tr0->setStep(54, h10);
        sequencer::StepData h11{};
        h11.active = true; h11.note = 60; h11.extraNotes = {64, 67, 71}; h11.velocity = 0.76f; h11.gateLength = 3.5f;
        tr0->setStep(60, h11);

        // Bar 5 (steps 64..79)
        sequencer::StepData h12{};
        h12.active = true; h12.note = 48; h12.extraNotes = {60, 64, 67, 71}; h12.velocity = 0.85f; h12.gateLength = 5.5f;
        tr0->setStep(64, h12);
        sequencer::StepData h13{};
        h13.active = true; h13.note = 60; h13.extraNotes = {64, 67, 71}; h13.velocity = 0.70f; h13.gateLength = 3.5f;
        tr0->setStep(70, h13);
        sequencer::StepData h14{};
        h14.active = true; h14.note = 60; h14.extraNotes = {64, 67}; h14.velocity = 0.75f; h14.gateLength = 5.5f;
        tr0->setStep(74, h14);

        // Bar 6 (steps 80..95)
        sequencer::StepData h15{};
        h15.active = true; h15.note = 48; h15.extraNotes = {60, 64, 67}; h15.velocity = 0.80f; h15.gateLength = 5.5f;
        tr0->setStep(80, h15);
        sequencer::StepData h16{};
        h16.active = true; h16.note = 64; h16.extraNotes = {67, 71}; h16.velocity = 0.70f; h16.gateLength = 5.5f;
        tr0->setStep(86, h16);
        sequencer::StepData h17{};
        h17.active = true; h17.note = 60; h17.extraNotes = {64, 67, 71}; h17.velocity = 0.75f; h17.gateLength = 3.5f;
        tr0->setStep(92, h17);

        // Bar 7 (steps 96..111)
        sequencer::StepData h18{};
        h18.active = true; h18.note = 48; h18.extraNotes = {60, 64, 67, 71}; h18.velocity = 0.85f; h18.gateLength = 5.5f;
        tr0->setStep(96, h18);
        sequencer::StepData h19{};
        h19.active = true; h19.note = 60; h19.extraNotes = {64, 67, 71}; h19.velocity = 0.72f; h19.gateLength = 3.5f;
        tr0->setStep(102, h19);
        sequencer::StepData h20{};
        h20.active = true; h20.note = 60; h20.extraNotes = {64, 67}; h20.velocity = 0.78f; h20.gateLength = 5.5f;
        tr0->setStep(106, h20);

        // Bar 8 (steps 112..127)
        sequencer::StepData h21{};
        h21.active = true; h21.note = 48; h21.extraNotes = {60, 64, 67}; h21.velocity = 0.80f; h21.gateLength = 5.5f;
        tr0->setStep(112, h21);
        sequencer::StepData h22{};
        h22.active = true; h22.note = 64; h22.extraNotes = {67, 71}; h22.velocity = 0.70f; h22.gateLength = 5.5f;
        tr0->setStep(118, h22);
        sequencer::StepData h23{};
        h23.active = true; h23.note = 60; h23.extraNotes = {64, 67, 71}; h23.velocity = 0.80f; h23.gateLength = 3.5f;
        tr0->setStep(124, h23);
    }

    // Track 2: Concert Grand (128 steps / 8 bars, expressive arpeggios and piano accompaniment)
    if (grandId != 0) {
        size_t t1 = seq.addTrack("Concert Grand", grandId, 128);
        auto* tr1 = seq.getTrack(t1);
        tr1->setChordFollowMode(theory::ChordFollowMode::Chord);

        // Broken chord arpeggiated accompaniment pattern across 8 bars
        const struct { uint32_t s; uint8_t n; float v; float g; } grandSteps[] = {
            // Bar 1
            {0, 36, 0.85f, 1.9f}, {2, 60, 0.70f, 1.8f}, {4, 64, 0.75f, 1.8f}, {6, 67, 0.78f, 1.8f},
            {8, 72, 0.80f, 1.8f}, {10, 67, 0.72f, 1.8f}, {12, 64, 0.74f, 1.8f}, {14, 60, 0.70f, 1.8f},
            // Bar 2
            {16, 36, 0.82f, 1.9f}, {18, 64, 0.72f, 1.8f}, {20, 67, 0.76f, 1.8f}, {22, 71, 0.82f, 1.8f},
            {24, 76, 0.85f, 1.8f}, {26, 72, 0.75f, 1.8f}, {28, 67, 0.72f, 1.8f}, {30, 64, 0.68f, 1.8f},
            // Bar 3
            {32, 36, 0.88f, 1.9f}, {34, 60, 0.70f, 1.8f}, {36, 64, 0.75f, 1.8f}, {38, 67, 0.78f, 1.8f},
            {40, 72, 0.82f, 1.8f}, {42, 67, 0.74f, 1.8f}, {44, 64, 0.72f, 1.8f}, {46, 60, 0.70f, 1.8f},
            // Bar 4
            {48, 36, 0.84f, 1.9f}, {50, 64, 0.72f, 1.8f}, {52, 67, 0.76f, 1.8f}, {54, 71, 0.80f, 1.8f},
            {56, 76, 0.86f, 1.8f}, {58, 72, 0.75f, 1.8f}, {60, 67, 0.74f, 1.8f}, {62, 64, 0.70f, 1.8f},
            // Bar 5
            {64, 36, 0.88f, 1.9f}, {66, 60, 0.72f, 1.8f}, {68, 64, 0.76f, 1.8f}, {70, 67, 0.80f, 1.8f},
            {72, 72, 0.85f, 1.8f}, {74, 67, 0.75f, 1.8f}, {76, 64, 0.72f, 1.8f}, {78, 60, 0.70f, 1.8f},
            // Bar 6
            {80, 36, 0.85f, 1.9f}, {82, 64, 0.74f, 1.8f}, {84, 67, 0.78f, 1.8f}, {86, 71, 0.82f, 1.8f},
            {88, 76, 0.88f, 1.8f}, {90, 72, 0.76f, 1.8f}, {92, 67, 0.74f, 1.8f}, {94, 64, 0.70f, 1.8f},
            // Bar 7
            {96, 36, 0.90f, 1.9f}, {98, 60, 0.72f, 1.8f}, {100, 64, 0.76f, 1.8f}, {102, 67, 0.80f, 1.8f},
            {104, 72, 0.86f, 1.8f}, {106, 67, 0.75f, 1.8f}, {108, 64, 0.72f, 1.8f}, {110, 60, 0.70f, 1.8f},
            // Bar 8
            {112, 36, 0.86f, 1.9f}, {114, 64, 0.74f, 1.8f}, {116, 67, 0.78f, 1.8f}, {118, 71, 0.84f, 1.8f},
            {120, 76, 0.90f, 1.8f}, {122, 72, 0.78f, 1.8f}, {124, 67, 0.75f, 1.8f}, {126, 64, 0.72f, 1.8f}
        };
        for (const auto& gs : grandSteps) {
            sequencer::StepData st{};
            st.active = true;
            st.note = gs.n;
            st.velocity = gs.v;
            st.gateLength = gs.g;
            tr1->setStep(gs.s, st);
        }
    }

    // Audio sequencer starts in stopped state; user initiates playback via Play transport button or Spacebar
    std::cout << "[Sequencer] Sequencer initialized and ready (stopped)." << std::endl;

    std::cout << "[GUI] Instantiating GuiWindow..." << std::endl;
#if defined(__EMSCRIPTEN__)
    g_webWindow = std::make_unique<GuiWindow>(1280, 800, "Eatsbits Modular Synthesizer Workstation (WebGPU)");
    GuiWindow& window = *g_webWindow;
#else
    GuiWindow window(1280, 800, "Eatsbits Modular Synthesizer Workstation (WebGPU)");
#endif
    std::cout << "[GUI] Initializing GuiWindow with engine..." << std::endl;
    window.initialize(engine);

    if (audioOk) {
        std::cout << "[Audio] Starting real-time audio hardware output..." << std::endl;
        engine.start();
    }

    std::cout << "[GUI] Running main event loop." << std::endl;
    window.runEventLoop();

#if !defined(__EMSCRIPTEN__)
    std::cout << "[Shutdown] Exiting application..." << std::endl;
    seq.stop();
    engine.shutdown();
#endif
    return 0;
}
