#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "../icon_registry.hpp"
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace eatsbits::ui {

/**
 * Reusable modal dialog for searching, selecting, and pasting track icons.
 * Features:
 * - Category filter pills (All, Instruments, Drums, FX, Hardware, General, Custom).
 * - Real-time keyword search.
 * - Multi-column responsive icon card grid with live vector previews.
 * - Clipboard SVG paste with live preview and validation.
 */
class IconSearchDialog {
public:
    IconSearchDialog();
    ~IconSearchDialog() = default;

    void open(const std::string& targetTrackName = "", uint32_t targetTrackIndex = 0,
              const std::string& currentIconRef = "");
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);
    bool handleScroll(float deltaY);

    void setClipboardProvider(std::function<std::string()> provider) {
        clipboardProvider_ = std::move(provider);
    }

    std::function<void(const std::string& iconRef, uint32_t trackIndex)> onIconSelected;
    std::function<void()> onClose;

private:
    void pasteFromClipboard();
    [[nodiscard]] std::vector<const IconDef*> getFilteredIcons() const;

    bool isOpen_{false};
    std::string targetTrackName_{""};
    uint32_t targetTrackIndex_{0};
    std::string currentIconRef_{""};

    std::string searchQuery_{""};
    int selectedCategoryIndex_{0}; // 0 = "All"
    std::vector<std::string> categories_;

    float screenWidth_{1280.0f};
    float screenHeight_{720.0f};
    Rect2D dialogBounds_{0, 0, 0, 0};
    Rect2D closeBtnBounds_{0, 0, 0, 0};
    Rect2D searchBoxBounds_{0, 0, 0, 0};
    Rect2D pasteBtnBounds_{0, 0, 0, 0};
    Rect2D gridBounds_{0, 0, 0, 0};

    float scrollY_{0.0f};
    float maxScrollY_{0.0f};

    // Custom SVG paste state
    std::optional<IconDef> customPastedDef_{std::nullopt};
    std::string pasteStatusMsg_{""};
    bool pasteHasError_{false};

    std::function<std::string()> clipboardProvider_;
};

} // namespace eatsbits::ui
