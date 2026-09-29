#include "eatsbits/ui/widgets/track_properties_panel.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

static const float kQuickPalette[8][3] = {
    {0.13f, 0.96f, 0.91f}, // Neon Cyan
    {1.0f,  0.55f, 0.0f }, // Neon Amber
    {0.0f,  1.0f,  0.40f}, // Acid Green
    {1.0f,  0.0f,  0.48f}, // Hot Pink
    {0.74f, 0.0f,  1.0f }, // Electric Purple
    {1.0f,  0.20f, 0.20f}, // Crimson Red
    {1.0f,  0.90f, 0.0f }, // Gold Yellow
    {0.20f, 0.60f, 1.0f }  // Sky Blue
};

TrackPropertiesPanel::TrackPropertiesPanel() {
    // Configure embedded PluginSearchDialog
    pluginDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (mode == PluginDialogMode::AddInstrument) {
            if (onChangeInstrument) onChangeInstrument(trackIndex);
        } else if (mode == PluginDialogMode::AddMidiFx) {
            if (onAddMidiFx) onAddMidiFx(trackIndex);
        } else if (mode == PluginDialogMode::AddAudioFx) {
            if (onAddAudioFx) onAddAudioFx(trackIndex);
        }
    };
}

void TrackPropertiesPanel::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
    float contentW = bounds_.w - (padding * 2.0f);
    float startY = bounds_.y + 4.0f;
    float curY = startY - scrollY_;

    bool isWide = (bounds_.w >= 560.0f);

    // 0. Optional Top Track Ribbon
    if (showTrackRibbon_) {
        float ribbonH = 34.0f;
        ribbonBounds_ = Rect2D(bounds_.x + padding, curY, contentW, ribbonH);
        curY += ribbonH + 8.0f;
    } else {
        ribbonBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 1. Track Identity Header Card (reclaimed vertical space by removing subtitle)
    float headerH = isWide ? 42.0f : 38.0f;
    headerCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, headerH);
    curY += headerH + 6.0f;

    // 2. Track Color Swatches Card (directly below Track Title)
    float colorH = 36.0f;
    colorCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, colorH);
    curY += colorH + 8.0f;

    // 3. Channel Mixer Quick Controls Card (Vol slider + Pan knob)
    float mixerH = isWide ? 68.0f : 60.0f;
    mixerCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, mixerH);
    curY += mixerH + 8.0f;

    // 4. 3-Band Parametric EQ Card (available when mixer mode is active)
    float eqH = 138.0f;
    eqCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, eqH);
    // Note: only advances curY during render if eq is active

    // 5. Dynamic Instrument Hardware Faceplate Card
    float faceplateH = isWide ? 265.0f : 215.0f;
    faceplateBounds_ = Rect2D(bounds_.x + padding, curY, contentW, faceplateH);
    curY += faceplateH + 8.0f;

    // 6. Harmonic Chord Track Follow Settings Card
    float chordFollowH = 88.0f;
    chordFollowBounds_ = Rect2D(bounds_.x + padding, curY, contentW, chordFollowH);
    curY += chordFollowH + 8.0f;

    // 7. MIDI FX Pipeline Rack Card
    float midiFxH = isWide ? 120.0f : 106.0f;
    midiFxBounds_ = Rect2D(bounds_.x + padding, curY, contentW, midiFxH);
    curY += midiFxH + 8.0f;

    // 8. Audio FX Insert Rack Card
    float audioFxH = isWide ? 120.0f : 106.0f;
    audioFxBounds_ = Rect2D(bounds_.x + padding, curY, contentW, audioFxH);
    curY += audioFxH + 8.0f;

    colorPaletteBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);

    curY += 12.0f;
    totalContentHeight_ = (curY + scrollY_) - startY;
    needScrollbar_ = (totalContentHeight_ > bounds_.h);
    scrollbarBounds_ = Rect2D(bounds_.x + bounds_.w - 7.0f, bounds_.y + 2.0f, 5.0f, bounds_.h - 4.0f);

    // Layout embedded plugin search dialog
    pluginDialog_.layout(ctx.logicalWidth > 0.0f ? ctx.logicalWidth : 1280.0f,
                         ctx.logicalHeight > 0.0f ? ctx.logicalHeight : 800.0f);
}

void TrackPropertiesPanel::render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                                  float mouseX, float mouseY) {
    data.syncKnobsIfEmpty();

    float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
    float contentX = bounds_.x + padding;
    float contentW = bounds_.w - (padding * 2.0f);
    bool isWide = (bounds_.w >= 560.0f);

    // Master Console Mode (Mixer Master selected)
    if (data.isMasterSelected || data.tab == TrackPropertiesTab::Master) {
        renderMasterSection(r, theme, data, contentX, bounds_.y + 8.0f - scrollY_, contentW, mouseX, mouseY);
        if (needScrollbar_) renderScrollbar(r, theme);
        return;
    }

    // Clip Mode (Arranger Clip selected)
    if (data.tab == TrackPropertiesTab::Clip) {
        renderClipSection(r, theme, data, contentX, bounds_.y + 8.0f - scrollY_, contentW, mouseX, mouseY);
        if (needScrollbar_) renderScrollbar(r, theme);
        return;
    }

    // Top Track Navigation Ribbon
    if (showTrackRibbon_) {
        renderTrackSelectorRibbon(r, theme, data);
    }

    // Render Cards in order: Header -> Color Palette -> Mixer Controls -> ...
    renderHeaderCard(r, theme, data, headerCardBounds_.x, headerCardBounds_.y, headerCardBounds_.w, isWide, mouseX, mouseY);
    renderColorPalette(r, theme, data, colorCardBounds_.x, colorCardBounds_.y, colorCardBounds_.w);
    renderMixerControlsCard(r, theme, data, mixerCardBounds_.x, mixerCardBounds_.y, mixerCardBounds_.w, isWide);

    if (data.isMixerMode) {
        renderEqCard(r, theme, data, eqCardBounds_.x, eqCardBounds_.y, eqCardBounds_.w);
    }

    renderFaceplateCard(r, theme, data, faceplateBounds_.x, faceplateBounds_.y, faceplateBounds_.w, isWide);
    renderChordFollowCard(r, theme, data, chordFollowBounds_.x, chordFollowBounds_.y, chordFollowBounds_.w);
    renderMidiFxCard(r, theme, data, midiFxBounds_.x, midiFxBounds_.y, midiFxBounds_.w, isWide);
    renderAudioFxCard(r, theme, data, audioFxBounds_.x, audioFxBounds_.y, audioFxBounds_.w, isWide);

    if (needScrollbar_) {
        renderScrollbar(r, theme);
    }
}

void TrackPropertiesPanel::renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme,
                                                     const TrackPropertiesDrawerData& data) {
    if (ribbonBounds_.y + ribbonBounds_.h < bounds_.y || ribbonBounds_.y > bounds_.y + bounds_.h) return;

    size_t trackCount = data.allTrackNames.empty() ? 5 : data.allTrackNames.size();
    const float startX = ribbonBounds_.x + 4.0f;
    const float tabW = std::clamp((ribbonBounds_.w - 40.0f) / static_cast<float>(trackCount), 90.0f, 130.0f);
    const float tabGap = 8.0f;
    const float tabH = ribbonBounds_.h - 4.0f;

    for (size_t i = 0; i < trackCount; ++i) {
        float tx = startX + static_cast<float>(i) * (tabW + tabGap);
        if (tx + tabW > ribbonBounds_.x + ribbonBounds_.w) break;

        bool isActive = (i == data.trackIndex);
        std::string tName = (i < data.allTrackNames.size()) ? data.allTrackNames[i] : ("Track " + std::to_string(i + 1));
        Color tCol = (i < data.allTrackColors.size()) ? data.allTrackColors[i] : Color(data.r, data.g, data.b);

        drawRoundedRect(r, tx, ribbonBounds_.y + 2.0f, tabW, tabH, 4.0f,
                        isActive ? (theme.primaryAccent * 0.28f) : theme.controlBackground,
                        isActive ? 0.95f : 0.70f);
        drawRoundedRectOutline(r, tx, ribbonBounds_.y + 2.0f, tabW, tabH, 4.0f,
                               isActive ? theme.primaryAccent : theme.borderSubtle,
                               isActive ? 0.95f : 0.40f, isActive ? 1.5f : 1.0f);

        // Accent indicator dot
        drawCircle(r, tx + 10.0f, ribbonBounds_.y + 2.0f + tabH * 0.5f, 3.5f,
                   tCol.r, tCol.g, tCol.b, isActive ? 1.0f : 0.6f);

        drawText(r, tName, tx + 20.0f, ribbonBounds_.y + 11.0f, 10.5f,
                 isActive ? theme.primaryAccent.r : theme.textPrimary.r,
                 isActive ? theme.primaryAccent.g : theme.textPrimary.g,
                 isActive ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);
    }
}

void TrackPropertiesPanel::renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                            TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide,
                                            float mouseX, float mouseY) {
    float h = isWide ? 42.0f : 38.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Accent Pill
    drawRoundedRect(r, cx + 8.0f, cy + 7.0f, 4.0f, h - 14.0f, 2.0f, data.r, data.g, data.b, 1.0f);

    // Right-aligned button cluster: [ Edit (pencil icon) ] [ M ] [ S ] (and [F], [CODE] if wide)
    float btnW = 24.0f;
    float btnH = 24.0f;
    float btnY = cy + (h - btnH) * 0.5f;

    float fzX = 0.0f;
    float soloX = 0.0f;
    float muteX = 0.0f;
    float editBtnX = 0.0f;
    float codeX = 0.0f;
    float codeW = 54.0f;

    if (isWide) {
        fzX = cx + cw - 8.0f - btnW;
        soloX = fzX - 5.0f - btnW;
        muteX = soloX - 5.0f - btnW;
        editBtnX = muteX - 5.0f - btnW;
        codeX = editBtnX - 8.0f - codeW;

        // Freeze button [F]
        Color fzBg = data.freeze ? Color(0.12f, 0.75f, 0.85f, 0.95f) : Color(0.12f, 0.14f, 0.18f, 0.85f);
        Color fzBorder = data.freeze ? Color(0.2f, 0.9f, 1.0f, 0.8f) : theme.borderSubtle;
        Color fzText = data.freeze ? Color(0.05f, 0.08f, 0.10f, 1.0f) : theme.textSecondary;
        drawButton(r, Rect2D(fzX, btnY, btnW, btnH), "F", fzBg, fzBorder, fzText, 10.0f, 4.0f, 1.0f);

        // [ CODE ] button
        drawButton(r, Rect2D(codeX, btnY, codeW, btnH), "[ CODE ]",
                   Color(0.14f, 0.16f, 0.22f, 1.0f), theme.borderSubtle, theme.primaryAccent, 8.5f, 4.0f, 1.0f);
    } else {
        soloX = cx + cw - 8.0f - btnW;
        muteX = soloX - 5.0f - btnW;
        editBtnX = muteX - 5.0f - btnW;
    }

    // Solo button [S]
    Color soloBg = data.solo ? Color(0.95f, 0.75f, 0.10f, 0.95f) : Color(0.12f, 0.14f, 0.18f, 0.85f);
    Color soloBorder = data.solo ? Color(1.0f, 0.85f, 0.2f, 0.9f) : theme.borderSubtle;
    Color soloText = data.solo ? Color(0.05f, 0.08f, 0.10f, 1.0f) : theme.textSecondary;
    drawButton(r, Rect2D(soloX, btnY, btnW, btnH), "S", soloBg, soloBorder, soloText, 10.0f, 4.0f, 1.0f);

    // Mute button [M]
    Color muteBg = data.mute ? Color(0.85f, 0.22f, 0.15f, 0.95f) : Color(0.12f, 0.14f, 0.18f, 0.85f);
    Color muteBorder = data.mute ? Color(1.0f, 0.35f, 0.3f, 0.9f) : theme.borderSubtle;
    Color muteText = data.mute ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textSecondary;
    drawButton(r, Rect2D(muteX, btnY, btnW, btnH), "M", muteBg, muteBorder, muteText, 10.0f, 4.0f, 1.0f);

    // Edit button [Edit (pencil icon)]
    bool editHov = (mouseX >= editBtnX && mouseX <= editBtnX + btnW &&
                    mouseY >= btnY && mouseY <= btnY + btnH);
    Color editBg = editHov ? Color(theme.primaryAccent.r * 0.25f, theme.primaryAccent.g * 0.25f, theme.primaryAccent.b * 0.25f, 0.95f)
                           : Color(theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.6f);
    Color editBdr = editHov ? theme.primaryAccent : Color(theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f);
    drawRoundedRect(r, editBtnX, btnY, btnW, btnH, 4.0f, editBg.r, editBg.g, editBg.b, editBg.a);
    drawRoundedRectOutline(r, editBtnX, btnY, btnW, btnH, 4.0f, editBdr.r, editBdr.g, editBdr.b, editBdr.a, editHov ? 1.3f : 1.0f);
    Color editColor = editHov ? theme.primaryAccent : theme.textSecondary;
    drawIconEdit(r, editBtnX + btnW * 0.5f, btnY + btnH * 0.5f, 11.0f, editColor);

    // Reclaimed full horizontal space for Track Name (vertically centered on single line)
    float titleX = cx + 18.0f;
    float titleY = cy + (h - 13.0f) * 0.5f;
    drawText(r, data.trackName, titleX, titleY, isWide ? 13.5f : 12.5f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
}

void TrackPropertiesPanel::renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                   TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float h = isWide ? 68.0f : 60.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    float sY = cy + (isWide ? 25.0f : 21.0f);
    float sH = isWide ? 18.0f : 16.0f;

    // 1. Expanded Volume Slider (takes majority of width)
    float vLabelX = cx + 14.0f;
    float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
    float panAreaW = isWide ? 85.0f : 74.0f;
    float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
    if (vTrackW < 50.0f) vTrackW = 50.0f;

    drawText(r, "VOL", vLabelX, sY + 3.0f, 10.5f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    drawRoundedRect(r, vTrackX, sY, vTrackW, sH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
    drawRoundedRectOutline(r, vTrackX, sY, vTrackW, sH, 3.0f, 0.20f, 0.22f, 0.28f, 1.0f, 1.0f);

    float normVol = std::clamp(data.volume / 1.5f, 0.0f, 1.0f);
    if (normVol > 0.01f) {
        drawRoundedRect(r, vTrackX + 2.0f, sY + 2.0f, (vTrackW - 4.0f) * normVol, sH - 4.0f, 2.5f,
                        data.r * 0.85f, data.g * 0.85f, data.b * 0.85f, 0.95f);
    }
    float volThumbX = vTrackX + normVol * vTrackW;
    drawRoundedRect(r, volThumbX - 4.0f, sY - 2.0f, 8.0f, sH + 4.0f, 2.0f, 0.80f, 0.82f, 0.88f, 1.0f);
    int volPct = static_cast<int>(std::round(data.volume * 100.0f));
    drawText(r, std::to_string(volPct) + "%", vTrackX + vTrackW + 8.0f, sY + 3.0f, 10.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // 2. Rotary Pan Knob (right side)
    float panKcx = cx + cw - (isWide ? 40.0f : 34.0f);
    float panKcy = cy + (h * 0.5f) - (isWide ? 2.0f : 1.0f);
    float kPanRadius = isWide ? 13.5f : 12.0f;
    bool isDraggingPan = (dragMode_ == DragMode::PanKnob);

    drawCircle(r, panKcx, panKcy + 2.0f, kPanRadius + 1.0f, 0.05f, 0.06f, 0.08f, 0.35f);
    drawCircle(r, panKcx, panKcy, kPanRadius, 0.16f, 0.18f, 0.22f, 1.0f);
    drawCircleOutline(r, panKcx, panKcy, kPanRadius,
                      isDraggingPan ? theme.highlight.r : theme.borderSubtle.r,
                      isDraggingPan ? theme.highlight.g : theme.borderSubtle.g,
                      isDraggingPan ? theme.highlight.b : theme.borderSubtle.b,
                      isDraggingPan ? 1.0f : 0.7f, 1.2f);

    // Center Detent Tick Mark at 12 o'clock
    drawLine(r, panKcx, panKcy - kPanRadius - 2.0f, panKcx, panKcy - kPanRadius + 2.0f,
             0.50f, 0.55f, 0.65f, 0.8f, 1.0f);

    // Rotary Needle
    constexpr float kMaxPanAngle = 2.35619449f;  // +135 deg
    float curPanAngle = data.pan * kMaxPanAngle; // 0.0 = straight UP (12 o'clock)
    float needleX = panKcx + std::sin(curPanAngle) * (kPanRadius - 3.0f);
    float needleY = panKcy - std::cos(curPanAngle) * (kPanRadius - 3.0f);
    drawLine(r, panKcx, panKcy, needleX, needleY,
             isDraggingPan ? theme.highlight.r : (std::abs(data.pan) < 0.04f ? 0.70f : data.r),
             isDraggingPan ? theme.highlight.g : (std::abs(data.pan) < 0.04f ? 0.75f : data.g),
             isDraggingPan ? theme.highlight.b : (std::abs(data.pan) < 0.04f ? 0.85f : data.b),
             1.0f, 2.0f);
    drawCircle(r, panKcx, panKcy, 3.0f, 0.25f, 0.28f, 0.32f, 1.0f);

    // PAN Label above knob
    drawCenteredText(r, "PAN", panKcx - 24.0f, panKcy - kPanRadius - 10.0f, 48.0f, 10.0f, 8.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    // Readout below knob ("C", "L25", "R40")
    std::string panStr = (std::abs(data.pan) < 0.04f) ? "C" : ((data.pan < 0.0f) ? "L" + std::to_string(static_cast<int>(std::round(-data.pan * 100.0f))) : "R" + std::to_string(static_cast<int>(std::round(data.pan * 100.0f))));
    drawCenteredText(r, panStr, panKcx - 24.0f, panKcy + kPanRadius + 3.0f, 48.0f, 10.0f, 8.0f,
                     isDraggingPan ? theme.highlight.r : theme.primaryAccent.r,
                     isDraggingPan ? theme.highlight.g : theme.primaryAccent.g,
                     isDraggingPan ? theme.highlight.b : theme.primaryAccent.b, 1.0f);
}

void TrackPropertiesPanel::renderEqCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                        TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float eqCardH = 138.0f;
    if (cy + eqCardH < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, eqCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, eqCardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "CHANNEL EQ (3-BAND PARAMETRIC)", cx + 12.0f, cy + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float eqTglW = 60.0f;
    float eqTglX = cx + cw - eqTglW - 10.0f;
    Color eqBg = data.eqEnabled ? (theme.primaryAccent * 0.3f) : Color(0.12f, 0.12f, 0.14f, 0.9f);
    Color eqBorder = data.eqEnabled ? theme.primaryAccent : theme.borderSubtle;
    Color eqText = data.eqEnabled ? theme.primaryAccent : theme.textMuted;
    drawButton(r, Rect2D(eqTglX, cy + 6.0f, eqTglW, 20.0f),
               data.eqEnabled ? "ACTIVE" : "BYPASS", eqBg, eqBorder, eqText, 8.5f, 3.0f, 1.0f);

    float kw3 = cw / 3.0f;
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

    float eqRow1Y = cy + 45.0f;
    float normHpf = std::clamp((data.eqHpf - 20.0f) / 480.0f, 0.0f, 1.0f);
    drawEqKnob(cx + kw3 * 0.5f, eqRow1Y, "HPF CUT", std::to_string(static_cast<int>(data.eqHpf)) + "Hz", normHpf);

    float normLow = std::clamp((data.eqLowGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string lowStr = (data.eqLowGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqLowGain)) + "dB";
    drawEqKnob(cx + kw3 * 1.5f, eqRow1Y, "LOW GAIN", lowStr, normLow);

    float normHigh = std::clamp((data.eqHighGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string highStr = (data.eqHighGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqHighGain)) + "dB";
    drawEqKnob(cx + kw3 * 2.5f, eqRow1Y, "HIGH GAIN", highStr, normHigh);

    float eqRow2Y = cy + 98.0f;
    float normMidF = std::clamp((data.eqMidFreq - 200.0f) / 7800.0f, 0.0f, 1.0f);
    std::string midFStr = data.eqMidFreq >= 1000.0f ? (std::to_string(static_cast<int>(data.eqMidFreq / 1000.0f)) + "kHz") : (std::to_string(static_cast<int>(data.eqMidFreq)) + "Hz");
    drawEqKnob(cx + kw3 * 0.5f, eqRow2Y, "MID FREQ", midFStr, normMidF);

    float normMidG = std::clamp((data.eqMidGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::string midGStr = (data.eqMidGain >= 0.0f ? "+" : "") + std::to_string(static_cast<int>(data.eqMidGain)) + "dB";
    drawEqKnob(cx + kw3 * 1.5f, eqRow2Y, "MID GAIN", midGStr, normMidG);

    float normMidQ = std::clamp((data.eqMidQ - 0.3f) / 9.7f, 0.0f, 1.0f);
    drawEqKnob(cx + kw3 * 2.5f, eqRow2Y, "MID Q", "Q=" + std::to_string(static_cast<int>(data.eqMidQ)), normMidQ);
}

void TrackPropertiesPanel::renderFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float faceH = isWide ? 265.0f : 215.0f;
    if (cy + faceH < bounds_.y || cy > bounds_.y + bounds_.h) return;

    bool isLight = (data.instrumentEngine == "tb303");

    // Chassis background & styling based on instrumentEngine
    if (data.instrumentEngine == "tb303") {
        drawRoundedRect(r, cx, cy, cw, faceH, 6.0f, 0.82f, 0.82f, 0.80f, 1.0f);
        drawRoundedRectOutline(r, cx, cy, cw, faceH, 6.0f, 0.45f, 0.45f, 0.45f, 1.0f, 2.0f);
    } else if (data.instrumentEngine == "dx7") {
        drawRoundedRect(r, cx, cy, cw, faceH, 6.0f, 0.12f, 0.13f, 0.15f, 1.0f);
        drawRoundedRectOutline(r, cx, cy, cw, faceH, 6.0f, 0.0f, 0.66f, 0.53f, 1.0f, 2.0f);
    } else if (data.instrumentEngine == "tr808" || data.instrumentEngine == "tr909") {
        drawRoundedRect(r, cx, cy, cw, faceH, 6.0f, 0.18f, 0.19f, 0.22f, 1.0f);
        drawRoundedRectOutline(r, cx, cy, cw, faceH, 6.0f, 0.85f, 0.35f, 0.15f, 1.0f, 2.0f);
    } else {
        drawRoundedRect(r, cx, cy, cw, faceH, 6.0f, 0.13f, 0.15f, 0.18f, 1.0f);
        drawRoundedRectOutline(r, cx, cy, cw, faceH, 6.0f, data.r * 0.7f, data.g * 0.7f, data.b * 0.7f, 1.0f, 1.8f);
    }

    // Corner mounting hex bolts
    drawCircle(r, cx + 10.0f, cy + 10.0f, 3.0f, 0.35f, 0.38f, 0.45f, 1.0f);
    drawCircle(r, cx + cw - 10.0f, cy + 10.0f, 3.0f, 0.35f, 0.38f, 0.45f, 1.0f);
    drawCircle(r, cx + 10.0f, cy + faceH - 10.0f, 3.0f, 0.35f, 0.38f, 0.45f, 1.0f);
    drawCircle(r, cx + cw - 10.0f, cy + faceH - 10.0f, 3.0f, 0.35f, 0.38f, 0.45f, 1.0f);

    // Top Banner Plate
    float banH = isWide ? 36.0f : 32.0f;
    drawRoundedRect(r, cx, cy, cw, banH, 6.0f, 0.08f, 0.09f, 0.12f, 0.95f);
    drawLine(r, cx, cy + banH, cx + cw, cy + banH, 0.25f, 0.28f, 0.36f, 1.0f, 1.2f);

    // < PREV and NEXT > Buttons
    drawButton(r, Rect2D(cx + 8.0f, cy + 5.0f, isWide ? 56.0f : 46.0f, isWide ? 24.0f : 22.0f), "< PREV",
               Color(0.16f, 0.18f, 0.24f, 1.0f), theme.borderSubtle, theme.primaryAccent, 8.5f, 3.0f, 1.0f);
    drawButton(r, Rect2D(cx + (isWide ? 70.0f : 58.0f), cy + 5.0f, isWide ? 56.0f : 46.0f, isWide ? 24.0f : 22.0f), "NEXT >",
               Color(0.16f, 0.18f, 0.24f, 1.0f), theme.borderSubtle, theme.primaryAccent, 8.5f, 3.0f, 1.0f);

    // Preset Counter
    if (isWide) {
        std::string pCounter = std::to_string(data.activePresetIdx + 1) + "/" + std::to_string(data.totalPresets);
        drawText(r, pCounter, cx + 135.0f, cy + 11.0f, 9.5f, 0.55f, 0.60f, 0.70f, 1.0f);
    }

    // Instrument Title & Subtitle
    float titleX = isWide ? (cx + 185.0f) : (cx + 112.0f);
    drawText(r, data.instrument, titleX, cy + (isWide ? 8.0f : 6.0f), isWide ? 12.0f : 11.0f, 0.0f, 0.95f, 1.0f, 1.0f);
    drawText(r, "HARDWARE SCRIPT INTERFACE", titleX, cy + (isWide ? 22.0f : 19.0f), 7.5f, 0.50f, 0.55f, 0.65f, 1.0f);

    // [ ⛶ FULL ] and [ ⇄ CHANGE INSTRUMENT ] Buttons
    float fullBtnW = isWide ? 56.0f : 44.0f;
    float fullBtnX = cx + cw - fullBtnW - 8.0f;
    drawButton(r, Rect2D(fullBtnX, cy + 5.0f, fullBtnW, isWide ? 24.0f : 22.0f),
               isWide ? "[ ⛶ FULL ]" : "[ ⛶ ]",
               Color(0.12f, 0.22f, 0.32f, 1.0f), theme.borderSubtle, theme.primaryAccent, 8.0f, 3.0f, 1.0f);

    float chgW = isWide ? 120.0f : 68.0f;
    float chgX = fullBtnX - chgW - 6.0f;
    drawButton(r, Rect2D(chgX, cy + 5.0f, chgW, isWide ? 24.0f : 22.0f),
               isWide ? "[ ⇄ CHANGE ]" : "[ ⇄ ]",
               Color(0.16f, 0.20f, 0.28f, 1.0f), theme.borderSubtle, theme.primaryAccent, 8.0f, 3.0f, 1.0f);

    // Rotary Knobs Layout
    size_t knobCount = std::min(data.knobs.size(), static_cast<size_t>(6));

    if (isWide) {
        // Wide Layout: 1 row of up to 6 knobs on left, CRT Oscilloscope on right
        float knobAreaW = cw - 230.0f; // Leave 210px for oscilloscope on the right
        float kStep = knobAreaW / static_cast<float>(std::max(size_t{1}, knobCount));
        float kRadius = 18.0f;

        for (size_t k = 0; k < knobCount; ++k) {
            float kcx = cx + 18.0f + static_cast<float>(k) * kStep + (kStep * 0.5f);
            float kcy = cy + banH + 75.0f;

            const auto& knob = data.knobs[k];
            bool isDragging = (dragMode_ == DragMode::InstrumentKnob && activeKnobIndex_ == static_cast<int>(k));

            // Collet well & drop shadow
            drawCircle(r, kcx, kcy + 2.0f, kRadius + 2.0f, 0.08f, 0.09f, 0.11f, 0.40f);
            drawCircle(r, kcx, kcy, kRadius, isLight ? 0.28f : 0.16f, isLight ? 0.28f : 0.18f, isLight ? 0.30f : 0.22f, 1.0f);
            drawCircle(r, kcx, kcy, kRadius - 2.5f, isLight ? 0.75f : 0.22f, isLight ? 0.75f : 0.24f, isLight ? 0.74f : 0.28f, 1.0f);
            drawCircleOutline(r, kcx, kcy, kRadius, isDragging ? theme.highlight.r : 0.45f,
                              isDragging ? theme.highlight.g : 0.48f, isDragging ? theme.highlight.b : 0.55f, 1.0f, 1.2f);

            // Needle
            constexpr float minA = -2.35619449f;
            constexpr float maxA = 2.35619449f;
            float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);
            float indX = kcx + std::sin(curA) * (kRadius - 4.0f);
            float indY = kcy - std::cos(curA) * (kRadius - 4.0f);
            drawLine(r, kcx, kcy, indX, indY,
                     isDragging ? theme.highlight.r : (isLight ? 0.90f : data.r),
                     isDragging ? theme.highlight.g : (isLight ? 0.20f : data.g),
                     isDragging ? theme.highlight.b : (isLight ? 0.15f : data.b), 1.0f, 2.0f);
            drawCircle(r, kcx, kcy, 4.0f, 0.20f, 0.22f, 0.25f, 1.0f);

            // Label and display
            drawCenteredText(r, knob.label, kcx - 35.0f, kcy + kRadius + 6.0f, 70.0f, 12.0f, 8.5f,
                             isDragging ? theme.highlight.r : (isLight ? 0.12f : 0.85f),
                             isDragging ? theme.highlight.g : (isLight ? 0.12f : 0.88f),
                             isDragging ? theme.highlight.b : (isLight ? 0.12f : 0.95f), 1.0f);
            drawCenteredText(r, knob.display, kcx - 35.0f, kcy + kRadius + 18.0f, 70.0f, 12.0f, 8.0f,
                             isDragging ? theme.highlight.r : (isLight ? 0.30f : 0.55f),
                             isDragging ? theme.highlight.g : (isLight ? 0.30f : 0.60f),
                             isDragging ? theme.highlight.b : (isLight ? 0.30f : 0.70f), 1.0f);
        }

        // Live Real-Time CRT Audio Oscilloscope Display
        float oscW = 195.0f;
        float oscH = 160.0f;
        float oscX = cx + cw - oscW - 14.0f;
        float oscY = cy + banH + 16.0f;

        drawRoundedRect(r, oscX, oscY, oscW, oscH, 6.0f, 0.02f, 0.05f, 0.03f, 1.0f);
        drawRoundedRectOutline(r, oscX, oscY, oscW, oscH, 6.0f, 0.15f, 0.85f, 0.35f, 0.8f, 1.4f);
        drawText(r, "OSCILLOSCOPE • LIVE OUTPUT", oscX + 10.0f, oscY + 8.0f, 8.5f, 0.20f, 0.95f, 0.40f, 1.0f);

        // CRT Phosphor Grid Lines
        float midY = oscY + oscH * 0.55f;
        drawLine(r, oscX + 6.0f, midY, oscX + oscW - 6.0f, midY, 0.10f, 0.45f, 0.20f, 0.45f, 1.0f);
        drawLine(r, oscX + 6.0f, midY - 30.0f, oscX + oscW - 6.0f, midY - 30.0f, 0.10f, 0.35f, 0.18f, 0.30f, 0.8f);
        drawLine(r, oscX + 6.0f, midY + 30.0f, oscX + oscW - 6.0f, midY + 30.0f, 0.10f, 0.35f, 0.18f, 0.30f, 0.8f);
        drawLine(r, oscX + oscW * 0.5f, oscY + 24.0f, oscX + oscW * 0.5f, oscY + oscH - 8.0f, 0.10f, 0.45f, 0.20f, 0.45f, 1.0f);

        // Waveform polyline from buffer
        if (data.scopeBuffer && data.scopeBufferCount > 0) {
            constexpr int kPts = 48;
            float prevX = oscX + 8.0f;
            float prevY = midY - data.scopeBuffer[0] * 38.0f;
            for (int i = 1; i < kPts; ++i) {
                size_t sIdx = (static_cast<size_t>(i) * data.scopeBufferCount) / static_cast<size_t>(kPts);
                float curX = oscX + 8.0f + (static_cast<float>(i) / static_cast<float>(kPts - 1)) * (oscW - 16.0f);
                float curYPoint = midY - data.scopeBuffer[sIdx] * 38.0f;
                drawLine(r, prevX, prevY, curX, curYPoint, 0.20f, 1.0f, 0.45f, 0.95f, 1.8f);
                prevX = curX;
                prevY = curYPoint;
            }
        }
    } else {
        // Compact Layout: 2 rows of 3 knobs
        float colW = cw / 3.0f;
        float kRadius = 14.5f;

        for (size_t k = 0; k < knobCount; ++k) {
            int row = static_cast<int>(k / 3);
            int col = static_cast<int>(k % 3);
            float kcx = cx + static_cast<float>(col) * colW + (colW * 0.5f);
            float kcy = cy + banH + 34.0f + static_cast<float>(row) * 72.0f;

            const auto& knob = data.knobs[k];
            bool isDragging = (dragMode_ == DragMode::InstrumentKnob && activeKnobIndex_ == static_cast<int>(k));

            drawCircle(r, kcx, kcy + 2.0f, kRadius + 2.0f, 0.08f, 0.09f, 0.11f, 0.40f);
            drawCircle(r, kcx, kcy, kRadius, isLight ? 0.28f : 0.16f, isLight ? 0.28f : 0.18f, isLight ? 0.30f : 0.22f, 1.0f);
            drawCircle(r, kcx, kcy, kRadius - 2.5f, isLight ? 0.75f : 0.22f, isLight ? 0.75f : 0.24f, isLight ? 0.74f : 0.28f, 1.0f);
            drawCircleOutline(r, kcx, kcy, kRadius, isDragging ? theme.highlight.r : 0.45f,
                              isDragging ? theme.highlight.g : 0.48f, isDragging ? theme.highlight.b : 0.55f, 1.0f, 1.2f);

            constexpr float minA = -2.35619449f;
            constexpr float maxA = 2.35619449f;
            float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);
            float indX = kcx + std::sin(curA) * (kRadius - 4.0f);
            float indY = kcy - std::cos(curA) * (kRadius - 4.0f);
            drawLine(r, kcx, kcy, indX, indY,
                     isDragging ? theme.highlight.r : (isLight ? 0.90f : data.r),
                     isDragging ? theme.highlight.g : (isLight ? 0.20f : data.g),
                     isDragging ? theme.highlight.b : (isLight ? 0.15f : data.b), 1.0f, 2.0f);
            drawCircle(r, kcx, kcy, 4.0f, 0.20f, 0.22f, 0.25f, 1.0f);

            drawCenteredText(r, knob.label, kcx - 35.0f, kcy + kRadius + 3.0f, 70.0f, 12.0f, 8.5f,
                             isDragging ? theme.highlight.r : (isLight ? 0.12f : 0.85f),
                             isDragging ? theme.highlight.g : (isLight ? 0.12f : 0.88f),
                             isDragging ? theme.highlight.b : (isLight ? 0.12f : 0.95f), 1.0f);
            drawCenteredText(r, knob.display, kcx - 35.0f, kcy + kRadius + 15.0f, 70.0f, 12.0f, 8.0f,
                             isDragging ? theme.highlight.r : (isLight ? 0.30f : 0.55f),
                             isDragging ? theme.highlight.g : (isLight ? 0.30f : 0.60f),
                             isDragging ? theme.highlight.b : (isLight ? 0.30f : 0.70f), 1.0f);
        }
    }
}

void TrackPropertiesPanel::renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                 TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float h = 88.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "HARMONIC CHORD TRACK FOLLOW", cx + 12.0f, cy + 10.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // 5 Mode Chips
    const char* chipLabels[5] = {"OFF", "CHORD", "BASS", "SCALE", "COLOR"};
    ChordFollowMode chipModes[5] = {ChordFollowMode::Off, ChordFollowMode::Chord, ChordFollowMode::Bass, ChordFollowMode::Scale, ChordFollowMode::ColorLead};
    float chipW = (cw - 170.0f) / 5.0f;
    float chipH = 24.0f;
    float chipY = cy + 32.0f;

    for (int i = 0; i < 5; ++i) {
        float chipX = cx + 12.0f + static_cast<float>(i) * (chipW + 5.0f);
        bool isSel = (data.chordFollowMode == chipModes[i]);
        drawRoundedRect(r, chipX, chipY, chipW, chipH, 3.5f,
                        isSel ? (theme.primaryAccent * 0.30f) : Color(0.12f, 0.14f, 0.18f, 0.9f),
                        isSel ? 0.95f : 0.65f);
        drawRoundedRectOutline(r, chipX, chipY, chipW, chipH, 3.5f,
                               isSel ? theme.primaryAccent : theme.borderSubtle,
                               isSel ? 0.95f : 0.40f, isSel ? 1.4f : 1.0f);
        drawCenteredText(r, chipLabels[i], chipX, chipY + 2.0f, chipW, chipH, 8.5f,
                         isSel ? theme.primaryAccent.r : theme.textMuted.r,
                         isSel ? theme.primaryAccent.g : theme.textMuted.g,
                         isSel ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
    }

    // Bake to MIDI action button
    float bakeW = 100.0f;
    float bakeX = cx + cw - bakeW - 12.0f;
    drawButton(r, Rect2D(bakeX, chipY, bakeW, chipH), "[ BAKE TO MIDI ]",
               Color(0.16f, 0.20f, 0.28f, 1.0f), theme.borderSubtle, theme.secondaryAccent, 8.5f, 3.5f, 1.0f);

    // Explanatory dynamic caption
    std::string desc = "Track notes play freely without chord progression transposition.";
    if (data.chordFollowMode == ChordFollowMode::Bass) {
        desc = "Follows root/bass harmonic voice leading with octave constraint.";
    } else if (data.chordFollowMode == ChordFollowMode::Chord) {
        desc = "Conforms notes to polyphonic triad/7th voicing across bars.";
    } else if (data.chordFollowMode == ChordFollowMode::Scale) {
        desc = "Constrains melodies dynamically to chord scale tones.";
    } else if (data.chordFollowMode == ChordFollowMode::ColorLead) {
        desc = "Voice leads tension color tones (9th, 11th, 13th) to resolution.";
    }
    drawText(r, desc, cx + 12.0f, cy + 64.0f, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
}

void TrackPropertiesPanel::renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                            TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float h = isWide ? 120.0f : 106.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "MIDI FX RACK (" + std::to_string(data.midiFx.size()) + ")", cx + 12.0f, cy + 10.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float addBtnW = 95.0f;
    float addBtnX = cx + cw - addBtnW - 10.0f;
    drawButton(r, Rect2D(addBtnX, cy + 6.0f, addBtnW, 20.0f), "+ ADD MIDI FX",
               theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent, 8.5f, 3.0f, 1.0f);

    if (data.midiFx.empty()) {
        float phY = cy + 34.0f;
        drawRoundedRect(r, cx + 10.0f, phY, cw - 20.0f, 30.0f, 4.0f, 0.08f, 0.09f, 0.12f, 0.65f);
        drawRoundedRectOutline(r, cx + 10.0f, phY, cw - 20.0f, 30.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.40f, 1.0f);
        drawText(r, "No MIDI FX loaded. Click + ADD MIDI FX to insert.", cx + 22.0f, phY + 9.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.80f);
    } else {
        float itemH = 28.0f;
        float itemGap = 5.0f;
        size_t maxItems = isWide ? 2 : 2;
        for (size_t mi = 0; mi < data.midiFx.size() && mi < maxItems; ++mi) {
            float itemY = cy + 32.0f + static_cast<float>(mi) * (itemH + itemGap);
            const auto& fx = data.midiFx[mi];

            drawRoundedRect(r, cx + 10.0f, itemY, cw - 20.0f, itemH, 4.0f,
                            theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.90f);
            drawRoundedRectOutline(r, cx + 10.0f, itemY, cw - 20.0f, itemH, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.50f, 1.0f);

            // Left Power/Status indicator dot
            drawCircle(r, cx + 22.0f, itemY + itemH * 0.5f, 3.5f,
                       theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b,
                       fx.enabled ? 1.0f : 0.35f);

            // FX Name
            drawText(r, fx.name, cx + 32.0f, itemY + 8.0f, 10.0f,
                     fx.enabled ? theme.textPrimary.r : theme.textMuted.r,
                     fx.enabled ? theme.textPrimary.g : theme.textMuted.g,
                     fx.enabled ? theme.textPrimary.b : theme.textMuted.b, 1.0f);

            // Status label
            drawText(r, fx.enabled ? "ACTIVE" : "BYPASS", cx + cw - 95.0f, itemY + 8.5f, 8.0f,
                     fx.enabled ? theme.primaryAccent.r : theme.textMuted.r,
                     fx.enabled ? theme.primaryAccent.g : theme.textMuted.g,
                     fx.enabled ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);

            // Fullscreen FX button [⛶]
            drawText(r, "[⛶]", cx + cw - 50.0f, itemY + 8.0f, 9.0f,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.90f);

            // Delete button [X]
            drawText(r, "[X]", cx + cw - 26.0f, itemY + 8.0f, 9.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
        }
    }
}

void TrackPropertiesPanel::renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float h = isWide ? 120.0f : 106.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "AUDIO FX INSERT RACK (" + std::to_string(data.audioFx.size()) + ")", cx + 12.0f, cy + 10.0f, 10.5f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float addBtnW = 85.0f;
    float addBtnX = cx + cw - addBtnW - 10.0f;
    drawButton(r, Rect2D(addBtnX, cy + 6.0f, addBtnW, 20.0f), "+ ADD FX",
               theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);

    if (data.audioFx.empty()) {
        float phY = cy + 34.0f;
        drawRoundedRect(r, cx + 10.0f, phY, cw - 20.0f, 30.0f, 4.0f, 0.08f, 0.09f, 0.12f, 0.65f);
        drawRoundedRectOutline(r, cx + 10.0f, phY, cw - 20.0f, 30.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.35f, 1.0f);
        drawText(r, "No audio effects inserted. Click + ADD FX to insert.", cx + 22.0f, phY + 9.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
    } else {
        float itemH = 28.0f;
        float itemGap = 5.0f;
        size_t maxItems = isWide ? 2 : 2;
        for (size_t fi = 0; fi < data.audioFx.size() && fi < maxItems; ++fi) {
            float itemY = cy + 32.0f + static_cast<float>(fi) * (itemH + itemGap);
            const auto& fx = data.audioFx[fi];

            drawRoundedRect(r, cx + 10.0f, itemY, cw - 20.0f, itemH, 4.0f,
                            theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.90f);
            drawRoundedRectOutline(r, cx + 10.0f, itemY, cw - 20.0f, itemH, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.50f, 1.0f);

            // Left Power/Status indicator dot
            drawCircle(r, cx + 22.0f, itemY + itemH * 0.5f, 3.5f,
                       theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b,
                       fx.enabled ? 1.0f : 0.35f);

            // FX Name
            drawText(r, fx.name, cx + 32.0f, itemY + 8.0f, 10.0f,
                     fx.enabled ? theme.textPrimary.r : theme.textMuted.r,
                     fx.enabled ? theme.textPrimary.g : theme.textMuted.g,
                     fx.enabled ? theme.textPrimary.b : theme.textMuted.b, 1.0f);

            // Status label
            drawText(r, fx.enabled ? "ACTIVE" : "BYPASS", cx + cw - 95.0f, itemY + 8.5f, 8.0f,
                     fx.enabled ? theme.secondaryAccent.r : theme.textMuted.r,
                     fx.enabled ? theme.secondaryAccent.g : theme.textMuted.g,
                     fx.enabled ? theme.secondaryAccent.b : theme.textMuted.b, 1.0f);

            // Fullscreen FX button [⛶]
            drawText(r, "[⛶]", cx + cw - 50.0f, itemY + 8.0f, 9.0f,
                     theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.90f);

            // Delete button [X]
            drawText(r, "[X]", cx + cw - 26.0f, itemY + 8.0f, 9.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
        }
    }
}

void TrackPropertiesPanel::renderColorPalette(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float h = 36.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "TRACK COLOR", cx + 12.0f, cy + 12.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    float startX = cx + (cw >= 560.0f ? 120.0f : 95.0f);
    float availW = (cx + cw - 14.0f) - startX;
    float swatchSpacing = availW / 8.0f;
    float scy = cy + h * 0.5f;

    for (int p = 0; p < 8; ++p) {
        float scx = startX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
        float cr = kQuickPalette[p][0];
        float cg = kQuickPalette[p][1];
        float cb = kQuickPalette[p][2];
        bool isCurCol = (std::abs(data.r - cr) < 0.05f && std::abs(data.g - cg) < 0.05f && std::abs(data.b - cb) < 0.05f);

        drawCircle(r, scx, scy, 9.0f, cr, cg, cb, 1.0f);
        if (isCurCol) {
            drawCircleOutline(r, scx, scy, 12.5f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f);
        }
    }
}

void TrackPropertiesPanel::renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                               TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                               float mouseX, float mouseY) {
    (void)mouseX; (void)mouseY;
    float curY = cy;

    // Header card
    float headH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, headH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, headH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawRoundedRect(r, cx + 10.0f, curY + 10.0f, 5.0f, 30.0f, 2.5f, 1.0f, 0.85f, 0.0f, 1.0f);
    drawText(r, "MASTER BUS CONSOLE", cx + 24.0f, curY + 12.0f, 13.5f, 1.0f, 0.88f, 0.10f, 1.0f);
    drawText(r, "STEREO SUMMING & DYNAMICS MASTERING CHAIN", cx + 24.0f, curY + 30.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    curY += headH + 8.0f;

    // Master EQ Card
    float eqH = 100.0f;
    drawRoundedRect(r, cx, curY, cw, eqH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, eqH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "MASTER BUS EQUALIZER", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float kw4 = cw / 4.0f;
    auto drawKnobMini = [&](float kx, float ky, const std::string& label, const std::string& valStr, float normVal) {
        drawCircle(r, kx, ky, 12.0f, 0.14f, 0.16f, 0.20f, 1.0f);
        drawCircleOutline(r, kx, ky, 12.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
        float ang = -2.35f + normVal * 4.71f;
        drawLine(r, kx, ky, kx + std::cos(ang) * 9.0f, ky + std::sin(ang) * 9.0f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.8f);
        drawCenteredText(r, label, kx - 30.0f, ky + 14.0f, 60.0f, 10.0f, 7.5f,
                         theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        drawCenteredText(r, valStr, kx - 30.0f, ky + 24.0f, 60.0f, 10.0f, 7.5f,
                         theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    };

    float eqRowY = curY + 44.0f;
    drawKnobMini(cx + kw4 * 0.5f, eqRowY, "SUB CUT", std::to_string(static_cast<int>(data.masterSubCut)) + "Hz", (data.masterSubCut - 20.0f) / 25.0f);
    drawKnobMini(cx + kw4 * 1.5f, eqRowY, "LOW", (data.masterLowGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterLowGain)) + "dB", (data.masterLowGain + 12.0f) / 24.0f);
    drawKnobMini(cx + kw4 * 2.5f, eqRowY, "MID", (data.masterMidGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterMidGain)) + "dB", (data.masterMidGain + 12.0f) / 24.0f);
    drawKnobMini(cx + kw4 * 3.5f, eqRowY, "HIGH", (data.masterHighGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterHighGain)) + "dB", (data.masterHighGain + 12.0f) / 24.0f);
    curY += eqH + 8.0f;

    // Master Limiter Card
    float limH = 110.0f;
    drawRoundedRect(r, cx, curY, cw, limH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, limH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "BRICKWALL LIMITER & LOUDNESS", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float autoBtnW = 100.0f;
    drawButton(r, Rect2D(cx + cw - autoBtnW - 10.0f, curY + 6.0f, autoBtnW, 20.0f), "[ AUTO-MASTER ]",
               theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);

    float limRowY = curY + 48.0f;
    float kw3 = cw / 3.0f;
    drawKnobMini(cx + kw3 * 0.5f, limRowY, "CEILING", std::to_string(data.masterCeilingDbfs).substr(0, 4) + "dB", (data.masterCeilingDbfs + 2.0f) / 2.0f);
    drawKnobMini(cx + kw3 * 1.5f, limRowY, "DRIVE", "+" + std::to_string(static_cast<int>(data.masterLimiterDrive)) + "dB", data.masterLimiterDrive / 12.0f);
    drawKnobMini(cx + kw3 * 2.5f, limRowY, "TARGET LUFS", std::to_string(static_cast<int>(data.masterTargetLufs)) + "LUFS", (data.masterTargetLufs + 24.0f) / 18.0f);
}

void TrackPropertiesPanel::renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                             float mouseX, float mouseY) {
    (void)mouseX; (void)mouseY;
    float curY = cy;

    float headH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, headH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, headH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawRoundedRect(r, cx + 10.0f, curY + 10.0f, 5.0f, 30.0f, 2.5f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, data.clipName, cx + 24.0f, curY + 12.0f, 13.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "MIDI SEQUENCE CLIP", cx + 24.0f, curY + 30.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    curY += headH + 8.0f;

    // Timing & Looping Card
    float timeH = 90.0f;
    drawRoundedRect(r, cx, curY, cw, timeH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, timeH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "CLIP TIMING & PLAYBACK", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    std::string posStr = "Start: Bar " + std::to_string(data.clipStartBar) + "  •  Length: " + std::to_string(data.clipLengthBars) + " Bars";
    drawText(r, posStr, cx + 12.0f, curY + 32.0f, 9.5f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);

    float loopBtnW = 90.0f;
    drawButton(r, Rect2D(cx + 12.0f, curY + 54.0f, loopBtnW, 24.0f),
               data.clipLooped ? "LOOP: ON" : "LOOP: OFF",
               data.clipLooped ? theme.primaryAccent * 0.3f : Color(0.12f, 0.14f, 0.18f, 0.9f),
               theme.borderSubtle, theme.primaryAccent, 8.5f, 3.0f, 1.0f);

    // Edit in Piano Roll Button
    float editBtnW = 140.0f;
    drawButton(r, Rect2D(cx + cw - editBtnW - 12.0f, curY + 54.0f, editBtnW, 24.0f),
               "[ EDIT IN PIANO ROLL ]", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent,
               8.5f, 3.0f, 1.0f);
}

void TrackPropertiesPanel::renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme) {
    drawRoundedRect(r, scrollbarBounds_.x, scrollbarBounds_.y, scrollbarBounds_.w, scrollbarBounds_.h, 2.5f,
                    0.06f, 0.07f, 0.09f, 0.85f);

    float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
    if (maxScroll <= 0.0f) return;

    float thumbRatio = std::clamp(bounds_.h / totalContentHeight_, 0.15f, 0.90f);
    float thumbH = scrollbarBounds_.h * thumbRatio;
    float normScroll = std::clamp(scrollY_ / maxScroll, 0.0f, 1.0f);
    float thumbY = scrollbarBounds_.y + normScroll * (scrollbarBounds_.h - thumbH);

    bool isDragging = (dragMode_ == DragMode::Scrollbar);
    drawRoundedRect(r, scrollbarBounds_.x, thumbY, scrollbarBounds_.w, thumbH, 2.5f,
                    isDragging ? theme.primaryAccent.r : 0.40f,
                    isDragging ? theme.primaryAccent.g : 0.45f,
                    isDragging ? theme.primaryAccent.b : 0.55f,
                    isDragging ? 1.0f : 0.70f);
}

TrackPropertiesHitResult TrackPropertiesPanel::hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    TrackPropertiesHitResult res;

    // Scrollbar hit
    if (scrollbarBounds_.contains(mx, my)) {
        res.hit = true;
        res.area = TrackPropertiesHitArea::ScrollbarTrack;
        return res;
    }

    // Ribbon hits
    if (showTrackRibbon_ && ribbonBounds_.contains(mx, my)) {
        size_t trackCount = data.allTrackNames.empty() ? 5 : data.allTrackNames.size();
        const float startX = ribbonBounds_.x + 4.0f;
        const float tabW = std::clamp((ribbonBounds_.w - 40.0f) / static_cast<float>(trackCount), 90.0f, 130.0f);
        const float tabGap = 8.0f;
        for (size_t i = 0; i < trackCount; ++i) {
            float tx = startX + static_cast<float>(i) * (tabW + tabGap);
            if (mx >= tx && mx <= tx + tabW) {
                res.hit = true;
                res.index = static_cast<int>(i);
                return res;
            }
        }
    }

    // Header Card Hits
    if (headerCardBounds_.contains(mx, my)) {
        float cx = headerCardBounds_.x;
        float cy = headerCardBounds_.y;
        float cw = headerCardBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);
        float h = isWide ? 42.0f : 38.0f;
        float btnW = 24.0f;
        float btnH = 24.0f;
        float btnY = cy + (h - btnH) * 0.5f;

        if (isWide) {
            float fzX = cx + cw - 8.0f - btnW;
            float soloX = fzX - 5.0f - btnW;
            float muteX = soloX - 5.0f - btnW;
            float editBtnX = muteX - 5.0f - btnW;
            float codeW = 54.0f;
            float codeX = editBtnX - 8.0f - codeW;

            if (mx >= fzX && mx <= fzX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FreezeButton;
                return res;
            }
            if (mx >= soloX && mx <= soloX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::SoloButton;
                return res;
            }
            if (mx >= muteX && mx <= muteX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MuteButton;
                return res;
            }
            if (mx >= editBtnX && mx <= editBtnX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RenameButton;
                return res;
            }
            if (mx >= codeX && mx <= codeX + codeW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::CodeButton;
                return res;
            }
        } else {
            float soloX = cx + cw - 8.0f - btnW;
            float muteX = soloX - 5.0f - btnW;
            float editBtnX = muteX - 5.0f - btnW;

            if (mx >= soloX && mx <= soloX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::SoloButton;
                return res;
            }
            if (mx >= muteX && mx <= muteX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MuteButton;
                return res;
            }
            if (mx >= editBtnX && mx <= editBtnX + btnW && my >= btnY && my <= btnY + btnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RenameButton;
                return res;
            }
        }

        // Clicking anywhere on track title or header background also triggers track rename / properties dialog
        res.hit = true;
        res.area = TrackPropertiesHitArea::RenameButton;
        return res;
    }

    // Color Swatches Card Hits (directly below Header Card)
    if (colorCardBounds_.contains(mx, my)) {
        float cx = colorCardBounds_.x;
        float cy = colorCardBounds_.y;
        float cw = colorCardBounds_.w;
        float startX = cx + (cw >= 560.0f ? 120.0f : 95.0f);
        float availW = (cx + cw - 14.0f) - startX;
        float swatchSpacing = availW / 8.0f;
        float scy = cy + 18.0f;
        for (int p = 0; p < 8; ++p) {
            float scx = startX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
            if (std::hypot(mx - scx, my - scy) <= 12.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ColorSwatch;
                res.index = p;
                return res;
            }
        }
    }

    // Mixer Card Hits
    if (mixerCardBounds_.contains(mx, my)) {
        float cx = mixerCardBounds_.x;
        float cy = mixerCardBounds_.y;
        float cw = mixerCardBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);
        float h = isWide ? 68.0f : 60.0f;
        float sY = cy + (isWide ? 25.0f : 21.0f);
        float sH = isWide ? 18.0f : 16.0f;

        // Volume slider
        float vLabelX = cx + 14.0f;
        float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
        float panAreaW = isWide ? 85.0f : 74.0f;
        float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
        if (vTrackW < 50.0f) vTrackW = 50.0f;

        if (mx >= vTrackX - 6.0f && mx <= vTrackX + vTrackW + 6.0f && my >= sY - 6.0f && my <= sY + sH + 6.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::VolumeSlider;
            res.normVal = std::clamp((mx - vTrackX) / vTrackW, 0.0f, 1.0f);
            return res;
        }

        // Pan rotary knob
        float panKcx = cx + cw - (isWide ? 40.0f : 34.0f);
        float panKcy = cy + (h * 0.5f) - (isWide ? 2.0f : 1.0f);
        float kPanRadius = isWide ? 13.5f : 12.0f;
        if (std::hypot(mx - panKcx, my - panKcy) <= kPanRadius + 6.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::PanKnob;
            return res;
        }
    }

    // Faceplate Hits
    if (faceplateBounds_.contains(mx, my)) {
        float cx = faceplateBounds_.x;
        float cy = faceplateBounds_.y;
        float cw = faceplateBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);

        // Prev & Next preset
        if (mx >= cx + 8.0f && mx <= cx + 64.0f && my >= cy + 4.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::InstrumentPrevPreset;
            return res;
        }
        float nextX = cx + (isWide ? 70.0f : 58.0f);
        if (mx >= nextX && mx <= nextX + (isWide ? 56.0f : 46.0f) && my >= cy + 4.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::InstrumentNextPreset;
            return res;
        }

        // Fullscreen Instrument button [cx + cw - fullBtnW - 8, cy + 4, fullBtnW, 26]
        float fullBtnW = isWide ? 56.0f : 44.0f;
        float fullBtnX = cx + cw - fullBtnW - 8.0f;
        if (mx >= fullBtnX && mx <= fullBtnX + fullBtnW && my >= cy + 4.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::FullscreenInstrument;
            return res;
        }

        // Change instrument
        float chgW = isWide ? 120.0f : 68.0f;
        float chgX = fullBtnX - chgW - 6.0f;
        if (mx >= chgX && mx <= chgX + chgW && my >= cy + 4.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ChangeInstrument;
            return res;
        }

        // Knobs
        size_t knobCount = std::min(data.knobs.size(), static_cast<size_t>(6));
        if (isWide) {
            float knobAreaW = cw - 230.0f;
            float kStep = knobAreaW / static_cast<float>(std::max(size_t{1}, knobCount));
            for (size_t k = 0; k < knobCount; ++k) {
                float kcx = cx + 18.0f + static_cast<float>(k) * kStep + (kStep * 0.5f);
                float kcy = cy + 36.0f + 75.0f;
                if (std::hypot(mx - kcx, my - kcy) <= 24.0f) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::InstrumentKnob;
                    res.index = static_cast<int>(k);
                    return res;
                }
            }
        } else {
            float colW = cw / 3.0f;
            for (size_t k = 0; k < knobCount; ++k) {
                int row = static_cast<int>(k / 3);
                int col = static_cast<int>(k % 3);
                float kcx = cx + static_cast<float>(col) * colW + (colW * 0.5f);
                float kcy = cy + 32.0f + 34.0f + static_cast<float>(row) * 72.0f;
                if (std::hypot(mx - kcx, my - kcy) <= 20.0f) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::InstrumentKnob;
                    res.index = static_cast<int>(k);
                    return res;
                }
            }
        }
    }

    // Chord Follow Hits
    if (chordFollowBounds_.contains(mx, my)) {
        float cx = chordFollowBounds_.x;
        float cy = chordFollowBounds_.y;
        float cw = chordFollowBounds_.w;
        float chipW = (cw - 170.0f) / 5.0f;
        float chipH = 24.0f;
        float chipY = cy + 32.0f;

        for (int i = 0; i < 5; ++i) {
            float chipX = cx + 12.0f + static_cast<float>(i) * (chipW + 5.0f);
            if (mx >= chipX && mx <= chipX + chipW && my >= chipY && my <= chipY + chipH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ChordFollowChip;
                res.index = i;
                return res;
            }
        }

        float bakeW = 100.0f;
        float bakeX = cx + cw - bakeW - 12.0f;
        if (mx >= bakeX && mx <= bakeX + bakeW && my >= chipY && my <= chipY + chipH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::BakeChords;
            return res;
        }
    }

    // MIDI FX Hits
    if (midiFxBounds_.contains(mx, my)) {
        float cx = midiFxBounds_.x;
        float cy = midiFxBounds_.y;
        float cw = midiFxBounds_.w;
        float addBtnW = 95.0f;
        float addBtnX = cx + cw - addBtnW - 10.0f;
        if (mx >= addBtnX && mx <= addBtnX + addBtnW && my >= cy + 4.0f && my <= cy + 28.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::AddMidiFx;
            return res;
        }

        float itemH = 28.0f;
        float itemGap = 5.0f;
        for (size_t mi = 0; mi < data.midiFx.size() && mi < 2; ++mi) {
            float itemY = cy + 32.0f + static_cast<float>(mi) * (itemH + itemGap);
            // Delete button [X]
            if (mx >= cx + cw - 32.0f && mx <= cx + cw - 10.0f && my >= itemY && my <= itemY + itemH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RemoveMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }
            // Fullscreen FX [⛶]
            if (mx >= cx + cw - 60.0f && mx <= cx + cw - 34.0f && my >= itemY && my <= itemY + itemH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FullscreenMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }
            // Toggle / select item
            if (mx >= cx + 10.0f && mx <= cx + cw - 64.0f && my >= itemY && my <= itemY + itemH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }
        }
    }

    // Audio FX Hits
    if (audioFxBounds_.contains(mx, my)) {
        float cx = audioFxBounds_.x;
        float cy = audioFxBounds_.y;
        float cw = audioFxBounds_.w;
        float addBtnW = 85.0f;
        float addBtnX = cx + cw - addBtnW - 10.0f;
        if (mx >= addBtnX && mx <= addBtnX + addBtnW && my >= cy + 4.0f && my <= cy + 28.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::AddAudioFx;
            return res;
        }

        float itemH = 28.0f;
        float itemGap = 5.0f;
        for (size_t fi = 0; fi < data.audioFx.size() && fi < 2; ++fi) {
            float itemY = cy + 32.0f + static_cast<float>(fi) * (itemH + itemGap);
            // Delete button [X]
            if (mx >= cx + cw - 32.0f && mx <= cx + cw - 10.0f && my >= itemY && my <= itemY + itemH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RemoveAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }
            // Fullscreen FX [⛶]
            if (mx >= cx + cw - 60.0f && mx <= cx + cw - 34.0f && my >= itemY && my <= itemY + itemH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FullscreenAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }
            // Toggle / select item
            if (mx >= cx + 10.0f && mx <= cx + cw - 64.0f && my >= itemY && my <= itemY + itemH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }
        }
    }

    return res;
}

bool TrackPropertiesPanel::handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data,
                                         const ViewContext& ctx) {
    data.syncKnobsIfEmpty();

    // 0. Forward to embedded PluginSearchDialog if open
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handlePointer(ev)) return true;
        return true;
    }

    // Mouse Wheel Scrolling
    if (ev.action == PointerAction::Scroll) {
        float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
        scrollY_ = std::clamp(scrollY_ - ev.scrollY * 28.0f, 0.0f, maxScroll);
        if (onScrollChanged) onScrollChanged(scrollY_);
        return true;
    }

    // Pointer Down
    if (ev.action == PointerAction::Down) {
        // Check Scrollbar Hit
        if (scrollbarBounds_.contains(ev.x, ev.y)) {
            dragMode_ = DragMode::Scrollbar;
            dragStartY_ = ev.y;
            dragStartScrollY_ = scrollY_;
            return true;
        }

        auto hit = hitTest(ev.x, ev.y, data);
        if (hit.hit) {
            if (showTrackRibbon_ && hit.area == TrackPropertiesHitArea::None) {
                // Ribbon tab selection
                if (onTrackSelected) onTrackSelected(static_cast<uint32_t>(hit.index));
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::RenameButton) {
                if (ctx.onOpenValueEdit) {
                    ValueEditRequest req;
                    req.title = "EDIT TRACK PROPERTIES";
                    req.paramName = "Track Name";
                    req.isTextMode = true;
                    req.initialText = data.trackName;
                    req.currentIconRef = data.iconRef;
                    req.accentColor = Color(data.r, data.g, data.b, 1.0f);
                    if (onChooseTrackIcon) {
                        req.actionLinkLabel = "🎨 Choose Track Icon...";
                        req.onActionLink = [this, trackIdx = data.trackIndex]() {
                            if (onChooseTrackIcon) onChooseTrackIcon(trackIdx);
                        };
                    }
                    req.onCommitText = [this, &data, trackIdx = data.trackIndex](const std::string& newName) {
                        if (!newName.empty()) {
                            data.trackName = newName;
                            if (trackIdx < data.allTrackNames.size()) {
                                data.allTrackNames[trackIdx] = newName;
                            }
                            if (onTrackRenameWithText) {
                                onTrackRenameWithText(trackIdx, newName);
                            }
                        }
                    };
                    ctx.onOpenValueEdit(req);
                }
                if (onTrackRename) onTrackRename(data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::ColorSwatch) {
                if (hit.index >= 0 && hit.index < 8) {
                    data.r = kQuickPalette[hit.index][0];
                    data.g = kQuickPalette[hit.index][1];
                    data.b = kQuickPalette[hit.index][2];
                    if (onColorChanged) onColorChanged(data.trackIndex, data.r, data.g, data.b);
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::CodeButton) {
                if (onOpenCodeEditor) onOpenCodeEditor(data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::MuteButton) {
                data.mute = !data.mute;
                if (onMuteToggled) onMuteToggled(data.trackIndex, data.mute);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::SoloButton) {
                data.solo = !data.solo;
                if (onSoloToggled) onSoloToggled(data.trackIndex, data.solo);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::FreezeButton) {
                data.freeze = !data.freeze;
                if (onFreezeToggled) onFreezeToggled(data.trackIndex, data.freeze);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::VolumeSlider) {
                if (ev.button == PointerButton::Right) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = data.trackName + " VOLUME";
                        req.paramName = "Volume";
                        req.currentValue = data.volume;
                        req.minValue = 0.0f;
                        req.maxValue = 1.5f;
                        req.defaultValue = 0.8f;
                        req.hasDefault = true;
                        req.allowPercentage = true;
                        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
                        req.onCommit = [this, &data](float val) {
                            data.volume = val;
                            if (onVolumeChanged) onVolumeChanged(data.trackIndex, val);
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }
                dragMode_ = DragMode::VolumeSlider;
                data.volume = hit.normVal * 1.5f;
                if (onVolumeChanged) onVolumeChanged(data.trackIndex, data.volume);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::PanKnob) {
                if (ev.button == PointerButton::Right) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = data.trackName + " PAN";
                        req.paramName = "Pan";
                        req.currentValue = data.pan;
                        req.minValue = -1.0f;
                        req.maxValue = 1.0f;
                        req.defaultValue = 0.0f;
                        req.hasDefault = true;
                        req.allowPercentage = false;
                        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
                        req.onCommit = [this, &data](float val) {
                            data.pan = val;
                            if (onPanChanged) onPanChanged(data.trackIndex, val);
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }
                dragMode_ = DragMode::PanKnob;
                dragStartY_ = ev.y;
                dragStartVal_ = data.pan;
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::InstrumentPrevPreset) {
                if (onPrevPreset) onPrevPreset();
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::InstrumentNextPreset) {
                if (onNextPreset) onNextPreset();
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::FullscreenInstrument) {
                if (onOpenFullscreenDevice) onOpenFullscreenDevice(data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::FullscreenMidiFx) {
                if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(data.trackIndex, static_cast<size_t>(hit.index));
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::FullscreenAudioFx) {
                if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(data.trackIndex, static_cast<size_t>(hit.index));
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::ChangeInstrument) {
                pluginDialog_.open(PluginDialogMode::AddInstrument, data.trackName, data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::InstrumentKnob) {
                dragMode_ = DragMode::InstrumentKnob;
                activeKnobIndex_ = hit.index;
                dragStartY_ = ev.y;
                dragStartVal_ = (activeKnobIndex_ < static_cast<int>(data.knobs.size()))
                                    ? data.knobs[activeKnobIndex_].value
                                    : 0.5f;
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::ChordFollowChip) {
                ChordFollowMode modes[5] = {ChordFollowMode::Off, ChordFollowMode::Chord, ChordFollowMode::Bass, ChordFollowMode::Scale, ChordFollowMode::ColorLead};
                if (hit.index >= 0 && hit.index < 5) {
                    data.chordFollowMode = modes[hit.index];
                    if (onChordFollowChanged) onChordFollowChanged(data.trackIndex, data.chordFollowMode);
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::BakeChords) {
                if (onBakeChords) onBakeChords(data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::AddMidiFx) {
                pluginDialog_.open(PluginDialogMode::AddMidiFx, data.trackName, data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::AddAudioFx) {
                pluginDialog_.open(PluginDialogMode::AddAudioFx, data.trackName, data.trackIndex);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::RemoveMidiFx) {
                if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
                    if (onRemoveMidiFx) onRemoveMidiFx(data.trackIndex, static_cast<size_t>(hit.index));
                    data.midiFx.erase(data.midiFx.begin() + hit.index);
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::RemoveAudioFx) {
                if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
                    if (onRemoveAudioFx) onRemoveAudioFx(data.trackIndex, static_cast<size_t>(hit.index));
                    data.audioFx.erase(data.audioFx.begin() + hit.index);
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::ToggleMidiFx) {
                if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
                    data.midiFx[hit.index].enabled = !data.midiFx[hit.index].enabled;
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::ToggleAudioFx) {
                if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
                    data.audioFx[hit.index].enabled = !data.audioFx[hit.index].enabled;
                }
                return true;
            }
        }
    }

    // Pointer Move
    if (ev.action == PointerAction::Move) {
        if (dragMode_ == DragMode::Scrollbar) {
            float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
            if (maxScroll > 0.0f) {
                float dy = ev.y - dragStartY_;
                float scrollDelta = (dy / scrollbarBounds_.h) * totalContentHeight_;
                scrollY_ = std::clamp(dragStartScrollY_ + scrollDelta, 0.0f, maxScroll);
                if (onScrollChanged) onScrollChanged(scrollY_);
            }
            return true;
        }

        if (dragMode_ == DragMode::VolumeSlider) {
            float cx = mixerCardBounds_.x;
            float cw = mixerCardBounds_.w;
            bool isWide = (bounds_.w >= 560.0f);
            float vLabelX = cx + 14.0f;
            float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
            float panAreaW = isWide ? 85.0f : 74.0f;
            float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
            if (vTrackW < 50.0f) vTrackW = 50.0f;

            float norm = std::clamp((ev.x - vTrackX) / vTrackW, 0.0f, 1.0f);
            data.volume = norm * 1.5f;
            if (onVolumeChanged) onVolumeChanged(data.trackIndex, data.volume);
            return true;
        }

        if (dragMode_ == DragMode::PanKnob) {
            float dy = dragStartY_ - ev.y;
            data.pan = std::clamp(dragStartVal_ + (dy / 100.0f), -1.0f, 1.0f);
            if (onPanChanged) onPanChanged(data.trackIndex, data.pan);
            return true;
        }

        if (dragMode_ == DragMode::InstrumentKnob && activeKnobIndex_ >= 0 &&
            activeKnobIndex_ < static_cast<int>(data.knobs.size())) {
            float dy = dragStartY_ - ev.y;
            float newVal = std::clamp(dragStartVal_ + (dy / 150.0f), 0.0f, 1.0f);
            data.knobs[activeKnobIndex_].value = newVal;
            data.knobs[activeKnobIndex_].display = std::to_string(static_cast<int>(std::round(newVal * 100.0f))) + "%";
            if (onParamChanged) {
                onParamChanged(data.trackIndex, data.knobs[activeKnobIndex_].name, newVal);
            }
            return true;
        }
    }

    // Pointer Up or Cancel
    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        if (dragMode_ != DragMode::None) {
            dragMode_ = DragMode::None;
            activeKnobIndex_ = -1;
            return true;
        }
    }

    return false;
}

bool TrackPropertiesPanel::handleKey(int key, int scancode, int action, int mods,
                                     [[maybe_unused]] const ViewContext& ctx) {
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handleKey(key, scancode, action, mods)) return true;
    }
    return false;
}

} // namespace eatsbits::ui
