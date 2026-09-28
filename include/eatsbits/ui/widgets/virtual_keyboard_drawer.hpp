#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <set>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "../widgets/piano_keyboard.hpp"
#include "../widgets/drum_pad_grid_widget.hpp"

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::ui {

enum class KeyboardDrawerMode : uint8_t {
    Piano = 0,
    DrumPads = 1
};

/**
 * Universal Virtual Instrument Drawer.
 * Positioned docked immediately above the bottom navigation bar across all DAW tabs.
 * Features an expandable pull tab, dual-mode auditioning (Piano Keyboard vs. 16-Pad MPC Drum Grid),
 * octave transposition, bank selection, real-time sound auditioning, and velocity response.
 */
class VirtualKeyboardDrawer {
public:
    VirtualKeyboardDrawer();
    ~VirtualKeyboardDrawer() = default;

    void layout(float screenWidth, float bottomNavTopY);
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme, audio::AudioEngine& engine);

    bool handlePointer(const PointerEvent& ev, audio::AudioEngine& engine);

    [[nodiscard]] bool isExpanded() const noexcept { return isExpanded_; }
    void setExpanded(bool exp) noexcept { isExpanded_ = exp; }
    void toggleExpanded() noexcept { isExpanded_ = !isExpanded_; }

    [[nodiscard]] KeyboardDrawerMode getMode() const noexcept { return mode_; }
    void setMode(KeyboardDrawerMode mode) noexcept { mode_ = mode; }
    void toggleMode() noexcept {
        mode_ = (mode_ == KeyboardDrawerMode::Piano) ? KeyboardDrawerMode::DrumPads : KeyboardDrawerMode::Piano;
    }

    [[nodiscard]] float getDrawerHeight() const noexcept {
        if (!isExpanded_) return kPullTabHeight;
        return (mode_ == KeyboardDrawerMode::DrumPads ? kDrumDrawerHeight : kPianoDrawerHeight) + kPullTabHeight;
    }

    [[nodiscard]] Rect2D getDrawerBounds() const noexcept { return drawerBounds_; }
    [[nodiscard]] Rect2D getPullTabBounds() const noexcept { return pullTabBounds_; }

    void setBaseOctave(int oct) noexcept;
    [[nodiscard]] int getBaseOctave() const noexcept { return baseOctave_; }

    void setActiveTrackIndex(uint32_t idx) noexcept { activeTrackIndex_ = idx; }
    [[nodiscard]] uint32_t getActiveTrackIndex() const noexcept { return activeTrackIndex_; }

    [[nodiscard]] DrumPadGridWidget& getDrumPadGrid() noexcept { return drumPadGrid_; }
    [[nodiscard]] const DrumPadGridWidget& getDrumPadGrid() const noexcept { return drumPadGrid_; }

private:
    void triggerNoteOn(int pitch, float velocity, audio::AudioEngine& engine);
    void triggerNoteOff(int pitch, audio::AudioEngine& engine);
    void releaseAllNotes(audio::AudioEngine& engine);

    uint32_t activeTrackIndex_{0};

    bool isExpanded_{false};
    KeyboardDrawerMode mode_{KeyboardDrawerMode::Piano};
    float animProgress_{0.0f}; // 0.0 (collapsed) to 1.0 (expanded)

    static constexpr float kPianoDrawerHeight = 110.0f;
    static constexpr float kDrumDrawerHeight = 164.0f;
    static constexpr float kPullTabHeight = 22.0f;
    static constexpr float kPullTabWidth = 180.0f;

    Rect2D drawerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D pullTabBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Mode toggle buttons in control strip
    Rect2D modeKeysBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D modePadsBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Piano controls
    Rect2D octDownBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D octUpBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D keysBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    int baseOctave_{3}; // Default C3
    PianoKeyboard keyboard_;
    DrumPadGridWidget drumPadGrid_;
    std::set<int> activePitches_;
    int lastGlissandoPitch_{-1};
    bool isDraggingKeys_{false};
};

} // namespace eatsbits::ui
