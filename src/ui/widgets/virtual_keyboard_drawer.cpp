#include "eatsbits/ui/widgets/virtual_keyboard_drawer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>

namespace eatsbits::ui {

VirtualKeyboardDrawer::VirtualKeyboardDrawer() {
    PianoKeyboardConfig cfg;
    cfg.orientation = KeyboardOrientation::Horizontal;
    cfg.baseOctave = baseOctave_;
    cfg.octavesCount = 3;
    cfg.minPitch = baseOctave_ * 12;
    cfg.maxPitch = cfg.minPitch + (cfg.octavesCount * 12);
    keyboard_.setConfig(cfg);
}

void VirtualKeyboardDrawer::setBaseOctave(int oct) noexcept {
    baseOctave_ = std::clamp(oct, 1, 6);
    auto cfg = keyboard_.getConfig();
    cfg.baseOctave = baseOctave_;
    cfg.minPitch = baseOctave_ * 12;
    cfg.maxPitch = cfg.minPitch + (cfg.octavesCount * 12);
    keyboard_.setConfig(cfg);
}

void VirtualKeyboardDrawer::update(float dt) noexcept {
    drumPadGrid_.update(dt);
}

void VirtualKeyboardDrawer::layout(float screenWidth, float bottomNavTopY) {
    float tabX = (screenWidth - kPullTabWidth) * 0.5f;
    float tabY = bottomNavTopY - kPullTabHeight;

    float activeDrawerH = (mode_ == KeyboardDrawerMode::DrumPads) ? kDrumDrawerHeight : kPianoDrawerHeight;

    if (isExpanded_) {
        float drawerY = bottomNavTopY - activeDrawerH - kPullTabHeight;
        drawerBounds_ = Rect2D(0.0f, drawerY, screenWidth, activeDrawerH + kPullTabHeight);
        pullTabBounds_ = Rect2D(tabX, drawerY, kPullTabWidth, kPullTabHeight);

        // Control strip at top of drawer
        float ctrlY = drawerY + kPullTabHeight + 2.0f;

        // Mode switch buttons on left of control strip
        modeKeysBtnBounds_ = Rect2D(10.0f, ctrlY, 52.0f, 20.0f);
        modePadsBtnBounds_ = Rect2D(66.0f, ctrlY, 52.0f, 20.0f);

        if (mode_ == KeyboardDrawerMode::Piano) {
            // Octave controls
            octDownBounds_ = Rect2D(128.0f, ctrlY, 54.0f, 20.0f);
            octUpBounds_ = Rect2D(186.0f, ctrlY, 54.0f, 20.0f);

            // Keys area
            float keysY = ctrlY + 24.0f;
            float keysH = activeDrawerH - 28.0f;
            keysBounds_ = Rect2D(10.0f, keysY, screenWidth - 20.0f, keysH);
        } else {
            octDownBounds_ = Rect2D();
            octUpBounds_ = Rect2D();
            keysBounds_ = Rect2D();

            // Drum Pad grid bounds
            float padGridY = ctrlY + 2.0f;
            float padGridH = activeDrawerH - 6.0f;
            drumPadGrid_.layout(Rect2D(10.0f, padGridY, screenWidth - 20.0f, padGridH));
        }
    } else {
        drawerBounds_ = Rect2D(tabX, tabY, kPullTabWidth, kPullTabHeight);
        pullTabBounds_ = Rect2D(tabX, tabY, kPullTabWidth, kPullTabHeight);
        modeKeysBtnBounds_ = Rect2D();
        modePadsBtnBounds_ = Rect2D();
        octDownBounds_ = Rect2D();
        octUpBounds_ = Rect2D();
        keysBounds_ = Rect2D();
        drumPadGrid_.layout(Rect2D());
    }
}

void VirtualKeyboardDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine& engine) {
    (void)engine;

    // 1. Draw Pull Tab
    drawRoundedRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.95f);
    drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    // Instrument Icon & Label
    float iconW = 16.0f;
    float iconH = 11.0f;
    float iconX = pullTabBounds_.x + 10.0f;
    float iconY = pullTabBounds_.y + (pullTabBounds_.h - iconH) * 0.5f;

    Color iconCol = isExpanded_ ? theme.primaryAccent : theme.textSecondary;
    drawPianoIcon(r, iconX, iconY, iconW, iconH, iconCol);

    std::string tabLabel;
    if (mode_ == KeyboardDrawerMode::DrumPads) {
        tabLabel = isExpanded_ ? "MPC DRUM PADS" : "DRUM PADS (16)";
    } else {
        tabLabel = isExpanded_ ? "VIRTUAL PIANO" : ("PIANO (C" + std::to_string(baseOctave_) + ")");
    }

    float labelAreaW = pullTabBounds_.w - (iconX + iconW) - 24.0f;
    Color tabCol = isExpanded_ ? theme.primaryAccent : theme.textPrimary;
    drawCenteredText(r, tabLabel, iconX + iconW + 4.0f, pullTabBounds_.y, labelAreaW, pullTabBounds_.h, 10.0f, tabCol);

    // Active status mini-LED on right
    float ledX = pullTabBounds_.x + pullTabBounds_.w - 12.0f;
    float ledY = pullTabBounds_.y + (pullTabBounds_.h * 0.5f);
    if (isExpanded_) {
        drawCircle(r, ledX, ledY, 3.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    } else {
        drawCircle(r, ledX, ledY, 2.5f, 0.35f, 0.35f, 0.35f, 0.8f);
    }

    if (!isExpanded_) return;

    // 2. Draw Drawer Background Panel
    float activeDrawerH = (mode_ == KeyboardDrawerMode::DrumPads) ? kDrumDrawerHeight : kPianoDrawerHeight;
    drawRect(r, drawerBounds_.x, drawerBounds_.y + kPullTabHeight, drawerBounds_.w, activeDrawerH,
             theme.panelBackground.r * 0.85f, theme.panelBackground.g * 0.85f, theme.panelBackground.b * 0.85f, 0.98f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y + kPullTabHeight, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y + kPullTabHeight,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.5f);

    // 3. Mode Toggle Buttons: [ KEYS ] and [ PADS ]
    bool isPiano = (mode_ == KeyboardDrawerMode::Piano);
    drawButton(r, modeKeysBtnBounds_, "KEYS",
               isPiano ? theme.primaryAccent.withAlpha(0.25f) : theme.controlBackground,
               isPiano ? theme.primaryAccent : theme.borderSubtle,
               isPiano ? theme.primaryAccent : theme.textMuted,
               9.0f, 3.0f, 1.0f);

    drawButton(r, modePadsBtnBounds_, "PADS",
               !isPiano ? theme.secondaryAccent.withAlpha(0.25f) : theme.controlBackground,
               !isPiano ? theme.secondaryAccent : theme.borderSubtle,
               !isPiano ? theme.secondaryAccent : theme.textMuted,
               9.0f, 3.0f, 1.0f);

    if (mode_ == KeyboardDrawerMode::DrumPads) {
        // Delegate rendering to DrumPadGridWidget
        drumPadGrid_.render(r, theme);
        return;
    }

    // 4. Piano Mode: Octave Controls & Range Readout
    drawButton(r, octDownBounds_, "< OCT",
               theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 3.0f, 1.0f);
    drawButton(r, octUpBounds_, "OCT >",
               theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 3.0f, 1.0f);

    std::string rangeStr = "RANGE: C" + std::to_string(baseOctave_) + " - C" + std::to_string(baseOctave_ + 3) +
                           "  (TOUCH & DRAG GLISSANDO ACTIVE)";
    float rangeY = octUpBounds_.y + (octUpBounds_.h - 10.0f) * 0.5f;
    drawText(r, rangeStr, octUpBounds_.x + octUpBounds_.w + 16.0f, rangeY, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    // 5. Draw Horizontal Piano Keys
    int numWhiteKeys = 3 * 7 + 1; // 22 white keys across 3 octaves + high C
    float whiteKeyWidth = keysBounds_.w / static_cast<float>(numWhiteKeys);
    float whiteKeyHeight = keysBounds_.h;
    float blackKeyWidth = whiteKeyWidth * 0.62f;
    float blackKeyHeight = whiteKeyHeight * 0.60f;

    // Draw White Keys First
    int curWhiteIdx = 0;
    int startPitch = baseOctave_ * 12;
    int endPitch = startPitch + 36; // 3 octaves

    for (int p = startPitch; p <= endPitch; ++p) {
        if (PianoKeyboard::isBlackKey(p)) continue;

        float kx = keysBounds_.x + static_cast<float>(curWhiteIdx) * whiteKeyWidth;
        float ky = keysBounds_.y;
        float kw = whiteKeyWidth - 1.0f;
        float kh = whiteKeyHeight;

        bool isDown = activePitches_.find(p) != activePitches_.end();
        Color bgCol = isDown ? theme.primaryAccent : Color(0.92f, 0.93f, 0.95f, 1.0f);

        drawRoundedRect(r, kx, ky, kw, kh, 3.0f, bgCol.r, bgCol.g, bgCol.b, bgCol.a);
        drawRoundedRectOutline(r, kx, ky, kw, kh, 3.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);

        // Note label at bottom of C keys
        if (p % 12 == 0) {
            std::string cLabel = "C" + std::to_string(p / 12);
            drawCenteredText(r, cLabel, kx, ky + kh - 16.0f, kw, 14.0f, 9.0f,
                             isDown ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted);
        }

        curWhiteIdx++;
    }

    // Draw Black Keys on Top
    curWhiteIdx = 0;
    for (int p = startPitch; p <= endPitch; ++p) {
        if (!PianoKeyboard::isBlackKey(p)) {
            curWhiteIdx++;
            continue;
        }

        float kx = keysBounds_.x + static_cast<float>(curWhiteIdx) * whiteKeyWidth - (blackKeyWidth * 0.5f);
        float ky = keysBounds_.y;
        float kw = blackKeyWidth;
        float kh = blackKeyHeight;

        bool isDown = activePitches_.find(p) != activePitches_.end();
        Color bgCol = isDown ? theme.secondaryAccent : Color(0.12f, 0.13f, 0.15f, 1.0f);

        drawRoundedRect(r, kx, ky, kw, kh, 2.0f, bgCol.r, bgCol.g, bgCol.b, bgCol.a);
        drawRoundedRectOutline(r, kx, ky, kw, kh, 2.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.9f, 1.0f);
    }
}

bool VirtualKeyboardDrawer::handlePointer(const PointerEvent& ev, audio::AudioEngine& engine) {
    // 1. Pull Tab Hit Testing
    if (pullTabBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            toggleExpanded();
            if (!isExpanded_) {
                releaseAllNotes(engine);
                drumPadGrid_.releaseAllPads();
            }
            return true;
        }
    }

    if (!isExpanded_) return false;

    // 2. Control Strip: Mode Buttons [ KEYS | PADS ]
    if (ev.action == PointerAction::Down) {
        if (modeKeysBtnBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            drumPadGrid_.releaseAllPads();
            setMode(KeyboardDrawerMode::Piano);
            return true;
        }
        if (modePadsBtnBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            drumPadGrid_.releaseAllPads();
            setMode(KeyboardDrawerMode::DrumPads);
            return true;
        }
    }

    // 3. Drum Pad Mode Event Routing
    if (mode_ == KeyboardDrawerMode::DrumPads) {
        // Wire callbacks to audio engine
        drumPadGrid_.onPadTrigger = [this, &engine](uint8_t note, float velocity) {
            triggerNoteOn(static_cast<int>(note), velocity, engine);
        };
        drumPadGrid_.onPadRelease = [this, &engine](uint8_t note) {
            triggerNoteOff(static_cast<int>(note), engine);
        };

        if (drumPadGrid_.handlePointer(ev)) {
            return true;
        }
        if (drawerBounds_.contains(ev.x, ev.y)) {
            return true;
        }
        return false;
    }

    // 4. Piano Mode: Octave Buttons
    if (ev.action == PointerAction::Down) {
        if (octDownBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            setBaseOctave(baseOctave_ - 1);
            return true;
        }
        if (octUpBounds_.contains(ev.x, ev.y)) {
            releaseAllNotes(engine);
            setBaseOctave(baseOctave_ + 1);
            return true;
        }
    }

    // 5. Piano Keys Interaction & Multi-Touch Polyphonic Glissando
    if (keysBounds_.contains(ev.x, ev.y) || !activeTouches_.empty()) {
        int numWhiteKeys = 3 * 7 + 1;
        float whiteKeyWidth = keysBounds_.w / static_cast<float>(numWhiteKeys);
        float blackKeyWidth = whiteKeyWidth * 0.62f;
        float blackKeyHeight = keysBounds_.h * 0.60f;
        int startPitch = baseOctave_ * 12;
        int endPitch = startPitch + 36;

        auto getPitchAt = [&](float px, float py) -> int {
            if (!keysBounds_.contains(px, py)) return -1;

            // Check black keys first (top region)
            if (py < keysBounds_.y + blackKeyHeight) {
                int curWhite = 0;
                for (int p = startPitch; p <= endPitch; ++p) {
                    if (!PianoKeyboard::isBlackKey(p)) {
                        curWhite++;
                        continue;
                    }
                    float bkx = keysBounds_.x + static_cast<float>(curWhite) * whiteKeyWidth - (blackKeyWidth * 0.5f);
                    if (px >= bkx && px <= bkx + blackKeyWidth) {
                        return p;
                    }
                }
            }

            // Fallback to white keys
            int wIdx = static_cast<int>((px - keysBounds_.x) / whiteKeyWidth);
            wIdx = std::clamp(wIdx, 0, numWhiteKeys - 1);

            int whiteCounter = 0;
            for (int p = startPitch; p <= endPitch; ++p) {
                if (PianoKeyboard::isBlackKey(p)) continue;
                if (whiteCounter == wIdx) return p;
                whiteCounter++;
            }
            return -1;
        };

        if (ev.action == PointerAction::Down) {
            int hitPitch = getPitchAt(ev.x, ev.y);
            if (hitPitch != -1) {
                float vel = (ev.pressure > 0.05f) ? ev.pressure : 0.85f;
                activeTouches_[ev.id] = {hitPitch, vel};
                triggerNoteOn(hitPitch, vel, engine);
                return true;
            }
        } else if (ev.action == PointerAction::Move) {
            auto it = activeTouches_.find(ev.id);
            if (it != activeTouches_.end()) {
                int hitPitch = getPitchAt(ev.x, ev.y);
                if (hitPitch != -1 && hitPitch != it->second.pitch) {
                    triggerNoteOff(it->second.pitch, engine);
                    it->second.pitch = hitPitch;
                    triggerNoteOn(hitPitch, it->second.velocity, engine);
                }
                return true;
            }
        } else if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            auto it = activeTouches_.find(ev.id);
            if (it != activeTouches_.end()) {
                triggerNoteOff(it->second.pitch, engine);
                activeTouches_.erase(it);
                return true;
            }
        }
    }

    if (drawerBounds_.contains(ev.x, ev.y)) {
        return true;
    }

    return false;
}

void VirtualKeyboardDrawer::triggerNoteOn(int pitch, float velocity, audio::AudioEngine& engine) {
    activePitches_.insert(pitch);
    engine.postNoteOn(static_cast<uint8_t>(pitch), velocity, false, false, static_cast<int>(activeTrackIndex_));
}

void VirtualKeyboardDrawer::triggerNoteOff(int pitch, audio::AudioEngine& engine) {
    activePitches_.erase(pitch);
    engine.postNoteOff(static_cast<uint8_t>(pitch), static_cast<int>(activeTrackIndex_));
}

void VirtualKeyboardDrawer::releaseAllNotes(audio::AudioEngine& engine) {
    for (int p : activePitches_) {
        engine.postNoteOff(static_cast<uint8_t>(p), static_cast<int>(activeTrackIndex_));
    }
    activePitches_.clear();
    activeTouches_.clear();
}

} // namespace eatsbits::ui
