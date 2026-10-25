#ifndef EATS_EXPORT_CLAP_GUI_PUGL_BRIDGE_HPP
#define EATS_EXPORT_CLAP_GUI_PUGL_BRIDGE_HPP

#include "instrument_faceplate.hpp"
#include "plugin_gui.hpp"
#include "software_canvas.hpp"
#include <memory>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eatsbits::export_clap::gui {

/**
 * Cross-platform window lifecycle and event bridge.
 * Embeds a native child window (Win32 HWND, Cocoa NSView, Linux X11)
 * directly into the container provided by the CLAP host DAW.
 * Drives an InstrumentFaceplate matching the instrument's Eatscript GUI declaration.
 */
class PuglBridge final : public IPluginGui {
public:
    PuglBridge(IParamProvider* paramProvider,
               const clap_host* host,
               InstrumentGuiDesc desc,
               uint32_t baseWidth = 800,
               uint32_t baseHeight = 360) noexcept;
    ~PuglBridge() override;

    // --- IPluginGui Interface ---

    [[nodiscard]] bool isApiSupported(const char* api, bool isFloating) const noexcept override;
    [[nodiscard]] const char* getPreferredApi() const noexcept override;

    bool create(const char* api, bool isFloating) noexcept override;
    void destroy() noexcept override;

    bool setParent(const clap_window_t* window) noexcept override;
    bool setScale(double scale) noexcept override;

    [[nodiscard]] bool getSize(uint32_t* width, uint32_t* height) const noexcept override;
    [[nodiscard]] bool canResize() const noexcept override;
    [[nodiscard]] bool getResizeHints(clap_gui_resize_hints_t* hints) const noexcept override;
    [[nodiscard]] bool adjustSize(uint32_t* width, uint32_t* height) const noexcept override;
    bool setSize(uint32_t width, uint32_t height) noexcept override;

    bool show() noexcept override;
    bool hide() noexcept override;

    void onTimer() noexcept override;

private:
    void invalidateWindow() noexcept;

#if defined(_WIN32)
    static LRESULT CALLBACK staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT memberWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool registerWindowClass() noexcept;

    HWND hwnd_{nullptr};
    HWND parentHwnd_{nullptr};
    UINT_PTR timerId_{0};
    bool trackingMouseLeave_{false};
#elif defined(__APPLE__)
    void* nsView_{nullptr};
    void* parentNsView_{nullptr};
#else
    unsigned long xWindow_{0};
    unsigned long parentXWindow_{0};
#endif

    uint32_t baseWidth_{800};
    uint32_t baseHeight_{360};
    uint32_t currentWidth_{800};
    uint32_t currentHeight_{360};
    double currentScale_{1.0};
    bool isVisible_{false};

    SoftwareCanvas canvas_;
    InstrumentFaceplate faceplate_;
};

} // namespace eatsbits::export_clap::gui

#endif // EATS_EXPORT_CLAP_GUI_PUGL_BRIDGE_HPP
