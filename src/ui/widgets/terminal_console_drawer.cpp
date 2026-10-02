#include "eatsbits/ui/widgets/terminal_console_drawer.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

TerminalConsoleDrawer::TerminalConsoleDrawer() {
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

void TerminalConsoleDrawer::setupHostBindings() {
    // 1. engine_play()
    hostRegistry_.registerHostFunction("engine_play", EATS_ABI_TYPE_BOOL, {}, [this](const auto&) -> EatsAbiValue {
        if (audioEngine_) {
            audioEngine_->getSequencer().start();
            return eats_abi_value_bool(true);
        }
        return eats_abi_value_bool(false);
    }, "Start sequencer playback");

    // 2. engine_stop()
    hostRegistry_.registerHostFunction("engine_stop", EATS_ABI_TYPE_BOOL, {}, [this](const auto&) -> EatsAbiValue {
        if (audioEngine_) {
            audioEngine_->getSequencer().stop();
            return eats_abi_value_bool(true);
        }
        return eats_abi_value_bool(false);
    }, "Stop sequencer playback");

    // 3. engine_is_playing()
    hostRegistry_.registerHostFunction("engine_is_playing", EATS_ABI_TYPE_BOOL, {}, [this](const auto&) -> EatsAbiValue {
        bool playing = audioEngine_ ? audioEngine_->getSequencer().isPlaying() : false;
        return eats_abi_value_bool(playing);
    }, "Check if sequencer is playing");

    // 4. engine_get_bpm()
    hostRegistry_.registerHostFunction("engine_get_bpm", EATS_ABI_TYPE_F64, {}, [this](const auto&) -> EatsAbiValue {
        double bpm = audioEngine_ ? audioEngine_->getSequencer().getBpm() : 120.0;
        return eats_abi_value_f64(bpm);
    }, "Get current sequencer BPM");

    // 5. engine_set_bpm(bpm)
    hostRegistry_.registerHostFunction("engine_set_bpm", EATS_ABI_TYPE_VOID, {EATS_ABI_TYPE_F64}, [this](const auto& args) -> EatsAbiValue {
        if (audioEngine_ && !args.empty()) {
            audioEngine_->getSequencer().setBpm(static_cast<float>(args[0].as.f64));
        }
        return eats_abi_value_void();
    }, "Set current sequencer BPM");

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

    // 8. engine_panic()
    hostRegistry_.registerHostFunction("engine_panic", EATS_ABI_TYPE_VOID, {}, [this](const auto&) -> EatsAbiValue {
        if (audioEngine_) {
            audioEngine_->panic();
        }
        return eats_abi_value_void();
    }, "Silence all voices");

    // 9. engine_get_tracks()
    hostRegistry_.registerHostFunction("engine_get_tracks", EATS_ABI_TYPE_I32, {}, [this](const auto&) -> EatsAbiValue {
        int count = audioEngine_ ? static_cast<int>(audioEngine_->getSequencer().getNumTracks()) : 0;
        return eats_abi_value_i32(count);
    }, "Get number of sequencer tracks");

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
    isExpanded_ = exp;
    if (isExpanded_) {
        cursorBlinkTimer_ = 0.0;
        cursorBlinkState_ = true;
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

void TerminalConsoleDrawer::layout(float screenWidth, float bottomNavTopY, float uiScale) {
    if (uiScale <= 0.001f) uiScale = 1.0f;

    const float scaledCellW = kCellWidth * uiScale;
    const float scaledCellH = kCellHeight * uiScale;

    drawerHeight_ = kDefaultDrawerHeight * uiScale;
    const float topY = isExpanded_ ? (bottomNavTopY - drawerHeight_ - kPullTabHeight) : (bottomNavTopY - kPullTabHeight);

    drawerBounds_ = {0.0f, topY, screenWidth, isExpanded_ ? (drawerHeight_ + kPullTabHeight) : kPullTabHeight};

    // Right-docked tab above bottom nav bar (leaving virtual piano centered at screenWidth*0.5f completely clear)
    const float tabW = 150.0f * uiScale;
    const float tabX = screenWidth - tabW - 16.0f * uiScale;
    pullTabBounds_ = {tabX, topY, tabW, kPullTabHeight};

    if (isExpanded_) {
        const float termX = 12.0f * uiScale;
        const float termY = topY + kPullTabHeight + 4.0f;
        const float termW = screenWidth - termX * 2.0f;
        const float termH = drawerHeight_ - 8.0f;
        terminalAreaBounds_ = {termX, termY, termW, termH};

        int cols = std::clamp(static_cast<int>(termW / scaledCellW), 20, 220);
        int rows = std::clamp(static_cast<int>(termH / scaledCellH), 5, 80);

        if (cols != grid_.getCols() || rows != grid_.getRows()) {
            grid_.resize(cols, rows);
        }
    }
}

void TerminalConsoleDrawer::update(const FrameTimeContext& time) noexcept {
    if (!isExpanded_) return;

    cursorBlinkTimer_ += time.deltaTime;
    cursorBlinkState_ = (std::fmod(cursorBlinkTimer_, 0.8) < 0.4);

    grid_.swapBuffers();
}

void TerminalConsoleDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme, float uiScale) {
    (void)theme;
    if (uiScale <= 0.001f) uiScale = 1.0f;

    const float tabX = pullTabBounds_.x;
    const float tabY = pullTabBounds_.y;
    const float tabW = pullTabBounds_.w;
    const float tabH = pullTabBounds_.h;

    // 1. Pull Tab Header
    r.drawRoundedRect(tabX, tabY, tabW, tabH, 6.0f, 0.12f, 0.14f, 0.18f, 0.95f);
    r.drawRoundedRectOutline(tabX, tabY, tabW, tabH, 6.0f, 0.25f, 0.28f, 0.38f, 1.0f, 1.0f);

    // Tab Icon & Title
    r.drawMonospaceText(tabX + 14.0f * uiScale, tabY + 6.0f, 8.0f * uiScale, 16.0f * uiScale,
                        isExpanded_ ? "v  >_ TERMINAL" : "^  >_ TERMINAL",
                        0xFFE0E0E0);

    if (!isExpanded_) return;

    // 2. Main Terminal Acrylic Drawer Background
    const float termX = drawerBounds_.x;
    const float termY = drawerBounds_.y + kPullTabHeight;
    const float termW = drawerBounds_.w;
    const float termH = drawerHeight_;

    r.drawRect(termX, termY, termW, termH, 0.06f, 0.07f, 0.09f, 0.98f);
    r.drawLine(termX, termY, termX + termW, termY, 0.65f, 0.35f, 0.95f, 1.0f, 1.5f);

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
        if (isExpanded_ && drawerBounds_.contains(ev.x, ev.y)) {
            return true; // Consume clicks within terminal drawer
        }
    }
    return false;
}

bool TerminalConsoleDrawer::handleKey(int key, int scancode, int action, int mods) {
    (void)scancode;
    (void)mods;
    if (!isExpanded_) return false;
    if (action != 1 && action != 2) return false; // Press or Repeat

    if (key == 257 || key == 13) { // Enter
        commitCurrentLine();
        return true;
    }
    if (key == 259 || key == 8) { // Backspace
        if (!currentLine_.empty()) {
            currentLine_.pop_back();
            parser_.parse("\b \b");
        }
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
    if (key == 256) { // Escape
        setExpanded(false);
        return true;
    }

    return false;
}

bool TerminalConsoleDrawer::handleChar(char32_t codepoint) {
    if (!isExpanded_) return false;
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

void TerminalConsoleDrawer::commitCurrentLine() {
    parser_.parse("\r\n");
    std::string lineToExec = currentLine_;
    currentLine_.clear();

    executeCommand(lineToExec);
}

void TerminalConsoleDrawer::executeCommand(const std::string& command) {
    auto res = repl_.feedLine(command);

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
