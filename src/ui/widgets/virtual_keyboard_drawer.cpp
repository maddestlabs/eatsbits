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

void VirtualKeyboardDrawer::layout(float screenWidth, float bottomNavTopY) {
    float tabX = (screenWidth - kPullTabWidth) * 0.5f;
    float tabY = bottomNavTopY - kPullTabHeight;

    if (isExpanded_) {
        float drawerY = bottomNavTopY - kDrawerHeight - kPullTabHeight;
        drawerBounds_ = Rect2D(0.0f, drawerY, screenWidth, kDrawerHeight + kPullTabHeight);
        pullTabBounds_ = Rect2D(tabX, drawerY, kPullTabWidth, kPullTabHeight);

        // Control strip at top of drawer
        float ctrlY = drawerY + kPullTabHeight + 2.0f;
        octDownBounds_ = Rect2D(12.0f, ctrlY, 54.0f, 20.0f);
        octUpBounds_ = Rect2D(72.0f, ctrlY, 54.0f, 20.0f);

        // Keys area
        float keysY = ctrlY + 24.0f;
        float keysH = kDrawerHeight - 28.0f;
        keysBounds_ = Rect2D(10.0f, keysY, screenWidth - 20.0f, keysH);
    } else {
        drawerBounds_ = Rect2D(tabX, tabY, kPullTabWidth, kPullTabHeight);
        pullTabBounds_ = Rect2D(tabX, tabY, kPullTabWidth, kPullTabHeight);
        octDownBounds_ = Rect2D();
        octUpBounds_ = Rect2D();
        keysBounds_ = Rect2D();
    }
}

void VirtualKeyboardDrawer::render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine& engine) {
    // 1. Draw Pull Tab
    drawRoundedRect(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 0.95f);
    drawRoundedRectOutline(r, pullTabBounds_.x, pullTabBounds_.y, pullTabBounds_.w, pullTabBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    // Piano Icon & Label (No arrows!)
    float iconW = 16.0f;
    float iconH = 11.0f;
    float iconX = pullTabBounds_.x + 12.0f;
    float iconY = pullTabBounds_.y + (pullTabBounds_.h - iconH) * 0.5f;
    drawPianoIcon(r, iconX, iconY, iconW, iconH, isExpanded_ ? theme.primaryAccent : theme.textSecondary);

    std::string tabLabel = isExpanded_ ? "VIRTUAL PIANO" : ("VIRTUAL PIANO (C" + std::to_string(baseOctave_) + ")");
    float labelAreaW = pullTabBounds_.w - (iconX + iconW) - 24.0f;
    Color tabCol = isExpanded_ ? theme.primaryAccent : theme.textPrimary;
    drawCenteredText(r, tabLabel, iconX + iconW + 4.0f, pullTabBounds_.y, labelAreaW, pullTabBounds_.h, 10.5f, tabCol);

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
    drawRect(r, drawerBounds_.x, drawerBounds_.y + kPullTabHeight, drawerBounds_.w, kDrawerHeight,
             theme.panelBackground.r * 0.85f, theme.panelBackground.g * 0.85f, theme.panelBackground.b * 0.85f, 0.98f);
    drawLine(r, drawerBounds_.x, drawerBounds_.y + kPullTabHeight, drawerBounds_.x + drawerBounds_.w, drawerBounds_.y + kPullTabHeight,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.5f);

    // 3. Octave Controls (Vertically centered button text)
    drawButton(r, octDownBounds_, "< OCT",
               theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 3.0f, 1.0f);
    drawButton(r, octUpBounds_, "OCT >",
               theme.panelHeader, theme.borderSubtle, theme.textPrimary, 10.0f, 3.0f, 1.0f);

    // Range readout
    std::string rangeStr = "RANGE: C" + std::to_string(baseOctave_) + " - C" + std::to_string(baseOctave_ + 3) +
                           "  (TOUCH & DRAG GLISSANDO ACTIVE)";
    float rangeY = octUpBounds_.y + (octUpBounds_.h - 10.0f) * 0.5f;
    drawText(r, rangeStr, octUpBounds_.x + octUpBounds_.w + 16.0f, rangeY, 10.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);

    // 4. Draw Horizontal Piano Keys
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

        bool isActive = activePitches_.find(p) != activePitches_.end();

        if (isActive) {
            drawRoundedRect(r, kx, ky, kw, kh, 2.0f,
                            theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f);
        } else {
            drawRoundedRect(r, kx, ky, kw, kh, 2.0f, 0.92f, 0.92f, 0.90f, 1.0f);
            drawRect(r, kx, ky + kh - 4.0f, kw, 4.0f, 0.78f, 0.78f, 0.76f, 1.0f);
        }

        // Draw C note labels on white key lips
        if (p % 12 == 0) {
            std::string cLabel = "C" + std::to_string(p / 12 - 1);
            drawText(r, cLabel, kx + 3.0f, ky + kh - 14.0f, 9.0f, 0.15f, 0.15f, 0.15f, 0.8f);
        }

        curWhiteIdx++;
    }

    // Draw Black Keys on Top
    curWhiteIdx = 0;
    for (int p = startPitch; p < endPitch; ++p) {
        if (!PianoKeyboard::isBlackKey(p)) {
            curWhiteIdx++;
            continue;
        }

        float kx = keysBounds_.x + static_cast<float>(curWhiteIdx) * whiteKeyWidth - (blackKeyWidth * 0.5f);
        float ky = keysBounds_.y;
        float kw = blackKeyWidth;
        float kh = blackKeyHeight;

        bool isActive = activePitches_.find(p) != activePitches_.end();

        if (isActive) {
            drawRoundedRect(r, kx, ky, kw, kh, 2.0f,
                            theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);
        } else {
            drawRoundedRect(r, kx, ky, kw, kh, 2.0f, 0.12f, 0.12f, 0.13f, 1.0f);
            drawRect(r, kx + 1.0f, ky + 1.0f, kw - 2.0f, kh - 4.0f, 0.22f, 0.22f, 0.24f, 1.0f);
        }
    }
}

bool VirtualKeyboardDrawer::handlePointer(const PointerEvent& ev, audio::AudioEngine& engine) {
    if (pullTabBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            toggleExpanded();
            if (!isExpanded_) {
                releaseAllNotes(engine);
            }
            return true;
        }
    }

    if (!isExpanded_) return false;

    if (octDownBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            releaseAllNotes(engine);
            setBaseOctave(baseOctave_ - 1);
            return true;
        }
    }

    if (octUpBounds_.contains(ev.x, ev.y)) {
        if (ev.action == PointerAction::Down) {
            releaseAllNotes(engine);
            setBaseOctave(baseOctave_ + 1);
            return true;
        }
    }

    if (keysBounds_.contains(ev.x, ev.y) || isDraggingKeys_) {
        if (ev.action == PointerAction::Down) {
            isDraggingKeys_ = true;
            auto hit = keyboard_.hitTest(ev.x, ev.y, keysBounds_.x, keysBounds_.y, keysBounds_.w, keysBounds_.h, 0.0f);
            if (hit.hit) {
                lastGlissandoPitch_ = hit.pitch;
                triggerNoteOn(hit.pitch, hit.velocity, engine);
            }
            return true;
        } else if (ev.action == PointerAction::Move && isDraggingKeys_) {
            auto hit = keyboard_.hitTest(ev.x, ev.y, keysBounds_.x, keysBounds_.y, keysBounds_.w, keysBounds_.h, 0.0f);
            if (hit.hit && hit.pitch != lastGlissandoPitch_) {
                if (lastGlissandoPitch_ >= 0) {
                    triggerNoteOff(lastGlissandoPitch_, engine);
                }
                lastGlissandoPitch_ = hit.pitch;
                triggerNoteOn(hit.pitch, hit.velocity, engine);
            }
            return true;
        } else if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
            if (isDraggingKeys_) {
                isDraggingKeys_ = false;
                releaseAllNotes(engine);
                lastGlissandoPitch_ = -1;
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
}

} // namespace eatsbits::ui
