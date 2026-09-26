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
    Presets,
    Macros,
    History
};

struct BrowserItem {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::string engineTag;
};

/**
 * Slide-in Project Browser Drawer.
 * Slides in from the right edge with an animated transition.
 * Houses categorized Presets, procedural Macros, and Diff History milestones.
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
    [[nodiscard]] float getAnimOffset() const noexcept { return animOffset_; }
    [[nodiscard]] Rect2D getDrawerBounds() const noexcept { return drawerBounds_; }

    void setTab(BrowserDrawerTab tab) noexcept { activeTab_ = tab; }
    [[nodiscard]] BrowserDrawerTab getTab() const noexcept { return activeTab_; }

    // Callback on selecting/loading a preset or macro
    std::function<void(const std::string& presetId)> onSelectPreset;
    std::function<void(const std::string& macroId)> onRunMacro;
    std::function<void()> onClose;

private:
    bool isOpen_{false};
    float animProgress_{0.0f}; // 0.0 (closed) to 1.0 (open)
    float animOffset_{320.0f}; // Pixels offscreen to the right

    static constexpr float kDrawerWidth = 320.0f;

    BrowserDrawerTab activeTab_{BrowserDrawerTab::Presets};
    std::string selectedCategory_{"ALL"};
    int selectedIndex_{0};

    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D tabPresetsBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D tabMacrosBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D tabHistoryBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    std::vector<BrowserItem> presets_;
    std::vector<BrowserItem> macros_;
    std::vector<std::string> categories_;
    float scrollOffset_{0.0f};
};

} // namespace eatsbits::ui
