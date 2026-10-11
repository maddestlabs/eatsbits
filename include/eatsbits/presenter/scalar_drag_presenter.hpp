#ifndef EATS_SCALAR_DRAG_PRESENTER_HPP
#define EATS_SCALAR_DRAG_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include "eatsbits/presenter/parameter_presenter.hpp"
#include <functional>
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

/**
 * @brief Spatial delta interpretation orientation for scalar parameters.
 */
enum class DragDirection {
    VerticalUpIncreases,     ///< Dragging upward increases value (knobs, faders)
    VerticalDownIncreases,   ///< Dragging downward increases value
    HorizontalRightIncreases,///< Dragging rightward increases value (horizontal sliders)
    BidirectionalPan         ///< (dx - dy) diagonal panning delta
};

namespace audio_taper {

/**
 * @brief Converts linear gain [0.0, 1.5] to normalized fader travel [0.0, 1.0].
 * Unity gain (1.0, 0 dB) maps exactly to 0.75 (75% height).
 * Max gain (1.5, +3.52 dB) maps to 1.0 (100% height).
 * Lower travel [0.0, 0.75] maps logarithmically down to -inf (0.0 gain).
 */
inline float gainToTravel(float gain) noexcept {
    gain = std::clamp(gain, 0.0f, 1.5f);
    if (gain <= 1e-4f) return 0.0f;
    if (gain >= 1.0f) {
        float frac = std::clamp((gain - 1.0f) / 0.5f, 0.0f, 1.0f);
        return 0.75f + frac * 0.25f;
    } else {
        float norm = std::cbrt(gain);
        return std::clamp(norm * 0.75f, 0.0f, 0.75f);
    }
}

/**
 * @brief Converts normalized fader travel [0.0, 1.0] to linear gain [0.0, 1.5].
 */
inline float travelToGain(float t) noexcept {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t <= 0.001f) return 0.0f;
    if (t >= 0.75f) {
        float frac = (t - 0.75f) / 0.25f;
        return 1.0f + frac * 0.5f;
    } else {
        float norm = t / 0.75f;
        return norm * norm * norm;
    }
}

/**
 * @brief Formats linear gain into authentic decibel readout string.
 */
inline std::string formatDb(float gain) {
    if (gain <= 0.001f) return "-INF dB";
    float db = 20.0f * std::log10(gain);
    if (std::abs(db) < 0.05f) return "0.0 dB";
    char buf[32];
    if (db > 0.0f) {
        std::snprintf(buf, sizeof(buf), "+%.1f dB", db);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f dB", db);
    }
    return buf;
}

} // namespace audio_taper

/**
 * @brief Configuration tuning continuous mouse/pointer drag behavior.
 */
struct ScalarDragConfig {
    ui::DragMode dragMode{ui::DragMode::None};
    DragDirection direction{DragDirection::VerticalUpIncreases};
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};   ///< Default value restored via Ctrl/Cmd+Click
    float sensitivity{1.0f / 160.0f};
    float fineRatio{0.10f};     ///< 0.1x precision fine adjustment multiplier via Shift
    float stepSnap{0.0f};       ///< 0.0 means continuous smooth variation
    bool isInteger{false};
    bool useAudioTaper{false};  ///< True if fader follows authentic console dB taper
    bool relativeMode{false};   ///< Virtual relative delta drag without border clamping
    bool captureMouse{false};   ///< Capture/lock mouse during drag and restore on release
};

/**
 * @brief Headless presenter encapsulating 1D continuous parameter dragging,
 * infinite relative delta mode, sensitivity scaling, fine control, bounds clamping,
 * step snapping, commit, and cancel.
 */
class ScalarDragPresenter : public PresenterBase, public IDragHandler {
public:
    ScalarDragPresenter() = default;

    /**
     * @brief Begin a continuous drag session with raw callbacks.
     */
    void startDrag(float startX, float startY, float initialValue, const ScalarDragConfig& config,
                   std::function<void(float)> onValueChanged = nullptr,
                   std::function<void(float)> onDragCommitted = nullptr);

    /**
     * @brief Begin a continuous drag session bound directly to a ParameterPresenter.
     */
    void startDragWithPresenter(float startX, float startY, ParameterPresenter& param,
                                const ScalarDragConfig& config,
                                std::function<void(float)> onDragCommitted = nullptr);

    /**
     * @brief Inspect pointer down for Ctrl/Cmd+click reset or double-click to edit.
     * @return True if consumed (e.g. reset to default or double-click triggered).
     */
    bool onPointerDown(const ui::PointerEvent& ev) override;

    /**
     * @brief Restore control to its default value (e.g. center for pan, 0 dB for track faders).
     */
    void resetToDefault();

    // IDragHandler Implementation
    using IDragHandler::onPointerMove;
    using IDragHandler::onPointerUp;
    void onPointerMove(const ui::PointerEvent& ev) override;
    void onPointerUp(const ui::PointerEvent& ev) override;
    void cancelDrag() override;

    [[nodiscard]] bool isDragging() const noexcept override { return isDragging_; }
    [[nodiscard]] ui::DragMode getDragMode() const noexcept override { return isDragging_ ? config_.dragMode : ui::DragMode::None; }

    [[nodiscard]] float getCurrentValue() const noexcept { return currentValue_; }
    [[nodiscard]] float getInitialValue() const noexcept { return initialValue_; }
    [[nodiscard]] float getDefaultValue() const noexcept { return config_.defaultValue; }
    void setDefaultValue(float defVal) noexcept { config_.defaultValue = defVal; }
    [[nodiscard]] const ScalarDragConfig& getConfig() const noexcept { return config_; }

    void setFineControl(bool fine) noexcept { fineControl_ = fine; }
    [[nodiscard]] bool isFineControl() const noexcept { return fineControl_; }

    [[nodiscard]] bool isMouseCaptured() const noexcept { return isMouseCaptured_; }
    [[nodiscard]] float getAccumulatedDelta() const noexcept { return accumulatedDelta_; }

    /**
     * @brief Hook for windowing shell to capture/lock mouse and restore cursor on release.
     * Arguments: captured (true = start drag, false = release drag), restoreX, restoreY.
     */
    void setOnMouseCapture(std::function<void(bool captured, float restoreX, float restoreY)> cb) {
        onMouseCapture_ = std::move(cb);
    }

    /**
     * @brief Hook triggered when double-click gesture is detected on control.
     */
    void setOnDoubleClicked(std::function<void()> cb) {
        onDoubleClicked_ = std::move(cb);
    }

private:
    bool isDragging_{false};
    bool fineControl_{false};
    bool isMouseCaptured_{false};
    float startX_{0.0f};
    float startY_{0.0f};
    float lastX_{0.0f};
    float lastY_{0.0f};
    float accumulatedDelta_{0.0f};
    float initialValue_{0.0f};
    float initialTravel_{0.75f};
    float currentValue_{0.0f};
    ScalarDragConfig config_{};

    double lastClickTimeMs_{0.0};
    float lastClickX_{0.0f};
    float lastClickY_{0.0f};

    ParameterPresenter* boundParam_{nullptr};
    std::function<void(float)> onValueChanged_{nullptr};
    std::function<void(float)> onDragCommitted_{nullptr};
    std::function<void(bool, float, float)> onMouseCapture_{nullptr};
    std::function<void()> onDoubleClicked_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_SCALAR_DRAG_PRESENTER_HPP
