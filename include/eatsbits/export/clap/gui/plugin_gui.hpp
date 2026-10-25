#ifndef EATS_EXPORT_CLAP_GUI_PLUGIN_GUI_HPP
#define EATS_EXPORT_CLAP_GUI_PLUGIN_GUI_HPP

#include <clap/clap.h>
#include <cstdint>

namespace eatsbits::export_clap::gui {

/**
 * Abstract cross-platform GUI interface managed by ClapPluginShell.
 * Bridges host window attachment (Win32, Cocoa, X11) to the high-performance
 * vector rasterization faceplate.
 */
class IPluginGui {
public:
    virtual ~IPluginGui() = default;

    [[nodiscard]] virtual bool isApiSupported(const char* api, bool isFloating) const noexcept = 0;
    [[nodiscard]] virtual const char* getPreferredApi() const noexcept = 0;

    virtual bool create(const char* api, bool isFloating) noexcept = 0;
    virtual void destroy() noexcept = 0;

    virtual bool setParent(const clap_window_t* window) noexcept = 0;
    virtual bool setScale(double scale) noexcept = 0;

    [[nodiscard]] virtual bool getSize(uint32_t* width, uint32_t* height) const noexcept = 0;
    [[nodiscard]] virtual bool canResize() const noexcept = 0;
    [[nodiscard]] virtual bool getResizeHints(clap_gui_resize_hints_t* hints) const noexcept = 0;
    [[nodiscard]] virtual bool adjustSize(uint32_t* width, uint32_t* height) const noexcept = 0;
    virtual bool setSize(uint32_t width, uint32_t height) noexcept = 0;

    virtual bool show() noexcept = 0;
    virtual bool hide() noexcept = 0;

    /// 60 Hz timer hook called on the main/UI thread for parameter synchronization and repainting.
    virtual void onTimer() noexcept = 0;
};

} // namespace eatsbits::export_clap::gui

#endif // EATS_EXPORT_CLAP_GUI_PLUGIN_GUI_HPP
