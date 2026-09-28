#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "scrollable_area.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

enum class PluginDialogMode {
    AddInstrument,
    AddMidiFx,
    AddAudioFx
};

struct PluginEntry {
    std::string id;
    std::string name;
    std::string category;
    std::string engineTag;
    std::string description;
    float r{0.13f}, g{0.96f}, b{0.91f};
};

/**
 * Reusable Contextual Dialog for adding/changing Instruments, Audio FX, and MIDI FX.
 * Inspired by Eatsbeats PresetSearchDialog / ScriptSearchDialog.
 * Features category filter pills, real-time search filtering, and categorized cards with badges.
 */
class PluginSearchDialog {
public:
    PluginSearchDialog();
    ~PluginSearchDialog() = default;

    void open(PluginDialogMode mode, const std::string& targetTrackName = "", uint32_t targetTrackIndex = 0);
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] PluginDialogMode getMode() const noexcept { return mode_; }
    [[nodiscard]] uint32_t getTargetTrackIndex() const noexcept { return targetTrackIndex_; }

    std::function<void(PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex)> onPluginSelected;
    std::function<void()> onClose;

private:
    void initLibrary();
    [[nodiscard]] std::vector<PluginEntry> getFilteredEntries() const;

    bool isOpen_{false};
    PluginDialogMode mode_{PluginDialogMode::AddInstrument};
    std::string targetTrackName_{""};
    uint32_t targetTrackIndex_{0};

    std::string searchQuery_{""};
    int selectedCategoryIndex_{0}; // 0 = ALL

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};
    Rect2D dialogBounds_{0.0f, 0.0f, 560.0f, 520.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D searchBoxBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    ScrollableArea scrollArea_;
    float scrollY_{0.0f};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
    bool isDraggingScroll_{false};
    float dragStartY_{0.0f};
    float dragStartScrollY_{0.0f};

    std::vector<PluginEntry> instrumentLibrary_;
    std::vector<PluginEntry> midiFxLibrary_;
    std::vector<PluginEntry> audioFxLibrary_;

    std::vector<std::string> instrumentCategories_{"ALL", "SYNTHS", "DRUMS", "BASS", "ACOUSTIC", "FM / RETRO"};
    std::vector<std::string> midiFxCategories_{"ALL", "SCALE & KEY", "ARPEGGIATOR", "HUMANIZE", "CHORDS"};
    std::vector<std::string> audioFxCategories_{"ALL", "DISTORTION", "DELAY & REVERB", "MODULATION", "DYNAMICS & EQ"};
};

} // namespace eatsbits::ui
