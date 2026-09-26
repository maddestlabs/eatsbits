#include <iostream>
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
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
    cfg.bufferFrameSize = 512; // 10.6ms buffer prevents WebAudio scheduler underruns
#else
    cfg.bufferFrameSize = 128;
#endif

    std::cout << "[Audio] Initializing real-time audio engine..." << std::endl;
    if (!engine.initialize(cfg)) {
        std::cerr << "[Audio] Warning: Failed to open hardware audio output. Running in headless audio mode." << std::endl;
    } else {
        engine.start();
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
    audio::NodeId drumsId = 0;
    for (const auto& [id, n] : nodes) {
        if (dynamic_cast<audio::Tb303Node*>(n.get())) tbId = id;
        else if (dynamic_cast<audio::DrumKitNode*>(n.get())) drumsId = id;
    }

    // Track 1: TB-303 Acid Bass
    if (tbId != 0) {
        size_t t1 = seq.addTrack("303 Bass", tbId, 16);
        auto* tr1 = seq.getTrack(t1);
        const uint8_t bassNotes[16] = {36, 36, 48, 36, 39, 41, 36, 46, 48, 36, 39, 43, 36, 41, 39, 36};
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            st.active = true;
            st.note = bassNotes[s];
            st.velocity = (s % 4 == 0) ? 1.0f : 0.78f;
            st.gateLength = 0.65f;
            st.slide = (s == 5 || s == 13);
            st.accent = (s == 0 || s == 7);
            tr1->setStep(s, st);
        }
    }

    // Track 2: TR-808 Drums
    if (drumsId != 0) {
        size_t t2 = seq.addTrack("808 Drums", drumsId, 16);
        auto* tr2 = seq.getTrack(t2);
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            if (s == 0 || s == 8) {
                // 808 Kick downbeats (note 35)
                st.active = true;
                st.note = 35;
                st.velocity = 0.95f;
                st.gateLength = 0.8f;
                st.accent = (s == 0);
            } else if (s == 10) {
                // Syncopated 808 Kick (note 35)
                st.active = true;
                st.note = 35;
                st.velocity = 0.85f;
                st.gateLength = 0.7f;
            } else if (s == 4 || s == 12) {
                // 808 Snare on beats 2 & 4 (note 38)
                st.active = true;
                st.note = 38;
                st.velocity = 0.90f;
                st.gateLength = 0.6f;
            } else if (s % 2 == 1) {
                // 808 Closed Hi-Hat on off-beats (note 42)
                st.active = true;
                st.note = 42;
                st.velocity = 0.65f;
                st.gateLength = 0.3f;
            }
            if (st.active) {
                tr2->setStep(s, st);
            }
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

    std::cout << "[GUI] Running main event loop." << std::endl;
    window.runEventLoop();

#if !defined(__EMSCRIPTEN__)
    std::cout << "[Shutdown] Exiting application..." << std::endl;
    seq.stop();
    engine.shutdown();
#endif
    return 0;
}
