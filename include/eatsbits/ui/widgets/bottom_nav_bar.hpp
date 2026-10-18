#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <functional>
#include <string>

namespace eatsbits::ui {

enum class WorkspaceView;

namespace clay {
    class ClayContext;
}

/**
 * BottomNavBar: Mechanical skeuomorphic hardware navigation strip.
 * Switches between ARRANGER, EDIT, TRACK, MIXER, and DESIGN tabs.
 * Features illuminated LED status tallies and tactile keycap bevels.
 */
class BottomNavBar {
public:
    BottomNavBar();
    ~BottomNavBar();
    BottomNavBar(const BottomNavBar&) = delete;
    BottomNavBar& operator=(const BottomNavBar&) = delete;
    BottomNavBar(BottomNavBar&&) noexcept;
    BottomNavBar& operator=(BottomNavBar&&) noexcept;

    void declareLayout(clay::ClayContext& ctx, float screenWidth, float screenHeight, float height = 48.0f);
    void layout(float screenWidth, float screenHeight, float height = 48.0f, clay::ClayContext* ctx = nullptr);
    void render(BatchRenderer2D& r, const ThemeTokens& theme, int activeTab, bool isMobile);
    bool handlePointer(const PointerEvent& ev, int& outNewTab);
    bool handleKey(int key, int scancode, int action, int mods, int& outNewTab);

    [[nodiscard]] Rect2D getBounds() const noexcept { return bounds_; }
    [[nodiscard]] float getTopY() const noexcept { return bounds_.y; }

    std::function<void(int)> onTabSelected;

private:
    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D tabBounds_[5];
};

} // namespace eatsbits::ui
