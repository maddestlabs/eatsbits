#include "eatsbits/ui/widgets/arranger_mixer_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/views/view_base.hpp"
#include "eatsbits/ui/views/arranger_view.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace eatsbits::ui {

ArrangerMixerDrawer::ArrangerMixerDrawer() {
    height_ = kDefaultHeight;
}

ArrangerMixerDrawer::FaderGeometry ArrangerMixerDrawer::computeFaderGeometry(
    float wellX, float wellY, float wellW, float wellH, float gain) const {
    FaderGeometry geom;
    geom.well = Rect2D(wellX, wellY, wellW, wellH);
    geom.travel = presenter::audio_taper::gainToTravel(gain);

    const float thumbH = 14.0f;
    const float thumbW = wellW + 10.0f;
    const float thumbY = wellY + (1.0f - geom.travel) * (wellH - thumbH);
    const float thumbX = wellX - 5.0f;
    geom.thumb = Rect2D(thumbX, thumbY, thumbW, thumbH);
    return geom;
}

void ArrangerMixerDrawer::layout(const Rect2D& containerBounds, float rightMargin) {
    containerBounds_ = containerBounds;
    rightMargin_ = rightMargin;

    const float effectiveW = (std::max)(200.0f, containerBounds.w - rightMargin);
    const float currentH = height_ * animProgress_;
    const float drawerY = (containerBounds.y + containerBounds.h) - currentH;

    drawerBounds_ = Rect2D(containerBounds.x, drawerY, effectiveW, currentH);

    // Pull-tab geometry
    const float tabX = containerBounds.x + (effectiveW - kPullTabWidth) * 0.5f;
    if (animProgress_ <= 0.001f) {
        pullTabBounds_ = Rect2D(tabX, containerBounds.y + containerBounds.h - kPullTabHeight, kPullTabWidth, kPullTabHeight);
    } else {
        pullTabBounds_ = Rect2D(tabX, drawerY - kPullTabHeight, kPullTabWidth, kPullTabHeight);
    }

    if (animProgress_ <= 0.001f) {
        headerBounds_ = Rect2D();
        closeBtnBounds_ = Rect2D();
        resizeHandleBounds_ = Rect2D();
        masterStripBounds_ = Rect2D();
        channelsViewportBounds_ = Rect2D();
        scrollBarBounds_ = Rect2D();
        scrollThumbBounds_ = Rect2D();
        return;
    }

    // Top grab/resize handle
    resizeHandleBounds_ = Rect2D(containerBounds.x, drawerY - 4.0f, effectiveW, 8.0f);

    // Top Header bar inside drawer
    headerBounds_ = Rect2D(containerBounds.x, drawerY, effectiveW, 24.0f);
    closeBtnBounds_ = Rect2D(containerBounds.x + effectiveW - 26.0f, drawerY + 3.0f, 20.0f, 18.0f);

    // Master Channel Strip (docked on left)
    const float contentY = drawerY + 24.0f;
    const float contentH = (std::max)(20.0f, currentH - 24.0f);
    masterStripBounds_ = Rect2D(containerBounds.x + 6.0f, contentY + 2.0f, kMasterStripWidth, contentH - 4.0f);

    // Channels Viewport
    const float channelsX = masterStripBounds_.x + masterStripBounds_.w + 8.0f;
    const float channelsW = (std::max)(50.0f, (containerBounds.x + effectiveW - 6.0f) - channelsX);
    channelsViewportBounds_ = Rect2D(channelsX, contentY + 2.0f, channelsW, contentH - 4.0f);

    // Scrollbar bounds along bottom of channels viewport
    const float scrollbarH = 6.0f;
    scrollBarBounds_ = Rect2D(channelsX, contentY + contentH - scrollbarH - 2.0f, channelsW, scrollbarH);
}

void ArrangerMixerDrawer::update(float dt) noexcept {
    const float target = isExpanded_ ? 1.0f : 0.0f;
    constexpr float kDamping = 18.0f;
    const float factor = 1.0f - std::exp(-kDamping * dt);
    animProgress_ += (target - animProgress_) * factor;
    if (std::abs(animProgress_ - target) < 0.001f) {
        animProgress_ = target;
    }
}

void ArrangerMixerDrawer::render(
    BatchRenderer2D& r, const ThemeTokens& theme,
    const std::vector<ArrangerTimelineTrack>& tracks,
    uint32_t activeTrackIndex,
    float masterVol, float masterPan, bool masterMute,
    float masterPeakL, float masterPeakR,
    const float* chPeaksL, const float* chPeaksR, size_t numPeaks,
    float mouseX, float mouseY) {

    (void)masterPan;

    // 1. Draw Pull Tab
    const bool isHoverTab = pullTabBounds_.contains(mouseX, mouseY);
    const Color tabBg = isHoverTab ? theme.panelHeader.lighten(0.08f) : theme.panelHeader;

    drawRoundedRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                    tabBg.r, tabBg.g, tabBg.b, 0.96f);
    drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                           isExpanded_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);

    // Icon + Label
    const float iconX = pullTabBounds_.x + 10.0f;
    const float iconY = pullTabBounds_.y + (pullTabBounds_.h - 12.0f) * 0.5f;
    drawSlidersTuneIcon(r, iconX, iconY, 12.0f, isExpanded_ ? theme.primaryAccent : theme.textSecondary);

    const std::string tabLabel = isExpanded_ ? "MIXER [M] ^" : "ARRANGER MIXER [M]";
    const Color labelCol = isExpanded_ ? theme.primaryAccent : theme.textPrimary;
    drawCenteredText(r, tabLabel, pullTabBounds_.x + 24.0f, pullTabBounds_.y, pullTabBounds_.w - 42.0f, pullTabBounds_.h, 10.0f, labelCol);

    // Active status mini-LED on right
    const float ledX = pullTabBounds_.x + pullTabBounds_.w - 12.0f;
    const float ledY = pullTabBounds_.y + (pullTabBounds_.h * 0.5f);
    if (isExpanded_) {
        drawCircle(r, ledX, ledY, 3.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    } else {
        drawCircle(r, ledX, ledY, 2.5f, 0.35f, 0.35f, 0.35f, 0.8f);
    }

    if (animProgress_ <= 0.001f || drawerBounds_.h <= 5.0f) {
        return;
    }

    // 2. Drop Shadow above drawer
    drawRect(r, drawerBounds_.x, drawerBounds_.y - 5.0f, drawerBounds_.w, 5.0f, 0.0f, 0.0f, 0.0f, 0.45f * animProgress_);

    // 3. Drawer Chassis Background
    drawRectGradient(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.w, drawerBounds_.h,
                     theme.panelBackground.lighten(0.03f), theme.panelBackground.darken(0.12f));

    // Top accent border line
    drawLine(r, drawerBounds_.x, drawerBounds_.y, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y,
             theme.primaryAccent, 0.85f * animProgress_, 1.5f);

    // Subtle resize grip dots in center of top edge
    const float gripCx = drawerBounds_.x + drawerBounds_.w * 0.5f;
    for (int g = -2; g <= 2; ++g) {
        drawCircle(r, gripCx + static_cast<float>(g) * 6.0f, drawerBounds_.y + 2.0f, 1.2f, theme.textMuted, 0.6f);
    }

    if (drawerBounds_.h < 35.0f) {
        return;
    }

    // 4. Header Bar (Title, Active track badge, Close button)
    drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h,
             theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.75f);
    drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h,
             headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
             theme.borderSubtle, 0.7f, 1.0f);

    drawText(r, "ARRANGER MIXER", headerBounds_.x + 10.0f, headerBounds_.y + 6.0f, 10.5f, theme.primaryAccent);

    // Selected track info badge
    if (activeTrackIndex < tracks.size()) {
        const auto& actTrack = tracks[activeTrackIndex];
        const std::string badge = "TRACK #" + std::to_string(activeTrackIndex + 1) + " • " + actTrack.name;
        drawText(r, badge, headerBounds_.x + 120.0f, headerBounds_.y + 6.5f, 9.5f, theme.textSecondary);
    }

    // Close button [v]
    const bool isCloseHover = closeBtnBounds_.contains(mouseX, mouseY);
    drawButton(r, closeBtnBounds_, "v",
               isCloseHover ? theme.primaryAccent.withAlpha(0.3f) : theme.controlBackground,
               isCloseHover ? theme.primaryAccent : theme.borderSubtle,
               isCloseHover ? theme.primaryAccent : theme.textMuted,
               9.0f, 3.0f, 1.0f);

    // 5. MASTER CHANNEL STRIP (Fixed on left)
    const Rect2D& mb = masterStripBounds_;
    drawRoundedRect(r, mb.x, mb.y, mb.w, mb.h, 4.0f,
                    theme.controlBackground.darken(0.08f), 0.95f);
    drawRoundedRectOutline(r, mb.x, mb.y, mb.w, mb.h, 4.0f,
                           Color(1.0f, 0.75f, 0.2f, 0.65f), 1.0f);

    // Master Header LCD pill
    drawRoundedRect(r, mb.x + 4.0f, mb.y + 4.0f, mb.w - 8.0f, 18.0f, 3.0f,
                    0.12f, 0.10f, 0.04f, 0.95f);
    drawRoundedRectOutline(r, mb.x + 4.0f, mb.y + 4.0f, mb.w - 8.0f, 18.0f, 3.0f,
                           1.0f, 0.75f, 0.2f, 0.8f, 1.0f);
    drawCenteredText(r, "MASTER", mb.x + 4.0f, mb.y + 4.0f, mb.w - 8.0f, 18.0f, 9.5f,
                     Color(1.0f, 0.85f, 0.35f, 1.0f));

    // Master Mute Button
    const Rect2D masterMuteBtn(mb.x + (mb.w - 36.0f) * 0.5f, mb.y + 26.0f, 36.0f, 16.0f);
    drawButton(r, masterMuteBtn, "MUTE",
               masterMute ? Color(0.85f, 0.2f, 0.2f, 0.9f) : theme.controlBackground,
               masterMute ? Color(1.0f, 0.3f, 0.3f, 1.0f) : theme.borderSubtle,
               masterMute ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted,
               8.5f, 3.0f, 1.0f);

    // Master Fader and Peak Meter
    const float faderTopY = mb.y + 48.0f;
    const float faderBottomY = mb.y + mb.h - 22.0f;
    const float faderH = (std::max)(20.0f, faderBottomY - faderTopY);
    const float wellX = mb.x + 14.0f;
    const float wellW = 14.0f;

    auto masterGeom = computeFaderGeometry(wellX, faderTopY, wellW, faderH, masterVol);

    // Fader Well
    drawRoundedRect(r, masterGeom.well.x, masterGeom.well.y, masterGeom.well.w, masterGeom.well.h, 2.0f,
                    0.05f, 0.05f, 0.06f, 0.95f);
    drawRoundedRectOutline(r, masterGeom.well.x, masterGeom.well.y, masterGeom.well.w, masterGeom.well.h, 2.0f,
                           theme.borderSubtle, 1.0f);

    // Center groove line
    drawLine(r, masterGeom.well.x + masterGeom.well.w * 0.5f, masterGeom.well.y + 2.0f,
             masterGeom.well.x + masterGeom.well.w * 0.5f, masterGeom.well.y + masterGeom.well.h - 2.0f,
             0.0f, 0.0f, 0.0f, 0.9f, 1.5f);

    // Unity 0 dB tick mark at 0.75 travel
    const float unityY = masterGeom.well.y + (1.0f - 0.75f) * masterGeom.well.h;
    drawLine(r, masterGeom.well.x - 3.0f, unityY, masterGeom.well.x + masterGeom.well.w + 3.0f, unityY,
             1.0f, 0.8f, 0.2f, 0.7f, 1.0f);

    // Fader Thumb
    drawRoundedRect(r, masterGeom.thumb.x, masterGeom.thumb.y, masterGeom.thumb.w, masterGeom.thumb.h, 3.0f,
                    0.85f, 0.87f, 0.90f, 0.98f);
    drawRoundedRectOutline(r, masterGeom.thumb.x, masterGeom.thumb.y, masterGeom.thumb.w, masterGeom.thumb.h, 3.0f,
                           0.3f, 0.3f, 0.35f, 1.0f, 1.0f);
    // Gold indicator line on master thumb
    drawLine(r, masterGeom.thumb.x + 2.0f, masterGeom.thumb.y + masterGeom.thumb.h * 0.5f,
             masterGeom.thumb.x + masterGeom.thumb.w - 2.0f, masterGeom.thumb.y + masterGeom.thumb.h * 0.5f,
             1.0f, 0.75f, 0.2f, 1.0f, 1.5f);

    // Master Stereo Peak Meter (L and R)
    const float meterX = wellX + wellW + 10.0f;
    const float meterBarW = 4.0f;
    const float meterH = faderH;

    // Meter wells
    drawRect(r, meterX, faderTopY, meterBarW, meterH, 0.06f, 0.06f, 0.07f, 0.9f);
    drawRect(r, meterX + meterBarW + 2.0f, faderTopY, meterBarW, meterH, 0.06f, 0.06f, 0.07f, 0.9f);

    // Filled levels
    const float normL = masterMute ? 0.0f : (std::clamp)(masterPeakL, 0.0f, 1.2f);
    const float normR = masterMute ? 0.0f : (std::clamp)(masterPeakR, 0.0f, 1.2f);
    const float fillHL = (std::clamp)(normL * meterH, 0.0f, meterH);
    const float fillHR = (std::clamp)(normR * meterH, 0.0f, meterH);

    Color meterColL = (normL > 0.95f) ? Color(1.0f, 0.25f, 0.25f, 1.0f) :
                      (normL > 0.75f) ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.2f, 0.9f, 0.35f, 1.0f);
    Color meterColR = (normR > 0.95f) ? Color(1.0f, 0.25f, 0.25f, 1.0f) :
                      (normR > 0.75f) ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.2f, 0.9f, 0.35f, 1.0f);

    drawRect(r, meterX, faderTopY + meterH - fillHL, meterBarW, fillHL, meterColL);
    drawRect(r, meterX + meterBarW + 2.0f, faderTopY + meterH - fillHR, meterBarW, fillHR, meterColR);

    // Master dB Readout at bottom
    const std::string masterDb = presenter::audio_taper::formatDb(masterVol);
    drawCenteredText(r, masterDb, mb.x + 2.0f, mb.y + mb.h - 18.0f, mb.w - 4.0f, 14.0f, 9.0f,
                     Color(1.0f, 0.85f, 0.35f, 0.95f));

    // Vertical Divider Line after Master
    drawLine(r, mb.x + mb.w + 4.0f, mb.y, mb.x + mb.w + 4.0f, mb.y + mb.h,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.65f, 1.0f);

    // 6. TRACK CHANNEL STRIPS VIEWPORT
    const float totalChannelsW = static_cast<float>(tracks.size()) * (kChannelWidth + kChannelGap);
    maxScrollX_ = (std::max)(0.0f, totalChannelsW - channelsViewportBounds_.w);
    scrollX_ = (std::clamp)(scrollX_, 0.0f, maxScrollX_);

    r.pushScissor(channelsViewportBounds_.x, channelsViewportBounds_.y,
                  channelsViewportBounds_.w, channelsViewportBounds_.h);

    const float stripH = channelsViewportBounds_.h;

    for (size_t i = 0; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        const float stripX = channelsViewportBounds_.x + static_cast<float>(i) * (kChannelWidth + kChannelGap) - scrollX_;

        // Horizontal culling
        if (stripX + kChannelWidth < channelsViewportBounds_.x ||
            stripX > channelsViewportBounds_.x + channelsViewportBounds_.w) {
            continue;
        }

        const bool isSelected = (static_cast<uint32_t>(i) == activeTrackIndex);
        const Color trackCol(track.r, track.g, track.b, 1.0f);

        // Strip Background
        drawRoundedRect(r, stripX, channelsViewportBounds_.y, kChannelWidth, stripH, 4.0f,
                        theme.controlBackground.darken(0.04f), 0.92f);

        // Selection Border highlight
        if (isSelected) {
            drawRoundedRectOutline(r, stripX, channelsViewportBounds_.y, kChannelWidth, stripH, 4.0f,
                                   trackCol, 1.5f);
        } else {
            drawRoundedRectOutline(r, stripX, channelsViewportBounds_.y, kChannelWidth, stripH, 4.0f,
                                   theme.borderSubtle, 1.0f);
        }

        // Top Track Header Pill (tinted with track color)
        drawRoundedRect(r, stripX + 3.0f, channelsViewportBounds_.y + 3.0f, kChannelWidth - 6.0f, 18.0f, 3.0f,
                        trackCol.withAlpha(0.25f));
        drawRoundedRectOutline(r, stripX + 3.0f, channelsViewportBounds_.y + 3.0f, kChannelWidth - 6.0f, 18.0f, 3.0f,
                               trackCol.withAlpha(0.85f), 1.0f);

        std::string trackLabel = "#" + std::to_string(i + 1) + " " + track.name;
        if (trackLabel.length() > 10) {
            trackLabel = trackLabel.substr(0, 9) + "..";
        }
        drawCenteredText(r, trackLabel, stripX + 3.0f, channelsViewportBounds_.y + 3.0f,
                         kChannelWidth - 6.0f, 18.0f, 9.0f, trackCol);

        // Pan Readout & Indicator
        const float panY = channelsViewportBounds_.y + 24.0f;
        std::string panStr = "C";
        if (track.pan < -0.05f) {
            panStr = "L" + std::to_string(static_cast<int>(std::round(-track.pan * 100.0f)));
        } else if (track.pan > 0.05f) {
            panStr = "R" + std::to_string(static_cast<int>(std::round(track.pan * 100.0f)));
        }

        const Rect2D panPillBounds(stripX + 6.0f, panY, kChannelWidth - 12.0f, 14.0f);
        drawRoundedRect(r, panPillBounds.x, panPillBounds.y, panPillBounds.w, panPillBounds.h, 2.0f,
                        theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 0.85f);

        // Pan center detent line
        const float panMidX = panPillBounds.x + panPillBounds.w * 0.5f;
        drawLine(r, panMidX, panPillBounds.y + 2.0f, panMidX, panPillBounds.y + panPillBounds.h - 2.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.4f, 1.0f);

        // Pan needle indicator
        const float panNeedleX = panMidX + track.pan * (panPillBounds.w * 0.5f - 4.0f);
        drawLine(r, panNeedleX, panPillBounds.y + 1.0f, panNeedleX, panPillBounds.y + panPillBounds.h - 1.0f,
                 trackCol, 1.0f, 1.5f);

        drawCenteredText(r, panStr, panPillBounds.x, panPillBounds.y, panPillBounds.w, panPillBounds.h,
                         8.5f, isSelected ? theme.textPrimary : theme.textSecondary);

        // Mute / Solo Buttons Row
        const float btnY = panY + 17.0f;
        const float btnW = (kChannelWidth - 16.0f) * 0.5f;
        const Rect2D muteBtnBounds(stripX + 6.0f, btnY, btnW, 16.0f);
        const Rect2D soloBtnBounds(stripX + 10.0f + btnW, btnY, btnW, 16.0f);

        drawButton(r, muteBtnBounds, "M",
                   track.mute ? Color(0.85f, 0.2f, 0.2f, 0.95f) : theme.controlBackground,
                   track.mute ? Color(1.0f, 0.3f, 0.3f, 1.0f) : theme.borderSubtle,
                   track.mute ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted,
                   8.5f, 2.0f, 1.0f);

        drawButton(r, soloBtnBounds, "S",
                   track.solo ? Color(0.95f, 0.8f, 0.2f, 0.95f) : theme.controlBackground,
                   track.solo ? Color(1.0f, 0.9f, 0.3f, 1.0f) : theme.borderSubtle,
                   track.solo ? Color(0.1f, 0.1f, 0.1f, 1.0f) : theme.textMuted,
                   8.5f, 2.0f, 1.0f);

        // Channel Fader & Peak Meter
        const float chFaderTopY = btnY + 20.0f;
        const float chFaderBottomY = channelsViewportBounds_.y + stripH - 22.0f;
        const float chFaderH = (std::max)(20.0f, chFaderBottomY - chFaderTopY);
        const float chWellX = stripX + 16.0f;
        const float chWellW = 14.0f;

        auto geom = computeFaderGeometry(chWellX, chFaderTopY, chWellW, chFaderH, track.volume);

        // Fader Well
        drawRoundedRect(r, geom.well.x, geom.well.y, geom.well.w, geom.well.h, 2.0f,
                        0.05f, 0.05f, 0.06f, 0.95f);
        drawRoundedRectOutline(r, geom.well.x, geom.well.y, geom.well.w, geom.well.h, 2.0f,
                               theme.borderSubtle, 1.0f);

        // Groove
        drawLine(r, geom.well.x + geom.well.w * 0.5f, geom.well.y + 2.0f,
                 geom.well.x + geom.well.w * 0.5f, geom.well.y + geom.well.h - 2.0f,
                 0.0f, 0.0f, 0.0f, 0.9f, 1.5f);

        // Unity 0 dB tick mark at 0.75 travel
        const float chUnityY = geom.well.y + (1.0f - 0.75f) * geom.well.h;
        drawLine(r, geom.well.x - 3.0f, chUnityY, geom.well.x + geom.well.w + 3.0f, chUnityY,
                 trackCol.withAlpha(0.6f), 1.0f);

        // Fader Thumb
        drawRoundedRect(r, geom.thumb.x, geom.thumb.y, geom.thumb.w, geom.thumb.h, 3.0f,
                        0.85f, 0.87f, 0.90f, 0.98f);
        drawRoundedRectOutline(r, geom.thumb.x, geom.thumb.y, geom.thumb.w, geom.thumb.h, 3.0f,
                               0.3f, 0.3f, 0.35f, 1.0f, 1.0f);
        // Track-colored indicator needle
        drawLine(r, geom.thumb.x + 2.0f, geom.thumb.y + geom.thumb.h * 0.5f,
                 geom.thumb.x + geom.thumb.w - 2.0f, geom.thumb.y + geom.thumb.h * 0.5f,
                 trackCol, 1.0f, 1.5f);

        // Channel Stereo Peak Meter (L and R)
        const float chMeterX = chWellX + chWellW + 10.0f;
        const float chMeterBarW = 4.0f;
        const float chMeterH = chFaderH;

        drawRect(r, chMeterX, chFaderTopY, chMeterBarW, chMeterH, 0.06f, 0.06f, 0.07f, 0.9f);
        drawRect(r, chMeterX + chMeterBarW + 2.0f, chFaderTopY, chMeterBarW, chMeterH, 0.06f, 0.06f, 0.07f, 0.9f);

        float pL = 0.0f;
        float pR = 0.0f;
        if (chPeaksL && i < numPeaks) pL = chPeaksL[i];
        if (chPeaksR && i < numPeaks) pR = chPeaksR[i];
        if (track.mute) { pL = 0.0f; pR = 0.0f; }

        const float chFillL = (std::clamp)(pL * chMeterH, 0.0f, chMeterH);
        const float chFillR = (std::clamp)(pR * chMeterH, 0.0f, chMeterH);

        Color cL = (pL > 0.95f) ? Color(1.0f, 0.25f, 0.25f, 1.0f) :
                   (pL > 0.75f) ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.2f, 0.9f, 0.35f, 1.0f);
        Color cR = (pR > 0.95f) ? Color(1.0f, 0.25f, 0.25f, 1.0f) :
                   (pR > 0.75f) ? Color(1.0f, 0.85f, 0.2f, 1.0f) : Color(0.2f, 0.9f, 0.35f, 1.0f);

        drawRect(r, chMeterX, chFaderTopY + chMeterH - chFillL, chMeterBarW, chFillL, cL);
        drawRect(r, chMeterX + chMeterBarW + 2.0f, chFaderTopY + chMeterH - chFillR, chMeterBarW, chFillR, cR);

        // Bottom Formatted dB Readout
        const std::string chDb = presenter::audio_taper::formatDb(track.volume);
        drawCenteredText(r, chDb, stripX + 2.0f, channelsViewportBounds_.y + stripH - 18.0f,
                         kChannelWidth - 4.0f, 14.0f, 8.5f,
                         isSelected ? trackCol : theme.textSecondary);
    }

    r.popScissor();

    // 7. Horizontal Scrollbar (if content overflows viewport)
    if (maxScrollX_ > 0.0f && scrollBarBounds_.h > 2.0f) {
        drawRoundedRect(r, scrollBarBounds_.x, scrollBarBounds_.y, scrollBarBounds_.w, scrollBarBounds_.h, 3.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.7f);

        const float viewFrac = (std::clamp)(channelsViewportBounds_.w / totalChannelsW, 0.1f, 1.0f);
        const float thumbW = (std::max)(24.0f, scrollBarBounds_.w * viewFrac);
        const float scrollFrac = scrollX_ / maxScrollX_;
        const float thumbX = scrollBarBounds_.x + scrollFrac * (scrollBarBounds_.w - thumbW);

        scrollThumbBounds_ = Rect2D(thumbX, scrollBarBounds_.y, thumbW, scrollBarBounds_.h);
        drawRoundedRect(r, thumbX, scrollBarBounds_.y, thumbW, scrollBarBounds_.h, 3.0f,
                        theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b,
                        isDraggingScrollbar_ ? 0.95f : 0.6f);
    } else {
        scrollThumbBounds_ = Rect2D();
    }
}

bool ArrangerMixerDrawer::handlePointer(
    const PointerEvent& ev,
    std::vector<ArrangerTimelineTrack>& tracks,
    uint32_t& activeTrackIndex,
    float& masterVol, float& masterPan, bool& masterMute,
    [[maybe_unused]] const ViewContext& ctx) {

    (void)masterPan;

    // 1. Pull-Tab Click / Tap
    if (pullTabBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            toggle();
            return true;
        }
        return true;
    }

    // If collapsed, don't handle clicks inside the empty drawer area
    if (animProgress_ <= 0.001f) {
        return false;
    }

    // 2. Active Drags (Fader, Pan, Resize, Scrollbar)
    if (ev.action == PointerAction::Move) {
        if (isResizing_) {
            const float dy = resizeStartY_ - ev.y;
            height_ = (std::clamp)(initialResizeH_ + dy, kMinHeight, kMaxHeight);
            return true;
        }

        if (isDraggingScrollbar_ && maxScrollX_ > 0.0f) {
            const float dx = ev.x - scrollbarDragStartX_;
            const float availableTrack = scrollBarBounds_.w - scrollThumbBounds_.w;
            if (availableTrack > 1.0f) {
                const float fracDelta = dx / availableTrack;
                scrollX_ = (std::clamp)(scrollbarDragStartScrollX_ + fracDelta * maxScrollX_, 0.0f, maxScrollX_);
            }
            return true;
        }

        if (activeFaderIndex_ != -999) {
            const float dy = dragStartY_ - ev.y;
            // Shift trim modifier for fine 0.15 scale
            const float trimScale = ev.mods.shift ? 0.15f : 1.0f;
            const float faderTravelDelta = (dy / (height_ - 60.0f)) * trimScale;

            const float startTravel = presenter::audio_taper::gainToTravel(dragStartGain_);
            const float newTravel = (std::clamp)(startTravel + faderTravelDelta, 0.0f, 1.0f);
            const float newGain = presenter::audio_taper::travelToGain(newTravel);

            if (activeFaderIndex_ == -1) {
                // Master fader
                masterVol = newGain;
                if (onMasterVolumeChanged) onMasterVolumeChanged(masterVol);
            } else if (activeFaderIndex_ >= 0 && static_cast<size_t>(activeFaderIndex_) < tracks.size()) {
                tracks[activeFaderIndex_].volume = newGain;
                if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(activeFaderIndex_), newGain);
            }
            return true;
        }

        if (activePanIndex_ != -999) {
            const float dx = ev.x - dragStartX_;
            const float trimScale = ev.mods.shift ? 0.2f : 1.0f;
            const float panDelta = (dx / 60.0f) * trimScale;
            const float newPan = (std::clamp)(dragStartPan_ + panDelta, -1.0f, 1.0f);

            if (activePanIndex_ >= 0 && static_cast<size_t>(activePanIndex_) < tracks.size()) {
                tracks[activePanIndex_].pan = newPan;
                if (onPanChanged) onPanChanged(static_cast<uint32_t>(activePanIndex_), newPan);
            }
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        bool hadDrag = isDragging();
        isResizing_ = false;
        isDraggingScrollbar_ = false;
        activeFaderIndex_ = -999;
        activePanIndex_ = -999;
        if (hadDrag) return true;
    }

    // 3. Scroll Wheel: horizontal panning across channel strips
    if (ev.action == PointerAction::Scroll) {
        if (channelsViewportBounds_.contains(ev.x, ev.y) && maxScrollX_ > 0.0f) {
            const float delta = (std::abs(ev.scrollX) > 0.01f ? ev.scrollX : ev.scrollY);
            scrollX_ = (std::clamp)(scrollX_ - delta * 45.0f, 0.0f, maxScrollX_);
            return true;
        }
    }

    // 4. Close Button [v]
    if (closeBtnBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            setExpanded(false);
            return true;
        }
        return true;
    }

    // 5. Top Resize Handle Drag
    if (resizeHandleBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        isResizing_ = true;
        resizeStartY_ = ev.y;
        initialResizeH_ = height_;
        return true;
    }

    // 6. Horizontal Scrollbar Click & Drag
    if (scrollThumbBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        isDraggingScrollbar_ = true;
        scrollbarDragStartX_ = ev.x;
        scrollbarDragStartScrollX_ = scrollX_;
        return true;
    } else if (scrollBarBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        // Jump scroll towards clicked position
        const float clickFrac = (ev.x - scrollBarBounds_.x) / scrollBarBounds_.w;
        scrollX_ = (std::clamp)(clickFrac * maxScrollX_, 0.0f, maxScrollX_);
        isDraggingScrollbar_ = true;
        scrollbarDragStartX_ = ev.x;
        scrollbarDragStartScrollX_ = scrollX_;
        return true;
    }

    // 7. Master Channel Strip Hits
    if (masterStripBounds_.contains(ev.x, ev.y)) {
        const Rect2D masterMuteBtn(masterStripBounds_.x + (masterStripBounds_.w - 36.0f) * 0.5f,
                                   masterStripBounds_.y + 26.0f, 36.0f, 16.0f);
        if (masterMuteBtn.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
            masterMute = !masterMute;
            if (onMasterMuteToggled) onMasterMuteToggled(masterMute);
            return true;
        }

        // Master Fader hit
        const float faderTopY = masterStripBounds_.y + 48.0f;
        const float faderBottomY = masterStripBounds_.y + masterStripBounds_.h - 22.0f;
        const float faderH = (std::max)(20.0f, faderBottomY - faderTopY);
        const float wellX = masterStripBounds_.x + 14.0f;
        const float wellW = 14.0f;

        auto geom = computeFaderGeometry(wellX, faderTopY, wellW, faderH, masterVol);
        const Rect2D faderHitBox(geom.well.x - 6.0f, geom.well.y - 4.0f, geom.well.w + 18.0f, geom.well.h + 8.0f);

        if (faderHitBox.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFaderClickTime_).count();

            if (elapsedMs < 350 && lastFaderClickedIndex_ == -1) {
                // Double-click resets master fader to 0 dB unity (1.0f)
                masterVol = 1.0f;
                if (onMasterVolumeChanged) onMasterVolumeChanged(masterVol);
                lastFaderClickTime_ = {};
                lastFaderClickedIndex_ = -999;
                return true;
            }

            lastFaderClickTime_ = now;
            lastFaderClickedIndex_ = -1;

            activeFaderIndex_ = -1;
            dragStartY_ = ev.y;
            dragStartGain_ = masterVol;
            return true;
        }

        return true; // Consume other clicks on master strip
    }

    // 8. Track Channel Strips Hits
    if (channelsViewportBounds_.contains(ev.x, ev.y)) {
        for (size_t i = 0; i < tracks.size(); ++i) {
            const float stripX = channelsViewportBounds_.x + static_cast<float>(i) * (kChannelWidth + kChannelGap) - scrollX_;
            const Rect2D stripBounds(stripX, channelsViewportBounds_.y, kChannelWidth, channelsViewportBounds_.h);

            if (!stripBounds.contains(ev.x, ev.y)) continue;

            // Strip clicked: select track
            if (ev.action == PointerAction::Down) {
                activeTrackIndex = static_cast<uint32_t>(i);
                if (onTrackSelected) onTrackSelected(activeTrackIndex);
            }

            // Top Header Card hit
            const Rect2D headerPill(stripX + 3.0f, channelsViewportBounds_.y + 3.0f, kChannelWidth - 6.0f, 18.0f);
            if (headerPill.contains(ev.x, ev.y)) {
                return true;
            }

            // Pan control hit
            const float panY = channelsViewportBounds_.y + 24.0f;
            const Rect2D panPillBounds(stripX + 6.0f, panY, kChannelWidth - 12.0f, 14.0f);
            if (panPillBounds.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPanClickTime_).count();

                if (elapsedMs < 350 && lastPanClickedIndex_ == static_cast<int>(i)) {
                    // Double click resets pan to Center
                    tracks[i].pan = 0.0f;
                    if (onPanChanged) onPanChanged(static_cast<uint32_t>(i), 0.0f);
                    lastPanClickTime_ = {};
                    lastPanClickedIndex_ = -999;
                    return true;
                }

                lastPanClickTime_ = now;
                lastPanClickedIndex_ = static_cast<int>(i);

                activePanIndex_ = static_cast<int>(i);
                dragStartX_ = ev.x;
                dragStartPan_ = tracks[i].pan;
                return true;
            }

            // Mute / Solo buttons
            const float btnY = panY + 17.0f;
            const float btnW = (kChannelWidth - 16.0f) * 0.5f;
            const Rect2D muteBtnBounds(stripX + 6.0f, btnY, btnW, 16.0f);
            const Rect2D soloBtnBounds(stripX + 10.0f + btnW, btnY, btnW, 16.0f);

            if (muteBtnBounds.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
                tracks[i].mute = !tracks[i].mute;
                if (onMuteToggled) onMuteToggled(static_cast<uint32_t>(i), tracks[i].mute);
                return true;
            }

            if (soloBtnBounds.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
                tracks[i].solo = !tracks[i].solo;
                if (onSoloToggled) onSoloToggled(static_cast<uint32_t>(i), tracks[i].solo);
                return true;
            }

            // Fader hit
            const float chFaderTopY = btnY + 20.0f;
            const float chFaderBottomY = channelsViewportBounds_.y + channelsViewportBounds_.h - 22.0f;
            const float chFaderH = (std::max)(20.0f, chFaderBottomY - chFaderTopY);
            const float chWellX = stripX + 16.0f;
            const float chWellW = 14.0f;

            auto geom = computeFaderGeometry(chWellX, chFaderTopY, chWellW, chFaderH, tracks[i].volume);
            const Rect2D faderHitBox(geom.well.x - 6.0f, geom.well.y - 4.0f, geom.well.w + 18.0f, geom.well.h + 8.0f);

            if (faderHitBox.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFaderClickTime_).count();

                if (elapsedMs < 350 && lastFaderClickedIndex_ == static_cast<int>(i)) {
                    // Double-click resets track fader to 0 dB unity (1.0f)
                    tracks[i].volume = 1.0f;
                    if (onVolumeChanged) onVolumeChanged(static_cast<uint32_t>(i), 1.0f);
                    lastFaderClickTime_ = {};
                    lastFaderClickedIndex_ = -999;
                    return true;
                }

                lastFaderClickTime_ = now;
                lastFaderClickedIndex_ = static_cast<int>(i);

                activeFaderIndex_ = static_cast<int>(i);
                dragStartY_ = ev.y;
                dragStartGain_ = tracks[i].volume;
                return true;
            }

            return true; // Strip absorbed click
        }
    }

    // If pointer is inside drawer container, absorb click so it doesn't fall through
    if (drawerBounds_.contains(ev.x, ev.y)) {
        return true;
    }

    return false;
}

bool ArrangerMixerDrawer::handleKey(int key, int scancode, int action, int mods, [[maybe_unused]] const ViewContext& ctx) {
    (void)scancode;

    if (action != 1 && action != 2) return false; // Press or repeat only

    // Hotkey 'M' (without Ctrl or Alt) toggles the drawer
    if ((key == 'M' || key == 'm' || key == 77 || key == 109) && !(mods & 2) && !(mods & 4)) {
        toggle();
        return true;
    }

    // Escape collapses the drawer if it is open
    if (key == 256 && isExpanded_) {
        setExpanded(false);
        return true;
    }

    return false;
}

} // namespace eatsbits::ui
