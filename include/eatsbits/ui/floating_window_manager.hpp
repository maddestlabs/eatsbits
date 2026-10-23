#pragma once

#include "geometry.hpp"
#include "theme.hpp"
#include "gui_panel_def.hpp"
#include "batch_renderer_2d.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

struct GLFWwindow;

namespace eatsbits::ui {

/**
 * DetachedWindowInfo: State tracking for an active native floating/detached window.
 */
struct DetachedWindowInfo {
    uint32_t trackIndex{0};
    int fxIndex{-1};
    std::string title;
    bool isClapNative{false};
    void* nativeHwnd{nullptr};
    bool isDirty{true};
    GLFWwindow* window{nullptr};
    GuiPanelDef panelDef{};
    std::unique_ptr<BatchRenderer2D> renderer;
    int draggingRow{-1};
    int draggingWidget{-1};
    float dragStartX{0.0f};
    float dragStartY{0.0f};
    float dragStartVal{0.0f};
    bool isDragging{false};
    bool isDraggingListBox{false};
    bool isListBoxDragging{false};
};

/**
 * FloatingWindowManager: High-performance multi-monitor manager for detached native windows.
 *
 * Provides DAW-grade multi-display windowing:
 * - Native desktop OS windows for Instruments, Audio FX, MIDI FX, and Mixer
 * - Dedicated event dispatching & focus tracking via GLFW
 * - Direct GDI / software rasterized vector rendering via BatchRenderer2D (DirectPresent)
 * - Seamless integration with external CLAP plugin native windows
 * - Safe fallbacks and stubs for headless / WebAssembly builds
 */
class FloatingWindowManager {
public:
    static FloatingWindowManager& instance() noexcept {
        static FloatingWindowManager s_instance;
        return s_instance;
    }

    FloatingWindowManager() = default;
    ~FloatingWindowManager() { closeAll(); }

    // Non-copyable, non-movable
    FloatingWindowManager(const FloatingWindowManager&) = delete;
    FloatingWindowManager& operator=(const FloatingWindowManager&) = delete;

    /// Open or focus a detached native OS window for an instrument on trackIndex.
    bool openDetachedInstrument(uint32_t trackIndex,
                                const std::string& instrumentName,
                                const GuiPanelDef& panel,
                                GLFWwindow* parentWindow = nullptr,
                                bool isClapNative = false,
                                void* nativeHwnd = nullptr);

    /// Closes a detached window for trackIndex if active.
    void closeDetachedInstrument(uint32_t trackIndex);

    /// Brings the detached window for trackIndex to the front of desktop.
    void focusDetachedInstrument(uint32_t trackIndex);

    /// Returns true if a detached window is currently open for trackIndex.
    [[nodiscard]] bool isDetached(uint32_t trackIndex) const noexcept;

    /// Returns the number of currently open floating windows.
    [[nodiscard]] size_t getOpenCount() const noexcept;

    /// Closes all active detached windows.
    void closeAll();

    /// Renders all active non-native floating windows with real-time audio visualization.
    void renderAllWindows(const ThemeTokens& theme, const float* scopeBuffer = nullptr, size_t scopeBufferCount = 0);

    /// Returns pointer to active panel def if detached, or nullptr.
    [[nodiscard]] const GuiPanelDef* getPanelDef(uint32_t trackIndex) const noexcept;

    /// Returns mutable pointer to active panel def if detached, or nullptr.
    [[nodiscard]] GuiPanelDef* getMutablePanelDef(uint32_t trackIndex) noexcept;

    /// Renders a specific detached window (e.g. on OS refresh callback).
    void renderWindow(GLFWwindow* w);

    /// Callback triggered when a detached window is closed by the user clicking [X].
    std::function<void(uint32_t trackIndex)> onWindowClosed;

    /// Callback to request a frame redraw for a detached window.
    std::function<void(uint32_t trackIndex, BatchRenderer2D& r)> onRenderDetached;

    /// Parameter modification callback from detached windows.
    std::function<void(uint32_t trackIndex, const std::string& paramName, float normVal)> onParamChanged;

    /// Audition note callback for auditioning when presets/types are selected in detached windows.
    std::function<void(uint32_t trackIndex, uint8_t note, float velocity)> onAuditionNote;

    void handleMouseMove(DetachedWindowInfo& info, float xpos, float ypos);
    void handleMouseButton(DetachedWindowInfo& info, int button, int action, float xpos, float ypos);
    void handleScroll(DetachedWindowInfo& info, float xoffset, float yoffset, float xpos, float ypos);
    void selectListBoxOption(DetachedWindowInfo& info, GuiWidgetDef& w, int newIdx);
    void triggerRandomize(DetachedWindowInfo& info);

private:
    std::unordered_map<uint32_t, DetachedWindowInfo> windows_;
    ThemeTokens currentTheme_{};
};

} // namespace eatsbits::ui
