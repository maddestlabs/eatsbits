#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <cmath>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"

namespace eatsbits::audio {
    class AudioEngine;
}

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

struct ActiveKeyTouch {
    int pitch{-1};
    float velocity{0.85f};
};

struct PianoKeyboardConfig {
    KeyboardOrientation orientation{KeyboardOrientation::Vertical};
    int minPitch{24};              // C1 (MIDI note 24)
    int maxPitch{96};              // C7 (MIDI note 96) -> 73 semitones total
    int baseOctave{3};             // For horizontal drawer: C3 (MIDI note 48)
    int octavesCount{3};           // For horizontal drawer: 3 octaves (36 keys)
    float keyWidth{80.0f};         // Width in vertical mode (FL Studio metric: 80px)
    float keyHeight{22.0f};        // Semitone row height in vertical mode (default 22px)
    float blackKeyRatio{0.64f};    // Black key width ratio: 0.64f (~51px black key, 29px white lip)
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

    explicit PianoKeyboard(const PianoKeyboardConfig& config = {}) : config_(config) {
        if (config_.orientation == KeyboardOrientation::Horizontal) {
            config_.minPitch = (config_.baseOctave + 1) * 12;
            config_.maxPitch = config_.minPitch + (config_.octavesCount * 12);
        }
    }

    [[nodiscard]] const PianoKeyboardConfig& getConfig() const noexcept { return config_; }
    void setConfig(const PianoKeyboardConfig& config) noexcept {
        config_ = config;
        if (config_.orientation == KeyboardOrientation::Horizontal) {
            config_.minPitch = (config_.baseOctave + 1) * 12;
            config_.maxPitch = config_.minPitch + (config_.octavesCount * 12);
        }
    }

    [[nodiscard]] KeyboardOrientation getOrientation() const noexcept { return config_.orientation; }
    void setOrientation(KeyboardOrientation orient) noexcept { config_.orientation = orient; }

    [[nodiscard]] int getMinPitch() const noexcept { return config_.minPitch; }
    void setMinPitch(int p) noexcept { config_.minPitch = p; }
    [[nodiscard]] int getMaxPitch() const noexcept { return config_.maxPitch; }
    void setMaxPitch(int p) noexcept { config_.maxPitch = p; }
    [[nodiscard]] int getBaseOctave() const noexcept { return config_.baseOctave; }
    void setBaseOctave(int oct) noexcept {
        config_.baseOctave = std::clamp(oct, 1, 6);
        if (config_.orientation == KeyboardOrientation::Horizontal) {
            config_.minPitch = (config_.baseOctave + 1) * 12;
            config_.maxPitch = config_.minPitch + (config_.octavesCount * 12);
        }
    }
    [[nodiscard]] int getOctavesCount() const noexcept { return config_.octavesCount; }
    void setOctavesCount(int count) noexcept {
        config_.octavesCount = std::clamp(count, 1, 7);
        if (config_.orientation == KeyboardOrientation::Horizontal) {
            config_.maxPitch = config_.minPitch + (config_.octavesCount * 12);
        }
    }

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

    // Active pitch inspection & state
    [[nodiscard]] bool isPitchActive(int pitch) const noexcept {
        return activePitches_.find(pitch) != activePitches_.end();
    }
    [[nodiscard]] const std::set<int>& getActivePitches() const noexcept { return activePitches_; }
    [[nodiscard]] bool hasActiveTouches() const noexcept { return !activeTouches_.empty(); }

    // Real-time note auditioning & ADSR routing
    void triggerNoteOn(int pitch, float velocity, audio::AudioEngine* engine, uint32_t trackIndex = 0);
    void triggerNoteOff(int pitch, audio::AudioEngine* engine, uint32_t trackIndex = 0);
    void releaseAllNotes(audio::AudioEngine* engine, uint32_t trackIndex = 0);

    // Event handling with multi-touch & glissando
    bool handlePointer(const PointerEvent& ev, const Rect2D& bounds, float scrollOffset, audio::AudioEngine* engine, uint32_t trackIndex = 0);

    // Rendering with FL Studio aesthetic parity
    void render(BatchRenderer2D& r, const Rect2D& bounds, const ThemeTokens& theme, float scrollOffset = 0.0f) const;

    // Optional event callbacks
    std::function<void(int pitch, float velocity)> onNoteOn;
    std::function<void(int pitch)> onNoteOff;

private:
    void renderVertical(BatchRenderer2D& r, const Rect2D& bounds, const ThemeTokens& theme, float scrollOffset) const;
    void renderHorizontal(BatchRenderer2D& r, const Rect2D& bounds, const ThemeTokens& theme) const;

    PianoKeyboardConfig config_;
    std::set<int> activePitches_;
    std::unordered_map<int, ActiveKeyTouch> activeTouches_;
    std::unordered_map<int, float> activePitchVelocities_;
};

} // namespace eatsbits::ui
