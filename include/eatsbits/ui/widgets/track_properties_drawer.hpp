#pragma once

#include "track_properties_panel.hpp"

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
        return panel_.handleKey(key, scancode, action, mods, ctx);
    }

    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    void setExpanded(bool exp) noexcept { isExpanded_ = exp; }
    void toggle() noexcept { isExpanded_ = !isExpanded_; }

    [[nodiscard]] float getAnimProgress() const noexcept { return animProgress_; }
    [[nodiscard]] bool isAnimating() const noexcept { return std::abs(animProgress_ - (isExpanded_ ? 1.0f : 0.0f)) > 0.001f; }
    [[nodiscard]] float getEffectiveWidth() const noexcept { return kPullTabWidth + (width_ * animProgress_); }
    [[nodiscard]] bool isPluginDialogOpen() const noexcept { return panel_.getPluginSearchDialog().isOpen(); }
    [[nodiscard]] bool isDragging() const noexcept { return panel_.isDragging() || isResizing_; }

    [[nodiscard]] float getWidth() const noexcept { return width_; }
    void setWidth(float w) noexcept { width_ = std::clamp(w, minWidth_, maxWidth_); }

    [[nodiscard]] const Rect2D& getPullTabBounds() const noexcept { return pullTabBounds_; }
    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return closeButtonBounds_; }

    [[nodiscard]] float getScrollY() const noexcept { return panel_.getScrollY(); }
    void setScrollY(float sy) noexcept { panel_.setScrollY(sy); }

    [[nodiscard]] TrackPropertiesPanel& getPanel() noexcept { return panel_; }
    [[nodiscard]] const TrackPropertiesPanel& getPanel() const noexcept { return panel_; }

    [[nodiscard]] PluginSearchDialog& getPluginSearchDialog() noexcept { return panel_.getPluginSearchDialog(); }
    [[nodiscard]] const PluginSearchDialog& getPluginSearchDialog() const noexcept { return panel_.getPluginSearchDialog(); }

    static constexpr float kPullTabWidth = 24.0f;
    static constexpr float kDefaultWidth = 360.0f;
    static constexpr float kMinWidth = 260.0f;
    static constexpr float kMaxWidth = 640.0f;

    // Decoupled callbacks for shared usage in Arranger and Mixer tabs
    std::function<void(uint32_t trackIndex)> onTrackSelected;
    std::function<void(uint32_t trackIndex)> onTrackRename;
    std::function<void(uint32_t trackIndex, const std::string& newName)> onTrackRenameWithText;
    std::function<void(uint32_t trackIndex)> onChooseTrackIcon;
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
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onMidiFxParamChanged;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenAudioFx;
    std::function<void(uint32_t trackIndex, size_t fxIndex)> onOpenFullscreenMidiFx;

private:
    bool isExpanded_{true};
    float animProgress_{1.0f};
    float browserOffset_{0.0f};
    bool isResizing_{false};
    float width_{kDefaultWidth};
    float minWidth_{kMinWidth};
    float maxWidth_{kMaxWidth};

    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D pullTabBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeButtonBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    float resizeStartX_{0.0f};
    float resizeStartWidth_{kDefaultWidth};

    TrackPropertiesPanel panel_;
};

} // namespace eatsbits::ui
