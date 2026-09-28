#include "eatsbits/ui/widgets/icon_search_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <cctype>

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
}

void IconSearchDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    float dw = std::min(screenW * 0.88f, 640.0f);
    float dh = std::min(screenH * 0.84f, 540.0f);
    float dx = (screenW - dw) * 0.5f;
    float dy = (screenH - dh) * 0.5f;

    dialogBounds_ = Rect2D(dx, dy, dw, dh);
    closeBtnBounds_ = Rect2D(dx + dw - 36.0f, dy + 14.0f, 24.0f, 24.0f);
    searchBoxBounds_ = Rect2D(dx + 20.0f, dy + 88.0f, dw - 40.0f, 32.0f);
    
    // Grid bounds
    float listY = searchBoxBounds_.y + searchBoxBounds_.h + 10.0f;
    float bottomBarH = 46.0f;
    float listH = dy + dh - listY - bottomBarH - 8.0f;
    gridBounds_ = Rect2D(dx + 20.0f, listY, dw - 40.0f, listH);

    // Paste button in bottom bar
    pasteBtnBounds_ = Rect2D(dx + 20.0f, dy + dh - 42.0f, dw - 40.0f, 30.0f);
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

    // 1. Semi-transparent backdrop scrim
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.70f);

    // 2. Main Dialog Frame (Chunky weathered CRT metal plate)
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 8.0f,
                           theme.borderFocus, 0.85f, 1.5f);

    // Dialog Header Banner
    drawRoundedRect(r, dialogBounds_.x + 2.0f, dialogBounds_.y + 2.0f, dialogBounds_.w - 4.0f, 44.0f, 6.0f,
                    theme.panelHeader.r, theme.panelHeader.g, theme.panelHeader.b, 1.0f);
    drawLine(r, dialogBounds_.x + 2.0f, dialogBounds_.y + 46.0f, 
             dialogBounds_.x + dialogBounds_.w - 2.0f, dialogBounds_.y + 46.0f,
             theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Header Title
    std::string headerTitle = "SELECT TRACK ICON" + (targetTrackName_.empty() ? "" : (" — " + targetTrackName_));
    drawText(r, headerTitle, dialogBounds_.x + 20.0f, dialogBounds_.y + 16.0f, 13.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, theme.primaryAccent);

    // 3. Category Filter Chips
    float chipX = dialogBounds_.x + 20.0f;
    float chipY = dialogBounds_.y + 54.0f;
    for (size_t i = 0; i < categories_.size(); ++i) {
        bool isSelected = (selectedCategoryIndex_ == static_cast<int>(i));
        float chipW = static_cast<float>(categories_[i].length()) * 7.0f + 16.0f;
        Rect2D chipRect(chipX, chipY, chipW, 24.0f);

        Color chipBg = isSelected
                           ? Color{theme.primaryAccent.r * 0.35f, theme.primaryAccent.g * 0.35f, theme.primaryAccent.b * 0.35f, 0.95f}
                           : Color{theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.70f};
        Color chipBorder = isSelected
                               ? theme.primaryAccent
                               : Color{theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.50f};
        Color chipText = isSelected ? theme.primaryAccent : theme.textMuted;

        drawRoundedRect(r, chipRect.x, chipRect.y, chipRect.w, chipRect.h, 4.0f, chipBg);
        drawRoundedRectOutline(r, chipRect.x, chipRect.y, chipRect.w, chipRect.h, 4.0f, chipBorder, 1.0f);
        drawCenteredText(r, categories_[i], chipRect, 9.5f, chipText);

        chipX += chipW + 6.0f;
    }

    // 4. Search Bar
    drawRoundedRect(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                    theme.backgroundDark.r * 0.8f, theme.backgroundDark.g * 0.8f, theme.backgroundDark.b * 0.8f, 1.0f);
    drawRoundedRectOutline(r, searchBoxBounds_.x, searchBoxBounds_.y, searchBoxBounds_.w, searchBoxBounds_.h, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);

    std::string searchDisplay = searchQuery_.empty() ? "Search icons (e.g. synth, kick, bass, mod, fx)..." : searchQuery_;
    Color searchCol = searchQuery_.empty() ? theme.textMuted : theme.textPrimary;
    drawText(r, searchDisplay, searchBoxBounds_.x + 12.0f, searchBoxBounds_.y + 9.0f, 11.0f, searchCol);

    // 5. Content Area
    bool isCustomTab = (selectedCategoryIndex_ == static_cast<int>(categories_.size()) - 1);

    if (isCustomTab) {
        // Custom SVG Preview and Apply Area
        float cx = gridBounds_.x + 20.0f;
        float cy = gridBounds_.y + 20.0f;
        float cw = gridBounds_.w - 40.0f;
        float ch = gridBounds_.h - 40.0f;

        drawRoundedRect(r, cx, cy, cw, ch, 6.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.6f);
        drawRoundedRectOutline(r, cx, cy, cw, ch, 6.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

        if (customPastedDef_.has_value()) {
            // Render large preview
            float previewSize = 72.0f;
            float px = cx + (cw - previewSize) * 0.5f;
            float py = cy + 30.0f;

            drawRoundedRect(r, px - 8.0f, py - 8.0f, previewSize + 16.0f, previewSize + 16.0f, 8.0f,
                            theme.backgroundDark.r, theme.backgroundDark.g, theme.backgroundDark.b, 0.9f);
            drawRoundedRectOutline(r, px - 8.0f, py - 8.0f, previewSize + 16.0f, previewSize + 16.0f, 8.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.8f, 1.5f);

            IconRegistry::instance().renderIcon(r, "svg:" + customPastedDef_->svgPath,
                                                px, py, previewSize, theme.primaryAccent);

            drawCenteredText(r, "CUSTOM SVG PREVIEW", Rect2D{cx, py + previewSize + 16.0f, cw, 20.0f},
                             11.0f, theme.textPrimary);

            // Apply Button
            float applyW = 160.0f;
            float applyH = 34.0f;
            float applyX = cx + (cw - applyW) * 0.5f;
            float applyY = py + previewSize + 52.0f;

            Color btnBg{theme.primaryAccent.r * 0.3f, theme.primaryAccent.g * 0.3f, theme.primaryAccent.b * 0.3f, 1.0f};
            drawButton(r, Rect2D{applyX, applyY, applyW, applyH}, "APPLY TO TRACK",
                       btnBg, theme.primaryAccent, theme.primaryAccent, 11.0f, 4.0f, 1.5f);
        } else {
            drawCenteredText(r, "No custom SVG loaded.", Rect2D{cx, cy + ch * 0.35f, cw, 24.0f},
                             12.0f, theme.textMuted);
            drawCenteredText(r, "Click 'PASTE SVG FROM CLIPBOARD' below or press Ctrl+V to import.",
                             Rect2D{cx, cy + ch * 0.45f, cw, 20.0f}, 10.0f, theme.textMuted);
        }
    } else {
        // Multi-column Card Grid
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

            Color cardBg = isCurrent
                               ? Color{theme.primaryAccent.r * 0.20f, theme.primaryAccent.g * 0.20f, theme.primaryAccent.b * 0.20f, 0.90f}
                               : Color{theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.85f};
            Color cardBorder = isCurrent ? theme.primaryAccent : Color{theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f};

            drawRoundedRect(r, cardX, cardY, cardW, cardH, 5.0f, cardBg);
            drawRoundedRectOutline(r, cardX, cardY, cardW, cardH, 5.0f, cardBorder, 1.0f);

            // Icon glyph (32x32)
            float iconSize = 30.0f;
            float iconX = cardX + (cardW - iconSize) * 0.5f;
            float iconY = cardY + 8.0f;
            Color iconColor = isCurrent ? theme.primaryAccent : theme.textPrimary;
            IconRegistry::instance().renderIcon(r, "preset:" + def->id, iconX, iconY, iconSize, iconColor);

            // Label
            drawCenteredText(r, def->displayName, Rect2D{cardX + 2.0f, cardY + cardH - 22.0f, cardW - 4.0f, 16.0f},
                             8.5f, isCurrent ? theme.primaryAccent : theme.textMuted);
        }

        if (icons.empty()) {
            drawCenteredText(r, "No matching icons found.", gridBounds_, 12.0f, theme.textMuted);
        }
    }

    // 6. Bottom Bar: Paste Button and Status Message
    Color pasteBg{theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.90f};
    Color pasteBorder = theme.borderFocus;
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

    if (ev.action != PointerAction::Down) return true; // Consume events behind modal

    // Click outside to close
    if (!dialogBounds_.contains(ev.x, ev.y)) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Close button
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    if (closeBtnBounds_.contains(ev.x, ev.y) || std::hypot(ev.x - clCenterX, ev.y - clCenterY) <= 13.0f) {
        close();
        if (onClose) onClose();
        return true;
    }

    // Category chips
    float chipX = dialogBounds_.x + 20.0f;
    float chipY = dialogBounds_.y + 54.0f;
    for (size_t i = 0; i < categories_.size(); ++i) {
        float chipW = static_cast<float>(categories_[i].length()) * 7.0f + 16.0f;
        Rect2D chipRect(chipX, chipY, chipW, 24.0f);
        if (chipRect.contains(ev.x, ev.y)) {
            selectedCategoryIndex_ = static_cast<int>(i);
            scrollY_ = 0.0f;
            return true;
        }
        chipX += chipW + 6.0f;
    }

    // Paste button
    if (pasteBtnBounds_.contains(ev.x, ev.y)) {
        pasteFromClipboard();
        return true;
    }

    // Custom tab apply button
    bool isCustomTab = (selectedCategoryIndex_ == static_cast<int>(categories_.size()) - 1);
    if (isCustomTab && customPastedDef_.has_value()) {
        float cw = gridBounds_.w - 40.0f;
        float applyW = 160.0f;
        float applyH = 34.0f;
        float applyX = gridBounds_.x + 20.0f + (cw - applyW) * 0.5f;
        float applyY = gridBounds_.y + 20.0f + 30.0f + 72.0f + 52.0f;

        Rect2D applyRect(applyX, applyY, applyW, applyH);
        if (applyRect.contains(ev.x, ev.y)) {
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

bool IconSearchDialog::handleKey(int key, [[maybe_unused]] int scancode, int action, int mods) {
    if (!isOpen_) return false;
    if (action != 1 && action != 2) return false;

    // Esc
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

    // Backspace
    if (key == 259) {
        if (!searchQuery_.empty()) {
            searchQuery_.pop_back();
            scrollY_ = 0.0f;
        }
        return true;
    }

    // Character input
    if (key >= 32 && key <= 126) {
        searchQuery_ += static_cast<char>(key);
        scrollY_ = 0.0f;
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
