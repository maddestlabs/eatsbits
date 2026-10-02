#include "eatsbits/ui/widgets/terminal_console_drawer.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/theme.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

void testTerminalConsoleLifecycleAndLayout() {
    std::cout << "[Test 1/4] TerminalConsoleDrawer Lifecycle, Layout & Resizing..." << std::endl;

    eatsbits::ui::TerminalConsoleDrawer drawer;
    assert(!drawer.isExpanded());
    assert(drawer.getDrawerHeight() == 28.0f); // kPullTabHeight

    // Layout collapsed
    drawer.layout(1280.0f, 760.0f, 1.0f);
    assert(!drawer.isExpanded());
    assert(drawer.getDrawerBounds().w == 1280.0f);
    assert(drawer.getDrawerBounds().y == 760.0f - 28.0f);

    // Expand
    drawer.setExpanded(true);
    assert(drawer.isExpanded());
    drawer.layout(1280.0f, 760.0f, 1.0f);
    assert(drawer.getDrawerHeight() > 200.0f);
    assert(drawer.getGrid().getCols() >= 100);
    assert(drawer.getGrid().getRows() >= 10);

    // Toggle
    drawer.toggleExpanded();
    assert(!drawer.isExpanded());

    std::cout << "  [PASS] Lifecycle and layout calculations verified." << std::endl;
}

void testTerminalAudioHostBinding() {
    std::cout << "[Test 2/4] Live AudioEngine Host ABI Reflection & REPL Commands..." << std::endl;

    eatsbits::audio::AudioEngine engine;
    eatsbits::audio::AudioEngineConfig config{};
    config.sampleRate = 44100;
    config.bufferFrameSize = 128;
    engine.initialize(config);

    eatsbits::ui::TerminalConsoleDrawer drawer;
    drawer.bindAudioEngine(&engine);

    // Default BPM
    assert(std::abs(engine.getSequencer().getBpm() - 120.0f) < 0.1f);

    // Execute Eatscript command mutating host engine
    drawer.executeCommand("engine_set_bpm(145.0)");
    assert(std::abs(engine.getSequencer().getBpm() - 145.0f) < 0.1f);

    // Start playback via REPL
    assert(!engine.getSequencer().isPlaying());
    drawer.executeCommand("engine_play()");
    assert(engine.getSequencer().isPlaying());

    // Stop playback via REPL
    drawer.executeCommand("engine_stop()");
    assert(!engine.getSequencer().isPlaying());

    // Trigger note on
    drawer.executeCommand("engine_note_on(60, 0.95)");

    std::cout << "  [PASS] Bidirectional REPL <-> AudioEngine host reflection verified." << std::endl;
}

void testTerminalInputHandling() {
    std::cout << "[Test 3/4] Keyboard, Character & Pointer Event Routing..." << std::endl;

    eatsbits::ui::TerminalConsoleDrawer drawer;
    drawer.layout(1280.0f, 760.0f);

    // Test pointer click on pull tab
    auto pullTab = drawer.getPullTabBounds();
    eatsbits::ui::PointerEvent clickTab{};
    clickTab.action = eatsbits::ui::PointerAction::Down;
    clickTab.x = pullTab.center().x;
    clickTab.y = pullTab.center().y;
    [[maybe_unused]] bool handledTab = drawer.handlePointer(clickTab);
    assert(handledTab);
    assert(drawer.isExpanded());

    // Type character 'v' 'a' 'r' ' ' 'x' '=' '1' '0'
    for (char c : std::string("var x = 10")) {
        [[maybe_unused]] bool charHandled = drawer.handleChar(static_cast<char32_t>(c));
        assert(charHandled);
    }

    // Press Backspace twice
    drawer.handleKey(259, 0, 1, 0); // Backspace '0'
    drawer.handleKey(259, 0, 1, 0); // Backspace '1'

    // Type '4' '2'
    drawer.handleChar('4');
    drawer.handleChar('2');

    // Press Enter to evaluate
    drawer.handleKey(257, 0, 1, 0); // Enter

    // Now query x in REPL
    drawer.executeCommand("x * 2");
    auto res = drawer.getRepl().feedLine("x * 2");
    assert(res.status == eatsbits::eatscript::ReplResult::Status::Complete);
    assert(res.resultValue.isNumber());
    assert(res.resultValue.asNumber() == 84.0);

    // Test history
    drawer.getRepl().addHistory("x * 2");
    std::string histPrev = drawer.getRepl().historyPrev();
    assert(histPrev == "x * 2");

    std::cout << "  [PASS] Pointer, character, backspace, and history routing verified." << std::endl;
}

void testTerminalWebGpuRendering() {
    std::cout << "[Test 4/4] Monospace Hardware-Accelerated Rendering Pass..." << std::endl;

    eatsbits::ui::TerminalConsoleDrawer drawer;
    drawer.setExpanded(true);
    drawer.layout(800.0f, 600.0f);

    eatsbits::ui::BatchRenderer2D renderer;
    [[maybe_unused]] bool initOk = renderer.initialize(nullptr, 800, 600, eatsbits::ui::RenderBackendType::Filament);
    assert(initOk);
    renderer.initDefaultMonospaceAtlas();

    eatsbits::ui::ThemeTokens theme{};
    eatsbits::FrameTimeContext time{0.0, 0.0166, 0, false};

    renderer.beginFrame(800.0f, 600.0f);
    drawer.update(time);
    drawer.render(renderer, theme, 1.0f);
    renderer.endFrame();

    assert(renderer.getFramebuffer() != nullptr);
    assert(renderer.getVertexCount() > 0);

    std::cout << "  [PASS] Headless WebGPU terminal rendering pass verified." << std::endl;
}

int main() {
    std::cout << "============================================================" << std::endl;
    std::cout << " Running Eatsbits Terminal Console & REPL Drawer Tests" << std::endl;
    std::cout << "============================================================" << std::endl;

    testTerminalConsoleLifecycleAndLayout();
    testTerminalAudioHostBinding();
    testTerminalInputHandling();
    testTerminalWebGpuRendering();

    std::cout << "============================================================" << std::endl;
    std::cout << " All Terminal Console Drawer Tests PASSED!" << std::endl;
    std::cout << "============================================================" << std::endl;
    return 0;
}
