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
        pluginDialog_.close();
        if (onOpenPresetDialog) onOpenPresetDialog(trackIndex);
    };
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

    // Right-aligned elements matching user screenshot:
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
    float marginX = 24.0f;
    float marginY = 16.0f;
    float bodyY = headerH + marginY;
    float bodyH = screenHeight_ - bodyY - marginY;
    float bodyW = screenWidth_ - (marginX * 2.0f);
    bodyBounds_ = Rect2D{marginX, bodyY, bodyW, bodyH};

    // Ensure trackData_ has knobs initialized
    trackData_.syncKnobsIfEmpty();

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

    size_t displayKnobs = std::min(numKnobs, size_t{8});

    float knobAreaY = bodyBounds_.y + 60.0f;
    float knobAreaH = std::min(240.0f, bodyBounds_.h * 0.45f);
    float kStep = bodyBounds_.w / static_cast<float>(displayKnobs);
    float kRadius = std::clamp(kStep * 0.22f, 26.0f, 40.0f);

    if (knobSlots_.size() != displayKnobs) {
        knobSlots_.resize(displayKnobs);
        for (size_t i = 0; i < displayKnobs; ++i) {
            knobSlots_[i].name = trackData_.knobs[i].name;
            knobSlots_[i].label = trackData_.knobs[i].label;
            knobSlots_[i].normVal = trackData_.knobs[i].value;
            knobSlots_[i].readout = trackData_.knobs[i].display;
        }
    }

    for (size_t i = 0; i < displayKnobs; ++i) {
        knobSlots_[i].name = trackData_.knobs[i].name;
        knobSlots_[i].label = trackData_.knobs[i].label;
        if (!isDraggingKnob_ || activeKnobIndex_ != static_cast<int>(i)) {
            knobSlots_[i].normVal = trackData_.knobs[i].value;
            knobSlots_[i].readout = trackData_.knobs[i].display;
        }
        knobSlots_[i].center = Point2D{bodyBounds_.x + static_cast<float>(i) * kStep + (kStep * 0.5f), knobAreaY + knobAreaH * 0.5f};
        knobSlots_[i].radius = kRadius;
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

    // 3. Main Device Body View
    if (target_.type == DeviceTargetType::Instrument) {
        renderInstrumentFaceplate(r, theme);
    } else if (target_.type == DeviceTargetType::AudioFx) {
        renderAudioFxRacks(r, theme);
    } else if (target_.type == DeviceTargetType::MidiFx) {
        renderMidiFxRacks(r, theme);
    }

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
    if (trackTitle.empty()) {
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
    Color chipCol = (trackData_.r > 0.05f || trackData_.g > 0.05f || trackData_.b > 0.05f)
                        ? Color(trackData_.r, trackData_.g, trackData_.b, 1.0f)
                        : theme.primaryAccent;
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
    float bx = bodyBounds_.x;
    float by = bodyBounds_.y;
    float bw = bodyBounds_.w;
    float bh = bodyBounds_.h;

    // Chassis background & borders styled by engine type
    const std::string& eng = trackData_.instrumentEngine;
    if (eng == "tb303") {
        // Brushed Aluminum / Silver Diode Ladder Faceplate
        drawRoundedRect(r, bx, by, bw, bh, 8.0f, 0.82f, 0.83f, 0.85f, 1.0f);
        drawRoundedRectOutline(r, bx, by, bw, bh, 8.0f, 0.45f, 0.46f, 0.48f, 1.0f, 2.5f);
        drawText(r, "COMPUTER CONTROLLED", bx + 24.0f, by + 18.0f, 10.0f, 0.70f, 0.10f, 0.10f, 1.0f);
        drawText(r, "BASS LINE TB-303", bx + 24.0f, by + 32.0f, 14.0f, 0.15f, 0.20f, 0.45f, 1.0f);
    } else if (eng == "tr808" || eng == "tr909") {
        // Cream & Dark Charcoal Hardware Faceplate with Orange accents
        drawRoundedRect(r, bx, by, bw, bh, 8.0f, 0.18f, 0.19f, 0.22f, 1.0f);
        drawRoundedRectOutline(r, bx, by, bw, bh, 8.0f, 0.90f, 0.40f, 0.15f, 1.0f, 2.5f);
        drawText(r, "RHYTHM COMPOSER", bx + 24.0f, by + 18.0f, 10.0f, 0.90f, 0.40f, 0.15f, 1.0f);
        drawText(r, eng == "tr808" ? "TRANSISTOR RHYTHM TR-808" : "RHYTHM COMPOSER TR-909", bx + 24.0f, by + 32.0f, 14.0f, 0.95f, 0.95f, 1.0f, 1.0f);
    } else if (eng == "dx7") {
        // Deep Emerald / Yamaha FM Digital Synthesizer
        drawRoundedRect(r, bx, by, bw, bh, 8.0f, 0.12f, 0.14f, 0.16f, 1.0f);
        drawRoundedRectOutline(r, bx, by, bw, bh, 8.0f, 0.0f, 0.75f, 0.55f, 1.0f, 2.5f);
        drawText(r, "DIGITAL PROGRAMMABLE ALGORITHM SYNTHESIZER", bx + 24.0f, by + 18.0f, 9.5f, 0.0f, 0.75f, 0.55f, 1.0f);
        drawText(r, "FREQUENCY MODULATION 6-OPERATOR DX7", bx + 24.0f, by + 32.0f, 14.0f, 0.90f, 0.95f, 1.0f, 1.0f);
    } else {
        // Custom Eatscript Synth Faceplate with Track Accent
        drawRoundedRect(r, bx, by, bw, bh, 8.0f, 0.12f, 0.14f, 0.18f, 1.0f);
        drawRoundedRectOutline(r, bx, by, bw, bh, 8.0f, trackData_.r * 0.8f, trackData_.g * 0.8f, trackData_.b * 0.8f, 1.0f, 2.0f);
        drawText(r, "EATSCRIPT DSP WORKBENCH", bx + 24.0f, by + 18.0f, 10.0f, 0.50f, 0.60f, 0.75f, 1.0f);
        drawText(r, trackData_.trackName + " • " + trackData_.presetTitle, bx + 24.0f, by + 32.0f, 14.0f, 0.0f, 0.95f, 1.0f, 1.0f);
    }

    // Corner mounting hex screws
    auto drawCornerScrew = [&](float sx, float sy) {
        drawCircle(r, sx, sy, 5.0f, 0.35f, 0.38f, 0.45f, 1.0f);
        drawCircle(r, sx, sy, 3.5f, 0.20f, 0.22f, 0.26f, 1.0f);
        drawLine(r, sx - 2.5f, sy, sx + 2.5f, sy, 0.45f, 0.48f, 0.55f, 1.0f, 1.2f);
    };
    drawCornerScrew(bx + 14.0f, by + 14.0f);
    drawCornerScrew(bx + bw - 14.0f, by + 14.0f);
    drawCornerScrew(bx + 14.0f, by + bh - 14.0f);
    drawCornerScrew(bx + bw - 14.0f, by + bh - 14.0f);

    // Render Large Rotary Knobs
    for (size_t i = 0; i < knobSlots_.size(); ++i) {
        const auto& knob = knobSlots_[i];
        float cx = knob.center.x;
        float cy = knob.center.y;
        float kRad = knob.radius;
        float normVal = knob.normVal;
        bool isEng303 = (eng == "tb303");

        // Outer well drop shadow
        drawCircle(r, cx, cy, kRad + 6.0f, 0.04f, 0.05f, 0.06f, 0.55f);

        // Arc angle range (-135 deg to +135 deg)
        constexpr float minA = -2.35619449f;
        constexpr float maxA = 2.35619449f;
        float curA = minA + normVal * (maxA - minA);

        // Base Arc track
        Color baseArcCol = isEng303 ? Color(0.50f, 0.52f, 0.56f, 0.8f) : Color(0.20f, 0.24f, 0.30f, 1.0f);
        r.drawArc(cx, cy, kRad + 3.0f, minA, maxA, baseArcCol.r, baseArcCol.g, baseArcCol.b, baseArcCol.a, 2.5f);

        // Active value arc
        if (normVal > 0.01f) {
            Color arcCol = isEng303 ? Color(0.15f, 0.35f, 0.85f, 1.0f) : theme.primaryAccent;
            r.drawArc(cx, cy, kRad + 3.0f, minA, curA, arcCol.r, arcCol.g, arcCol.b, arcCol.a, 3.2f);
        }

        // Knob body
        if (isEng303) {
            drawCircle(r, cx, cy, kRad, 0.76f, 0.78f, 0.80f, 1.0f);
            drawCircleOutline(r, cx, cy, kRad, 0.40f, 0.42f, 0.45f, 1.0f, 1.8f);
            // Fluted grooves
            for (int g = 0; g < 12; ++g) {
                float ga = static_cast<float>(g) * (6.2831853f / 12.0f);
                float gx1 = cx + std::sin(ga) * (kRad * 0.65f);
                float gy1 = cy - std::cos(ga) * (kRad * 0.65f);
                float gx2 = cx + std::sin(ga) * (kRad * 0.95f);
                float gy2 = cy - std::cos(ga) * (kRad * 0.95f);
                drawLine(r, gx1, gy1, gx2, gy2, 0.55f, 0.58f, 0.62f, 1.0f, 1.2f);
            }
        } else {
            drawCircle(r, cx, cy, kRad, 0.16f, 0.18f, 0.23f, 1.0f);
            drawCircleOutline(r, cx, cy, kRad, 0.30f, 0.35f, 0.45f, 1.0f, 1.8f);
        }

        // Pointer line
        float indX = cx + std::sin(curA) * (kRad - 3.0f);
        float indY = cy - std::cos(curA) * (kRad - 3.0f);
        drawLine(r, cx, cy, indX, indY,
                 isEng303 ? 0.10f : 0.0f,
                 isEng303 ? 0.20f : 0.95f,
                 isEng303 ? 0.75f : 1.0f, 1.0f, 2.4f);

        // Parameter Name Label
        drawText(r, knob.label, cx - (knob.label.size() * 3.2f), cy + kRad + 14.0f, 10.5f,
                 isEng303 ? 0.20f : 0.75f,
                 isEng303 ? 0.22f : 0.80f,
                 isEng303 ? 0.25f : 0.90f, 1.0f);

        // Readout display badge
        drawRoundedRect(r, cx - 30.0f, cy + kRad + 28.0f, 60.0f, 18.0f, 3.0f, 0.08f, 0.09f, 0.12f, 0.85f);
        drawRoundedRectOutline(r, cx - 30.0f, cy + kRad + 28.0f, 60.0f, 18.0f, 3.0f, 0.25f, 0.30f, 0.40f, 0.6f, 1.0f);
        drawText(r, knob.readout, cx - (knob.readout.size() * 2.8f), cy + kRad + 33.0f, 9.0f,
                 0.0f, 0.90f, 1.0f, 1.0f);
    }

    // Lower section: Real-time Audio Oscilloscope & Waveform Display
    float oscW = bw - 48.0f;
    float oscH = std::min(160.0f, bh * 0.35f);
    float oscX = bx + 24.0f;
    float oscY = by + bh - oscH - 24.0f;
    renderOscilloscope(r, oscX, oscY, oscW, oscH, theme);
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
        for (size_t i = 0; i < knobSlots_.size(); ++i) {
            float dist = std::hypot(mx - knobSlots_[i].center.x, my - knobSlots_[i].center.y);
            if (dist <= knobSlots_[i].radius * 1.6f) {
                float newVal = std::clamp(knobSlots_[i].normVal + ev.scrollY * 0.04f, 0.0f, 1.0f);
                knobSlots_[i].normVal = newVal;
                if (i < trackData_.knobs.size()) {
                    trackData_.knobs[i].value = newVal;
                }
                int pct = static_cast<int>(std::round(newVal * 100.0f));
                knobSlots_[i].readout = std::to_string(pct) + "%";
                if (i < trackData_.knobs.size()) {
                    trackData_.knobs[i].display = knobSlots_[i].readout;
                }
                if (onParamChanged) {
                    onParamChanged(target_.trackIndex, knobSlots_[i].name, newVal);
                }
                return true;
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
            pluginDialog_.open(PluginDialogMode::AddInstrument, trackData_.trackName, target_.trackIndex);
            if (onOpenPresetDialog) onOpenPresetDialog(target_.trackIndex);
            return true;
        }

        // 3. Design chip button linking to code editor
        if (designBtnBounds_.contains(mx, my)) {
            if (onOpenCodeEditor) onOpenCodeEditor(target_.trackIndex);
            close();
            return true;
        }

        // 4. Knobs hit testing
        for (size_t i = 0; i < knobSlots_.size(); ++i) {
            float dist = std::hypot(mx - knobSlots_[i].center.x, my - knobSlots_[i].center.y);
            if (dist <= knobSlots_[i].radius * 1.6f ||
                (std::abs(mx - knobSlots_[i].center.x) <= knobSlots_[i].radius * 1.5f &&
                 std::abs(my - knobSlots_[i].center.y) <= knobSlots_[i].radius * 2.0f)) {
                isDraggingKnob_ = true;
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
        if (activeKnobIndex_ >= 0 && activeKnobIndex_ < static_cast<int>(knobSlots_.size())) {
            float deltaY = dragStartY_ - my; // Up = increase
            float newVal = std::clamp(dragStartVal_ + (deltaY / 150.0f), 0.0f, 1.0f);
            knobSlots_[activeKnobIndex_].normVal = newVal;
            if (static_cast<size_t>(activeKnobIndex_) < trackData_.knobs.size()) {
                trackData_.knobs[activeKnobIndex_].value = newVal;
            }

            // Formatted readout
            int pct = static_cast<int>(std::round(newVal * 100.0f));
            knobSlots_[activeKnobIndex_].readout = std::to_string(pct) + "%";
            if (static_cast<size_t>(activeKnobIndex_) < trackData_.knobs.size()) {
                trackData_.knobs[activeKnobIndex_].display = knobSlots_[activeKnobIndex_].readout;
            }

            // Fire real-time parameter change callback
            if (onParamChanged) {
                onParamChanged(target_.trackIndex, knobSlots_[activeKnobIndex_].name, newVal);
            }
            return true;
        }
    }

    if (ev.action == PointerAction::Up) {
        if (isDraggingKnob_) {
            isDraggingKnob_ = false;
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
