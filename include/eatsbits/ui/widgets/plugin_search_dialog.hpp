#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "scrollable_area.hpp"
#include "eatsbits/project/preset_manager.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

enum class PluginDialogMode {
    AddInstrument,
    AddMidiFx,
    AddAudioFx,
    SelectPreset,
    AddDrums,
    AddMidiSeq,
    AddUtility
};

using PresetDialogMode = PluginDialogMode;

struct PluginEntry {
    std::string id;
    std::string name;
    std::string category;
    std::string engineTag;
    std::string description;
    float r{0.13f}, g{0.96f}, b{0.91f};
    std::string author{""};
    std::string filePath{""};
};

/**
 * Reusable Contextual Dialog for searching, browsing, and applying Presets, Instruments, and FX.
 * Inspired by Eatsbeats PresetSearchDialog / ScriptSearchDialog.
 * Features category filter pills, real-time search filtering, keyboard arrow navigation,
 * and categorized cards with badges and colors.
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
    std::function<void(const project::PresetItem& item, uint32_t trackIndex)> onPresetSelected;
    std::function<void()> onClose;

private:
    void initLibrary();
    [[nodiscard]] std::vector<PluginEntry> getFilteredEntries() const;
    void selectHighlightedCard();
    void scrollIndexIntoView(int index);

    bool isOpen_{false};
    PluginDialogMode mode_{PluginDialogMode::AddInstrument};
    std::string targetTrackName_{""};
    uint32_t targetTrackIndex_{0};

    std::string searchQuery_{""};
    int selectedCategoryIndex_{0}; // 0 = ALL
    int selectedItemIndex_{0};     // For keyboard navigation

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};
    Rect2D dialogBounds_{0.0f, 0.0f, 620.0f, 540.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D searchBoxBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    ScrollableArea scrollArea_;
    float scrollY_{0.0f};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
    bool isDraggingScroll_{false};
    float dragStartY_{0.0f};
    float dragStartScrollY_{0.0f};

    std::vector<PluginEntry> allPresetLibrary_;
    std::vector<PluginEntry> instrumentLibrary_;
    std::vector<PluginEntry> midiFxLibrary_;
    std::vector<PluginEntry> audioFxLibrary_;
    std::vector<PluginEntry> drumsLibrary_;
    std::vector<PluginEntry> midiSeqLibrary_;
    std::vector<PluginEntry> utilityLibrary_;

    std::vector<std::string> selectPresetCategories_{"ALL", "INSTRUMENTS", "DRUMS", "AUDIO FX", "MIDI FX", "MIDI SEQ", "UTILITY"};
    std::vector<std::string> instrumentCategories_{"ALL", "SYNTHS", "DRUMS", "PHYSICAL MODELING", "KEYS", "GUITAR & BASS", "RETRO CHIP"};
    std::vector<std::string> midiFxCategories_{"ALL", "SCALE & KEY", "ARPEGGIATOR", "HUMANIZE", "CHORDS"};
    std::vector<std::string> audioFxCategories_{"ALL", "DISTORTION", "DELAY & REVERB", "MODULATION", "DYNAMICS & EQ"};
    std::vector<std::string> drumsCategories_{"ALL", "808", "909", "ACOUSTIC", "ELECTRONIC", "PERCUSSION"};
    std::vector<std::string> midiSeqCategories_{"ALL", "ACID", "TECHNO", "HOUSE", "CHORDS", "GROOVES"};
    std::vector<std::string> utilityCategories_{"ALL", "SPLITTER", "MACRO", "ACTION"};
};

using PresetSearchDialog = PluginSearchDialog;

} // namespace eatsbits::ui
