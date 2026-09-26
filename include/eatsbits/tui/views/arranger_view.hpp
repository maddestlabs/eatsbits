#pragma once

#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/audio_telemetry.hpp"
#include <string>
#include <vector>
#include <memory>

namespace eatsbits::tui {

struct AudioClip {
    std::string name;
    int startBar{0};     // 0-indexed bar position (e.g. 0, 4, 8)
    int lengthBars{4};   // Duration in bars
    Color color{Color::CyberCyan()};
    bool isLooping{true};
};

struct ArrangerTrack {
    std::string name;
    audio::NodeId nodeId{0};
    float volume{0.8f};
    float pan{0.5f};     // 0.0 = Left, 0.5 = Center, 1.0 = Right
    bool muted{false};
    bool solo{false};
    bool armed{false};
    std::vector<AudioClip> clips;
};

enum class DragMode {
    None,
    MoveClip,
    ResizeClip,
    ScrubVolume,
    ScrubPan
};

struct DragState {
    DragMode mode{DragMode::None};
    int trackIndex{-1};
    int clipIndex{-1};
    int startMouseX{0};
    int startMouseY{0};
    int originalStartBar{0};
    int originalLengthBars{0};
    float originalVal{0.0f};
};

class ArrangerView {
public:
    ArrangerView();
    ~ArrangerView() = default;

    void setupDefaultProject();

    void render(CellSurface& surface, const Rect& area, const TelemetrySnapshot& telemetry, bool isFocused);

    bool handleMouseDown(int mouseX, int mouseY, int button);
    bool handleMouseDrag(int mouseX, int mouseY);
    void handleMouseUp();
    void handleScroll(int delta);

    void toggleInspector() { showInspector_ = !showInspector_; }
    bool isInspectorOpen() const { return showInspector_; }

    int getSelectedTrackIndex() const { return selectedTrack_; }
    int getSelectedClipIndex() const { return selectedClip_; }
    const ArrangerTrack* getSelectedTrack() const;
    const AudioClip* getSelectedClip() const;

    std::vector<ArrangerTrack>& getTracks() { return tracks_; }
    const std::vector<ArrangerTrack>& getTracks() const { return tracks_; }

    static std::string getBraillePanGlyph(float pan);

private:
    std::vector<ArrangerTrack> tracks_;
    int selectedTrack_{0};
    int selectedClip_{-1};
    bool showInspector_{true};

    int colsPerBar_{6};     // Screen columns per bar in timeline grid
    int timelineScrollX_{0}; // Horizontal timeline scroll in bars
    int trackScrollY_{0};    // Vertical track scroll

    DragState dragState_;

    // Layout caching for hit-testing
    Rect cachedTimelineArea_{};
    Rect cachedTrackHeadersArea_{};
    Rect cachedInspectorArea_{};

    void renderRuler(CellSurface& surface, const Rect& area, int startBar, int numBars, const TelemetrySnapshot& telemetry);
    void renderTrackHeader(CellSurface& surface, const Rect& area, ArrangerTrack& track, int trackIdx, bool isSelected);
    void renderTrackLane(CellSurface& surface, const Rect& area, ArrangerTrack& track, int trackIdx, int startBar, int numBars, const TelemetrySnapshot& telemetry);
    void renderInspector(CellSurface& surface, const Rect& area);
};

} // namespace eatsbits::tui
