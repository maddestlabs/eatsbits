#include "eatsbits/ui/widgets/fullscreen_device_modal.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/icon_registry.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace eatsbits::ui {

FullscreenDeviceModal::FullscreenDeviceModal() {
    trackData_.knobs = {
        {"tuning", "TUNING", 0.50f, "440 Hz"},
        {"cutoff", "CUTOFF", 0.65f, "65%"},
        {"resonance", "RESON", 0.70f, "70%"},
        {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
        {"decay", "DECAY", 0.45f, "225 ms"},
        {"accent", "ACCENT", 0.75f, "75%"}
    };

    pluginDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        (void)mode;
        trackData_.instrument = entry.name;
        trackData_.presetTitle = entry.name;
        trackData_.instrumentEngine = entry.engineTag;
        trackData_.knobs.clear();
        trackData_.syncKnobsIfEmpty();
        knobSlots_.clear();
        guiPanel_.rows.clear();
        syncGuiPanelFromTrackData();
        pluginDialog_.close();
        if (onOpenPresetDialog) onOpenPresetDialog(trackIndex);
    };
    syncGuiPanelFromTrackData();
    pluginDialog_.onClose = [this]() {
        pluginDialog_.close();
    };
}

void FullscreenDeviceModal::open(const DeviceTarget& target) noexcept {
    target_ = target;
    isOpen_ = true;
    isDraggingKnob_ = false;
    activeKnobIndex_ = -1;
    pluginDialog_.close();
    syncGuiPanelFromTrackData();
}

void FullscreenDeviceModal::close() noexcept {
    isOpen_ = false;
    isDraggingKnob_ = false;
    activeKnobIndex_ = -1;
    pluginDialog_.close();
    if (onClose) {
        onClose();
    }
}

void FullscreenDeviceModal::toggle(const DeviceTarget& target) noexcept {
    if (isOpen_ && target_.trackIndex == target.trackIndex && target_.type == target.type && target_.fxIndex == target.fxIndex) {
        close();
    } else {
        open(target);
    }
}

void FullscreenDeviceModal::syncData(const TrackPropertiesDrawerData& data) noexcept {
    trackData_ = data;
    trackData_.syncKnobsIfEmpty();
    syncGuiPanelFromTrackData();
}

void FullscreenDeviceModal::syncGuiPanelFromTrackData() noexcept {
    trackData_.syncKnobsIfEmpty();

    if (target_.type == DeviceTargetType::AudioFx) {
        TrackAudioFxItem fx;
        if (target_.fxIndex >= 0 && static_cast<size_t>(target_.fxIndex) < trackData_.audioFx.size()) {
            fx = trackData_.audioFx[target_.fxIndex];
        } else {
            fx.name = target_.deviceName.empty() ? "Audio FX Insert" : target_.deviceName;
            fx.type = "AUDIO_FX";
            fx.ensureDefaultKnobs();
        }
        fx.ensureDefaultKnobs();

        guiPanel_.title = fx.name;
        guiPanel_.subtitle = "Studio DSP Insert Effect • " + fx.type + " • Unit " + std::to_string(std::max(1, target_.fxIndex + 1));

        if (fx.background == "snes") {
            guiPanel_.chassisStyle = GuiChassisStyle::Snes;
        } else if (fx.background == "grunge") {
            guiPanel_.chassisStyle = GuiChassisStyle::Grunge;
        } else if (fx.background == "silver") {
            guiPanel_.chassisStyle = GuiChassisStyle::Silver;
        } else if (fx.background == "dark") {
            guiPanel_.chassisStyle = GuiChassisStyle::DarkChassis;
        } else if (fx.background == "minimal_white") {
            guiPanel_.chassisStyle = GuiChassisStyle::Walnut;
        } else {
            guiPanel_.chassisStyle = GuiChassisStyle::BrushedSteel;
        }

        if (fx.accentR > 0.01f || fx.accentG > 0.01f || fx.accentB > 0.01f) {
            guiPanel_.accentColor = Color(fx.accentR, fx.accentG, fx.accentB, 1.0f);
        } else {
            guiPanel_.accentColor = Color(0.15f, 0.85f, 0.95f, 1.0f);
        }
        guiPanel_.woodCheeks = true;
        guiPanel_.cornerRadius = 8.0f;

        guiPanel_.rows.clear();
        GuiRowDef r1;

        trackData_.knobs = fx.knobs;
        for (size_t i = 0; i < fx.knobs.size(); ++i) {
            const auto& k = fx.knobs[i];
            GuiWidgetDef w;
            w.id = "w_" + k.name;
            w.type = GuiWidgetType::Knob;
            w.label = k.label;
            w.param = k.name;
            w.knobStyle = static_cast<GuiKnobStyle>(i % 5);
            w.size = 64.0f;
            w.currentVal = k.value;
            w.minVal = 0.0f;
            w.maxVal = 1.0f;
            w.unit = k.unit;
            w.accentColor = guiPanel_.accentColor;
            r1.widgets.push_back(w);
        }
        guiPanel_.rows.push_back(r1);
        return;
    }

    if (target_.type == DeviceTargetType::MidiFx) {
        TrackMidiFxItem fx;
        if (target_.fxIndex >= 0 && static_cast<size_t>(target_.fxIndex) < trackData_.midiFx.size()) {
            fx = trackData_.midiFx[target_.fxIndex];
        } else {
            fx.name = target_.deviceName.empty() ? "MIDI FX Processor" : target_.deviceName;
            fx.type = "MIDI_FX";
            fx.ensureDefaultKnobs();
        }
        fx.ensureDefaultKnobs();

        guiPanel_.title = fx.name;
        guiPanel_.subtitle = "MIDI Real-Time Transform Processor • " + fx.type + " • Unit " + std::to_string(std::max(1, target_.fxIndex + 1));
        guiPanel_.accentColor = Color(0.95f, 0.60f, 0.15f, 1.0f);
        guiPanel_.chassisStyle = GuiChassisStyle::DarkChassis;
        guiPanel_.woodCheeks = true;
        guiPanel_.cornerRadius = 8.0f;

        guiPanel_.rows.clear();
        GuiRowDef r1;

        trackData_.knobs = fx.knobs;
        for (size_t i = 0; i < fx.knobs.size(); ++i) {
            const auto& k = fx.knobs[i];
            GuiWidgetDef w;
            w.id = "w_" + k.name;
            w.type = GuiWidgetType::Knob;
            w.label = k.label;
            w.param = k.name;
            w.knobStyle = static_cast<GuiKnobStyle>((i + 1) % 5);
            w.size = 64.0f;
            w.currentVal = k.value;
            w.minVal = 0.0f;
            w.maxVal = 1.0f;
            w.unit = k.unit;
            w.accentColor = guiPanel_.accentColor;
            r1.widgets.push_back(w);
        }
        guiPanel_.rows.push_back(r1);
        return;
    }

    guiPanel_.title = trackData_.presetTitle.empty() ? trackData_.instrument : trackData_.presetTitle;
    guiPanel_.subtitle = trackData_.presetSubtitle.empty() ? (trackData_.trackName + " • " + trackData_.instrument) : trackData_.presetSubtitle;
    guiPanel_.accentColor = Color(trackData_.r, trackData_.g, trackData_.b, 1.0f);

    const std::string& eng = trackData_.instrumentEngine;
    bool is303 = (eng == "tb303" || trackData_.instrument.find("303") != std::string::npos ||
                  guiPanel_.title.find("303") != std::string::npos);

    auto findKnobVal = [&](const std::string& p1, const std::string& p2, float def) -> float {
        for (const auto& k : trackData_.knobs) {
            if (_stricmp(k.name.c_str(), p1.c_str()) == 0 || _stricmp(k.label.c_str(), p1.c_str()) == 0 ||
                (!p2.empty() && (_stricmp(k.name.c_str(), p2.c_str()) == 0 || _stricmp(k.label.c_str(), p2.c_str()) == 0))) {
                return k.value;
            }
        }
        return def;
    };

    if (is303) {
        guiPanel_.chassisStyle = GuiChassisStyle::MinimalWhite;
        guiPanel_.woodCheeks = false;
        guiPanel_.hideHeader = true;
        guiPanel_.cornerRadius = 6.0f;

        if (guiPanel_.rows.size() != 2 || guiPanel_.rows[0].widgets.size() != 8) {
            guiPanel_.rows.clear();

            GuiRowDef r1;
            r1.widgets.push_back({"w_waveform", GuiWidgetType::Knob, "WAVEFORM", "waveform", GuiKnobStyle::Tb303SelectorSilver, 56.0f, findKnobVal("waveform", "wave", 0.0f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"div1", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r1.widgets.push_back({"w_pitch", GuiWidgetType::Knob, "PITCH", "pitch", GuiKnobStyle::Tb303Potentiometer, 56.0f, findKnobVal("pitch", "tuning", 0.5f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_cutoff", GuiWidgetType::Knob, "CUTOFF", "cutoff", GuiKnobStyle::Tb303Potentiometer, 56.0f, findKnobVal("cutoff", "", 0.65f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_res", GuiWidgetType::Knob, "RESONANCE", "resonance", GuiKnobStyle::Tb303Potentiometer, 56.0f, findKnobVal("resonance", "reson", 0.75f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_env", GuiWidgetType::Knob, "ENV MOD", "envMod", GuiKnobStyle::Tb303Potentiometer, 56.0f, findKnobVal("envMod", "env", 0.60f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_decay", GuiWidgetType::Knob, "DECAY", "decay", GuiKnobStyle::Tb303Potentiometer, 56.0f, findKnobVal("decay", "", 0.45f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r1.widgets.push_back({"w_accent", GuiWidgetType::Knob, "ACCENT", "accent", GuiKnobStyle::Tb303Potentiometer, 56.0f, findKnobVal("accent", "", 0.75f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            guiPanel_.rows.push_back(r1);

            GuiRowDef r2;
            r2.widgets.push_back({"w_octave", GuiWidgetType::Knob, "OCTAVE", "octave", GuiKnobStyle::Tb303SelectorBlack, 56.0f, findKnobVal("octave", "", 0.5f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"div2", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_subosc", GuiWidgetType::ToggleSwitch, "SUB OSC", "subOsc", GuiKnobStyle::Standard, 52.0f, findKnobVal("subOsc", "subWaveform", 0.0f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"w_subvol", GuiWidgetType::Knob, "SUB VOL", "subVol", GuiKnobStyle::MiniPotCream, 52.0f, findKnobVal("subVol", "subVolume", 0.35f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"div3", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_glide", GuiWidgetType::Knob, "GLIDE CURVE", "glideCurve", GuiKnobStyle::MiniPotCream, 52.0f, findKnobVal("glideCurve", "glide", 0.40f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            r2.widgets.push_back({"div4", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_drive", GuiWidgetType::Knob, "DRIVE", "drive", GuiKnobStyle::MiniPotCream, 52.0f, findKnobVal("drive", "overdrive", 0.25f), 0.0f, 1.0f, "", guiPanel_.accentColor});
            guiPanel_.rows.push_back(r2);
        } else {
            for (auto& row : guiPanel_.rows) {
                for (auto& w : row.widgets) {
                    if (w.type == GuiWidgetType::Divider) continue;
                    w.currentVal = findKnobVal(w.param, "", w.currentVal);
                }
            }
        }
        return;
    }

    guiPanel_.hideHeader = false;
    guiPanel_.woodCheeks = true;
    guiPanel_.cornerRadius = 8.0f;

    if (eng == "tr808" || eng == "tr909") {
        guiPanel_.chassisStyle = GuiChassisStyle::Grunge;
    } else if (eng == "dx7") {
        guiPanel_.chassisStyle = GuiChassisStyle::DarkChassis;
    } else if (eng == "piano" || eng == "piano_physical") {
        guiPanel_.chassisStyle = GuiChassisStyle::Walnut;
    } else if (eng == "snes") {
        guiPanel_.chassisStyle = GuiChassisStyle::Snes;
        guiPanel_.woodCheeks = false;
    } else if (eng == "c64") {
        guiPanel_.chassisStyle = GuiChassisStyle::PcbGreen;
        guiPanel_.woodCheeks = false;
    } else if (eng == "convolver") {
        guiPanel_.chassisStyle = GuiChassisStyle::BrushedSteel;
    } else {
        guiPanel_.chassisStyle = GuiChassisStyle::DarkChassis;
    }

    size_t numKnobs = trackData_.knobs.size();
    if (numKnobs == 0) {
        trackData_.knobs = {
            {"cutoff", "CUTOFF", 0.65f, "65%"},
            {"resonance", "RESON", 0.50f, "50%"},
            {"envMod", "ENV MOD", 0.60f, "+2.4 oct"},
            {"decay", "DECAY", 0.45f, "225 ms"},
            {"accent", "ACCENT", 0.70f, "70%"},
            {"tuning", "TUNING", 0.50f, "440 Hz"}
        };
        numKnobs = trackData_.knobs.size();
    }

    size_t displayKnobs = std::min(numKnobs, size_t{6});

    if (guiPanel_.rows.empty() || guiPanel_.rows[0].widgets.size() != displayKnobs) {
        guiPanel_.rows.clear();
        GuiRowDef r1;
        for (size_t i = 0; i < displayKnobs; ++i) {
            const auto& k = trackData_.knobs[i];
            GuiKnobStyle kStyle = GuiKnobStyle::Standard;
            if (eng == "tr808" || eng == "tr909") {
                if (i == 0) kStyle = GuiKnobStyle::BakeliteSkirt;
                else if (i == 1) kStyle = GuiKnobStyle::CreamFluted;
                else if (i == 2) kStyle = GuiKnobStyle::AnodizedKnurled;
                else if (i == 3) kStyle = GuiKnobStyle::TwoToneStepped;
                else if (i == 4) kStyle = GuiKnobStyle::Tb303Halo;
                else kStyle = GuiKnobStyle::Standard;
            } else if (eng == "dx7") {
                if (i == 0) kStyle = GuiKnobStyle::AnodizedKnurled;
                else if (i == 1) kStyle = GuiKnobStyle::TwoToneStepped;
                else if (i == 2) kStyle = GuiKnobStyle::BakeliteSkirt;
                else if (i == 3) kStyle = GuiKnobStyle::CreamFluted;
                else if (i == 4) kStyle = GuiKnobStyle::Standard;
                else kStyle = GuiKnobStyle::Tb303Halo;
            } else {
                kStyle = static_cast<GuiKnobStyle>(i % 5);
            }

            GuiWidgetDef w;
            w.id = "w_" + k.name;
            w.type = GuiWidgetType::Knob;
            w.label = k.label;
            w.param = k.name;
            w.knobStyle = kStyle;
            w.size = 64.0f;
            w.currentVal = k.value;
            w.minVal = 0.0f;
            w.maxVal = 1.0f;
            w.unit = "";
            w.accentColor = guiPanel_.accentColor;
            r1.widgets.push_back(w);
        }
        guiPanel_.rows.push_back(r1);

        if (numKnobs > 6) {
            GuiRowDef r2;
            for (size_t i = 6; i < numKnobs && i < 12; ++i) {
                const auto& k = trackData_.knobs[i];
                GuiWidgetDef w;
                w.id = "w_" + k.name;
                w.type = GuiWidgetType::Knob;
                w.label = k.label;
                w.param = k.name;
                w.knobStyle = static_cast<GuiKnobStyle>(i % 5);
                w.size = 56.0f;
                w.currentVal = k.value;
                w.minVal = 0.0f;
                w.maxVal = 1.0f;
                w.unit = "";
                w.accentColor = guiPanel_.accentColor;
                r2.widgets.push_back(w);
            }
            guiPanel_.rows.push_back(r2);
        }
    } else {
        // Sync values to existing widgets
        for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
            for (size_t wIdx = 0; wIdx < guiPanel_.rows[rIdx].widgets.size(); ++wIdx) {
                size_t kIdx = rIdx * 6 + wIdx;
                if (kIdx < trackData_.knobs.size()) {
                    auto& w = guiPanel_.rows[rIdx].widgets[wIdx];
                    if (!isDraggingKnob_ || draggingRow_ != static_cast<int>(rIdx) || draggingWidget_ != static_cast<int>(wIdx)) {
                        w.currentVal = trackData_.knobs[kIdx].value;
                        w.label = trackData_.knobs[kIdx].label;
                        w.param = trackData_.knobs[kIdx].name;
                        w.accentColor = guiPanel_.accentColor;
                    }
                }
            }
        }
    }
}

void FullscreenDeviceModal::setAudioScopeBuffer(const float* buffer, size_t count) noexcept {
    scopeBuffer_ = buffer;
    scopeBufferCount_ = count;
}

void FullscreenDeviceModal::layout(float screenW, float screenH) noexcept {
    screenWidth_ = std::max(640.0f, screenW);
    screenHeight_ = std::max(480.0f, screenH);

    backdropBounds_ = Rect2D{0.0f, 0.0f, screenWidth_, screenHeight_};

    // Header bar along the top
    float headerH = 44.0f;
    headerBounds_ = Rect2D{0.0f, 0.0f, screenWidth_, headerH};

    // 1. Universal Close Button (rotated screw icon, rightmost)
    float closeBtnSize = 28.0f;
    float closeX = screenWidth_ - closeBtnSize - 14.0f;
    float btnY = (headerH - closeBtnSize) * 0.5f;
    closeBtnBounds_ = Rect2D{closeX, btnY, closeBtnSize, closeBtnSize};

    // 2. Preset button linking to preset dialog (middle)
    float presetBtnW = 92.0f;
    float presetBtnH = 26.0f;
    float presetX = closeX - presetBtnW - 10.0f;
    float presetY = (headerH - presetBtnH) * 0.5f;
    presetBtnBounds_ = Rect2D{presetX, presetY, presetBtnW, presetBtnH};

    // 3. Design chip icon button (left of preset button)
    float designBtnSize = 28.0f;
    float designX = presetX - designBtnSize - 10.0f;
    float designY = (headerH - designBtnSize) * 0.5f;
    designBtnBounds_ = Rect2D{designX, designY, designBtnSize, designBtnSize};

    // Body fills the rest of the display with clean margins
    float marginX = 16.0f;
    float marginY = 12.0f;
    float bodyY = headerH + marginY;
    float bodyH = screenHeight_ - bodyY - marginY;
    float bodyW = screenWidth_ - (marginX * 2.0f);
    bodyBounds_ = Rect2D{marginX, bodyY, bodyW, bodyH};

    syncGuiPanelFromTrackData();

    // Allocate faceplate & oscilloscope responsively utilizing full available screen space:
    float oscH = std::clamp(bodyH * 0.18f, 75.0f, 150.0f);
    float gapY = 10.0f;
    float fpH = bodyH - oscH - gapY;
    float fpW = bodyW;
    float fpX = bodyBounds_.x;
    float fpY = bodyBounds_.y;
    Rect2D fpRect{fpX, fpY, fpW, fpH};
    guiPanel_.bounds = fpRect;

    float oscY = fpY + fpH + gapY;
    oscBounds_ = Rect2D{fpX, oscY, fpW, oscH};

    // Compute divider-aware widget bounds inside guiPanel_
    float fpHeaderH = (guiPanel_.hideHeader || guiPanel_.chassisStyle == GuiChassisStyle::MinimalWhite) ? 0.0f : 44.0f;
    float topPad = (fpHeaderH > 0.0f) ? (fpHeaderH + 12.0f) : 10.0f;
    float botPad = 8.0f;
    float rowStartY = fpRect.y + topPad;
    float rowAvailableH = fpH - topPad - botPad;
    float rowH = rowAvailableH / std::max(1, static_cast<int>(guiPanel_.rows.size()));

    for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
        auto& row = guiPanel_.rows[rIdx];
        float ry = rowStartY + (static_cast<float>(rIdx) * rowH);
        row.bounds = Rect2D{fpRect.x + 8.0f, ry, fpRect.w - 16.0f, rowH};

        size_t dividerCount = 0;
        for (const auto& wid : row.widgets) {
            if (wid.type == GuiWidgetType::Divider) dividerCount++;
        }
        float divWidth = std::clamp(row.bounds.w * 0.015f, 12.0f, 28.0f);
        float remainingW = row.bounds.w - (static_cast<float>(dividerCount) * divWidth);
        size_t nonDivCount = (row.widgets.size() > dividerCount) ? (row.widgets.size() - dividerCount) : 1;
        float normalColW = remainingW / static_cast<float>(nonDivCount);

        float curX = row.bounds.x;
        for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
            auto& w = row.widgets[wIdx];
            float itemW = (w.type == GuiWidgetType::Divider) ? divWidth : normalColW;
            w.bounds = Rect2D{curX, ry + 2.0f, itemW, rowH - 4.0f};
            curX += itemW;
        }
    }

    // Mirror Row 0's knobs into knobSlots_ for backward compatibility
    if (!guiPanel_.rows.empty()) {
        const auto& r0 = guiPanel_.rows[0];
        size_t displayKnobs = r0.widgets.size();
        knobSlots_.resize(displayKnobs);
        for (size_t i = 0; i < displayKnobs; ++i) {
            const auto& w = r0.widgets[i];
            knobSlots_[i].name = w.param;
            knobSlots_[i].label = w.label;
            knobSlots_[i].normVal = w.currentVal;
            knobSlots_[i].readout = (i < trackData_.knobs.size()) ? trackData_.knobs[i].display : "";
            knobSlots_[i].center = Point2D{w.bounds.x + w.bounds.w * 0.5f, w.bounds.y + w.bounds.h * 0.44f};
            knobSlots_[i].radius = std::clamp(std::min(w.bounds.w * 0.34f, w.bounds.h * 0.28f), 14.0f, 66.0f);
        }
    }

    if (pluginDialog_.isOpen()) {
        pluginDialog_.layout(screenWidth_, screenHeight_);
    }
}

void FullscreenDeviceModal::update(float dt) noexcept {
    pulsePhase_ += dt * 3.0f;
    if (pulsePhase_ > 6.2831853f) pulsePhase_ -= 6.2831853f;
}

void FullscreenDeviceModal::render(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    if (!isOpen_) return;

    // 1. Dark matte studio background (skipping all underlying DAW drawings!)
    Color bg1 = theme.backgroundDark.darken(0.12f);
    Color bg2 = theme.backgroundDark.darken(0.20f);
    r.drawRectGradient(backdropBounds_.x, backdropBounds_.y, backdropBounds_.w, backdropBounds_.h,
                       bg1.r, bg1.g, bg1.b, bg2.r, bg2.g, bg2.b, 1.0f);

    // Subtle track-tinted radial illumination
    float pulseAlpha = 0.05f + 0.02f * std::sin(pulsePhase_);
    drawCircle(r, screenWidth_ * 0.5f, screenHeight_ * 0.45f, screenWidth_ * 0.45f,
               trackData_.r, trackData_.g, trackData_.b, pulseAlpha);

    // 2. Top Machined Header Strip
    renderHeaderBar(r, theme);

    // 3. Main Device Body View - Unified Faceplate for Instruments, Audio FX, and MIDI FX
    renderInstrumentFaceplate(r, theme);

    // 4. Modal Preset / Plugin Search Dialog if open
    if (pluginDialog_.isOpen()) {
        pluginDialog_.render(r, theme);
    }
}

void FullscreenDeviceModal::renderHeaderBar(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    float hw = headerBounds_.w;
    float hh = headerBounds_.h;

    // Machined dark chassis plate
    drawRoundedRect(r, 0.0f, 0.0f, hw, hh, 0.0f, 0.10f, 0.11f, 0.14f, 0.98f);
    drawLine(r, 0.0f, hh, hw, hh, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.5f);
    drawLine(r, 0.0f, 1.0f, hw, 1.0f, 0.30f, 0.34f, 0.42f, 0.4f, 1.0f);

    // Left: Track color circle + Track Title (matching user screenshot)
    float dotX = 22.0f;
    float dotY = hh * 0.5f;
    float dotR = 6.0f;
    drawCircle(r, dotX, dotY, dotR + 3.0f, trackData_.r, trackData_.g, trackData_.b, 0.30f);
    drawCircle(r, dotX, dotY, dotR, trackData_.r, trackData_.g, trackData_.b, 1.0f);
    drawCircle(r, dotX - 1.5f, dotY - 1.5f, 2.0f, 1.0f, 1.0f, 1.0f, 0.85f);

    // Track Title (bold uppercase white)
    std::string trackTitle = trackData_.trackName;
    if (target_.type == DeviceTargetType::AudioFx || target_.type == DeviceTargetType::MidiFx) {
        trackTitle = trackData_.trackName + " • " + guiPanel_.title;
    } else if (trackTitle.empty()) {
        trackTitle = trackData_.instrument.empty() ? "SYNTHESIZER INSTRUMENT" : trackData_.instrument;
    }
    std::string upperTitle = trackTitle;
    for (char& c : upperTitle) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    drawText(r, upperTitle, dotX + 16.0f, hh * 0.5f - 6.0f, 12.5f, 0.96f, 0.97f, 0.98f, 1.0f);

    // Right: 1. Design Chip Icon Button
    drawRoundedRect(r, designBtnBounds_.x, designBtnBounds_.y, designBtnBounds_.w, designBtnBounds_.h, 4.0f,
                    0.13f, 0.14f, 0.18f, 0.95f);
    drawRoundedRectOutline(r, designBtnBounds_.x, designBtnBounds_.y, designBtnBounds_.w, designBtnBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.85f, 1.0f);
    float chipSize = 16.0f;
    float chipX = designBtnBounds_.x + (designBtnBounds_.w - chipSize) * 0.5f;
    float chipY = designBtnBounds_.y + (designBtnBounds_.h - chipSize) * 0.5f;
    Color chipCol = guiPanel_.accentColor;
    drawDesignChipIcon(r, chipX, chipY, chipSize, chipCol);

    // Right: 2. Preset Button linking to Preset Dialog
    drawRoundedRect(r, presetBtnBounds_.x, presetBtnBounds_.y, presetBtnBounds_.w, presetBtnBounds_.h, 4.0f,
                    0.13f, 0.14f, 0.18f, 0.95f);
    drawRoundedRectOutline(r, presetBtnBounds_.x, presetBtnBounds_.y, presetBtnBounds_.w, presetBtnBounds_.h, 4.0f,
                           chipCol.r * 0.6f, chipCol.g * 0.6f, chipCol.b * 0.6f, 0.9f, 1.2f);
    // Sliders icon inside preset button
    float tuneSize = 12.0f;
    float tuneX = presetBtnBounds_.x + 8.0f;
    float tuneY = presetBtnBounds_.y + (presetBtnBounds_.h - tuneSize) * 0.5f;
    drawSlidersTuneIcon(r, tuneX, tuneY, tuneSize, chipCol);

    // "PRESET" text
    drawText(r, "PRESET", tuneX + tuneSize + 6.0f, presetBtnBounds_.y + 7.5f, 10.0f,
             chipCol.r, chipCol.g, chipCol.b, 1.0f);

    // Down arrow ▾
    float triX = presetBtnBounds_.x + presetBtnBounds_.w - 12.0f;
    float triY = presetBtnBounds_.y + presetBtnBounds_.h * 0.5f - 1.0f;
    drawTriangle(r, triX - 3.5f, triY - 2.0f, triX + 3.5f, triY - 2.0f, triX, triY + 3.0f,
                 chipCol.r, chipCol.g, chipCol.b, 0.9f);

    // Right: 3. Universal Close Button (rotated screw icon)
    float cx = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float cy = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(mouseX_, mouseY_) || std::hypot(mouseX_ - cx, mouseY_ - cy) <= 12.0f;
    drawScrewCloseButton(r, cx, cy, 9.0f, closeHov, theme.primaryAccent);
}

void FullscreenDeviceModal::renderInstrumentFaceplate(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    // 1. Unified GUI Faceplate with authentic skeuomorphic hardware styling,
    // wood cheeks, 3D multi-stop radial gradient knobs, sliders, nixies, etc.
    drawGuiFaceplate(r, guiPanel_, guiPanel_.bounds, theme, scopeBuffer_, scopeBufferCount_, draggingRow_, draggingWidget_);

    // 2. Real-time CRT Audio Oscilloscope Display beneath the faceplate
    renderOscilloscope(r, oscBounds_.x, oscBounds_.y, oscBounds_.w, oscBounds_.h, theme);
}

void FullscreenDeviceModal::renderAudioFxRacks(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    float bx = bodyBounds_.x;
    float by = bodyBounds_.y;
    float bw = bodyBounds_.w;
    float bh = bodyBounds_.h;

    drawRoundedRect(r, bx, by, bw, bh, 8.0f, 0.11f, 0.13f, 0.17f, 1.0f);
    drawRoundedRectOutline(r, bx, by, bw, bh, 8.0f, theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.7f, 2.0f);

    drawText(r, "5-UNIT STUDIO FX RACK", bx + 24.0f, by + 18.0f, 10.0f, theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);
    drawText(r, "HIGH-FIDELITY INSERT MODULES • LOW-LATENCY DSP", bx + 24.0f, by + 32.0f, 13.0f, 0.90f, 0.95f, 1.0f, 1.0f);

    float unitH = (bh - 80.0f) / 5.0f;
    const char* fxNames[5] = {"1. TAPE ECHO / STEREO DELAY", "2. ANALOG MULTI-VOICE CHORUS", "3. 5-BAND PARAMETRIC EQ", "4. STUDIO BUS COMPRESSOR", "5. IMPULSE CONVOLVER REVERB"};
    bool fxActive[5] = {trackData_.audioFxData.delayEnabled, trackData_.audioFxData.chorusEnabled, trackData_.audioFxData.eqEnabled, trackData_.audioFxData.compEnabled, trackData_.audioFxData.convolverEnabled};

    for (int i = 0; i < 5; ++i) {
        float uy = by + 60.0f + static_cast<float>(i) * unitH;
        float uw = bw - 48.0f;
        float ux = bx + 24.0f;

        drawRoundedRect(r, ux, uy, uw, unitH - 8.0f, 4.0f, 0.14f, 0.16f, 0.20f, 0.95f);
        drawRoundedRectOutline(r, ux, uy, uw, unitH - 8.0f, 4.0f,
                               fxActive[i] ? theme.secondaryAccent.r : 0.25f,
                               fxActive[i] ? theme.secondaryAccent.g : 0.28f,
                               fxActive[i] ? theme.secondaryAccent.b : 0.35f, 0.8f, 1.2f);

        // Power LED
        drawCircle(r, ux + 18.0f, uy + (unitH - 8.0f) * 0.5f, 5.0f,
                   fxActive[i] ? theme.secondaryAccent.r : 0.3f,
                   fxActive[i] ? theme.secondaryAccent.g : 0.3f,
                   fxActive[i] ? theme.secondaryAccent.b : 0.3f, 1.0f);

        drawText(r, fxNames[i], ux + 32.0f, uy + (unitH - 8.0f) * 0.35f, 11.5f,
                 fxActive[i] ? 0.95f : 0.55f,
                 fxActive[i] ? 0.95f : 0.55f,
                 fxActive[i] ? 1.0f : 0.60f, 1.0f);

        drawText(r, fxActive[i] ? "[ ACTIVE ]" : "[ BYPASS ]", ux + uw - 90.0f, uy + (unitH - 8.0f) * 0.35f, 10.0f,
                 fxActive[i] ? theme.secondaryAccent.r : 0.5f,
                 fxActive[i] ? theme.secondaryAccent.g : 0.5f,
                 fxActive[i] ? theme.secondaryAccent.b : 0.5f, 1.0f);
    }
}

void FullscreenDeviceModal::renderMidiFxRacks(BatchRenderer2D& r, const ThemeTokens& theme) noexcept {
    float bx = bodyBounds_.x;
    float by = bodyBounds_.y;
    float bw = bodyBounds_.w;
    float bh = bodyBounds_.h;

    drawRoundedRect(r, bx, by, bw, bh, 8.0f, 0.11f, 0.13f, 0.17f, 1.0f);
    drawRoundedRectOutline(r, bx, by, bw, bh, 8.0f, 0.95f, 0.75f, 0.15f, 0.8f, 2.0f);

    drawText(r, "MIDI HARMONIC FX PROCESSOR", bx + 24.0f, by + 18.0f, 10.0f, 0.95f, 0.75f, 0.15f, 1.0f);
    drawText(r, "SCALE HARMONIZER • ARPEGGIATOR • GROOVE HUMANIZE", bx + 24.0f, by + 32.0f, 13.0f, 0.95f, 0.95f, 1.0f, 1.0f);

    float unitH = (bh - 80.0f) / 3.0f;
    const char* mNames[3] = {"1. POLYPHONIC PATTERN ARPEGGIATOR", "2. SCALE & KEY SNAP CONSTRAINER", "3. GROOVE & TIMING HUMANIZE ENGINE"};
    bool mActive[3] = {trackData_.midiFxData.arpEnabled, trackData_.midiFxData.scaleSnapEnabled, trackData_.midiFxData.humanizeEnabled};

    for (int i = 0; i < 3; ++i) {
        float uy = by + 60.0f + static_cast<float>(i) * unitH;
        float uw = bw - 48.0f;
        float ux = bx + 24.0f;

        drawRoundedRect(r, ux, uy, uw, unitH - 12.0f, 4.0f, 0.14f, 0.16f, 0.20f, 0.95f);
        drawRoundedRectOutline(r, ux, uy, uw, unitH - 12.0f, 4.0f,
                               mActive[i] ? 0.95f : 0.25f,
                               mActive[i] ? 0.75f : 0.28f,
                               mActive[i] ? 0.15f : 0.35f, 0.8f, 1.2f);

        drawCircle(r, ux + 18.0f, uy + (unitH - 12.0f) * 0.5f, 5.0f,
                   mActive[i] ? 0.95f : 0.3f,
                   mActive[i] ? 0.75f : 0.3f,
                   mActive[i] ? 0.15f : 0.3f, 1.0f);

        drawText(r, mNames[i], ux + 32.0f, uy + (unitH - 12.0f) * 0.35f, 12.0f,
                 mActive[i] ? 0.95f : 0.55f,
                 mActive[i] ? 0.95f : 0.55f,
                 mActive[i] ? 1.0f : 0.60f, 1.0f);

        drawText(r, mActive[i] ? "[ ACTIVE ]" : "[ BYPASS ]", ux + uw - 90.0f, uy + (unitH - 12.0f) * 0.35f, 10.0f,
                 mActive[i] ? 0.95f : 0.5f,
                 mActive[i] ? 0.75f : 0.5f,
                 mActive[i] ? 0.15f : 0.5f, 1.0f);
    }
}

void FullscreenDeviceModal::renderOscilloscope(BatchRenderer2D& r, float ox, float oy, float ow, float oh, const ThemeTokens& theme) noexcept {
    (void)theme;
    // CRT phosphor bezel
    drawRoundedRect(r, ox, oy, ow, oh, 6.0f, 0.03f, 0.05f, 0.04f, 0.95f);
    drawRoundedRectOutline(r, ox, oy, ow, oh, 6.0f, 0.15f, 0.60f, 0.30f, 0.6f, 1.5f);

    // Center reticle
    drawLine(r, ox, oy + oh * 0.5f, ox + ow, oy + oh * 0.5f, 0.10f, 0.30f, 0.18f, 0.5f, 1.0f);
    drawLine(r, ox + ow * 0.5f, oy, ox + ow * 0.5f, oy + oh, 0.10f, 0.30f, 0.18f, 0.5f, 1.0f);

    drawText(r, "REAL-TIME OSCILLOSCOPE FEED", ox + 12.0f, oy + 10.0f, 8.5f, 0.20f, 0.85f, 0.40f, 0.8f);

    // Waveform line
    if (scopeBuffer_ && scopeBufferCount_ > 1) {
        size_t pts = std::min(scopeBufferCount_, size_t{128});
        float stepX = ow / static_cast<float>(pts - 1);
        float midY = oy + oh * 0.5f;

        for (size_t i = 1; i < pts; ++i) {
            float x1 = ox + static_cast<float>(i - 1) * stepX;
            float y1 = midY - (scopeBuffer_[i - 1] * (oh * 0.42f));
            float x2 = ox + static_cast<float>(i) * stepX;
            float y2 = midY - (scopeBuffer_[i] * (oh * 0.42f));
            drawLine(r, x1, y1, x2, y2, 0.0f, 1.0f, 0.50f, 0.95f, 1.8f);
        }
    } else {
        // Flat baseline
        drawLine(r, ox + 10.0f, oy + oh * 0.5f, ox + ow - 10.0f, oy + oh * 0.5f, 0.0f, 0.80f, 0.40f, 0.6f, 1.2f);
    }
}

bool FullscreenDeviceModal::handlePointer(const PointerEvent& ev) noexcept {
    if (!isOpen_) return false;

    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handlePointer(ev)) return true;
        return true;
    }

    mouseX_ = ev.x;
    mouseY_ = ev.y;
    float mx = ev.x;
    float my = ev.y;

    if (ev.action == PointerAction::Scroll) {
        for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
            auto& row = guiPanel_.rows[rIdx];
            for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                auto& w = row.widgets[wIdx];
                if (w.type == GuiWidgetType::Divider) continue;
                float cx = w.bounds.x + (w.bounds.w * 0.5f);
                float cy = w.bounds.y + (w.bounds.h * 0.44f);
                float dist = std::hypot(mx - cx, my - cy);
                if (w.bounds.contains(mx, my) || dist <= (w.bounds.w * 0.5f)) {
                    float newVal = std::clamp(w.currentVal + ev.scrollY * 0.04f, 0.0f, 1.0f);
                    w.currentVal = newVal;
                    size_t kIdx = rIdx * 6 + wIdx;
                    if (kIdx < trackData_.knobs.size()) {
                        trackData_.knobs[kIdx].value = newVal;
                        int pct = static_cast<int>(std::round(newVal * 100.0f));
                        trackData_.knobs[kIdx].display = std::to_string(pct) + "%";
                        if (rIdx == 0 && wIdx < knobSlots_.size()) {
                            knobSlots_[wIdx].normVal = newVal;
                            knobSlots_[wIdx].readout = trackData_.knobs[kIdx].display;
                        }
                    }
                    if (target_.type == DeviceTargetType::AudioFx) {
                        if (target_.fxIndex >= 0 && static_cast<size_t>(target_.fxIndex) < trackData_.audioFx.size()) {
                            auto& fx = trackData_.audioFx[target_.fxIndex];
                            if (w.param == "drive" || w.param == "Drive" || w.param == "time" || w.param == "decay" || w.param == "rate" || w.param == "threshold" || w.param == "thresh") {
                                fx.drive = newVal;
                            } else if (w.param == "mix" || w.param == "Mix" || w.param == "WetLevel" || w.param == "gain") {
                                fx.mix = newVal;
                            }
                            for (auto& k : fx.knobs) {
                                if (k.name == w.param) {
                                    k.value = newVal;
                                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                                    k.display = std::to_string(pct) + (k.unit.empty() ? "%" : (" " + k.unit));
                                    break;
                                }
                            }
                        }
                        if (onAudioFxParamChanged) {
                            onAudioFxParamChanged(target_.trackIndex, w.param, newVal);
                        }
                    } else if (target_.type == DeviceTargetType::MidiFx) {
                        if (target_.fxIndex >= 0 && static_cast<size_t>(target_.fxIndex) < trackData_.midiFx.size()) {
                            auto& fx = trackData_.midiFx[target_.fxIndex];
                            for (auto& k : fx.knobs) {
                                if (k.name == w.param) {
                                    k.value = newVal;
                                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                                    k.display = std::to_string(pct) + (k.unit.empty() ? "%" : (" " + k.unit));
                                    break;
                                }
                            }
                        }
                        if (onMidiFxParamChanged) {
                            onMidiFxParamChanged(target_.trackIndex, w.param, newVal);
                        }
                    } else {
                        if (onParamChanged) {
                            onParamChanged(target_.trackIndex, w.param, newVal);
                        }
                    }
                    return true;
                }
            }
        }
        return true;
    }

    if (ev.action == PointerAction::Down && (ev.button == PointerButton::Left || ev.button == PointerButton::None)) {
        // 1. Universal Close button (screw)
        float cx = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
        float cy = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
        if (closeBtnBounds_.contains(mx, my) || std::hypot(mx - cx, my - cy) <= 14.0f) {
            close();
            return true;
        }

        // 2. Preset button linking to preset dialog
        if (presetBtnBounds_.contains(mx, my)) {
            pluginDialog_.open(PluginDialogMode::SelectPreset, trackData_.trackName, target_.trackIndex);
            if (onOpenPresetDialog) onOpenPresetDialog(target_.trackIndex);
            return true;
        }

        // 3. Design chip button linking to code editor
        if (designBtnBounds_.contains(mx, my)) {
            if (onOpenCodeEditor) onOpenCodeEditor(target_.trackIndex);
            close();
            return true;
        }

        // 4. GUI Widgets hit testing
        for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
            auto& row = guiPanel_.rows[rIdx];
            for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                auto& w = row.widgets[wIdx];
                if (w.type == GuiWidgetType::Divider) continue;
                float kx = w.bounds.x + (w.bounds.w * 0.5f);
                float ky = w.bounds.y + (w.bounds.h * 0.44f);
                float dist = std::hypot(mx - kx, my - ky);
                if (w.bounds.contains(mx, my) || dist <= (w.bounds.w * 0.5f)) {
                    if (w.type == GuiWidgetType::ToggleSwitch) {
                        w.currentVal = (w.currentVal > 0.5f) ? 0.0f : 1.0f;
                        for (auto& k : trackData_.knobs) {
                            if (_stricmp(k.name.c_str(), w.param.c_str()) == 0) {
                                k.value = w.currentVal;
                                k.display = (w.currentVal > 0.5f) ? "On" : "Off";
                                break;
                            }
                        }
                        if (onParamChanged) {
                            onParamChanged(target_.trackIndex, w.param, w.currentVal);
                        }
                        return true;
                    }
                    isDraggingKnob_ = true;
                    draggingRow_ = static_cast<int>(rIdx);
                    draggingWidget_ = static_cast<int>(wIdx);
                    activeKnobIndex_ = (rIdx == 0) ? static_cast<int>(wIdx) : -1;
                    dragStartY_ = my;
                    dragStartVal_ = w.currentVal;
                    return true;
                }
            }
        }

        // Fallback for knobSlots_
        for (size_t i = 0; i < knobSlots_.size(); ++i) {
            float dist = std::hypot(mx - knobSlots_[i].center.x, my - knobSlots_[i].center.y);
            if (dist <= knobSlots_[i].radius * 1.8f ||
                (std::abs(mx - knobSlots_[i].center.x) <= knobSlots_[i].radius * 1.5f &&
                 std::abs(my - knobSlots_[i].center.y) <= knobSlots_[i].radius * 2.0f)) {
                isDraggingKnob_ = true;
                draggingRow_ = 0;
                draggingWidget_ = static_cast<int>(i);
                activeKnobIndex_ = static_cast<int>(i);
                dragStartY_ = my;
                dragStartVal_ = knobSlots_[i].normVal;
                return true;
            }
        }

        // Absorb all other clicks while full display view is open
        return true;
    }

    if (ev.action == PointerAction::Move && isDraggingKnob_) {
        float deltaY = dragStartY_ - my; // Up = increase
        float newVal = std::clamp(dragStartVal_ + (deltaY / 150.0f), 0.0f, 1.0f);
        if (draggingRow_ >= 0 && draggingRow_ < static_cast<int>(guiPanel_.rows.size())) {
            auto& row = guiPanel_.rows[draggingRow_];
            if (draggingWidget_ >= 0 && draggingWidget_ < static_cast<int>(row.widgets.size())) {
                auto& w = row.widgets[draggingWidget_];
                w.currentVal = newVal;
                for (auto& k : trackData_.knobs) {
                    if (_stricmp(k.name.c_str(), w.param.c_str()) == 0) {
                        k.value = newVal;
                        int pct = static_cast<int>(std::round(newVal * 100.0f));
                        k.display = std::to_string(pct) + "%";
                        break;
                    }
                }
                if (draggingRow_ == 0 && draggingWidget_ < static_cast<int>(knobSlots_.size())) {
                    knobSlots_[draggingWidget_].normVal = newVal;
                    knobSlots_[draggingWidget_].readout = std::to_string(static_cast<int>(std::round(newVal * 100.0f))) + "%";
                }
                if (target_.type == DeviceTargetType::AudioFx) {
                    if (target_.fxIndex >= 0 && static_cast<size_t>(target_.fxIndex) < trackData_.audioFx.size()) {
                        auto& fx = trackData_.audioFx[target_.fxIndex];
                        if (w.param == "drive" || w.param == "Drive" || w.param == "time" || w.param == "decay" || w.param == "rate" || w.param == "threshold" || w.param == "thresh") {
                            fx.drive = newVal;
                        } else if (w.param == "mix" || w.param == "Mix" || w.param == "WetLevel" || w.param == "gain") {
                            fx.mix = newVal;
                        }
                        for (auto& k : fx.knobs) {
                            if (k.name == w.param) {
                                k.value = newVal;
                                int pct = static_cast<int>(std::round(newVal * 100.0f));
                                k.display = std::to_string(pct) + (k.unit.empty() ? "%" : (" " + k.unit));
                                break;
                            }
                        }
                    }
                    if (onAudioFxParamChanged) {
                        onAudioFxParamChanged(target_.trackIndex, w.param, newVal);
                    }
                } else if (target_.type == DeviceTargetType::MidiFx) {
                    if (target_.fxIndex >= 0 && static_cast<size_t>(target_.fxIndex) < trackData_.midiFx.size()) {
                        auto& fx = trackData_.midiFx[target_.fxIndex];
                        for (auto& k : fx.knobs) {
                            if (k.name == w.param) {
                                k.value = newVal;
                                int pct = static_cast<int>(std::round(newVal * 100.0f));
                                k.display = std::to_string(pct) + (k.unit.empty() ? "%" : (" " + k.unit));
                                break;
                            }
                        }
                    }
                    if (onMidiFxParamChanged) {
                        onMidiFxParamChanged(target_.trackIndex, w.param, newVal);
                    }
                } else {
                    if (onParamChanged) {
                        onParamChanged(target_.trackIndex, w.param, newVal);
                    }
                }
                return true;
            }
        } else if (activeKnobIndex_ >= 0 && activeKnobIndex_ < static_cast<int>(knobSlots_.size())) {
            knobSlots_[activeKnobIndex_].normVal = newVal;
            if (static_cast<size_t>(activeKnobIndex_) < trackData_.knobs.size()) {
                trackData_.knobs[activeKnobIndex_].value = newVal;
                int pct = static_cast<int>(std::round(newVal * 100.0f));
                knobSlots_[activeKnobIndex_].readout = std::to_string(pct) + "%";
                trackData_.knobs[activeKnobIndex_].display = knobSlots_[activeKnobIndex_].readout;
            }
            if (onParamChanged) {
                onParamChanged(target_.trackIndex, knobSlots_[activeKnobIndex_].name, newVal);
            }
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        if (isDraggingKnob_) {
            isDraggingKnob_ = false;
            draggingRow_ = -1;
            draggingWidget_ = -1;
            activeKnobIndex_ = -1;
            return true;
        }
    }

    return true; // Eat pointer events while modal is open
}

bool FullscreenDeviceModal::handleKey(int key, int scancode, int action, int mods) noexcept {
    (void)scancode;
    if (!isOpen_) return false;

    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handleKey(key, scancode, action, mods)) return true;
        return true;
    }

    if (action == 1 /* GLFW_PRESS */) {
        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            return true;
        }
        if ((mods & 1) != 0 && (key == 70 || key == 102)) { // Shift+F
            close();
            return true;
        }
        if (key == 263 || key == 91) { // Left arrow or '['
            if (onPrevPreset) onPrevPreset();
            return true;
        }
        if (key == 262 || key == 93) { // Right arrow or ']'
            if (onNextPreset) onNextPreset();
            return true;
        }
    }
    // Return false for unhandled keys (F11, Alt+Enter, Space, etc.) so DAW keyboard shortcuts work!
    return false;
}

} // namespace eatsbits::ui
