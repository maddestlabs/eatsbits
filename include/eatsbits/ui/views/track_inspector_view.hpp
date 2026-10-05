#pragma once

#include "view_base.hpp"
#include "../widgets/track_properties_panel.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>

namespace eatsbits::ui {

struct InspectorTrackChannel {
    std::string name{"303 Acid Bass"};
    std::string type{"SYNTH"};
    float r{1.0f}, g{0.55f}, b{0.0f};
    float volume{0.85f};
    float pan{0.0f};
    bool mute{false};
    bool solo{false};
    bool freeze{false};
    std::string instrument{"Roland TB-303"};
    std::string instrumentEngine{"tb303"}; // "tb303", "tr808", "tr909", "dx7", "snes", "c64", "convolver", "piano"
    ChordFollowMode chordFollowMode{ChordFollowMode::Off};
    std::vector<InspectorKnobDef> knobs;
    TrackMidiFxData midiFx;
    TrackAudioFxData audioFx;
    std::vector<TrackMidiFxItem> midiFxList;
    std::vector<TrackAudioFxItem> audioFxList;
};

/**
 * TrackInspectorView: Channel hardware faceplate, mixer settings & audio/MIDI FX rack.
 * Full Eatsbeats parity, delegating to the unified TrackPropertiesPanel component.
 */
class TrackInspectorView : public ViewBase {
public:
    TrackInspectorView();
    ~TrackInspectorView() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx) override;
    void render(const ViewContext& ctx) override;
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) override;
    bool handleChar(char32_t codepoint, const ViewContext& ctx) override;

    // Track Navigation & Accessors
    void setActiveTrack(uint32_t idx) noexcept;
    [[nodiscard]] uint32_t getActiveTrack() const noexcept { return selectedTrackIndex_; }
    [[nodiscard]] size_t getTrackCount() const noexcept { return tracks_.size(); }
    [[nodiscard]] const InspectorTrackChannel& getTrack(size_t idx) const;
    [[nodiscard]] InspectorTrackChannel& getTrack(size_t idx);
    [[nodiscard]] const std::vector<InspectorTrackChannel>& getTracks() const noexcept { return tracks_; }
    [[nodiscard]] std::vector<InspectorTrackChannel>& getTracks() noexcept { return tracks_; }

    // Live Oscilloscope Audio Buffer
    void setAudioScopeBuffer(const float* buffer, size_t count);

    // Preset & Preset Counter
    void setPresetInfo(size_t activePresetIdx, size_t totalPresets, const std::string& presetTitle, const std::string& presetSubtitle);

    // Reusable Plugin Search Dialog
    [[nodiscard]] PluginSearchDialog& getPluginSearchDialog() noexcept { return panel_.getPluginSearchDialog(); }
    [[nodiscard]] const PluginSearchDialog& getPluginSearchDialog() const noexcept { return panel_.getPluginSearchDialog(); }

    // Unified TrackPropertiesPanel Accessor
    [[nodiscard]] TrackPropertiesPanel& getPanel() noexcept { return panel_; }
    [[nodiscard]] const TrackPropertiesPanel& getPanel() const noexcept { return panel_; }

    // Synchronization from Window
    void syncFromWindow(
        const std::vector<std::string>& trackNames,
        const std::vector<float>& trackVols,
        const std::vector<float>& trackPans,
        const std::vector<bool>& trackMutes,
        const std::vector<bool>& trackSolos,
        const std::vector<bool>& trackFreezes,
        const std::vector<Color>& trackColors,
        uint32_t selectedTrackIdx,
        const std::string& activePresetTitle,
        const std::string& activePresetSubtitle,
        size_t activePresetIdx,
        size_t totalPresets,
        float scrollY
    );

    // Callbacks for Window & Audio Engine synchronization
    std::function<void(uint32_t trackIdx)> onTrackSelected;
    std::function<void(uint32_t trackIdx, float vol)> onVolumeChanged;
    std::function<void(uint32_t trackIdx, float pan)> onPanChanged;
    std::function<void(uint32_t trackIdx, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIdx, bool solo)> onSoloToggled;
    std::function<void(uint32_t trackIdx, bool freeze)> onFreezeToggled;
    std::function<void(uint32_t trackIdx, float r, float g, float b)> onColorChanged;
    std::function<void(uint32_t trackIdx)> onOpenCodeEditor;
    std::function<void(uint32_t trackIdx)> onChangeInstrument;
    std::function<void()> onPrevPreset;
    std::function<void()> onNextPreset;
    std::function<void(uint32_t trackIdx)> onAddMidiFx;
    std::function<void(uint32_t trackIdx)> onAddAudioFx;
    std::function<void(uint32_t trackIdx, ChordFollowMode mode)> onChordFollowChanged;
    std::function<void(uint32_t trackIdx)> onBakeChords;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onMidiFxParamChanged;
    std::function<void(uint32_t trackIdx, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIdx, size_t fxIdx)> onRemoveMidiFx;
    std::function<void(uint32_t trackIdx, size_t fxIdx)> onRemoveAudioFx;
    std::function<void(uint32_t trackIdx, size_t fxIdx, bool enabled)> onToggleMidiFx;
    std::function<void(uint32_t trackIdx, size_t fxIdx, bool enabled)> onToggleAudioFx;
    std::function<void(uint32_t trackIdx, size_t fromIdx, size_t toIdx)> onReorderMidiFx;
    std::function<void(uint32_t trackIdx, size_t fromIdx, size_t toIdx)> onReorderAudioFx;
    std::function<void(uint32_t trackIdx)> onMidiFxChanged;
    std::function<void(uint32_t trackIdx)> onAudioFxChanged;
    std::function<void(float scrollY)> onScrollChanged;
    std::function<void(uint32_t trackIdx)> onOpenFullscreenDevice;
    std::function<void(uint32_t trackIdx, size_t fxIdx)> onOpenFullscreenAudioFx;
    std::function<void(uint32_t trackIdx, size_t fxIdx)> onOpenFullscreenMidiFx;
    std::function<void(uint32_t trackIdx)> onAddClip;
    std::function<void(uint32_t trackIdx)> onDeleteTrack;
    std::function<void(uint32_t trackIdx)> onDuplicateTrack;
    std::function<void(uint32_t trackIdx, int clipIdx)> onDuplicateClip;
    std::function<void(uint32_t trackIdx, int clipIdx)> onDeleteClip;

    [[nodiscard]] float getScrollY() const noexcept { return panel_.getScrollY(); }
    void setScrollY(float y) noexcept { panel_.setScrollY(y); }

private:
    void initDefaultTracks();
    void syncKnobsForTrack(InspectorTrackChannel& trk);
    TrackPropertiesDrawerData buildDrawerData() const;
    void syncBackFromDrawerData(const TrackPropertiesDrawerData& d);

    uint32_t selectedTrackIndex_{0};
    std::vector<InspectorTrackChannel> tracks_;

    float scopeBuffer_[256]{0.0f};
    size_t scopeBufferCount_{256};

    size_t activePresetIdx_{0};
    size_t totalPresets_{156};
    std::string presetTitle_{"TB-303 Acid Bassline"};
    std::string presetSubtitle_{"Diode Ladder Synthesizer"};

    TrackPropertiesPanel panel_;
};

} // namespace eatsbits::ui
