#include "eatsbits/ui/widgets/piano_keyboard.hpp"

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

        int row = static_cast<int>(localY / config_.keyHeight);
        int pitch = std::clamp(config_.maxPitch - row, config_.minPitch, config_.maxPitch);
        bool isBlack = isBlackKey(pitch);

        // Vertical Piano Roll Keyboard: The further to the right, the higher the velocity
        float normX = std::clamp((mx - x) / w, 0.0f, 1.0f);
        float velocity = std::clamp(0.20f + 0.80f * normX, 0.20f, 1.0f);

        res.hit = true;
        res.pitch = pitch;
        res.velocity = velocity;
        res.isBlack = isBlack;
        res.keyPos = {x, y + static_cast<float>(row) * config_.keyHeight - scrollOffset};
        res.keySize = {isBlack ? (w * config_.blackKeyRatio) : w, config_.keyHeight};
        return res;
    } else {
        // Horizontal Virtual Piano Keyboard
        int startPitch = (config_.baseOctave + 1) * 12;
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
                        // The further to the top, the higher the velocity
                        float normTop = 1.0f - std::clamp((my - y) / blackKeyH, 0.0f, 1.0f);
                        res.hit = true;
                        res.pitch = p;
                        res.velocity = std::clamp(0.20f + 0.80f * normTop, 0.20f, 1.0f);
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
                    // The further to the top, the higher the velocity
                    float normTop = 1.0f - std::clamp((my - y) / h, 0.0f, 1.0f);
                    res.hit = true;
                    res.pitch = p;
                    res.velocity = std::clamp(0.20f + 0.80f * normTop, 0.20f, 1.0f);
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

} // namespace eatsbits::ui
