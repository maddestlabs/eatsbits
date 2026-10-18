#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"

namespace eatsbits::ui {

struct ViewContext;

enum class DockEdge : uint8_t {
    Bottom = 0,
    Right  = 1,
    Left   = 2,
    Top    = 3
};

enum class DockTabPlacement : uint8_t {
    Center       = 0,
    Start        = 1,
    End          = 2,
    CustomOffset = 3
};

/**
 * @brief Decoupled content interface for sliding drawers and docks.
 * Implement this interface to host any content inside a SlidingDrawer or SlidingDock
 * without coupling the content to drawer physics, tabs, or geometry.
 */
class IDrawerContent {
public:
    virtual ~IDrawerContent() = default;
    virtual void onDrawerLayout(const Rect2D& contentBounds, const ViewContext& ctx) = 0;
    virtual void onDrawerUpdate([[maybe_unused]] float dt) {}
    virtual void onDrawerRender(BatchRenderer2D& r, const ThemeTokens& theme, const ViewContext& ctx) = 0;
    virtual bool onDrawerPointer([[maybe_unused]] const PointerEvent& ev, [[maybe_unused]] const ViewContext& ctx) { return false; }
    virtual bool onDrawerKey([[maybe_unused]] int key, [[maybe_unused]] int scancode, [[maybe_unused]] int action,
                             [[maybe_unused]] int mods, [[maybe_unused]] const ViewContext& ctx) { return false; }
    virtual bool onDrawerChar([[maybe_unused]] char32_t codepoint) { return false; }
};

/**
 * @brief Metadata and configuration for a tab within a SlidingDock or SlidingDrawer.
 */
struct DockTabItem {
    std::string id{"default"};
    std::string title{""};
    std::string iconId{""};
    std::shared_ptr<IDrawerContent> content{nullptr};

    float preferredExtent{220.0f};  // Height for Bottom/Top; Width for Right/Left
    float minExtent{120.0f};
    float maxExtent{500.0f};
    bool resizable{true};
    bool showCloseButton{true};
    bool showHeaderBar{false};
    float headerHeight{24.0f};

    DockTabPlacement tabPlacement{DockTabPlacement::Center};
    float tabOffset{0.0f};          // Pixel offset from placement anchor
    float tabWidth{160.0f};
    float tabHeight{22.0f};
    bool fullEdgeTab{false};        // If true (e.g. sidebar), tab height/width matches container edge

    int hotkey{0};                  // Optional GLFW hotkey (e.g. 'M', 'K')
};

/**
 * @brief Unified configuration for a SlidingDrawer.
 */
struct DrawerConfig {
    DockEdge edge{DockEdge::Bottom};
    DockTabItem tab{};
    float animationDamping{18.0f};   // ~180ms smooth ease-out
};

/**
 * @brief Core reusable sliding drawer component.
 * Encapsulates the animation physics, geometry calculations, pull-tab rendering,
 * drag-resizing, header bar, close button, and viewport clipping for all 4 edges.
 */
namespace clay {
    class ClayContext;
}

class SlidingDrawer {
public:
    explicit SlidingDrawer(DrawerConfig config = {});
    virtual ~SlidingDrawer();
    SlidingDrawer(const SlidingDrawer&) = delete;
    SlidingDrawer& operator=(const SlidingDrawer&) = delete;
    SlidingDrawer(SlidingDrawer&&) noexcept;
    SlidingDrawer& operator=(SlidingDrawer&&) noexcept;

    void setConfig(const DrawerConfig& config);
    [[nodiscard]] const DrawerConfig& getConfig() const noexcept { return config_; }

    void setContent(std::shared_ptr<IDrawerContent> content) { config_.tab.content = std::move(content); }
    [[nodiscard]] std::shared_ptr<IDrawerContent> getContent() const noexcept { return config_.tab.content; }

    void layout(const Rect2D& containerBounds, float edgeMargin = 0.0f);
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme, const ViewContext& ctx);
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx);
    bool handleChar(char32_t codepoint);

    // State inspection and control
    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    void setExpanded(bool exp) noexcept;
    void toggle() noexcept { setExpanded(!isExpanded_); }
    void toggleExpanded() noexcept { toggle(); }

    [[nodiscard]] float getAnimProgress() const noexcept { return animProgress_; }
    void setAnimProgress(float p) noexcept { animProgress_ = std::clamp(p, 0.0f, 1.0f); }
    [[nodiscard]] bool isAnimating() const noexcept {
        return std::abs(animProgress_ - (isExpanded_ ? 1.0f : 0.0f)) > 0.001f;
    }

    [[nodiscard]] float getExtent() const noexcept { return extent_; }
    void setExtent(float ext) noexcept;

    [[nodiscard]] bool isDragging() const noexcept { return isResizing_ || isDraggingTab_; }
    [[nodiscard]] bool isResizing() const noexcept { return isResizing_; }

    // Geometry inspect
    [[nodiscard]] const Rect2D& getContainerBounds() const noexcept { return containerBounds_; }
    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] const Rect2D& getPullTabBounds() const noexcept { return pullTabBounds_; }
    [[nodiscard]] const Rect2D& getHeaderBounds() const noexcept { return headerBounds_; }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return closeButtonBounds_; }
    [[nodiscard]] const Rect2D& getResizeHandleBounds() const noexcept { return resizeHandleBounds_; }
    [[nodiscard]] const Rect2D& getContentBounds() const noexcept { return contentBounds_; }

    // Callbacks
    std::function<void(bool expanded)> onExpandedChanged;
    std::function<void(float newExtent)> onExtentResized;

protected:
    void updateGeometry();

    DrawerConfig config_;
    bool isExpanded_{false};
    float animProgress_{0.0f};
    float extent_{220.0f};
    float edgeMargin_{0.0f};

    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D pullTabBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeButtonBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D resizeHandleBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D contentBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Interaction tracking
    bool isResizing_{false};
    float resizeStartCoord_{0.0f};
    float resizeStartExtent_{0.0f};

    bool isDraggingTab_{false};
    float tabDragStartCoord_{0.0f};
    float tabDragStartExtent_{0.0f};
    bool tabDragMoved_{false};

    std::unique_ptr<clay::ClayContext> clayCtx_;
};

/**
 * @brief Unified multi-tab edge dock container.
 * Coordinates multiple tabs on a single dock edge (Bottom or Right).
 * Switches between content panels smoothly, manages shared or multi-tab pull strips,
 * and maintains a single active content viewport.
 */
class SlidingDock {
public:
    explicit SlidingDock(DockEdge edge = DockEdge::Bottom);
    ~SlidingDock();
    SlidingDock(const SlidingDock&) = delete;
    SlidingDock& operator=(const SlidingDock&) = delete;
    SlidingDock(SlidingDock&&) noexcept;
    SlidingDock& operator=(SlidingDock&&) noexcept;

    void setEdge(DockEdge edge) noexcept { edge_ = edge; }
    [[nodiscard]] DockEdge getEdge() const noexcept { return edge_; }

    // Tab Management
    void addTab(DockTabItem tab);
    void removeTab(const std::string& id);
    void selectTab(const std::string& id);
    void selectTabIndex(int index);
    void toggleTab(const std::string& id);
    void closeDock() noexcept;

    [[nodiscard]] bool isOpen() const noexcept { return activeTabIndex_ >= 0 && isExpanded_; }
    [[nodiscard]] int getActiveTabIndex() const noexcept { return activeTabIndex_; }
    [[nodiscard]] const std::string& getActiveTabId() const noexcept;
    [[nodiscard]] size_t getTabCount() const noexcept { return tabs_.size(); }
    [[nodiscard]] const DockTabItem* getTab(size_t index) const noexcept;
    [[nodiscard]] DockTabItem* getTab(size_t index) noexcept;
    [[nodiscard]] const DockTabItem* findTab(const std::string& id) const noexcept;
    [[nodiscard]] DockTabItem* findTab(const std::string& id) noexcept;

    // Lifecycles
    void layout(const Rect2D& containerBounds, float edgeMargin = 0.0f);
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme, const ViewContext& ctx);
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx);
    bool handleChar(char32_t codepoint);

    [[nodiscard]] float getAnimProgress() const noexcept { return animProgress_; }
    void setAnimProgress(float p) noexcept { animProgress_ = std::clamp(p, 0.0f, 1.0f); }
    [[nodiscard]] bool isAnimating() const noexcept {
        float target = (isOpen()) ? 1.0f : 0.0f;
        return std::abs(animProgress_ - target) > 0.001f;
    }

    [[nodiscard]] float getCurrentExtent() const noexcept { return currentExtent_; }
    void setCurrentExtent(float ext) noexcept;

    [[nodiscard]] const Rect2D& getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] const Rect2D& getTabStripBounds() const noexcept { return tabStripBounds_; }
    [[nodiscard]] const Rect2D& getPullTabBounds(size_t index) const noexcept;
    [[nodiscard]] const Rect2D& getContentBounds() const noexcept { return contentBounds_; }
    [[nodiscard]] const Rect2D& getCloseButtonBounds() const noexcept { return closeButtonBounds_; }
    [[nodiscard]] bool isDragging() const noexcept { return isResizing_; }

    // Callbacks
    std::function<void(const std::string& tabId, bool open)> onTabToggled;
    std::function<void(float newExtent)> onExtentResized;

private:
    void updateGeometry();

    DockEdge edge_{DockEdge::Bottom};
    std::vector<DockTabItem> tabs_;
    std::vector<Rect2D> tabBoundsList_;
    int activeTabIndex_{-1};
    bool isExpanded_{false};

    float animProgress_{0.0f};
    float currentExtent_{220.0f};
    float targetExtent_{220.0f};
    float edgeMargin_{0.0f};
    float animationDamping_{18.0f};

    Rect2D containerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D tabStripBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeButtonBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D resizeHandleBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D contentBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    bool isResizing_{false};
    float resizeStartCoord_{0.0f};
    float resizeStartExtent_{0.0f};

    std::unique_ptr<clay::ClayContext> clayCtx_;
};

} // namespace eatsbits::ui
