#include "eatsbits/ui/widgets/track_properties_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cmath>
#include <atomic>

namespace eatsbits::ui {
TrackPropertiesDrawer::TrackPropertiesDrawer() {
    DrawerConfig cfg;
    cfg.edge = DockEdge::Right;
    cfg.tab.title = "PROPERTIES";
    cfg.tab.fullEdgeTab = true;
    cfg.tab.tabWidth = kPullTabWidth;
    cfg.tab.preferredExtent = kDefaultWidth;
    cfg.tab.minExtent = kMinWidth;
    cfg.tab.maxExtent = kMaxWidth;
    cfg.tab.resizable = true;
    cfg.tab.showCloseButton = true;
    cfg.tab.headerHeight = 44.0f;
    drawer_.setConfig(cfg);
    drawer_.setExpanded(true);
    drawer_.setAnimProgress(1.0f);

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
    panel_.onOpenDetachedWindow = [this](uint32_t idx) { if (onOpenDetachedWindow) onOpenDetachedWindow(idx); };
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
    panel_.onAudioFxParamChanged = [this](uint32_t idx, size_t fxIdx, const std::string& p, float v) { if (onAudioFxParamChanged) onAudioFxParamChanged(idx, fxIdx, p, v); };
    panel_.onMidiFxParamChanged = [this](uint32_t idx, const std::string& p, float v) { if (onMidiFxParamChanged) onMidiFxParamChanged(idx, p, v); };
    panel_.onOpenFullscreenAudioFx = [this](uint32_t idx, size_t fi) { if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(idx, fi); };
    panel_.onOpenFullscreenMidiFx = [this](uint32_t idx, size_t fi) { if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(idx, fi); };
    panel_.onAddClip = [this](uint32_t idx) { if (onAddClip) onAddClip(idx); };
    panel_.onDeleteTrack = [this](uint32_t idx) { if (onDeleteTrack) onDeleteTrack(idx); };
    panel_.onDuplicateTrack = [this](uint32_t idx) { if (onDuplicateTrack) onDuplicateTrack(idx); };
    panel_.onDuplicateClip = [this](uint32_t tIdx, int cIdx) { if (onDuplicateClip) onDuplicateClip(tIdx, cIdx); };
    panel_.onDeleteClip = [this](uint32_t tIdx, int cIdx) { if (onDeleteClip) onDeleteClip(tIdx, cIdx); };
    panel_.onChordFollowChanged = [this](uint32_t idx, ChordFollowMode mode) { if (onChordFollowChanged) onChordFollowChanged(idx, mode); };
    panel_.onBakeChords = [this](uint32_t idx) { if (onBakeChords) onBakeChords(idx); };
}

void TrackPropertiesDrawer::update(float dt) noexcept {
    drawer_.update(dt);
}

void TrackPropertiesDrawer::layout(const Rect2D& containerBounds, float browserOffset) {
    containerBounds_ = containerBounds;
    browserOffset_ = browserOffset;

    drawer_.layout(containerBounds, browserOffset);

    const Rect2D& drBounds = drawer_.getDrawerBounds();
    float contentX = drBounds.x + 8.0f;
    float contentY = drBounds.y + 44.0f;
    float contentW = drBounds.w - 16.0f;
    float contentH = drBounds.h - 48.0f;

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

    const Rect2D& pullTabBounds = drawer_.getPullTabBounds();
    const Rect2D& drawerBounds = drawer_.getDrawerBounds();
    const float animProgress = drawer_.getAnimProgress();

    // 1. Vertical Pull Tab Strip
    drawRect(r, pullTabBounds.x, pullTabBounds.y, pullTabBounds.w, pullTabBounds.h,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.98f);
    drawRoundedRectOutline(r, pullTabBounds.x, pullTabBounds.y, pullTabBounds.w, pullTabBounds.h, 0.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);
    drawLine(r, pullTabBounds.x, pullTabBounds.y, pullTabBounds.x, pullTabBounds.y + pullTabBounds.h,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f, 1.5f);

    // Grip & Properties Icon / Label
    float gripY = pullTabBounds.y + (pullTabBounds.h * 0.5f);
    drawPropertiesIcon(r, pullTabBounds.x + 5.0f, gripY - 55.0f, 14.0f, 14.0f,
                       (animProgress > 0.5f) ? theme.secondaryAccent : theme.primaryAccent);

    // Sideways rotated "PROPERTIES" label
    drawRotatedText(r, "PROPERTIES", pullTabBounds.x + (pullTabBounds.w * 0.5f), gripY + 8.0f, -90.0f, 9.5f,
                    (animProgress > 0.5f) ? theme.secondaryAccent : theme.primaryAccent);

    // Track Color Dot at Top of Pull Tab
    float dotColR = data.isMasterSelected ? 1.0f : data.r;
    float dotColG = data.isMasterSelected ? 0.85f : data.g;
    float dotColB = data.isMasterSelected ? 0.0f : data.b;
    drawCircle(r, pullTabBounds.x + 12.0f, pullTabBounds.y + 18.0f, 4.0f, dotColR, dotColG, dotColB, 1.0f);
    drawCircleOutline(r, pullTabBounds.x + 12.0f, pullTabBounds.y + 18.0f, 5.5f, dotColR, dotColG, dotColB, 0.5f, 1.0f);

    if (animProgress <= 0.001f) {
        if (panel_.getPluginSearchDialog().isOpen()) {
            panel_.getPluginSearchDialog().render(r, theme);
        }
        return;
    }

    // 2. Expanded Drawer Body Background
    drawRect(r, drawerBounds.x, drawerBounds.y, drawerBounds.w, drawerBounds.h,
             theme.panelBackground.r * 0.92f, theme.panelBackground.g * 0.92f, theme.panelBackground.b * 0.92f, 0.98f);
    drawLine(r, drawerBounds.x, drawerBounds.y, drawerBounds.x, drawerBounds.y + drawerBounds.h,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.85f, 1.5f);
    drawRoundedRectOutline(r, drawerBounds.x, drawerBounds.y, drawerBounds.w, drawerBounds.h, 0.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

    // 3. Delegate card stack & content rendering to TrackPropertiesPanel
    // Scissor strictly to content area below 36px header bar
    float contentY = drawerBounds.y + 36.0f;
    float contentH = std::max(0.0f, drawerBounds.h - 36.0f);
    {
        BatchRenderer2D::ScissorScope contentScissor(r, drawerBounds.x, contentY, drawerBounds.w, contentH);
        panel_.render(r, theme, data, mouseX, mouseY);
    }

    // 4. Top Title Header Bar (36px high) - Rendered over content so scrolling never overwrites header
    drawRect(r, drawerBounds.x, drawerBounds.y, drawerBounds.w, 36.0f,
             theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.98f);
    drawLine(r, drawerBounds.x, drawerBounds.y + 36.0f, drawerBounds.x + drawerBounds.w, drawerBounds.y + 36.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.5f);

    if (!data.isMixerMode) {
        // Arranger View Context: TRACK vs CLIP Tab Switcher at top
        float tabW = (drawerBounds.w - 60.0f) * 0.5f;
        float tabY = drawerBounds.y + 5.0f;
        float tabH = 26.0f;

        bool isTrk = (data.tab == TrackPropertiesTab::Track);
        drawRoundedRect(r, drawerBounds.x + 10.0f, tabY, tabW, tabH, 4.0f,
                        isTrk ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                        isTrk ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                        isTrk ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.95f);
        drawRoundedRectOutline(r, drawerBounds.x + 10.0f, tabY, tabW, tabH, 4.0f,
                               isTrk ? theme.primaryAccent.r : theme.borderSubtle.r,
                               isTrk ? theme.primaryAccent.g : theme.borderSubtle.g,
                               isTrk ? theme.primaryAccent.b : theme.borderSubtle.b,
                               isTrk ? 0.9f : 0.4f, 1.0f);
        drawCenteredText(r, "TRACK", drawerBounds.x + 10.0f, tabY, tabW, tabH, 9.5f,
                         isTrk ? theme.primaryAccent.r : theme.textMuted.r,
                         isTrk ? theme.primaryAccent.g : theme.textMuted.g,
                         isTrk ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);

        bool isClp = (data.tab == TrackPropertiesTab::Clip);
        drawRoundedRect(r, drawerBounds.x + 14.0f + tabW, tabY, tabW, tabH, 4.0f,
                        isClp ? theme.primaryAccent.r * 0.25f : theme.controlBackground.r,
                        isClp ? theme.primaryAccent.g * 0.25f : theme.controlBackground.g,
                        isClp ? theme.primaryAccent.b * 0.25f : theme.controlBackground.b, 0.95f);
        drawRoundedRectOutline(r, drawerBounds.x + 14.0f + tabW, tabY, tabW, tabH, 4.0f,
                               isClp ? theme.primaryAccent.r : theme.borderSubtle.r,
                               isClp ? theme.primaryAccent.g : theme.borderSubtle.g,
                               isClp ? theme.primaryAccent.b : theme.borderSubtle.b,
                               isClp ? 0.9f : 0.4f, 1.0f);
        drawCenteredText(r, "CLIP", drawerBounds.x + 14.0f + tabW, tabY, tabW, tabH, 9.5f,
                         isClp ? theme.primaryAccent.r : theme.textMuted.r,
                         isClp ? theme.primaryAccent.g : theme.textMuted.g,
                         isClp ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
    } else {
        // Mixer View Context: Fixed Header Title
        drawCircle(r, drawerBounds.x + 18.0f, drawerBounds.y + 18.0f, 4.0f, dotColR, dotColG, dotColB, 1.0f);
        const char* titleText = data.isMasterSelected ? "MASTER BUS CONSOLE" : "TRACK PROPERTIES";
        drawText(r, titleText, drawerBounds.x + 28.0f, drawerBounds.y + 12.0f, 11.5f, theme.primaryAccent, 1.0f);
    }

    // Close Button (metallic screw icon)
    const Rect2D& closeButtonBounds = drawer_.getCloseButtonBounds();
    float cx = closeButtonBounds.x + closeButtonBounds.w * 0.5f;
    float cy = closeButtonBounds.y + closeButtonBounds.h * 0.5f;
    bool closeHov = closeButtonBounds.contains(mouseX, mouseY) || std::hypot(mouseX - cx, mouseY - cy) <= 12.0f;
    drawScrewCloseButton(r, cx, cy, 9.0f, closeHov, theme.primaryAccent);

    // 5. Floating tactile tooltip badge on hover
    if (mouseX >= 0.0f && mouseY >= 0.0f) {
        std::string tip = getTooltip(mouseX, mouseY, data);
        if (!tip.empty()) {
            drawTooltipBadge(r, tip, mouseX, mouseY - 14.0f, theme, false, containerBounds_.x + containerBounds_.w);
        }
    }
}

std::string TrackPropertiesDrawer::getTooltip(float x, float y, const TrackPropertiesDrawerData& data) const noexcept {
    const Rect2D& pullTabBounds = drawer_.getPullTabBounds();
    const Rect2D& drawerBounds = drawer_.getDrawerBounds();
    const Rect2D& closeButtonBounds = drawer_.getCloseButtonBounds();
    const float animProgress = drawer_.getAnimProgress();
    const bool isExpanded = drawer_.isExpanded();

    if (pullTabBounds.contains(x, y)) {
        return isExpanded ? "Collapse Properties Drawer" : "Expand Properties Drawer";
    }

    if (isExpanded && animProgress > 0.05f) {
        float cx = closeButtonBounds.x + closeButtonBounds.w * 0.5f;
        float cy = closeButtonBounds.y + closeButtonBounds.h * 0.5f;
        if (closeButtonBounds.contains(x, y) || std::hypot(x - cx, y - cy) <= 12.0f) {
            return "Close Properties (Esc)";
        }

        if (!data.isMixerMode && y >= drawerBounds.y + 5.0f && y <= drawerBounds.y + 31.0f) {
            float tabW = (drawerBounds.w - 60.0f) * 0.5f;
            if (x >= drawerBounds.x + 10.0f && x <= drawerBounds.x + 10.0f + tabW) {
                return "Track Properties & Device Chain";
            }
            if (x >= drawerBounds.x + 14.0f + tabW && x <= drawerBounds.x + 14.0f + tabW * 2.0f) {
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

    const Rect2D& pullTabBounds = drawer_.getPullTabBounds();
    const Rect2D& drawerBounds = drawer_.getDrawerBounds();
    const Rect2D& closeButtonBounds = drawer_.getCloseButtonBounds();
    const float animProgress = drawer_.getAnimProgress();

    if (pullTabBounds.contains(mx, my)) {
        res.hit = true;
        res.area = TrackPropertiesHitArea::PullTab;
        return res;
    }

    if (animProgress > 0.05f) {
        if (closeButtonBounds.contains(mx, my)) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::CloseButton;
            return res;
        }

        if (!data.isMixerMode && my >= drawerBounds.y + 5.0f && my <= drawerBounds.y + 31.0f) {
            float tabW = (drawerBounds.w - 60.0f) * 0.5f;
            if (mx >= drawerBounds.x + 10.0f && mx <= drawerBounds.x + 10.0f + tabW) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::TabTrack;
                return res;
            }
            if (mx >= drawerBounds.x + 14.0f + tabW && mx <= drawerBounds.x + 14.0f + tabW * 2.0f) {
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

    // 1. Let unified drawer handle pull-tab click, close button, and left-edge drag resize
    if (drawer_.handlePointer(ev, ctx)) {
        return true;
    }

    // 1b. Active Continuous Dragging forward (knobs, sliders, scrollbar)
    if (panel_.isDragging()) {
        return panel_.handlePointer(ev, data, ctx);
    }

    const Rect2D& drawerBounds = drawer_.getDrawerBounds();
    const float animProgress = drawer_.getAnimProgress();

    // 2. Drawer Interaction
    if (animProgress > 0.05f && drawerBounds.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            if (!data.isMixerMode && ev.y >= drawerBounds.y + 5.0f && ev.y <= drawerBounds.y + 31.0f) {
                float tabW = (drawerBounds.w - 60.0f) * 0.5f;
                if (ev.x >= drawerBounds.x + 10.0f && ev.x <= drawerBounds.x + 10.0f + tabW) {
                    data.tab = TrackPropertiesTab::Track;
                    if (onTabSelected) onTabSelected(data.tab);
                    return true;
                }
                if (ev.x >= drawerBounds.x + 14.0f + tabW && ev.x <= drawerBounds.x + 14.0f + tabW * 2.0f) {
                    data.tab = TrackPropertiesTab::Clip;
                    if (onTabSelected) onTabSelected(data.tab);
                    return true;
                }
            }
        }

        // Delegate to unified panel and absorb interaction so it never passes through to components behind it
        panel_.handlePointer(ev, data, ctx);
        return true;
    }

    return false;
}

} // namespace eatsbits::ui


