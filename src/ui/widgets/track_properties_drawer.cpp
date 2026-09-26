#include "eatsbits/ui/widgets/track_properties_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <iomanip>
#include <sstream>

namespace eatsbits::ui {

static const float kQuickPalette[8][3] = {
    {0.0f, 0.90f, 1.0f},   // Neon Cyan
    {1.0f, 0.55f, 0.0f},   // Neon Amber
    {0.0f, 1.00f, 0.40f},  // Acid Green
    {1.0f, 0.00f, 0.48f},  // Hot Pink
    {0.74f, 0.00f, 1.0f},  // Electric Purple
    {1.0f, 0.20f, 0.20f},  // Crimson Red
    {1.0f, 0.85f, 0.0f},   // Gold Yellow
    {0.20f, 0.60f, 1.0f}   // Sky Blue
};

TrackPropertiesDrawer::TrackPropertiesDrawer() = default;

void TrackPropertiesDrawer::layout(const Rect2D& containerBounds, float browserOffset) {
    containerBounds_ = containerBounds;
    float drawerTotalW = isExpanded_ ? (kPullTabWidth + width_) : kPullTabWidth;
    float rightBoundary = containerBounds.x + containerBounds.w - drawerTotalW - browserOffset;

    pullTabBounds_ = Rect2D(rightBoundary, containerBounds.y, kPullTabWidth, containerBounds.h);
    drawerBounds_ = Rect2D(rightBoundary + kPullTabWidth, containerBounds.y, width_, containerBounds.h);
    closeButtonBounds_ = Rect2D(drawerBounds_.x + width_ - 28.0f, containerBounds.y + 7.0f, 22.0f, 22.0f);
}

void TrackPropertiesDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme,
                                   TrackPropertiesDrawerData& data, float mouseX, float mouseY) {
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
                       isExpanded_ ? theme.secondaryAccent : theme.primaryAccent);

    // Sideways rotated "PROPERTIES" label
    drawRotatedText(r, "PROPERTIES", pullTabBounds_.x + (pullTabBounds_.w * 0.5f), gripY + 8.0f, -90.0f, 9.5f,
                    isExpanded_ ? theme.secondaryAccent : theme.primaryAccent);

    // Track Color Dot at Top of Pull Tab
    float dotColR = data.isMasterSelected ? 1.0f : data.r;
    float dotColG = data.isMasterSelected ? 0.85f : data.g;
    float dotColB = data.isMasterSelected ? 0.0f : data.b;
    drawCircle(r, pullTabBounds_.x + 12.0f, pullTabBounds_.y + 18.0f, 4.0f, dotColR, dotColG, dotColB, 1.0f);
    drawCircleOutline(r, pullTabBounds_.x + 12.0f, pullTabBounds_.y + 18.0f, 5.5f, dotColR, dotColG, dotColB, 0.5f, 1.0f);

    if (!isExpanded_) return;

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

    drawCircle(r, drawerBounds_.x + 18.0f, drawerBounds_.y + 18.0f, 4.0f, dotColR, dotColG, dotColB, 1.0f);

    std::string titleText = data.isMasterSelected ? "MASTER BUS CONSOLE" : (data.tab == TrackPropertiesTab::Clip ? "CLIP PROPERTIES" : "TRACK PROPERTIES");
    drawText(r, titleText, drawerBounds_.x + 28.0f, drawerBounds_.y + 12.0f, 11.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Close Button [X]
    bool closeHov = closeButtonBounds_.contains(mouseX, mouseY);
    drawCircle(r, closeButtonBounds_.x + 11.0f, closeButtonBounds_.y + 11.0f, 9.0f,
               closeHov ? 0.35f : 0.18f, closeHov ? 0.20f : 0.20f, closeHov ? 0.22f : 0.25f, 1.0f);
    drawCircleOutline(r, closeButtonBounds_.x + 11.0f, closeButtonBounds_.y + 11.0f, 9.0f,
                      theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
    drawLine(r, closeButtonBounds_.x + 6.0f, closeButtonBounds_.y + 6.0f,
             closeButtonBounds_.x + 16.0f, closeButtonBounds_.y + 16.0f,
             closeHov ? 1.0f : 0.75f, closeHov ? 0.3f : 0.75f, closeHov ? 0.3f : 0.75f, 1.0f, 1.5f);
    drawLine(r, closeButtonBounds_.x + 16.0f, closeButtonBounds_.y + 6.0f,
             closeButtonBounds_.x + 6.0f, closeButtonBounds_.y + 16.0f,
             closeHov ? 1.0f : 0.75f, closeHov ? 0.3f : 0.75f, closeHov ? 0.3f : 0.75f, 1.0f, 1.5f);

    float contentX = drawerBounds_.x + 10.0f;
    float contentY = drawerBounds_.y + 44.0f;
    float contentW = drawerBounds_.w - 20.0f;

    if (data.isMasterSelected) {
        renderMasterSection(r, theme, data, contentX, contentY, contentW, mouseX, mouseY);
    } else if (data.tab == TrackPropertiesTab::Clip) {
        renderClipSection(r, theme, data, contentX, contentY, contentW, mouseX, mouseY);
    } else {
        renderTrackSection(r, theme, data, contentX, contentY, contentW, mouseX, mouseY);
    }
}

void TrackPropertiesDrawer::renderTrackSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float contentX, float contentY, float contentW,
                                              float mouseX, float mouseY) {
    float curY = contentY - scrollY_;

    // SECTION 1: Track Identity Header Card (52px high)
    drawRoundedRect(r, contentX, curY, contentW, 52.0f, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, 52.0f, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
    drawRoundedRect(r, contentX + 10.0f, curY + 10.0f, 5.0f, 32.0f, 2.5f, data.r, data.g, data.b, 1.0f);

    drawText(r, data.trackName, contentX + 24.0f, curY + 12.0f, 12.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "TRACK CHANNEL " + std::to_string(data.trackIndex + 1) + " • EATSCRIPT DSP",
             contentX + 24.0f, curY + 32.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    // Mute Button: [contentX + contentW - 110, curY + 12, 50, 30] (Preserves exact test coordinate compatibility)
    float btnMuteX = contentX + contentW - 110.0f;
    float btnMuteY = curY + 12.0f;
    Color muteBg = data.mute ? Color(0.85f, 0.22f, 0.15f, 0.95f) : theme.panelHeader;
    Color muteBorder = data.mute ? Color(1.0f, 0.3f, 0.3f, 0.8f) : theme.borderSubtle;
    Color muteText = data.mute ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textSecondary;
    drawButton(r, Rect2D(btnMuteX, btnMuteY, 50.0f, 30.0f), "MUTE", muteBg, muteBorder, muteText, 10.5f, 4.0f, 1.0f);

    // Solo Button: [contentX + contentW - 55, curY + 12, 50, 30]
    float btnSoloX = contentX + contentW - 55.0f;
    float btnSoloY = curY + 12.0f;
    Color soloBg = data.solo ? Color(0.95f, 0.75f, 0.10f, 0.95f) : theme.panelHeader;
    Color soloBorder = data.solo ? Color(1.0f, 0.8f, 0.2f, 0.8f) : theme.borderSubtle;
    Color soloText = data.solo ? Color(0.05f, 0.08f, 0.10f, 1.0f) : theme.textSecondary;
    drawButton(r, Rect2D(btnSoloX, btnSoloY, 50.0f, 30.0f), "SOLO", soloBg, soloBorder, soloText, 10.5f, 4.0f, 1.0f);

    curY += 60.0f;

    // SECTION 2: 3-Band Parametric Channel EQ Card (Eatsbeats Parity)
    float eqCardH = 138.0f;
    drawRoundedRect(r, contentX, curY, contentW, eqCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, eqCardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // EQ Header
    drawText(r, "CHANNEL EQ (3-BAND PARAMETRIC)", contentX + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Active / Bypass Toggle Button
    float eqTglW = 60.0f;
    float eqTglX = contentX + contentW - eqTglW - 10.0f;
    Color eqBg = data.eqEnabled ? (theme.primaryAccent * 0.3f) : Color(0.12f, 0.12f, 0.14f, 0.9f);
    Color eqBorder = data.eqEnabled ? theme.primaryAccent : theme.borderSubtle;
    Color eqText = data.eqEnabled ? theme.primaryAccent : theme.textMuted;
    drawButton(r, Rect2D(eqTglX, curY + 6.0f, eqTglW, 20.0f),
               data.eqEnabled ? "ACTIVE" : "BYPASS", eqBg, eqBorder, eqText, 8.5f, 3.0f, 1.0f);

    // EQ Knob Row 1: HPF CUT, LOW GAIN, HIGH GAIN
    float kw3 = contentW / 3.0f;
    auto drawEqKnob = [&](float kx, float ky, const std::string& label, const std::string& valStr, float normVal) {
        drawCircle(r, kx, ky, 13.0f, 0.14f, 0.16f, 0.20f, 1.0f);
        drawCircleOutline(r, kx, ky, 13.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
        float ang = -2.35f + normVal * 4.71f;
        drawLine(r, kx, ky, kx + std::cos(ang) * 10.0f, ky + std::sin(ang) * 10.0f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.8f);
        drawText(r, label, kx - static_cast<float>(label.length()) * 2.7f, ky + 16.0f, 7.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        drawText(r, valStr, kx - static_cast<float>(valStr.length()) * 2.5f, ky + 26.0f, 7.5f,
                 theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    };

    float eqRow1Y = curY + 45.0f;
    float normHpf = std::clamp((data.eqHpf - 20.0f) / 480.0f, 0.0f, 1.0f);
    drawEqKnob(contentX + kw3 * 0.5f, eqRow1Y, "HPF CUT", std::to_string(static_cast<int>(data.eqHpf)) + "Hz", normHpf);

    float normLow = std::clamp((data.eqLowGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string lowStr = (data.eqLowGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqLowGain)) + "dB";
    drawEqKnob(contentX + kw3 * 1.5f, eqRow1Y, "LOW GAIN", lowStr, normLow);

    float normHigh = std::clamp((data.eqHighGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string highStr = (data.eqHighGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqHighGain)) + "dB";
    drawEqKnob(contentX + kw3 * 2.5f, eqRow1Y, "HIGH GAIN", highStr, normHigh);

    // EQ Knob Row 2: MID FREQ, MID GAIN, MID Q
    float eqRow2Y = curY + 98.0f;
    float normMidF = std::clamp((data.eqMidFreq - 200.0f) / 7800.0f, 0.0f, 1.0f);
    std::string midFStr = data.eqMidFreq >= 1000.0f ? (std::to_string(static_cast<int>(data.eqMidFreq / 1000.0f)) + "kHz") : (std::to_string(static_cast<int>(data.eqMidFreq)) + "Hz");
    drawEqKnob(contentX + kw3 * 0.5f, eqRow2Y, "MID FREQ", midFStr, normMidF);

    float normMidG = std::clamp((data.eqMidGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string midGStr = (data.eqMidGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqMidGain)) + "dB";
    drawEqKnob(contentX + kw3 * 1.5f, eqRow2Y, "MID GAIN", midGStr, normMidG);

    float normMidQ = std::clamp((data.eqMidQ - 0.3f) / 9.7f, 0.0f, 1.0f);
    drawEqKnob(contentX + kw3 * 2.5f, eqRow2Y, "MID Q", "Q=" + std::to_string(static_cast<int>(data.eqMidQ)), normMidQ);

    curY += eqCardH + 8.0f;

    // SECTION 3: Instrument Card with 4 Authentic Rotary Knobs
    float instH = 110.0f;
    drawRoundedRect(r, contentX, curY, contentW, instH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, instH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawCircle(r, contentX + 14.0f, curY + 16.0f, 3.5f, data.r, data.g, data.b, 1.0f);
    drawText(r, data.instrument, contentX + 24.0f, curY + 10.0f, 11.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    float kw4 = (contentW - 20.0f) / 4.0f;
    for (int k = 0; k < 4; ++k) {
        float kcx = contentX + 10.0f + static_cast<float>(k) * kw4 + kw4 * 0.5f;
        float kcy = curY + 50.0f;
        float val = (k == 0) ? data.knob1 : ((k == 1) ? data.knob2 : ((k == 2) ? data.knob3 : data.knob4));
        const std::string& kname = (k == 0) ? data.knob1Name : ((k == 1) ? data.knob2Name : ((k == 2) ? data.knob3Name : data.knob4Name));

        drawCircle(r, kcx, kcy, 12.0f, 0.15f, 0.16f, 0.18f, 1.0f);
        drawCircle(r, kcx, kcy, 10.0f, 0.08f, 0.08f, 0.09f, 1.0f);
        float ang = -2.35f + val * 4.71f;
        drawLine(r, kcx, kcy, kcx + std::cos(ang) * 9.0f, kcy + std::sin(ang) * 9.0f,
                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f, 1.8f);

        drawText(r, kname, kcx - static_cast<float>(kname.length()) * 2.8f, kcy + 16.0f, 8.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    }

    // Change Instrument Button
    float chgBtnY = curY + instH - 26.0f;
    drawButton(r, Rect2D(contentX + 10.0f, chgBtnY, contentW - 20.0f, 20.0f),
               "<-> CHANGE INSTRUMENT", Color(0.10f, 0.10f, 0.12f, 0.9f), theme.borderSubtle, theme.textPrimary, 9.0f, 4.0f, 1.0f);

    curY += instH + 8.0f;

    // SECTION 4: MIDI FX RACK
    drawText(r, "MIDI FX RACK", contentX + 4.0f, curY + 4.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float addMidiBtnW = 85.0f;
    float addMidiBtnX = contentX + contentW - addMidiBtnW;
    drawButton(r, Rect2D(addMidiBtnX, curY, addMidiBtnW, 18.0f),
               "+ ADD MIDI FX", theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent, 8.5f, 3.0f, 1.0f);

    curY += 24.0f;
    for (size_t mi = 0; mi < data.midiFx.size() && mi < 2; ++mi) {
        drawRoundedRect(r, contentX, curY, contentW, 30.0f, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, contentX, curY, contentW, 30.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);
        drawCircle(r, contentX + 12.0f, curY + 15.0f, 3.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
        drawText(r, data.midiFx[mi].name, contentX + 22.0f, curY + 9.0f, 10.0f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
        drawText(r, "[X]", contentX + contentW - 20.0f, curY + 9.0f, 8.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);
        curY += 34.0f;
    }

    curY += 6.0f;

    // SECTION 5: AUDIO FX RACK
    drawText(r, "AUDIO FX RACK (" + std::to_string(data.audioFx.size()) + ")", contentX + 4.0f, curY + 4.0f, 10.5f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float addFxBtnW = 75.0f;
    float addFxBtnX = contentX + contentW - addFxBtnW;
    drawButton(r, Rect2D(addFxBtnX, curY, addFxBtnW, 18.0f),
               "+ ADD FX", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);

    curY += 24.0f;
    for (size_t fi = 0; fi < data.audioFx.size() && fi < 2; ++fi) {
        drawRoundedRect(r, contentX, curY, contentW, 30.0f, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, contentX, curY, contentW, 30.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);
        drawCircle(r, contentX + 12.0f, curY + 15.0f, 3.0f, theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);
        drawText(r, data.audioFx[fi].name, contentX + 22.0f, curY + 9.0f, 10.0f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
        drawText(r, "[X]", contentX + contentW - 20.0f, curY + 9.0f, 8.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.8f);
        curY += 34.0f;
    }

    curY += 6.0f;

    // SECTION 6: Track Color Palette (8 Quick Colors)
    drawText(r, "TRACK COLOR", contentX + 4.0f, curY, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    float swatchSpacing = contentW / 8.0f;
    for (int p = 0; p < 8; ++p) {
        float scx = contentX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
        float scy = curY + 20.0f;
        float cr = kQuickPalette[p][0];
        float cg = kQuickPalette[p][1];
        float cb = kQuickPalette[p][2];
        bool isCurCol = (std::abs(data.r - cr) < 0.05f && std::abs(data.g - cg) < 0.05f && std::abs(data.b - cb) < 0.05f);

        drawCircle(r, scx, scy, 10.0f, cr, cg, cb, 1.0f);
        if (isCurCol) {
            drawCircleOutline(r, scx, scy, 13.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.8f);
        }
    }

    curY += 38.0f;

    // SECTION 7: Reorder Buttons (< and > for Left / Right Move in Mixer)
    if (data.isMixerMode) {
        float reorderW = (contentW - 12.0f) * 0.5f;
        drawButton(r, Rect2D(contentX, curY, reorderW, 26.0f),
                   "< MOVE TRACK LEFT", theme.controlBackground, theme.borderSubtle, theme.textSecondary, 9.0f, 4.0f, 1.0f);
        drawButton(r, Rect2D(contentX + reorderW + 12.0f, curY, reorderW, 26.0f),
                   "MOVE TRACK RIGHT >", theme.controlBackground, theme.borderSubtle, theme.textSecondary, 9.0f, 4.0f, 1.0f);
    }
}

void TrackPropertiesDrawer::renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                               TrackPropertiesDrawerData& data, float contentX, float contentY, float contentW,
                                               float mouseX, float mouseY) {
    float curY = contentY - scrollY_;

    // SECTION 1: MASTER PRECISION 4-BAND EQ
    float eqCardH = 105.0f;
    drawRoundedRect(r, contentX, curY, contentW, eqCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, eqCardH, 6.0f,
                           1.0f, 0.85f, 0.0f, 0.8f, 1.2f);

    drawText(r, "MASTER 4-BAND PRECISION EQ", contentX + 12.0f, curY + 8.0f, 10.5f,
             1.0f, 0.88f, 0.20f, 1.0f);

    float kw4 = contentW / 4.0f;
    auto drawMasterKnob = [&](float kx, float ky, const std::string& label, const std::string& valStr, float normVal) {
        drawCircle(r, kx, ky, 13.0f, 0.16f, 0.18f, 0.22f, 1.0f);
        drawCircleOutline(r, kx, ky, 13.0f, 1.0f, 0.85f, 0.0f, 0.6f, 1.0f);
        float ang = -2.35f + normVal * 4.71f;
        drawLine(r, kx, ky, kx + std::cos(ang) * 10.0f, ky + std::sin(ang) * 10.0f,
                 1.0f, 0.88f, 0.20f, 1.0f, 1.8f);
        drawText(r, label, kx - static_cast<float>(label.length()) * 2.7f, ky + 16.0f, 7.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        drawText(r, valStr, kx - static_cast<float>(valStr.length()) * 2.5f, ky + 26.0f, 7.5f,
                 1.0f, 0.88f, 0.20f, 1.0f);
    };

    float mEqY = curY + 44.0f;
    float normSub = std::clamp((data.masterSubCut - 20.0f) / 25.0f, 0.0f, 1.0f);
    drawMasterKnob(contentX + kw4 * 0.5f, mEqY, "SUB CUT", std::to_string(static_cast<int>(data.masterSubCut)) + "Hz", normSub);

    float normLow = std::clamp((data.masterLowGain + 12.0f) / 24.0f, 0.0f, 1.0f);
    std::string lowStr = (data.masterLowGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.masterLowGain)) + "dB";
    drawMasterKnob(contentX + kw4 * 1.5f, mEqY, "LOW GAIN", lowStr, normLow);

    float normMid = std::clamp((data.masterMidGain + 12.0f) / 24.0f, 0.0f, 1.0f);
    std::string midStr = (data.masterMidGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.masterMidGain)) + "dB";
    drawMasterKnob(contentX + kw4 * 2.5f, mEqY, "MID GAIN", midStr, normMid);

    float normHigh = std::clamp((data.masterHighGain + 12.0f) / 24.0f, 0.0f, 1.0f);
    std::string highStr = (data.masterHighGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.masterHighGain)) + "dB";
    drawMasterKnob(contentX + kw4 * 3.5f, mEqY, "HIGH GAIN", highStr, normHigh);

    curY += eqCardH + 10.0f;

    // SECTION 2: TRUE PEAK BRICKWALL LIMITER
    float limCardH = 105.0f;
    drawRoundedRect(r, contentX, curY, contentW, limCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, limCardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "TRUE PEAK BRICKWALL LIMITER", contentX + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float limTglW = 46.0f;
    float limTglX = contentX + contentW - limTglW - 10.0f;
    Color limBg = data.masterLimiterEnabled ? (theme.primaryAccent * 0.3f) : Color(0.12f, 0.12f, 0.14f, 0.9f);
    Color limBorder = data.masterLimiterEnabled ? theme.primaryAccent : theme.borderSubtle;
    Color limText = data.masterLimiterEnabled ? theme.primaryAccent : theme.textMuted;
    drawButton(r, Rect2D(limTglX, curY + 6.0f, limTglW, 18.0f),
               data.masterLimiterEnabled ? "ON" : "OFF", limBg, limBorder, limText, 8.5f, 3.0f, 1.0f);

    float kw3 = contentW / 3.0f;
    float limRowY = curY + 44.0f;
    float normCeil = std::clamp((data.masterCeilingDbfs + 2.0f) / 2.0f, 0.0f, 1.0f);
    drawMasterKnob(contentX + kw3 * 0.5f, limRowY, "CEILING", std::to_string(data.masterCeilingDbfs).substr(0, 4) + "dB", normCeil);

    float normDrive = std::clamp(data.masterLimiterDrive / 12.0f, 0.0f, 1.0f);
    drawMasterKnob(contentX + kw3 * 1.5f, limRowY, "DRIVE BOOST", "+" + std::to_string(static_cast<int>(data.masterLimiterDrive)) + "dB", normDrive);

    float normLufs = std::clamp((data.masterTargetLufs + 24.0f) / 18.0f, 0.0f, 1.0f);
    drawMasterKnob(contentX + kw3 * 2.5f, limRowY, "TARGET LUFS", std::to_string(static_cast<int>(data.masterTargetLufs)) + " LUFS", normLufs);

    curY += limCardH + 10.0f;

    // SECTION 3: MASTER FX INSERTS (Convolver Reverb & Dynamic Limiter)
    drawText(r, "MASTER BUS FX INSERTS", contentX + 4.0f, curY + 4.0f, 10.5f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    curY += 24.0f;
    for (size_t fi = 0; fi < data.masterAudioFx.size(); ++fi) {
        drawRoundedRect(r, contentX, curY, contentW, 32.0f, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, contentX, curY, contentW, 32.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);
        drawCircle(r, contentX + 12.0f, curY + 16.0f, 3.5f, 1.0f, 0.85f, 0.0f, 1.0f);
        drawText(r, data.masterAudioFx[fi].name, contentX + 24.0f, curY + 10.0f, 10.5f,
                 theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
        drawText(r, "ACTIVE", contentX + contentW - 48.0f, curY + 10.0f, 8.5f,
                 0.18f, 0.95f, 0.45f, 1.0f);
        curY += 36.0f;
    }

    curY += 10.0f;

    // SECTION 4: GEMINI AI AUTO-MIX & MASTER
    drawButton(r, Rect2D(contentX, curY, contentW, 34.0f),
               "✨ GEMINI AI AUTO-MIX & MASTER",
               theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent,
               10.5f, 5.0f, 1.2f);
}

void TrackPropertiesDrawer::renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float contentX, float contentY, float contentW,
                                             float mouseX, float mouseY) {
    float curY = contentY - scrollY_;

    // Clip Title Card
    drawRoundedRect(r, contentX, curY, contentW, 40.0f, 5.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, 40.0f, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
    drawText(r, data.clipName, contentX + 14.0f, curY + 12.0f, 13.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    curY += 48.0f;

    // Timing & Loop Parameters Card
    float infoH = 120.0f;
    drawRoundedRect(r, contentX, curY, contentW, infoH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, contentX, curY, contentW, infoH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "Track: " + data.trackName, contentX + 14.0f, curY + 12.0f, 10.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    drawText(r, "Start Bar: " + std::to_string(data.clipStartBar), contentX + 14.0f, curY + 30.0f, 10.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "Length: " + std::to_string(data.clipLengthBars) + " Bars", contentX + 14.0f, curY + 48.0f, 10.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    float loopTglY = curY + 70.0f;
    std::string loopStr = data.clipLooped ? ("LOOP MODE: ON (" + std::to_string(data.clipLoopLengthBars) + "b)") : "LOOP MODE: OFF";
    Color loopBg = data.clipLooped ? (theme.primaryAccent * 0.3f) : Color(0.12f, 0.12f, 0.14f, 0.9f);
    Color loopBorder = data.clipLooped ? theme.primaryAccent : theme.borderSubtle;
    Color loopText = data.clipLooped ? theme.primaryAccent : theme.textMuted;
    drawButton(r, Rect2D(contentX + 14.0f, loopTglY, contentW - 28.0f, 24.0f),
               loopStr, loopBg, loopBorder, loopText, 9.5f, 4.0f, 1.0f);

    curY += infoH + 14.0f;

    // Edit in Piano Roll Action Button
    drawButton(r, Rect2D(contentX, curY, contentW, 34.0f),
               "[EDIT IN PIANO ROLL]",
               theme.secondaryAccent * 0.3f, theme.secondaryAccent, theme.secondaryAccent,
               11.0f, 5.0f, 1.2f);
}

TrackPropertiesHitResult TrackPropertiesDrawer::hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    TrackPropertiesHitResult res{};

    // Pull Tab Hit
    if (pullTabBounds_.contains(mx, my)) {
        res.hit = true;
        res.area = TrackPropertiesHitArea::PullTab;
        return res;
    }

    if (!isExpanded_ || !drawerBounds_.contains(mx, my)) {
        return res;
    }

    res.hit = true;

    // Close Button Hit
    if (closeButtonBounds_.contains(mx, my)) {
        res.area = TrackPropertiesHitArea::CloseButton;
        return res;
    }

    float contentX = drawerBounds_.x + 10.0f;
    float contentY = drawerBounds_.y + 44.0f;
    float contentW = drawerBounds_.w - 20.0f;
    float curY = contentY - scrollY_;

    // Compact Mute button test coordinate parity: [contentX + contentW - 110, curY + 12, 50, 30]
    float btnMuteX = contentX + contentW - 110.0f;
    float btnMuteY = curY + 12.0f;
    if (mx >= btnMuteX && mx <= btnMuteX + 50.0f && my >= btnMuteY && my <= btnMuteY + 30.0f) {
        res.area = TrackPropertiesHitArea::MuteButton;
        return res;
    }

    // Solo button: [contentX + contentW - 55, curY + 12, 50, 30]
    float btnSoloX = contentX + contentW - 55.0f;
    float btnSoloY = curY + 12.0f;
    if (mx >= btnSoloX && mx <= btnSoloX + 50.0f && my >= btnSoloY && my <= btnSoloY + 30.0f) {
        res.area = TrackPropertiesHitArea::SoloButton;
        return res;
    }

    return res;
}

bool TrackPropertiesDrawer::handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data, const ViewContext& ctx) {
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

    if (isExpanded_ && drawerBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            auto hit = hitTest(ev.x, ev.y, data);
            if (hit.hit) {
                if (hit.area == TrackPropertiesHitArea::CloseButton) {
                    setExpanded(false);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::MuteButton) {
                    data.mute = !data.mute;
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::SoloButton) {
                    data.solo = !data.solo;
                    return true;
                }
            }
        } else if (ev.action == PointerAction::Scroll) {
            scrollY_ = std::max(0.0f, scrollY_ - ev.scrollY * 24.0f);
            return true;
        }
    }

    return false;
}

} // namespace eatsbits::ui
