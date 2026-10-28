#include "eatsbits/ui/widgets/piano_keyboard.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/audio/audio_engine.hpp"

namespace eatsbits::ui {

KeyboardHitResult PianoKeyboard::hitTest(float mx, float my, float x, float y, float w, float h, float scrollOffset) const noexcept {
    KeyboardHitResult res{};

    if (mx < x || mx > x + w || my < y || my > y + h) {
        return res;
    }

    if (config_.orientation == KeyboardOrientation::Vertical) {
        float localY = (my - y) + scrollOffset;
        int totalKeys = config_.maxPitch - config_.minPitch + 1;
        if (localY < 0.0f || localY >= static_cast<float>(totalKeys) * config_.keyHeight) {
            return res;
        }

        const float blackKeyW = w * config_.blackKeyRatio;
        const float rowH = config_.keyHeight;

        // 1. Check Black Keys (left side of gutter, width = blackKeyW)
        if (mx <= x + blackKeyW) {
            int row = static_cast<int>(localY / rowH);
            int pitch = std::clamp(config_.maxPitch - row, config_.minPitch, config_.maxPitch);
            if (isBlackKey(pitch)) {
                float normX = std::clamp((mx - x) / blackKeyW, 0.0f, 1.0f);
                res.hit = true;
                res.pitch = pitch;
                res.velocity = std::clamp(0.15f + 0.85f * normX, 0.05f, 1.0f);
                res.isBlack = true;
                res.keyPos = {x, y + static_cast<float>(row) * rowH - scrollOffset};
                res.keySize = {blackKeyW, rowH};
                return res;
            }
        }

        // 2. Check White Keys
        for (int p = config_.minPitch; p <= config_.maxPitch; ++p) {
            if (isBlackKey(p)) continue;

            float py = y + static_cast<float>(config_.maxPitch - p) * rowH - scrollOffset;

            float wy_top = py;
            if (p + 1 <= config_.maxPitch && isBlackKey(p + 1)) {
                float nextPy = y + static_cast<float>(config_.maxPitch - (p + 1)) * rowH - scrollOffset;
                wy_top = nextPy + 0.5f * rowH;
            }

            float wy_bot = py + rowH;
            if (p - 1 >= config_.minPitch && isBlackKey(p - 1)) {
                float prevPy = y + static_cast<float>(config_.maxPitch - (p - 1)) * rowH - scrollOffset;
                wy_bot = prevPy + 0.5f * rowH;
            }

            if (my >= wy_top && my <= wy_bot) {
                float normX = std::clamp((mx - x) / w, 0.0f, 1.0f);
                res.hit = true;
                res.pitch = p;
                res.velocity = std::clamp(0.15f + 0.85f * normX, 0.05f, 1.0f);
                res.isBlack = false;
                res.keyPos = {x, wy_top};
                res.keySize = {w, wy_bot - wy_top};
                return res;
            }
        }
        return res;
    } else {
        // Horizontal Virtual Piano Keyboard
        int startPitch = (config_.orientation == KeyboardOrientation::Horizontal)
                         ? (config_.baseOctave + 1) * 12
                         : config_.minPitch;
        int totalNotes = config_.octavesCount * 12;
        int numWhiteKeys = config_.octavesCount * 7;
        if (numWhiteKeys <= 0) return res;

        float whiteKeyW = w / static_cast<float>(numWhiteKeys);
        float blackKeyW = whiteKeyW * 0.65f;
        float blackKeyH = h * 0.60f;

        // 1. Check Black Keys (Top 60% overlay)
        if (my <= y + blackKeyH) {
            int whiteIdx = 0;
            for (int p = startPitch; p < startPitch + totalNotes; ++p) {
                if (isBlackKey(p)) {
                    float cx = x + static_cast<float>(whiteIdx) * whiteKeyW;
                    float bx = cx - blackKeyW * 0.5f;
                    if (mx >= bx && mx <= bx + blackKeyW) {
                        // Softer velocity on lowest y axis (top, near housing), loudest velocity on highest y axis (bottom)
                        float normY = std::clamp((my - y) / blackKeyH, 0.0f, 1.0f);
                        res.hit = true;
                        res.pitch = p;
                        res.velocity = std::clamp(0.15f + 0.85f * normY, 0.05f, 1.0f);
                        res.isBlack = true;
                        res.keyPos = {bx, y};
                        res.keySize = {blackKeyW, blackKeyH};
                        return res;
                    }
                } else {
                    whiteIdx++;
                }
            }
        }

        // 2. Check White Keys
        int whiteIdx = static_cast<int>((mx - x) / whiteKeyW);
        whiteIdx = std::clamp(whiteIdx, 0, numWhiteKeys - 1);

        int currWhite = 0;
        for (int p = startPitch; p < startPitch + totalNotes; ++p) {
            if (!isBlackKey(p)) {
                if (currWhite == whiteIdx) {
                    // Softer velocity on lowest y axis (top), loudest velocity on highest y axis (bottom)
                    float normY = std::clamp((my - y) / h, 0.0f, 1.0f);
                    res.hit = true;
                    res.pitch = p;
                    res.velocity = std::clamp(0.15f + 0.85f * normY, 0.05f, 1.0f);
                    res.isBlack = false;
                    res.keyPos = {x + static_cast<float>(currWhite) * whiteKeyW, y};
                    res.keySize = {whiteKeyW, h};
                    return res;
                }
                currWhite++;
            }
        }
    }

    return res;
}

bool PianoKeyboard::handlePointer(const PointerEvent& ev, const Rect2D& bounds, float scrollOffset, audio::AudioEngine* engine, uint32_t trackIndex) {
    if (ev.action == PointerAction::Down) {
        if (!bounds.contains(ev.x, ev.y)) return false;
        auto hit = hitTest(ev.x, ev.y, bounds.x, bounds.y, bounds.w, bounds.h, scrollOffset);
        if (hit.hit) {
            float vel = hit.velocity;
            if (ev.type == PointerType::Pen && ev.pressure > 0.01f) {
                vel = ev.pressure;
            }
            activeTouches_[ev.id] = {hit.pitch, vel};
            triggerNoteOn(hit.pitch, vel, engine, trackIndex);
            return true;
        }
    } else if (ev.action == PointerAction::Move) {
        auto it = activeTouches_.find(ev.id);
        if (it != activeTouches_.end()) {
            auto hit = hitTest(ev.x, ev.y, bounds.x, bounds.y, bounds.w, bounds.h, scrollOffset);
            if (hit.hit) {
                float vel = hit.velocity;
                if (ev.type == PointerType::Pen && ev.pressure > 0.01f) {
                    vel = ev.pressure;
                }
                if (hit.pitch != it->second.pitch) {
                    triggerNoteOff(it->second.pitch, engine, trackIndex);
                    it->second.pitch = hit.pitch;
                    it->second.velocity = vel;
                    triggerNoteOn(hit.pitch, vel, engine, trackIndex);
                } else {
                    it->second.velocity = vel;
                    activePitchVelocities_[hit.pitch] = vel;
                }
            }
            return true;
        }
    } else if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        auto it = activeTouches_.find(ev.id);
        if (it != activeTouches_.end()) {
            triggerNoteOff(it->second.pitch, engine, trackIndex);
            activeTouches_.erase(it);
            return true;
        }
    }
    return false;
}

void PianoKeyboard::triggerNoteOn(int pitch, float velocity, audio::AudioEngine* engine, uint32_t trackIndex) {
    activePitches_.insert(pitch);
    activePitchVelocities_[pitch] = velocity;
    if (onNoteOn) {
        onNoteOn(pitch, velocity);
    }
    if (engine) {
        engine->postNoteOn(static_cast<uint8_t>(std::clamp(pitch, 0, 127)), velocity, false, false, static_cast<int>(trackIndex), false);
    }
}

void PianoKeyboard::triggerNoteOff(int pitch, audio::AudioEngine* engine, uint32_t trackIndex) {
    activePitches_.erase(pitch);
    activePitchVelocities_.erase(pitch);
    if (onNoteOff) {
        onNoteOff(pitch);
    }
    if (engine) {
        engine->postNoteOff(static_cast<uint8_t>(std::clamp(pitch, 0, 127)), static_cast<int>(trackIndex), false);
    }
}

void PianoKeyboard::releaseAllNotes(audio::AudioEngine* engine, uint32_t trackIndex) {
    for (int p : activePitches_) {
        if (onNoteOff) {
            onNoteOff(p);
        }
        if (engine) {
            engine->postNoteOff(static_cast<uint8_t>(std::clamp(p, 0, 127)), static_cast<int>(trackIndex), false);
        }
    }
    activePitches_.clear();
    activeTouches_.clear();
    activePitchVelocities_.clear();
}

void PianoKeyboard::render(BatchRenderer2D& r, const Rect2D& bounds, const ThemeTokens& theme, float scrollOffset) const {
    if (config_.orientation == KeyboardOrientation::Vertical) {
        renderVertical(r, bounds, theme, scrollOffset);
    } else {
        renderHorizontal(r, bounds, theme);
    }
}

void PianoKeyboard::renderVertical(BatchRenderer2D& r, const Rect2D& bounds, const ThemeTokens& theme, float scrollOffset) const {
    BatchRenderer2D::ScissorScope gutterScissor(r, bounds);

    // Gutter background
    drawRect(r, bounds.x, bounds.y, bounds.w, bounds.h, theme.panelHeader);

    const float blackKeyW = bounds.w * config_.blackKeyRatio;
    const float rowH = config_.keyHeight;

    // Pass 1: Render White Key Bodies & Semitone Dividers
    // White keys span continuously across the gutter, with dividers at black key centers and E/F & B/C boundaries
    for (int p = config_.minPitch; p <= config_.maxPitch; ++p) {
        if (isBlackKey(p)) continue;

        float py = bounds.y + static_cast<float>(config_.maxPitch - p) * rowH - scrollOffset;

        float wy_top = py;
        if (p + 1 <= config_.maxPitch && isBlackKey(p + 1)) {
            float nextPy = bounds.y + static_cast<float>(config_.maxPitch - (p + 1)) * rowH - scrollOffset;
            wy_top = nextPy + 0.5f * rowH;
        }

        float wy_bot = py + rowH;
        if (p - 1 >= config_.minPitch && isBlackKey(p - 1)) {
            float prevPy = bounds.y + static_cast<float>(config_.maxPitch - (p - 1)) * rowH - scrollOffset;
            wy_bot = prevPy + 0.5f * rowH;
        }

        float wy = wy_top;
        float wh = wy_bot - wy_top;
        if (wy + wh < bounds.y || wy > bounds.y + bounds.h) continue;

        bool isActive = isPitchActive(p);

        // Base white key gradient
        Color baseCol(0.92f, 0.94f, 0.97f, 1.0f);
        Color gradCol(0.78f, 0.80f, 0.84f, 1.0f);
        drawRectGradient(r, bounds.x, wy, bounds.w, wh, baseCol, gradCol);

        // Active velocity color opacity feedback (higher velocity = more noticeable color)
        if (isActive) {
            float vel = 0.85f;
            auto it = activePitchVelocities_.find(p);
            if (it != activePitchVelocities_.end()) vel = it->second;
            vel = std::clamp(vel, 0.05f, 1.0f);
            float alpha = std::clamp(0.22f + 0.78f * vel, 0.20f, 1.0f);
            drawRect(r, bounds.x, wy, bounds.w, wh, theme.primaryAccent.withAlpha(alpha));
        }

        // Horizontal divider line between white keys
        drawLine(r, bounds.x, wy_bot, bounds.x + bounds.w, wy_bot,
                 Color(0.28f, 0.30f, 0.35f, 0.45f), 1.0f, 1.0f);

        // FL Studio Style Octave Badge for C notes ("C5", "C6", etc.)
        if (p % 12 == 0 && config_.showOctaveLabels) {
            std::string cLabel = getOctaveLabel(p);

            float badgeW = 28.0f;
            float badgeH = rowH - 4.0f;
            float badgeX = bounds.x + bounds.w - badgeW - 3.0f;
            float badgeY = py + 2.0f;

            Color badgeBg = isActive ? Color(1.0f, 1.0f, 1.0f, 0.35f) : Color(0.70f, 0.73f, 0.78f, 0.95f);
            Color badgeBorder = isActive ? Color(1.0f, 1.0f, 1.0f, 0.70f) : Color(0.55f, 0.58f, 0.64f, 0.85f);
            Color textCol = isActive ? Color(1.0f, 1.0f, 1.0f, 1.0f) : Color(0.20f, 0.24f, 0.30f, 1.0f);

            drawRoundedRect(r, badgeX, badgeY, badgeW, badgeH, 2.0f, badgeBg);
            drawRoundedRectOutline(r, badgeX, badgeY, badgeW, badgeH, 2.0f, badgeBorder, 0.8f, 1.0f);
            drawCenteredText(r, cLabel, Rect2D{badgeX, badgeY, badgeW, badgeH}, 9.5f, textCol);
        }
    }

    // Pass 2: Render Black Keys Overlay (Nestled snugly centered on seams between white keys)
    for (int p = config_.minPitch; p <= config_.maxPitch; ++p) {
        if (!isBlackKey(p)) continue;

        float py = bounds.y + static_cast<float>(config_.maxPitch - p) * rowH - scrollOffset;
        if (py + rowH < bounds.y || py > bounds.y + bounds.h) continue;

        bool isActive = isPitchActive(p);
        float bky = py + 1.0f;
        float bkh = rowH - 2.0f;
        float bkx = bounds.x;
        float bkw = blackKeyW;

        // FL Studio matte dark charcoal with subtle 3D top bevel
        Color topCharcoal(0.25f, 0.26f, 0.29f, 1.0f);
        Color botCharcoal(0.12f, 0.13f, 0.15f, 1.0f);
        drawRoundedRectGradient(r, bkx, bky, bkw, bkh, 2.0f, topCharcoal, botCharcoal);

        if (isActive) {
            float vel = 0.85f;
            auto it = activePitchVelocities_.find(p);
            if (it != activePitchVelocities_.end()) vel = it->second;
            vel = std::clamp(vel, 0.05f, 1.0f);
            float alpha = std::clamp(0.28f + 0.72f * vel, 0.25f, 1.0f);
            drawRoundedRect(r, bkx, bky, bkw, bkh, 2.0f, theme.secondaryAccent.withAlpha(alpha));
        }
        drawRoundedRectOutline(r, bkx, bky, bkw, bkh, 2.0f, Color(0.08f, 0.09f, 0.11f, 0.95f), 1.0f, 1.0f);
    }

    // Right border dividing gutter from the grid
    drawLine(r, bounds.x + bounds.w, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h,
             theme.borderSubtle, 0.95f, 1.5f);
}

void PianoKeyboard::renderHorizontal(BatchRenderer2D& r, const Rect2D& bounds, const ThemeTokens& theme) const {
    BatchRenderer2D::ScissorScope scissor(r, bounds);

    int startPitch = (config_.orientation == KeyboardOrientation::Horizontal)
                     ? (config_.baseOctave + 1) * 12
                     : config_.minPitch;
    int totalNotes = config_.octavesCount * 12;
    int endPitch = startPitch + totalNotes;
    int numWhiteKeys = config_.octavesCount * 7;
    if (numWhiteKeys <= 0) return;

    float whiteKeyW = bounds.w / static_cast<float>(numWhiteKeys);
    float whiteKeyH = bounds.h;
    float blackKeyW = whiteKeyW * 0.65f;
    float blackKeyH = whiteKeyH * 0.60f;

    // Pass 1: Render White Keys
    int curWhiteIdx = 0;
    for (int p = startPitch; p < endPitch; ++p) {
        if (isBlackKey(p)) continue;

        float kx = bounds.x + static_cast<float>(curWhiteIdx) * whiteKeyW;
        float ky = bounds.y;
        float kw = whiteKeyW - 1.0f;
        float kh = whiteKeyH;

        bool isDown = isPitchActive(p);
        // Base white key
        drawRoundedRectGradient(r, kx, ky, kw, kh, 3.0f,
                                Color(0.88f, 0.90f, 0.93f, 1.0f), Color(0.76f, 0.78f, 0.82f, 1.0f));

        if (isDown) {
            float vel = 0.85f;
            auto it = activePitchVelocities_.find(p);
            if (it != activePitchVelocities_.end()) vel = it->second;
            vel = std::clamp(vel, 0.05f, 1.0f);
            float alpha = std::clamp(0.22f + 0.78f * vel, 0.20f, 1.0f);
            drawRoundedRect(r, kx, ky, kw, kh, 3.0f, theme.primaryAccent.withAlpha(alpha));
        }
        drawRoundedRectOutline(r, kx, ky, kw, kh, 3.0f, theme.borderSubtle, 0.85f, 1.0f);

        // Note label on white key bottom lip for C notes
        if (p % 12 == 0 && config_.showOctaveLabels) {
            std::string cLabel = getOctaveLabel(p);
            Color lblCol = isDown ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted;
            drawCenteredText(r, cLabel, Rect2D{kx, ky + kh - 16.0f, kw, 14.0f}, 9.0f, lblCol);
        }

        curWhiteIdx++;
    }

    // Pass 2: Render Black Keys Overlay
    curWhiteIdx = 0;
    for (int p = startPitch; p < endPitch; ++p) {
        if (!isBlackKey(p)) {
            curWhiteIdx++;
            continue;
        }

        float kx = bounds.x + static_cast<float>(curWhiteIdx) * whiteKeyW - (blackKeyW * 0.5f);
        float ky = bounds.y;
        float kw = blackKeyW;
        float kh = blackKeyH;

        bool isDown = isPitchActive(p);
        // Base charcoal black key
        Color topCharcoal(0.25f, 0.26f, 0.29f, 1.0f);
        Color botCharcoal(0.12f, 0.13f, 0.15f, 1.0f);
        drawRoundedRectGradient(r, kx, ky, kw, kh, 2.0f, topCharcoal, botCharcoal);

        if (isDown) {
            float vel = 0.85f;
            auto it = activePitchVelocities_.find(p);
            if (it != activePitchVelocities_.end()) vel = it->second;
            vel = std::clamp(vel, 0.05f, 1.0f);
            float alpha = std::clamp(0.28f + 0.72f * vel, 0.25f, 1.0f);
            drawRoundedRect(r, kx, ky, kw, kh, 2.0f, theme.secondaryAccent.withAlpha(alpha));
        }
        drawRoundedRectOutline(r, kx, ky, kw, kh, 2.0f, theme.borderSubtle, 0.90f, 1.0f);
    }
}

} // namespace eatsbits::ui
