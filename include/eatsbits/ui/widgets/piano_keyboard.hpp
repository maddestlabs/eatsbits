#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "../geometry.hpp"

namespace eatsbits::ui {

enum class KeyboardOrientation {
    Vertical,   // Vertical Piano Roll gutter (left column, scrolling vertically with pitch)
    Horizontal  // Horizontal Virtual Piano Keyboard (drawer at bottom of workspace)
};

struct KeyboardHitResult {
    bool hit{false};
    int pitch{0};
    float velocity{0.85f};
    bool isBlack{false};
    Point keyPos{0.0f, 0.0f};
    Point keySize{0.0f, 0.0f};
};

struct PianoKeyboardConfig {
    KeyboardOrientation orientation{KeyboardOrientation::Vertical};
    int minPitch{24};              // C1 (MIDI note 24)
    int maxPitch{96};              // C7 (MIDI note 96) -> 73 semitones total
    int baseOctave{3};             // For horizontal drawer: C3 (MIDI note 48)
    int octavesCount{3};           // For horizontal drawer: 3 octaves (36 keys)
    float keyWidth{72.0f};         // Width in vertical mode (FL Studio metric: 72px)
    float keyHeight{22.0f};        // Semitone row height in vertical mode (default 22px)
    float blackKeyRatio{0.64f};    // Black key width ratio: 0.64f (~46px black key, 26px white lip)
    bool showOctaveLabels{true};   // Print "C1", "C2", ... on white C key lip
};

class PianoKeyboard {
public:
    static constexpr bool isBlackKey(int midiPitch) noexcept {
        int note = midiPitch % 12;
        if (note < 0) note += 12;
        return (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);
    }

    static std::string getNoteName(int midiPitch) {
        static const char* const kNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int note = midiPitch % 12;
        if (note < 0) note += 12;
        int octave = (midiPitch / 12) - 1;
        return std::string(kNames[note]) + std::to_string(octave);
    }

    static std::string getOctaveLabel(int midiPitch) {
        int note = midiPitch % 12;
        if (note < 0) note += 12;
        int octave = (midiPitch / 12) - 1;
        if (note == 0) {
            return "C" + std::to_string(octave);
        }
        return "";
    }

    explicit PianoKeyboard(const PianoKeyboardConfig& config = {}) : config_(config) {}

    [[nodiscard]] const PianoKeyboardConfig& getConfig() const noexcept { return config_; }
    void setConfig(const PianoKeyboardConfig& config) noexcept { config_ = config; }

    [[nodiscard]] KeyboardOrientation getOrientation() const noexcept { return config_.orientation; }
    void setOrientation(KeyboardOrientation orient) noexcept { config_.orientation = orient; }

    [[nodiscard]] int getMinPitch() const noexcept { return config_.minPitch; }
    [[nodiscard]] int getMaxPitch() const noexcept { return config_.maxPitch; }
    [[nodiscard]] int getBaseOctave() const noexcept { return config_.baseOctave; }
    void setBaseOctave(int oct) noexcept { config_.baseOctave = std::clamp(oct, 1, 6); }
    [[nodiscard]] int getOctavesCount() const noexcept { return config_.octavesCount; }

    [[nodiscard]] float getKeyWidth() const noexcept { return config_.keyWidth; }
    void setKeyWidth(float w) noexcept { config_.keyWidth = w; }
    [[nodiscard]] float getKeyHeight() const noexcept { return config_.keyHeight; }
    void setKeyHeight(float h) noexcept { config_.keyHeight = h; }
    [[nodiscard]] float getBlackKeyRatio() const noexcept { return config_.blackKeyRatio; }
    void setBlackKeyRatio(float r) noexcept { config_.blackKeyRatio = std::clamp(r, 0.4f, 0.85f); }

    // Hit-testing logic (decoupled coordinate mapping)
    [[nodiscard]] KeyboardHitResult hitTest(float mx, float my, float x, float y, float w, float h, float scrollOffset = 0.0f) const noexcept;

    // Pitch to coordinate projection
    [[nodiscard]] float pitchToVerticalY(int pitch, float topY, float scrollY) const noexcept {
        return topY + static_cast<float>(config_.maxPitch - pitch) * config_.keyHeight - scrollY;
    }

    [[nodiscard]] int verticalYToPitch(float localY, float scrollY) const noexcept {
        float effectiveY = localY + scrollY;
        int row = static_cast<int>(effectiveY / config_.keyHeight);
        return std::clamp(config_.maxPitch - row, config_.minPitch, config_.maxPitch);
    }

    [[nodiscard]] int getTotalVerticalSemitones() const noexcept {
        return config_.maxPitch - config_.minPitch + 1;
    }

    [[nodiscard]] float getTotalVerticalContentHeight() const noexcept {
        return static_cast<float>(getTotalVerticalSemitones()) * config_.keyHeight;
    }

private:
    PianoKeyboardConfig config_;
};

} // namespace eatsbits::ui
