#ifndef EATS_TRACKER_CELL_DIALOG_HPP
#define EATS_TRACKER_CELL_DIALOG_HPP

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace eatsbits::ui {

struct PianoRollNote;

struct TrackerChipFxEntry {
    std::string key;
    std::string label;
    std::string value{"00"};
    Rect2D sliderBounds{};
    Rect2D downBtnBounds{};
    Rect2D upBtnBounds{};
};

/**
 * @brief Modal dialog to inspect and edit a single Tracker Cell on double-click/double-tap.
 * Features:
 * - Single continuous touch slider for pitch selection from C0 (12) up to C8 (108) with fine semitone nudging
 * - Velocity slider & quick presets (25%, 50%, 75%, 100%)
 * - Slide/Portamento toggle
 * - Tracker Effect Command (00 None, 01 Up, 02 Down, 03 Tone Porta, 04 Vibrato, 0A Vol Slide)
 * - Custom Chip FX parameter editors (echo, alg, fb, etc.) with hex value sliders
 * - Note deletion / clearing
 */
class TrackerCellDialog {
public:
    TrackerCellDialog();
    ~TrackerCellDialog() = default;

    void open(int step, int lane, const std::string& trackName, const PianoRollNote* note = nullptr,
              const std::vector<std::string>& customColKeys = {});
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] int getStep() const noexcept { return step_; }
    [[nodiscard]] int getLane() const noexcept { return lane_; }
    [[nodiscard]] uint8_t getPitch() const noexcept { return pitch_; }
    [[nodiscard]] float getVelocity() const noexcept { return velocity_; }
    [[nodiscard]] bool isSlide() const noexcept { return isSlide_; }
    [[nodiscard]] const std::string& getEffectCommand() const noexcept { return effectCommand_; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& getCustomParams() const noexcept { return customParams_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }

    std::function<void(int step, int lane, uint8_t pitch, float velocity, bool isSlide, const std::string& fxCmd,
                       const std::unordered_map<std::string, std::string>& chipParams)> onApply;
    std::function<void(int step, int lane)> onDelete;
    std::function<void(uint8_t pitch, float velocity)> onAudition;

private:
    void auditionCurrentPitch();

    bool isOpen_{false};
    int step_{0};
    int lane_{0};
    std::string trackName_{""};
    bool hasExistingNote_{false};

    uint8_t pitch_{60}; // C4
    float velocity_{0.85f};
    bool isSlide_{false};
    std::string effectCommand_{"00"};
    std::unordered_map<std::string, std::string> customParams_{};
    std::vector<TrackerChipFxEntry> chipFxEntries_{};

    bool isDraggingPitch_{false};
    bool isDraggingVelocity_{false};
    int draggingChipFxIdx_{-1};

    // Layout bounds
    Rect2D dialogBounds_{};
    Rect2D closeBtnBounds_{};

    // Pitch C0 - C8 slider bounds
    Rect2D pitchDownBtnBounds_{};
    Rect2D pitchUpBtnBounds_{};
    Rect2D pitchSliderBounds_{};
    Rect2D auditionBtnBounds_{};

    // Velocity bounds
    Rect2D velSliderBounds_{};
    std::vector<Rect2D> velPresetBounds_{};

    // Slide bounds
    Rect2D slideToggleBounds_{};

    // FX Command bounds
    std::vector<Rect2D> fxPresetBounds_{};

    // Action buttons
    Rect2D deleteBtnBounds_{};
    Rect2D cancelBtnBounds_{};
    Rect2D applyBtnBounds_{};
};

} // namespace eatsbits::ui

#endif // EATS_TRACKER_CELL_DIALOG_HPP
