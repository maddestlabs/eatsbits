#pragma once

#include <cstdint>
#include <functional>

namespace eatsbits::ui {

/**
 * @brief Interface for elements that can receive keyboard focus.
 */
class IFocusable {
public:
    virtual ~IFocusable() = default;

    virtual bool onFocusGained() { return true; }
    virtual void onFocusLost() {}
    virtual bool handleKey(int key, int scancode, int action, int mods) = 0;
    virtual bool handleChar(char32_t codepoint) = 0;
    [[nodiscard]] virtual bool isFocused() const noexcept = 0;
};

/**
 * @brief Central focus manager that ensures keyboard and text input events
 * are routed strictly to the currently focused UI element.
 */
class FocusManager {
public:
    FocusManager() = default;

    void requestFocus(IFocusable* element) {
        if (focused_ == element) return;

        IFocusable* old = focused_;
        if (old) {
            old->onFocusLost();
        }

        focused_ = element;
        if (focused_) {
            if (!focused_->onFocusGained()) {
                focused_ = nullptr;
            }
        }

        if (onFocusChanged) {
            onFocusChanged(old, focused_);
        }
    }

    void clearFocus() {
        requestFocus(nullptr);
    }

    [[nodiscard]] IFocusable* getFocusedElement() const noexcept {
        return focused_;
    }

    [[nodiscard]] bool hasFocus(const IFocusable* element) const noexcept {
        return focused_ != nullptr && focused_ == element;
    }

    [[nodiscard]] bool isAnyFocused() const noexcept {
        return focused_ != nullptr;
    }

    bool dispatchKey(int key, int scancode, int action, int mods) {
        if (focused_) {
            return focused_->handleKey(key, scancode, action, mods);
        }
        return false;
    }

    bool dispatchChar(char32_t codepoint) {
        if (focused_) {
            return focused_->handleChar(codepoint);
        }
        return false;
    }

    std::function<void(IFocusable* oldFocus, IFocusable* newFocus)> onFocusChanged{nullptr};

private:
    IFocusable* focused_{nullptr};
};

} // namespace eatsbits::ui
