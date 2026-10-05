#pragma once

#include "eatsbits/presenter/text_presenter.hpp"
#include "eatsbits/ui/views/view_base.hpp"
#include "eatsbits/ui/input/focus_manager.hpp"
#include <string>
#include <string_view>
#include <functional>
#include <chrono>

namespace eatsbits::ui {

/**
 * @brief High-performance multi-line code/script editor widget.
 * Features:
 * - Backed by headless MultiLineTextPresenter and TextDocument (undo/redo, 2D coords).
 * - Code Minimap: Replaces thin vertical scrollbars with a wide, touch-friendly
 *   code silhouette minimap with micro-bar syntax rendering and viewport lens scrubber.
 * - Line Number Gutter with active line highlighting.
 * - IFocusable implementation with full keyboard navigation, clipboard (Ctrl+C/V/X),
 *   multi-line indentation (Tab / Shift+Tab), comment toggling (Ctrl+/), and line duplication (Ctrl+D).
 * - Scissored rendering containing text within the editor bounds.
 */
class TextEditorWidget : public IFocusable {
public:
    TextEditorWidget();
    ~TextEditorWidget() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx);
    void render(const ViewContext& ctx);
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx);
    bool handleKey(int key, int scancode, int action, int mods) override;
    bool handleChar(char32_t codepoint) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx);
    bool handleChar(char32_t codepoint, const ViewContext& ctx);

    // IFocusable implementation
    void resetCursorBlink() noexcept {
        cursorBlinkResetTime_ = std::chrono::steady_clock::now();
    }

    bool onFocusGained() override;
    void onFocusLost() override;
    [[nodiscard]] bool isFocused() const noexcept override { return isFocused_; }

    void setClipboard(IClipboard* clipboard) noexcept { clipboard_ = clipboard; }

    void setText(std::string_view text);
    [[nodiscard]] std::string getText() const;
    [[nodiscard]] presenter::MultiLineTextPresenter& getPresenter() noexcept { return presenter_; }
    [[nodiscard]] const presenter::MultiLineTextPresenter& getPresenter() const noexcept { return presenter_; }

    void setReadOnly(bool readOnly) noexcept { readOnly_ = readOnly; }
    [[nodiscard]] bool isReadOnly() const noexcept { return readOnly_; }

    void setMinimapWidth(float w) noexcept { minimapW_ = std::max(0.0f, w); }
    [[nodiscard]] float getMinimapWidth() const noexcept { return minimapW_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }
    [[nodiscard]] const Rect2D& getMinimapBounds() const noexcept { return minimapBounds_; }
    [[nodiscard]] const Rect2D& getTextAreaBounds() const noexcept { return textAreaBounds_; }
    [[nodiscard]] const Rect2D& getGutterBounds() const noexcept { return gutterBounds_; }
    [[nodiscard]] const Rect2D& getAccessoryToolbarBounds() const noexcept { return toolbarBounds_; }

    void setAccessoryToolbarVisible(bool visible) noexcept {
        showToolbar_ = visible;
        autoToolbar_ = false;
    }
    [[nodiscard]] bool isAccessoryToolbarVisible() const noexcept { return showToolbar_; }
    void setAccessoryToolbarAuto(bool enable) noexcept { autoToolbar_ = enable; }
    [[nodiscard]] bool isAccessoryToolbarAuto() const noexcept { return autoToolbar_; }

    [[nodiscard]] float getCharWidth() const noexcept { return charWidth_; }
    [[nodiscard]] float getLineHeight() const noexcept { return lineHeight_; }

    [[nodiscard]] core::TextCoord coordFromPoint(float px, float py) const;

    // Callbacks
    std::function<void(const std::string& text)> onTextChanged;
    std::function<void()> onCompileTriggered; // Ctrl+Enter or F5

private:
    void renderMinimap(const ViewContext& ctx);
    void renderGutterAndText(const ViewContext& ctx);
    void renderAccessoryToolbar(const ViewContext& ctx);
    bool handleToolbarPointer(const PointerEvent& ev, const ViewContext& ctx);

    presenter::MultiLineTextPresenter presenter_;
    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D gutterBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D textAreaBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D minimapBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D toolbarBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    float gutterW_{46.0f};
    float minimapW_{56.0f};
    float lineHeight_{18.0f};
    float charWidth_{7.01f}; // Synchronized with getMonoCharAdvance(10.0f)

    bool isFocused_{false};
    bool readOnly_{false};
    bool isDraggingText_{false};
    bool isDraggingMinimap_{false};
    bool showToolbar_{false};
    bool autoToolbar_{true};
    float toolbarScrollX_{0.0f};
    float lastToolbarDragX_{0.0f};
    int pressedToolbarBtn_{-1};

    std::chrono::steady_clock::time_point lastClickTime_{};
    Point2D lastClickPos_{0.0f, 0.0f};
    std::chrono::steady_clock::time_point focusGainTime_{};
    std::chrono::steady_clock::time_point cursorBlinkResetTime_{};
    core::TextCoord lastCursor_{0, 0};
    IClipboard* clipboard_{nullptr};
};

} // namespace eatsbits::ui
