#include "eatsbits/ui/views/track_inspector_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

static const float kQuickSwatches[8][3] = {
    {0.13f, 0.96f, 0.91f}, // Neon Cyan
    {1.0f,  0.55f, 0.0f }, // Neon Amber
    {0.0f,  1.0f,  0.40f}, // Acid Green
    {1.0f,  0.0f,  0.48f}, // Hot Pink
    {0.74f, 0.0f,  1.0f }, // Electric Purple
    {1.0f,  0.20f, 0.20f}, // Crimson Red
    {1.0f,  0.90f, 0.0f }, // Gold Yellow
    {0.20f, 0.60f, 1.0f }  // Sky Blue
};

TrackInspectorView::TrackInspectorView() {
    initDefaultTracks();

    // Configure embedded PluginSearchDialog
    pluginDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (trackIndex < tracks_.size()) {
            auto& trk = tracks_[trackIndex];
            if (mode == PluginDialogMode::AddInstrument) {
                trk.instrument = entry.name;
                trk.instrumentEngine = entry.engineTag;
                trk.r = entry.r;
                trk.g = entry.g;
                trk.b = entry.b;
                syncKnobsForTrack(trk);
                if (onChangeInstrument) onChangeInstrument(trackIndex);
            } else if (mode == PluginDialogMode::AddMidiFx) {
                trk.midiFx.arpEnabled = true;
                if (onAddMidiFx) onAddMidiFx(trackIndex);
            } else if (mode == PluginDialogMode::AddAudioFx) {
                if (entry.engineTag == "delay") trk.audioFx.delayEnabled = true;
                else if (entry.engineTag == "chorus") trk.audioFx.chorusEnabled = true;
                else if (entry.engineTag == "convolver") trk.audioFx.convolverEnabled = true;
                else if (entry.engineTag == "eq") trk.audioFx.eqEnabled = true;
                else if (entry.engineTag == "comp") trk.audioFx.compEnabled = true;
                if (onAddAudioFx) onAddAudioFx(trackIndex);
            }
        }
    };

    // Initialize layout early so headless tests and interactions have valid geometry
    layout(Rect2D{0.0f, 56.0f, 1280.0f, 744.0f}, ViewContext{});
}

void TrackInspectorView::initDefaultTracks() {
    tracks_.clear();

    // 1. 303 Acid Bass
    InspectorTrackChannel t1;
    t1.name = "303 Acid Bass";
    t1.type = "SYNTH";
    t1.r = 1.0f; t1.g = 0.55f; t1.b = 0.0f;
    t1.volume = 0.85f;
    t1.pan = -0.10f;
    t1.mute = false;
    t1.solo = false;
    t1.freeze = false;
    t1.instrument = "Roland TB-303";
    t1.instrumentEngine = "tb303";
    t1.chordFollowMode = ChordFollowMode::Off;
    t1.knobs = {
        {"tuning", "TUNING", 0.50f, "440 Hz"},
        {"cutoff", "CUTOFF", 0.65f, "1.8 kHz"},
        {"resonance", "RESON", 0.80f, "80%"},
        {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
        {"decay", "DECAY", 0.45f, "240 ms"},
        {"accent", "ACCENT", 0.85f, "85%"}
    };
    t1.midiFx.arpEnabled = true;
    t1.midiFx.arpPattern = 2; // UpDown
    t1.midiFx.scaleSnapEnabled = true;
    t1.midiFx.humanizeEnabled = true;
    t1.audioFx.delayEnabled = true;
    t1.audioFx.delayTime = 0.375f;
    t1.audioFx.convolverEnabled = true;
    tracks_.push_back(t1);

    // 2. TR-808 Kit
    InspectorTrackChannel t2;
    t2.name = "TR-808 Kit";
    t2.type = "DRUMS";
    t2.r = 0.13f; t2.g = 0.96f; t2.b = 0.91f;
    t2.volume = 0.90f;
    t2.pan = 0.0f;
    t2.instrument = "Analog 808 Rhythm";
    t2.instrumentEngine = "tr808";
    t2.knobs = {
        {"tone", "TONE", 0.70f, "70%"},
        {"snappy", "SNAPPY", 0.80f, "80%"},
        {"decay", "DECAY", 0.60f, "600 ms"},
        {"tuning", "TUNING", 0.50f, "55 Hz"},
        {"drive", "DRIVE", 0.35f, "15%"},
        {"punch", "PUNCH", 0.75f, "+3 dB"}
    };
    t2.audioFx.compEnabled = true;
    tracks_.push_back(t2);

    // 3. TR-909 Drive
    InspectorTrackChannel t3;
    t3.name = "TR-909 Drive";
    t3.type = "DRUMS";
    t3.r = 1.0f; t3.g = 0.0f; t3.b = 0.48f;
    t3.volume = 0.82f;
    t3.pan = 0.10f;
    t3.instrument = "Analog 909 Groove";
    t3.instrumentEngine = "tr909";
    t3.knobs = {
        {"attack", "ATTACK", 0.85f, "12 ms"},
        {"punch", "PUNCH", 0.65f, "+4 dB"},
        {"tune", "TUNE", 0.50f, "62 Hz"},
        {"crack", "CRACK", 0.60f, "60%"},
        {"decay", "DECAY", 0.55f, "450 ms"},
        {"snap", "SNAP", 0.75f, "75%"}
    };
    tracks_.push_back(t3);

    // 4. DX7 Rhodes
    InspectorTrackChannel t4;
    t4.name = "DX7 Rhodes";
    t4.type = "SYNTH";
    t4.r = 0.74f; t4.g = 0.0f; t4.b = 1.0f;
    t4.volume = 0.78f;
    t4.pan = 0.20f;
    t4.instrument = "Yamaha DX7 FM";
    t4.instrumentEngine = "dx7";
    t4.knobs = {
        {"algo", "ALGO", 0.35f, "Algo 5"},
        {"feedback", "FEEDBACK", 0.65f, "Lvl 6"},
        {"attack", "ATTACK", 0.20f, "8 ms"},
        {"decay", "DECAY", 0.70f, "1.4 s"},
        {"bright", "BRIGHT", 0.60f, "60%"},
        {"detune", "DETUNE", 0.25f, "+12 ct"}
    };
    t4.audioFx.chorusEnabled = true;
    tracks_.push_back(t4);

    // 5. Concert Grand
    InspectorTrackChannel t5;
    t5.name = "Concert Grand";
    t5.type = "PHYSICAL";
    t5.r = 0.20f; t5.g = 0.60f; t5.b = 1.0f;
    t5.volume = 0.80f;
    t5.pan = -0.15f;
    t5.instrument = "Physical Grand Piano";
    t5.instrumentEngine = "piano";
    t5.knobs = {
        {"hammer", "HAMMER", 0.60f, "Medium"},
        {"string", "STRING", 0.75f, "Steel"},
        {"damper", "DAMPER", 0.45f, "45%"},
        {"resonance", "RESON", 0.80f, "80%"},
        {"decay", "DECAY", 0.85f, "3.2 s"},
        {"warmth", "WARMTH", 0.50f, "50%"}
    };
    t5.audioFx.convolverEnabled = true;
    tracks_.push_back(t5);

    selectedTrackIndex_ = 0;
}

void TrackInspectorView::syncKnobsForTrack(InspectorTrackChannel& trk) {
    trk.knobs.clear();
    if (trk.instrumentEngine == "tb303") {
        trk.knobs = {
            {"tuning", "TUNING", 0.50f, "440 Hz"},
            {"cutoff", "CUTOFF", 0.65f, "1.8 kHz"},
            {"resonance", "RESON", 0.80f, "80%"},
            {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
            {"decay", "DECAY", 0.45f, "240 ms"},
            {"accent", "ACCENT", 0.85f, "85%"}
        };
    } else if (trk.instrumentEngine == "tr808") {
        trk.knobs = {
            {"tone", "TONE", 0.70f, "70%"},
            {"snappy", "SNAPPY", 0.80f, "80%"},
            {"decay", "DECAY", 0.60f, "600 ms"},
            {"tuning", "TUNING", 0.50f, "55 Hz"},
            {"drive", "DRIVE", 0.35f, "15%"},
            {"punch", "PUNCH", 0.75f, "+3 dB"}
        };
    } else if (trk.instrumentEngine == "tr909") {
        trk.knobs = {
            {"attack", "ATTACK", 0.85f, "12 ms"},
            {"punch", "PUNCH", 0.65f, "+4 dB"},
            {"tune", "TUNE", 0.50f, "62 Hz"},
            {"crack", "CRACK", 0.60f, "60%"},
            {"decay", "DECAY", 0.55f, "450 ms"},
            {"snap", "SNAP", 0.75f, "75%"}
        };
    } else if (trk.instrumentEngine == "dx7") {
        trk.knobs = {
            {"algo", "ALGO", 0.35f, "Algo 5"},
            {"feedback", "FEEDBACK", 0.65f, "Lvl 6"},
            {"attack", "ATTACK", 0.20f, "8 ms"},
            {"decay", "DECAY", 0.70f, "1.4 s"},
            {"bright", "BRIGHT", 0.60f, "60%"},
            {"detune", "DETUNE", 0.25f, "+12 ct"}
        };
    } else {
        trk.knobs = {
            {"param1", "PARAM 1", 0.50f, "50%"},
            {"param2", "PARAM 2", 0.60f, "60%"},
            {"param3", "PARAM 3", 0.45f, "45%"},
            {"param4", "PARAM 4", 0.70f, "70%"},
            {"decay",  "DECAY",   0.55f, "55%"},
            {"output", "OUTPUT",  0.80f, "80%"}
        };
    }
}

void TrackInspectorView::setActiveTrack(uint32_t idx) noexcept {
    if (tracks_.empty()) return;
    selectedTrackIndex_ = std::min(idx, static_cast<uint32_t>(tracks_.size() - 1));
}

const InspectorTrackChannel& TrackInspectorView::getTrack(size_t idx) const {
    if (idx < tracks_.size()) return tracks_[idx];
    return tracks_[0];
}

InspectorTrackChannel& TrackInspectorView::getTrack(size_t idx) {
    if (idx < tracks_.size()) return tracks_[idx];
    return tracks_[0];
}

void TrackInspectorView::setAudioScopeBuffer(const float* buffer, size_t count) {
    if (!buffer || count == 0) return;
    size_t copyCount = std::min(count, static_cast<size_t>(256));
    std::copy_n(buffer, copyCount, scopeBuffer_);
}

void TrackInspectorView::setPresetInfo(size_t activePresetIdx, size_t totalPresets,
                                      const std::string& presetTitle, const std::string& presetSubtitle) {
    activePresetIdx_ = activePresetIdx;
    totalPresets_ = std::max(totalPresets, static_cast<size_t>(1));
    presetTitle_ = presetTitle;
    presetSubtitle_ = presetSubtitle;
}

void TrackInspectorView::syncFromWindow(
    const std::vector<std::string>& trackNames,
    const std::vector<float>& trackVols,
    const std::vector<float>& trackPans,
    const std::vector<bool>& trackMutes,
    const std::vector<bool>& trackSolos,
    const std::vector<bool>& trackFreezes,
    const std::vector<Color>& trackColors,
    uint32_t selectedTrackIdx,
    const std::string& activePresetTitle,
    const std::string& activePresetSubtitle,
    size_t activePresetIdx,
    size_t totalPresets,
    float scrollY) {

    size_t count = std::max(tracks_.size(), trackNames.size());
    if (tracks_.size() < count) tracks_.resize(count);

    for (size_t i = 0; i < count; ++i) {
        if (i < trackNames.size()) tracks_[i].name = trackNames[i];
        if (i < trackVols.size()) tracks_[i].volume = trackVols[i];
        if (i < trackPans.size()) tracks_[i].pan = trackPans[i];
        if (i < trackMutes.size()) tracks_[i].mute = trackMutes[i];
        if (i < trackSolos.size()) tracks_[i].solo = trackSolos[i];
        if (i < trackFreezes.size()) tracks_[i].freeze = trackFreezes[i];
        if (i < trackColors.size()) {
            tracks_[i].r = trackColors[i].r;
            tracks_[i].g = trackColors[i].g;
            tracks_[i].b = trackColors[i].b;
        }
    }

    if (selectedTrackIdx < tracks_.size()) {
        selectedTrackIndex_ = selectedTrackIdx;
    }

    if (!activePresetTitle.empty()) presetTitle_ = activePresetTitle;
    if (!activePresetSubtitle.empty()) presetSubtitle_ = activePresetSubtitle;
    activePresetIdx_ = activePresetIdx;
    totalPresets_ = std::max(totalPresets, static_cast<size_t>(1));
    scrollY_ = scrollY;
}

void TrackInspectorView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    float padding = ctx.isMobile ? 8.0f : 16.0f;
    float contentW = bounds_.w - (padding * 2.0f);
    float startY = bounds_.y + 4.0f;
    float curY = startY - scrollY_;

    // 0. Top Track Selector Ribbon
    float ribbonH = 34.0f;
    trackRibbonBounds_ = Rect2D(bounds_.x + padding, curY, contentW, ribbonH);
    curY += ribbonH + 8.0f;

    // 1. Track Identity Header Card
    float headerH = 64.0f;
    headerCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, headerH);
    curY += headerH + 12.0f;

    // 2. Channel Mixer Quick Controls Card
    float mixerH = 72.0f;
    mixerCardBounds_ = Rect2D(bounds_.x + padding, curY, contentW, mixerH);
    curY += mixerH + 12.0f;

    // 3. Dynamic Instrument Faceplate Card
    float faceplateH = 265.0f;
    faceplateBounds_ = Rect2D(bounds_.x + padding, curY, contentW, faceplateH);
    curY += faceplateH + 12.0f;

    // 4. Harmonic Chord Track Follow Settings Card
    float chordFollowH = 88.0f;
    chordFollowBounds_ = Rect2D(bounds_.x + padding, curY, contentW, chordFollowH);
    curY += chordFollowH + 12.0f;

    // 5. MIDI FX Pipeline Rack Card
    float midiFxH = 124.0f;
    midiFxBounds_ = Rect2D(bounds_.x + padding, curY, contentW, midiFxH);
    curY += midiFxH + 12.0f;

    // 6. Audio FX Insert Rack Card
    float audioFxH = 138.0f;
    audioFxBounds_ = Rect2D(bounds_.x + padding, curY, contentW, audioFxH);
    curY += audioFxH + 20.0f;

    totalContentHeight_ = curY - (startY - scrollY_);
    needScrollbar_ = (totalContentHeight_ > bounds_.h);
    scrollbarBounds_ = Rect2D(bounds_.x + bounds_.w - 10.0f, bounds_.y + 4.0f, 6.0f, bounds_.h - 8.0f);

    // Layout embedded plugin dialog
    pluginDialog_.layout(ctx.logicalWidth, ctx.logicalHeight);
}

void TrackInspectorView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // Background container fill
    drawRect(r, bounds_.x, bounds_.y, bounds_.w, bounds_.h,
             theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 1.0f);

    renderTrackSelectorRibbon(r, theme);
    renderHeaderCard(r, theme);
    renderMixerControlsCard(r, theme);
    renderDynamicFaceplateCard(r, theme);
    renderChordFollowCard(r, theme);
    renderMidiFxCard(r, theme);
    renderAudioFxCard(r, theme);

    if (needScrollbar_) {
        renderScrollbar(r, theme);
    }

    // Modal PluginSearchDialog on top when open
    if (pluginDialog_.isOpen()) {
        pluginDialog_.render(r, theme);
    }
}

void TrackInspectorView::renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (trackRibbonBounds_.y + trackRibbonBounds_.h < bounds_.y || trackRibbonBounds_.y > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, trackRibbonBounds_.x, trackRibbonBounds_.y, trackRibbonBounds_.w, trackRibbonBounds_.h, 5.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.95f);
    drawRoundedRectOutline(r, trackRibbonBounds_.x, trackRibbonBounds_.y, trackRibbonBounds_.w, trackRibbonBounds_.h, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    const float startX = 40.0f;
    const float tabW = 120.0f;
    const float tabGap = 8.0f;
    const float tabY = trackRibbonBounds_.y + 2.0f;
    const float tabH = 30.0f;

    for (size_t i = 0; i < tracks_.size(); ++i) {
        float tx = startX + static_cast<float>(i) * (tabW + tabGap);
        if (tx + tabW > trackRibbonBounds_.x + trackRibbonBounds_.w - 8.0f) break;

        const auto& trk = tracks_[i];
        bool isSel = (i == selectedTrackIndex_);

        if (isSel) {
            drawRoundedRect(r, tx, tabY, tabW, tabH, 4.0f,
                            trk.r * 0.25f, trk.g * 0.25f, trk.b * 0.25f, 0.95f);
            drawRoundedRectOutline(r, tx, tabY, tabW, tabH, 4.0f,
                                   trk.r, trk.g, trk.b, 1.0f, 1.5f);
        } else {
            drawRoundedRect(r, tx, tabY, tabW, tabH, 4.0f,
                            theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.70f);
            drawRoundedRectOutline(r, tx, tabY, tabW, tabH, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.40f, 1.0f);
        }

        // Color dot
        drawCircle(r, tx + 12.0f, tabY + 15.0f, 4.5f, trk.r, trk.g, trk.b, 1.0f);

        // Track name
        std::string label = "TRK " + std::to_string(i + 1) + ": " + trk.name;
        if (label.size() > 14) {
            label = label.substr(0, 12) + "..";
        }
        drawCenteredText(r, label, tx + 18.0f, tabY, tabW - 22.0f, tabH, 10.5f,
                         isSel ? 1.0f : theme.textMuted.r,
                         isSel ? 1.0f : theme.textMuted.g,
                         isSel ? 1.0f : theme.textMuted.b, 1.0f);
    }
}

void TrackInspectorView::renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (headerCardBounds_.y + headerCardBounds_.h < bounds_.y || headerCardBounds_.y > bounds_.y + bounds_.h) return;

    const auto& trk = getTrack(selectedTrackIndex_);

    // Background Card
    drawRoundedRect(r, headerCardBounds_.x, headerCardBounds_.y, headerCardBounds_.w, headerCardBounds_.h, 6.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, headerCardBounds_.x, headerCardBounds_.y, headerCardBounds_.w, headerCardBounds_.h, 6.0f,
                           trk.r * 0.7f + 0.1f, trk.g * 0.7f + 0.1f, trk.b * 0.7f + 0.1f, 0.85f, 1.4f);

    // Left Accent Pill
    drawRoundedRect(r, headerCardBounds_.x + 10.0f, headerCardBounds_.y + 10.0f, 10.0f, headerCardBounds_.h - 20.0f, 4.0f,
                    trk.r, trk.g, trk.b, 1.0f);

    // Track Name & Channel Type Subtitle
    std::string upperName = trk.name;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);
    drawText(r, upperName, headerCardBounds_.x + 28.0f, headerCardBounds_.y + 12.0f, 15.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);

    std::string subText = "TRACK CHANNEL " + std::to_string(selectedTrackIndex_ + 1) + " • " +
                          trk.type + " (DOUBLE-CLICK FOR CODE)";
    drawText(r, subText, headerCardBounds_.x + 28.0f, headerCardBounds_.y + 36.0f, 10.0f,
             trk.r, trk.g, trk.b, 0.9f);

    // 8 Color Swatches in the Center
    float swatchStartX = headerCardBounds_.x + 310.0f;
    float swatchY = headerCardBounds_.y + 24.0f;
    for (uint32_t s = 0; s < 8; ++s) {
        float sx = swatchStartX + static_cast<float>(s) * 24.0f;
        drawCircle(r, sx + 8.0f, swatchY + 8.0f, 7.0f,
                   kQuickSwatches[s][0], kQuickSwatches[s][1], kQuickSwatches[s][2], 1.0f);

        bool isCurrentColor = (std::abs(trk.r - kQuickSwatches[s][0]) < 0.06f &&
                               std::abs(trk.g - kQuickSwatches[s][1]) < 0.06f);
        if (isCurrentColor) {
            drawCircleOutline(r, sx + 8.0f, swatchY + 8.0f, 9.5f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f);
        }
    }

    // Action Buttons on Right: [ CODE ], MUTE, SOLO, FREEZE
    float btnY = headerCardBounds_.y + 17.0f;
    float btnH = 30.0f;

    // [ < > CODE ] Button
    float codeW = 74.0f;
    float codeX = headerCardBounds_.x + headerCardBounds_.w - 265.0f;
    drawButton(r, Rect2D{codeX, btnY, codeW, btnH}, "[ < > CODE ]",
               theme.panelHeader, theme.primaryAccent, theme.primaryAccent, 10.0f, 4.0f, 1.2f);

    // MUTE Button
    float muteW = 54.0f;
    float muteX = headerCardBounds_.x + headerCardBounds_.w - 185.0f;
    Color muteBg = trk.mute ? Color{0.88f, 0.22f, 0.16f, 1.0f} : theme.panelHeader;
    Color muteBorder = trk.mute ? Color{1.0f, 0.4f, 0.3f, 1.0f} : theme.borderSubtle;
    Color muteText = trk.mute ? Color{1.0f, 1.0f, 1.0f, 1.0f} : theme.textMuted;
    drawButton(r, Rect2D{muteX, btnY, muteW, btnH}, "MUTE",
               muteBg, muteBorder, muteText, 11.0f, 4.0f, 1.0f);

    // SOLO Button
    float soloW = 54.0f;
    float soloX = headerCardBounds_.x + headerCardBounds_.w - 125.0f;
    Color soloBg = trk.solo ? Color{0.98f, 0.78f, 0.12f, 1.0f} : theme.panelHeader;
    Color soloBorder = trk.solo ? Color{1.0f, 0.9f, 0.3f, 1.0f} : theme.borderSubtle;
    Color soloText = trk.solo ? Color{0.05f, 0.05f, 0.08f, 1.0f} : theme.textMuted;
    drawButton(r, Rect2D{soloX, btnY, soloW, btnH}, "SOLO",
               soloBg, soloBorder, soloText, 11.0f, 4.0f, 1.0f);

    // FREEZE Button
    float fzW = 60.0f;
    float fzX = headerCardBounds_.x + headerCardBounds_.w - 65.0f;
    Color fzBg = trk.freeze ? Color{0.0f, 0.88f, 0.95f, 0.9f} : theme.panelHeader;
    Color fzBorder = trk.freeze ? Color{0.4f, 1.0f, 1.0f, 1.0f} : theme.borderSubtle;
    Color fzText = trk.freeze ? Color{0.05f, 0.08f, 0.12f, 1.0f} : theme.textMuted;
    drawButton(r, Rect2D{fzX, btnY, fzW, btnH}, trk.freeze ? "FROZEN" : "FREEZE",
               fzBg, fzBorder, fzText, 10.5f, 4.0f, 1.0f);
}

void TrackInspectorView::renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (mixerCardBounds_.y + mixerCardBounds_.h < bounds_.y || mixerCardBounds_.y > bounds_.y + bounds_.h) return;

    const auto& trk = getTrack(selectedTrackIndex_);

    drawRoundedRect(r, mixerCardBounds_.x, mixerCardBounds_.y, mixerCardBounds_.w, mixerCardBounds_.h, 6.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.95f);
    drawRoundedRectOutline(r, mixerCardBounds_.x, mixerCardBounds_.y, mixerCardBounds_.w, mixerCardBounds_.h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    drawText(r, "CHANNEL MIXER SETTINGS", mixerCardBounds_.x + 16.0f, mixerCardBounds_.y + 10.0f, 11.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Left: Volume Slider
    float vLabelX = mixerCardBounds_.x + 24.0f;
    float vTrackX = mixerCardBounds_.x + 85.0f;
    float vTrackW = (mixerCardBounds_.w * 0.45f) - 90.0f;
    float sY = mixerCardBounds_.y + 36.0f;
    float sH = 18.0f;

    drawText(r, "VOLUME", vLabelX, sY + 3.0f, 11.0f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    drawRoundedRect(r, vTrackX, sY, vTrackW, sH, 4.0f, 0.06f, 0.07f, 0.09f, 1.0f);
    drawRoundedRectOutline(r, vTrackX, sY, vTrackW, sH, 4.0f, 0.20f, 0.22f, 0.28f, 1.0f, 1.0f);

    float normVol = std::clamp(trk.volume / 1.5f, 0.0f, 1.0f);
    if (normVol > 0.01f) {
        drawRoundedRect(r, vTrackX + 2.0f, sY + 2.0f, (vTrackW - 4.0f) * normVol, sH - 4.0f, 3.0f,
                        trk.r * 0.8f, trk.g * 0.8f, trk.b * 0.8f, 0.9f);
    }
    float volThumbX = vTrackX + normVol * vTrackW;
    drawRoundedRect(r, volThumbX - 6.0f, sY - 3.0f, 12.0f, 24.0f, 3.0f, 0.75f, 0.78f, 0.85f, 1.0f);
    drawRoundedRectOutline(r, volThumbX - 6.0f, sY - 3.0f, 12.0f, 24.0f, 3.0f, 0.25f, 0.28f, 0.35f, 1.0f, 1.0f);

    int volPct = static_cast<int>(std::round(trk.volume * 100.0f));
    drawText(r, std::to_string(volPct) + "%", vTrackX + vTrackW + 12.0f, sY + 3.0f, 11.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Right: Pan Slider
    float pStartX = mixerCardBounds_.x + mixerCardBounds_.w * 0.52f;
    drawText(r, "PAN", pStartX, sY + 3.0f, 11.0f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);

    float pTrackX = pStartX + 45.0f;
    float pTrackW = (mixerCardBounds_.w * 0.40f) - 60.0f;
    drawRoundedRect(r, pTrackX, sY, pTrackW, sH, 4.0f, 0.06f, 0.07f, 0.09f, 1.0f);
    drawRoundedRectOutline(r, pTrackX, sY, pTrackW, sH, 4.0f, 0.20f, 0.22f, 0.28f, 1.0f, 1.0f);

    // Center tick line
    drawLine(r, pTrackX + pTrackW * 0.5f, sY + 1.0f, pTrackX + pTrackW * 0.5f, sY + sH - 1.0f, 0.45f, 0.50f, 0.60f, 1.0f, 1.5f);

    float normPan = std::clamp((trk.pan + 1.0f) * 0.5f, 0.0f, 1.0f);
    float panThumbX = pTrackX + normPan * pTrackW;
    drawRoundedRect(r, panThumbX - 6.0f, sY - 3.0f, 12.0f, 24.0f, 3.0f, 0.75f, 0.78f, 0.85f, 1.0f);
    drawRoundedRectOutline(r, panThumbX - 6.0f, sY - 3.0f, 12.0f, 24.0f, 3.0f, 0.25f, 0.28f, 0.35f, 1.0f, 1.0f);

    std::string panStr = (std::abs(trk.pan) < 0.04f) ? "C" : ((trk.pan < 0.0f) ? "L" + std::to_string(static_cast<int>(std::round(-trk.pan * 100.0f))) : "R" + std::to_string(static_cast<int>(std::round(trk.pan * 100.0f))));
    drawText(r, panStr, pTrackX + pTrackW + 12.0f, sY + 3.0f, 11.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
}

void TrackInspectorView::renderDynamicFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (faceplateBounds_.y + faceplateBounds_.h < bounds_.y || faceplateBounds_.y > bounds_.y + bounds_.h) return;

    const auto& trk = getTrack(selectedTrackIndex_);
    const float pX = faceplateBounds_.x;
    const float pY = faceplateBounds_.y;
    const float pW = faceplateBounds_.w;
    const float pH = faceplateBounds_.h;

    // Background Chassis
    if (trk.instrumentEngine == "tb303") {
        drawRoundedRect(r, pX, pY, pW, pH, 6.0f, 0.82f, 0.82f, 0.80f, 1.0f);
        drawRoundedRectOutline(r, pX, pY, pW, pH, 6.0f, 0.45f, 0.45f, 0.45f, 1.0f, 2.0f);
    } else if (trk.instrumentEngine == "dx7") {
        drawRoundedRect(r, pX, pY, pW, pH, 6.0f, 0.12f, 0.13f, 0.15f, 1.0f);
        drawRoundedRectOutline(r, pX, pY, pW, pH, 6.0f, 0.0f, 0.66f, 0.53f, 1.0f, 2.0f);
    } else if (trk.instrumentEngine == "tr808" || trk.instrumentEngine == "tr909") {
        drawRoundedRect(r, pX, pY, pW, pH, 6.0f, 0.18f, 0.19f, 0.22f, 1.0f);
        drawRoundedRectOutline(r, pX, pY, pW, pH, 6.0f, 0.85f, 0.35f, 0.15f, 1.0f, 2.0f);
    } else {
        drawRoundedRect(r, pX, pY, pW, pH, 6.0f, 0.13f, 0.15f, 0.18f, 1.0f);
        drawRoundedRectOutline(r, pX, pY, pW, pH, 6.0f, trk.r * 0.7f, trk.g * 0.7f, trk.b * 0.7f, 1.0f, 1.8f);
    }

    // Corner mounting hex screws
    drawCircle(r, pX + 12.0f, pY + 12.0f, 3.5f, 0.35f, 0.38f, 0.45f, 1.0f);
    drawCircle(r, pX + pW - 12.0f, pY + 12.0f, 3.5f, 0.35f, 0.38f, 0.45f, 1.0f);
    drawCircle(r, pX + 12.0f, pY + pH - 12.0f, 3.5f, 0.35f, 0.38f, 0.45f, 1.0f);
    drawCircle(r, pX + pW - 12.0f, pY + pH - 12.0f, 3.5f, 0.35f, 0.38f, 0.45f, 1.0f);

    // Top Banner Plate (Height = 36px)
    float banH = 36.0f;
    drawRoundedRect(r, pX, pY, pW, banH, 6.0f, 0.08f, 0.09f, 0.12f, 0.95f);
    drawLine(r, pX, pY + banH, pX + pW, pY + banH, 0.25f, 0.28f, 0.36f, 1.0f, 1.2f);

    // < PREV and NEXT > Buttons
    float btnPrevX = pX + 12.0f;
    float btnNextX = pX + 75.0f;
    Color navBg{0.16f, 0.18f, 0.24f, 1.0f};
    Color navBorder{theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f};
    drawButton(r, Rect2D{btnPrevX, pY + 6.0f, 56.0f, 24.0f}, "< PREV",
               navBg, navBorder, theme.primaryAccent, 9.5f, 3.0f, 1.0f);

    drawButton(r, Rect2D{btnNextX, pY + 6.0f, 56.0f, 24.0f}, "NEXT >",
               navBg, navBorder, theme.primaryAccent, 9.5f, 3.0f, 1.0f);

    // Preset Counter
    std::string pCounter = std::to_string(activePresetIdx_ + 1) + "/" + std::to_string(totalPresets_);
    drawText(r, pCounter, pX + 142.0f, pY + 11.0f, 10.0f, 0.55f, 0.60f, 0.70f, 1.0f);

    // Instrument Title
    drawText(r, trk.instrument, pX + 215.0f, pY + 8.0f, 12.5f, 0.0f, 0.95f, 1.0f, 1.0f);
    drawText(r, "HARDWARE SCRIPT INTERFACE", pX + 215.0f, pY + 22.0f, 8.5f, 0.50f, 0.55f, 0.65f, 1.0f);

    // [ ⇄ CHANGE INSTRUMENT ] Button
    float chgW = 150.0f;
    float chgX = pX + pW - chgW - 14.0f;
    Color chgBg{0.16f, 0.20f, 0.28f, 1.0f};
    Color chgBorder{theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f};
    drawButton(r, Rect2D{chgX, pY + 6.0f, chgW, 24.0f}, "[ ⇄ CHANGE INSTRUMENT ]",
               chgBg, chgBorder, theme.primaryAccent, 9.5f, 3.0f, 1.2f);

    // Left Side: 6 Rotary Knobs
    bool isLightChassis = (trk.instrumentEngine == "tb303");
    float knobAreaW = pW - 240.0f; // Leaving 220px on the right for oscilloscope
    size_t kCount = std::min(trk.knobs.size(), static_cast<size_t>(6));
    float kStep = knobAreaW / static_cast<float>(std::max(size_t{1}, kCount));

    for (size_t i = 0; i < kCount; ++i) {
        float cx = pX + 24.0f + static_cast<float>(i) * kStep + (kStep * 0.5f);
        float cy = pY + 130.0f;
        float radius = 22.0f;

        const auto& knob = trk.knobs[i];

        // Drop shadow & collet well
        drawCircle(r, cx, cy + 2.0f, radius + 2.0f, 0.08f, 0.09f, 0.11f, 0.40f);
        drawCircle(r, cx, cy, radius, isLightChassis ? 0.28f : 0.16f, isLightChassis ? 0.28f : 0.18f, isLightChassis ? 0.30f : 0.22f, 1.0f);

        // Swept angle indicator arc
        constexpr float minA = -2.35619449f;
        constexpr float maxA = 2.35619449f;
        float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);

        drawCircle(r, cx, cy, radius - 3.0f, isLightChassis ? 0.75f : 0.22f, isLightChassis ? 0.75f : 0.24f, isLightChassis ? 0.74f : 0.28f, 1.0f);
        drawCircleOutline(r, cx, cy, radius, 0.45f, 0.48f, 0.55f, 1.0f, 1.2f);

        // Indicator needle
        float indX = cx + std::sin(curA) * (radius - 5.0f);
        float indY = cy - std::cos(curA) * (radius - 5.0f);
        drawLine(r, cx, cy, indX, indY, isLightChassis ? 0.90f : trk.r, isLightChassis ? 0.20f : trk.g, isLightChassis ? 0.15f : trk.b, 1.0f, 2.2f);

        // Center hub
        drawCircle(r, cx, cy, 5.0f, 0.20f, 0.22f, 0.25f, 1.0f);

        // Label and readout
        drawText(r, knob.label, cx - 18.0f, cy + radius + 12.0f, 9.5f,
                 isLightChassis ? 0.12f : 0.85f, isLightChassis ? 0.12f : 0.88f, isLightChassis ? 0.12f : 0.95f, 1.0f);
        drawText(r, knob.display, cx - 14.0f, cy + radius + 25.0f, 8.5f,
                 isLightChassis ? 0.30f : 0.55f, isLightChassis ? 0.30f : 0.60f, isLightChassis ? 0.30f : 0.70f, 1.0f);
    }

    // Right Side: Real-Time Audio CRT Oscilloscope Display
    float oscW = 195.0f;
    float oscH = 160.0f;
    float oscX = pX + pW - oscW - 20.0f;
    float oscY = pY + 65.0f;

    drawRoundedRect(r, oscX, oscY, oscW, oscH, 6.0f, 0.02f, 0.05f, 0.03f, 1.0f);
    drawRoundedRectOutline(r, oscX, oscY, oscW, oscH, 6.0f, 0.15f, 0.85f, 0.35f, 0.8f, 1.4f);

    // Top banner
    drawText(r, "OSCILLOSCOPE • LIVE OUTPUT", oscX + 10.0f, oscY + 8.0f, 8.5f, 0.20f, 0.95f, 0.40f, 1.0f);

    // CRT Phosphor Grid Lines
    float midY = oscY + oscH * 0.55f;
    drawLine(r, oscX + 6.0f, midY, oscX + oscW - 6.0f, midY, 0.10f, 0.45f, 0.20f, 0.45f, 1.0f);
    drawLine(r, oscX + 6.0f, midY - 30.0f, oscX + oscW - 6.0f, midY - 30.0f, 0.10f, 0.35f, 0.18f, 0.30f, 0.8f);
    drawLine(r, oscX + 6.0f, midY + 30.0f, oscX + oscW - 6.0f, midY + 30.0f, 0.10f, 0.35f, 0.18f, 0.30f, 0.8f);
    drawLine(r, oscX + oscW * 0.5f, oscY + 24.0f, oscX + oscW * 0.5f, oscY + oscH - 8.0f, 0.10f, 0.45f, 0.20f, 0.45f, 1.0f);

    // Waveform polyline
    constexpr int kPts = 48;
    float prevX = oscX + 8.0f;
    float prevY = midY - scopeBuffer_[0] * 38.0f;
    for (int i = 1; i < kPts; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kPts - 1);
        float sx = oscX + 8.0f + t * (oscW - 16.0f);
        float wave = scopeBuffer_[(i * 5) % 256];
        float sy = midY - wave * 38.0f;
        drawLine(r, prevX, prevY, sx, sy, 0.15f, 1.0f, 0.40f, 0.95f, 1.6f);
        prevX = sx;
        prevY = sy;
    }
}

void TrackInspectorView::renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (chordFollowBounds_.y + chordFollowBounds_.h < bounds_.y || chordFollowBounds_.y > bounds_.y + bounds_.h) return;

    const auto& trk = getTrack(selectedTrackIndex_);
    bool isFollowing = (trk.chordFollowMode != ChordFollowMode::Off);

    drawRoundedRect(r, chordFollowBounds_.x, chordFollowBounds_.y, chordFollowBounds_.w, chordFollowBounds_.h, 6.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.95f);
    drawRoundedRectOutline(r, chordFollowBounds_.x, chordFollowBounds_.y, chordFollowBounds_.w, chordFollowBounds_.h, 6.0f,
                           isFollowing ? 0.95f : theme.borderSubtle.r,
                           isFollowing ? 0.75f : theme.borderSubtle.g,
                           isFollowing ? 0.15f : theme.borderSubtle.b, isFollowing ? 1.5f : 1.0f);

    // Header Title & Icon
    drawText(r, "HARMONIC CHORD TRACK FOLLOW", chordFollowBounds_.x + 16.0f, chordFollowBounds_.y + 12.0f, 11.5f,
             0.95f, 0.80f, 0.20f, 1.0f);

    // Subtitle
    drawText(r, "Conform notes on this track non-destructively to active chord progression on the Chord Track.",
             chordFollowBounds_.x + 16.0f, chordFollowBounds_.y + 28.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    // BAKE TO MIDI Button on Right
    float bakeW = 100.0f;
    float bakeX = chordFollowBounds_.x + chordFollowBounds_.w - bakeW - 16.0f;
    drawButton(r, Rect2D{bakeX, chordFollowBounds_.y + 10.0f, bakeW, 24.0f}, "BAKE TO MIDI",
               Color{0.18f, 0.15f, 0.10f, 1.0f}, Color{0.95f, 0.75f, 0.15f, 1.0f}, Color{0.95f, 0.80f, 0.20f, 1.0f}, 9.5f, 3.0f, 1.2f);

    // 5 Mode Choice Chips
    static const char* kChipLabels[5] = {"OFF", "CHORD", "BASS", "SCALE", "COLOR LEAD"};
    static const ChordFollowMode kModes[5] = {
        ChordFollowMode::Off,
        ChordFollowMode::Chord,
        ChordFollowMode::Bass,
        ChordFollowMode::Scale,
        ChordFollowMode::ColorLead
    };

    float chipX = chordFollowBounds_.x + 16.0f;
    float chipY = chordFollowBounds_.y + 48.0f;
    for (int c = 0; c < 5; ++c) {
        float chipW = (c == 4) ? 95.0f : 70.0f;
        bool isSel = (trk.chordFollowMode == kModes[c]);

        if (isSel) {
            Color bg{c == 0 ? 0.35f : 0.0f, c == 0 ? 0.40f : 0.85f, c == 0 ? 0.45f : 1.0f, 1.0f};
            drawButton(r, Rect2D{chipX, chipY, chipW, 24.0f}, kChipLabels[c],
                       bg, Color{0.0f, 0.0f, 0.0f, 0.0f}, Color{0.05f, 0.08f, 0.12f, 1.0f}, 9.5f, 12.0f, 0.0f);
        } else {
            drawButton(r, Rect2D{chipX, chipY, chipW, 24.0f}, kChipLabels[c],
                       Color{0.14f, 0.16f, 0.20f, 1.0f}, Color{0.28f, 0.32f, 0.40f, 1.0f}, Color{0.70f, 0.75f, 0.82f, 1.0f}, 9.5f, 12.0f, 1.0f);
        }

        chipX += chipW + 8.0f;
    }
}

void TrackInspectorView::renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (midiFxBounds_.y + midiFxBounds_.h < bounds_.y || midiFxBounds_.y > bounds_.y + bounds_.h) return;

    const auto& trk = getTrack(selectedTrackIndex_);

    drawRoundedRect(r, midiFxBounds_.x, midiFxBounds_.y, midiFxBounds_.w, midiFxBounds_.h, 6.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.95f);
    drawRoundedRectOutline(r, midiFxBounds_.x, midiFxBounds_.y, midiFxBounds_.w, midiFxBounds_.h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    drawText(r, "MIDI FX PIPELINE", midiFxBounds_.x + 16.0f, midiFxBounds_.y + 12.0f, 11.5f,
             0.92f, 0.35f, 0.85f, 1.0f);

    // + ADD MIDI FX Button
    float addW = 110.0f;
    float addX = midiFxBounds_.x + midiFxBounds_.w - addW - 16.0f;
    drawButton(r, Rect2D{addX, midiFxBounds_.y + 8.0f, addW, 22.0f}, "+ ADD MIDI FX",
               Color{0.18f, 0.14f, 0.22f, 1.0f}, Color{0.92f, 0.35f, 0.85f, 0.9f}, Color{0.92f, 0.35f, 0.85f, 1.0f}, 9.0f, 3.0f, 1.0f);

    float modW = (midiFxBounds_.w - 48.0f) / 3.0f;
    float modH = 72.0f;
    float modY = midiFxBounds_.y + 38.0f;

    // Module 1: ARPEGGIATOR
    float m1X = midiFxBounds_.x + 16.0f;
    drawRoundedRect(r, m1X, modY, modW, modH, 4.0f, 0.09f, 0.10f, 0.14f, 1.0f);
    drawRoundedRectOutline(r, m1X, modY, modW, modH, 4.0f,
                           trk.midiFx.arpEnabled ? 0.92f : 0.22f,
                           trk.midiFx.arpEnabled ? 0.35f : 0.26f,
                           trk.midiFx.arpEnabled ? 0.85f : 0.32f, 1.0f, 1.2f);
    drawText(r, "ARPEGGIATOR", m1X + 10.0f, modY + 8.0f, 10.5f, 0.95f, 0.95f, 1.0f, 1.0f);

    // Toggle switch pill
    drawRoundedRect(r, m1X + modW - 32.0f, modY + 8.0f, 22.0f, 12.0f, 6.0f,
                    trk.midiFx.arpEnabled ? 0.92f : 0.22f,
                    trk.midiFx.arpEnabled ? 0.35f : 0.24f,
                    trk.midiFx.arpEnabled ? 0.85f : 0.28f, 1.0f);

    static const char* kArpPats[5] = {"UP", "DOWN", "UP/DN", "RAND", "CHORD"};
    std::string patStr = (trk.midiFx.arpPattern >= 0 && trk.midiFx.arpPattern < 5) ? kArpPats[trk.midiFx.arpPattern] : "UP";
    drawButton(r, Rect2D{m1X + 10.0f, modY + 28.0f, 55.0f, 20.0f}, patStr,
               Color{0.16f, 0.18f, 0.24f, 1.0f}, Color{0.25f, 0.28f, 0.35f, 1.0f}, Color{0.90f, 0.90f, 1.0f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawText(r, "RATE: 1/16", m1X + 75.0f, modY + 33.0f, 9.0f, 0.65f, 0.70f, 0.80f, 1.0f);
    drawText(r, "OCT: 2", m1X + 140.0f, modY + 33.0f, 9.0f, 0.65f, 0.70f, 0.80f, 1.0f);
    drawText(r, "GATE: 85%", m1X + 75.0f, modY + 50.0f, 8.5f, 0.55f, 0.60f, 0.70f, 1.0f);
    drawText(r, "SWING: 0%", m1X + 140.0f, modY + 50.0f, 8.5f, 0.55f, 0.60f, 0.70f, 1.0f);

    // Module 2: SCALE SNAP
    float m2X = midiFxBounds_.x + 24.0f + modW;
    drawRoundedRect(r, m2X, modY, modW, modH, 4.0f, 0.09f, 0.10f, 0.14f, 1.0f);
    drawRoundedRectOutline(r, m2X, modY, modW, modH, 4.0f,
                           trk.midiFx.scaleSnapEnabled ? 0.92f : 0.22f,
                           trk.midiFx.scaleSnapEnabled ? 0.35f : 0.26f,
                           trk.midiFx.scaleSnapEnabled ? 0.85f : 0.32f, 1.0f, 1.2f);
    drawText(r, "SCALE SNAP", m2X + 10.0f, modY + 8.0f, 10.5f, 0.95f, 0.95f, 1.0f, 1.0f);
    drawRoundedRect(r, m2X + modW - 32.0f, modY + 8.0f, 22.0f, 12.0f, 6.0f,
                    trk.midiFx.scaleSnapEnabled ? 0.92f : 0.22f,
                    trk.midiFx.scaleSnapEnabled ? 0.35f : 0.24f,
                    trk.midiFx.scaleSnapEnabled ? 0.85f : 0.28f, 1.0f);

    drawButton(r, Rect2D{m2X + 10.0f, modY + 28.0f, 60.0f, 20.0f}, "KEY: C",
               Color{0.16f, 0.18f, 0.24f, 1.0f}, Color{0.25f, 0.28f, 0.35f, 1.0f}, Color{0.90f, 0.90f, 1.0f, 1.0f}, 9.0f, 3.0f, 1.0f);
    drawButton(r, Rect2D{m2X + 80.0f, modY + 28.0f, 85.0f, 20.0f}, trk.midiFx.scaleMinor ? "NAT MINOR" : "MAJOR",
               Color{0.16f, 0.18f, 0.24f, 1.0f}, Color{0.25f, 0.28f, 0.35f, 1.0f}, Color{0.90f, 0.90f, 1.0f, 1.0f}, 9.0f, 3.0f, 1.0f);

    // Module 3: HUMANIZE
    float m3X = midiFxBounds_.x + 32.0f + 2.0f * modW;
    drawRoundedRect(r, m3X, modY, modW, modH, 4.0f, 0.09f, 0.10f, 0.14f, 1.0f);
    drawRoundedRectOutline(r, m3X, modY, modW, modH, 4.0f,
                           trk.midiFx.humanizeEnabled ? 0.92f : 0.22f,
                           trk.midiFx.humanizeEnabled ? 0.35f : 0.26f,
                           trk.midiFx.humanizeEnabled ? 0.85f : 0.32f, 1.0f, 1.2f);
    drawText(r, "HUMANIZE", m3X + 10.0f, modY + 8.0f, 10.5f, 0.95f, 0.95f, 1.0f, 1.0f);
    drawRoundedRect(r, m3X + modW - 32.0f, modY + 8.0f, 22.0f, 12.0f, 6.0f,
                    trk.midiFx.humanizeEnabled ? 0.92f : 0.22f,
                    trk.midiFx.humanizeEnabled ? 0.35f : 0.24f,
                    trk.midiFx.humanizeEnabled ? 0.85f : 0.28f, 1.0f);

    drawText(r, "TIMING: ±15ms", m3X + 12.0f, modY + 33.0f, 9.0f, 0.70f, 0.75f, 0.85f, 1.0f);
    drawText(r, "VELOCITY: ±20%", m3X + 105.0f, modY + 33.0f, 9.0f, 0.70f, 0.75f, 0.85f, 1.0f);
}

void TrackInspectorView::renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (audioFxBounds_.y + audioFxBounds_.h < bounds_.y || audioFxBounds_.y > bounds_.y + bounds_.h) return;

    const auto& trk = getTrack(selectedTrackIndex_);

    drawRoundedRect(r, audioFxBounds_.x, audioFxBounds_.y, audioFxBounds_.w, audioFxBounds_.h, 6.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.95f);
    drawRoundedRectOutline(r, audioFxBounds_.x, audioFxBounds_.y, audioFxBounds_.w, audioFxBounds_.h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    drawText(r, "AUDIO INSERT FX CHAIN", audioFxBounds_.x + 16.0f, audioFxBounds_.y + 12.0f, 11.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // + ADD AUDIO FX Button
    float addW = 115.0f;
    float addX = audioFxBounds_.x + audioFxBounds_.w - addW - 16.0f;
    Color afxBg{0.14f, 0.18f, 0.24f, 1.0f};
    drawButton(r, Rect2D{addX, audioFxBounds_.y + 8.0f, addW, 22.0f}, "+ ADD AUDIO FX",
               afxBg, theme.primaryAccent, theme.primaryAccent, 9.0f, 3.0f, 1.0f);

    float fxW = (audioFxBounds_.w - 64.0f) / 5.0f;
    float fxH = 88.0f;
    float fxY = audioFxBounds_.y + 36.0f;

    static const char* kFxTitles[5] = {"DELAY", "CHORUS", "5-BAND EQ", "COMPRESSOR", "CONVOLVER"};
    bool fxEnabled[5] = {
        trk.audioFx.delayEnabled,
        trk.audioFx.chorusEnabled,
        trk.audioFx.eqEnabled,
        trk.audioFx.compEnabled,
        trk.audioFx.convolverEnabled
    };

    auto drawMiniDial = [&](float cx, float cy, float radius, float normVal, const char* label) {
        constexpr float minA = -2.35619449f;
        constexpr float maxA = 2.35619449f;
        float curA = minA + std::clamp(normVal, 0.0f, 1.0f) * (maxA - minA);

        drawCircle(r, cx, cy, radius + 2.0f, 0.06f, 0.07f, 0.09f, 1.0f);
        drawCircle(r, cx, cy, radius, 0.16f, 0.18f, 0.24f, 1.0f);
        drawCircleOutline(r, cx, cy, radius, 0.30f, 0.34f, 0.44f, 1.0f, 1.0f);

        float indX = cx + std::sin(curA) * (radius - 2.0f);
        float indY = cy - std::cos(curA) * (radius - 2.0f);
        drawLine(r, cx, cy, indX, indY, 0.0f, 0.95f, 1.0f, 1.0f, 1.6f);

        if (label && label[0] != '\0') {
            drawText(r, label, cx - 10.0f, cy + radius + 4.0f, 7.5f, 0.65f, 0.70f, 0.80f, 1.0f);
        }
    };

    for (int u = 0; u < 5; ++u) {
        float ux = audioFxBounds_.x + 16.0f + static_cast<float>(u) * (fxW + 8.0f);

        drawRoundedRect(r, ux, fxY, fxW, fxH, 4.0f, 0.09f, 0.10f, 0.14f, 1.0f);
        drawRoundedRectOutline(r, ux, fxY, fxW, fxH, 4.0f,
                               fxEnabled[u] ? theme.primaryAccent.r : 0.20f,
                               fxEnabled[u] ? theme.primaryAccent.g : 0.24f,
                               fxEnabled[u] ? theme.primaryAccent.b : 0.30f, 1.0f, 1.2f);

        drawText(r, kFxTitles[u], ux + 8.0f, fxY + 8.0f, 10.0f, 0.95f, 0.95f, 1.0f, 1.0f);
        drawRoundedRect(r, ux + fxW - 26.0f, fxY + 8.0f, 20.0f, 12.0f, 6.0f,
                        fxEnabled[u] ? theme.primaryAccent.r : 0.20f,
                        fxEnabled[u] ? theme.primaryAccent.g : 0.24f,
                        fxEnabled[u] ? theme.primaryAccent.b : 0.28f, 1.0f);

        float kw = fxW / 3.0f;
        float ky = fxY + 44.0f;

        if (u == 0) { // Delay
            drawMiniDial(ux + kw * 0.5f, ky, 10.0f, trk.audioFx.delayTime, "TIME");
            drawMiniDial(ux + kw * 1.5f, ky, 10.0f, trk.audioFx.delayFeedback, "FDBK");
            drawMiniDial(ux + kw * 2.5f, ky, 10.0f, trk.audioFx.delayMix, "MIX");
        } else if (u == 1) { // Chorus
            drawMiniDial(ux + kw * 0.5f, ky, 10.0f, trk.audioFx.chorusRate, "RATE");
            drawMiniDial(ux + kw * 1.5f, ky, 10.0f, trk.audioFx.chorusDepth, "DPTH");
            drawMiniDial(ux + kw * 2.5f, ky, 10.0f, trk.audioFx.chorusMix, "MIX");
        } else if (u == 2) { // 5-Band EQ
            drawMiniDial(ux + kw * 0.5f, ky, 10.0f, trk.audioFx.eqLow, "LOW");
            drawMiniDial(ux + kw * 1.5f, ky, 10.0f, trk.audioFx.eqMid, "MID");
            drawMiniDial(ux + kw * 2.5f, ky, 10.0f, trk.audioFx.eqHigh, "HIGH");
        } else if (u == 3) { // Compressor
            drawMiniDial(ux + kw * 0.5f, ky, 10.0f, trk.audioFx.compThreshold, "THRS");
            drawMiniDial(ux + kw * 1.5f, ky, 10.0f, trk.audioFx.compRatio, "RATIO");
            drawMiniDial(ux + kw * 2.5f, ky, 10.0f, trk.audioFx.compGain, "GAIN");
        } else if (u == 4) { // Convolver
            drawMiniDial(ux + kw * 0.8f, ky, 10.0f, static_cast<float>(trk.audioFx.convolverPreset) / 5.0f, "SPACE");
            drawMiniDial(ux + kw * 2.2f, ky, 10.0f, trk.audioFx.convolverMix, "MIX");
        }
    }
}

void TrackInspectorView::renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme) {
    drawRoundedRect(r, scrollbarBounds_.x, scrollbarBounds_.y, scrollbarBounds_.w, scrollbarBounds_.h, 3.0f,
                    0.06f, 0.07f, 0.09f, 0.8f);
    drawRoundedRectOutline(r, scrollbarBounds_.x, scrollbarBounds_.y, scrollbarBounds_.w, scrollbarBounds_.h, 3.0f,
                           0.18f, 0.20f, 0.26f, 1.0f, 1.0f);

    float maxScroll = std::max(10.0f, totalContentHeight_ - bounds_.h + 20.0f);
    float thumbH = std::clamp(bounds_.h * (bounds_.h / totalContentHeight_), 32.0f, bounds_.h * 0.85f);
    float thumbNorm = std::clamp(scrollY_ / maxScroll, 0.0f, 1.0f);
    float thumbY = scrollbarBounds_.y + thumbNorm * (scrollbarBounds_.h - thumbH);

    bool isDragging = (dragMode_ == DragMode::Scrollbar);
    drawRoundedRect(r, scrollbarBounds_.x, thumbY, scrollbarBounds_.w, thumbH, 3.0f,
                    isDragging ? theme.primaryAccent.r : 0.35f,
                    isDragging ? theme.primaryAccent.g : 0.40f,
                    isDragging ? theme.primaryAccent.b : 0.50f, 1.0f);
}

bool TrackInspectorView::handlePointer(const PointerEvent& ev, [[maybe_unused]] const ViewContext& ctx) {
    // 0. Forward to embedded PluginSearchDialog if open
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handlePointer(ev)) return true;
        return true;
    }

    if (ev.action == PointerAction::Down) {
        // A. Track Selector Ribbon Hits
        if (trackRibbonBounds_.contains(ev.x, ev.y) || (ev.y >= 60.0f && ev.y <= 95.0f)) {
            const float startX = 40.0f;
            const float tabW = 120.0f;
            const float tabGap = 8.0f;
            for (size_t i = 0; i < tracks_.size(); ++i) {
                float tx = startX + static_cast<float>(i) * (tabW + tabGap);
                if (ev.x >= tx && ev.x <= tx + tabW) {
                    setActiveTrack(static_cast<uint32_t>(i));
                    if (onTrackSelected) onTrackSelected(selectedTrackIndex_);
                    return true;
                }
            }
        }

        // B. Header Card Hits
        if (headerCardBounds_.contains(ev.x, ev.y)) {
            auto& trk = getTrack(selectedTrackIndex_);
            float btnY = headerCardBounds_.y + 17.0f;
            float btnH = 30.0f;

            // Swatches
            float swatchStartX = headerCardBounds_.x + 310.0f;
            float swatchY = headerCardBounds_.y + 24.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = swatchStartX + static_cast<float>(s) * 24.0f + 8.0f;
                float sy = swatchY + 8.0f;
                if (std::hypot(ev.x - sx, ev.y - sy) <= 12.0f) {
                    trk.r = kQuickSwatches[s][0];
                    trk.g = kQuickSwatches[s][1];
                    trk.b = kQuickSwatches[s][2];
                    if (onColorChanged) onColorChanged(selectedTrackIndex_, trk.r, trk.g, trk.b);
                    return true;
                }
            }

            // [ CODE ] button
            float codeW = 74.0f;
            float codeX = headerCardBounds_.x + headerCardBounds_.w - 265.0f;
            if (ev.x >= codeX && ev.x <= codeX + codeW && ev.y >= btnY && ev.y <= btnY + btnH) {
                if (onOpenCodeEditor) onOpenCodeEditor(selectedTrackIndex_);
                return true;
            }

            // MUTE button
            float muteW = 54.0f;
            float muteX = headerCardBounds_.x + headerCardBounds_.w - 185.0f;
            if (ev.x >= muteX && ev.x <= muteX + muteW && ev.y >= btnY && ev.y <= btnY + btnH) {
                trk.mute = !trk.mute;
                if (onMuteToggled) onMuteToggled(selectedTrackIndex_, trk.mute);
                return true;
            }

            // SOLO button
            float soloW = 54.0f;
            float soloX = headerCardBounds_.x + headerCardBounds_.w - 125.0f;
            if (ev.x >= soloX && ev.x <= soloX + soloW && ev.y >= btnY && ev.y <= btnY + btnH) {
                trk.solo = !trk.solo;
                if (onSoloToggled) onSoloToggled(selectedTrackIndex_, trk.solo);
                return true;
            }

            // FREEZE button
            float fzW = 60.0f;
            float fzX = headerCardBounds_.x + headerCardBounds_.w - 65.0f;
            if (ev.x >= fzX && ev.x <= fzX + fzW && ev.y >= btnY && ev.y <= btnY + btnH) {
                trk.freeze = !trk.freeze;
                if (onFreezeToggled) onFreezeToggled(selectedTrackIndex_, trk.freeze);
                return true;
            }
        }

        // C. Mixer Controls Hits
        if (mixerCardBounds_.contains(ev.x, ev.y)) {
            auto& trk = getTrack(selectedTrackIndex_);
            float vTrackX = mixerCardBounds_.x + 85.0f;
            float vTrackW = (mixerCardBounds_.w * 0.45f) - 90.0f;
            float sY = mixerCardBounds_.y + 36.0f;
            float sH = 18.0f;

            // Volume Slider
            if (ev.x >= vTrackX - 8.0f && ev.x <= vTrackX + vTrackW + 8.0f && ev.y >= sY - 5.0f && ev.y <= sY + sH + 5.0f) {
                dragMode_ = DragMode::VolumeSlider;
                dragStartX_ = ev.x;
                float norm = std::clamp((ev.x - vTrackX) / vTrackW, 0.0f, 1.0f);
                trk.volume = norm * 1.5f;
                if (onVolumeChanged) onVolumeChanged(selectedTrackIndex_, trk.volume);
                return true;
            }

            // Pan Slider
            float pStartX = mixerCardBounds_.x + mixerCardBounds_.w * 0.52f;
            float pTrackX = pStartX + 45.0f;
            float pTrackW = (mixerCardBounds_.w * 0.40f) - 60.0f;
            if (ev.x >= pTrackX - 8.0f && ev.x <= pTrackX + pTrackW + 8.0f && ev.y >= sY - 5.0f && ev.y <= sY + sH + 5.0f) {
                dragMode_ = DragMode::PanSlider;
                dragStartX_ = ev.x;
                float norm = std::clamp((ev.x - pTrackX) / pTrackW, 0.0f, 1.0f);
                trk.pan = norm * 2.0f - 1.0f;
                if (onPanChanged) onPanChanged(selectedTrackIndex_, trk.pan);
                return true;
            }
        }

        // D. Dynamic Faceplate Hits
        if (faceplateBounds_.contains(ev.x, ev.y)) {
            auto& trk = getTrack(selectedTrackIndex_);
            float pX = faceplateBounds_.x;
            float pY = faceplateBounds_.y;
            float pW = faceplateBounds_.w;

            // < PREV
            float btnPrevX = pX + 12.0f;
            if (ev.x >= btnPrevX && ev.x <= btnPrevX + 56.0f && ev.y >= pY + 6.0f && ev.y <= pY + 30.0f) {
                if (onPrevPreset) onPrevPreset();
                return true;
            }

            // NEXT >
            float btnNextX = pX + 75.0f;
            if (ev.x >= btnNextX && ev.x <= btnNextX + 56.0f && ev.y >= pY + 6.0f && ev.y <= pY + 30.0f) {
                if (onNextPreset) onNextPreset();
                return true;
            }

            // [ ⇄ CHANGE INSTRUMENT ]
            float chgW = 150.0f;
            float chgX = pX + pW - chgW - 14.0f;
            if (ev.x >= chgX && ev.x <= chgX + chgW && ev.y >= pY + 6.0f && ev.y <= pY + 30.0f) {
                pluginDialog_.open(PluginDialogMode::AddInstrument, trk.name, selectedTrackIndex_);
                return true;
            }

            // Hardware Knobs
            float knobAreaW = pW - 240.0f;
            size_t kCount = std::min(trk.knobs.size(), static_cast<size_t>(6));
            float kStep = knobAreaW / static_cast<float>(std::max(size_t{1}, kCount));

            for (size_t i = 0; i < kCount; ++i) {
                float cx = pX + 24.0f + static_cast<float>(i) * kStep + (kStep * 0.5f);
                float cy = pY + 130.0f;
                if (std::hypot(ev.x - cx, ev.y - cy) <= 28.0f) {
                    dragMode_ = DragMode::HardwareKnob;
                    activeKnobIndex_ = static_cast<int>(i);
                    activeParamName_ = trk.knobs[i].name;
                    dragStartY_ = ev.y;
                    dragStartVal_ = trk.knobs[i].value;
                    return true;
                }
            }
        }

        // E. Harmonic Chord Track Follow Hits
        if (chordFollowBounds_.contains(ev.x, ev.y)) {
            auto& trk = getTrack(selectedTrackIndex_);

            // BAKE TO MIDI button
            float bakeW = 100.0f;
            float bakeX = chordFollowBounds_.x + chordFollowBounds_.w - bakeW - 16.0f;
            if (ev.x >= bakeX && ev.x <= bakeX + bakeW && ev.y >= chordFollowBounds_.y + 10.0f && ev.y <= chordFollowBounds_.y + 34.0f) {
                if (onBakeChords) onBakeChords(selectedTrackIndex_);
                return true;
            }

            // 5 Mode Chips
            static const ChordFollowMode kModes[5] = {
                ChordFollowMode::Off,
                ChordFollowMode::Chord,
                ChordFollowMode::Bass,
                ChordFollowMode::Scale,
                ChordFollowMode::ColorLead
            };
            float chipX = chordFollowBounds_.x + 16.0f;
            float chipY = chordFollowBounds_.y + 48.0f;
            for (int c = 0; c < 5; ++c) {
                float chipW = (c == 4) ? 95.0f : 70.0f;
                if (ev.x >= chipX && ev.x <= chipX + chipW && ev.y >= chipY && ev.y <= chipY + 24.0f) {
                    trk.chordFollowMode = kModes[c];
                    if (onChordFollowChanged) onChordFollowChanged(selectedTrackIndex_, trk.chordFollowMode);
                    return true;
                }
                chipX += chipW + 8.0f;
            }
        }

        // F. MIDI FX Rack Hits
        if (midiFxBounds_.contains(ev.x, ev.y)) {
            auto& trk = getTrack(selectedTrackIndex_);

            // + ADD MIDI FX button
            float addW = 110.0f;
            float addX = midiFxBounds_.x + midiFxBounds_.w - addW - 16.0f;
            if (ev.x >= addX && ev.x <= addX + addW && ev.y >= midiFxBounds_.y + 8.0f && ev.y <= midiFxBounds_.y + 30.0f) {
                pluginDialog_.open(PluginDialogMode::AddMidiFx, trk.name, selectedTrackIndex_);
                return true;
            }

            float modW = (midiFxBounds_.w - 48.0f) / 3.0f;
            float modY = midiFxBounds_.y + 38.0f;

            // Arp Toggle & Pattern
            float m1X = midiFxBounds_.x + 16.0f;
            if (ev.x >= m1X + modW - 35.0f && ev.x <= m1X + modW - 5.0f && ev.y >= modY + 5.0f && ev.y <= modY + 24.0f) {
                trk.midiFx.arpEnabled = !trk.midiFx.arpEnabled;
                return true;
            }
            if (ev.x >= m1X + 10.0f && ev.x <= m1X + 65.0f && ev.y >= modY + 28.0f && ev.y <= modY + 48.0f) {
                trk.midiFx.arpPattern = (trk.midiFx.arpPattern + 1) % 5;
                return true;
            }

            // Scale Snap Toggle & Mode
            float m2X = midiFxBounds_.x + 24.0f + modW;
            if (ev.x >= m2X + modW - 35.0f && ev.x <= m2X + modW - 5.0f && ev.y >= modY + 5.0f && ev.y <= modY + 24.0f) {
                trk.midiFx.scaleSnapEnabled = !trk.midiFx.scaleSnapEnabled;
                return true;
            }
            if (ev.x >= m2X + 80.0f && ev.x <= m2X + 165.0f && ev.y >= modY + 28.0f && ev.y <= modY + 48.0f) {
                trk.midiFx.scaleMinor = !trk.midiFx.scaleMinor;
                return true;
            }

            // Humanize Toggle
            float m3X = midiFxBounds_.x + 32.0f + 2.0f * modW;
            if (ev.x >= m3X + modW - 35.0f && ev.x <= m3X + modW - 5.0f && ev.y >= modY + 5.0f && ev.y <= modY + 24.0f) {
                trk.midiFx.humanizeEnabled = !trk.midiFx.humanizeEnabled;
                return true;
            }
        }

        // G. Audio FX Insert Rack Hits
        if (audioFxBounds_.contains(ev.x, ev.y)) {
            auto& trk = getTrack(selectedTrackIndex_);

            // + ADD AUDIO FX button
            float addW = 115.0f;
            float addX = audioFxBounds_.x + audioFxBounds_.w - addW - 16.0f;
            if (ev.x >= addX && ev.x <= addX + addW && ev.y >= audioFxBounds_.y + 8.0f && ev.y <= audioFxBounds_.y + 30.0f) {
                pluginDialog_.open(PluginDialogMode::AddAudioFx, trk.name, selectedTrackIndex_);
                return true;
            }

            float fxW = (audioFxBounds_.w - 64.0f) / 5.0f;
            float fxY = audioFxBounds_.y + 36.0f;
            for (int u = 0; u < 5; ++u) {
                float ux = audioFxBounds_.x + 16.0f + static_cast<float>(u) * (fxW + 8.0f);
                // Toggle pill on top-right of module
                if (ev.x >= ux + fxW - 28.0f && ev.x <= ux + fxW - 4.0f && ev.y >= fxY + 6.0f && ev.y <= fxY + 22.0f) {
                    if (u == 0) trk.audioFx.delayEnabled = !trk.audioFx.delayEnabled;
                    else if (u == 1) trk.audioFx.chorusEnabled = !trk.audioFx.chorusEnabled;
                    else if (u == 2) trk.audioFx.eqEnabled = !trk.audioFx.eqEnabled;
                    else if (u == 3) trk.audioFx.compEnabled = !trk.audioFx.compEnabled;
                    else if (u == 4) trk.audioFx.convolverEnabled = !trk.audioFx.convolverEnabled;
                    return true;
                }
            }
        }

        // H. Scrollbar Hits
        if (needScrollbar_ && scrollbarBounds_.contains(ev.x, ev.y)) {
            dragMode_ = DragMode::Scrollbar;
            dragStartY_ = ev.y;
            dragStartScrollY_ = scrollY_;
            return true;
        }
    } else if (ev.action == PointerAction::Move) {
        auto& trk = getTrack(selectedTrackIndex_);
        if (dragMode_ == DragMode::VolumeSlider) {
            float vTrackX = mixerCardBounds_.x + 85.0f;
            float vTrackW = (mixerCardBounds_.w * 0.45f) - 90.0f;
            float norm = std::clamp((ev.x - vTrackX) / vTrackW, 0.0f, 1.0f);
            trk.volume = norm * 1.5f;
            if (onVolumeChanged) onVolumeChanged(selectedTrackIndex_, trk.volume);
            return true;
        } else if (dragMode_ == DragMode::PanSlider) {
            float pStartX = mixerCardBounds_.x + mixerCardBounds_.w * 0.52f;
            float pTrackX = pStartX + 45.0f;
            float pTrackW = (mixerCardBounds_.w * 0.40f) - 60.0f;
            float norm = std::clamp((ev.x - pTrackX) / pTrackW, 0.0f, 1.0f);
            trk.pan = norm * 2.0f - 1.0f;
            if (onPanChanged) onPanChanged(selectedTrackIndex_, trk.pan);
            return true;
        } else if (dragMode_ == DragMode::HardwareKnob && activeKnobIndex_ >= 0) {
            float deltaY = dragStartY_ - ev.y;
            float newVal = std::clamp(dragStartVal_ + deltaY / 150.0f, 0.0f, 1.0f);
            if (activeKnobIndex_ < static_cast<int>(trk.knobs.size())) {
                trk.knobs[activeKnobIndex_].value = newVal;
                std::ostringstream ss;
                ss << std::fixed << std::setprecision(1) << (newVal * 100.0f) << "%";
                trk.knobs[activeKnobIndex_].display = ss.str();
                if (onParamChanged) onParamChanged(selectedTrackIndex_, activeParamName_, newVal);
            }
            return true;
        } else if (dragMode_ == DragMode::Scrollbar) {
            float maxScroll = std::max(10.0f, totalContentHeight_ - bounds_.h + 20.0f);
            float deltaY = ev.y - dragStartY_;
            float scrollDelta = (deltaY / bounds_.h) * maxScroll;
            scrollY_ = std::clamp(dragStartScrollY_ + scrollDelta, 0.0f, maxScroll);
            if (onScrollChanged) onScrollChanged(scrollY_);
            return true;
        }
    } else if (ev.action == PointerAction::Up) {
        if (dragMode_ != DragMode::None) {
            dragMode_ = DragMode::None;
            activeKnobIndex_ = -1;
            return true;
        }
    } else if (ev.action == PointerAction::Scroll) {
        float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h + 20.0f);
        scrollY_ -= ev.scrollY * 28.0f;
        scrollY_ = std::clamp(scrollY_, 0.0f, maxScroll);
        if (onScrollChanged) onScrollChanged(scrollY_);
        return true;
    }

    return false;
}

bool TrackInspectorView::handleKey(int key, int scancode, int action, int mods, [[maybe_unused]] const ViewContext& ctx) {
    if (pluginDialog_.isOpen()) {
        return pluginDialog_.handleKey(key, scancode, action, mods);
    }
    return false;
}

} // namespace eatsbits::ui
