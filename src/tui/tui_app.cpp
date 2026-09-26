#include "eatsbits/tui/tui_app.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include <thread>
#include <chrono>
#include <iostream>

namespace eatsbits::tui {

TuiApp::TuiApp() : surface_(120, 36) {}

TuiApp::~TuiApp() {
    stop();
}

bool TuiApp::init() {
    if (!terminalDevice_.init()) {
        std::cerr << "[TUI] Failed to initialize terminal raw mode." << std::endl;
        return false;
    }

    terminalDevice_.enterAlternateScreen();
    terminalDevice_.showCursor(false);
    terminalDevice_.enableMouseTracking();

    terminalDevice_.getWindowSize(screenCols_, screenRows_);
    surface_.resize(screenCols_, screenRows_);

    setupAudioSession();
    running_ = true;
    return true;
}

void TuiApp::setupAudioSession() {
    audio::AudioEngineConfig config;
    config.sampleRate = 48000;
    config.bufferFrameSize = 128; // Ultra-low latency

    audioEngine_.initialize(config);
    audioEngine_.setupDefaultAcidBeatGraph();

    // Configure sequencer pattern
    auto& seq = audioEngine_.getSequencer();
    seq.stop();
    seq.setBpm(132.0);
    seq.setSwing(0.55);

    audio::NodeId tbId = 0;
    audio::NodeId drumId = 0;
    for (const auto& [id, n] : audioEngine_.getGraph().getNodes()) {
        if (dynamic_cast<audio::Tb303Node*>(n.get())) tbId = id;
        if (dynamic_cast<audio::DrumKitNode*>(n.get())) drumId = id;
    }

    if (tbId != 0) {
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
    }

    if (drumId != 0) {
        size_t tKick = seq.addTrack("Kick", drumId, 16);
        auto* trackKick = seq.getTrack(tKick);
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            st.note = 36; // Kick MIDI
            st.active = (s % 4 == 0);
            st.velocity = 0.95f;
            trackKick->setStep(s, st);
        }

        size_t tSnare = seq.addTrack("Snare", drumId, 16);
        auto* trackSnare = seq.getTrack(tSnare);
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            st.note = 38; // Snare MIDI
            st.active = (s == 4 || s == 12);
            st.velocity = 0.85f;
            trackSnare->setStep(s, st);
        }

        size_t tHat = seq.addTrack("HiHat", drumId, 16);
        auto* trackHat = seq.getTrack(tHat);
        for (uint32_t s = 0; s < 16; ++s) {
            sequencer::StepData st{};
            st.note = 42; // Closed Hat
            st.active = (s % 2 == 1);
            st.velocity = 0.70f;
            trackHat->setStep(s, st);
        }
    }

    audioEngine_.setCutoff(850.0f);
    audioEngine_.setResonance(0.82f);
    audioEngine_.setMasterVolume(0.85f);

    audioEngine_.start();
    seq.start();
}

void TuiApp::run() {
    bool forceRedraw = true;

    while (running_) {
        auto frameStart = std::chrono::steady_clock::now();

        // Check if terminal size changed
        int newCols = 0, newRows = 0;
        if (terminalDevice_.getWindowSize(newCols, newRows) &&
            (newCols != screenCols_ || newRows != screenRows_)) {
            screenCols_ = newCols;
            screenRows_ = newRows;
            surface_.resize(screenCols_, screenRows_);
            forceRedraw = true;
        }

        // Process inputs
        processInput();

        // Update telemetry
        telemetryBridge_.update(audioEngine_);

        // Render views into CellSurface
        updateAndRender(forceRedraw);
        forceRedraw = false;

        // Diff and flush to terminal stdout
        outFlushBuffer_.clear();
        diffRenderer_.renderDiff(surface_, outFlushBuffer_);
        if (!outFlushBuffer_.empty()) {
            terminalDevice_.writeRaw(outFlushBuffer_);
            terminalDevice_.flush();
        }

        // Target 60 FPS (~16.6ms)
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        if (elapsed.count() < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - elapsed.count()));
        }
    }

    stop();
}

void TuiApp::stop() {
    if (!running_) return;
    running_ = false;

    audioEngine_.getSequencer().stop();
    audioEngine_.stop();
    audioEngine_.shutdown();

    terminalDevice_.shutdown();
}

void TuiApp::processInput() {
    char inputBuf[128];
    size_t n = terminalDevice_.readInput(inputBuf, sizeof(inputBuf));
    if (n > 0) {
        inputParser_.feed(inputBuf, n);
    }

    KeyEvent key;
    while (inputParser_.pollKey(key)) {
        handleKeyEvent(key);
    }

    MouseEvent mouse;
    while (inputParser_.pollMouse(mouse)) {
        handleMouseEvent(mouse);
    }
}

void TuiApp::handleKeyEvent(const KeyEvent& key) {
    if (key.code == KeyCode::Escape || key.isChar('q') || key.isChar('Q')) {
        running_ = false;
        return;
    }

    // Toggle transport playback
    if (key.code == KeyCode::Space) {
        auto& seq = audioEngine_.getSequencer();
        if (seq.isPlaying()) {
            seq.stop();
        } else {
            seq.start();
        }
        return;
    }

    // Workspace tab switching (F1-F5 or number keys)
    if (key.code == KeyCode::F1 || key.isChar('1')) { activeTab_ = WorkspaceTab::Arranger; return; }
    if (key.code == KeyCode::F2 || key.isChar('2')) { activeTab_ = WorkspaceTab::Edit; return; }
    if (key.code == KeyCode::F3 || key.isChar('3')) { activeTab_ = WorkspaceTab::Track; return; }
    if (key.code == KeyCode::F4 || key.isChar('4')) { activeTab_ = WorkspaceTab::Mixer; return; }
    if (key.code == KeyCode::F5 || key.isChar('5')) { activeTab_ = WorkspaceTab::Design; return; }

    // Toggle Inspector sidebar
    if (key.isChar('b') || key.isChar('B')) {
        arrangerView_.toggleInspector();
        return;
    }

    // Tab cycle
    if (key.code == KeyCode::Tab) {
        int next = (static_cast<int>(activeTab_) + 1) % 5;
        activeTab_ = static_cast<WorkspaceTab>(next);
        return;
    }

    // Workspace-specific keys
    if (activeTab_ == WorkspaceTab::Arranger) {
        if (key.code == KeyCode::Left) arrangerView_.handleScroll(-1);
        if (key.code == KeyCode::Right) arrangerView_.handleScroll(1);
    } else if (activeTab_ == WorkspaceTab::Edit) {
        auto& seq = audioEngine_.getSequencer();
        if (key.code == KeyCode::Up) trackerGridView_.moveCursor(0, -1, seq);
        else if (key.code == KeyCode::Down) trackerGridView_.moveCursor(0, 1, seq);
        else if (key.code == KeyCode::Left) trackerGridView_.moveCursor(-1, 0, seq);
        else if (key.code == KeyCode::Right) trackerGridView_.moveCursor(1, 0, seq);
        else if (key.code == KeyCode::Enter) trackerGridView_.toggleStep(seq);
        else if (key.isChar('+') || key.isChar('=')) trackerGridView_.changePitch(1, seq);
        else if (key.isChar('-') || key.isChar('_')) trackerGridView_.changePitch(-1, seq);
        else if (key.isChar('a') || key.isChar('A')) trackerGridView_.toggleAccent(seq);
        else if (key.isChar('s') || key.isChar('S')) trackerGridView_.toggleSlide(seq);
    } else if (activeTab_ == WorkspaceTab::Track) {
        if (key.code == KeyCode::Up) paramRackView_.moveSelection(-1);
        else if (key.code == KeyCode::Down) paramRackView_.moveSelection(1);
        else if (key.code == KeyCode::Left) paramRackView_.adjustValue(-1, audioEngine_);
        else if (key.code == KeyCode::Right) paramRackView_.adjustValue(1, audioEngine_);
        else if (key.isChar('[') || key.isChar('{')) paramRackView_.adjustValue(-5, audioEngine_);
        else if (key.isChar(']') || key.isChar('}')) paramRackView_.adjustValue(5, audioEngine_);
    }
}

void TuiApp::handleMouseEvent(const MouseEvent& mouse) {
    if (mouse.button == 64) { // Scroll Up
        if (activeTab_ == WorkspaceTab::Arranger) arrangerView_.handleScroll(-1);
        else if (activeTab_ == WorkspaceTab::Edit) trackerGridView_.moveCursor(0, -1, audioEngine_.getSequencer());
        else if (activeTab_ == WorkspaceTab::Track) paramRackView_.adjustValue(1, audioEngine_);
        return;
    } else if (mouse.button == 65) { // Scroll Down
        if (activeTab_ == WorkspaceTab::Arranger) arrangerView_.handleScroll(1);
        else if (activeTab_ == WorkspaceTab::Edit) trackerGridView_.moveCursor(0, 1, audioEngine_.getSequencer());
        else if (activeTab_ == WorkspaceTab::Track) paramRackView_.adjustValue(-1, audioEngine_);
        return;
    }

    if (mouse.isRelease) {
        isDragging_ = false;
        arrangerView_.handleMouseUp();
        return;
    }

    if (mouse.isDrag) {
        if (isDragging_ && activeTab_ == WorkspaceTab::Arranger) {
            arrangerView_.handleMouseDrag(mouse.x, mouse.y);
        }
        return;
    }

    // Click handling (Left button)
    if (mouse.button == 0) {
        // 1. Top Panel Buttons
        if (cachedPlayBtn_.contains(mouse.x, mouse.y)) {
            audioEngine_.getSequencer().start();
            return;
        }
        if (cachedStopBtn_.contains(mouse.x, mouse.y)) {
            audioEngine_.getSequencer().stop();
            recording_ = false;
            return;
        }
        if (cachedRecBtn_.contains(mouse.x, mouse.y)) {
            recording_ = !recording_;
            if (recording_ && !audioEngine_.getSequencer().isPlaying()) {
                audioEngine_.getSequencer().start();
            }
            return;
        }
        if (cachedBrowserBtn_.contains(mouse.x, mouse.y)) {
            arrangerView_.toggleInspector();
            return;
        }

        // 2. Bottom Panel Workspace Tabs
        for (int i = 0; i < 5; ++i) {
            if (cachedTabBtns_[i].contains(mouse.x, mouse.y)) {
                activeTab_ = static_cast<WorkspaceTab>(i);
                return;
            }
        }

        // 3. Central DAW Area
        if (activeTab_ == WorkspaceTab::Arranger) {
            if (arrangerView_.handleMouseDown(mouse.x, mouse.y, mouse.button)) {
                isDragging_ = true;
            }
        }
    }
}

void TuiApp::updateAndRender(bool forceRedraw) {
    if (forceRedraw) {
        surface_.clear(Color::Black());
        surface_.markAllDirty();
    }

    const auto& telemetry = telemetryBridge_.getSnapshot();

    const int topPanelHeight = 3;
    const int bottomPanelHeight = 2;
    const int dawHeight = screenRows_ - topPanelHeight - bottomPanelHeight;

    // 1. Top Panel
    cachedTopPanel_ = {0, 0, screenCols_, topPanelHeight};
    renderTopPanel(cachedTopPanel_, telemetry);

    // 2. Main DAW Workspace Area
    Rect dawArea{0, topPanelHeight, screenCols_, dawHeight};

    if (activeTab_ == WorkspaceTab::Arranger) {
        arrangerView_.render(surface_, dawArea, telemetry, true);
    } else if (activeTab_ == WorkspaceTab::Edit) {
        // Split view: Tracker on Left, Oscilloscope on Right
        int leftW = (screenCols_ * 60) / 100;
        int rightW = screenCols_ - leftW;
        Rect trackerRect{0, topPanelHeight, leftW, dawHeight};
        Rect scopeRect{leftW, topPanelHeight, rightW, dawHeight};
        trackerGridView_.render(surface_, trackerRect, audioEngine_.getSequencer(), telemetry, true);
        scopeView_.render(surface_, scopeRect, telemetry);
    } else if (activeTab_ == WorkspaceTab::Track) {
        // Split view: Param Rack on Left, Scope/Spectrum on Right
        int leftW = (screenCols_ * 50) / 100;
        int rightW = screenCols_ - leftW;
        Rect rackRect{0, topPanelHeight, leftW, dawHeight};
        Rect scopeRect{leftW, topPanelHeight, rightW, dawHeight};
        paramRackView_.render(surface_, rackRect, audioEngine_, true);
        scopeView_.render(surface_, scopeRect, telemetry);
    } else if (activeTab_ == WorkspaceTab::Mixer) {
        // Full screen Master Meters & Oscilloscope
        scopeView_.render(surface_, dawArea, telemetry);
    } else if (activeTab_ == WorkspaceTab::Design) {
        renderDesignView(dawArea);
    }

    // 3. Bottom Navigation Panel
    cachedBottomPanel_ = {0, screenRows_ - bottomPanelHeight, screenCols_, bottomPanelHeight};
    renderBottomPanel(cachedBottomPanel_);
}

void TuiApp::renderTopPanel(const Rect& area, const TelemetrySnapshot& telemetry) {
    surface_.drawBox(area, Color::DarkGray(), Color::FromHex(0x101217), false);

    // Left brand: "e:" icon in Acid Amber
    surface_.drawText(area.x + 2, area.y + 1, "e:", Color::AcidAmber(), Color::FromHex(0x101217), static_cast<uint8_t>(TextAttr::Bold));

    // Transport buttons: ▶ Play, ⏹ Stop, ⏺ Rec
    int curX = area.x + 6;

    // ▶ Play button
    Color playBg = telemetry.isPlaying ? Color::NeonGreen() : Color::DarkGray();
    Color playFg = telemetry.isPlaying ? Color::Black() : Color::White();
    std::string playText = telemetry.isPlaying ? " ▶ PLAY " : " ▶ Play ";
    surface_.drawText(curX, area.y + 1, playText, playFg, playBg, static_cast<uint8_t>(TextAttr::Bold));
    cachedPlayBtn_ = {curX, area.y + 1, static_cast<int>(playText.size()), 1};
    curX += static_cast<int>(playText.size()) + 1;

    // ⏹ Stop button
    Color stopBg = (!telemetry.isPlaying && !recording_) ? Color::DangerRed() : Color::DarkGray();
    Color stopFg = (!telemetry.isPlaying && !recording_) ? Color::White() : Color::LightGray();
    std::string stopText = (!telemetry.isPlaying && !recording_) ? " ⏹ STOP " : " ⏹ Stop ";
    surface_.drawText(curX, area.y + 1, stopText, stopFg, stopBg, static_cast<uint8_t>(TextAttr::Bold));
    cachedStopBtn_ = {curX, area.y + 1, static_cast<int>(stopText.size()), 1};
    curX += static_cast<int>(stopText.size()) + 1;

    // ⏺ Rec button
    Color recBg = recording_ ? Color::DangerRed() : Color::DarkGray();
    Color recFg = recording_ ? Color::White() : Color::LightGray();
    std::string recText = recording_ ? " ⏺ REC " : " ⏺ Rec ";
    surface_.drawText(curX, area.y + 1, recText, recFg, recBg, static_cast<uint8_t>(TextAttr::Bold));
    cachedRecBtn_ = {curX, area.y + 1, static_cast<int>(recText.size()), 1};
    curX += static_cast<int>(recText.size()) + 2;

    // Stats: BPM, Time signature, Position counter, CPU load
    char statsBuf[64];
    int curBar = static_cast<int>(telemetry.currentStep / 16) + 1;
    int curBeat = static_cast<int>((telemetry.currentStep % 16) / 4) + 1;
    int cur16th = static_cast<int>(telemetry.currentStep % 4) + 1;
    snprintf(statsBuf, sizeof(statsBuf), "TEMPO: %5.1f BPM │ 4/4 │ %03d.%02d.%02d │ CPU: 1.6%%",
             telemetry.bpm, curBar, curBeat, cur16th);
    surface_.drawText(curX, area.y + 1, statsBuf, Color::White(), Color::FromHex(0x101217));

    // Right-aligned Project Browser Button
    std::string browserText = " [▤ BROWSER] ";
    int browserX = area.x + area.width - static_cast<int>(browserText.size()) - 2;
    if (browserX > curX + 45) {
        Color browserBg = arrangerView_.isInspectorOpen() ? Color::AcidAmber() : Color::DarkGray();
        Color browserFg = arrangerView_.isInspectorOpen() ? Color::Black() : Color::White();
        surface_.drawText(browserX, area.y + 1, browserText, browserFg, browserBg, static_cast<uint8_t>(TextAttr::Bold));
        cachedBrowserBtn_ = {browserX, area.y + 1, static_cast<int>(browserText.size()), 1};
    }
}

void TuiApp::renderBottomPanel(const Rect& area) {
    surface_.fillRect(area, Color::FromHex(0x14161E), ' ');
    surface_.setCell(area.x, area.y, Cell{0x2500, Color::DarkGray(), Color::FromHex(0x14161E), 0});

    // 5 Workspace Tabs: Arranger | Edit | Track | Mixer | Design
    static const char* kTabLabels[5] = {
        "  Arranger  ",
        "    Edit    ",
        "   Track    ",
        "   Mixer    ",
        "   Design   "
    };

    int curX = area.x + 2;
    for (int i = 0; i < 5; ++i) {
        bool isActive = (static_cast<int>(activeTab_) == i);
        Color tabBg = isActive ? Color::AcidAmber() : Color::FromHex(0x1E222D);
        Color tabFg = isActive ? Color::Black() : Color::LightGray();

        std::string label = kTabLabels[i];
        surface_.drawText(curX, area.y + 1, label, tabFg, tabBg, isActive ? static_cast<uint8_t>(TextAttr::Bold) : 0);
        cachedTabBtns_[i] = {curX, area.y + 1, static_cast<int>(label.size()), 1};

        curX += static_cast<int>(label.size()) + 2;
        if (i < 4) {
            surface_.drawText(curX - 1, area.y + 1, "│", Color::DarkGray(), Color::FromHex(0x14161E));
        }
    }

    // Right-aligned branding & shortcuts
    std::string help = "[SPACE] Play  [B] Sidebar  [F1-F5] Views  [Q] Exit";
    int helpX = area.x + area.width - static_cast<int>(help.size()) - 2;
    if (helpX > curX) {
        surface_.drawText(helpX, area.y + 1, help, Color::Gray(), Color::FromHex(0x14161E));
    }
}

void TuiApp::renderDesignView(const Rect& area) {
    surface_.drawBox(area, Color::CyberCyan(), Color::Black(), true, "EATSCRIPT LIVE REPL & MODULAR GRAPH");

    int curY = area.y + 2;
    surface_.drawText(area.x + 2, curY++, "EatScript Dynamic Algorithmic Audio Runtime", Color::White(), Color::Black(), static_cast<uint8_t>(TextAttr::Bold));
    surface_.drawText(area.x + 2, curY++, "Live Code Editor & JIT DSP Compiler (C++ ABI)", Color::LightGray(), Color::Black());
    curY++;

    // Script preview box
    Rect codeBox{area.x + 2, curY, area.width - 4, std::min(10, area.height - 8)};
    surface_.drawBox(codeBox, Color::DarkGray(), Color::FromHex(0x0C0E12), false, "active_macro.eats");

    static const char* kCodeLines[5] = {
        "def process(time, freq, note, params):",
        "    cutoff = 800.0 + 400.0 * math.sin(time * 2.0)",
        "    res = params.get('resonance', 0.8)",
        "    return tb303_acid_core(note, cutoff, res)",
        "# Live compiled to native x86_64 AVX2 machine code"
    };

    for (int i = 0; i < 5; ++i) {
        Color fg = (i == 4) ? Color::Gray() : Color::NeonGreen();
        surface_.drawText(codeBox.x + 2, codeBox.y + 1 + i, kCodeLines[i], fg, Color::FromHex(0x0C0E12));
    }
}

} // namespace eatsbits::tui
