#pragma once

#include "track_properties_panel.hpp"
#include "sliding_dock.hpp"

namespace eatsbits::ui {

/**
 * TrackPropertiesDrawer: Collapsible sliding drawer widget hosting the unified TrackPropertiesPanel
 * for sidebar usage in Arranger and Mixer tabs.
 *
 * Provides:
 * - Vertical pull-tab strip with icon and rotation title
 * - Drag-resize handle on left edge
 * - Close button and tab switcher
 * - Hosts and delegates content to TrackPropertiesPanel
 */
class TrackPropertiesDrawer {
public:
    TrackPropertiesDrawer();
    ~TrackPropertiesDrawer() = default;

    void layout(const Rect2D& containerBounds, float browserOffset = 0.0f);
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data, float mouseX = -1.0f, float mouseY = -1.0f, float dt = 0.016f);
    [[nodiscard]] TrackPropertiesHitResult hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept;
    bool handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
        if (drawer_.handleKey(key, scancode, action, mods, ctx)) return true;
        return panel_.handleKey(key, scancode, action, mods, ctx);
    }
    bool handleChar(char32_t codepoint) {
        return panel_.handleChar(codepoint);
    }

    [[nodiscard]] bool isExpanded() const noexcept { return drawer_.isExpanded(); }
    void setExpanded(bool exp) noexcept { drawer_.setExpanded(exp); }
    void toggle() noexcept { drawer_.toggle(); }

    [[nodiscard]] float getAnimProgress() const noexcept { return drawer_.getAnimProgress(); }
    void setAnimProgress(float p) noexcept { drawer_.setAnimProgress(p); }
    [[nodiscard]] bool isAnimating() const noexcept { return drawer_.isAnimating(); }
    [[nodiscard]] float getEffectiveWidth() const noexcept { return kPullTabWidth + (drawer_.getExtent() * drawer_.getAnimProgress()); }
    [[nodiscard]] bool isPluginDialogOpen() const noexcept { return panel_.getPluginSearchDialog().isOpen(); }
    [[nodiscard]] bool isColorPickerOpen() const noexcept { return panel_.isColorPickerOpen(); }
    [[nodiscard]] ColorPickerDialog& getColorPickerDialog() noexcept { return panel_.getColorPickerDialog(); }
    [[nodiscard]] const ColorPickerDialog& getColorPickerDialog() const noexcept { return panel_.getColorPickerDialog(); }
    [[nodiscard]] bool isDragging() const noexcept { return panel_.isDragging() || drawer_.isDragging(); }

    [[nodiscard]] float getWidth() const noexcept { return drawer_.getExtent(); }
    void setWidth(float w) noexcept { drawer_.setExtent(w); }

    [[nodiscard]] const Rect2D& getPullTabBounds() const noexcept { return drawer_.getPullTabBounds(); }
    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawer_.getDrawerBounds(); }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return drawer_.getCloseButtonBounds(); }

    [[nodiscard]] SlidingDrawer& getDrawer() noexcept { return drawer_; }
    [[nodiscard]] const SlidingDrawer& getDrawer() const noexcept { return drawer_; }

    [[nodiscard]] float getScrollY() const noexcept { return panel_.getScrollY(); }
    void setScrollY(float sy) noexcept { panel_.setScrollY(sy); }

    [[nodiscard]] TrackPropertiesPanel& getPanel() noexcept { return panel_; }
    [[nodiscard]] const TrackPropertiesPanel& getPanel() const noexcept { return panel_; }

    [[nodiscard]] PluginSearchDialog& getPluginSearchDialog() noexcept { return panel_.getPluginSearchDialog(); }
    [[nodiscard]] const PluginSearchDialog& getPluginSearchDialog() const noexcept { return panel_.getPluginSearchDialog(); }

    [[nodiscard]] std::string getTooltip(float x, float y, const TrackPropertiesDrawerData& data) const noexcept;

    static constexpr float kPullTabWidth = 24.0f;
    static constexpr float kDefaultWidth = 460.0f;
    static constexpr float kMinWidth = 300.0f;
    static constexpr float kMaxWidth = 760.0f;

    // Decoupled callbacks for shared usage in Arranger and Mixer tabs
    std::function<void(uint32_t trackIndex)> onTrackSelected;
    std::function<void(uint32_t trackIndex)> onTrackRename;
    std::function<void(uint32_t trackIndex, const std::string& newName)> onTrackRenameWithText;
    std::function<void(uint32_t trackIndex)> onChooseTrackIcon;
    std::function<void(float scrollY)> onScrollChanged;
    std::function<void(uint32_t trackIndex, float volume)> onVolumeChanged;
    std::function<void(uint32_t trackIndex, float pan)> onPanChanged;
    std::function<void(uint32_t trackIndex, bool mute)> onMuteToggled;
    std::function<void(uint32_t trackIndex, bool solo)> onSoloToggled;
    std::function<void(uint32_t trackIndex, bool freeze)> onFreezeToggled;
    std::function<void(uint32_t trackIndex, float r, float g, float b)> onColorChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(uint32_t trackIndex)> onChangeInstrument;
    std::function<void(uint32_t trackIndex)> onOpenDesign;
    std::function<void(uint32_t trackIndex)> onOpenPresets;
    std::function<void(uint32_t trackIndex)> onOpenFullscreenDevice;
    std::function<void(uint32_t trackIndex)> onOpenDetachedWindow;
    std::function<void()> onPrevPreset;
    std::function<void()> onNextPreset;
    std::function<void(TrackPropertiesTab newTab)> onTabSelected;
    std::function<void(uint32_t trackIndex, int clipIndex)> onEditInPianoRoll;
    std::function<void(uint32_t trackIndex)> onOpenCodeEditor;
    std::function<void(uint32_t trackIndex)> onAddMidiFx;
    std::function<void(uint32_t trackIndex)> onAddAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onRemoveMidiFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onRemoveAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex, bool enabled)> onToggleMidiFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex, bool enabled)> onToggleAudioFx;
    std::function<void(uint32_t trackIndex, size_t fromIdx, size_t toIdx)> onReorderMidiFx;
    std::function<void(uint32_t trackIndex, size_t fromIdx, size_t toIdx)> onReorderAudioFx;
    std::function<void(uint32_t trackIndex)> onMidiFxChanged;
    std::function<void(uint32_t trackIndex)> onAudioFxChanged;
    std::function<void(uint32_t trackIndex, size_t fxIndex, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onMidiFxParamChanged;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenMidiFx;
    std::function<void(uint32_t trackIndex)> onAddClip;
    std::function<void(uint32_t trackIndex)> onDeleteTrack;
    std::function<void(uint32_t trackIndex)> onDuplicateTrack;
    std::function<void(uint32_t trackIndex, int clipIndex)> onDuplicateClip;
    std::function<void(uint32_t trackIndex, int clipIndex)> onDeleteClip;
    std::function<void(uint32_t trackIndex, ChordFollowMode mode)> onChordFollowChanged;
    std::function<void(uint32_t trackIndex)> onBakeChords;
    std::function<void(uint32_t trackIndex, uint8_t note, float velocity)> onAuditionNote;

private:
    SlidingDrawer drawer_;
    float browserOffset_{0.0f};
    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    TrackPropertiesPanel panel_;
};

} // namespace eatsbits::ui
