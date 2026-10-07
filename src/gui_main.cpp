#include <iostream>
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
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
    std::cout << " Eatsbits Modular Workstation (GLFW + Filament/NanoVG)" << std::endl;
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

    std::cout << "[Modular] Configuring TB-303 Acid + TR-808 Drums base song..." << std::endl;
    engine.setupDefaultAcidBeatGraph();
    std::cout << "[Modular] TB-303 + TR-808 Acid Beat graph configured." << std::endl;

    auto& seq = engine.getSequencer();
    seq.setBpm(132.0);
    seq.setSwing(0.55);

    // Locate node IDs
    const auto& nodes = engine.getGraph().getNodes();
    audio::NodeId tbId = 0;
    audio::NodeId drums808Id = 0;
    audio::NodeId drums909Id = 0;
    audio::NodeId dx7Id = 0;
    audio::NodeId grandId = 0;
    for (const auto& [id, n] : nodes) {
        if (!n) continue;
        if (n->getName() == "Tb303") tbId = id;
        else if (n->getName() == "Drums808") drums808Id = id;
        else if (n->getName() == "Drums909") drums909Id = id;
        else if (n->getName() == "Dx7Rhodes") dx7Id = id;
        else if (n->getName() == "ConcertGrand") grandId = id;
        else if (drums808Id == 0 && dynamic_cast<audio::DrumKitNode*>(n.get())) drums808Id = id;
        else if (dx7Id == 0 && dynamic_cast<audio::PolySynthNode*>(n.get())) dx7Id = id;
    }

    // Track 1: TB-303 Acid Bass
    if (tbId != 0) {
        size_t t1 = seq.addTrack("303 Acid Bass", tbId, 64);
        auto* tr1 = seq.getTrack(t1);
        const uint8_t bassNotes[16] = {36, 36, 48, 36, 39, 41, 36, 46, 48, 36, 39, 43, 36, 41, 39, 36};
        for (uint32_t s = 0; s < 64; ++s) {
            uint32_t patIdx = s % 16;
            sequencer::StepData st{};
            st.active = true;
            st.note = bassNotes[patIdx];
            st.velocity = (patIdx % 4 == 0) ? 1.0f : 0.78f;
            st.gateLength = 0.65f;
            st.slide = (patIdx == 5 || patIdx == 13);
            st.accent = (patIdx == 0 || patIdx == 7);
            tr1->setStep(s, st);
        }
    }

    // Track 2: TR-808 Kit
    if (drums808Id != 0) {
        size_t t2 = seq.addTrack("TR-808 Kit", drums808Id, 64);
        auto* tr2 = seq.getTrack(t2);
        for (uint32_t s = 0; s < 64; ++s) {
            uint32_t patIdx = s % 16;
            sequencer::StepData st{};
            if (patIdx == 0 || patIdx == 8) {
                st.active = true;
                st.note = 36; // 808 Kick
                st.velocity = 0.95f;
                st.gateLength = 0.8f;
                st.accent = (patIdx == 0);
                st.extraNotes = {42}; // Kick + Hat
            } else if (patIdx == 7 || patIdx == 10) {
                st.active = true;
                st.note = 36; // Syncopated kick
                st.velocity = 0.85f;
                st.gateLength = 0.7f;
            } else if (patIdx == 4 || patIdx == 12) {
                st.active = true;
                st.note = 38; // 808 Snare
                st.velocity = 0.90f;
                st.gateLength = 0.6f;
                st.extraNotes = {42}; // Snare + Hat
            } else if (patIdx == 15) {
                st.active = true;
                st.note = 46; // Open Hat
                st.velocity = 0.75f;
                st.gateLength = 0.8f;
            } else if (patIdx % 2 == 1 || patIdx % 2 == 0) {
                st.active = true;
                st.note = 42; // Closed Hat
                st.velocity = 0.60f;
                st.gateLength = 0.3f;
            }
            if (st.active) {
                tr2->setStep(s, st);
            }
        }
    }

    // Track 3: TR-909 Drive
    if (drums909Id != 0) {
        size_t t3 = seq.addTrack("TR-909 Drive", drums909Id, 64);
        auto* tr3 = seq.getTrack(t3);
        for (uint32_t s = 0; s < 64; ++s) {
            uint32_t patIdx = s % 16;
            sequencer::StepData st{};
            if (patIdx % 4 == 0) { // 909 four-on-the-floor
                st.active = true;
                st.note = 36;
                st.velocity = 1.0f;
                st.gateLength = 0.8f;
            } else if (patIdx == 4 || patIdx == 12) {
                st.active = true;
                st.note = 38; // 909 Snare
                st.velocity = 0.95f;
                st.gateLength = 0.7f;
            } else if (patIdx % 2 == 1) {
                st.active = true;
                st.note = 42; // 909 Hat
                st.velocity = 0.70f;
                st.gateLength = 0.35f;
            }
            if (st.active) tr3->setStep(s, st);
        }
    }

    // Track 4: DX7 Rhodes (Polyphonic 4-Chord Progression: Cm7, Bbmaj7, Abmaj7, Bb7)
    if (dx7Id != 0) {
        size_t t4 = seq.addTrack("DX7 Rhodes", dx7Id, 128);
        auto* tr4 = seq.getTrack(t4);
        // Bar 1..2 (steps 0..31): Cm7 {60, 63, 67, 70}
        sequencer::StepData c0{};
        c0.active = true;
        c0.note = 60;
        c0.extraNotes = {63, 67, 70};
        c0.velocity = 0.75f;
        c0.gateLength = 0.95f;
        tr4->setStep(0, c0);

        // Bar 3..4 (steps 32..63): Bbmaj7 {58, 62, 65, 69}
        sequencer::StepData c1{};
        c1.active = true;
        c1.note = 58;
        c1.extraNotes = {62, 65, 69};
        c1.velocity = 0.75f;
        c1.gateLength = 0.95f;
        tr4->setStep(32, c1);

        // Bar 5..6 (steps 64..95): Abmaj7 {56, 60, 63, 67}
        sequencer::StepData c2{};
        c2.active = true;
        c2.note = 56;
        c2.extraNotes = {60, 63, 67};
        c2.velocity = 0.75f;
        c2.gateLength = 0.95f;
        tr4->setStep(64, c2);

        // Bar 7..8 (steps 96..127): Bb7 {58, 62, 65, 68}
        sequencer::StepData c3{};
        c3.active = true;
        c3.note = 58;
        c3.extraNotes = {62, 65, 68};
        c3.velocity = 0.80f;
        c3.gateLength = 0.95f;
        tr4->setStep(96, c3);
    }

    // Track 5: Concert Grand (Melodic Piano Solo)
    if (grandId != 0) {
        size_t t5 = seq.addTrack("Concert Grand", grandId, 128);
        auto* tr5 = seq.getTrack(t5);
        // Bar 5..8 melodic solo
        const struct { uint32_t s; uint8_t n; float v; } grandNotes[] = {
            {64, 60, 0.85f}, {72, 64, 0.75f}, {80, 67, 0.90f},
            {88, 65, 0.70f}, {96, 64, 0.65f}, {104, 62, 0.80f},
            {112, 67, 0.95f}, {120, 71, 0.85f}
        };
        for (const auto& gn : grandNotes) {
            sequencer::StepData st{};
            st.active = true;
            st.note = gn.n;
            st.velocity = gn.v;
            st.gateLength = 0.85f;
            tr5->setStep(gn.s, st);
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
