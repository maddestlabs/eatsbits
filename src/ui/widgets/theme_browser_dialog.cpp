#include "eatsbits/ui/widgets/theme_browser_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace eatsbits::ui {

static bool stringContainsCaseInsensitive(const std::string& str, const std::string& sub) {
    if (sub.empty()) return true;
    auto it = std::search(
        str.begin(), str.end(),
        sub.begin(), sub.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return (it != str.end());
}

ThemeBrowserDialog::ThemeBrowserDialog() {
    const auto& catalog = Theme::getPresetCatalog();
    allItems_.clear();
    for (const auto& meta : catalog) {
        ThemeCardItem item;
        item.preset = meta.preset;
        item.name = meta.name;
        item.subtitle = meta.subtitle;
        item.category = meta.category;
        item.description = meta.description;
        item.author = meta.author;
        item.tokens = Theme::get(meta.preset);
        allItems_.push_back(item);
    }
    refreshFilteredList();
}

void ThemeBrowserDialog::open() {
    isOpen_ = true;
    initialPreset_ = Theme::getCurrentPreset();
    previewPreset_ = initialPreset_;
    searchQuery_.clear();
    selectedCategoryIndex_ = 0;
    scrollY_ = 0.0f;

    refreshFilteredList();

    // Select current active preset in filtered list if present
    selectedItemIndex_ = 0;
    for (size_t i = 0; i < filteredIndices_.size(); ++i) {
        if (allItems_[filteredIndices_[i]].preset == initialPreset_) {
            selectedItemIndex_ = static_cast<int>(i);
            break;
        }
    }
}

void ThemeBrowserDialog::refreshFilteredList() {
    filteredIndices_.clear();
    std::string activeCat = (selectedCategoryIndex_ >= 0 && selectedCategoryIndex_ < static_cast<int>(categories_.size()))
        ? categories_[selectedCategoryIndex_]
        : "ALL";

    for (size_t i = 0; i < allItems_.size(); ++i) {
        const auto& item = allItems_[i];

        // 1. Category Filter
        bool matchesCat = false;
        if (activeCat == "ALL") {
            matchesCat = true;
        } else if (activeCat == "DARK") {
            matchesCat = !item.tokens.isLight;
        } else if (activeCat == "LIGHT") {
            matchesCat = item.tokens.isLight;
        } else if (activeCat == "SYNTAX PORTS") {
            matchesCat = (item.category == "Syntax Port");
        } else if (activeCat == "MINIMAL / DUOTONE") {
            matchesCat = (item.category == "Minimal / DuoTone");
        } else if (activeCat == "HARDWARE") {
            matchesCat = (item.category == "Hardware");
        }

        if (!matchesCat) continue;

        // 2. Search Query Filter
        if (!searchQuery_.empty()) {
            bool matchesSearch = stringContainsCaseInsensitive(item.name, searchQuery_) ||
                                 stringContainsCaseInsensitive(item.subtitle, searchQuery_) ||
                                 stringContainsCaseInsensitive(item.category, searchQuery_) ||
                                 stringContainsCaseInsensitive(item.description, searchQuery_) ||
                                 stringContainsCaseInsensitive(item.author, searchQuery_);
            if (!matchesSearch) continue;
        }

        filteredIndices_.push_back(i);
    }

    if (filteredIndices_.empty()) {
        selectedItemIndex_ = -1;
    } else {
        selectedItemIndex_ = std::clamp(selectedItemIndex_, 0, static_cast<int>(filteredIndices_.size() - 1));
    }
}

void ThemeBrowserDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float diagW = std::min(780.0f, screenW - 40.0f);
    float diagH = std::min(580.0f, screenH - 40.0f);
    dialogBounds_ = Rect2D{(screenW - diagW) * 0.5f, (screenH - diagH) * 0.5f, diagW, diagH};

    closeBtnBounds_ = Rect2D{dialogBounds_.x + dialogBounds_.w - 38.0f, dialogBounds_.y + 12.0f, 26.0f, 26.0f};
    searchBarBounds_ = Rect2D{dialogBounds_.x + 20.0f, dialogBounds_.y + 56.0f, dialogBounds_.w - 40.0f, 32.0f};

    // Category pills layout
    categoryPillBounds_.clear();
    float pillX = dialogBounds_.x + 20.0f;
    float pillY = dialogBounds_.y + 98.0f;
    float pillH = 24.0f;
    for (const auto& cat : categories_) {
        float pillW = static_cast<float>(cat.length()) * 7.5f + 20.0f;
        categoryPillBounds_.emplace_back(pillX, pillY, pillW, pillH);
        pillX += pillW + 8.0f;
    }

    // Cards layout (2 columns)
    float cardsTopY = dialogBounds_.y + 132.0f;
    float footerH = 56.0f;
    cardsAreaBounds_ = Rect2D{dialogBounds_.x + 20.0f, cardsTopY, dialogBounds_.w - 40.0f,
                              dialogBounds_.h - (cardsTopY - dialogBounds_.y) - footerH};

    float colGap = 12.0f;
    float rowGap = 10.0f;
    float cardW = (cardsAreaBounds_.w - colGap) * 0.5f;
    float cardH = 86.0f;

    for (size_t rank = 0; rank < filteredIndices_.size(); ++rank) {
        size_t idx = filteredIndices_[rank];
        int col = static_cast<int>(rank % 2);
        int row = static_cast<int>(rank / 2);

        float cx = cardsAreaBounds_.x + static_cast<float>(col) * (cardW + colGap);
        float cy = cardsAreaBounds_.y + static_cast<float>(row) * (cardH + rowGap);
        allItems_[idx].bounds = Rect2D{cx, cy, cardW, cardH};
    }

    size_t rowCount = (filteredIndices_.size() + 1) / 2;
    float totalContentH = static_cast<float>(rowCount) * (cardH + rowGap);
    maxScrollY_ = std::max(0.0f, totalContentH - cardsAreaBounds_.h);
    scrollY_ = std::clamp(scrollY_, 0.0f, maxScrollY_);

    // Footer buttons layout
    float footerY = dialogBounds_.y + dialogBounds_.h - 44.0f;
    cancelBtnBounds_ = Rect2D{dialogBounds_.x + dialogBounds_.w - 240.0f, footerY, 105.0f, 30.0f};
    applyBtnBounds_  = Rect2D{dialogBounds_.x + dialogBounds_.w - 125.0f, footerY, 105.0f, 30.0f};
}

void ThemeBrowserDialog::applyPreview(Theme::Preset preset) {
    previewPreset_ = preset;
    Theme::setPreset(preset);
}

void ThemeBrowserDialog::confirmSelection() {
    if (selectedItemIndex_ >= 0 && selectedItemIndex_ < static_cast<int>(filteredIndices_.size())) {
        Theme::Preset selected = allItems_[filteredIndices_[selectedItemIndex_]].preset;
        Theme::setPreset(selected);
        if (onThemeApplied) {
            onThemeApplied(selected);
        }
    }
    close();
    if (onClose) onClose();
}

void ThemeBrowserDialog::cancelAndRevert() {
    Theme::setPreset(initialPreset_);
    close();
    if (onClose) onClose();
}

void ThemeBrowserDialog::setSearchQuery(const std::string& query) {
    searchQuery_ = query;
    refreshFilteredList();
}

void ThemeBrowserDialog::clearSearch() {
    searchQuery_.clear();
    refreshFilteredList();
}

void ThemeBrowserDialog::setSelectedCategoryIndex(int idx) noexcept {
    if (idx >= 0 && idx < static_cast<int>(categories_.size())) {
        selectedCategoryIndex_ = idx;
        refreshFilteredList();
    }
}

void ThemeBrowserDialog::setSelectedItemIndex(int idx) {
    if (idx >= 0 && idx < static_cast<int>(filteredIndices_.size())) {
        selectedItemIndex_ = idx;
        applyPreview(allItems_[filteredIndices_[idx]].preset);
    }
}

void ThemeBrowserDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop
    r.drawRect(0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.70f);

    // 2. Dialog Modal Window
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f, theme.borderFocus, 1.5f);

    // Header bar
    float headerH = 46.0f;
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, headerH, 8.0f, theme.panelHeader);
    drawLine(r, dialogBounds_.x, dialogBounds_.y + headerH,
             dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + headerH, theme.borderSubtle, 1.0f);

    drawText(r, "THEME REGISTRY & PALETTE BROWSER", dialogBounds_.x + 20.0f, dialogBounds_.y + 14.0f, 13.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, "Curated hardware consoles and authentic syntax heritage themes",
             dialogBounds_.x + 20.0f, dialogBounds_.y + 30.0f, 9.5f, theme.textMuted);

    // Close [X] Button
    drawRoundedRect(r, closeBtnBounds_.x, closeBtnBounds_.y, closeBtnBounds_.w, closeBtnBounds_.h, 4.0f, theme.controlBackground);
    drawRoundedRectOutline(r, closeBtnBounds_.x, closeBtnBounds_.y, closeBtnBounds_.w, closeBtnBounds_.h, 4.0f, theme.borderSubtle, 1.0f);
    drawCenteredText(r, "X", closeBtnBounds_, 10.0f, theme.textSecondary);

    // 3. Search Bar
    drawRoundedRect(r, searchBarBounds_.x, searchBarBounds_.y, searchBarBounds_.w, searchBarBounds_.h, 5.0f, theme.controlWell);
    drawRoundedRectOutline(r, searchBarBounds_.x, searchBarBounds_.y, searchBarBounds_.w, searchBarBounds_.h, 5.0f,
                           searchFocused_ ? theme.primaryAccent : theme.borderSubtle, 1.2f);

    if (searchQuery_.empty()) {
        drawText(r, "Search themes by name, heritage, keywords (e.g. 'solar', 'nord', 'dracula', 'duotone')...",
                 searchBarBounds_.x + 12.0f, searchBarBounds_.y + 10.0f, 10.0f, theme.textMuted);
    } else {
        drawText(r, searchQuery_, searchBarBounds_.x + 12.0f, searchBarBounds_.y + 10.0f, 11.0f, theme.textPrimary);
    }

    // 4. Category Filter Pills
    for (size_t c = 0; c < categories_.size() && c < categoryPillBounds_.size(); ++c) {
        const auto& pb = categoryPillBounds_[c];
        bool isAct = (selectedCategoryIndex_ == static_cast<int>(c));
        drawRoundedRect(r, pb.x, pb.y, pb.w, pb.h, 12.0f, isAct ? theme.primaryAccent.darken(0.35f) : theme.controlBackground);
        drawRoundedRectOutline(r, pb.x, pb.y, pb.w, pb.h, 12.0f, isAct ? theme.primaryAccent : theme.borderSubtle, isAct ? 1.5f : 1.0f);
        drawCenteredText(r, categories_[c], pb, 9.0f, isAct ? Color{1.0f, 1.0f, 1.0f} : theme.textSecondary);
    }

    // 5. Cards Area (Scissored)
    r.pushScissor(cardsAreaBounds_.x, cardsAreaBounds_.y, cardsAreaBounds_.w, cardsAreaBounds_.h);

    for (size_t rank = 0; rank < filteredIndices_.size(); ++rank) {
        size_t idx = filteredIndices_[rank];
        const auto& item = allItems_[idx];
        Rect2D cb = item.bounds;
        cb.y -= scrollY_;

        // Culling
        if (cb.y + cb.h < cardsAreaBounds_.y || cb.y > cardsAreaBounds_.y + cardsAreaBounds_.h) {
            continue;
        }

        bool isSel = (selectedItemIndex_ == static_cast<int>(rank));
        bool isCurrent = (Theme::getCurrentPreset() == item.preset);

        Color cardBg = isSel ? theme.controlWell.lighten(0.04f) : theme.controlBackground;
        drawRoundedRect(r, cb.x, cb.y, cb.w, cb.h, 6.0f, cardBg);

        Color borderCol = isCurrent ? theme.primaryAccent : (isSel ? theme.borderFocus : theme.borderSubtle);
        float borderW = (isCurrent || isSel) ? 1.5f : 1.0f;
        drawRoundedRectOutline(r, cb.x, cb.y, cb.w, cb.h, 6.0f, borderCol, borderW);

        // Header: Name & Category Badge
        drawText(r, item.name, cb.x + 12.0f, cb.y + 10.0f, 12.0f, isSel ? theme.primaryAccent : theme.textPrimary);
        drawText(r, item.subtitle, cb.x + 12.0f, cb.y + 26.0f, 9.0f, theme.textSecondary);

        // Active indicator badge
        if (isCurrent) {
            float badgeW = 60.0f;
            float badgeX = cb.x + cb.w - badgeW - 10.0f;
            drawRoundedRect(r, badgeX, cb.y + 8.0f, badgeW, 16.0f, 3.0f, theme.primaryAccent.darken(0.3f));
            drawCenteredText(r, "ACTIVE", Rect2D{badgeX, cb.y + 8.0f, badgeW, 16.0f}, 8.0f, Color{1.0f, 1.0f, 1.0f});
        }

        // Description
        drawText(r, item.description, cb.x + 12.0f, cb.y + 42.0f, 8.5f, theme.textMuted);

        // Swatch Palette (6 circular discs)
        float swatchY = cb.y + cb.h - 18.0f;
        float discRadius = 6.0f;
        const Color swatches[6] = {
            item.tokens.backgroundDark,
            item.tokens.primaryAccent,
            item.tokens.secondaryAccent,
            item.tokens.syntaxKeyword,
            item.tokens.syntaxString,
            item.tokens.syntaxNumber
        };

        for (int s = 0; s < 6; ++s) {
            float sx = cb.x + 18.0f + static_cast<float>(s) * 18.0f;
            drawCircle(r, sx, swatchY, discRadius, swatches[s].r, swatches[s].g, swatches[s].b, 1.0f);
            drawCircleOutline(r, sx, swatchY, discRadius, 0.4f, 0.4f, 0.4f, 0.8f, 0.8f);
        }

        // Author tag
        drawText(r, item.author, cb.x + 140.0f, swatchY - 5.0f, 8.0f, theme.textMuted);
    }

    r.popScissor();

    // Scrollbar indicator
    if (maxScrollY_ > 0.0f) {
        float trackX = cardsAreaBounds_.x + cardsAreaBounds_.w - 4.0f;
        float trackY = cardsAreaBounds_.y;
        float trackH = cardsAreaBounds_.h;
        drawRect(r, trackX, trackY, 4.0f, trackH, 0.1f, 0.1f, 0.1f, 0.4f);

        float thumbNorm = scrollY_ / maxScrollY_;
        float thumbH = std::max(20.0f, trackH * (cardsAreaBounds_.h / (cardsAreaBounds_.h + maxScrollY_)));
        float thumbY = trackY + thumbNorm * (trackH - thumbH);
        drawRoundedRect(r, trackX, thumbY, 4.0f, thumbH, 2.0f, theme.primaryAccent);
    }

    // 6. Footer Controls
    drawLine(r, dialogBounds_.x, dialogBounds_.y + dialogBounds_.h - 52.0f,
             dialogBounds_.x + dialogBounds_.w, dialogBounds_.y + dialogBounds_.h - 52.0f, theme.borderSubtle, 1.0f);

    drawText(r, "LIVE PREVIEW: Changes apply in real-time. Press Enter to lock in or Esc to cancel.",
             dialogBounds_.x + 20.0f, dialogBounds_.y + dialogBounds_.h - 32.0f, 9.0f, theme.textSecondary);

    drawButton(r, cancelBtnBounds_, "CANCEL (ESC)", theme.controlBackground, theme.borderSubtle, theme.textSecondary, 9.5f, 4.0f, 1.0f);
    drawButton(r, applyBtnBounds_, "APPLY THEME", theme.primaryAccent.darken(0.35f), theme.primaryAccent, Color{1.0f, 1.0f, 1.0f}, 9.5f, 4.0f, 1.2f);
}

bool ThemeBrowserDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    // Check outside dialog click
    if (!dialogBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        cancelAndRevert();
        return true;
    }

    // Close button click
    if (closeBtnBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        cancelAndRevert();
        return true;
    }

    // Cancel button click
    if (cancelBtnBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        cancelAndRevert();
        return true;
    }

    // Apply button click
    if (applyBtnBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        confirmSelection();
        return true;
    }

    // Category pills click
    if (ev.action == PointerAction::Down) {
        for (size_t c = 0; c < categoryPillBounds_.size(); ++c) {
            if (categoryPillBounds_[c].contains(ev.x, ev.y)) {
                setSelectedCategoryIndex(static_cast<int>(c));
                return true;
            }
        }
    }

    // Search bar focus
    if (ev.action == PointerAction::Down && searchBarBounds_.contains(ev.x, ev.y)) {
        searchFocused_ = true;
        return true;
    }

    // Scroll wheel over cards area
    if (cardsAreaBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Scroll) {
        scrollY_ = std::clamp(scrollY_ - ev.scrollY * 40.0f, 0.0f, maxScrollY_);
        return true;
    }

    // Card click
    if (cardsAreaBounds_.contains(ev.x, ev.y) && ev.action == PointerAction::Down) {
        for (size_t rank = 0; rank < filteredIndices_.size(); ++rank) {
            size_t idx = filteredIndices_[rank];
            Rect2D cb = allItems_[idx].bounds;
            cb.y -= scrollY_;
            if (cb.contains(ev.x, ev.y)) {
                setSelectedItemIndex(static_cast<int>(rank));
                return true;
            }
        }
    }

    return true; // Eat event while modal is open
}

bool ThemeBrowserDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, [[maybe_unused]] int mods) {
    if (!isOpen_) return false;
    if (action != 1 && action != 2) return true; // Down or repeat only

    // Escape -> cancel and revert
    if (key == 256) { // GLFW_KEY_ESCAPE
        cancelAndRevert();
        return true;
    }

    // Enter -> apply and confirm
    if (key == 257) { // GLFW_KEY_ENTER
        confirmSelection();
        return true;
    }

    // Backspace for search query
    if (key == 259) { // GLFW_KEY_BACKSPACE
        if (!searchQuery_.empty()) {
            searchQuery_.pop_back();
            refreshFilteredList();
        }
        return true;
    }

    // Up / Down / Left / Right arrow navigation
    if (key == 265) { // GLFW_KEY_UP
        if (selectedItemIndex_ >= 2) {
            setSelectedItemIndex(selectedItemIndex_ - 2);
        }
        return true;
    }
    if (key == 264) { // GLFW_KEY_DOWN
        if (selectedItemIndex_ + 2 < static_cast<int>(filteredIndices_.size())) {
            setSelectedItemIndex(selectedItemIndex_ + 2);
        }
        return true;
    }
    if (key == 263) { // GLFW_KEY_LEFT
        if (selectedItemIndex_ > 0) {
            setSelectedItemIndex(selectedItemIndex_ - 1);
        }
        return true;
    }
    if (key == 262) { // GLFW_KEY_RIGHT
        if (selectedItemIndex_ + 1 < static_cast<int>(filteredIndices_.size())) {
            setSelectedItemIndex(selectedItemIndex_ + 1);
        }
        return true;
    }

    return true;
}

bool ThemeBrowserDialog::handleChar(char32_t codepoint) {
    if (!isOpen_) return false;
    if (codepoint >= 32 && codepoint < 127) {
        searchQuery_ += static_cast<char>(codepoint);
        refreshFilteredList();
        return true;
    }
    return false;
}

} // namespace eatsbits::ui
