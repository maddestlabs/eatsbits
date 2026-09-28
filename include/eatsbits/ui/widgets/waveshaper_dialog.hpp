#ifndef EATS_WAVESHAPER_DIALOG_HPP
#define EATS_WAVESHAPER_DIALOG_HPP

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <string>
#include <vector>
#include <array>
#include <functional>

namespace eatsbits::ui {

enum class WaveshaperShape {
    SoftSaturation = 0, // Soft S-Curve / Tanh
    TubeAsymmetric = 1, // Asymmetric tube overdrive
    SineWavefold   = 2, // Smooth trigonometric wavefolder
    Angry1         = 3, // Kilohearts-style triangle multi-fold
    Angry2         = 4  // Extreme wavefold crunch
};

struct WaveshaperParams {
    WaveshaperShape shape{WaveshaperShape::SoftSaturation};
    float tension{0.0f};      // -1.0 to 1.0 (curve convexity / concavity)
    float preGain{1.0f};      // 0.1 to 4.0 drive
    float postGain{1.0f};     // 0.0 to 4.0 level
    bool dcFilter{true};      // DC-blocking highpass
    float dryWet{1.0f};       // 0.0 (bypass) to 1.0 (100% wet)
    int oversampling{2};      // 1x, 2x, 4x
};

/**
 * Interactive 2D transfer curve canvas and distortion shaping modal dialog.
 * Inspired by FL Studio WaveShaper and Kilohearts Shaper.
 */
class WaveshaperDialog {
public:
    WaveshaperDialog();
    ~WaveshaperDialog() = default;

    // Evaluates the transfer curve for an input sample x in [-1.0, 1.0]
    static float evaluateTransfer(const WaveshaperParams& params, float x) noexcept;

    void open(const WaveshaperParams& initialParams = {});
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] const WaveshaperParams& getParams() const noexcept { return params_; }
    void setParams(const WaveshaperParams& params) { params_ = params; updateHarmonics(); }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }

    // Computes harmonic spectrum (amplitudes of harmonics 1 through 8)
    [[nodiscard]] const std::array<float, 8>& getHarmonics() const noexcept { return harmonics_; }

    std::function<void(const WaveshaperParams& params)> onParamsChanged;
    std::function<void(const WaveshaperParams& params)> onApplied;

private:
    void updateHarmonics() noexcept;

    bool isOpen_{false};
    WaveshaperParams params_{};
    Rect2D dialogBounds_{};
    Rect2D curveCanvasBounds_{};
    Rect2D harmonicsBounds_{};

    // Sliders & Controls hitboxes
    Rect2D preGainBounds_{};
    Rect2D postGainBounds_{};
    Rect2D dryWetBounds_{};
    Rect2D dcFilterBounds_{};
    Rect2D oversamplingBounds_{};
    Rect2D applyBtnBounds_{};
    Rect2D closeBtnBounds_{};

    // Preset selection pills
    std::vector<Rect2D> shapePillBounds_{};

    // Drag tracking
    bool isDraggingCurve_{false};
    bool isDraggingPreGain_{false};
    bool isDraggingPostGain_{false};
    bool isDraggingDryWet_{false};

    // Harmonic spectrum analysis output
    std::array<float, 8> harmonics_{};
};

} // namespace eatsbits::ui

#endif // EATS_WAVESHAPER_DIALOG_HPP
