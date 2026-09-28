#ifndef EATS_SPACE_VISUALIZER_WIDGET_HPP
#define EATS_SPACE_VISUALIZER_WIDGET_HPP

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <string>
#include <vector>
#include <array>
#include <functional>

namespace eatsbits::ui {

enum class SpaceVisualizerMode {
    AcousticRoom2_5D,    // 2.5D room perspective, rays & draggable source/listener
    StereoGoniometer     // Lissajous 45-deg phase correlation & M/S balance scope
};

struct RoomCoordinates {
    float roomWidth{8.0f};    // meters
    float roomLength{12.0f};  // meters
    float roomHeight{3.5f};   // meters
    float sourceX{2.5f};      // meters from center-left
    float sourceZ{3.0f};      // meters from back
    float listenerX{5.5f};    // meters
    float listenerZ{8.0f};    // meters
};

struct StereoMetrics {
    float phaseCorrelation{1.0f}; // -1.0 (out of phase) to +1.0 (mono in phase)
    float balanceLR{0.0f};        // -1.0 (hard left) to +1.0 (hard right)
    float midEnergy{0.0f};
    float sideEnergy{0.0f};
    float midSideRatio{1.0f};
};

/**
 * SpaceVisualizerWidget: Dual-mode 2.5D acoustic space geometry visualizer
 * and real-time Lissajous stereo phase goniometer.
 */
class SpaceVisualizerWidget {
public:
    explicit SpaceVisualizerWidget(float height = 220.0f);
    ~SpaceVisualizerWidget() = default;

    void setMode(SpaceVisualizerMode mode) noexcept { mode_ = mode; }
    [[nodiscard]] SpaceVisualizerMode getMode() const noexcept { return mode_; }

    void setRoomCoords(const RoomCoordinates& coords) noexcept { room_ = coords; }
    [[nodiscard]] const RoomCoordinates& getRoomCoords() const noexcept { return room_; }

    // Live stereo audio feeding for real-time Lissajous Goniometer
    void feedAudio(const float* left, const float* right, size_t count) noexcept;

    [[nodiscard]] const StereoMetrics& getStereoMetrics() const noexcept { return metrics_; }

    void layout(const Rect2D& bounds);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);

    std::function<void(const RoomCoordinates& coords)> onRoomCoordsChanged;

private:
    void updateStereoMetrics() noexcept;

    SpaceVisualizerMode mode_{SpaceVisualizerMode::StereoGoniometer};
    RoomCoordinates room_{};
    StereoMetrics metrics_{};

    Rect2D bounds_{};
    Rect2D modeToggleBounds_{};
    Rect2D scopeBounds_{};
    Rect2D metricsBounds_{};

    // Pre-allocated circular scope sample history (for real-time zero allocation)
    static constexpr size_t SCOPE_POINTS = 256;
    std::array<float, SCOPE_POINTS> scopeL_{};
    std::array<float, SCOPE_POINTS> scopeR_{};
    size_t writeIdx_{0};

    // Interactive dragging in 2.5D room mode
    int dragTarget_{0}; // 0 = none, 1 = source, 2 = listener
};

} // namespace eatsbits::ui

#endif // EATS_SPACE_VISUALIZER_WIDGET_HPP
