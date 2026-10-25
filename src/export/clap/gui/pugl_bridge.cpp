#include "eatsbits/export/clap/gui/pugl_bridge.hpp"
#include <algorithm>
#include <cstring>

namespace eatsbits::export_clap::gui {

PuglBridge::PuglBridge(IParamProvider* paramProvider,
                       const clap_host* host,
                       InstrumentGuiDesc desc,
                       uint32_t baseWidth,
                       uint32_t baseHeight) noexcept
    : baseWidth_(baseWidth), baseHeight_(baseHeight),
      currentWidth_(baseWidth), currentHeight_(baseHeight),
      canvas_(baseWidth, baseHeight, 1.0),
      faceplate_(paramProvider, host, std::move(desc)) {
}

PuglBridge::~PuglBridge() {
    destroy();
}

bool PuglBridge::isApiSupported(const char* api, bool isFloating) const noexcept {
    if (!api || isFloating) return false;
#if defined(_WIN32)
    return std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0;
#elif defined(__APPLE__)
    return std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0;
#else
    return std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
#endif
}

const char* PuglBridge::getPreferredApi() const noexcept {
#if defined(_WIN32)
    return CLAP_WINDOW_API_WIN32;
#elif defined(__APPLE__)
    return CLAP_WINDOW_API_COCOA;
#else
    return CLAP_WINDOW_API_X11;
#endif
}

bool PuglBridge::create(const char* api, bool isFloating) noexcept {
    if (!isApiSupported(api, isFloating)) return false;
    currentWidth_ = static_cast<uint32_t>(baseWidth_ * currentScale_);
    currentHeight_ = static_cast<uint32_t>(baseHeight_ * currentScale_);
    canvas_.resize(currentWidth_, currentHeight_, currentScale_);
    return true;
}

void PuglBridge::destroy() noexcept {
#if defined(_WIN32)
    if (timerId_ && hwnd_) {
        KillTimer(hwnd_, timerId_);
        timerId_ = 0;
    }
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    parentHwnd_ = nullptr;
#endif
    isVisible_ = false;
}

bool PuglBridge::setParent(const clap_window_t* window) noexcept {
    if (!window || !window->api) return false;

#if defined(_WIN32)
    if (std::strcmp(window->api, CLAP_WINDOW_API_WIN32) != 0 || !window->win32) {
        return false;
    }

    parentHwnd_ = static_cast<HWND>(window->win32);
    if (!registerWindowClass()) {
        return false;
    }

    currentWidth_ = static_cast<uint32_t>(baseWidth_ * currentScale_);
    currentHeight_ = static_cast<uint32_t>(baseHeight_ * currentScale_);
    canvas_.resize(currentWidth_, currentHeight_, currentScale_);

    hwnd_ = CreateWindowExW(
        0,
        L"EatsbitsPluginGui",
        L"Eatsbits Plugin",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0,
        static_cast<int>(currentWidth_),
        static_cast<int>(currentHeight_),
        parentHwnd_,
        nullptr,
        GetModuleHandleW(nullptr),
        this
    );

    if (!hwnd_) {
        return false;
    }

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    timerId_ = SetTimer(hwnd_, 101, 16, nullptr); // ~60 Hz UI timer
    return true;
#elif defined(__APPLE__)
    if (std::strcmp(window->api, CLAP_WINDOW_API_COCOA) != 0) return false;
    parentNsView_ = window->cocoa;
    return true;
#else
    if (std::strcmp(window->api, CLAP_WINDOW_API_X11) != 0) return false;
    parentXWindow_ = window->x11;
    return true;
#endif
}

bool PuglBridge::setScale(double scale) noexcept {
    if (scale <= 0.1 || scale > 8.0) return false;
    currentScale_ = scale;
    currentWidth_ = static_cast<uint32_t>(baseWidth_ * currentScale_);
    currentHeight_ = static_cast<uint32_t>(baseHeight_ * currentScale_);
    canvas_.resize(currentWidth_, currentHeight_, currentScale_);
    faceplate_.markDirty();

#if defined(_WIN32)
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, 0, 0,
                     static_cast<int>(currentWidth_),
                     static_cast<int>(currentHeight_),
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        invalidateWindow();
    }
#endif
    return true;
}

bool PuglBridge::getSize(uint32_t* width, uint32_t* height) const noexcept {
    if (!width || !height) return false;
    *width = currentWidth_;
    *height = currentHeight_;
    return true;
}

bool PuglBridge::canResize() const noexcept {
    return true;
}

bool PuglBridge::getResizeHints(clap_gui_resize_hints_t* hints) const noexcept {
    if (!hints) return false;
    hints->can_resize_horizontally = true;
    hints->can_resize_vertically = true;
    hints->preserve_aspect_ratio = true;
    hints->aspect_ratio_width = baseWidth_;
    hints->aspect_ratio_height = baseHeight_;
    return true;
}

bool PuglBridge::adjustSize(uint32_t* width, uint32_t* height) const noexcept {
    if (!width || !height) return false;
    const double targetAspect = static_cast<double>(baseWidth_) / static_cast<double>(baseHeight_);
    uint32_t w = std::clamp(*width, 400u, 3840u);
    uint32_t h = static_cast<uint32_t>(w / targetAspect + 0.5);
    if (h < 180) h = 180;
    if (h > 2160) h = 2160;
    *width = w;
    *height = h;
    return true;
}

bool PuglBridge::setSize(uint32_t width, uint32_t height) noexcept {
    if (width == 0 || height == 0) return false;
    currentWidth_ = width;
    currentHeight_ = height;
    currentScale_ = static_cast<double>(width) / static_cast<double>(baseWidth_);
    canvas_.resize(currentWidth_, currentHeight_, currentScale_);
    faceplate_.markDirty();

#if defined(_WIN32)
    if (hwnd_) {
        SetWindowPos(hwnd_, nullptr, 0, 0,
                     static_cast<int>(width),
                     static_cast<int>(height),
                     SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);
        invalidateWindow();
    }
#endif
    return true;
}

bool PuglBridge::show() noexcept {
    isVisible_ = true;
#if defined(_WIN32)
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        invalidateWindow();
    }
#endif
    return true;
}

bool PuglBridge::hide() noexcept {
    isVisible_ = false;
#if defined(_WIN32)
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
    }
#endif
    return true;
}

void PuglBridge::onTimer() noexcept {
    if (faceplate_.onTimer()) {
        invalidateWindow();
    }
}

void PuglBridge::invalidateWindow() noexcept {
#if defined(_WIN32)
    if (hwnd_ && isVisible_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
#endif
}

#if defined(_WIN32)

bool PuglBridge::registerWindowClass() noexcept {
    static bool s_registered = false;
    if (s_registered) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = staticWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"EatsbitsPluginGui";

    if (RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        s_registered = true;
        return true;
    }
    return false;
}

LRESULT CALLBACK PuglBridge::staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<PuglBridge*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self && msg == WM_CREATE) {
        const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<PuglBridge*>(cs->lpCreateParams);
        if (self) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
    }
    if (self) {
        return self->memberWndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT PuglBridge::memberWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc) {
                faceplate_.render(canvas_);

                BITMAPINFO bmi{};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = static_cast<LONG>(canvas_.width());
                bmi.bmiHeader.biHeight = -static_cast<LONG>(canvas_.height()); // Top-down DIB
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                SetDIBitsToDevice(hdc, 0, 0,
                                  canvas_.width(), canvas_.height(),
                                  0, 0, 0, canvas_.height(),
                                  canvas_.pixels(), &bmi, DIB_RGB_COLORS);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Prevent window background flickering

        case WM_MOUSEMOVE: {
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));

            if (!trackingMouseLeave_) {
                TRACKMOUSEEVENT tme{};
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                trackingMouseLeave_ = true;
            }

            if (faceplate_.onMouseMove(x, y, (wParam & MK_LBUTTON) != 0, (wParam & MK_SHIFT) != 0)) {
                invalidateWindow();
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            SetCapture(hwnd);
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            if (faceplate_.onMouseDown(x, y, (wParam & MK_SHIFT) != 0)) {
                invalidateWindow();
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            ReleaseCapture();
            const int x = static_cast<short>(LOWORD(lParam));
            const int y = static_cast<short>(HIWORD(lParam));
            if (faceplate_.onMouseUp(x, y)) {
                invalidateWindow();
            }
            return 0;
        }

        case WM_CAPTURECHANGED: {
            if (faceplate_.onMouseUp(0, 0)) {
                invalidateWindow();
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            trackingMouseLeave_ = false;
            if (faceplate_.onMouseLeave()) {
                invalidateWindow();
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == timerId_) {
                onTimer();
            }
            return 0;
        }

        case WM_SIZE: {
            const UINT w = LOWORD(lParam);
            const UINT h = HIWORD(lParam);
            if (w > 0 && h > 0 && (w != currentWidth_ || h != currentHeight_)) {
                currentWidth_ = w;
                currentHeight_ = h;
                currentScale_ = static_cast<double>(w) / static_cast<double>(baseWidth_);
                canvas_.resize(w, h, currentScale_);
                faceplate_.markDirty();
                invalidateWindow();
            }
            return 0;
        }

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

#endif // _WIN32

} // namespace eatsbits::export_clap::gui
