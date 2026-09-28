#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

/**
 * @brief Reusable, lightweight scrollable viewport and scrollbar manager for DAW-wide usage.
 * Handles viewport layout, content height tracking, smooth wheel scrolling,
 * thumb/track dragging, visibility culling, and polished vector scrollbar rendering.
 */
class ScrollableArea {
public:
    ScrollableArea() = default;
    explicit ScrollableArea(const Rect2D& viewportBounds) : viewportBounds_(viewportBounds) {}

    // Viewport & Content Dimensions
    void setViewport(const Rect2D& bounds) noexcept {
        viewportBounds_ = bounds;
        updateScrollLimits();
    }
    void setViewport(float x, float y, float w, float h) noexcept {
        setViewport(Rect2D{x, y, w, h});
    }
    [[nodiscard]] const Rect2D& getViewport() const noexcept { return viewportBounds_; }

    void setContentHeight(float totalHeight) noexcept {
        contentHeight_ = std::max(0.0f, totalHeight);
        updateScrollLimits();
    }
    [[nodiscard]] float getContentHeight() const noexcept { return contentHeight_; }

    // Scroll Position
    [[nodiscard]] float getScrollY() const noexcept { return scrollY_; }
    void setScrollY(float sy) noexcept {
        scrollY_ = std::clamp(sy, 0.0f, maxScrollY_);
    }
    [[nodiscard]] float getMaxScroll() const noexcept { return maxScrollY_; }
    [[nodiscard]] bool canScroll() const noexcept { return maxScrollY_ > 0.5f; }

    void scrollBy(float delta) noexcept {
        setScrollY(scrollY_ + delta);
    }

    [[nodiscard]] float getScrollRatio() const noexcept {
        return (maxScrollY_ > 0.001f) ? (scrollY_ / maxScrollY_) : 0.0f;
    }

    // Coordinate Helpers & Culling
    [[nodiscard]] float contentToScreenY(float contentY) const noexcept {
        return viewportBounds_.y + contentY - scrollY_;
    }
    [[nodiscard]] float screenToContentY(float screenY) const noexcept {
        return screenY - viewportBounds_.y + scrollY_;
    }

    /// Fast visibility test for vertical culling
    [[nodiscard]] bool isVisible(float screenY, float itemH) const noexcept {
        return (screenY + itemH >= viewportBounds_.y) && (screenY <= viewportBounds_.y + viewportBounds_.h);
    }
    [[nodiscard]] bool isContentVisible(float contentY, float itemH) const noexcept {
        float sy = contentToScreenY(contentY);
        return isVisible(sy, itemH);
    }

    // Scrollbar Geometry
    [[nodiscard]] Rect2D getScrollbarTrackBounds() const noexcept {
        float trackX = viewportBounds_.x + viewportBounds_.w - scrollbarWidth_ - scrollbarMarginRight_;
        return Rect2D{trackX, viewportBounds_.y, scrollbarWidth_, viewportBounds_.h};
    }

    [[nodiscard]] Rect2D getScrollbarThumbBounds() const noexcept {
        if (!canScroll()) return Rect2D{0.0f, 0.0f, 0.0f, 0.0f};
        Rect2D track = getScrollbarTrackBounds();
        float viewH = viewportBounds_.h;
        float thumbH = std::clamp((viewH / contentHeight_) * track.h, minThumbHeight_, track.h);
        float thumbY = track.y + getScrollRatio() * (track.h - thumbH);
        return Rect2D{track.x, thumbY, track.w, thumbH};
    }

    // Rendering
    void renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme, float alpha = 1.0f) const {
        if (!canScroll()) return;
        Rect2D track = getScrollbarTrackBounds();
        Rect2D thumb = getScrollbarThumbBounds();

        // Track pill
        r.drawRoundedRect(track.x, track.y, track.w, track.h, track.w * 0.5f,
                          theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 0.35f * alpha);

        // Thumb pill with active/hover highlight
        Color thumbColor = isDragging_ ? theme.primaryAccent : (isHovered_ ? theme.primaryAccent.lighten(0.15f) : theme.primaryAccent.darken(0.10f));
        r.drawRoundedRect(thumb.x, thumb.y, thumb.w, thumb.h, thumb.w * 0.5f,
                          thumbColor.r, thumbColor.g, thumbColor.b, 0.85f * alpha);
    }

    // Interaction Handling
    bool handleScroll(float yoffset, float mouseX, float mouseY, float step = 32.0f) {
        if (!viewportBounds_.contains(mouseX, mouseY) && !getScrollbarTrackBounds().contains(mouseX, mouseY)) {
            return false;
        }
        if (!canScroll()) return false;
        scrollBy(-yoffset * step);
        return true;
    }

    bool handlePointer(const PointerEvent& ev) {
        if (!canScroll()) return false;

        Rect2D track = getScrollbarTrackBounds();
        Rect2D thumb = getScrollbarThumbBounds();

        if (ev.action == PointerAction::Move) {
            isHovered_ = thumb.contains(ev.x, ev.y) || track.contains(ev.x, ev.y);
            if (isDragging_) {
                float deltaY = ev.y - dragStartY_;
                float availableTrackH = track.h - thumb.h;
                if (availableTrackH > 1.0f) {
                    float newScroll = dragStartScrollY_ + (deltaY / availableTrackH) * maxScrollY_;
                    setScrollY(newScroll);
                }
                return true;
            }
            return false;
        }

        if (ev.action == PointerAction::Down && ev.button == PointerButton::Left) {
            if (thumb.contains(ev.x, ev.y)) {
                isDragging_ = true;
                dragStartY_ = ev.y;
                dragStartScrollY_ = scrollY_;
                return true;
            }
            if (track.contains(ev.x, ev.y)) {
                // Page jump towards click
                if (ev.y < thumb.y) {
                    scrollBy(-viewportBounds_.h * 0.8f);
                } else {
                    scrollBy(viewportBounds_.h * 0.8f);
                }
                return true;
            }
        }

        if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            if (isDragging_) {
                isDragging_ = false;
                return true;
            }
        }

        if (ev.action == PointerAction::Scroll) {
            if (viewportBounds_.contains(ev.x, ev.y) || track.contains(ev.x, ev.y)) {
                scrollBy(-ev.scrollY * 32.0f);
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] bool isDragging() const noexcept { return isDragging_; }
    void stopDragging() noexcept { isDragging_ = false; }

    void setScrollbarWidth(float w) noexcept { scrollbarWidth_ = w; }
    void setScrollbarMarginRight(float m) noexcept { scrollbarMarginRight_ = m; }
    void setMinThumbHeight(float h) noexcept { minThumbHeight_ = h; }

private:
    void updateScrollLimits() noexcept {
        maxScrollY_ = std::max(0.0f, contentHeight_ - viewportBounds_.h);
        scrollY_ = std::clamp(scrollY_, 0.0f, maxScrollY_);
    }

    Rect2D viewportBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    float contentHeight_{0.0f};
    float scrollY_{0.0f};
    float maxScrollY_{0.0f};

    float scrollbarWidth_{5.0f};
    float scrollbarMarginRight_{3.0f};
    float minThumbHeight_{24.0f};

    bool isDragging_{false};
    bool isHovered_{false};
    float dragStartY_{0.0f};
    float dragStartScrollY_{0.0f};
};

} // namespace eatsbits::ui
