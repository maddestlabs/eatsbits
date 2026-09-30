#pragma once

#include "view_base.hpp"
#include "../widgets/plugin_search_dialog.hpp"
#include "../widgets/circle_of_fifths_dialog.hpp"
#include "../widgets/icon_search_dialog.hpp"
#include "../widgets/track_properties_drawer.hpp"
#include "../icon_registry.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include <vector>
#include <string>
#include <functional>
#include <chrono>

namespace eatsbits::ui {

enum class ArrangerInspectorTab {
    Track,
    Clip
};

using ArrangerMidiFxCard = TrackMidiFxItem;
using ArrangerAudioFxCard = TrackAudioFxItem;

struct ArrangerClipNote {
    uint8_t pitch{60};
    float startBeat{0.0f}; // relative to clip start, in beats (4 beats = 1 bar)
    float lengthBeats{1.0f};
    float velocity{0.85f};
};

struct ArrangerTimelineClip {
    std::string id;
    std::string name;
    uint32_t trackIndex{0};
    uint32_t startBar{1};
    uint32_t lengthBars{4};
    float r{1.0f}, g{0.55f}, b{0.0f};
    bool isSelected{false};
    bool isAudio{false};
    bool isLooped{false};
    uint32_t loopLengthBars{4};
    int transposeSemitones{0};
    float volumeScale{1.0f};
    bool mute{false};
    std::vector<ArrangerClipNote> notes;
    std::vector<theory::ChordEvent> detectedChords;
};

struct ArrangerTimelineTrack {
    std::string name;
    std::string instrument;
    std::string instrumentEngine{"tb303"};
    std::string iconRef{"preset:inst_synth"};
    float r{1.0f}, g{0.55f}, b{0.0f};
    float volume{0.8f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};
    theory::ChordFollowMode chordFollowMode{theory::ChordFollowMode::Off};
    int chordLeaderTrackIndex{-1}; // -1 = Independent / Self, >= 0 = Track to follow
    bool isChordLeader{false};     // Flag indicating track serves as a harmonic chord reference
    std::vector<ArrangerTimelineClip> clips;
    std::vector<ArrangerMidiFxCard> midiFx;
    std::vector<ArrangerAudioFxCard> audioFx;

    // 4 Authentic Hardware rotary sweep parameters
    float knob1{0.55f}; // e.g. Tone / Cutoff
    float knob2{0.75f}; // e.g. Snappy / Resonance
    float knob3{0.45f}; // e.g. Decay
    float knob4{0.80f}; // e.g. Var / Accent
    std::string knob1Name{"TONE"};
    std::string knob2Name{"SNAPPY"};
    std::string knob3Name{"DECAY"};
    std::string knob4Name{"VAR"};
};

/**
 * ArrangerView: Multi-track playlist and song arrangement workspace.
 * Features:
 * - Clip Loop Resize Handle (top right corner loop icon)
 * - Standard Clip Resize Handle (bottom right corner)
 * - Clip Movement across tracks and horizontally within same track
 * - Contextual properties drawer exposing Track Properties on track click, Clip Properties on clip click
 * - Dedicated '+ ADD' track option below the last track
 * - Shared contextual dialog for adding instruments, Audio FX, and MIDI FX
 * - Piano Roll style middle-click / spacebar 2D panning
 * - Interactive timeline minimap scrollbar for full song navigation
 */
class ArrangerView : public ViewBase {
public:
    ArrangerView();
    ~ArrangerView() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx) override;
    void render(const ViewContext& ctx) override;
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) override;
    bool handleFileDrop(const std::vector<std::string>& filePaths, float x, float y, const ViewContext& ctx) override;

    [[nodiscard]] bool isFollowPlayback() const noexcept { return followPlayback_; }
    void setFollowPlayback(bool follow) noexcept { followPlayback_ = follow; }
    void toggleFollowPlayback() noexcept { followPlayback_ = !followPlayback_; }

    [[nodiscard]] uint32_t getActiveTrack() const noexcept { return activeTrackIndex_; }
    [[nodiscard]] uint32_t getActiveTrackIndex() const noexcept { return activeTrackIndex_; }
    void setActiveTrack(uint32_t idx) noexcept;

    [[nodiscard]] int getSelectedClip() const noexcept { return selectedClipIndex_; }
    [[nodiscard]] int getSelectedClipIndex() const noexcept { return selectedClipIndex_; }
    void setSelectedClip(int idx) noexcept;

    [[nodiscard]] const std::vector<ArrangerTimelineTrack>& getTracks() const noexcept { return tracks_; }
    std::vector<ArrangerTimelineTrack>& getTracks() noexcept { return tracks_; }

    // Inspector Properties Drawer Controls (Decoupled & Shared TrackPropertiesDrawer)
    [[nodiscard]] bool isInspectorOpen() const noexcept { return propertiesDrawer_.isExpanded(); }
    void setInspectorOpen(bool open) noexcept { inspectorOpen_ = open; propertiesDrawer_.setExpanded(open); }
    void toggleInspector() noexcept { inspectorOpen_ = !inspectorOpen_; propertiesDrawer_.toggle(); }
    [[nodiscard]] float getInspectorWidth() const noexcept { return propertiesDrawer_.getWidth(); }
    void setInspectorWidth(float w) noexcept { inspectorWidth_ = w; propertiesDrawer_.setWidth(w); }

    [[nodiscard]] ArrangerInspectorTab getInspectorTab() const noexcept { return inspectorTab_; }
    void setInspectorTab(ArrangerInspectorTab tab) noexcept {
        inspectorTab_ = tab;
        drawerData_.tab = (tab == ArrangerInspectorTab::Clip) ? TrackPropertiesTab::Clip : TrackPropertiesTab::Track;
    }

    TrackPropertiesDrawer& getPropertiesDrawer() noexcept { return propertiesDrawer_; }
    const TrackPropertiesDrawer& getPropertiesDrawer() const noexcept { return propertiesDrawer_; }

    // Track addition / modification API
    void addTrack(const std::string& name, const std::string& instrument, float r, float g, float b);
    void addMidiFxToTrack(uint32_t trackIdx, const std::string& name, const std::string& type);
    void addAudioFxToTrack(uint32_t trackIdx, const std::string& name, const std::string& type);

    PluginSearchDialog& getPluginSearchDialog() noexcept { return pluginDialog_; }
    CircleOfFifthsDialog& getCircleOfFifthsDialog() noexcept { return circleOfFifthsDialog_; }
    IconSearchDialog& getIconSearchDialog() noexcept { return iconDialog_; }

    // Chord Track & Music Theory APIs (Eatsbeats parity)
    [[nodiscard]] const std::vector<theory::ChordEvent>& getChordTrack() const noexcept { return chordTrack_; }
    std::vector<theory::ChordEvent>& getChordTrack() noexcept { return chordTrack_; }
    void setChordTrack(std::vector<theory::ChordEvent> chords) { chordTrack_ = std::move(chords); }
    void addOrUpdateChord(const theory::ChordEvent& chord);
    void removeChord(const std::string& chordId);
    void clearChordTrack() noexcept { chordTrack_.clear(); }

    [[nodiscard]] const theory::ChordEvent* getActiveChordAtBar(float bar) const noexcept;
    [[nodiscard]] const theory::ChordEvent* getActiveChordAtStep(float step) const noexcept;

    void setSongKey(int rootPitchClass, bool isMinor) noexcept;
    [[nodiscard]] int getSongKeyRoot() const noexcept { return songKeyRoot_; }
    [[nodiscard]] bool isSongKeyMinor() const noexcept { return isSongKeyMinor_; }
    [[nodiscard]] std::string getSongKeyName() const;

    void applyChordProgressionPreset(const theory::ChordProgressionPreset& preset, uint32_t startBar = 0);
    uint32_t extractChordsFromTrack(uint32_t trackIdx);
    uint32_t extractChordsFromClip(uint32_t trackIdx, uint32_t clipIdx);
    void bakeChordsToTrack(uint32_t trackIdx);

    // Dynamic Clip Chords & Track-to-Track Harmonic Sync
    void updateClipDetectedChords(ArrangerTimelineClip& clip);
    void refreshAllClipChords();
    [[nodiscard]] const theory::ChordEvent* getActiveChordForTrackAtBar(uint32_t trackIdx, float bar) const noexcept;

    struct OverviewChordInfo {
        theory::ChordEvent chord;
        int sourceTrackIdx{-1};
        std::string sourceTrackName;
        int sourceClipIdx{-1};
        float startBar{0.0f};
        float barLength{1.0f};
    };
    [[nodiscard]] std::vector<OverviewChordInfo> getHarmonicOverviewChords() const;

    std::function<void(uint32_t trackIdx, float volume)> onVolumeChanged;
    std::function<void(uint32_t trackIdx, float pan)> onPanChanged;
    std::function<void(uint32_t trackIdx, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIdx, bool solo)> onSoloToggled;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(const theory::ChordEvent& chord)> onAuditionChord;
    std::function<void(uint32_t trackIdx, theory::ChordFollowMode mode)> onTrackChordFollowChanged;
    std::function<void()> onClipsChanged;
    std::function<void(uint32_t trackIdx)> onTrackSelected;
    std::function<void(uint32_t trackIdx, int clipIdx)> onEditClipInPianoRoll;
    std::function<void(uint32_t trackIdx, const std::string& newName)> onTrackRename;
    std::function<void(uint32_t trackIdx, const std::string& iconRef)> onTrackIconChanged;
    std::function<void(uint32_t trackIdx, size_t fxIdx, bool enabled)> onToggleAudioFx;
    std::function<void(uint32_t trackIdx, size_t fxIdx, bool enabled)> onToggleMidiFx;
    std::function<void(uint32_t trackIdx)> onAudioFxChanged;
    std::function<void(uint32_t trackIdx)> onMidiFxChanged;
    std::function<void(uint32_t trackIdx, size_t fxIdx, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIdx)> onOpenFullscreenDevice;
    std::function<void(uint32_t trackIdx, size_t fxIdx)> onOpenFullscreenAudioFx;
    std::function<void(uint32_t trackIdx, size_t fxIdx)> onOpenFullscreenMidiFx;

private:
    void renderGrid(const ViewContext& ctx);
    void renderChordLane(const ViewContext& ctx);
    void renderClips(const ViewContext& ctx);
    void renderTrackHeaders(const ViewContext& ctx);
    void renderRulerAndMinimap(const ViewContext& ctx);
    void renderPropertiesDrawer(const ViewContext& ctx);

    float trackHeaderWidth_{190.0f};
    float trackRowHeight_{64.0f};
    float barWidth_{64.0f};
    float rulerHeight_{28.0f};
    float minimapHeight_{16.0f};
    float chordLaneHeight_{30.0f};
    float scrollX_{0.0f};
    float scrollY_{0.0f};
    uint32_t totalBars_{32};

    bool followPlayback_{true};
    uint32_t activeTrackIndex_{0};
    int selectedClipIndex_{-1};
    int selectedChordIndex_{-1};
    std::chrono::steady_clock::time_point lastClipClickTime_{};
    int lastClickedClipIdx_{-1};
    int lastClickedClipTrack_{-1};
    std::chrono::steady_clock::time_point lastOverviewClickTime_{};
    int lastClickedOverviewChordIdx_{-1};

    // Properties Drawer State
    bool inspectorOpen_{true};
    ArrangerInspectorTab inspectorTab_{ArrangerInspectorTab::Track};
    float inspectorWidth_{310.0f};

    // Clip manipulation drag modes
    enum class ClipDragMode {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
        LoopResize,
        PlayheadScrub,
        ViewportPan,
        TouchPan,
        TouchGridPending,
        MinimapScrub,
        TrackVolume,
        TrackPan,
        InspectorKnob,
        InspectorVolume,
        InspectorPan,
        ChordMove,
        ChordResize
    } dragMode_{ClipDragMode::None};

    KineticScroller kineticScroller_;
    Point2D touchDownPos_{0.0f, 0.0f};
    std::chrono::steady_clock::time_point touchDownTimePoint_{};
    bool touchPanCommitted_{false};

    float dragStartPointerX_{0.0f};
    float dragStartPointerY_{0.0f};
    float dragStartScrollX_{0.0f};
    float dragStartScrollY_{0.0f};
    float dragStartVal_{0.0f};
    int dragKnobIdx_{-1};
    uint32_t dragOrigStartBar_{1};
    uint32_t dragOrigLengthBars_{4};
    uint32_t dragOrigTrackIdx_{0};
    int dragClipIdx_{-1};

    // Chord drag tracking
    int dragChordIdx_{-1};
    uint32_t dragChordOrigStartBar_{0};
    float dragChordOrigLength_{1.0f};

    // Layout Bounds
    Rect2D rulerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D minimapBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D chordLaneBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D chordHeaderBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D tracksListBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D gridBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D inspectorBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D addTrackRowBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    std::vector<ArrangerTimelineTrack> tracks_;

    // Harmonic Chord Track & Song Key State
    std::vector<theory::ChordEvent> chordTrack_;
    int songKeyRoot_{0}; // C
    bool isSongKeyMinor_{false};

    // Shared Decoupled Track Properties Drawer
    TrackPropertiesDrawer propertiesDrawer_;
    TrackPropertiesDrawerData drawerData_;

    // Contextual Dialogs
    PluginSearchDialog pluginDialog_;
    CircleOfFifthsDialog circleOfFifthsDialog_;
    IconSearchDialog iconDialog_;
};

} // namespace eatsbits::ui
