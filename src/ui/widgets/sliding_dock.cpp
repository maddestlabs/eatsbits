#include "eatsbits/ui/widgets/sliding_dock.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/views/view_base.hpp"
#include <cmath>
#include <algorithm>

namespace eatsbits::ui {

// =============================================================================
// SlidingDrawer Implementation
// =============================================================================

SlidingDrawer::SlidingDrawer(DrawerConfig config)
    : config_(std::move(config)), extent_(config_.tab.preferredExtent) {
}

void SlidingDrawer::setConfig(const DrawerConfig& config) {
    config_ = config;
    if (config_.tab.preferredExtent > 0.0f) {
        extent_ = config_.tab.preferredExtent;
    }
    extent_ = std::clamp(extent_, config_.tab.minExtent, config_.tab.maxExtent);
    updateGeometry();
}

void SlidingDrawer::setExpanded(bool exp) noexcept {
    if (isExpanded_ != exp) {
        isExpanded_ = exp;
        if (onExpandedChanged) {
            onExpandedChanged(isExpanded_);
        }
    }
}

void SlidingDrawer::setExtent(float ext) noexcept {
    extent_ = std::clamp(ext, config_.tab.minExtent, config_.tab.maxExtent);
    updateGeometry();
    if (onExtentResized) {
        onExtentResized(extent_);
    }
}

void SlidingDrawer::layout(const Rect2D& containerBounds, float edgeMargin) {
    containerBounds_ = containerBounds;
    edgeMargin_ = edgeMargin;
    updateGeometry();

    if (config_.tab.content && animProgress_ > 0.001f) {
        ViewContext ctx;
        ctx.logicalWidth = containerBounds.w;
        ctx.logicalHeight = containerBounds.h;
        config_.tab.content->onDrawerLayout(contentBounds_, ctx);
    }
}

void SlidingDrawer::updateGeometry() {
    const float progress = animProgress_;
    const float currentExtent = extent_ * progress;

    if (config_.edge == DockEdge::Bottom) {
        const float effectiveW = (std::max)(100.0f, containerBounds_.w - edgeMargin_);
        const float drawerY = (containerBounds_.y + containerBounds_.h) - currentExtent;
        drawerBounds_ = Rect2D(containerBounds_.x, drawerY, effectiveW, currentExtent);

        // Pull-tab geometry
        const float tabW = config_.tab.tabWidth;
        const float tabH = config_.tab.tabHeight;
        float tabX = containerBounds_.x + config_.tab.tabOffset;
        if (config_.tab.tabPlacement == DockTabPlacement::Center) {
            tabX = containerBounds_.x + (effectiveW - tabW) * 0.5f + config_.tab.tabOffset;
        } else if (config_.tab.tabPlacement == DockTabPlacement::End) {
            tabX = containerBounds_.x + effectiveW - tabW - config_.tab.tabOffset;
        }

        const float tabY = (progress <= 0.001f)
            ? (containerBounds_.y + containerBounds_.h - tabH)
            : (drawerY - tabH);
        pullTabBounds_ = Rect2D(tabX, tabY, tabW, tabH);

        if (progress <= 0.001f) {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = Rect2D();
            resizeHandleBounds_ = Rect2D();
            contentBounds_ = Rect2D();
            return;
        }

        // Resize handle on top edge
        resizeHandleBounds_ = Rect2D(containerBounds_.x, drawerY - 4.0f, effectiveW, 8.0f);

        // Header and Close button
        float headerH = config_.tab.showHeaderBar ? config_.tab.headerHeight : 0.0f;
        if (headerH > 0.0f) {
            headerBounds_ = Rect2D(containerBounds_.x, drawerY, effectiveW, headerH);
            closeButtonBounds_ = Rect2D(containerBounds_.x + effectiveW - 26.0f, drawerY + (headerH - 18.0f) * 0.5f, 20.0f, 18.0f);
        } else {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = config_.tab.showCloseButton
                ? Rect2D(containerBounds_.x + effectiveW - 26.0f, drawerY + 3.0f, 20.0f, 18.0f)
                : Rect2D();
        }

        // Content viewport
        contentBounds_ = Rect2D(containerBounds_.x, drawerY + headerH, effectiveW, (std::max)(0.0f, currentExtent - headerH));

    } else if (config_.edge == DockEdge::Right) {
        const float effectiveH = containerBounds_.h;
        const float tabW = config_.tab.tabWidth;
        const float totalW = tabW + (extent_ * progress);
        const float rightBoundary = containerBounds_.x + containerBounds_.w - totalW - edgeMargin_;

        if (config_.tab.fullEdgeTab) {
            pullTabBounds_ = Rect2D(rightBoundary, containerBounds_.y, tabW, effectiveH);
        } else {
            const float tabH = config_.tab.tabHeight;
            float tabY = containerBounds_.y + config_.tab.tabOffset;
            if (config_.tab.tabPlacement == DockTabPlacement::Center) {
                tabY = containerBounds_.y + (effectiveH - tabH) * 0.5f + config_.tab.tabOffset;
            }
            pullTabBounds_ = Rect2D(rightBoundary, tabY, tabW, tabH);
        }

        drawerBounds_ = Rect2D(rightBoundary + tabW, containerBounds_.y, extent_, effectiveH);

        if (progress <= 0.001f) {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = Rect2D();
            resizeHandleBounds_ = Rect2D();
            contentBounds_ = Rect2D();
            return;
        }

        // Resize handle on left edge
        resizeHandleBounds_ = Rect2D(drawerBounds_.x - 4.0f, containerBounds_.y, 8.0f, effectiveH);

        // Header and Close button
        float headerH = config_.tab.showHeaderBar ? config_.tab.headerHeight : 0.0f;
        if (headerH > 0.0f) {
            headerBounds_ = Rect2D(drawerBounds_.x, containerBounds_.y, extent_, headerH);
            closeButtonBounds_ = Rect2D(drawerBounds_.x + extent_ - 28.0f, containerBounds_.y + (headerH - 22.0f) * 0.5f, 22.0f, 22.0f);
        } else {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = config_.tab.showCloseButton
                ? Rect2D(drawerBounds_.x + extent_ - 28.0f, containerBounds_.y + 7.0f, 22.0f, 22.0f)
                : Rect2D();
        }

        contentBounds_ = Rect2D(drawerBounds_.x, containerBounds_.y + headerH, extent_, (std::max)(0.0f, effectiveH - headerH));

    } else if (config_.edge == DockEdge::Left) {
        const float effectiveH = containerBounds_.h;
        const float tabW = config_.tab.tabWidth;
        const float drawerX = containerBounds_.x + edgeMargin_;

        drawerBounds_ = Rect2D(drawerX, containerBounds_.y, extent_ * progress, effectiveH);
        if (config_.tab.fullEdgeTab) {
            pullTabBounds_ = Rect2D(drawerBounds_.x + drawerBounds_.w, containerBounds_.y, tabW, effectiveH);
        } else {
            const float tabH = config_.tab.tabHeight;
            float tabY = containerBounds_.y + config_.tab.tabOffset;
            if (config_.tab.tabPlacement == DockTabPlacement::Center) {
                tabY = containerBounds_.y + (effectiveH - tabH) * 0.5f + config_.tab.tabOffset;
            }
            pullTabBounds_ = Rect2D(drawerBounds_.x + drawerBounds_.w, tabY, tabW, tabH);
        }
        resizeHandleBounds_ = Rect2D(drawerBounds_.x + drawerBounds_.w - 4.0f, containerBounds_.y, 8.0f, effectiveH);

        float headerH = config_.tab.showHeaderBar ? config_.tab.headerHeight : 0.0f;
        if (headerH > 0.0f) {
            headerBounds_ = Rect2D(drawerBounds_.x, containerBounds_.y, drawerBounds_.w, headerH);
            closeButtonBounds_ = Rect2D(drawerBounds_.x + drawerBounds_.w - 28.0f, containerBounds_.y + (headerH - 22.0f) * 0.5f, 22.0f, 22.0f);
        } else {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = config_.tab.showCloseButton
                ? Rect2D(drawerBounds_.x + drawerBounds_.w - 26.0f, containerBounds_.y + 3.0f, 20.0f, 18.0f)
                : Rect2D();
        }
        contentBounds_ = Rect2D(drawerBounds_.x, containerBounds_.y + headerH, drawerBounds_.w, (std::max)(0.0f, effectiveH - headerH));

    } else if (config_.edge == DockEdge::Top) {
        const float effectiveW = containerBounds_.w;
        const float drawerY = containerBounds_.y + edgeMargin_;

        drawerBounds_ = Rect2D(containerBounds_.x, drawerY, effectiveW, extent_ * progress);
        pullTabBounds_ = Rect2D(containerBounds_.x + (effectiveW - config_.tab.tabWidth) * 0.5f,
                                drawerBounds_.y + drawerBounds_.h, config_.tab.tabWidth, config_.tab.tabHeight);
        resizeHandleBounds_ = Rect2D(containerBounds_.x, drawerBounds_.y + drawerBounds_.h - 4.0f, effectiveW, 8.0f);
        contentBounds_ = drawerBounds_;
    }
}

void SlidingDrawer::update(float dt) noexcept {
    const float target = isExpanded_ ? 1.0f : 0.0f;
    const float factor = 1.0f - std::exp(-config_.animationDamping * dt);
    animProgress_ += (target - animProgress_) * factor;
    if (std::abs(animProgress_ - target) < 0.001f) {
        animProgress_ = target;
    }

    if (config_.tab.content && isExpanded_) {
        config_.tab.content->onDrawerUpdate(dt);
    }
}

void SlidingDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme, const ViewContext& ctx) {
    // 1. Pull Tab
    const bool isTabHovered = pullTabBounds_.contains(ctx.mouseX, ctx.mouseY);
    const Color tabBg = isTabHovered
        ? Color(theme.panelHeader.r * 1.15f, theme.panelHeader.g * 1.15f, theme.panelHeader.b * 1.15f, 0.96f)
        : Color(theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.92f);

    if (config_.edge == DockEdge::Bottom) {
        drawRoundedRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f, tabBg);
        drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                               theme.borderSubtle, 1.0f);

        // Chevron
        const float chevX = pullTabBounds_.x + 14.0f;
        const float chevY = pullTabBounds_.y + pullTabBounds_.h * 0.5f;
        if (isExpanded_) {
            drawChevronDown(r, chevX, chevY, 4.0f, theme.textSecondary, 1.6f);
        } else {
            drawChevronUp(r, chevX, chevY, 4.0f, theme.primaryAccent, 1.6f);
        }

        // Title
        if (!config_.tab.title.empty()) {
            drawMonoText(r, config_.tab.title, pullTabBounds_.x + 28.0f, pullTabBounds_.y + 5.0f, 10.0f,
                         isExpanded_ ? theme.primaryAccent : theme.textPrimary);
        }
    } else if (config_.edge == DockEdge::Right || config_.edge == DockEdge::Left) {
        drawRoundedRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f, tabBg);
        drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                               theme.borderSubtle, 1.0f);

        // Icon or Vertical Text
        if (!config_.tab.title.empty()) {
            drawRotatedText(r, config_.tab.title, pullTabBounds_.x + pullTabBounds_.w * 0.5f,
                            pullTabBounds_.y + pullTabBounds_.h * 0.5f, config_.edge == DockEdge::Left ? -90.0f : 90.0f, 10.0f,
                            isExpanded_ ? theme.primaryAccent : theme.textPrimary);
        }
    }

    if (animProgress_ <= 0.001f) {
        return;
    }

    // 2. Drawer Body Background
    drawRect(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h,
             theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y,
             theme.borderSubtle, 1.0f);

    // 3. Header Bar & Close Button
    if (headerBounds_.h > 0.0f) {
        drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h, theme.panelHeader);
        drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h,
                 headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
                 theme.borderSubtle, 1.0f);

        if (!config_.tab.title.empty()) {
            drawMonoText(r, config_.tab.title, headerBounds_.x + 12.0f, headerBounds_.y + 5.0f, 11.0f, theme.textPrimary);
        }
    }

    if (closeButtonBounds_.w > 0.0f) {
        bool closeHov = closeButtonBounds_.contains(ctx.mouseX, ctx.mouseY);
        drawScrewCloseButton(r, closeButtonBounds_.x + closeButtonBounds_.w * 0.5f,
                             closeButtonBounds_.y + closeButtonBounds_.h * 0.5f,
                             7.0f, closeHov, theme.primaryAccent);
    }

    // 4. Resize Handle Grip Indicator (when resizable)
    if (config_.tab.resizable && (isResizing_ || resizeHandleBounds_.contains(ctx.mouseX, ctx.mouseY))) {
        if (config_.edge == DockEdge::Bottom) {
            float handleCx = drawerBounds_.x + drawerBounds_.w * 0.5f;
            drawRoundedRect(r, handleCx - 24.0f, drawerBounds_.y + 1.0f, 48.0f, 3.0f, 1.5f,
                            isResizing_ ? theme.primaryAccent : theme.borderSubtle);
        } else if (config_.edge == DockEdge::Right || config_.edge == DockEdge::Left) {
            float handleCy = drawerBounds_.y + drawerBounds_.h * 0.5f;
            float handleX = (config_.edge == DockEdge::Right) ? (drawerBounds_.x + 1.0f) : (drawerBounds_.x + drawerBounds_.w - 4.0f);
            drawRoundedRect(r, handleX, handleCy - 24.0f, 3.0f, 48.0f, 1.5f,
                            isResizing_ ? theme.primaryAccent : theme.borderSubtle);
        }
    }

    // 5. Host Content View
    if (config_.tab.content) {
        config_.tab.content->onDrawerRender(r, theme, ctx);
    }
}

bool SlidingDrawer::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    // 1. Drag Resizing in Progress
    if (isResizing_) {
        if (ev.action == PointerAction::Move) {
            float delta = 0.0f;
            if (config_.edge == DockEdge::Bottom) {
                delta = resizeStartCoord_ - ev.y;
            } else if (config_.edge == DockEdge::Right) {
                delta = resizeStartCoord_ - ev.x;
            } else if (config_.edge == DockEdge::Left) {
                delta = ev.x - resizeStartCoord_;
            } else if (config_.edge == DockEdge::Top) {
                delta = ev.y - resizeStartCoord_;
            }
            setExtent(resizeStartExtent_ + delta);
            return true;
        }
        if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            isResizing_ = false;
            return true;
        }
        return true;
    }

    // 1b. Pull-Tab Drag Resizing / Click Toggle in Progress
    if (isDraggingTab_) {
        if (ev.action == PointerAction::Move) {
            float delta = 0.0f;
            if (config_.edge == DockEdge::Bottom) {
                delta = tabDragStartCoord_ - ev.y;
            } else if (config_.edge == DockEdge::Right) {
                delta = tabDragStartCoord_ - ev.x;
            } else if (config_.edge == DockEdge::Left) {
                delta = ev.x - tabDragStartCoord_;
            } else if (config_.edge == DockEdge::Top) {
                delta = ev.y - tabDragStartCoord_;
            }

            if (std::abs(delta) > 4.0f) {
                tabDragMoved_ = true;
                if (!isExpanded_ && delta > 12.0f) {
                    setExpanded(true);
                }
                if (config_.tab.resizable) {
                    setExtent(tabDragStartExtent_ + delta);
                }
            }
            return true;
        }
        if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            if (!tabDragMoved_) {
                toggle();
            }
            isDraggingTab_ = false;
            return true;
        }
        return true;
    }

    // 2. Close Button Hit
    if (closeButtonBounds_.w > 0.0f && closeButtonBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            setExpanded(false);
            return true;
        }
        return true;
    }

    // 3. Resize Handle Press
    if (config_.tab.resizable && isExpanded_ && resizeHandleBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down && ev.button == PointerButton::Left) {
            isResizing_ = true;
            resizeStartExtent_ = extent_;
            resizeStartCoord_ = (config_.edge == DockEdge::Bottom || config_.edge == DockEdge::Top) ? ev.y : ev.x;
            return true;
        }
    }

    // 4. Pull-Tab Press
    if (pullTabBounds_.w > 0.0f && pullTabBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down && ev.button == PointerButton::Left) {
            isDraggingTab_ = true;
            tabDragMoved_ = false;
            tabDragStartExtent_ = extent_;
            tabDragStartCoord_ = (config_.edge == DockEdge::Bottom || config_.edge == DockEdge::Top) ? ev.y : ev.x;
            return true;
        }
        return true;
    }

    // 5. Delegate to Content if Inside Content Bounds
    if (isExpanded_ && config_.tab.content && contentBounds_.contains(ev.x, ev.y)) {
        if (config_.tab.content->onDrawerPointer(ev, ctx)) {
            return true;
        }
        // If content is present but didn't handle it, absorb pointer inside drawer so it doesn't bleed through
        return true;
    }

    // Intercept clicks on drawer body so they don't bleed through to underlying arranger only if content is hosted
    if (isExpanded_ && config_.tab.content && drawerBounds_.contains(ev.x, ev.y)) {
        return true;
    }

    return false;
}

bool SlidingDrawer::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (action != 1 /* GLFW_PRESS */) {
        if (isExpanded_ && config_.tab.content) {
            return config_.tab.content->onDrawerKey(key, scancode, action, mods, ctx);
        }
        return false;
    }

    // Hotkey toggle
    if (config_.tab.hotkey > 0 && key == config_.tab.hotkey) {
        toggle();
        return true;
    }

    // Esc closes drawer
    if (key == 256 /* GLFW_KEY_ESCAPE */ && isExpanded_) {
        setExpanded(false);
        return true;
    }

    if (isExpanded_ && config_.tab.content) {
        return config_.tab.content->onDrawerKey(key, scancode, action, mods, ctx);
    }

    return false;
}

bool SlidingDrawer::handleChar(char32_t codepoint) {
    if (isExpanded_ && config_.tab.content) {
        return config_.tab.content->onDrawerChar(codepoint);
    }
    return false;
}

// =============================================================================
// SlidingDock Implementation (Multi-Tab Edge Dock)
// =============================================================================

SlidingDock::SlidingDock(DockEdge edge)
    : edge_(edge) {
}

void SlidingDock::addTab(DockTabItem tab) {
    tabs_.push_back(std::move(tab));
    tabBoundsList_.resize(tabs_.size());
    if (activeTabIndex_ < 0 && !tabs_.empty()) {
        activeTabIndex_ = 0;
        currentExtent_ = tabs_[0].preferredExtent;
        targetExtent_ = currentExtent_;
    }
}

void SlidingDock::removeTab(const std::string& id) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [&](const DockTabItem& t) { return t.id == id; });
    if (it != tabs_.end()) {
        int idx = static_cast<int>(std::distance(tabs_.begin(), it));
        tabs_.erase(it);
        tabBoundsList_.resize(tabs_.size());
        if (activeTabIndex_ == idx) {
            activeTabIndex_ = tabs_.empty() ? -1 : 0;
            if (activeTabIndex_ >= 0) {
                targetExtent_ = tabs_[activeTabIndex_].preferredExtent;
            }
        } else if (activeTabIndex_ > idx) {
            activeTabIndex_--;
        }
    }
}

void SlidingDock::selectTab(const std::string& id) {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].id == id) {
            selectTabIndex(static_cast<int>(i));
            return;
        }
    }
}

void SlidingDock::selectTabIndex(int index) {
    if (index >= 0 && index < static_cast<int>(tabs_.size())) {
        activeTabIndex_ = index;
        isExpanded_ = true;
        targetExtent_ = tabs_[index].preferredExtent;
        if (onTabToggled) {
            onTabToggled(tabs_[index].id, true);
        }
    }
}

void SlidingDock::toggleTab(const std::string& id) {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].id == id) {
            if (activeTabIndex_ == static_cast<int>(i) && isExpanded_) {
                closeDock();
            } else {
                selectTabIndex(static_cast<int>(i));
            }
            return;
        }
    }
}

void SlidingDock::closeDock() noexcept {
    isExpanded_ = false;
    if (activeTabIndex_ >= 0 && activeTabIndex_ < static_cast<int>(tabs_.size())) {
        if (onTabToggled) {
            onTabToggled(tabs_[activeTabIndex_].id, false);
        }
    }
}

const std::string& SlidingDock::getActiveTabId() const noexcept {
    static const std::string kEmpty = "";
    if (activeTabIndex_ >= 0 && activeTabIndex_ < static_cast<int>(tabs_.size())) {
        return tabs_[activeTabIndex_].id;
    }
    return kEmpty;
}

const DockTabItem* SlidingDock::getTab(size_t index) const noexcept {
    return (index < tabs_.size()) ? &tabs_[index] : nullptr;
}

DockTabItem* SlidingDock::getTab(size_t index) noexcept {
    return (index < tabs_.size()) ? &tabs_[index] : nullptr;
}

const DockTabItem* SlidingDock::findTab(const std::string& id) const noexcept {
    for (const auto& t : tabs_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

DockTabItem* SlidingDock::findTab(const std::string& id) noexcept {
    for (auto& t : tabs_) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

const Rect2D& SlidingDock::getPullTabBounds(size_t index) const noexcept {
    static const Rect2D kEmpty{0.0f, 0.0f, 0.0f, 0.0f};
    return (index < tabBoundsList_.size()) ? tabBoundsList_[index] : kEmpty;
}

void SlidingDock::setCurrentExtent(float ext) noexcept {
    if (activeTabIndex_ >= 0 && activeTabIndex_ < static_cast<int>(tabs_.size())) {
        const auto& t = tabs_[activeTabIndex_];
        currentExtent_ = std::clamp(ext, t.minExtent, t.maxExtent);
        targetExtent_ = currentExtent_;
        updateGeometry();
        if (onExtentResized) {
            onExtentResized(currentExtent_);
        }
    }
}

void SlidingDock::layout(const Rect2D& containerBounds, float edgeMargin) {
    containerBounds_ = containerBounds;
    edgeMargin_ = edgeMargin;
    updateGeometry();

    if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content && animProgress_ > 0.001f) {
        ViewContext ctx;
        ctx.logicalWidth = containerBounds.w;
        ctx.logicalHeight = containerBounds.h;
        tabs_[activeTabIndex_].content->onDrawerLayout(contentBounds_, ctx);
    }
}

void SlidingDock::updateGeometry() {
    tabBoundsList_.resize(tabs_.size());
    const float progress = animProgress_;
    const float currentExt = currentExtent_ * progress;

    if (edge_ == DockEdge::Bottom) {
        const float effectiveW = (std::max)(100.0f, containerBounds_.w - edgeMargin_);
        const float drawerY = (containerBounds_.y + containerBounds_.h) - currentExt;
        drawerBounds_ = Rect2D(containerBounds_.x, drawerY, effectiveW, currentExt);

        // Tab strip layout along top edge of drawer
        constexpr float kTabH = 22.0f;
        const float tabStripY = (progress <= 0.001f)
            ? (containerBounds_.y + containerBounds_.h - kTabH)
            : (drawerY - kTabH);

        tabStripBounds_ = Rect2D(containerBounds_.x, tabStripY, effectiveW, kTabH);

        // Layout individual tab pills
        float currentTabX = containerBounds_.x + 12.0f;
        for (size_t i = 0; i < tabs_.size(); ++i) {
            const auto& t = tabs_[i];
            float tabW = t.tabWidth > 0.0f ? t.tabWidth : 120.0f;
            if (t.tabPlacement == DockTabPlacement::End) {
                float endX = containerBounds_.x + effectiveW - tabW - 12.0f;
                tabBoundsList_[i] = Rect2D(endX, tabStripY, tabW, kTabH);
            } else {
                tabBoundsList_[i] = Rect2D(currentTabX, tabStripY, tabW, kTabH);
                currentTabX += tabW + 6.0f;
            }
        }

        if (progress <= 0.001f) {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = Rect2D();
            resizeHandleBounds_ = Rect2D();
            contentBounds_ = Rect2D();
            return;
        }

        // Top drag handle
        resizeHandleBounds_ = Rect2D(containerBounds_.x, drawerY - 4.0f, effectiveW, 8.0f);

        // Header bar & Close button
        headerBounds_ = Rect2D(containerBounds_.x, drawerY, effectiveW, 24.0f);
        closeButtonBounds_ = Rect2D(containerBounds_.x + effectiveW - 26.0f, drawerY + 3.0f, 20.0f, 18.0f);

        contentBounds_ = Rect2D(containerBounds_.x, drawerY + 24.0f, effectiveW, (std::max)(0.0f, currentExt - 24.0f));

    } else if (edge_ == DockEdge::Right) {
        const float effectiveH = containerBounds_.h;
        constexpr float kTabW = 24.0f;
        const float totalW = kTabW + (currentExtent_ * progress);
        const float rightBoundary = containerBounds_.x + containerBounds_.w - totalW - edgeMargin_;

        tabStripBounds_ = Rect2D(rightBoundary, containerBounds_.y, kTabW, effectiveH);
        drawerBounds_ = Rect2D(rightBoundary + kTabW, containerBounds_.y, currentExtent_, effectiveH);

        float currentTabY = containerBounds_.y + 12.0f;
        for (size_t i = 0; i < tabs_.size(); ++i) {
            const auto& t = tabs_[i];
            float tabH = t.tabHeight > 0.0f ? t.tabHeight : 100.0f;
            tabBoundsList_[i] = Rect2D(rightBoundary, currentTabY, kTabW, tabH);
            currentTabY += tabH + 6.0f;
        }

        if (progress <= 0.001f) {
            headerBounds_ = Rect2D();
            closeButtonBounds_ = Rect2D();
            resizeHandleBounds_ = Rect2D();
            contentBounds_ = Rect2D();
            return;
        }

        resizeHandleBounds_ = Rect2D(drawerBounds_.x - 4.0f, containerBounds_.y, 8.0f, effectiveH);
        closeButtonBounds_ = Rect2D(drawerBounds_.x + currentExtent_ - 28.0f, containerBounds_.y + 7.0f, 22.0f, 22.0f);
        contentBounds_ = Rect2D(drawerBounds_.x, containerBounds_.y + 36.0f, currentExtent_, (std::max)(0.0f, effectiveH - 36.0f));
    }
}

void SlidingDock::update(float dt) noexcept {
    const float target = isExpanded_ ? 1.0f : 0.0f;
    const float factor = 1.0f - std::exp(-animationDamping_ * dt);
    animProgress_ += (target - animProgress_) * factor;
    if (std::abs(animProgress_ - target) < 0.001f) {
        animProgress_ = target;
    }

    // Extent interpolation when switching tabs with different preferred sizes
    currentExtent_ += (targetExtent_ - currentExtent_) * factor;
    if (std::abs(currentExtent_ - targetExtent_) < 0.5f) {
        currentExtent_ = targetExtent_;
    }

    if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content) {
        tabs_[activeTabIndex_].content->onDrawerUpdate(dt);
    }
}

void SlidingDock::render(BatchRenderer2D& r, const ThemeTokens& theme, const ViewContext& ctx) {
    // 1. Render Tab Strip
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const auto& t = tabs_[i];
        const Rect2D& tb = tabBoundsList_[i];
        const bool isActive = (activeTabIndex_ == static_cast<int>(i) && isExpanded_);
        const bool isHov = tb.contains(ctx.mouseX, ctx.mouseY);

        Color tabBg = isActive
            ? theme.panelHeader
            : (isHov ? Color(theme.panelHeader.r * 1.1f, theme.panelHeader.g * 1.1f, theme.panelHeader.b * 1.1f, 0.9f)
                     : Color(theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.85f));

        drawRoundedRect(r, tb.x, tb.y, tb.w, tb.h, 4.0f, tabBg);
        drawRoundedRectOutline(r, tb.x, tb.y, tb.w, tb.h, 4.0f,
                               isActive ? theme.primaryAccent : theme.borderSubtle, 1.0f);

        if (edge_ == DockEdge::Bottom) {
            // Chevron & Title
            float chevX = tb.x + 12.0f;
            float chevY = tb.y + tb.h * 0.5f;
            if (isActive) {
                drawChevronDown(r, chevX, chevY, 3.5f, theme.primaryAccent, 1.6f);
            } else {
                drawChevronUp(r, chevX, chevY, 3.5f, theme.textSecondary, 1.6f);
            }

            drawMonoText(r, t.title, tb.x + 22.0f, tb.y + 5.0f, 10.0f,
                         isActive ? theme.primaryAccent : (isHov ? theme.textPrimary : theme.textSecondary));
        } else if (edge_ == DockEdge::Right) {
            drawRotatedText(r, t.title, tb.x + tb.w * 0.5f, tb.y + tb.h * 0.5f, 90.0f, 10.0f,
                            isActive ? theme.primaryAccent : (isHov ? theme.textPrimary : theme.textSecondary));
        }
    }

    if (animProgress_ <= 0.001f) {
        return;
    }

    // 2. Dock Body Background
    drawRect(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h,
             theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y,
             theme.borderSubtle, 1.0f);

    // 3. Top Header Bar & Close Button
    if (headerBounds_.h > 0.0f) {
        drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h, theme.panelHeader);
        drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h,
                 headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
                 theme.borderSubtle, 1.0f);

        if (activeTabIndex_ >= 0 && activeTabIndex_ < static_cast<int>(tabs_.size())) {
            drawMonoText(r, tabs_[activeTabIndex_].title, headerBounds_.x + 12.0f, headerBounds_.y + 6.0f, 11.0f, theme.textPrimary);
        }
    }

    if (closeButtonBounds_.w > 0.0f) {
        bool closeHov = closeButtonBounds_.contains(ctx.mouseX, ctx.mouseY);
        drawScrewCloseButton(r, closeButtonBounds_.x + closeButtonBounds_.w * 0.5f,
                             closeButtonBounds_.y + closeButtonBounds_.h * 0.5f,
                             7.0f, closeHov, theme.primaryAccent);
    }

    // 4. Resize Grip Indicator
    bool canResize = (activeTabIndex_ >= 0 && tabs_[activeTabIndex_].resizable);
    if (canResize && (isResizing_ || resizeHandleBounds_.contains(ctx.mouseX, ctx.mouseY))) {
        if (edge_ == DockEdge::Bottom) {
            float handleCx = drawerBounds_.x + drawerBounds_.w * 0.5f;
            drawRoundedRect(r, handleCx - 24.0f, drawerBounds_.y + 1.0f, 48.0f, 3.0f, 1.5f,
                            isResizing_ ? theme.primaryAccent : theme.borderSubtle);
        } else if (edge_ == DockEdge::Right) {
            float handleCy = drawerBounds_.y + drawerBounds_.h * 0.5f;
            drawRoundedRect(r, drawerBounds_.x + 1.0f, handleCy - 24.0f, 3.0f, 48.0f, 1.5f,
                            isResizing_ ? theme.primaryAccent : theme.borderSubtle);
        }
    }

    // 5. Render Active Content View
    if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content) {
        tabs_[activeTabIndex_].content->onDrawerRender(r, theme, ctx);
    }
}

bool SlidingDock::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    // 1. Resizing
    if (isResizing_) {
        if (ev.action == PointerAction::Move) {
            float delta = (edge_ == DockEdge::Bottom) ? (resizeStartCoord_ - ev.y) : (resizeStartCoord_ - ev.x);
            setCurrentExtent(resizeStartExtent_ + delta);
            return true;
        }
        if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            isResizing_ = false;
            return true;
        }
        return true;
    }

    // 2. Close Button
    if (closeButtonBounds_.w > 0.0f && closeButtonBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            closeDock();
            return true;
        }
        return true;
    }

    // 3. Tab Buttons Hit Test
    for (size_t i = 0; i < tabBoundsList_.size(); ++i) {
        if (tabBoundsList_[i].contains(ev.x, ev.y)) {
            if (ev.action == PointerAction::Down && ev.button == PointerButton::Left) {
                toggleTab(tabs_[i].id);
                return true;
            }
            return true;
        }
    }

    // 4. Resize Handle Press
    bool canResize = (activeTabIndex_ >= 0 && tabs_[activeTabIndex_].resizable);
    if (canResize && isExpanded_ && resizeHandleBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down && ev.button == PointerButton::Left) {
            isResizing_ = true;
            resizeStartExtent_ = currentExtent_;
            resizeStartCoord_ = (edge_ == DockEdge::Bottom) ? ev.y : ev.x;
            return true;
        }
    }

    // 5. Delegate to Active Content
    if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content && contentBounds_.contains(ev.x, ev.y)) {
        if (tabs_[activeTabIndex_].content->onDrawerPointer(ev, ctx)) {
            return true;
        }
    }

    if (isOpen() && drawerBounds_.contains(ev.x, ev.y)) {
        return true;
    }

    return false;
}

bool SlidingDock::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (action != 1 /* GLFW_PRESS */) {
        if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content) {
            return tabs_[activeTabIndex_].content->onDrawerKey(key, scancode, action, mods, ctx);
        }
        return false;
    }

    // Check tab hotkeys
    for (size_t i = 0; i < tabs_.size(); ++i) {
        if (tabs_[i].hotkey > 0 && key == tabs_[i].hotkey) {
            toggleTab(tabs_[i].id);
            return true;
        }
    }

    // Esc closes dock
    if (key == 256 /* GLFW_KEY_ESCAPE */ && isOpen()) {
        closeDock();
        return true;
    }

    if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content) {
        return tabs_[activeTabIndex_].content->onDrawerKey(key, scancode, action, mods, ctx);
    }

    return false;
}

bool SlidingDock::handleChar(char32_t codepoint) {
    if (isOpen() && activeTabIndex_ >= 0 && tabs_[activeTabIndex_].content) {
        return tabs_[activeTabIndex_].content->onDrawerChar(codepoint);
    }
    return false;
}

} // namespace eatsbits::ui
