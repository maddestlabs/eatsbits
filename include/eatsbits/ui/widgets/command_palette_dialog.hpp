#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"

namespace eatsbits::ui {

enum class CommandCategory : uint8_t {
    All = 0,
    View = 1,
    Action = 2,
    Preset = 3,
    Theme = 4,
    Macro = 5
};

struct QuickCommand {
    std::string id;
    std::string title;
    std::string subtitle;
    CommandCategory category{CommandCategory::Action};
    std::string shortcutHint;
    std::function<void()> onExecute;
};

/**
 * Universal Quick Command Palette Dialog (Ctrl+P / Ctrl+K / Search).
 * Provides a floating, keyboard-driven spotlight command runner across:
 * - Workstation navigation (Arranger, Piano Roll, Inspector, Mixer, Modular Rack, Scripts)
 * - DAW actions (Play/Pause, Panic/Stop, Record, Loop, Metronome, Export, Circle of Fifths)
 * - Preset hot-swapping (TB-303, TR-808, TR-909, DX7 Piano, etc.)
 * - Theme selection (Cyberpunk Neon, Tokyo Night, Vaporwave, Charcoal Studio, etc.)
 * - Eatscript macros (Procedural Acid, 909 Techno, Humanize, Arpeggiator)
 */
class CommandPaletteDialog {
public:
    CommandPaletteDialog();
    ~CommandPaletteDialog() = default;

    void open() noexcept;
    void close() noexcept;
    void toggle() noexcept;
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void registerCommand(const QuickCommand& cmd);
    void clearCommands() noexcept;

    void setQuery(const std::string& q) noexcept;
    [[nodiscard]] const std::string& getQuery() const noexcept { return searchQuery_; }

    void setSelectedCategory(CommandCategory cat) noexcept;
    [[nodiscard]] CommandCategory getSelectedCategory() const noexcept { return selectedCategory_; }

    void layout(float screenW, float screenH) noexcept;
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;

    bool handlePointer(const PointerEvent& ev) noexcept;
    bool handleKey(int key, int scancode, int action, int mods) noexcept;
    bool handleChar(unsigned int codepoint) noexcept;

    [[nodiscard]] size_t getFilteredCount() const noexcept { return filteredCommands_.size(); }
    [[nodiscard]] int getSelectedIndex() const noexcept { return selectedIndex_; }
    void executeSelected() noexcept;

    // Callbacks
    std::function<void()> onClose;

private:
    void rebuildFilteredList() noexcept;
    static Color getCategoryColor(CommandCategory cat) noexcept;
    static std::string getCategoryName(CommandCategory cat) noexcept;

    bool isOpen_{false};
    std::string searchQuery_;
    CommandCategory selectedCategory_{CommandCategory::All};
    int selectedIndex_{0};
    float scrollOffset_{0.0f};

    std::vector<QuickCommand> allCommands_;
    std::vector<const QuickCommand*> filteredCommands_;

    // Layout Bounds
    Rect2D dialogBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D searchBoxBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D categoryBarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<Rect2D> categoryPillBounds_;
    Rect2D resultsListBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};
};

} // namespace eatsbits::ui
