#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

/**
 * Filterable Modal Theme Browser Dialog.
 * Features category filter pills, real-time search filtering, keyboard arrow navigation,
 * swatch disc palettes (chassis, accents, syntax tokens), and real-time live preview.
 */
class ThemeBrowserDialog {
public:
    ThemeBrowserDialog();
    ~ThemeBrowserDialog() = default;

    void open();
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);
    bool handleChar(char32_t codepoint);

    // Search query & category filter
    void setSearchQuery(const std::string& query);
    [[nodiscard]] const std::string& getSearchQuery() const noexcept { return searchQuery_; }
    void clearSearch();
    [[nodiscard]] int getSelectedCategoryIndex() const noexcept { return selectedCategoryIndex_; }
    void setSelectedCategoryIndex(int idx) noexcept;
    [[nodiscard]] int getSelectedItemIndex() const noexcept { return selectedItemIndex_; }
    void setSelectedItemIndex(int idx);

    std::function<void(Theme::Preset preset)> onThemeApplied;
    std::function<void()> onClose;

private:
    struct ThemeCardItem {
        Theme::Preset preset;
        std::string name;
        std::string subtitle;
        std::string category;
        std::string description;
        std::string author;
        ThemeTokens tokens;
        Rect2D bounds{0.0f, 0.0f, 0.0f, 0.0f};
    };

    void refreshFilteredList();
    void applyPreview(Theme::Preset preset);
    void confirmSelection();
    void cancelAndRevert();

    bool isOpen_{false};
    Theme::Preset initialPreset_{Theme::Preset::AteTrack};
    Theme::Preset previewPreset_{Theme::Preset::AteTrack};

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};
    Rect2D dialogBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D searchBarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D cardsAreaBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D applyBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D cancelBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    std::string searchQuery_{""};
    bool searchFocused_{true};
    float cursorBlinkTimer_{0.0f};

    int selectedCategoryIndex_{0}; // 0 = ALL
    int selectedItemIndex_{0};
    float scrollY_{0.0f};
    float maxScrollY_{0.0f};

    std::vector<std::string> categories_{
        "ALL", "DARK", "LIGHT", "SYNTAX PORTS", "MINIMAL / DUOTONE", "HARDWARE"
    };
    std::vector<Rect2D> categoryPillBounds_;

    std::vector<ThemeCardItem> allItems_;
    std::vector<size_t> filteredIndices_;
};

} // namespace eatsbits::ui
