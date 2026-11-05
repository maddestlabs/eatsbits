#include "eatsbits/ui/widgets/terminal_console_drawer.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/eatscript/note_script.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

TerminalConsoleDrawer::TerminalConsoleDrawer() {
    DrawerConfig cfg;
    cfg.edge = DockEdge::Bottom;
    cfg.tab.title = ">_ TERMINAL";
    cfg.tab.tabPlacement = DockTabPlacement::End;
    cfg.tab.tabOffset = 16.0f;
    cfg.tab.tabWidth = 150.0f;
    cfg.tab.tabHeight = kPullTabHeight;
    cfg.tab.preferredExtent = kDefaultDrawerHeight;
    cfg.tab.minExtent = 140.0f;
    cfg.tab.maxExtent = 600.0f;
    cfg.tab.resizable = true;
    cfg.tab.showHeaderBar = false;
    cfg.tab.showCloseButton = false;
    drawer_.setConfig(cfg);

    setupHostBindings();
    printWelcomeBanner();
    printPrompt();
    grid_.swapBuffers();
}

TerminalConsoleDrawer::~TerminalConsoleDrawer() = default;

void TerminalConsoleDrawer::bindAudioEngine(audio::AudioEngine* engine) {
    audioEngine_ = engine;
    setupHostBindings();
}

void TerminalConsoleDrawer::setCallbacks(TerminalCallbacks callbacks) noexcept {
    callbacks_ = std::move(callbacks);
    setupHostBindings();
}

void TerminalConsoleDrawer::setupHostBindings() {
    // 0. help() / ?
    hostRegistry_.registerHostFunction("help", EATS_ABI_TYPE_BOOL, {}, [this](const auto&) -> EatsAbiValue {
        printHelpGuide();
        return eats_abi_value_bool(true);
    }, "Show terminal command reference and API guide");

    // 1. engine_play() / play()
    auto playFn = [this](const auto&) -> EatsAbiValue {
        if (audioEngine_) {
            audioEngine_->getSequencer().start();
            return eats_abi_value_bool(true);
        }
        return eats_abi_value_bool(false);
    };
    hostRegistry_.registerHostFunction("engine_play", EATS_ABI_TYPE_BOOL, {}, playFn, "Start sequencer playback");
    hostRegistry_.registerHostFunction("play", EATS_ABI_TYPE_BOOL, {}, playFn, "Start sequencer playback");

    // 2. engine_stop() / stop()
    auto stopFn = [this](const auto&) -> EatsAbiValue {
        if (audioEngine_) {
            audioEngine_->stopPlayback();
            return eats_abi_value_bool(true);
        }
        return eats_abi_value_bool(false);
    };
    hostRegistry_.registerHostFunction("engine_stop", EATS_ABI_TYPE_BOOL, {}, stopFn, "Stop sequencer playback");
    hostRegistry_.registerHostFunction("stop", EATS_ABI_TYPE_BOOL, {}, stopFn, "Stop sequencer playback");

    // 3. engine_is_playing() / is_playing()
    auto isPlayingFn = [this](const auto&) -> EatsAbiValue {
        bool playing = audioEngine_ ? audioEngine_->getSequencer().isPlaying() : false;
        return eats_abi_value_bool(playing);
    };
    hostRegistry_.registerHostFunction("engine_is_playing", EATS_ABI_TYPE_BOOL, {}, isPlayingFn, "Check if sequencer is playing");
    hostRegistry_.registerHostFunction("is_playing", EATS_ABI_TYPE_BOOL, {}, isPlayingFn, "Check if sequencer is playing");

    // 4. engine_get_bpm() / bpm()
    auto getBpmFn = [this](const auto&) -> EatsAbiValue {
        double bpm = audioEngine_ ? audioEngine_->getSequencer().getBpm() : 120.0;
        return eats_abi_value_f64(bpm);
    };
    hostRegistry_.registerHostFunction("engine_get_bpm", EATS_ABI_TYPE_F64, {}, getBpmFn, "Get current sequencer BPM");
    hostRegistry_.registerHostFunction("bpm", EATS_ABI_TYPE_F64, {}, getBpmFn, "Get current sequencer BPM");

    // 5. engine_set_bpm(bpm) / set_bpm(bpm)
    auto setBpmFn = [this](const auto& args) -> EatsAbiValue {
        if (audioEngine_ && !args.empty()) {
            audioEngine_->getSequencer().setBpm(static_cast<float>(args[0].as.f64));
        }
        return eats_abi_value_void();
    };
    hostRegistry_.registerHostFunction("engine_set_bpm", EATS_ABI_TYPE_VOID, {EATS_ABI_TYPE_F64}, setBpmFn, "Set current sequencer BPM");
    hostRegistry_.registerHostFunction("set_bpm", EATS_ABI_TYPE_VOID, {EATS_ABI_TYPE_F64}, setBpmFn, "Set current sequencer BPM");

    // 6. engine_note_on(note, vel)
    hostRegistry_.registerHostFunction("engine_note_on", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_I32, EATS_ABI_TYPE_F64}, [this](const auto& args) -> EatsAbiValue {
        if (audioEngine_ && args.size() >= 2) {
            uint8_t note = static_cast<uint8_t>(args[0].as.i32);
            float vel = static_cast<float>(args[1].as.f64);
            audioEngine_->postNoteOn(note, vel);
            return eats_abi_value_bool(true);
        }
        return eats_abi_value_bool(false);
    }, "Trigger note on event");

    // 7. engine_note_off(note)
    hostRegistry_.registerHostFunction("engine_note_off", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_I32}, [this](const auto& args) -> EatsAbiValue {
        if (audioEngine_ && !args.empty()) {
            uint8_t note = static_cast<uint8_t>(args[0].as.i32);
            audioEngine_->postNoteOff(note);
            return eats_abi_value_bool(true);
        }
        return eats_abi_value_bool(false);
    }, "Trigger note off event");

    // 8. engine_panic() / panic()
    auto panicFn = [this](const auto&) -> EatsAbiValue {
        if (audioEngine_) {
            audioEngine_->panic();
        }
        return eats_abi_value_void();
    };
    hostRegistry_.registerHostFunction("engine_panic", EATS_ABI_TYPE_VOID, {}, panicFn, "Silence all voices");
    hostRegistry_.registerHostFunction("panic", EATS_ABI_TYPE_VOID, {}, panicFn, "Silence all voices");

    // 9. engine_get_tracks() / tracks()
    auto getTracksFn = [this](const auto&) -> EatsAbiValue {
        int count = audioEngine_ ? static_cast<int>(audioEngine_->getSequencer().getNumTracks()) : 0;
        return eats_abi_value_i32(count);
    };
    hostRegistry_.registerHostFunction("engine_get_tracks", EATS_ABI_TYPE_I32, {}, getTracksFn, "Get number of sequencer tracks");
    hostRegistry_.registerHostFunction("tracks", EATS_ABI_TYPE_I32, {}, getTracksFn, "Get number of sequencer tracks");

    // 10. track_create(name, engine) / track_add(name, engine)
    auto trackCreateFn = [this](const auto& args) -> EatsAbiValue {
        if (args.size() >= 2 && callbacks_.onTrackCreate) {
            std::string name = args[0].as.stringVal ? args[0].as.stringVal : "Track";
            std::string engine = args[1].as.stringVal ? args[1].as.stringVal : "synth";
            bool ok = callbacks_.onTrackCreate(name, engine);
            return eats_abi_value_bool(ok);
        }
        return eats_abi_value_bool(false);
    };
    hostRegistry_.registerHostFunction("track_create", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_STRING, EATS_ABI_TYPE_STRING}, trackCreateFn, "Create new track with name and engine type (synth, dx7, drums, sampler)");
    hostRegistry_.registerHostFunction("track_add", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_STRING, EATS_ABI_TYPE_STRING}, trackCreateFn, "Create new track with name and engine type (synth, dx7, drums, sampler)");

    // 11. track_delete(idx) / track_rm(idx)
    auto trackDelFn = [this](const auto& args) -> EatsAbiValue {
        if (!args.empty() && callbacks_.onTrackDelete) {
            uint32_t idx = static_cast<uint32_t>(args[0].as.i32);
            bool ok = callbacks_.onTrackDelete(idx);
            return eats_abi_value_bool(ok);
        }
        return eats_abi_value_bool(false);
    };
    hostRegistry_.registerHostFunction("track_delete", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_I32}, trackDelFn, "Remove track by index");
    hostRegistry_.registerHostFunction("track_rm", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_I32}, trackDelFn, "Remove track by index");

    // 12. macro_run(macro_id) / macro(macro_id)
    auto macroFn = [this](const auto& args) -> EatsAbiValue {
        if (!args.empty() && callbacks_.onMacroRun) {
            std::string id = args[0].as.stringVal ? args[0].as.stringVal : "";
            std::string msg;
            bool ok = callbacks_.onMacroRun(id, msg);
            return eats_abi_value_bool(ok);
        }
        return eats_abi_value_bool(false);
    };
    hostRegistry_.registerHostFunction("macro_run", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_STRING}, macroFn, "Run procedural generation macro (e.g. 'macro_acid_gen', 'macro_song_gen')");
    hostRegistry_.registerHostFunction("macro", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_STRING}, macroFn, "Run procedural generation macro (e.g. 'acid', 'song')");

    // 13. notes_replace(from_pitch, to_pitch)
    hostRegistry_.registerHostFunction("notes_replace", EATS_ABI_TYPE_I32, {EATS_ABI_TYPE_I32, EATS_ABI_TYPE_I32}, [this](const auto& args) -> EatsAbiValue {
        if (args.size() >= 2 && callbacks_.onNotesReplace) {
            int from = args[0].as.i32;
            int to = args[1].as.i32;
            int count = callbacks_.onNotesReplace(from, to, -1);
            return eats_abi_value_i32(count);
        }
        return eats_abi_value_i32(0);
    }, "Search and replace notes across all tracks");

    // 14. view(nameOrIdx)
    hostRegistry_.registerHostFunction("view", EATS_ABI_TYPE_BOOL, {EATS_ABI_TYPE_STRING}, [this](const auto& args) -> EatsAbiValue {
        if (!args.empty() && callbacks_.onNavigateView) {
            std::string v = args[0].as.stringVal ? args[0].as.stringVal : "";
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            int idx = -1;
            if (v == "arranger" || v == "0" || v == "arrangement") idx = 0;
            else if (v == "edit" || v == "tracker" || v == "pianoroll" || v == "1") idx = 1;
            else if (v == "track" || v == "inspector" || v == "2") idx = 2;
            else if (v == "mixer" || v == "3") idx = 3;
            else if (v == "design" || v == "rack" || v == "modular" || v == "4") idx = 4;
            if (idx >= 0) {
                callbacks_.onNavigateView(idx);
                return eats_abi_value_bool(true);
            }
        }
        return eats_abi_value_bool(false);
    }, "Navigate to workspace view ('arranger', 'edit', 'track', 'mixer', 'design')");

    // 15. loop()
    hostRegistry_.registerHostFunction("loop", EATS_ABI_TYPE_BOOL, {}, [this](const auto&) -> EatsAbiValue {
        if (callbacks_.onToggleLoop) {
            bool l = callbacks_.onToggleLoop();
            return eats_abi_value_bool(l);
        }
        return eats_abi_value_bool(false);
    }, "Toggle transport loop cycle");

    // 16. metronome()
    hostRegistry_.registerHostFunction("metronome", EATS_ABI_TYPE_BOOL, {}, [this](const auto&) -> EatsAbiValue {
        if (callbacks_.onToggleMetronome) {
            bool m = callbacks_.onToggleMetronome();
            return eats_abi_value_bool(m);
        }
        return eats_abi_value_bool(false);
    }, "Toggle metronome click");

    // 17. keyboard()
    hostRegistry_.registerHostFunction("keyboard", EATS_ABI_TYPE_VOID, {}, [this](const auto&) -> EatsAbiValue {
        if (callbacks_.onToggleKeyboard) {
            callbacks_.onToggleKeyboard();
        }
        return eats_abi_value_void();
    }, "Toggle piano keyboard drawer");

    // 18. browser()
    hostRegistry_.registerHostFunction("browser", EATS_ABI_TYPE_VOID, {}, [this](const auto&) -> EatsAbiValue {
        if (callbacks_.onToggleBrowser) {
            callbacks_.onToggleBrowser();
        }
        return eats_abi_value_void();
    }, "Toggle project browser drawer");

    // 19. clear()
    hostRegistry_.registerHostFunction("clear", EATS_ABI_TYPE_VOID, {}, [this](const auto&) -> EatsAbiValue {
        clear();
        return eats_abi_value_void();
    }, "Clear terminal screen");

    // Bind to the REPL's evaluator
    hostRegistry_.bindToEvaluator(repl_.getEvaluator());
}

void TerminalConsoleDrawer::printWelcomeBanner() {
    parser_.parse("\x1b[38;2;160;100;255m┌──────────────────────────────────────────────────────────────┐\x1b[0m\r\n");
    parser_.parse("\x1b[38;2;160;100;255m│\x1b[0m  \x1b[1;38;2;255;255;255mEatsbits Interactive WebGPU Terminal & REPL\x1b[0m                 \x1b[38;2;160;100;255m│\x1b[0m\r\n");
    parser_.parse("\x1b[38;2;160;100;255m│\x1b[0m  Type expressions, \x1b[38;2;80;220;255mhelp()\x1b[0m, or \x1b[38;2;100;230;120mengine_get_bpm()\x1b[0m to inspect.  \x1b[38;2;160;100;255m│\x1b[0m\r\n");
    parser_.parse("\x1b[38;2;160;100;255m└──────────────────────────────────────────────────────────────┘\x1b[0m\r\n\r\n");
}

void TerminalConsoleDrawer::printPrompt() {
    std::string prompt = repl_.getCurrentPrompt();
    parser_.parse("\x1b[1;38;2;140;110;250m" + prompt + "\x1b[0m");
}

void TerminalConsoleDrawer::setExpanded(bool exp) noexcept {
    drawer_.setExpanded(exp);
    if (exp) {
        isFocused_ = true;
        cursorBlinkTimer_ = 0.0;
        cursorBlinkState_ = true;
    } else {
        isFocused_ = false;
    }
}

void TerminalConsoleDrawer::clear() {
    grid_.eraseInDisplay(2);
    grid_.setCursorPos(0, 0);
    currentLine_.clear();
    cursorCol_ = 0;
    printWelcomeBanner();
    printPrompt();
    grid_.swapBuffers();
}

void TerminalConsoleDrawer::layout(float screenWidth, float bottomNavTopY, float uiScale, float customTabX, float customTabW) {
    if (uiScale <= 0.001f) uiScale = 1.0f;

    const float scaledCellW = kCellWidth * uiScale;
    const float scaledCellH = kCellHeight * uiScale;

    drawerHeight_ = kDefaultDrawerHeight * uiScale;
    drawer_.setExtent(drawerHeight_);
    drawer_.layout(Rect2D(0.0f, 0.0f, screenWidth, bottomNavTopY), 0.0f);

    float animProg = drawer_.getAnimProgress();
    float currentH = drawerHeight_ * animProg;
    const float topY = bottomNavTopY - currentH - kPullTabHeight;
    drawerBounds_ = {0.0f, bottomNavTopY - currentH, screenWidth, currentH};

    const float tabW = (customTabW > 0.0f) ? (customTabW * uiScale) : (135.0f * uiScale);
    const float tabX = (customTabX >= 0.0f) ? customTabX : (screenWidth - tabW - 16.0f * uiScale);
    pullTabBounds_ = {tabX, topY, tabW, kPullTabHeight};

    if (animProg > 0.05f && currentH > 20.0f) {
        const float termX = 12.0f * uiScale;
        const float termY = (bottomNavTopY - currentH) + 4.0f;
        const float termW = screenWidth - termX * 2.0f;
        const float termH = currentH - 8.0f;
        terminalAreaBounds_ = {termX, termY, termW, termH};

        int cols = std::clamp(static_cast<int>(termW / scaledCellW), 20, 220);
        int rows = std::clamp(static_cast<int>(termH / scaledCellH), 5, 80);

        if (cols != grid_.getCols() || rows != grid_.getRows()) {
            grid_.resize(cols, rows);
        }
    }
}

void TerminalConsoleDrawer::update(const FrameTimeContext& time) noexcept {
    drawer_.update(static_cast<float>(time.deltaTime));
    if (!drawer_.isExpanded()) return;

    cursorBlinkTimer_ += time.deltaTime;
    cursorBlinkState_ = (std::fmod(cursorBlinkTimer_, 0.8) < 0.4);

    grid_.swapBuffers();
}

void TerminalConsoleDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme, float uiScale) {
    if (uiScale <= 0.001f) uiScale = 1.0f;

    const float tabX = pullTabBounds_.x;
    const float tabY = pullTabBounds_.y;
    const float tabW = pullTabBounds_.w;
    const float tabH = pullTabBounds_.h;

    // 1. Pull Tab Header - matches Virtual Piano tab geometry and styling exactly
    const auto& tabAccent = drawer_.isExpanded() ? theme.secondaryAccent : theme.primaryAccent;
    r.drawRoundedRectGradient(tabX, tabY, tabW, tabH + 4.0f, 4.0f,
                              theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 1.0f,
                              theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 1.0f);
    r.drawRoundedRectOutline(tabX, tabY, tabW, tabH + 4.0f, 4.0f,
                             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 1.0f, 1.0f);

    // Centered label without up/down arrow, exactly matching virtual keyboard tab sizing and aesthetics
    drawCenteredText(r, ">_ TERMINAL", tabX, tabY, tabW, tabH, 10.5f, tabAccent);

    float animProg = drawer_.getAnimProgress();
    if (animProg <= 0.01f) return;

    // 2. Main Terminal Acrylic Drawer Background
    const float termX = drawerBounds_.x;
    const float termY = drawerBounds_.y;
    const float termW = drawerBounds_.w;
    const float termH = drawerBounds_.h;

    r.drawRect(termX, termY, termW, termH, 0.06f, 0.07f, 0.09f, 0.98f);
    r.drawLine(termX, termY, termX + termW, termY, 0.65f, 0.35f, 0.95f, 1.0f, 1.5f);

    if (animProg <= 0.15f || termH <= 30.0f) return;

    // 3. Render WebGPU Monospace Terminal Grid
    const float scaledCellW = kCellWidth * uiScale;
    const float scaledCellH = kCellHeight * uiScale;

    r.drawTerminalGrid(terminalAreaBounds_.x, terminalAreaBounds_.y,
                       scaledCellW, scaledCellH,
                       grid_.getFrontBuffer(),
                       grid_.getCols(), grid_.getRows());

    // 4. Render Blinking Block Cursor
    if (cursorBlinkState_ && grid_.isCursorVisible()) {
        const float curPixelX = terminalAreaBounds_.x + static_cast<float>(grid_.getCursorX()) * scaledCellW;
        const float curPixelY = terminalAreaBounds_.y + static_cast<float>(grid_.getCursorY()) * scaledCellH;
        if (curPixelX + scaledCellW <= terminalAreaBounds_.x + terminalAreaBounds_.w &&
            curPixelY + scaledCellH <= terminalAreaBounds_.y + terminalAreaBounds_.h) {
            r.drawRect(curPixelX, curPixelY, scaledCellW, scaledCellH, 0.85f, 0.85f, 0.95f, 0.75f);
        }
    }
}

bool TerminalConsoleDrawer::handlePointer(const PointerEvent& ev) {
    if (ev.action == PointerAction::Down) {
        if (pullTabBounds_.contains(ev.x, ev.y)) {
            toggleExpanded();
            return true;
        }
        if (drawer_.isExpanded() && drawerBounds_.contains(ev.x, ev.y)) {
            isFocused_ = true;
            return true; // Consume clicks within terminal drawer
        }
    }
    return false;
}

bool TerminalConsoleDrawer::handleKey(int key, int scancode, int action, int mods) {
    (void)scancode;
    if (!drawer_.isExpanded()) return false;
    if (action != 1 && action != 2) return true; // Consume key release events when expanded

    bool isCtrl = (mods & 0x0002) != 0;
    bool isAlt = (mods & 0x0004) != 0;

    // Universal DAW bypasses that must still work across the DAW:
    // F11 (300): Fullscreen toggle
    if (key == 300) {
        return false;
    }
    // Alt+F4: Window exit
    if (isAlt && key == 293) {
        return false;
    }

    if (key == 256) { // Escape
        setExpanded(false);
        isFocused_ = false;
        return true;
    }

    if (key == 257 || key == 13) { // Enter
        commitCurrentLine();
        return true;
    }

    // Ctrl+L: Clear terminal screen
    if (isCtrl && (key == 76 || key == 108)) {
        clear();
        return true;
    }

    // Ctrl+C: Cancel current input line
    if (isCtrl && (key == 67 || key == 99)) {
        parser_.parse("^C\r\n");
        currentLine_.clear();
        printPrompt();
        grid_.swapBuffers();
        return true;
    }

    // Ctrl+W or Ctrl+Backspace: Word deletion
    if ((isCtrl && (key == 87 || key == 119)) || (isCtrl && (key == 259 || key == 8))) {
        if (!currentLine_.empty()) {
            while (!currentLine_.empty() && std::isspace(static_cast<unsigned char>(currentLine_.back()))) {
                currentLine_.pop_back();
                parser_.parse("\b \b");
            }
            while (!currentLine_.empty() && !std::isspace(static_cast<unsigned char>(currentLine_.back()))) {
                currentLine_.pop_back();
                parser_.parse("\b \b");
            }
        }
        return true;
    }

    if (key == 259 || key == 8) { // Backspace
        if (!currentLine_.empty()) {
            currentLine_.pop_back();
            parser_.parse("\b \b");
        }
        return true;
    }

    // Left Arrow
    if (key == 263) {
        parser_.parse("\x1b[D");
        return true;
    }

    // Right Arrow
    if (key == 262) {
        parser_.parse("\x1b[C");
        return true;
    }
    if (key == 265) { // Up arrow (History Prev)
        std::string prev = repl_.historyPrev();
        if (!prev.empty()) {
            while (!currentLine_.empty()) {
                currentLine_.pop_back();
                parser_.parse("\b \b");
            }
            currentLine_ = prev;
            parser_.parse(currentLine_);
        }
        return true;
    }
    if (key == 264) { // Down arrow (History Next)
        std::string next = repl_.historyNext();
        while (!currentLine_.empty()) {
            currentLine_.pop_back();
            parser_.parse("\b \b");
        }
        currentLine_ = next;
        if (!currentLine_.empty()) {
            parser_.parse(currentLine_);
        }
        return true;
    }
    if (key == 258) { // Tab (Auto-completion)
        auto matches = repl_.complete(currentLine_);
        if (matches.size() == 1) {
            while (!currentLine_.empty()) {
                currentLine_.pop_back();
                parser_.parse("\b \b");
            }
            currentLine_ = matches[0];
            parser_.parse(currentLine_);
        } else if (matches.size() > 1) {
            parser_.parse("\r\n\x1b[38;2;120;140;180m");
            for (const auto& m : matches) {
                parser_.parse(m + "  ");
            }
            parser_.parse("\x1b[0m\r\n");
            printPrompt();
            parser_.parse(currentLine_);
        }
        return true;
    }

    // Absorb all other keystrokes when expanded (letters like 'p', 'b', numbers, space, punctuation)
    // to shield the terminal from DAW hotkey hijacking. Characters will be delivered to handleChar.
    return true;
}

bool TerminalConsoleDrawer::handleChar(char32_t codepoint) {
    if (!drawer_.isExpanded()) return false;
    if (codepoint < 32 || codepoint == 127) return false;

    // Convert UTF-32 codepoint to UTF-8
    std::string utf8;
    if (codepoint < 0x80) {
        utf8.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        utf8.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        utf8.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        utf8.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        utf8.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }

    currentLine_ += utf8;
    parser_.parse(utf8);
    return true;
}

void TerminalConsoleDrawer::setInputText(const std::string& text) {
    if (!drawer_.isExpanded()) {
        setExpanded(true);
    }
    // Visually erase whatever was on current line
    while (!currentLine_.empty()) {
        currentLine_.pop_back();
        parser_.parse("\b \b");
    }
    currentLine_ = text;
    parser_.parse(currentLine_);
    isFocused_ = true;
    grid_.swapBuffers();
}

static std::vector<std::string> tokenizeCommandLine(const std::string& str) {
    std::vector<std::string> tokens;
    std::string cur;
    bool inQuote = false;
    char quoteChar = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (inQuote) {
            if (c == quoteChar) {
                inQuote = false;
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"' || c == '\'') {
                inQuote = true;
                quoteChar = c;
            } else if (std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.empty()) {
                    tokens.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(c);
            }
        }
    }
    if (!cur.empty()) {
        tokens.push_back(cur);
    }
    return tokens;
}

void TerminalConsoleDrawer::printHelpGuide() {
    parser_.parse("\x1b[1;38;2;160;100;255m─── Eatsbits DAW Terminal & Shell Commands ───\x1b[0m\r\n");
    parser_.parse("  \x1b[38;2;100;230;120mtrack add <name> [engine]\x1b[0m   Create track (synth, dx7, 808, 909, sampler)\r\n");
    parser_.parse("  \x1b[38;2;100;230;120mtrack rm <index>\x1b[0m            Delete track by 0-based index\r\n");
    parser_.parse("  \x1b[38;2;100;230;120mtrack list\x1b[0m                  List all tracks in project\r\n");
    parser_.parse("  \x1b[38;2;255;180;50mmacro list\x1b[0m                  List available procedural generation macros\r\n");
    parser_.parse("  \x1b[38;2;255;180;50mmacro <name>\x1b[0m                Run generative macro (acid, techno, song, chords...)\r\n");
    parser_.parse("  \x1b[38;2;250;100;150mnotes replace <from> <to>\x1b[0m   Search and replace notes throughout project\r\n");
    parser_.parse("  \x1b[38;2;80;220;255mview <name>\x1b[0m                 Switch view (arranger, edit, track, mixer, design)\r\n");
    parser_.parse("  \x1b[38;2;80;220;255mbpm [val]\x1b[0m                   Query or set project tempo\r\n");
    parser_.parse("  \x1b[38;2;80;220;255mplay / stop / panic\x1b[0m         Audio playback transport controls\r\n");
    parser_.parse("  \x1b[38;2;80;220;255mloop / metronome\x1b[0m            Toggle cycle loop or metronome click\r\n");
    parser_.parse("  \x1b[38;2;80;220;255mkeyboard / browser\x1b[0m          Toggle piano or project browser drawer\r\n");
    parser_.parse("  \x1b[38;2;80;220;255mclear\x1b[0m                       Clear terminal scrollback\r\n");
    parser_.parse("  \x1b[38;2;140;110;250mEatscript REPL: help(), play(), stop(), panic(), bpm(), set_bpm(130),\x1b[0m\r\n");
    parser_.parse("  \x1b[38;2;140;110;250m                track_add(\"303\", \"synth\"), track_delete(0), macro(\"acid\")\x1b[0m\r\n");
}

void TerminalConsoleDrawer::logCommandExecution(const std::string& title) {
    parser_.parse("\r\n\x1b[38;2;100;180;255m[COMMAND PALETTE]\x1b[0m Executed: " + title + "\r\n");
    printPrompt();
    grid_.swapBuffers();
}

bool TerminalConsoleDrawer::dispatchShellCommand(const std::string& cmd) {
    auto tokens = tokenizeCommandLine(cmd);
    if (tokens.empty()) return true;

    // Normalize verb (lowercase)
    std::string verb = tokens[0];
    std::transform(verb.begin(), verb.end(), verb.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Handle dot-command synonyms (e.g. macro.acid -> macro acid, track.create -> track create)
    if (verb.rfind("macro.", 0) == 0 && verb.size() > 6) {
        std::string sub = verb.substr(6);
        tokens[0] = "macro";
        tokens.insert(tokens.begin() + 1, sub);
        verb = "macro";
    } else if (verb.rfind("track.", 0) == 0 && verb.size() > 6) {
        std::string sub = verb.substr(6);
        tokens[0] = "track";
        tokens.insert(tokens.begin() + 1, sub);
        verb = "track";
    } else if (verb.rfind("notes.", 0) == 0 && verb.size() > 6) {
        std::string sub = verb.substr(6);
        tokens[0] = "notes";
        tokens.insert(tokens.begin() + 1, sub);
        verb = "notes";
    }

    // 1. HELP
    if (verb == "help" || verb == "help()" || verb == "help();" || verb == "help;" || verb == "?" || verb == "man") {
        printHelpGuide();
        return true;
    }

    // 2. CLEAR
    if (verb == "clear" || verb == "clear()" || verb == "clear();" || verb == "cls") {
        clear();
        return true;
    }

    // 3. TRANSPORT
    if (verb == "play" || verb == "play()" || verb == "play();" || verb == "start") {
        if (audioEngine_) {
            audioEngine_->getSequencer().start();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Playback started.\r\n");
        }
        return true;
    }
    if (verb == "stop" || verb == "stop()" || verb == "stop();" || verb == "pause") {
        if (audioEngine_) {
            audioEngine_->stopPlayback();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Playback stopped.\r\n");
        }
        return true;
    }
    if (verb == "panic" || verb == "panic()" || verb == "panic();") {
        if (audioEngine_) {
            audioEngine_->panic();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Audio panic: all active voices silenced.\r\n");
        }
        return true;
    }

    // 4. BPM
    if (verb == "bpm" || verb == "bpm()" || verb == "bpm();") {
        if (tokens.size() == 1) {
            double cur = audioEngine_ ? audioEngine_->getSequencer().getBpm() : 120.0;
            parser_.parse("\x1b[38;2;80;220;255m[BPM]\x1b[0m Project tempo: " + std::to_string(static_cast<int>(cur)) + " BPM\r\n");
        } else {
            try {
                float val = std::stof(tokens[1]);
                if (audioEngine_) audioEngine_->getSequencer().setBpm(val);
                parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Tempo set to " + tokens[1] + " BPM.\r\n");
            } catch (...) {
                parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Invalid BPM: " + tokens[1] + "\r\n");
            }
        }
        return true;
    }

    // 5. TRACK COMMANDS
    if (verb == "track" || verb == "tracks") {
        std::string sub = (tokens.size() > 1) ? tokens[1] : "list";
        std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (sub == "list" || tokens.size() == 1) {
            if (callbacks_.onTrackList) {
                auto list = callbacks_.onTrackList();
                parser_.parse("\x1b[1;38;2;160;100;255m─── Project Tracks (" + std::to_string(list.size()) + ") ───\x1b[0m\r\n");
                for (size_t i = 0; i < list.size(); ++i) {
                    parser_.parse("  \x1b[38;2;80;220;255m#" + std::to_string(i) + "\x1b[0m  " + list[i] + "\r\n");
                }
            } else if (audioEngine_) {
                size_t cnt = audioEngine_->getSequencer().getNumTracks();
                parser_.parse("\x1b[1;38;2;160;100;255m─── Sequencer Tracks (" + std::to_string(cnt) + ") ───\x1b[0m\r\n");
                for (size_t i = 0; i < cnt; ++i) {
                    const auto* trk = audioEngine_->getSequencer().getTrack(i);
                    std::string name = trk ? trk->getName() : ("Track " + std::to_string(i + 1));
                    parser_.parse("  \x1b[38;2;80;220;255m#" + std::to_string(i) + "\x1b[0m  " + name + "\r\n");
                }
            }
            return true;
        }

        if (sub == "add" || sub == "create" || sub == "new") {
            std::string name = (tokens.size() > 2) ? tokens[2] : "New Track";
            std::string engineType = (tokens.size() > 3) ? tokens[3] : "synth";
            if (callbacks_.onTrackCreate) {
                bool ok = callbacks_.onTrackCreate(name, engineType);
                if (ok) {
                    parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Created track '" + name + "' (" + engineType + ").\r\n");
                } else {
                    parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Failed to create track '" + name + "'.\r\n");
                }
            }
            return true;
        }

        if (sub == "rm" || sub == "delete" || sub == "del") {
            if (tokens.size() > 2) {
                try {
                    uint32_t idx = static_cast<uint32_t>(std::stoi(tokens[2]));
                    if (callbacks_.onTrackDelete) {
                        bool ok = callbacks_.onTrackDelete(idx);
                        if (ok) {
                            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Removed track #" + std::to_string(idx) + ".\r\n");
                        } else {
                            parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Invalid track index: " + std::to_string(idx) + "\r\n");
                        }
                    }
                } catch (...) {
                    parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Invalid track index parameter.\r\n");
                }
            } else {
                parser_.parse("Usage: track rm <index>\r\n");
            }
            return true;
        }
    }

    // 6. MACRO COMMANDS
    if (verb == "macro" || verb == "macros") {
        std::string sub = (tokens.size() > 1) ? tokens[1] : "list";
        std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (sub == "list") {
            parser_.parse("\x1b[1;38;2;255;180;50m─── Procedural Macros ───\x1b[0m\r\n");
            parser_.parse("  \x1b[38;2;100;230;120macid\x1b[0m       (macro_acid_gen)             Procedural 303 Acid Bassline\r\n");
            parser_.parse("  \x1b[38;2;100;230;120mtechno\x1b[0m     (macro_drum_techno)          Procedural 909 Drum Groover\r\n");
            parser_.parse("  \x1b[38;2;100;230;120msong\x1b[0m       (macro_song_gen)             Procedural Song Architect\r\n");
            parser_.parse("  \x1b[38;2;100;230;120mensemble\x1b[0m   (macro_ensemble_arranger)    Dynamic Multi-Track Ensemble\r\n");
            parser_.parse("  \x1b[38;2;100;230;120mchords\x1b[0m     (macro_harmonic_progression) Harmonic Progression Generator\r\n");
            parser_.parse("  \x1b[38;2;100;230;120mpiano\x1b[0m      (macro_piano_comp)           Procedural Piano Accompaniment\r\n");
            parser_.parse("  \x1b[38;2;100;230;120mhumanize\x1b[0m   (macro_humanize_all)         Humanize Velocities & Timing\r\n");
            parser_.parse("  \x1b[38;2;100;230;120marp\x1b[0m        (macro_arp_track)            Arpeggiate Active Track\r\n");
            parser_.parse("  \x1b[38;2;100;230;120mtranspose\x1b[0m  (macro_global_transpose)     Global Scale-Aware Transpose\r\n");
            parser_.parse("  Usage: \x1b[38;2;255;180;50mmacro <name>\x1b[0m (e.g. 'macro acid', 'macro song')\r\n");
            return true;
        }

        std::string macroId = sub;
        if (macroId == "run" && tokens.size() > 2) {
            macroId = tokens[2];
            std::transform(macroId.begin(), macroId.end(), macroId.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        }

        // Map short alias to macro ID
        if (macroId == "acid") macroId = "macro_acid_gen";
        else if (macroId == "techno" || macroId == "drum" || macroId == "drums") macroId = "macro_drum_techno";
        else if (macroId == "song") macroId = "macro_song_gen";
        else if (macroId == "ensemble") macroId = "macro_ensemble_arranger";
        else if (macroId == "chords" || macroId == "harmony") macroId = "macro_harmonic_progression";
        else if (macroId == "piano") macroId = "macro_piano_comp";
        else if (macroId == "humanize") macroId = "macro_humanize_all";
        else if (macroId == "arp") macroId = "macro_arp_track";
        else if (macroId == "transpose") macroId = "macro_global_transpose";

        if (callbacks_.onMacroRun) {
            std::string outMsg;
            bool ok = callbacks_.onMacroRun(macroId, outMsg);
            if (ok) {
                parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Macro '" + macroId + "' executed: " + outMsg + "\r\n");
            } else {
                parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Macro failed: " + outMsg + "\r\n");
            }
        }
        return true;
    }

    // 7. NOTES SEARCH & REPLACE
    if (verb == "notes" || verb == "note") {
        std::string sub = (tokens.size() > 1) ? tokens[1] : "";
        std::transform(sub.begin(), sub.end(), sub.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (sub == "replace" && tokens.size() >= 4) {
            std::string fromStr = tokens[2];
            std::string toStr = tokens[3];
            int trackIdx = -1;
            if (tokens.size() > 4) {
                try { trackIdx = std::stoi(tokens[4]); } catch (...) {}
            }

            int fromPitch = eatscript::NoteScriptEngine::stringToPitch(fromStr);
            int toPitch = eatscript::NoteScriptEngine::stringToPitch(toStr);

            if (fromPitch < 0 || toPitch < 0) {
                parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Invalid note pitch (use e.g. 'D#4', 'Eb3', or '60').\r\n");
                return true;
            }

            if (callbacks_.onNotesReplace) {
                int count = callbacks_.onNotesReplace(fromPitch, toPitch, trackIdx);
                parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Replaced " + std::to_string(count) + " note(s) from " + fromStr + " (" + std::to_string(fromPitch) + ") to " + toStr + " (" + std::to_string(toPitch) + ").\r\n");
            }
            return true;
        } else {
            parser_.parse("Usage: notes replace <from_note> <to_note> [track_index]\r\n");
            parser_.parse("Example: notes replace D#4 Eb4\r\n");
            return true;
        }
    }

    // 8. VIEW SWITCHING
    if (verb == "view" || verb == "goto") {
        if (tokens.size() > 1) {
            std::string v = tokens[1];
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            int idx = -1;
            std::string name;
            if (v == "arranger" || v == "0" || v == "arrangement" || v == "timeline") { idx = 0; name = "Arranger"; }
            else if (v == "edit" || v == "tracker" || v == "pianoroll" || v == "1") { idx = 1; name = "Edit (Piano Roll / Tracker)"; }
            else if (v == "track" || v == "inspector" || v == "2") { idx = 2; name = "Track Inspector"; }
            else if (v == "mixer" || v == "3" || v == "console") { idx = 3; name = "Modular Mixer"; }
            else if (v == "design" || v == "rack" || v == "modular" || v == "4") { idx = 4; name = "Modular Rack & Design"; }

            if (idx >= 0 && callbacks_.onNavigateView) {
                callbacks_.onNavigateView(idx);
                parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Switched to " + name + " View.\r\n");
            } else {
                parser_.parse("\x1b[38;2;255;90;90m[ERROR]\x1b[0m Unknown view '" + tokens[1] + "'. Valid views: arranger, edit, track, mixer, design\r\n");
            }
        } else {
            parser_.parse("Usage: view <arranger | edit | track | mixer | design>\r\n");
        }
        return true;
    }

    // 9. LOOP
    if (verb == "loop" || verb == "loop()" || verb == "loop();") {
        if (callbacks_.onToggleLoop) {
            bool l = callbacks_.onToggleLoop();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Transport cycle loop: " + std::string(l ? "ON" : "OFF") + "\r\n");
        }
        return true;
    }

    // 10. METRONOME
    if (verb == "metronome" || verb == "metronome()" || verb == "metronome();" || verb == "click") {
        if (callbacks_.onToggleMetronome) {
            bool m = callbacks_.onToggleMetronome();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Metronome click: " + std::string(m ? "ON" : "OFF") + "\r\n");
        }
        return true;
    }

    // 11. KEYBOARD / PIANO DRAWER
    if (verb == "keyboard" || verb == "piano" || verb == "keyboard()" || verb == "piano()") {
        if (callbacks_.onToggleKeyboard) {
            callbacks_.onToggleKeyboard();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Toggled Virtual Piano Keyboard.\r\n");
        }
        return true;
    }

    // 12. BROWSER DRAWER
    if (verb == "browser" || verb == "browser()" || verb == "browser();") {
        if (callbacks_.onToggleBrowser) {
            callbacks_.onToggleBrowser();
            parser_.parse("\x1b[38;2;80;230;120m[OK]\x1b[0m Toggled Preset & Sample Browser.\r\n");
        }
        return true;
    }

    return false; // Pass through to Eatscript REPL
}

void TerminalConsoleDrawer::commitCurrentLine() {
    parser_.parse("\r\n");
    std::string lineToExec = currentLine_;
    currentLine_.clear();

    executeCommand(lineToExec);
}

void TerminalConsoleDrawer::executeCommand(const std::string& command) {
    std::string trimmed = command;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();

    if (trimmed.empty()) {
        printPrompt();
        grid_.swapBuffers();
        return;
    }

    repl_.addHistory(trimmed);

    if (dispatchShellCommand(trimmed)) {
        printPrompt();
        grid_.swapBuffers();
        return;
    }

    auto res = repl_.feedLine(trimmed);

    if (res.status == eatscript::ReplResult::Status::Complete) {
        if (!res.output.empty()) {
            parser_.parse(res.output);
            if (res.output.back() != '\n') {
                parser_.parse("\r\n");
            }
        }
    } else if (res.status == eatscript::ReplResult::Status::Error) {
        parser_.parse("\x1b[38;2;255;90;90m" + res.error + "\x1b[0m\r\n");
    }

    printPrompt();
    grid_.swapBuffers();
}

} // namespace eatsbits::ui
