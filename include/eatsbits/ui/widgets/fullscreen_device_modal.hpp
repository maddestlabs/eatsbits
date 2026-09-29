#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "track_properties_panel.hpp"
#include "plugin_search_dialog.hpp"

#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>

namespace eatsbits::ui {

enum class DeviceTargetType : uint8_t {
    Instrument = 0,
    AudioFx = 1,
    MidiFx = 2
};

struct DeviceTarget {
    DeviceTargetType type{DeviceTargetType::Instrument};
    uint32_t trackIndex{0};
    int fxIndex{-1};
    std::string deviceName;
};

/**
 * FullscreenDeviceModal: High-Performance Dedicated Full-Display Device View.
 *
 * Replicates the Eatsbeats dedicated fullscreen device view for Instruments & FX.
 * When active, the entire DAW workspace rendering is bypassed for maximum frame rate
 * and lowest possible latency while tweaking parameters.
 *
 * Features:
 * - Top Machined Header Strip with Track Glowing LED, Device Title, Type Subtitle
 * - Preset Navigation Strip (< PREV, Preset Name, Counter, NEXT >)
 * - [ EDIT CODE ] button jumping directly to DesignView / Script Editor
 * - Tactile Vintage Chassis Screw [X] / ESC to dismiss
 * - Large, high-resolution skeuomorphic controls (potentiometers, sliders, switches, nixies)
 * - Live real-time oscilloscope display powered by audio engine scope buffer
 * - Instant real-time DSP parameter synchronization
 */
class FullscreenDeviceModal {
public:
    FullscreenDeviceModal();
    ~FullscreenDeviceModal() = default;

    void open(const DeviceTarget& target) noexcept;
    void close() noexcept;
    void toggle(const DeviceTarget& target) noexcept;
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    [[nodiscard]] const DeviceTarget& getTarget() const noexcept { return target_; }
    void setTarget(const DeviceTarget& target) noexcept { target_ = target; }

    void syncData(const TrackPropertiesDrawerData& data) noexcept;
    void setAudioScopeBuffer(const float* buffer, size_t count) noexcept;

    void layout(float screenW, float screenH) noexcept;
    void update(float dt) noexcept;
    void render(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;

    bool handlePointer(const PointerEvent& ev) noexcept;
    bool handleKey(int key, int scancode, int action, int mods) noexcept;

    // Callbacks
    std::function<void()> onClose;
    std::function<void()> onPrevPreset;
    std::function<void()> onNextPreset;
    std::function<void(uint32_t trackIndex)> onOpenCodeEditor;
    std::function<void(uint32_t trackIndex)> onOpenPresetDialog;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onAudioFxParamChanged;
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onMidiFxParamChanged;

private:
    void renderHeaderBar(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;
    void renderInstrumentFaceplate(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;
    void renderAudioFxRacks(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;
    void renderMidiFxRacks(BatchRenderer2D& r, const ThemeTokens& theme) noexcept;
    void renderOscilloscope(BatchRenderer2D& r, float ox, float oy, float ow, float oh, const ThemeTokens& theme) noexcept;

    bool isOpen_{false};
    DeviceTarget target_{DeviceTargetType::Instrument, 0, -1, ""};
    TrackPropertiesDrawerData trackData_{};
    PluginSearchDialog pluginDialog_;

    const float* scopeBuffer_{nullptr};
    size_t scopeBufferCount_{0};

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};

    // Layout bounds
    Rect2D backdropBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D bodyBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    Rect2D prevBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D nextBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D designBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D presetBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    float mouseX_{0.0f};
    float mouseY_{0.0f};

    // Knobs in fullscreen view
    struct KnobSlot {
        std::string name;
        std::string label;
        float normVal{0.5f};
        std::string readout;
        Point2D center{0.0f, 0.0f};
        float radius{32.0f};
    };
    std::vector<KnobSlot> knobSlots_;

    // Interactive Drag State
    bool isDraggingKnob_{false};
    int activeKnobIndex_{-1};
    float dragStartY_{0.0f};
    float dragStartVal_{0.0f};
    float pulsePhase_{0.0f};
};

} // namespace eatsbits::ui
