#include "eatsbits/ui/widgets/icon_search_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace eatsbits::ui {

IconSearchDialog::IconSearchDialog() {
    categories_ = {"ALL", "Instruments", "Drums", "FX & Mod", "Hardware", "General", "CUSTOM / SVG"};
}

void IconSearchDialog::open(const std::string& targetTrackName, uint32_t targetTrackIndex,
                            const std::string& currentIconRef) {
    targetTrackName_ = targetTrackName;
    targetTrackIndex_ = targetTrackIndex;
    currentIconRef_ = currentIconRef;
    isOpen_ = true;
    searchQuery_.clear();
    selectedCategoryIndex_ = 0; // Default to "ALL"
    scrollY_ = 0.0f;
    pasteStatusMsg_.clear();
    pasteHasError_ = false;
    customPastedDef_.reset();
    searchFocused_ = true;
}

void IconSearchDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float dw = std::min(screenW * 0.88f, 660.0f);
    float dh = std::min(screenH * 0.84f, 540.0f);
    float dx = (screenW - dw) * 0.5f;
    float dy = (screenH - dh) * 0.5f;

    dialogBounds_ = Rect2D(dx, dy, dw, dh);
    closeBtnBounds_ = Rect2D(dx + dw - 36.0f, dy + 12.0f, 24.0f, 24.0f);

    // Category Filter Chips
    categoryChipBounds_.resize(categories_.size());
    float chipX = dx + 20.0f;
    float chipY = dy + 52.0f;
    for (size_t i = 0; i < categories_.size(); ++i) {
        float chipW = static_cast<float>(categories_[i].length()) * 7.2f + 18.0f;
        categoryChipBounds_[i] = Rect2D(chipX, chipY, chipW, 24.0f);
        chipX += chipW + 6.0f;
    }

    // Search Box
    searchBoxBounds_ = Rect2D(dx + 20.0f, dy + 84.0f, dw - 40.0f, 32.0f);
    searchClearBtnBounds_ = Rect2D(searchBoxBounds_.x + searchBoxBounds_.w - 28.0f, searchBoxBounds_.y + 4.0f, 24.0f, 24.0f);

    // Grid bounds
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 10.0f;
    float bottomBarH = 46.0f;
    float listH = dy + dh - listY - bottomBarH - 8.0f;
    gridBounds_ = Rect2D(dx + 20.0f, listY, dw - 40.0f, listH);

    // Custom tab apply button
    float cw = gridBounds_.w - 40.0f;
    float applyW = 160.0f;
    float applyH = 34.0f;
    float applyX = gridBounds_.x + 20.0f + (cw - applyW) * 0.5f;
    float applyY = gridBounds_.y + 20.0f + 30.0f + 72.0f + 52.0f;
    applyBtnBounds_ = Rect2D(applyX, applyY, applyW, applyH);

    // Paste button in bottom bar
    pasteBtnBounds_ = Rect2D(dx + 20.0f, dy + dh - 42.0f, dw - 40.0f, 30.0f);
}

void IconSearchDialog::clearSearch() noexcept {
    searchQuery_.clear();
    scrollY_ = 0.0f;
}

std::vector<const IconDef*> IconSearchDialog::getFilteredIcons() const {
    std::string activeCat = (selectedCategoryIndex_ >= 0 && selectedCategoryIndex_ < static_cast<int>(categories_.size()))
                                ? categories_[selectedCategoryIndex_]
                                : "ALL";

    if (activeCat == "CUSTOM / SVG") {
        return {};
    }

    return IconRegistry::instance().query(searchQuery_, activeCat);
}

void IconSearchDialog::pasteFromClipboard() {
    if (!clipboardProvider_) {
        pasteStatusMsg_ = "Clipboard access not available";
        pasteHasError_ = true;
        return;
    }

    std::string text = clipboardProvider_();
    if (text.empty()) {
        pasteStatusMsg_ = "Clipboard is empty";
        pasteHasError_ = true;
        return;
    }

    auto parsed = IconRegistry::parsePastedSvg(text, "custom_pasted", "Custom SVG");
    if (parsed.has_value()) {
        customPastedDef_ = std::move(parsed);
        pasteStatusMsg_ = "Valid SVG path detected!";
        pasteHasError_ = false;
        // Automatically switch to Custom tab to view preview
        selectedCategoryIndex_ = static_cast<int>(categories_.size()) - 1;
    } else {
        pasteStatusMsg_ = "Clipboard does not contain a valid SVG path";
        pasteHasError_ = true;
    }
}

void IconSearchDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop scrim (absorbs clicks)
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.70f);

    // 2. Main Dialog Frame (Chunky weathered CRT metal plate)
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                    theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                           theme.borderFocus, 1.5f);

    // Dialog Header Banner
    drawRoundedRect(r, dialogBounds_.x + 2.0f, dialogBounds_.y + 2.0f, dialogBounds_.w - 4.0f, 44.0f, 6.0f,
                    theme.panelHeader);
    drawLine(r, dialogBounds_.x + 2.0f, dialogBounds_.y + 46.0f,
             dialogBounds_.x + dialogBounds_.w - 2.0f, dialogBounds_.y + 46.0f,
             theme.borderSubtle, 1.0f);

    // Header Title & Track Name
    std::string headerTitle = "SELECT TRACK ICON" + (targetTrackName_.empty() ? "" : (" — " + targetTrackName_));
    drawText(r, headerTitle, dialogBounds_.x + 20.0f, dialogBounds_.y + 16.0f, 13.0f,
             theme.primaryAccent);

    // Close button (metallic screw icon) with hover highlight
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 16.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);

    // 3. Category Filter Chips
    for (size_t i = 0; i < categories_.size() && i < categoryChipBounds_.size(); ++i) {
        bool isSelected = (selectedCategoryIndex_ == static_cast<int>(i));
        const auto& chipRect = categoryChipBounds_[i];
        bool chipHov = chipRect.contains(lastMouseX_, lastMouseY_);

        Color chipBg = isSelected
                           ? theme.primaryAccent.withAlpha(0.35f)
                           : (chipHov ? theme.controlBackground.lighten(0.08f) : theme.controlBackground);
        Color chipBorder = isSelected
                               ? theme.primaryAccent
                               : (chipHov ? theme.borderFocus : theme.borderSubtle);
        Color chipText = isSelected ? theme.primaryAccent : (chipHov ? theme.textPrimary : theme.textMuted);

        drawRoundedRect(r, chipRect.x, chipRect.y, chipRect.w, chipRect.h, 4.0f, chipBg);
        drawRoundedRectOutline(r, chipRect.x, chipRect.y, chipRect.w, chipRect.h, 4.0f, chipBorder, 1.0f);
        drawCenteredText(r, categories_[i], chipRect, 9.5f, chipText);
    }

    // 4. Search Bar
    drawRoundedRect(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                    theme.controlWell);
    drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                           searchFocused_ ? theme.primaryAccent : theme.borderSubtle, searchFocused_ ? 1.4f : 1.0f);

    std::string searchDisplay = searchQuery_.empty() ? "Search icons (e.g. synth, kick, bass, mod, fx, guitar)..." : searchQuery_;
    Color searchCol = searchQuery_.empty() ? theme.textMuted : theme.textPrimary;
    drawText(r, searchDisplay, searchBoxBounds_.x + 12.0f, searchBoxBounds_.y + 9.0f, 11.0f, searchCol);

    // Clear Search [X] Button
    if (!searchQuery_.empty()) {
        bool clearHov = searchClearBtnBounds_.contains(lastMouseX_, lastMouseY_);
        drawRoundedRect(r, searchClearBtnBounds_.x, searchClearBtnBounds_.y,
                        searchClearBtnBounds_.w, searchClearBtnBounds_.h, 3.0f,
                        clearHov ? theme.primaryAccent.withAlpha(0.25f) : theme.controlBackground);
        drawCenteredText(r, "x", searchClearBtnBounds_, 10.0f, clearHov ? theme.primaryAccent : theme.textMuted);
    }

    // 5. Content Area
    bool isCustomTab = (selectedCategoryIndex_ == static_cast<int>(categories_.size()) - 1);

    if (isCustomTab) {
        // Custom SVG Preview and Apply Area
        float cx = gridBounds_.x + 20.0f;
        float cy = gridBounds_.y + 20.0f;
        float cw = gridBounds_.w - 40.0f;
        float ch = gridBounds_.h - 40.0f;

        drawRoundedRect(r, cx, cy, cw, ch, 6.0f, theme.controlBackground);
        drawRoundedRectOutline(r, cx, cy, cw, ch, 6.0f, theme.borderSubtle, 1.0f);

        if (customPastedDef_.has_value()) {
            // Render large preview
            float previewSize = 72.0f;
            float px = cx + (cw - previewSize) * 0.5f;
            float py = cy + 30.0f;

            drawRoundedRect(r, px - 8.0f, py - 8.0f, previewSize + 16.0f, previewSize + 16.0f, 8.0f,
                            theme.backgroundDark);
            drawRoundedRectOutline(r, px - 8.0f, py - 8.0f, previewSize + 16.0f, previewSize + 16.0f, 8.0f,
                                   theme.primaryAccent, 1.5f);

            IconRegistry::instance().renderIcon(r, "svg:" + customPastedDef_->svgPath,
                                                px, py, previewSize, theme.primaryAccent);

            drawCenteredText(r, "CUSTOM SVG PREVIEW", Rect2D{cx, py + previewSize + 16.0f, cw, 20.0f},
                             11.0f, theme.textPrimary);

            // Apply Button
            bool applyHov = applyBtnBounds_.contains(lastMouseX_, lastMouseY_);
            Color btnBg = applyHov ? theme.primaryAccent.withAlpha(0.45f) : theme.primaryAccent.withAlpha(0.25f);
            drawButton(r, applyBtnBounds_, "APPLY TO TRACK",
                       btnBg, theme.primaryAccent, theme.primaryAccent, 11.0f, 4.0f, 1.5f);
        } else {
            drawCenteredText(r, "No custom SVG loaded.", Rect2D{cx, cy + ch * 0.35f, cw, 24.0f},
                             12.0f, theme.textMuted);
            drawCenteredText(r, "Click 'PASTE SVG FROM CLIPBOARD' below or press Ctrl+V to import.",
                             Rect2D{cx, cy + ch * 0.45f, cw, 20.0f}, 10.0f, theme.textMuted);
        }
    } else {
        // Multi-column Card Grid with GPU scissoring containment
        ScissorScope scissor(r, gridBounds_);
        auto icons = getFilteredIcons();
        float cardW = 86.0f;
        float cardH = 74.0f;
        float gapX = 10.0f;
        float gapY = 10.0f;
        int cols = std::max(1, static_cast<int>((gridBounds_.w + gapX) / (cardW + gapX)));

        float totalRows = std::ceil(static_cast<float>(icons.size()) / static_cast<float>(cols));
        maxScrollY_ = std::max(0.0f, totalRows * (cardH + gapY) - gridBounds_.h);

        for (size_t i = 0; i < icons.size(); ++i) {
            int col = static_cast<int>(i % cols);
            int row = static_cast<int>(i / cols);

            float cardX = gridBounds_.x + static_cast<float>(col) * (cardW + gapX);
            float cardY = gridBounds_.y + static_cast<float>(row) * (cardH + gapY) - scrollY_;

            if (cardY + cardH < gridBounds_.y || cardY > gridBounds_.y + gridBounds_.h) {
                continue; // Culling
            }

            const auto* def = icons[i];
            bool isCurrent = (!currentIconRef_.empty() &&
                              (currentIconRef_ == ("preset:" + def->id) || currentIconRef_ == def->id));

            Rect2D cardRect(cardX, cardY, cardW, cardH);
            bool isHovered = cardRect.contains(lastMouseX_, lastMouseY_);

            Color cardBg = isCurrent
                               ? theme.primaryAccent.withAlpha(0.25f)
                                : (isHovered
                                       ? theme.controlBackground.lighten(0.08f)
                                       : theme.controlBackground);
            Color cardBorder = isCurrent
                                   ? theme.primaryAccent
                                   : (isHovered ? theme.primaryAccent.withAlpha(0.70f) : theme.borderSubtle);

            drawRoundedRect(r, cardX, cardY, cardW, cardH, 5.0f, cardBg);
            drawRoundedRectOutline(r, cardX, cardY, cardW, cardH, 5.0f, cardBorder, isHovered || isCurrent ? 1.5f : 1.0f);

            // Icon glyph (30x30)
            float iconSize = 30.0f;
            float iconX = cardX + (cardW - iconSize) * 0.5f;
            float iconY = cardY + 8.0f;
            Color iconColor = isCurrent ? theme.primaryAccent : (isHovered ? theme.primaryAccent.lighten(0.15f) : theme.textPrimary);
            IconRegistry::instance().renderIcon(r, "preset:" + def->id, iconX, iconY, iconSize, iconColor);

            // Label
            drawCenteredText(r, def->displayName, Rect2D{cardX + 2.0f, cardY + cardH - 22.0f, cardW - 4.0f, 16.0f},
                             8.5f, isCurrent ? theme.primaryAccent : (isHovered ? theme.textPrimary : theme.textMuted));
        }

        if (icons.empty()) {
            drawCenteredText(r, "No matching icons found.", gridBounds_, 12.0f, theme.textMuted);
        }
    }

    // 5b. Scrollbar Indicator
    if (maxScrollY_ > 0.0f) {
        float trackX = gridBounds_.x + gridBounds_.w - 4.0f;
        float trackY = gridBounds_.y;
        float trackH = gridBounds_.h;
        drawRect(r, trackX, trackY, 4.0f, trackH, 0.1f, 0.1f, 0.1f, 0.4f);

        float thumbNorm = (maxScrollY_ > 0.0f) ? (scrollY_ / maxScrollY_) : 0.0f;
        float thumbH = std::max(20.0f, trackH * (gridBounds_.h / (gridBounds_.h + maxScrollY_)));
        float thumbY = trackY + thumbNorm * (trackH - thumbH);
        drawRoundedRect(r, trackX, thumbY, 4.0f, thumbH, 2.0f, theme.primaryAccent);
    }

    // 6. Bottom Bar: Paste Button and Status Message
    bool pasteHov = pasteBtnBounds_.contains(lastMouseX_, lastMouseY_);
    Color pasteBg = pasteHov ? theme.controlBackground.lighten(0.08f) : theme.controlBackground;
    Color pasteBorder = pasteHov ? theme.primaryAccent : theme.borderFocus;
    drawButton(r, pasteBtnBounds_, "📋 PASTE SVG FROM CLIPBOARD (Ctrl+V)", pasteBg, pasteBorder, theme.primaryAccent, 10.0f, 4.0f);

    if (!pasteStatusMsg_.empty()) {
        Color statusCol = pasteHasError_ ? Color{1.0f, 0.3f, 0.3f, 1.0f} : Color{0.3f, 1.0f, 0.4f, 1.0f};
        drawText(r, pasteStatusMsg_, dialogBounds_.x + 24.0f, dialogBounds_.y + dialogBounds_.h - 10.0f, 9.0f, statusCol);
    }
}

bool IconSearchDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    if (ev.action == PointerAction::Scroll) {
        return handleScroll(ev.scrollY);
    }

    // Scrollbar dragging on move
    if (ev.action == PointerAction::Move && isDraggingScrollbar_) {
        float trackH = gridBounds_.h;
        float thumbH = std::max(20.0f, trackH * (gridBounds_.h / (gridBounds_.h + maxScrollY_)));
        float travel = trackH - thumbH;
        if (travel > 1.0f) {
            float dy = ev.y - dragStartY_;
            scrollY_ = std::clamp(dragStartScrollY_ + (dy / travel) * maxScrollY_, 0.0f, maxScrollY_);
        }
        return true;
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        if (isDraggingScrollbar_) {
            isDraggingScrollbar_ = false;
            return true;
        }
        return true; // Absorb release events over modal
    }

    // Scrollbar click / start drag on down
    if (ev.action == PointerAction::Down && maxScrollY_ > 0.0f) {
        float trackX = gridBounds_.x + gridBounds_.w - 14.0f;
        if (ev.x >= trackX && ev.x <= gridBounds_.x + gridBounds_.w + 6.0f &&
            ev.y >= gridBounds_.y && ev.y <= gridBounds_.y + gridBounds_.h) {
            isDraggingScrollbar_ = true;
            dragStartY_ = ev.y;
            dragStartScrollY_ = scrollY_;
            float trackH = gridBounds_.h;
            float thumbH = std::max(20.0f, trackH * (gridBounds_.h / (gridBounds_.h + maxScrollY_)));
            float travel = trackH - thumbH;
            float thumbNorm = (travel > 1.0f) ? (scrollY_ / maxScrollY_) : 0.0f;
            float thumbY = gridBounds_.y + thumbNorm * travel;
            if (ev.y < thumbY || ev.y > thumbY + thumbH) {
                float targetNorm = (travel > 1.0f) ? std::clamp((ev.y - gridBounds_.y - thumbH * 0.5f) / travel, 0.0f, 1.0f) : 0.0f;
                scrollY_ = targetNorm * maxScrollY_;
                dragStartScrollY_ = scrollY_;
            }
            return true;
        }
    }

    if (ev.action != PointerAction::Down) return true; // Consume all move/hover events over modal

    // Close button (metallic screw icon) - check first with generous hit area
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    if (closeBtnBounds_.contains(ev.x, ev.y) || std::hypot(ev.x - clCenterX, ev.y - clCenterY) <= 16.0f) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Click outside dialog to close
    if (!dialogBounds_.contains(ev.x, ev.y)) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Search Clear Button [x]
    if (!searchQuery_.empty() && searchClearBtnBounds_.contains(ev.x, ev.y)) {
        clearSearch();
        return true;
    }

    // Search Box Focus
    if (searchBoxBounds_.contains(ev.x, ev.y)) {
        searchFocused_ = true;
        return true;
    }

    // Category chips
    for (size_t i = 0; i < categoryChipBounds_.size() && i < categories_.size(); ++i) {
        if (categoryChipBounds_[i].contains(ev.x, ev.y)) {
            selectedCategoryIndex_ = static_cast<int>(i);
            scrollY_ = 0.0f;
            return true;
        }
    }

    // Paste button
    if (pasteBtnBounds_.contains(ev.x, ev.y)) {
        pasteFromClipboard();
        return true;
    }

    // Custom tab apply button
    bool isCustomTab = (selectedCategoryIndex_ == static_cast<int>(categories_.size()) - 1);
    if (isCustomTab && customPastedDef_.has_value()) {
        if (applyBtnBounds_.contains(ev.x, ev.y)) {
            std::string svgRef = "svg:" + customPastedDef_->svgPath;
            close();
            if (onIconSelected) {
                onIconSelected(svgRef, targetTrackIndex_);
            }
            return true;
        }
    }

    // Icon Cards click
    if (!isCustomTab && gridBounds_.contains(ev.x, ev.y)) {
        auto icons = getFilteredIcons();
        float cardW = 86.0f;
        float cardH = 74.0f;
        float gapX = 10.0f;
        float gapY = 10.0f;
        int cols = std::max(1, static_cast<int>((gridBounds_.w + gapX) / (cardW + gapX)));

        for (size_t i = 0; i < icons.size(); ++i) {
            int col = static_cast<int>(i % cols);
            int row = static_cast<int>(i / cols);

            float cardX = gridBounds_.x + static_cast<float>(col) * (cardW + gapX);
            float cardY = gridBounds_.y + static_cast<float>(row) * (cardH + gapY) - scrollY_;

            Rect2D cardRect(cardX, cardY, cardW, cardH);
            if (cardRect.contains(ev.x, ev.y)) {
                std::string iconRef = "preset:" + icons[i]->id;
                close();
                if (onIconSelected) {
                    onIconSelected(iconRef, targetTrackIndex_);
                }
                return true;
            }
        }
    }

    return true;
}

bool IconSearchDialog::handleChar(char32_t codepoint) {
    if (!isOpen_) return false;
    // Accept valid ASCII printable characters (space through tilde)
    if (codepoint >= 32 && codepoint <= 126) {
        searchQuery_ += static_cast<char>(codepoint);
        scrollY_ = 0.0f;
        return true;
    }
    return false;
}

bool IconSearchDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, int mods) {
    if (!isOpen_) return false;
    if (action != 1 && action != 2) return false;

    // Esc (256)
    if (key == 256) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Ctrl+V paste
    bool isCtrlPressed = (mods & 0x0002) != 0; // GLFW_MOD_CONTROL
    if (isCtrlPressed && (key == 86 || key == 118)) { // 'V' or 'v'
        pasteFromClipboard();
        return true;
    }

    // Backspace (259)
    if (key == 259) {
        if (!searchQuery_.empty()) {
            searchQuery_.pop_back();
            scrollY_ = 0.0f;
        }
        return true;
    }

    // Delete (261)
    if (key == 261) {
        if (!searchQuery_.empty()) {
            searchQuery_.clear();
            scrollY_ = 0.0f;
        }
        return true;
    }

    // Enter (257) - auto-select top matched icon
    if (key == 257) {
        auto icons = getFilteredIcons();
        if (!icons.empty()) {
            std::string iconRef = "preset:" + icons[0]->id;
            close();
            if (onIconSelected) {
                onIconSelected(iconRef, targetTrackIndex_);
            }
            return true;
        }
    }

    // Arrow navigation / scrolling
    if (key == 264) { // Down
        handleScroll(-1.5f);
        return true;
    }
    if (key == 265) { // Up
        handleScroll(1.5f);
        return true;
    }
    if (key == 266) { // Page Up
        handleScroll(6.0f);
        return true;
    }
    if (key == 267) { // Page Down
        handleScroll(-6.0f);
        return true;
    }

    // Printable keys fallback if not handled by onChar
    if (key >= 32 && key <= 126 && !isCtrlPressed) {
        return true;
    }

    return false;
}

bool IconSearchDialog::handleScroll(float deltaY) {
    if (!isOpen_) return false;
    scrollY_ = std::clamp(scrollY_ - deltaY * 30.0f, 0.0f, maxScrollY_);
    return true;
}

} // namespace eatsbits::ui
