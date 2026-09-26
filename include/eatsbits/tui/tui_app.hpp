#pragma once

#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/tui/terminal_device.hpp"
#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/ansi_diff_renderer.hpp"
#include "eatsbits/tui/input_parser.hpp"
#include "eatsbits/tui/audio_telemetry.hpp"
#include "eatsbits/tui/views/arranger_view.hpp"
#include "eatsbits/tui/views/braille_scope.hpp"
#include "eatsbits/tui/views/tracker_grid.hpp"
#include "eatsbits/tui/views/param_rack.hpp"
#include <string>

namespace eatsbits::tui {

enum class WorkspaceTab {
    Arranger = 0, // Default
    Edit,         // Tracker / Step Sequencer
    Track,        // Device & Synth Param Rack
    Mixer,        // Oscilloscope & Master Meters
    Design        // Live Scripting / Graph
};

class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    bool init();
    void run();
    void stop();

    audio::AudioEngine& getAudioEngine() { return audioEngine_; }
    CellSurface& getSurface() { return surface_; }

    void setWorkspaceTab(WorkspaceTab tab) { activeTab_ = tab; }
    WorkspaceTab getWorkspaceTab() const { return activeTab_; }

private:
    bool running_{false};
    WorkspaceTab activeTab_{WorkspaceTab::Arranger};
    bool recording_{false};
    bool isDragging_{false};

    audio::AudioEngine audioEngine_;
    TerminalDevice terminalDevice_;
    CellSurface surface_;
    AnsiDiffRenderer diffRenderer_;
    InputParser inputParser_;
    AudioTelemetryBridge telemetryBridge_;

    ArrangerView arrangerView_;
    BrailleScopeView scopeView_;
    TrackerGridView trackerGridView_;
    ParamRackView paramRackView_;

    std::string outFlushBuffer_;
    int screenCols_{120};
    int screenRows_{36};

    // Cached hit regions
    Rect cachedTopPanel_{};
    Rect cachedPlayBtn_{};
    Rect cachedStopBtn_{};
    Rect cachedRecBtn_{};
    Rect cachedBrowserBtn_{};
    Rect cachedBottomPanel_{};
    Rect cachedTabBtns_[5]{};

    void setupAudioSession();
    void processInput();
    void handleKeyEvent(const KeyEvent& key);
    void handleMouseEvent(const MouseEvent& mouse);
    void updateAndRender(bool forceRedraw = false);

    void renderTopPanel(const Rect& area, const TelemetrySnapshot& telemetry);
    void renderBottomPanel(const Rect& area);
    void renderDesignView(const Rect& area);
};

} // namespace eatsbits::tui
