#include "eatsbits/ui/views/mixer_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace eatsbits::ui {

MixerView::MixerView() {
    masterChannel_.name = "MASTER";
    masterChannel_.type = "BUS";
    masterChannel_.r = 1.0f;
    masterChannel_.g = 0.85f;
    masterChannel_.b = 0.0f;
    masterChannel_.fader = 0.85f;
    masterChannel_.pan = 0.0f;
    masterChannel_.mute = false;

    channels_ = {
        {"303 Acid", "SYNTH", 0.0f, 0.90f, 1.0f, 0.80f, -0.10f, false, false, false, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f,
         true, 30.0f, 2.5f, 850.0f, 3.0f, 1.4f, -1.0f, "TB-303", 0.65f, 0.80f, 0.45f, 0.90f, "CUTOFF", "RESON", "ENV", "ACCENT", {}, {}},
        {"808 Drums", "DRUMS", 1.0f, 0.55f, 0.10f, 0.85f, 0.0f, false, false, false, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f,
         true, 25.0f, 3.0f, 1200.0f, -1.5f, 1.0f, 1.5f, "TR-808", 0.75f, 0.60f, 0.50f, 0.85f, "TONE", "SNAPPY", "DECAY", "TUNING", {}, {}},
        {"909 Groove", "DRUMS", 1.0f, 0.16f, 0.43f, 0.78f, 0.10f, false, false, false, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f,
         true, 20.0f, 1.0f, 2500.0f, 0.5f, 1.2f, 2.0f, "TR-909", 0.80f, 0.70f, 0.40f, 0.95f, "TUNE", "ATTACK", "DECAY", "SNAPPY", {}, {}},
        {"DX7 Rhodes", "SYNTH", 0.62f, 0.31f, 0.87f, 0.75f, 0.20f, false, false, false, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f,
         true, 40.0f, -2.0f, 1500.0f, 2.0f, 1.1f, 0.0f, "DX7 FM", 0.50f, 0.65f, 0.70f, 0.60f, "ALGO", "FEEDBACK", "BRIGHT", "DETUNE", {}, {}},
        {"Concert Grand", "PHYSICAL", 0.88f, 0.66f, 0.43f, 0.70f, -0.15f, false, false, false, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f,
         true, 20.0f, 0.0f, 1000.0f, 0.0f, 1.0f, 0.0f, "GRAND PIANO", 0.55f, 0.75f, 0.45f, 0.80f, "TONE", "SNAPPY", "DECAY", "VAR", {}, {}}
    };

    // Default standalone properties drawer is closed
    propertiesDrawer_.setExpanded(false);

    // Shared TrackPropertiesDrawer callbacks across Arranger and Mixer
    propertiesDrawer_.onVolumeChanged = [this](uint32_t trackIdx, float vol) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].fader = vol;
        }
    };
    propertiesDrawer_.onPanChanged = [this](uint32_t trackIdx, float pan) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].pan = pan;
        }
    };
    propertiesDrawer_.onMuteToggled = [this](uint32_t trackIdx, bool mute) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].mute = mute;
        }
    };
    propertiesDrawer_.onSoloToggled = [this](uint32_t trackIdx, bool solo) {
        if (trackIdx < channels_.size()) {
            channels_[trackIdx].solo = solo;
        }
    };
    propertiesDrawer_.onParamChanged = [this](uint32_t trackIdx, const std::string& paramName, float normVal) {
        if (trackIdx < channels_.size()) {
            auto& ch = channels_[trackIdx];
            if (paramName == "cutoff" || paramName == "tone" || paramName == "attack" || paramName == "param1") {
                ch.knob1 = normVal;
            } else if (paramName == "resonance" || paramName == "snappy" || paramName == "punch" || paramName == "decay" || paramName == "param2") {
                ch.knob2 = normVal;
            } else if (paramName == "decay" || paramName == "tune" || paramName == "bright" || paramName == "param3") {
                ch.knob3 = normVal;
            } else if (paramName == "accent" || paramName == "tuning" || paramName == "crack" || paramName == "detune" || paramName == "param4") {
                ch.knob4 = normVal;
            }
        }
    };
    propertiesDrawer_.onChooseTrackIcon = [this](uint32_t trackIdx) {
        if (onChooseTrackIcon) {
            onChooseTrackIcon(trackIdx);
        }
    };
    propertiesDrawer_.onTrackRenameWithText = [this](uint32_t trackIdx, const std::string& newName) {
        if (!newName.empty()) {
            drawerData_.trackName = newName;
            if (trackIdx < drawerData_.allTrackNames.size()) {
                drawerData_.allTrackNames[trackIdx] = newName;
            }
            if (trackIdx < channels_.size()) {
                channels_[trackIdx].name = newName;
            }
            if (onTrackRename) {
                onTrackRename(trackIdx, newName);
            }
        }
    };
    propertiesDrawer_.getPanel().getPluginSearchDialog().onPluginSelected =
        [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t targetIdx) {
            if (mode == PluginDialogMode::AddInstrument) {
                if (targetIdx < channels_.size()) {
                    channels_[targetIdx].instrument = entry.name;
                    channels_[targetIdx].r = entry.r;
                    channels_[targetIdx].g = entry.g;
                    channels_[targetIdx].b = entry.b;
                }
                drawerData_.instrument = entry.name;
                drawerData_.instrumentEngine = entry.engineTag;
                drawerData_.r = entry.r;
                drawerData_.g = entry.g;
                drawerData_.b = entry.b;
            } else if (mode == PluginDialogMode::AddMidiFx) {
                drawerData_.midiFx.push_back({entry.name, entry.engineTag, true});
            } else if (mode == PluginDialogMode::AddAudioFx) {
                drawerData_.audioFx.push_back({entry.name, entry.engineTag, 0.5f, 0.5f, true});
            }
        };
}

void MixerView::setPreset(ModularMixerPreset preset) noexcept {
    switch (preset) {
        case ModularMixerPreset::Full:
            showRouting_ = true;
            showAutomation_ = true;
            showPan_ = true;
            showFaders_ = true;
            showMeters_ = true;
            showButtons_ = true;
            showReadouts_ = true;
            break;
        case ModularMixerPreset::FadersOnly:
            showRouting_ = false;
            showAutomation_ = false;
            showPan_ = false;
            showFaders_ = true;
            showMeters_ = false;
            showButtons_ = false;
            showReadouts_ = true;
            break;
        case ModularMixerPreset::FadersAndMeters:
            showRouting_ = false;
            showAutomation_ = false;
            showPan_ = false;
            showFaders_ = true;
            showMeters_ = true;
            showButtons_ = false;
            showReadouts_ = true;
            break;
    }
}

void MixerView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;
    masterStripWidth_ = 140.0f;
    channelStripWidth_ = 140.0f;
    channelGap_ = 10.0f;

    // Sliding Track Properties Drawer Layout
    propertiesDrawer_.layout(bounds_, 0.0f);
    float drawerTotalW = propertiesDrawer_.isExpanded()
        ? (TrackPropertiesDrawer::kPullTabWidth + propertiesDrawer_.getWidth())
        : TrackPropertiesDrawer::kPullTabWidth;

    // Clean authentic layout: Strips start directly at top padding (Eatsbeats parity)
    float stripTopY = bounds_.y + 8.0f;
    float stripH = bounds_.h - 16.0f;

    // Master Bus Strip Position (Left or Far-Right)
    if (masterPosition_ == MixerMasterPosition::Right) {
        float mX = bounds_.x + bounds_.w - drawerTotalW - masterStripWidth_ - 10.0f;
        masterBounds_ = Rect2D(mX, stripTopY, masterStripWidth_, stripH);
        channelsScrollBounds_ = Rect2D(bounds_.x + 40.0f, stripTopY, mX - bounds_.x - 50.0f, stripH);
    } else {
        // Pinned Left (Default, exactly matches Eatsbeats)
        masterBounds_ = Rect2D(bounds_.x, stripTopY, masterStripWidth_, stripH);
        float scrollX = bounds_.x + masterStripWidth_ + 24.0f;
        float scrollW = bounds_.w - scrollX - drawerTotalW;
        channelsScrollBounds_ = Rect2D(scrollX, stripTopY, std::max(100.0f, scrollW), stripH);
    }
}

void MixerView::syncFromWindow(
    const std::vector<std::string>& trackNames,
    const std::vector<float>& trackVols,
    const std::vector<float>& trackPans,
    const std::vector<bool>& trackMutes,
    const std::vector<bool>& trackSolos,
    const std::vector<bool>& trackFreezes,
    const std::vector<Color>& trackColors,
    uint32_t selectedTrackIdx,
    float masterVol, float masterPanVal, bool masterMuteVal,
    float masterPeakLeft, float masterPeakRight,
    const float* chPeaksL, const float* chPeaksR, size_t numPeaks,
    bool propertiesExpanded, float propertiesWidth,
    bool showRouting, bool showPan, bool showButtons, bool showMeters,
    bool showAutomation, bool showReadouts, bool browserOpen, float scrollY) {

    masterChannel_.fader = masterVol;
    masterChannel_.pan = masterPanVal;
    masterChannel_.mute = masterMuteVal;
    masterChannel_.peakL = masterPeakLeft;
    masterChannel_.peakR = masterPeakRight;
    masterChannel_.peakHoldL = std::max(masterChannel_.peakHoldL * 0.96f, masterPeakLeft);
    masterChannel_.peakHoldR = std::max(masterChannel_.peakHoldR * 0.96f, masterPeakRight);

    selectedChannel_ = static_cast<int>(selectedTrackIdx);

    propertiesDrawer_.setExpanded(propertiesExpanded);
    propertiesDrawer_.setWidth(propertiesWidth);
    propertiesDrawer_.setScrollY(scrollY);

    showRouting_ = showRouting;
    showPan_ = showPan;
    showButtons_ = showButtons;
    showMeters_ = showMeters;
    showAutomation_ = showAutomation;
    showReadouts_ = showReadouts;

    size_t count = std::max(channels_.size(), trackNames.size());
    if (channels_.size() < count) channels_.resize(count);

    for (size_t i = 0; i < count; ++i) {
        if (i < trackNames.size()) channels_[i].name = trackNames[i];
        if (i < trackVols.size()) channels_[i].fader = trackVols[i];
        if (i < trackPans.size()) channels_[i].pan = trackPans[i];
        if (i < trackMutes.size()) channels_[i].mute = trackMutes[i];
        if (i < trackSolos.size()) channels_[i].solo = trackSolos[i];
        if (i < trackFreezes.size()) channels_[i].freeze = trackFreezes[i];
        if (i < trackColors.size()) {
            channels_[i].r = trackColors[i].r;
            channels_[i].g = trackColors[i].g;
            channels_[i].b = trackColors[i].b;
        }
        if (chPeaksL && i < numPeaks) {
            channels_[i].peakL = chPeaksL[i];
            channels_[i].peakHoldL = std::max(channels_[i].peakHoldL * 0.96f, chPeaksL[i]);
        }
        if (chPeaksR && i < numPeaks) {
            channels_[i].peakR = chPeaksR[i];
            channels_[i].peakHoldR = std::max(channels_[i].peakHoldR * 0.96f, chPeaksR[i]);
        }
    }
}

void MixerView::render(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 0. Update meters from Audio Engine if active
    if (ctx.audioEngine) {
        MeterFeedback fb;
        if (ctx.audioEngine->pollMeterFeedback(fb)) {
            masterChannel_.peakL = fb.peakLeft;
            masterChannel_.peakR = fb.peakRight;
            masterChannel_.peakHoldL = std::max(masterChannel_.peakHoldL * 0.96f, fb.peakLeft);
            masterChannel_.peakHoldR = std::max(masterChannel_.peakHoldR * 0.96f, fb.peakRight);
        }
        for (size_t i = 0; i < channels_.size(); ++i) {
            MeterFeedback trFb;
            if (ctx.audioEngine->getTrackMeterFeedback(static_cast<uint32_t>(i), trFb)) {
                channels_[i].peakL = trFb.peakLeft;
                channels_[i].peakR = trFb.peakRight;
                channels_[i].peakHoldL = std::max(channels_[i].peakHoldL * 0.96f, trFb.peakLeft);
                channels_[i].peakHoldR = std::max(channels_[i].peakHoldR * 0.96f, trFb.peakRight);
            }
        }
    }

    // 1. Pinned Master Bus Strip (Always in view on the left, Eatsbeats parity)
    renderMasterStrip(r, theme, masterBounds_);

    // Divider Line between Master Bus and Scrollable Track Strips
    float divX = masterBounds_.x + masterBounds_.w + 12.0f;
    drawLine(r, divX, masterBounds_.y + 4.0f, divX, masterBounds_.y + masterBounds_.h - 4.0f,
             0.22f, 0.25f, 0.32f, 0.8f, 1.5f);

    // 2. Horizontally Scrollable Virtualized Track Strips
    const float startX = 195.0f;
    const float stripW = channelStripWidth_;
    const float gap = channelGap_;

    float rightBound = propertiesDrawer_.getPullTabBounds().x;

    for (size_t i = 0; i < channels_.size(); ++i) {
        float cx = startX + static_cast<float>(i) * (stripW + gap) - scrollX_;
        if (cx + stripW < 185.0f) continue;
        if (cx > rightBound) break;

        Rect2D stripRect(cx, masterBounds_.y, stripW, masterBounds_.h);
        renderChannelStrip(r, theme, channels_[i], stripRect, i, selectedChannel_ == static_cast<int>(i));
    }

    // 3. Sliding Track Properties Drawer (Reusing Decoupled Drawer)
    drawerData_.isMixerMode = true;
    drawerData_.isMasterSelected = isMasterSelected_;
    drawerData_.tab = TrackPropertiesTab::Track;

    if (isMasterSelected_) {
        drawerData_.masterEqEnabled = masterEqEnabled_;
        drawerData_.masterSubCut = masterSubCut_;
        drawerData_.masterLowGain = masterLowGain_;
        drawerData_.masterMidGain = masterMidGain_;
        drawerData_.masterHighGain = masterHighGain_;
        drawerData_.masterLimiterEnabled = masterLimiterEnabled_;
        drawerData_.masterCeilingDbfs = masterCeilingDbfs_;
        drawerData_.masterLimiterDrive = masterLimiterDrive_;
        drawerData_.masterTargetLufs = masterTargetLufs_;
        drawerData_.masterAudioFx = masterAudioFx_;
    } else {
        size_t chIdx = (selectedChannel_ >= 0 && static_cast<size_t>(selectedChannel_) < channels_.size())
            ? static_cast<size_t>(selectedChannel_) : 0;
        const auto& selCh = channels_[chIdx];
        drawerData_.trackIndex = static_cast<uint32_t>(chIdx);
        drawerData_.totalTracks = static_cast<uint32_t>(channels_.size());
        drawerData_.trackName = selCh.name;
        drawerData_.trackType = selCh.type;
        drawerData_.r = selCh.r;
        drawerData_.g = selCh.g;
        drawerData_.b = selCh.b;
        drawerData_.volume = selCh.fader;
        drawerData_.pan = selCh.pan;
        drawerData_.mute = selCh.mute;
        drawerData_.solo = selCh.solo;
        drawerData_.freeze = selCh.freeze;

        drawerData_.eqEnabled = selCh.eqEnabled;
        drawerData_.eqHpf = selCh.eqHpf;
        drawerData_.eqLowGain = selCh.eqLowGain;
        drawerData_.eqMidFreq = selCh.eqMidFreq;
        drawerData_.eqMidGain = selCh.eqMidGain;
        drawerData_.eqMidQ = selCh.eqMidQ;
        drawerData_.eqHighGain = selCh.eqHighGain;

        drawerData_.instrument = selCh.instrument;
        if (selCh.instrument.find("303") != std::string::npos) drawerData_.instrumentEngine = "tb303";
        else if (selCh.instrument.find("808") != std::string::npos) drawerData_.instrumentEngine = "tr808";
        else if (selCh.instrument.find("909") != std::string::npos) drawerData_.instrumentEngine = "tr909";
        else if (selCh.instrument.find("DX7") != std::string::npos) drawerData_.instrumentEngine = "dx7";
        else if (selCh.instrument.find("PIANO") != std::string::npos || selCh.instrument.find("Piano") != std::string::npos) drawerData_.instrumentEngine = "piano";
        else drawerData_.instrumentEngine = "synth";

        drawerData_.knob1 = selCh.knob1;
        drawerData_.knob2 = selCh.knob2;
        drawerData_.knob3 = selCh.knob3;
        drawerData_.knob4 = selCh.knob4;
        drawerData_.knob1Name = selCh.knob1Name;
        drawerData_.knob2Name = selCh.knob2Name;
        drawerData_.knob3Name = selCh.knob3Name;
        drawerData_.knob4Name = selCh.knob4Name;

        drawerData_.midiFx = selCh.midiFx;
        drawerData_.audioFx = selCh.audioFx;

        drawerData_.syncKnobsIfEmpty();
    }

    propertiesDrawer_.render(r, theme, drawerData_, ctx.mouseX, ctx.mouseY);

    // 4. Floating tactile tooltip badge on top
    renderTooltip(r, theme);
}

void MixerView::renderToolbar(BatchRenderer2D& r, const ThemeTokens& theme) {
    // Toolbar is hidden for clean authentic mixer view
}

void MixerView::renderBacklitLcd(BatchRenderer2D& r, float x, float y, float w, float h,
                                 const std::string& title, const std::string& leftText, const std::string& rightText,
                                 const Color& titleColor) {
    // Inset LCD bezel & well: #141210 / #3B3226
    drawRoundedRect(r, x, y, w, h, 4.0f, 0.08f, 0.07f, 0.06f, 1.0f);
    drawRoundedRectOutline(r, x, y, w, h, 4.0f, 0.23f, 0.20f, 0.15f, 0.9f, 1.4f);

    // Subtle dot matrix scan lines
    for (float gy = y + 4.0f; gy < y + h - 2.0f; gy += 3.5f) {
        drawLine(r, x + 4.0f, gy, x + w - 4.0f, gy, 0.13f, 0.11f, 0.09f, 0.4f, 1.0f);
    }

    // Top centered title: Vintage amber #FFB347
    float textW = static_cast<float>(title.length()) * 6.6f;
    float textX = x + std::max(4.0f, (w - textW) * 0.5f);
    drawText(r, title, textX, y + 4.0f, 10.5f, 1.0f, 0.70f, 0.28f, 1.0f);

    // Bottom row: Left status (pan / st-out) | Right status (vol %)
    drawText(r, leftText, x + 6.0f, y + 20.0f, 9.0f, 1.0f, 0.70f, 0.28f, 0.85f);
    float rw = static_cast<float>(rightText.length()) * 6.0f;
    drawText(r, rightText, x + w - rw - 6.0f, y + 20.0f, 9.0f, 1.0f, 0.70f, 0.28f, 0.85f);
}

void MixerView::renderRotaryPanKnob(BatchRenderer2D& r, float kx, float ky, float radius, float pan,
                                    const Color& accentColor) {
    // Outer shadow / well
    drawCircle(r, kx, ky, radius + 2.0f, 0.05f, 0.06f, 0.08f, 0.6f);
    // Dial chassis
    drawCircle(r, kx, ky, radius, 0.13f, 0.14f, 0.17f, 1.0f);
    drawCircleOutline(r, kx, ky, radius, 0.28f, 0.32f, 0.40f, 0.85f, 1.2f);
    // Inner bevel
    drawCircle(r, kx, ky, radius - 3.5f, 0.09f, 0.10f, 0.12f, 1.0f);

    // Radial indicator notch (-135 to +135 deg)
    float angle = pan * 2.356f;
    float sinA = std::sin(angle);
    float cosA = std::cos(angle);
    drawLine(r, kx + sinA * 3.5f, ky - cosA * 3.5f,
             kx + sinA * (radius - 2.0f), ky - cosA * (radius - 2.0f),
             accentColor.r, accentColor.g, accentColor.b, 1.0f, 2.0f);

    // Center cap
    drawCircle(r, kx, ky, 2.0f, 0.25f, 0.28f, 0.35f, 1.0f);
}

void MixerView::renderCenterButton(BatchRenderer2D& r, float bx, float by, float bw, float bh) {
    drawButton(r, Rect2D{bx, by, bw, bh}, "C",
               Color{0.11f, 0.12f, 0.15f, 0.95f}, Color{0.28f, 0.32f, 0.38f, 0.7f}, Color{0.80f, 0.84f, 0.90f, 0.95f},
               8.5f, 3.0f, 1.0f);
}

void MixerView::renderTooltip(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!showTooltip_ || tooltipText_.empty()) return;

    float tw = static_cast<float>(tooltipText_.length()) * 6.5f + 16.0f;
    float th = 20.0f;
    float tx = tooltipX_ - tw * 0.5f;
    float ty = tooltipY_ - 26.0f;

    // Drop shadow
    drawRoundedRect(r, tx + 1.0f, ty + 2.0f, tw, th, 4.0f, 0.0f, 0.0f, 0.0f, 0.45f);
    // Creamy white background
    drawRoundedRect(r, tx, ty, tw, th, 4.0f, 0.94f, 0.95f, 0.96f, 0.98f);
    drawRoundedRectOutline(r, tx, ty, tw, th, 4.0f, 0.60f, 0.64f, 0.70f, 0.8f, 1.0f);
    // Crisp dark text
    drawText(r, tooltipText_, tx + 8.0f, ty + 4.5f, 9.5f, 0.10f, 0.12f, 0.15f, 1.0f);
}

void MixerView::renderMasterStrip(BatchRenderer2D& r, const ThemeTokens& theme, const Rect2D& b) {
    float mX = (b.x == 0.0f) ? 12.0f : b.x;
    float mW = 140.0f;
    float mY = b.y;
    float stripH = b.h;

    // Master Chassis with Gold/Amber Border (Eatsbeats parity)
    drawRoundedRect(r, mX, mY, mW, stripH, 8.0f, 0.08f, 0.09f, 0.11f, 0.98f);
    drawRoundedRectOutline(r, mX, mY, mW, stripH, 8.0f, 1.0f, 0.70f, 0.10f, isMasterSelected_ ? 1.0f : 0.75f, isMasterSelected_ ? 2.0f : 1.5f);

    // Top Backlit LCD: [MASTER, st-out, vol%]
    int mVolPct = static_cast<int>(std::round(masterChannel_.fader * 100.0f));
    renderBacklitLcd(r, mX + 8.0f, mY + 8.0f, mW - 16.0f, 38.0f, "MASTER", "st-out", std::to_string(mVolPct) + "%", Color(1.0f, 0.70f, 0.28f));

    // Master Balance Control (Rotary Knob + "C" button)
    float kx = mX + mW * 0.5f;
    float ky = mY + 8.0f + 38.0f + 16.0f;
    renderRotaryPanKnob(r, kx, ky, 16.0f, masterChannel_.pan, Color(1.0f, 0.75f, 0.10f));
    renderCenterButton(r, kx - 9.0f, ky + 18.0f, 18.0f, 14.0f);

    // Fader & Meter Well
    float wellTopY = ky + 38.0f;
    float wellBottomY = mY + stripH - 12.0f;
    float wellH = std::max(100.0f, wellBottomY - wellTopY);

    // Master Fader on Left
    float fx = mX + 38.0f;
    drawLine(r, fx, wellTopY, fx, wellBottomY, 0.02f, 0.03f, 0.05f, 1.0f, 3.0f);
    drawText(r, "0.00", fx - 10.0f, wellBottomY - 2.0f, 7.5f, 0.40f, 0.44f, 0.50f, 0.8f);

    float mNormVol = std::clamp(masterChannel_.fader / 1.5f, 0.0f, 1.0f);
    float mFy = wellBottomY - mNormVol * (wellBottomY - wellTopY);
    drawRoundedRect(r, fx - 18.0f, mFy - 9.0f, 36.0f, 18.0f, 3.0f, 0.32f, 0.34f, 0.40f, 1.0f);
    drawRoundedRectOutline(r, fx - 18.0f, mFy - 9.0f, 36.0f, 18.0f, 3.0f, 0.65f, 0.70f, 0.80f, 0.9f, 1.2f);
    drawLine(r, fx - 16.0f, mFy, fx + 16.0f, mFy, 1.0f, 0.75f, 0.10f, 1.0f, 2.0f);

    // Master Dual Stereo Meter on Right
    float mx = mX + 70.0f;
    float mw = 44.0f;
    renderLedMeter(r, theme, mx, wellTopY, mw, wellH,
                   masterChannel_.mute ? 0.0f : masterChannel_.peakL,
                   masterChannel_.mute ? 0.0f : masterChannel_.peakR,
                   masterChannel_.peakHoldL, masterChannel_.peakHoldR);
}

void MixerView::renderChannelStrip(BatchRenderer2D& r, const ThemeTokens& theme, MixerChannelStrip& ch,
                                   const Rect2D& b, size_t index, bool isSelected) {
    float cx = b.x;
    float cy = b.y;
    float stripW = 140.0f;
    float stripH = b.h;

    // Chassis
    Color bg = isSelected ? Color(0.10f, 0.11f, 0.14f) : Color(0.08f, 0.09f, 0.11f);
    drawRoundedRect(r, cx, cy, stripW, stripH, 8.0f, bg.r, bg.g, bg.b, 0.98f);
    if (isSelected) {
        // Glowing vibrant outline matching track color (Eatsbeats parity)
        drawRoundedRectOutline(r, cx, cy, stripW, stripH, 8.0f, ch.r, ch.g, ch.b, 1.0f, 2.0f);
    } else {
        drawRoundedRectOutline(r, cx, cy, stripW, stripH, 8.0f, 0.20f, 0.22f, 0.28f, 0.45f, 1.0f);
    }

    // Top Backlit LCD
    std::string panStr = (std::abs(ch.pan) < 0.03f) ? "center" : ((ch.pan < 0) ? ("L" + std::to_string(static_cast<int>(-ch.pan * 100))) : ("R" + std::to_string(static_cast<int>(ch.pan * 100))));
    int volPct = static_cast<int>(std::round(ch.fader * 100.0f));
    std::string titleStr = ch.name;
    std::transform(titleStr.begin(), titleStr.end(), titleStr.begin(), ::toupper);
    renderBacklitLcd(r, cx + 8.0f, cy + 8.0f, stripW - 16.0f, 38.0f, titleStr, panStr, std::to_string(volPct) + "%", Color(1.0f, 0.70f, 0.28f));

    // Pan Rotary Knob & "C" button
    float kx = cx + stripW * 0.5f;
    float ky = cy + 8.0f + 38.0f + 16.0f;
    renderRotaryPanKnob(r, kx, ky, 16.0f, ch.pan, Color(ch.r, ch.g, ch.b));
    renderCenterButton(r, kx - 9.0f, ky + 18.0f, 18.0f, 14.0f);

    // Lower Section: Fader (Left) + Stereo Meter (Center) + Hardware Buttons (Right)
    float wellTopY = ky + 38.0f;
    float wellBottomY = cy + stripH - 12.0f;
    float wellH = std::max(100.0f, wellBottomY - wellTopY);

    // 1. Vertical Console Fader
    float fx = cx + 26.0f;
    drawLine(r, fx, wellTopY, fx, wellBottomY, 0.02f, 0.03f, 0.05f, 1.0f, 3.0f);
    drawText(r, "0.00", fx - 10.0f, wellBottomY - 2.0f, 7.5f, 0.40f, 0.44f, 0.50f, 0.8f);

    float normVol = std::clamp(ch.fader / 1.5f, 0.0f, 1.0f);
    float fy = wellBottomY - normVol * (wellBottomY - wellTopY);
    drawRoundedRect(r, fx - 17.0f, fy - 9.0f, 34.0f, 18.0f, 3.0f, 0.32f, 0.34f, 0.40f, 1.0f);
    drawRoundedRectOutline(r, fx - 17.0f, fy - 9.0f, 34.0f, 18.0f, 3.0f, 0.65f, 0.70f, 0.80f, 0.9f, 1.2f);
    drawLine(r, fx - 15.0f, fy, fx + 15.0f, fy, ch.r, ch.g, ch.b, 1.0f, 2.0f);

    // 2. Inset Glass Dual Stereo Meter (Right of fader)
    float mx = cx + 50.0f;
    float mw = 38.0f;
    renderLedMeter(r, theme, mx, wellTopY, mw, wellH,
                   ch.mute ? 0.0f : ch.peakL,
                   ch.mute ? 0.0f : ch.peakR,
                   ch.peakHoldL, ch.peakHoldR);

    // 3. Compact Hardware Buttons Column (Far Right: M, S, *)
    float bx = cx + 96.0f;
    float by = wellTopY + 2.0f;
    float bw = 26.0f;
    float bh = 24.0f;

    // Mute (Red)
    if (ch.mute) {
        drawButton(r, Rect2D{bx, by, bw, bh}, "M", Color{0.92f, 0.15f, 0.20f, 1.0f}, Color{0.0f, 0.0f, 0.0f, 0.0f}, Color{1.0f, 1.0f, 1.0f, 1.0f}, 10.5f, 3.0f, 0.0f);
    } else {
        drawButton(r, Rect2D{bx, by, bw, bh}, "M", Color{0.12f, 0.14f, 0.18f, 0.95f}, Color{0.85f, 0.20f, 0.25f, 0.65f}, Color{0.90f, 0.22f, 0.28f, 1.0f}, 10.5f, 3.0f, 1.2f);
    }

    // Solo (Yellow)
    float sy = by + bh + 4.0f;
    if (ch.solo) {
        drawButton(r, Rect2D{bx, sy, bw, bh}, "S", Color{1.0f, 0.80f, 0.10f, 1.0f}, Color{0.0f, 0.0f, 0.0f, 0.0f}, Color{0.10f, 0.10f, 0.10f, 1.0f}, 10.5f, 3.0f, 0.0f);
    } else {
        drawButton(r, Rect2D{bx, sy, bw, bh}, "S", Color{0.12f, 0.14f, 0.18f, 0.95f}, Color{0.95f, 0.78f, 0.10f, 0.65f}, Color{0.95f, 0.78f, 0.10f, 1.0f}, 10.5f, 3.0f, 1.2f);
    }

    // Freeze (Cyan)
    float fzy = sy + bh + 4.0f;
    if (ch.freeze) {
        drawButton(r, Rect2D{bx, fzy, bw, bh}, "*", Color{0.13f, 0.96f, 0.91f, 1.0f}, Color{0.0f, 0.0f, 0.0f, 0.0f}, Color{0.05f, 0.10f, 0.15f, 1.0f}, 11.5f, 3.0f, 0.0f);
    } else {
        drawButton(r, Rect2D{bx, fzy, bw, bh}, "*", Color{0.12f, 0.14f, 0.18f, 0.95f}, Color{0.13f, 0.96f, 0.91f, 0.65f}, Color{0.13f, 0.96f, 0.91f, 1.0f}, 11.5f, 3.0f, 1.2f);
    }
}

void MixerView::renderLedMeter(BatchRenderer2D& r, const ThemeTokens& theme, float mx, float my, float mw, float mh,
                              float levelL, float levelR, float holdL, float holdR) {
    // Inset glass dark well
    drawRoundedRect(r, mx, my, mw, mh, 4.0f, 0.04f, 0.05f, 0.07f, 1.0f);
    drawRoundedRectOutline(r, mx, my, mw, mh, 4.0f, 0.18f, 0.20f, 0.27f, 0.9f, 1.4f);

    // Centered printed dB scale markings (Eatsbeats parity)
    static const char* dbLabels[11] = {
        "-inf", "-6", "-12", "-18", "-24", "-30", "-36", "-42", "-48", "-54", "-60"
    };

    for (int i = 0; i < 11; ++i) {
        float frac = static_cast<float>(i) / 10.0f;
        float lblY = my + 6.0f + frac * (mh - 16.0f);
        const char* lbl = dbLabels[i];
        float lw = static_cast<float>(std::strlen(lbl)) * 4.8f;
        drawText(r, lbl, mx + (mw - lw) * 0.5f, lblY - 3.5f, 7.0f, 0.48f, 0.52f, 0.61f, 0.85f);
    }

    // Dual LED columns
    const float ledW = 4.5f;
    const float leftX = mx + 3.5f;
    const float rightX = mx + mw - 3.5f - ledW;

    const int totalSegs = 22;
    const float segGap = 1.0f;
    float segH = (mh - 6.0f - (totalSegs - 1) * segGap) / static_cast<float>(totalSegs);

    for (int s = 0; s < totalSegs; ++s) {
        float thresh = static_cast<float>(s + 1) / static_cast<float>(totalSegs);
        bool onL = (levelL >= thresh - (1.0f / totalSegs) * 0.5f);
        bool onR = (levelR >= thresh - (1.0f / totalSegs) * 0.5f);

        int yIndexFromTop = totalSegs - 1 - s;
        float sy = my + 3.0f + yIndexFromTop * (segH + segGap);

        float lr, lg, lb, ur, ug, ub;
        if (thresh > 0.88f) {
            lr = 1.0f; lg = 0.09f; lb = 0.27f;
            ur = 0.17f; ug = 0.03f; ub = 0.05f;
        } else if (thresh > 0.70f) {
            lr = 1.0f; lg = 0.75f; lb = 0.03f;
            ur = 0.16f; ug = 0.11f; ub = 0.03f;
        } else {
            lr = 0.0f; lg = 0.90f; lb = 0.46f;
            ur = 0.03f; ug = 0.13f; ub = 0.07f;
        }

        drawRect(r, leftX, sy, ledW, segH, onL ? lr : ur, onL ? lg : ug, onL ? lb : ub, 1.0f);
        drawRect(r, rightX, sy, ledW, segH, onR ? lr : ur, onR ? lg : ug, onR ? lb : ub, 1.0f);
    }

    // Top Peak Clip Indicators
    if (levelL >= 0.95f) {
        drawRect(r, leftX, my + 1.0f, ledW, 2.5f, 1.0f, 0.0f, 0.33f, 1.0f);
    }
    if (levelR >= 0.95f) {
        drawRect(r, rightX, my + 1.0f, ledW, 2.5f, 1.0f, 0.0f, 0.33f, 1.0f);
    }

    // Diagonal Glass Glare Reflection Overlay
    drawRect(r, mx + 2.0f, my + 2.0f, mw - 4.0f, (mh - 4.0f) * 0.35f, 1.0f, 1.0f, 1.0f, 0.04f);
    drawLine(r, mx + 2.0f, my + (mh - 4.0f) * 0.35f, mx + mw - 2.0f, my + 2.0f, 1.0f, 1.0f, 1.0f, 0.12f, 1.0f);
}

bool MixerView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    // 1. Sliding Track Properties Drawer (Pull-tab, Drag-resize, Controls)
    if (propertiesDrawer_.handlePointer(ev, drawerData_, ctx)) {
        return true;
    }

    // 2. Master Strip Pointer Events
    float mX = (masterBounds_.x == 0.0f) ? 12.0f : masterBounds_.x;
    float mW = 140.0f;
    float mY = masterBounds_.y;
    float stripH = masterBounds_.h;

    if (masterBounds_.contains(ev.x, ev.y) || activeFaderIndex_ == -1 || activePanIndex_ == -1) {
        float kx = mX + mW * 0.5f;
        float ky = mY + 8.0f + 38.0f + 16.0f;
        float wellTopY = ky + 38.0f;
        float wellBottomY = mY + stripH - 12.0f;

        if (ev.action == PointerAction::Down) {
            // Right-click manual value edit dialog
            if (ev.button == PointerButton::Right) {
                // Master Fader slot / cap
                if ((ev.x >= mX + 16.0f && ev.x <= mX + 60.0f && ev.y >= wellTopY && ev.y <= wellBottomY) ||
                    (ev.x >= 60.0f && ev.x <= 115.0f && ev.y >= 260.0f)) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = "MASTER VOLUME";
                        req.paramName = "Master Volume";
                        req.currentValue = masterChannel_.fader;
                        req.minValue = 0.0f;
                        req.maxValue = 1.5f;
                        req.defaultValue = 0.85f;
                        req.hasDefault = true;
                        req.allowPercentage = true;
                        req.accentColor = ctx.theme ? ctx.theme->primaryAccent : Color(1.0f, 0.70f, 0.10f);
                        req.onCommit = [this, ctx](float val) {
                            masterChannel_.fader = val;
                            if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(val);
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }
                // Master Pan knob
                if (std::hypot(ev.x - kx, ev.y - ky) <= 18.0f || std::hypot(ev.x - 107.0f, ev.y - 215.0f) <= 18.0f) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = "MASTER PAN";
                        req.paramName = "Master Pan";
                        req.currentValue = masterChannel_.pan;
                        req.minValue = -1.0f;
                        req.maxValue = 1.0f;
                        req.defaultValue = 0.0f;
                        req.hasDefault = true;
                        req.allowPercentage = false;
                        req.accentColor = ctx.theme ? ctx.theme->primaryAccent : Color(1.0f, 0.70f, 0.10f);
                        req.onCommit = [this](float val) {
                            masterChannel_.pan = val;
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }
            }

            // Left-click on LCD Volume % readout -> Open manual edit dialog
            if (ev.x >= mX + mW - 50.0f && ev.x <= mX + mW - 8.0f && ev.y >= mY + 8.0f && ev.y <= mY + 46.0f) {
                if (ctx.onOpenValueEdit) {
                    ValueEditRequest req;
                    req.title = "MASTER VOLUME";
                    req.paramName = "Master Volume";
                    req.currentValue = masterChannel_.fader;
                    req.minValue = 0.0f;
                    req.maxValue = 1.5f;
                    req.defaultValue = 0.85f;
                    req.hasDefault = true;
                    req.allowPercentage = true;
                    req.accentColor = ctx.theme ? ctx.theme->primaryAccent : Color(1.0f, 0.70f, 0.10f);
                    req.onCommit = [this, ctx](float val) {
                        masterChannel_.fader = val;
                        if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(val);
                    };
                    ctx.onOpenValueEdit(req);
                }
                return true;
            }

            // LCD Screen tap -> Select Master & Expand Sidebar
            if ((ev.x >= mX + 8.0f && ev.x <= mX + mW - 8.0f && ev.y >= mY + 8.0f && ev.y <= mY + 46.0f) ||
                (ev.x >= 48.0f && ev.x <= 167.0f && ev.y >= 116.0f && ev.y <= 150.0f)) {
                isMasterSelected_ = true;
                propertiesDrawer_.setExpanded(true);
                return true;
            }
            // "C" button tap -> Center Pan
            if (ev.x >= kx - 12.0f && ev.x <= kx + 12.0f && ev.y >= ky + 16.0f && ev.y <= ky + 34.0f) {
                masterChannel_.pan = 0.0f;
                return true;
            }
            // Pan knob tap
            if (std::hypot(ev.x - kx, ev.y - ky) <= 18.0f || std::hypot(ev.x - 107.0f, ev.y - 215.0f) <= 18.0f) {
                activePanIndex_ = -1;
                dragStartX_ = ev.x;
                initialPanVal_ = masterChannel_.pan;
                return true;
            }
            // Mute button (legacy test support)
            if (ev.x >= 54.0f && ev.x <= 104.0f && ev.y >= 152.0f && ev.y <= 176.0f) {
                masterChannel_.mute = !masterChannel_.mute;
                if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(masterChannel_.mute ? 0.0f : masterChannel_.fader);
                return true;
            }
            // Master Fader tap
            if ((ev.x >= mX + 16.0f && ev.x <= mX + 60.0f && ev.y >= wellTopY && ev.y <= wellBottomY) ||
                (ev.x >= 60.0f && ev.x <= 115.0f && ev.y >= 260.0f)) {
                activeFaderIndex_ = -1;
                dragStartY_ = ev.y;
                initialFaderVal_ = masterChannel_.fader;
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Master Volume: %.2f", masterChannel_.fader);
                tooltipText_ = buf;
                tooltipX_ = mX + 60.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            }
        } else if (ev.action == PointerAction::Move) {
            if (activeFaderIndex_ == -1) {
                float dy = dragStartY_ - ev.y;
                float newFader = std::clamp(initialFaderVal_ + (dy / 200.0f), 0.0f, 1.5f);
                masterChannel_.fader = newFader;
                if (ctx.audioEngine) ctx.audioEngine->setMasterVolume(newFader);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Master Volume: %.2f", newFader);
                tooltipText_ = buf;
                tooltipX_ = mX + 60.0f;
                tooltipY_ = ev.y;
                showTooltip_ = true;
                return true;
            }
            if (activePanIndex_ == -1) {
                float dx = ev.x - dragStartX_;
                masterChannel_.pan = std::clamp(initialPanVal_ + dx / 100.0f, -1.0f, 1.0f);
                return true;
            }
        }
    }

    // 3. Channel Strips Pointer Events
    const float startX = 195.0f;
    const float stripW = channelStripWidth_;
    const float gap = channelGap_;

    for (size_t i = 0; i < channels_.size(); ++i) {
        float cx = startX + static_cast<float>(i) * (stripW + gap) - scrollX_;
        Rect2D stripRect(cx, masterBounds_.y, stripW, masterBounds_.h);

        float kx = cx + stripW * 0.5f;
        float ky = masterBounds_.y + 8.0f + 38.0f + 16.0f;
        float wellTopY = ky + 38.0f;
        float wellBottomY = masterBounds_.y + masterBounds_.h - 12.0f;
        float bx = cx + 96.0f;
        float by = wellTopY + 2.0f;

        if (stripRect.contains(ev.x, ev.y) || activeFaderIndex_ == static_cast<int>(i) || activePanIndex_ == static_cast<int>(i)) {
            if (ev.action == PointerAction::Down) {
                selectedChannel_ = static_cast<int>(i);
                isMasterSelected_ = false;
                if (onTrackSelected) onTrackSelected(static_cast<uint32_t>(i));

                // Right-click manual value edit dialog
                if (ev.button == PointerButton::Right) {
                    // Channel Fader slot / cap
                    if ((ev.x >= cx + 6.0f && ev.x <= cx + 46.0f && ev.y >= wellTopY && ev.y <= wellBottomY) ||
                        (ev.x >= cx + 10.0f && ev.x <= cx + 75.0f && ev.y >= 260.0f)) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = channels_[i].name + " VOLUME";
                            req.paramName = channels_[i].name + " Volume";
                            req.currentValue = channels_[i].fader;
                            req.minValue = 0.0f;
                            req.maxValue = 1.5f;
                            req.defaultValue = 0.80f;
                            req.hasDefault = true;
                            req.allowPercentage = true;
                            req.accentColor = Color(channels_[i].r, channels_[i].g, channels_[i].b, 1.0f);
                            req.onCommit = [this, i](float val) {
                                channels_[i].fader = val;
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                    // Channel Pan knob
                    if (std::hypot(ev.x - kx, ev.y - ky) <= 18.0f || std::hypot(ev.x - (cx + 65.0f), ev.y - 215.0f) <= 18.0f) {
                        if (ctx.onOpenValueEdit) {
                            ValueEditRequest req;
                            req.title = channels_[i].name + " PAN";
                            req.paramName = channels_[i].name + " Pan";
                            req.currentValue = channels_[i].pan;
                            req.minValue = -1.0f;
                            req.maxValue = 1.0f;
                            req.defaultValue = 0.0f;
                            req.hasDefault = true;
                            req.allowPercentage = false;
                            req.accentColor = Color(channels_[i].r, channels_[i].g, channels_[i].b, 1.0f);
                            req.onCommit = [this, i](float val) {
                                channels_[i].pan = val;
                            };
                            ctx.onOpenValueEdit(req);
                        }
                        return true;
                    }
                }

                // Left-click on Channel LCD Volume % readout
                if (ev.x >= cx + stripW - 50.0f && ev.x <= cx + stripW - 8.0f && ev.y >= masterBounds_.y + 8.0f && ev.y <= masterBounds_.y + 46.0f) {
                    if (ctx.onOpenValueEdit) {
                        ValueEditRequest req;
                        req.title = channels_[i].name + " VOLUME";
                        req.paramName = channels_[i].name + " Volume";
                        req.currentValue = channels_[i].fader;
                        req.minValue = 0.0f;
                        req.maxValue = 1.5f;
                        req.defaultValue = 0.80f;
                        req.hasDefault = true;
                        req.allowPercentage = true;
                        req.accentColor = Color(channels_[i].r, channels_[i].g, channels_[i].b, 1.0f);
                        req.onCommit = [this, i](float val) {
                            channels_[i].fader = val;
                        };
                        ctx.onOpenValueEdit(req);
                    }
                    return true;
                }

                // Card click or Backlit LCD click -> Select track & open sidebar (NO Edit view!)
                if ((ev.x >= cx + 8.0f && ev.x <= cx + stripW - 8.0f && ev.y >= masterBounds_.y + 8.0f && ev.y <= masterBounds_.y + 46.0f) ||
                    (ev.x >= cx + 8.0f && ev.x <= cx + stripW - 8.0f && ev.y >= 116.0f && ev.y <= 150.0f)) {
                    propertiesDrawer_.setExpanded(true);
                    return true;
                }
                // "C" button tap -> Center Pan
                if (ev.x >= kx - 12.0f && ev.x <= kx + 12.0f && ev.y >= ky + 16.0f && ev.y <= ky + 34.0f) {
                    channels_[i].pan = 0.0f;
                    return true;
                }
                // Pan knob tap
                if (std::hypot(ev.x - kx, ev.y - ky) <= 18.0f || std::hypot(ev.x - (cx + 65.0f), ev.y - 215.0f) <= 18.0f) {
                    activePanIndex_ = static_cast<int>(i);
                    dragStartX_ = ev.x;
                    initialPanVal_ = channels_[i].pan;
                    return true;
                }
                // Mute button: [bx, by, 26, 24] or [cx + 12, 152, 50, 24]
                if ((ev.x >= bx && ev.x <= bx + 28.0f && ev.y >= by && ev.y <= by + 26.0f) ||
                    (ev.x >= cx + 12.0f && ev.x <= cx + 62.0f && ev.y >= 152.0f && ev.y <= 176.0f)) {
                    channels_[i].mute = !channels_[i].mute;
                    if (onMuteToggled) onMuteToggled(static_cast<uint32_t>(i), channels_[i].mute);
                    if (ctx.audioEngine) ctx.audioEngine->setTrackMute(static_cast<uint32_t>(i), channels_[i].mute);
                    return true;
                }
                // Solo button: [bx, by + 26, 26, 24] or [cx + 68, 152, 50, 24]
                if ((ev.x >= bx && ev.x <= bx + 28.0f && ev.y >= by + 26.0f && ev.y <= by + 52.0f) ||
                    (ev.x >= cx + 68.0f && ev.x <= cx + 118.0f && ev.y >= 152.0f && ev.y <= 176.0f)) {
                    channels_[i].solo = !channels_[i].solo;
                    if (onSoloToggled) onSoloToggled(static_cast<uint32_t>(i), channels_[i].solo);
                    if (ctx.audioEngine) ctx.audioEngine->setTrackSolo(static_cast<uint32_t>(i), channels_[i].solo);
                    return true;
                }
                // Freeze button: [bx, by + 52, 26, 24] or [cx + 80, 321, 26, 20]
                if ((ev.x >= bx && ev.x <= bx + 28.0f && ev.y >= by + 52.0f && ev.y <= by + 78.0f) ||
                    (ev.x >= cx + 80.0f && ev.x <= cx + 106.0f && ev.y >= 321.0f && ev.y <= 341.0f)) {
                    channels_[i].freeze = !channels_[i].freeze;
                    return true;
                }
                // Fader slot tap
                if ((ev.x >= cx + 6.0f && ev.x <= cx + 46.0f && ev.y >= wellTopY && ev.y <= wellBottomY) ||
                    (ev.x >= cx + 10.0f && ev.x <= cx + 75.0f && ev.y >= 260.0f)) {
                    activeFaderIndex_ = static_cast<int>(i);
                    dragStartY_ = ev.y;
                    initialFaderVal_ = channels_[i].fader;
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s Volume: %.2f", channels_[i].name.c_str(), channels_[i].fader);
                    tooltipText_ = buf;
                    tooltipX_ = cx + 45.0f;
                    tooltipY_ = ev.y;
                    showTooltip_ = true;
                    return true;
                }

                // General card tap -> Select track & open sidebar
                propertiesDrawer_.setExpanded(true);
                return true;
            } else if (ev.action == PointerAction::Move) {
                if (activeFaderIndex_ == static_cast<int>(i)) {
                    float dy = dragStartY_ - ev.y;
                    float newFader = std::clamp(initialFaderVal_ + (dy / 200.0f), 0.0f, 1.5f);
                    channels_[i].fader = newFader;
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "%s Volume: %.2f", channels_[i].name.c_str(), newFader);
                    tooltipText_ = buf;
                    tooltipX_ = cx + 45.0f;
                    tooltipY_ = ev.y;
                    showTooltip_ = true;
                    return true;
                }
                if (activePanIndex_ == static_cast<int>(i)) {
                    float dx = ev.x - dragStartX_;
                    channels_[i].pan = std::clamp(initialPanVal_ + dx / 100.0f, -1.0f, 1.0f);
                    return true;
                }
            }
        }
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        if (activeFaderIndex_ != -2 || activePanIndex_ != -2 || showTooltip_) {
            activeFaderIndex_ = -2;
            activePanIndex_ = -2;
            showTooltip_ = false;
            return true;
        }
    }

    // Horizontal Scroll
    if (ev.action == PointerAction::Scroll && channelsScrollBounds_.contains(ev.x, ev.y)) {
        scrollX_ -= ev.scrollX * 30.0f;
        scrollX_ = std::max(0.0f, scrollX_);
        return true;
    }

    return false;
}

bool MixerView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (propertiesDrawer_.isPluginDialogOpen()) {
        return propertiesDrawer_.handleKey(key, scancode, action, mods, ctx);
    }
    return false;
}

} // namespace eatsbits::ui
