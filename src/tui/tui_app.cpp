#include "eatsbits/tui/tui_app.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "clay.h"
#include <thread>
#include <chrono>
#include <iostream>

namespace eatsbits::tui {

TuiApp::TuiApp() : surface_(120, 36) {
    clayContext_ = std::make_unique<ui::clay::ClayContext>();
    clayContext_->initialize(120.0f, 36.0f);
    clayContext_->setDefaultMonospaceMeasure(1.0f, 1.0f);
}

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
    if (clayContext_) {
        clayContext_->setDimensions(static_cast<float>(screenCols_), static_cast<float>(screenRows_));
    }

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
        bool isIdle = !audioEngine_.getSequencer().isPlaying() &&
                      telemetryBridge_.isSettled() &&
                      !(activeTab_ == WorkspaceTab::Track && paramRackView_.isEditing());

        if (isIdle && !forceRedraw) {
            // Suspend into kernel event-wait rather than spinning at 60 FPS (~0% idle CPU)
            terminalDevice_.waitForInput(50);
        }

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

        // Frame pacing (when active)
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

    audioEngine_.stopPlayback();
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
    // When editing in Track Param Rack, prioritize inline edit buffer
    if (activeTab_ == WorkspaceTab::Track && paramRackView_.isEditing()) {
        if (paramRackView_.handleKey(key, audioEngine_)) {
            return;
        }
    }

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
        if (key.code == KeyCode::Enter || key.isChar('e') || key.isChar('E')) {
            paramRackView_.startEdit(audioEngine_);
            return;
        }
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
            audioEngine_.stopPlayback();
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
    if (!clayContext_ || !clayContext_->isInitialized()) {
        return;
    }

    clayContext_->setDimensions(static_cast<float>(area.width), static_cast<float>(area.height));
    clayContext_->beginLayout();

    CLAY(CLAY_ID("TuiTopBar"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(static_cast<float>(area.width)), CLAY_SIZING_FIXED(static_cast<float>(area.height)) },
            .padding = { 2, 2, 1, 1 },
            .childGap = 1,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = { 16, 18, 23, 255 },
        .border = { .color = { 80, 80, 80, 255 }, .width = { 1, 1, 1, 1, 0 } }
    }) {
        // Left brand logo: "e:" in Acid Amber
        CLAY(CLAY_ID("TuiBrandLogo"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } }
        }) {
            CLAY_TEXT(CLAY_STRING("e:"), CLAY_TEXT_CONFIG({ .textColor = { 255, 179, 0, 255 } }));
        }

        // Spacer element to position buttons starting at x = 6 (padding=2 + logo=2 + spacer=2 = 6)
        CLAY(CLAY_ID("BrandSpacer"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(1) } }
        }) {}

        // Transport buttons: ▶ Play, ⏹ Stop, ⏺ Rec
        std::string playText = telemetry.isPlaying ? " ▶ PLAY " : " ▶ Play ";
        Clay_Color playBg = telemetry.isPlaying ? Clay_Color{ 57, 255, 20, 255 } : Clay_Color{ 80, 80, 80, 255 };
        Clay_Color playFg = telemetry.isPlaying ? Clay_Color{ 0, 0, 0, 255 } : Clay_Color{ 255, 255, 255, 255 };
        CLAY(CLAY_ID("PlayBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = playBg
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(playText), CLAY_TEXT_CONFIG({ .textColor = playFg }));
        }

        bool isStopped = !telemetry.isPlaying && !recording_;
        std::string stopText = isStopped ? " ⏹ STOP " : " ⏹ Stop ";
        Clay_Color stopBg = isStopped ? Clay_Color{ 235, 30, 60, 255 } : Clay_Color{ 80, 80, 80, 255 };
        Clay_Color stopFg = isStopped ? Clay_Color{ 255, 255, 255, 255 } : Clay_Color{ 180, 180, 180, 255 };
        CLAY(CLAY_ID("StopBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = stopBg
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(stopText), CLAY_TEXT_CONFIG({ .textColor = stopFg }));
        }

        std::string recText = recording_ ? " ⏺ REC " : " ⏺ Rec ";
        Clay_Color recBg = recording_ ? Clay_Color{ 235, 30, 60, 255 } : Clay_Color{ 80, 80, 80, 255 };
        Clay_Color recFg = recording_ ? Clay_Color{ 255, 255, 255, 255 } : Clay_Color{ 180, 180, 180, 255 };
        CLAY(CLAY_ID("RecBtn"), {
            .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
            .backgroundColor = recBg
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(recText), CLAY_TEXT_CONFIG({ .textColor = recFg }));
        }

        // Stats container (Flex grow to fill center)
        char statsBuf[64];
        int curBar = static_cast<int>(telemetry.currentStep / 16) + 1;
        int curBeat = static_cast<int>((telemetry.currentStep % 16) / 4) + 1;
        int cur16th = static_cast<int>(telemetry.currentStep % 4) + 1;
        snprintf(statsBuf, sizeof(statsBuf), "TEMPO: %5.1f BPM │ 4/4 │ %03d.%02d.%02d │ CPU: 1.6%%",
                 telemetry.bpm, curBar, curBeat, cur16th);

        CLAY(CLAY_ID("StatsContainer"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) },
                .padding = { 1, 0, 0, 0 },
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }
            }
        }) {
            CLAY_TEXT(ClayTuiBridge::toClayString(statsBuf), CLAY_TEXT_CONFIG({ .textColor = { 255, 255, 255, 255 } }));
        }

        // Right-aligned Project Browser Button
        if (area.width >= 75) {
            std::string browserText = " [▤ BROWSER] ";
            bool isInspector = arrangerView_.isInspectorOpen();
            Clay_Color browserBg = isInspector ? Clay_Color{ 255, 179, 0, 255 } : Clay_Color{ 80, 80, 80, 255 };
            Clay_Color browserFg = isInspector ? Clay_Color{ 0, 0, 0, 255 } : Clay_Color{ 255, 255, 255, 255 };
            CLAY(CLAY_ID("BrowserBtn"), {
                .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
                .backgroundColor = browserBg
            }) {
                CLAY_TEXT(ClayTuiBridge::toClayString(browserText), CLAY_TEXT_CONFIG({ .textColor = browserFg }));
            }
        }
    }

    Clay_RenderCommandArray cmds = clayContext_->endLayout(0.016f);
    clayBridge_.render(cmds, surface_, area.x, area.y);

    // Update cached hit testing regions from Clay layout solver
    auto playData = clayContext_->getElementData(CLAY_ID("PlayBtn"));
    if (playData.found) {
        cachedPlayBtn_ = ClayTuiBridge::snapToCells(playData.boundingBox);
        cachedPlayBtn_.x += area.x;
        cachedPlayBtn_.y += area.y;
    }

    auto stopData = clayContext_->getElementData(CLAY_ID("StopBtn"));
    if (stopData.found) {
        cachedStopBtn_ = ClayTuiBridge::snapToCells(stopData.boundingBox);
        cachedStopBtn_.x += area.x;
        cachedStopBtn_.y += area.y;
    }

    auto recData = clayContext_->getElementData(CLAY_ID("RecBtn"));
    if (recData.found) {
        cachedRecBtn_ = ClayTuiBridge::snapToCells(recData.boundingBox);
        cachedRecBtn_.x += area.x;
        cachedRecBtn_.y += area.y;
    }

    auto browserData = clayContext_->getElementData(CLAY_ID("BrowserBtn"));
    if (browserData.found) {
        cachedBrowserBtn_ = ClayTuiBridge::snapToCells(browserData.boundingBox);
        cachedBrowserBtn_.x += area.x;
        cachedBrowserBtn_.y += area.y;
    } else {
        cachedBrowserBtn_ = {};
    }
}

void TuiApp::renderBottomPanel(const Rect& area) {
    if (!clayContext_ || !clayContext_->isInitialized()) {
        return;
    }

    clayContext_->setDimensions(static_cast<float>(area.width), static_cast<float>(area.height));
    clayContext_->beginLayout();

    CLAY(CLAY_ID("TuiBottomBar"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(static_cast<float>(area.width)), CLAY_SIZING_FIXED(static_cast<float>(area.height)) },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = { 20, 22, 30, 255 }, // Color::FromHex(0x14161E)
        .border = { .color = { 80, 80, 80, 255 }, .width = { 0, 0, 1, 0, 0 } }
    }) {
        // Border separator row (row 0)
        CLAY(CLAY_ID("BottomTopBorderRow"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } }
        }) {}

        // Content row (row 1): Tabs and keyboard shortcuts
        CLAY(CLAY_ID("BottomContentRow"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) },
                .padding = { 2, 2, 0, 0 },
                .childGap = 0,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            }
        }) {
            static const char* kTabLabels[5] = {
                "  Arranger  ",
                "    Edit    ",
                "   Track    ",
                "   Mixer    ",
                "   Design   "
            };

            for (int i = 0; i < 5; ++i) {
                bool isActive = (static_cast<int>(activeTab_) == i);
                Clay_Color tabBg = isActive ? Clay_Color{ 255, 179, 0, 255 } : Clay_Color{ 30, 34, 45, 255 };
                Clay_Color tabFg = isActive ? Clay_Color{ 0, 0, 0, 255 } : Clay_Color{ 180, 180, 180, 255 };

                CLAY(CLAY_IDI("TabBtn", i), {
                    .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } },
                    .backgroundColor = tabBg
                }) {
                    CLAY_TEXT(ClayTuiBridge::toClayString(kTabLabels[i]), CLAY_TEXT_CONFIG({ .textColor = tabFg }));
                }

                if (i < 4) {
                    CLAY(CLAY_IDI("TabSep", i), {
                        .layout = {
                            .sizing = { CLAY_SIZING_FIXED(2), CLAY_SIZING_FIXED(1) },
                            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER }
                        }
                    }) {
                        CLAY_TEXT(CLAY_STRING("│"), CLAY_TEXT_CONFIG({ .textColor = { 80, 80, 80, 255 } }));
                    }
                }
            }

            // Flex spacer pushing shortcuts to right
            CLAY(CLAY_ID("BottomSpacer"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } }
            }) {}

            // Right-aligned keyboard shortcuts
            std::string help = "[SPACE] Play  [B] Sidebar  [F1-F5] Views  [Q] Exit";
            CLAY(CLAY_ID("BottomHelp"), {
                .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(1) } }
            }) {
                CLAY_TEXT(ClayTuiBridge::toClayString(help), CLAY_TEXT_CONFIG({ .textColor = { 128, 128, 128, 255 } }));
            }
        }
    }

    Clay_RenderCommandArray cmds = clayContext_->endLayout(0.016f);
    clayBridge_.render(cmds, surface_, area.x, area.y);

    // Update cached tab button regions from Clay layout solver
    for (int i = 0; i < 5; ++i) {
        auto tabData = clayContext_->getElementData(CLAY_IDI("TabBtn", i));
        if (tabData.found) {
            cachedTabBtns_[i] = ClayTuiBridge::snapToCells(tabData.boundingBox);
            cachedTabBtns_[i].x += area.x;
            cachedTabBtns_[i].y += area.y;
        }
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
