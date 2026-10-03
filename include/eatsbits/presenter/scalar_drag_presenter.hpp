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
    float sensitivity{1.0f / 160.0f};
    float fineRatio{0.15f};
    float stepSnap{0.0f};   ///< 0.0 means continuous smooth variation
    bool isInteger{false};
    bool useAudioTaper{false}; ///< True if fader follows authentic console dB taper
};

/**
 * @brief Headless presenter encapsulating 1D continuous parameter dragging,
 * sensitivity scaling, fine control, bounds clamping, step snapping, commit, and cancel.
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
    [[nodiscard]] const ScalarDragConfig& getConfig() const noexcept { return config_; }

    void setFineControl(bool fine) noexcept { fineControl_ = fine; }
    [[nodiscard]] bool isFineControl() const noexcept { return fineControl_; }

private:
    bool isDragging_{false};
    bool fineControl_{false};
    float startX_{0.0f};
    float startY_{0.0f};
    float initialValue_{0.0f};
    float initialTravel_{0.75f};
    float currentValue_{0.0f};
    ScalarDragConfig config_{};

    ParameterPresenter* boundParam_{nullptr};
    std::function<void(float)> onValueChanged_{nullptr};
    std::function<void(float)> onDragCommitted_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_SCALAR_DRAG_PRESENTER_HPP
