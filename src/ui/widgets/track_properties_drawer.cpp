#include "eatsbits/ui/widgets/track_properties_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {


TrackPropertiesDrawer::TrackPropertiesDrawer() {
    panel_.setShowTrackRibbon(false);
    panel_.setIsInsideDrawer(true);

    // Bridge callbacks from panel_ to drawer callbacks
    panel_.onTrackSelected = [this](uint32_t idx) { if (onTrackSelected) onTrackSelected(idx); };
    panel_.onTrackRename = [this](uint32_t idx) { if (onTrackRename) onTrackRename(idx); };
    panel_.onTrackRenameWithText = [this](uint32_t idx, const std::string& name) { if (onTrackRenameWithText) onTrackRenameWithText(idx, name); };
    panel_.onChooseTrackIcon = [this](uint32_t idx) { if (onChooseTrackIcon) onChooseTrackIcon(idx); };
    panel_.onVolumeChanged = [this](uint32_t idx, float vol) { if (onVolumeChanged) onVolumeChanged(idx, vol); };
    panel_.onPanChanged = [this](uint32_t idx, float pan) { if (onPanChanged) onPanChanged(idx, pan); };
    panel_.onMuteToggled = [this](uint32_t idx, bool mute) { if (onMuteToggled) onMuteToggled(idx, mute); };
    panel_.onSoloToggled = [this](uint32_t idx, bool solo) { if (onSoloToggled) onSoloToggled(idx, solo); };
    panel_.onFreezeToggled = [this](uint32_t idx, bool freeze) { if (onFreezeToggled) onFreezeToggled(idx, freeze); };
    panel_.onColorChanged = [this](uint32_t idx, float cr, float cg, float cb) { if (onColorChanged) onColorChanged(idx, cr, cg, cb); };
    panel_.onParamChanged = [this](uint32_t idx, const std::string& p, float v) { if (onParamChanged) onParamChanged(idx, p, v); };
    panel_.onChangeInstrument = [this](uint32_t idx) { if (onChangeInstrument) onChangeInstrument(idx); };
    panel_.onOpenDesign = [this](uint32_t idx) { if (onOpenDesign) onOpenDesign(idx); };
    panel_.onOpenPresets = [this](uint32_t idx) { if (onOpenPresets) onOpenPresets(idx); };
    panel_.onOpenFullscreenDevice = [this](uint32_t idx) { if (onOpenFullscreenDevice) onOpenFullscreenDevice(idx); };
    panel_.onPrevPreset = [this]() { if (onPrevPreset) onPrevPreset(); };
    panel_.onNextPreset = [this]() { if (onNextPreset) onNextPreset(); };
    panel_.onTabSelected = [this](TrackPropertiesTab t) { if (onTabSelected) onTabSelected(t); };
    panel_.onEditInPianoRoll = [this](uint32_t idx, int c) { if (onEditInPianoRoll) onEditInPianoRoll(idx, c); };
    panel_.onOpenCodeEditor = [this](uint32_t idx) { if (onOpenCodeEditor) onOpenCodeEditor(idx); };
    panel_.onAddMidiFx = [this](uint32_t idx) { if (onAddMidiFx) onAddMidiFx(idx); };
    panel_.onAddAudioFx = [this](uint32_t idx) { if (onAddAudioFx) onAddAudioFx(idx); };
    panel_.onRemoveMidiFx = [this](uint32_t idx, size_t fi) { if (onRemoveMidiFx) onRemoveMidiFx(idx, fi); };
    panel_.onRemoveAudioFx = [this](uint32_t idx, size_t fi) { if (onRemoveAudioFx) onRemoveAudioFx(idx, fi); };
    panel_.onToggleMidiFx = [this](uint32_t idx, size_t fi, bool en) { if (onToggleMidiFx) onToggleMidiFx(idx, fi, en); };
    panel_.onToggleAudioFx = [this](uint32_t idx, size_t fi, bool en) { if (onToggleAudioFx) onToggleAudioFx(idx, fi, en); };
    panel_.onReorderMidiFx = [this](uint32_t idx, size_t fromIdx, size_t toIdx) { if (onReorderMidiFx) onReorderMidiFx(idx, fromIdx, toIdx); };
    panel_.onReorderAudioFx = [this](uint32_t idx, size_t fromIdx, size_t toIdx) { if (onReorderAudioFx) onReorderAudioFx(idx, fromIdx, toIdx); };
    panel_.onMidiFxChanged = [this](uint32_t idx) { if (onMidiFxChanged) onMidiFxChanged(idx); };
    panel_.onAudioFxChanged = [this](uint32_t idx) { if (onAudioFxChanged) onAudioFxChanged(idx); };
    panel_.onAudioFxParamChanged = [this](uint32_t idx, const std::string& p, float v) { if (onAudioFxParamChanged) onAudioFxParamChanged(idx, p, v); };
    panel_.onMidiFxParamChanged = [this](uint32_t idx, const std::string& p, float v) { if (onMidiFxParamChanged) onMidiFxParamChanged(idx, p, v); };
    panel_.onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fi) { if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(idx, fi); };
    panel_.onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fi) { if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(idx, fi); };
}

void TrackPropertiesDrawer::update(float dt) noexcept {
    float target = isExpanded_ ? 1.0f : 0.0f;
    constexpr float kDrawerDamping = 18.0f; // ~180ms smooth ease-out
    float factor = 1.0f - std::exp(-kDrawerDamping * dt);
    animProgress_ += (target - animProgress_) * factor;
    if (std::abs(animProgress_ - target) < 0.001f) {
        animProgress_ = target;
    }
}

void TrackPropertiesDrawer::layout(const Rect2D& containerBounds, float browserOffset) {
    containerBounds_ = containerBounds;
    browserOffset_ = browserOffset;
    float drawerTotalW = kPullTabWidth + (width_ * animProgress_);
    float rightBoundary = containerBounds.x + containerBounds.w - drawerTotalW - browserOffset;

    pullTabBounds_ = Rect2D(rightBoundary, containerBounds.y, kPullTabWidth, containerBounds.h);
    drawerBounds_ = Rect2D(rightBoundary + kPullTabWidth, containerBounds.y, width_, containerBounds.h);
    closeButtonBounds_ = Rect2D(drawerBounds_.x + width_ - 28.0f, containerBounds.y + 7.0f, 22.0f, 22.0f);

    float contentX = drawerBounds_.x + 8.0f;
    float contentY = drawerBounds_.y + 44.0f;
    float contentW = drawerBounds_.w - 16.0f;
    float contentH = drawerBounds_.h - 48.0f;

    ViewContext ctx;
    ctx.logicalWidth = containerBounds.w;
    ctx.logicalHeight = containerBounds.h;
    panel_.layout(Rect2D(contentX, contentY, contentW, contentH), ctx);
}

void TrackPropertiesDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme,
                                   TrackPropertiesDrawerData& data, float mouseX, float mouseY, float dt) {
    update(dt);
    layout(containerBounds_, browserOffset_);

    data.syncKnobsIfEmpty();

    // 1. Vertical Pull Tab Strip
    drawRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.98f);
    drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 0.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);
    drawLine(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.x, pullTabBounds_.y + pullTabBounds_.h,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.5f);

    // Grip & Properties Icon / Label
    float gripY = pullTabBounds_.y + (pullTabBounds_.h * 0.5f);
    drawPropertiesIcon(r, pullTabBounds_.x + 5.0f, gripY - 55.0f, 14.0f, 14.0f,
                       (animProgress_ > 0.5f) ? theme.secondaryAccent : theme.primaryAccent);

    // Sideways rotated "PROPERTIES" label
    drawRotatedText(r, "PROPERTIES", pullTabBounds_.x + (pullTabBounds_.w * 0.5f), gripY + 8.0f, -90.0f, 9.5f,
                    (animProgress_ > 0.5f) ? theme.secondaryAccent : theme.primaryAccent);

    // Track Color Dot at Top of Pull Tab
    float dotColR = data.isMasterSelected ? 1.0f : data.r;
    float dotColG = data.isMasterSelected ? 0.85f : data.g;
    float dotColB = data.isMasterSelected ? 0.0f : data.b;
    drawCircle(r, pullTabBounds_.x + 12.0f, pullTabBounds_.y + 18.0f, 4.0f, dotColR, dotColG, dotColB, 1.0f);
    drawCircleOutline(r, pullTabBounds_.x + 12.0f, pullTabBounds_.y + 18.0f, 5.5f, dotColR, dotColG, dotColB, 0.5f, 1.0f);

    if (animProgress_ <= 0.001f) {
        if (panel_.getPluginSearchDialog().isOpen()) {
            panel_.getPluginSearchDialog().render(r, theme);
        }
        return;
    }

    // 2. Expanded Drawer Body Background
    drawRect(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h,
             theme.panelBackground.r * 0.92f, theme.panelBackground.g * 0.92f, theme.panelBackground.b * 0.92f, 0.98f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.x, drawerBounds_.y + drawerBounds_.h,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f, 1.5f);
    drawRoundedRectOutline(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h, 0.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

    // 3. Top Title Header Bar (36px high)
    drawRect(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, 36.0f,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.95f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y + 36.0f, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y + 36.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.5f);

    if (!data.isMixerMode) {
        // Arranger View Context: TRACK vs CLIP Tab Switcher at top
        float tabW = (drawerBounds_.w - 60.0f) * 0.5f;
        float tabY = drawerBounds_.y + 5.0f;
        float tabH = 26.0f;

        bool isTrk = (data.tab == TrackPropertiesTab::Track);
        drawRoundedRect(r, drawerBounds_.x + 10.0f, tabY, tabW, tabH, 4.0f,
                        isTrk ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                        isTrk ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                        isTrk ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.95f);
        drawRoundedRectOutline(r, drawerBounds_.x + 10.0f, tabY, tabW, tabH, 4.0f,
                               isTrk ? theme.primaryAccent.r : theme.borderSubtle.r,
                               isTrk ? theme.primaryAccent.g : theme.borderSubtle.g,
                               isTrk ? theme.primaryAccent.b : theme.borderSubtle.b,
                               isTrk ? 0.9f : 0.4f, 1.0f);
        drawCenteredText(r, "TRACK PROPERTIES", drawerBounds_.x + 10.0f, tabY, tabW, tabH, 9.5f,
                         isTrk ? theme.primaryAccent.r : theme.textMuted.r,
                         isTrk ? theme.primaryAccent.g : theme.textMuted.g,
                         isTrk ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);

        bool isClp = (data.tab == TrackPropertiesTab::Clip);
        drawRoundedRect(r, drawerBounds_.x + 14.0f + tabW, tabY, tabW, tabH, 4.0f,
                        isClp ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                        isClp ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                        isClp ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.95f);
        drawRoundedRectOutline(r, drawerBounds_.x + 14.0f + tabW, tabY, tabW, tabH, 4.0f,
                               isClp ? theme.primaryAccent.r : theme.borderSubtle.r,
                               isClp ? theme.primaryAccent.g : theme.borderSubtle.g,
                               isClp ? theme.primaryAccent.b : theme.borderSubtle.b,
                               isClp ? 0.9f : 0.4f, 1.0f);
        drawCenteredText(r, "CLIP PROPERTIES", drawerBounds_.x + 14.0f + tabW, tabY, tabW, tabH, 9.5f,
                         isClp ? theme.primaryAccent.r : theme.textMuted.r,
                         isClp ? theme.primaryAccent.g : theme.textMuted.g,
                         isClp ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
    } else {
        // Mixer View Context: Fixed Header Title
        drawCircle(r, drawerBounds_.x + 18.0f, drawerBounds_.y + 18.0f, 4.0f, dotColR, dotColG, dotColB, 1.0f);
        std::string titleText = data.isMasterSelected ? "MASTER BUS CONSOLE" : "TRACK PROPERTIES";
        drawText(r, titleText, drawerBounds_.x + 28.0f, drawerBounds_.y + 12.0f, 11.5f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    }

    // Close Button (metallic screw icon)
    float cx = closeButtonBounds_.x + closeButtonBounds_.w * 0.5f;
    float cy = closeButtonBounds_.y + closeButtonBounds_.h * 0.5f;
    bool closeHov = closeButtonBounds_.contains(mouseX, mouseY) || std::hypot(mouseX - cx, mouseY - cy) <= 12.0f;
    drawScrewCloseButton(r, cx, cy, 9.0f, closeHov, theme.primaryAccent);

    // 4. Delegate card stack & content rendering to TrackPropertiesPanel
    panel_.render(r, theme, data, mouseX, mouseY);

    // 5. Floating tactile tooltip badge on hover
    if (mouseX >= 0.0f && mouseY >= 0.0f) {
        std::string tip = getTooltip(mouseX, mouseY, data);
        if (!tip.empty()) {
            drawTooltipBadge(r, tip, mouseX, mouseY - 14.0f, theme, false, containerBounds_.x + containerBounds_.w);
        }
    }
}

std::string TrackPropertiesDrawer::getTooltip(float x, float y, const TrackPropertiesDrawerData& data) const noexcept {
    if (pullTabBounds_.contains(x, y)) {
        return isExpanded_ ? "Collapse Properties Drawer" : "Expand Properties Drawer";
    }

    if (isExpanded_ && animProgress_ > 0.05f) {
        float cx = closeButtonBounds_.x + closeButtonBounds_.w * 0.5f;
        float cy = closeButtonBounds_.y + closeButtonBounds_.h * 0.5f;
        if (closeButtonBounds_.contains(x, y) || std::hypot(x - cx, y - cy) <= 12.0f) {
            return "Close Properties (Esc)";
        }

        if (!data.isMixerMode && y >= drawerBounds_.y + 5.0f && y <= drawerBounds_.y + 31.0f) {
            float tabW = (drawerBounds_.w - 60.0f) * 0.5f;
            if (x >= drawerBounds_.x + 10.0f && x <= drawerBounds_.x + 10.0f + tabW) {
                return "Track Properties & Device Chain";
            }
            if (x >= drawerBounds_.x + 14.0f + tabW && x <= drawerBounds_.x + 14.0f + tabW * 2.0f) {
                return "Clip Inspector & Playback Parameters";
            }
        }

        return panel_.getTooltip(x, y, data);
    }
    return "";
}

TrackPropertiesHitResult TrackPropertiesDrawer::hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    TrackPropertiesHitResult res;

    if (panel_.getPluginSearchDialog().isOpen()) {
        res.hit = true;
        return res;
    }

    if (pullTabBounds_.contains(mx, my)) {
        res.hit = true;
        res.area = TrackPropertiesHitArea::PullTab;
        return res;
    }

    if (animProgress_ > 0.05f) {
        if (closeButtonBounds_.contains(mx, my)) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::CloseButton;
            return res;
        }

        if (!data.isMixerMode && my >= drawerBounds_.y + 5.0f && my <= drawerBounds_.y + 31.0f) {
            float tabW = (drawerBounds_.w - 60.0f) * 0.5f;
            if (mx >= drawerBounds_.x + 10.0f && mx <= drawerBounds_.x + 10.0f + tabW) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::TabTrack;
                return res;
            }
            if (mx >= drawerBounds_.x + 14.0f + tabW && mx <= drawerBounds_.x + 14.0f + tabW * 2.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::TabClip;
                return res;
            }
        }

        // Delegate to panel
        return panel_.hitTest(mx, my, data);
    }

    return res;
}

bool TrackPropertiesDrawer::handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data,
                                          const ViewContext& ctx) {
    data.syncKnobsIfEmpty();

    // 0. Forward to embedded panel modal dialog if open (covers full screen)
    if (panel_.getPluginSearchDialog().isOpen()) {
        return panel_.handlePointer(ev, data, ctx);
    }

    // 1. Pull Tab Resize & Toggle
    if (pullTabBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            isResizing_ = true;
            resizeStartX_ = ev.x;
            resizeStartWidth_ = width_;
            return true;
        } else if (ev.action == PointerAction::Up) {
            if (isResizing_) {
                if (std::abs(ev.x - resizeStartX_) < 4.0f) {
                    toggle();
                }
                isResizing_ = false;
                return true;
            }
        }
    }

    if (isResizing_) {
        if (ev.action == PointerAction::Move) {
            float dx = resizeStartX_ - ev.x;
            setWidth(resizeStartWidth_ + dx);
            return true;
        } else if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            isResizing_ = false;
            return true;
        }
    }

    // 1b. Active Continuous Dragging forward (knobs, sliders, scrollbar)
    if (panel_.isDragging()) {
        return panel_.handlePointer(ev, data, ctx);
    }

    // 2. Drawer Interaction
    if (animProgress_ > 0.05f && drawerBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            if (closeButtonBounds_.contains(ev.x, ev.y)) {
                setExpanded(false);
                return true;
            }

            if (!data.isMixerMode && ev.y >= drawerBounds_.y + 5.0f && ev.y <= drawerBounds_.y + 31.0f) {
                float tabW = (drawerBounds_.w - 60.0f) * 0.5f;
                if (ev.x >= drawerBounds_.x + 10.0f && ev.x <= drawerBounds_.x + 10.0f + tabW) {
                    data.tab = TrackPropertiesTab::Track;
                    if (onTabSelected) onTabSelected(data.tab);
                    return true;
                }
                if (ev.x >= drawerBounds_.x + 14.0f + tabW && ev.x <= drawerBounds_.x + 14.0f + tabW * 2.0f) {
                    data.tab = TrackPropertiesTab::Clip;
                    if (onTabSelected) onTabSelected(data.tab);
                    return true;
                }
            }
        }

        // Delegate to unified panel
        return panel_.handlePointer(ev, data, ctx);
    }

    return false;
}

} // namespace eatsbits::ui
