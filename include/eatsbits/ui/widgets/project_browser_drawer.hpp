#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"

namespace eatsbits::ui {

enum class BrowserDrawerTab {
    Assets = 0,   // Tab 1: Project Assets (Tracks, Clips, FX inserts)
    Scripts = 1,  // Tab 2: Script & Engine Library (Synths, DSP, Audio FX, MIDI FX, Sequences, Macros)
    Macros = 1,   // Backward-compatibility alias for Scripts & Macros tab
    Presets = 2,  // Tab 3: Preset Library (Sound Patches, Room Acoustic Spaces, Amp Cabinets)
    Packs = 3,    // Tab 4: Expansion Packs (SoundFonts, Audio-to-MIDI / AI tools)
    Projects = 4, // Tab 5: Local Saved Projects (Projects/*.eats file manager)
    History = 5   // Tab 6: History & Time Travel (Undo/Redo, Milestones, Checkpoints)
};

struct BrowserTrackAssetItem {
    uint32_t index{0};
    std::string name;
    std::string type;       // "SYNTH", "DRUMS", "AUDIO", "FOLDER"
    size_t clipCount{0};
    bool isMuted{false};
    bool isSolo{false};
    Color color{0.0f, 0.95f, 1.0f, 1.0f};
};

struct BrowserScriptItem {
    std::string id;
    std::string name;
    std::string category;   // "SYNTH", "AUDIO_FX", "MIDI_FX", "MIDI_SEQ", "MACRO"
    std::string description;
    std::string tags;
};

struct BrowserSoundPatchItem {
    std::string id;
    std::string name;
    std::string category;   // "BASS", "LEAD", "PAD", "PLUCK", "DRUMS", "SPACE", "CAB"
    std::string engineTag;
    std::string description;
    std::string meta;       // e.g. "RT60: 2.4s", "Mic: 15cm", "Roland TB-303"
};

struct BrowserPackItem {
    std::string id;
    std::string title;
    std::string description;
    float sizeMb{0.0f};
    bool isInstalled{false};
    std::string category;   // "SOUNDFONT", "AI_TOOL"
};

struct BrowserSavedProjectItem {
    std::string name;
    std::string fileName;
    std::string filePath;
    uint64_t sizeBytes{0};
    std::string lastModified;
};

struct BrowserHistoryMilestoneItem {
    size_t stepIndex{0};
    std::string description;
    std::string category;   // "EDIT", "TUNE", "NOTE", "ARRANGE"
    std::string timestamp;
    bool isMilestone{false};
    bool isCurrent{false};
};

/**
 * Slide-in Project Browser Drawer (Modular Subsystem Pattern).
 * Slides in from the right edge with smooth animated transition.
 * Houses 6 workstation hubs:
 * 1. Project Assets (Tracks, Clips, Active FX)
 * 2. Script & Engine Library (Synths, Audio FX, MIDI FX, Sequences, Macros)
 * 3. Preset Library (Sound Patches, Room Acoustic Spaces, Amp Cabinets)
 * 4. Expansion Packs (SoundFonts, Audio-to-MIDI)
 * 5. Local Saved Projects (Projects/*.eats file manager)
 * 6. History & Time Travel (Undo/Redo stack and milestones)
 */
class ProjectBrowserDrawer {
public:
    ProjectBrowserDrawer();
    ~ProjectBrowserDrawer() = default;

    void layout(float screenWidth, float screenHeight, float topHeaderHeight, float bottomNavHeight);
    void update(float dt);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);

    bool handlePointer(const PointerEvent& ev);

    void open() noexcept;
    void close() noexcept;
    void toggle() noexcept;
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }
    [[nodiscard]] bool isAnimating() const noexcept {
        return std::abs(animProgress_ - (isOpen_ ? 1.0f : 0.0f)) > 0.001f;
    }
    [[nodiscard]] float getAnimOffset() const noexcept { return animOffset_; }
    [[nodiscard]] Rect2D getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] static constexpr float getDrawerWidth() noexcept { return kDrawerWidth; }

    void setTab(BrowserDrawerTab tab) noexcept;
    [[nodiscard]] BrowserDrawerTab getTab() const noexcept { return activeTab_; }

    void setSearchQuery(const std::string& q) { searchQuery_ = q; }
    [[nodiscard]] const std::string& getSearchQuery() const noexcept { return searchQuery_; }

    void setCategoryFilter(const std::string& cat) { selectedCategory_ = cat; }
    [[nodiscard]] const std::string& getCategoryFilter() const noexcept { return selectedCategory_; }

    // Live state updates from engine / sequencer / history
    void setTracks(const std::vector<BrowserTrackAssetItem>& tracks);
    void setHistory(const std::vector<BrowserHistoryMilestoneItem>& history, bool canUndo, bool canRedo);
    void scanSavedProjects();

    // Callbacks
    std::function<void(uint32_t trackIndex)> onSelectTrack;
    std::function<void(const std::string& presetId)> onSelectPreset;
    std::function<void(const std::string& macroId)> onRunMacro;
    std::function<void(const std::string& scriptId)> onRunScript;
    std::function<void(const std::string& presetId)> onAddPresetTrack;
    std::function<void(const std::string& fxId)> onAddAudioFx;
    std::function<void(const std::string& fxId)> onAddMidiFx;
    std::function<void(const std::string& filePath)> onLoadProject;
    std::function<void(const std::string& filePath)> onSaveProject;
    std::function<void()> onSaveProjectAs;
    std::function<void()> onOpenProjectsFolder;
    std::function<void(const std::string& filePath)> onDeleteProject;
    std::function<void()> onUndo;
    std::function<void()> onRedo;
    std::function<void(size_t historyIndex)> onJumpToHistory;
    std::function<void(const std::string& name)> onCreateCheckpoint;
    std::function<void()> onClearHistory;
    std::function<void()> onLaunchAudioToMidi;
    std::function<void()> onClose;

private:
    void initData();
    std::string formatBytes(uint64_t bytes) const;

    bool isOpen_{false};
    float animProgress_{0.0f}; // 0.0 (closed) to 1.0 (open)
    float animOffset_{380.0f}; // Pixels offscreen to the right

    static constexpr float kDrawerWidth = 380.0f;

    BrowserDrawerTab activeTab_{BrowserDrawerTab::Assets};
    std::string searchQuery_{""};
    std::string selectedCategory_{"ALL"};
    int selectedIndex_{0};
    float scrollOffset_{0.0f};
    float mouseX_{0.0f};
    float mouseY_{0.0f};
    bool isDraggingScroll_{false};
    float dragStartY_{0.0f};
    float dragStartOffset_{0.0f};

    bool canUndo_{false};
    bool canRedo_{false};

    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D searchBoxBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // 6 Tab Bounds
    std::array<Rect2D, 6> tabBounds_{};

    // Data stores
    std::vector<BrowserTrackAssetItem> tracks_;
    std::vector<BrowserScriptItem> scripts_;
    std::vector<BrowserSoundPatchItem> patches_;
    std::vector<BrowserPackItem> packs_;
    std::vector<BrowserSavedProjectItem> savedProjects_;
    std::vector<BrowserHistoryMilestoneItem> history_;
};

} // namespace eatsbits::ui
