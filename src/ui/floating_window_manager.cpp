#include "eatsbits/ui/floating_window_manager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

#if !defined(__EMSCRIPTEN__)
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#endif

namespace eatsbits::ui {

bool FloatingWindowManager::openDetachedInstrument(uint32_t trackIndex,
                                                   const std::string& instrumentName,
                                                   const GuiPanelDef& panel,
                                                   GLFWwindow* parentWindow,
                                                   bool isClapNative,
                                                   void* nativeHwnd) {
    (void)parentWindow;
    auto it = windows_.find(trackIndex);
    if (it != windows_.end()) {
        focusDetachedInstrument(trackIndex);
        return true;
    }

#if !defined(__EMSCRIPTEN__)
    std::string winTitle = (instrumentName.empty() ? "Instrument" : instrumentName) + " [Detached] - Eatsbits";

    DetachedWindowInfo info;
    info.trackIndex = trackIndex;
    info.title = winTitle;
    info.isClapNative = isClapNative;
    info.nativeHwnd = nativeHwnd;
    info.panelDef = panel;

    // Check if panel contains absolute coordinates
    bool hasAbsoluteBounds = false;
    for (const auto& w : panel.widgets) {
        if (w.designBounds.w > 0.0f || w.designBounds.h > 0.0f) {
            hasAbsoluteBounds = true;
            break;
        }
    }
    if (panel.layoutMode == GuiLayoutMode::AbsoluteCanvas && hasAbsoluteBounds) {
        info.panelDef.layoutMode = GuiLayoutMode::AbsoluteCanvas;
    } else {
        info.panelDef.layoutMode = GuiLayoutMode::AutoRowFlow;
    }

    if (isClapNative) {
        // External CLAP plugin manages its own top-level OS window
        info.window = nullptr;
        windows_[trackIndex] = std::move(info);
        return true;
    }

    int initW = 860;
    int initH = 460;
    if (info.panelDef.rows.size() == 2) {
        initW = 880;
        initH = 460;
    } else if (info.panelDef.rows.size() == 1) {
        initW = 760;
        initH = 340;
    } else if (panel.designWidth > 100.0f && panel.designHeight > 100.0f) {
        initW = static_cast<int>(panel.designWidth);
        initH = static_cast<int>(panel.designHeight);
    }

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);

    GLFWwindow* win = glfwCreateWindow(initW, initH, winTitle.c_str(), nullptr, nullptr);
    if (!win) {
        std::cerr << "[FloatingWindowManager] Warning: Failed to spawn detached OS window." << std::endl;
        return false;
    }

    int winX = 100;
    int winY = 100;
    if (parentWindow) {
        int px = 0, py = 0, pw = 0, ph = 0;
        glfwGetWindowPos(parentWindow, &px, &py);
        glfwGetWindowSize(parentWindow, &pw, &ph);
        winX = px + std::max(40, (pw - initW) / 2);
        winY = py + std::max(40, (ph - initH) / 2);
    }
    glfwSetWindowPos(win, winX, winY);

    glfwShowWindow(win);
    glfwFocusWindow(win);

#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(win);
    if (hwnd) {
        SetWindowPos(hwnd, HWND_TOP, winX, winY, initW, initH, SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
    }
#endif

    info.window = win;
    info.renderer = std::make_unique<BatchRenderer2D>();
    info.renderer->initialize(win, static_cast<uint32_t>(initW), static_cast<uint32_t>(initH), RenderBackendType::Filament);
    info.renderer->setDirectPresent(true);

    // Perform immediate first frame paint
    BatchRenderer2D::setActiveBatchRenderer(info.renderer.get());
    info.renderer->resize(static_cast<uint32_t>(initW), static_cast<uint32_t>(initH));
    info.renderer->beginFrame(static_cast<float>(initW), static_cast<float>(initH));
    Rect2D initPanelRect{0.0f, 0.0f, static_cast<float>(initW), static_cast<float>(initH)};
    drawGuiFaceplate(*info.renderer, info.panelDef, initPanelRect, currentTheme_, nullptr, 0, -1, -1);
    info.renderer->endFrame();
    BatchRenderer2D::setActiveBatchRenderer(nullptr);

    glfwSetWindowUserPointer(win, this);
    glfwSetWindowCloseCallback(win, [](GLFWwindow* w) {
        auto* mgr = static_cast<FloatingWindowManager*>(glfwGetWindowUserPointer(w));
        if (!mgr) return;
        for (auto it = mgr->windows_.begin(); it != mgr->windows_.end(); ++it) {
            if (it->second.window == w) {
                uint32_t tIdx = it->first;
                if (mgr->onWindowClosed) {
                    mgr->onWindowClosed(tIdx);
                }
                mgr->windows_.erase(it);
                glfwDestroyWindow(w);
                break;
            }
        }
    });

    glfwSetWindowRefreshCallback(win, [](GLFWwindow* w) {
        auto* mgr = static_cast<FloatingWindowManager*>(glfwGetWindowUserPointer(w));
        if (mgr) {
            mgr->renderWindow(w);
        }
    });

    glfwSetCursorPosCallback(win, [](GLFWwindow* w, double xpos, double ypos) {
        auto* mgr = static_cast<FloatingWindowManager*>(glfwGetWindowUserPointer(w));
        if (!mgr) return;
        for (auto& pair : mgr->windows_) {
            if (pair.second.window == w) {
                mgr->handleMouseMove(pair.second, static_cast<float>(xpos), static_cast<float>(ypos));
                if (pair.second.isDirty) {
                    mgr->renderWindow(w);
                }
                break;
            }
        }
    });

    glfwSetMouseButtonCallback(win, [](GLFWwindow* w, int button, int action, int /*mods*/) {
        auto* mgr = static_cast<FloatingWindowManager*>(glfwGetWindowUserPointer(w));
        if (!mgr) return;
        double xpos, ypos;
        glfwGetCursorPos(w, &xpos, &ypos);
        for (auto& pair : mgr->windows_) {
            if (pair.second.window == w) {
                mgr->handleMouseButton(pair.second, button, action, static_cast<float>(xpos), static_cast<float>(ypos));
                if (pair.second.isDirty) {
                    mgr->renderWindow(w);
                }
                break;
            }
        }
    });

    windows_[trackIndex] = std::move(info);
    return true;
#else
    (void)trackIndex;
    (void)instrumentName;
    (void)panel;
    (void)parentWindow;
    (void)isClapNative;
    return false;
#endif
}

void FloatingWindowManager::closeDetachedInstrument(uint32_t trackIndex) {
    auto it = windows_.find(trackIndex);
    if (it == windows_.end()) return;

    if (onWindowClosed) {
        onWindowClosed(trackIndex);
    }
#if !defined(__EMSCRIPTEN__)
    if (it->second.window) {
        glfwDestroyWindow(it->second.window);
    }
#endif
    windows_.erase(it);
}

const GuiPanelDef* FloatingWindowManager::getPanelDef(uint32_t trackIndex) const noexcept {
    auto it = windows_.find(trackIndex);
    if (it != windows_.end()) {
        return &it->second.panelDef;
    }
    return nullptr;
}

GuiPanelDef* FloatingWindowManager::getMutablePanelDef(uint32_t trackIndex) noexcept {
    auto it = windows_.find(trackIndex);
    if (it != windows_.end()) {
        return &it->second.panelDef;
    }
    return nullptr;
}

void FloatingWindowManager::focusDetachedInstrument(uint32_t trackIndex) {
    auto it = windows_.find(trackIndex);
    if (it == windows_.end()) return;

#if !defined(__EMSCRIPTEN__)
    if (it->second.isClapNative) {
#if defined(_WIN32)
        if (it->second.nativeHwnd && IsWindow(static_cast<HWND>(it->second.nativeHwnd))) {
            HWND hwnd = static_cast<HWND>(it->second.nativeHwnd);
            ShowWindow(hwnd, SW_RESTORE);
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
        }
#endif
        return;
    }

    if (it->second.window) {
        glfwShowWindow(it->second.window);
        glfwFocusWindow(it->second.window);
#if defined(_WIN32)
        HWND hwnd = glfwGetWin32Window(it->second.window);
        if (hwnd) {
            ShowWindow(hwnd, SW_RESTORE);
            BringWindowToTop(hwnd);
            SetForegroundWindow(hwnd);
        }
#endif
        renderWindow(it->second.window);
    }
#endif
}

bool FloatingWindowManager::isDetached(uint32_t trackIndex) const noexcept {
    return windows_.find(trackIndex) != windows_.end();
}

size_t FloatingWindowManager::getOpenCount() const noexcept {
    return windows_.size();
}

void FloatingWindowManager::closeAll() {
#if !defined(__EMSCRIPTEN__)
    for (auto& pair : windows_) {
        if (pair.second.window) {
            glfwDestroyWindow(pair.second.window);
        }
    }
#endif
    windows_.clear();
}

void FloatingWindowManager::renderAllWindows(const ThemeTokens& theme, const float* scopeBuffer, size_t scopeBufferCount) {
#if !defined(__EMSCRIPTEN__)
    if (windows_.empty()) return;
    currentTheme_ = theme;
    for (auto& pair : windows_) {
        auto& info = pair.second;
        if (!info.window || info.isClapNative || !info.renderer) continue;
        int curW = 0, curH = 0;
        glfwGetFramebufferSize(info.window, &curW, &curH);
        if (curW <= 0 || curH <= 0) continue;

        BatchRenderer2D::setActiveBatchRenderer(info.renderer.get());
        info.renderer->resize(static_cast<uint32_t>(curW), static_cast<uint32_t>(curH));
        info.renderer->beginFrame(static_cast<float>(curW), static_cast<float>(curH));
        Rect2D panelRect{0.0f, 0.0f, static_cast<float>(curW), static_cast<float>(curH)};
        // Localized stable lighting per detached OS window (isolated from main window CRT swaying spotlight / rumble)
        ThemeTokens localTheme = theme;
        localTheme.globalLight = LightSource2D(static_cast<float>(curW) * 0.5f, -180.0f, 500.0f, theme.globalLight.intensity);

        drawGuiFaceplate(*info.renderer, info.panelDef, panelRect, localTheme, scopeBuffer, scopeBufferCount, info.draggingRow, info.draggingWidget);
        info.renderer->endFrame();
        BatchRenderer2D::setActiveBatchRenderer(nullptr);
        info.isDirty = false;
    }
#else
    (void)theme;
    (void)scopeBuffer;
    (void)scopeBufferCount;
#endif
}

void FloatingWindowManager::renderWindow(GLFWwindow* w) {
#if !defined(__EMSCRIPTEN__)
    for (auto& pair : windows_) {
        auto& info = pair.second;
        if (info.window == w && !info.isClapNative && info.renderer) {
            int curW = 0, curH = 0;
            glfwGetFramebufferSize(info.window, &curW, &curH);
            if (curW <= 0 || curH <= 0) return;

            BatchRenderer2D::setActiveBatchRenderer(info.renderer.get());
            info.renderer->resize(static_cast<uint32_t>(curW), static_cast<uint32_t>(curH));
            info.renderer->beginFrame(static_cast<float>(curW), static_cast<float>(curH));
            Rect2D panelRect{0.0f, 0.0f, static_cast<float>(curW), static_cast<float>(curH)};

            // Localized stable lighting per detached OS window (isolated from main window CRT swaying spotlight / rumble)
            ThemeTokens localTheme = currentTheme_;
            localTheme.globalLight = LightSource2D(static_cast<float>(curW) * 0.5f, -180.0f, 500.0f, currentTheme_.globalLight.intensity);

            drawGuiFaceplate(*info.renderer, info.panelDef, panelRect, localTheme, nullptr, 0, info.draggingRow, info.draggingWidget);
            info.renderer->endFrame();
            BatchRenderer2D::setActiveBatchRenderer(nullptr);
            info.isDirty = false;
            break;
        }
    }
#else
    (void)w;
#endif
}

void FloatingWindowManager::handleMouseButton(DetachedWindowInfo& info, int button, int action, float mx, float my) {
    if (button == 1 /* GLFW_MOUSE_BUTTON_RIGHT */ && action == 1 /* GLFW_PRESS */) {
        auto resetWidget = [&](GuiWidgetDef& w) {
            w.currentVal = w.defaultVal;
            w.valueDisplay = w.getFormattedValue();
            float normVal = (w.maxVal > w.minVal) ? (w.currentVal - w.minVal) / (w.maxVal - w.minVal) : w.currentVal;
            if (onParamChanged) onParamChanged(info.trackIndex, w.param, normVal);
            info.isDirty = true;
        };

        if (info.panelDef.layoutMode == GuiLayoutMode::AbsoluteCanvas) {
            for (auto& w : info.panelDef.widgets) {
                if (w.type == GuiWidgetType::Divider) continue;
                float cx = w.bounds.x + (w.bounds.w * 0.5f);
                float cy = w.bounds.y + (w.bounds.h * 0.44f);
                float dist = std::hypot(mx - cx, my - cy);
                if (w.bounds.contains(mx, my) || dist <= (w.bounds.w * 0.5f)) {
                    resetWidget(w);
                    return;
                }
            }
        } else {
            for (auto& row : info.panelDef.rows) {
                for (auto& w : row.widgets) {
                    if (w.type == GuiWidgetType::Divider) continue;
                    float cx = w.bounds.x + (w.bounds.w * 0.5f);
                    float cy = w.bounds.y + (w.bounds.h * 0.44f);
                    float dist = std::hypot(mx - cx, my - cy);
                    if (w.bounds.contains(mx, my) || dist <= (w.bounds.w * 0.5f)) {
                        resetWidget(w);
                        return;
                    }
                }
            }
        }
        return;
    }

    if (button != 0 /* GLFW_MOUSE_BUTTON_LEFT */) return;

    if (action == 1 /* GLFW_PRESS */) {
        if (info.panelDef.layoutMode == GuiLayoutMode::AbsoluteCanvas) {
            for (size_t i = 0; i < info.panelDef.widgets.size(); ++i) {
                auto& w = info.panelDef.widgets[i];
                if (w.type == GuiWidgetType::Divider) continue;
                float cx = w.bounds.x + (w.bounds.w * 0.5f);
                float cy = w.bounds.y + (w.bounds.h * 0.44f);
                float dist = std::hypot(mx - cx, my - cy);
                if (w.bounds.contains(mx, my) || dist <= (w.bounds.w * 0.5f)) {
                    if (w.type == GuiWidgetType::ToggleSwitch) {
                        w.currentVal = (w.currentVal > 0.5f) ? 0.0f : 1.0f;
                        float normVal = (w.maxVal > w.minVal) ? (w.currentVal - w.minVal) / (w.maxVal - w.minVal) : w.currentVal;
                        if (onParamChanged) onParamChanged(info.trackIndex, w.param, normVal);
                        info.isDirty = true;
                        return;
                    }
                    info.isDragging = true;
                    info.draggingRow = 0;
                    info.draggingWidget = static_cast<int>(i);
                    info.dragStartY = my;
                    if (w.maxVal > w.minVal) {
                        info.dragStartVal = std::clamp((w.currentVal - w.minVal) / (w.maxVal - w.minVal), 0.0f, 1.0f);
                    } else {
                        info.dragStartVal = std::clamp(w.currentVal, 0.0f, 1.0f);
                    }
                    info.isDirty = true;
                    return;
                }
            }
        } else {
            // AutoRowFlow: check panelDef.rows
            for (size_t rIdx = 0; rIdx < info.panelDef.rows.size(); ++rIdx) {
                auto& row = info.panelDef.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    if (w.type == GuiWidgetType::Divider) continue;
                    float cx = w.bounds.x + (w.bounds.w * 0.5f);
                    float cy = w.bounds.y + (w.bounds.h * 0.44f);
                    float dist = std::hypot(mx - cx, my - cy);
                    if (w.bounds.contains(mx, my) || dist <= (w.bounds.w * 0.5f)) {
                        if (w.type == GuiWidgetType::ToggleSwitch) {
                            w.currentVal = (w.currentVal > 0.5f) ? 0.0f : 1.0f;
                            float normVal = (w.maxVal > w.minVal) ? (w.currentVal - w.minVal) / (w.maxVal - w.minVal) : w.currentVal;
                            if (onParamChanged) onParamChanged(info.trackIndex, w.param, normVal);
                            info.isDirty = true;
                            return;
                        }
                        info.isDragging = true;
                        info.draggingRow = static_cast<int>(rIdx);
                        info.draggingWidget = static_cast<int>(wIdx);
                        info.dragStartY = my;
                        if (w.maxVal > w.minVal) {
                            info.dragStartVal = std::clamp((w.currentVal - w.minVal) / (w.maxVal - w.minVal), 0.0f, 1.0f);
                        } else {
                            info.dragStartVal = std::clamp(w.currentVal, 0.0f, 1.0f);
                        }
                        info.isDirty = true;
                        return;
                    }
                }
            }
        }
    } else if (action == 0 /* GLFW_RELEASE */) {
        info.isDragging = false;
        info.draggingRow = -1;
        info.draggingWidget = -1;
        info.isDirty = true;
    }
}

void FloatingWindowManager::handleMouseMove(DetachedWindowInfo& info, float mx, float my) {
    if (!info.isDragging) return;
    (void)mx;

    float divisor = 150.0f;
#if !defined(__EMSCRIPTEN__)
    if (info.window) {
        if (glfwGetKey(info.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(info.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) {
            divisor = 750.0f; // 5x precision fine-tuning mode
        }
    }
#endif

    float deltaY = info.dragStartY - my;
    float normVal = std::clamp(info.dragStartVal + (deltaY / divisor), 0.0f, 1.0f);

    auto updateWidget = [&](GuiWidgetDef& w) {
        if (w.step > 0.0f && w.maxVal > w.minVal) {
            float scaled = w.minVal + normVal * (w.maxVal - w.minVal);
            scaled = w.minVal + std::round((scaled - w.minVal) / w.step) * w.step;
            scaled = std::clamp(scaled, w.minVal, w.maxVal);
            normVal = (scaled - w.minVal) / (w.maxVal - w.minVal);
            w.currentVal = scaled;
        } else if (w.maxVal > w.minVal) {
            w.currentVal = w.minVal + normVal * (w.maxVal - w.minVal);
        } else {
            w.currentVal = normVal;
        }
        w.valueDisplay = w.getFormattedValue();
        if (onParamChanged) onParamChanged(info.trackIndex, w.param, normVal);
        info.isDirty = true;
    };

    if (info.panelDef.layoutMode == GuiLayoutMode::AbsoluteCanvas) {
        if (info.draggingWidget >= 0 && static_cast<size_t>(info.draggingWidget) < info.panelDef.widgets.size()) {
            updateWidget(info.panelDef.widgets[info.draggingWidget]);
        }
    } else {
        if (info.draggingRow >= 0 && static_cast<size_t>(info.draggingRow) < info.panelDef.rows.size()) {
            auto& row = info.panelDef.rows[info.draggingRow];
            if (info.draggingWidget >= 0 && static_cast<size_t>(info.draggingWidget) < row.widgets.size()) {
                updateWidget(row.widgets[info.draggingWidget]);
            }
        }
    }
}

} // namespace eatsbits::ui
