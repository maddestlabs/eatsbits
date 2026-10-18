#include "eatsbits/ui/widgets/value_edit_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/icon_registry.hpp"
#include <clay.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace eatsbits::ui {

ValueEditDialog::ValueEditDialog() {
    dialogBounds_ = Rect2D(0.0f, 0.0f, 370.0f, 260.0f);
    layout(1280.0f, 800.0f);
}

void ValueEditDialog::open(const ValueEditRequest& req) {
    isOpen_ = true;
    req_ = req;
    isPercentMode_ = false;
    cursorBlinkTimer_ = 0.0f;
    isDraggingSelection_ = false;
    isDraggingSlider_ = false;
    inputScrollX_ = 0.0f;

    presenter::ValueEditConfig cfg;
    cfg.title = req.title;
    cfg.paramName = req.paramName;
    cfg.isTextMode = req.isTextMode;
    cfg.initialText = req.initialText;
    cfg.currentValue = req.currentValue;
    cfg.minValue = req.minValue;
    cfg.maxValue = req.maxValue;
    cfg.defaultValue = req.defaultValue;
    cfg.hasDefault = req.hasDefault;
    cfg.isInteger = req.isInteger;
    cfg.allowPercentage = req.allowPercentage;
    cfg.unit = req.unit;
    cfg.onCommit = req.onCommit;
    cfg.onCommitText = req.onCommitText;
    cfg.onCancel = [this]() { close(); };

    presenter_.open(cfg);
    textModel_.setText(presenter_.getInitialEditText(), /*selectAllOnSet=*/ true);
    layout(screenWidth_, screenHeight_);
}

void ValueEditDialog::formatBufferFromValue(float val) {
    textModel_.setText(presenter_.formatValueForEdit(val), /*selectAllOnSet=*/ true);
}

void ValueEditDialog::setPercentMode(bool enabled) {
    if (isPercentMode_ == enabled) return;
    togglePercentMode();
}

void ValueEditDialog::togglePercentMode() {
    std::string next = presenter_.togglePercentMode(textModel_.getText());
    isPercentMode_ = presenter_.isPercentMode();
    textModel_.setText(next, /*selectAllOnSet=*/ true);
}

void ValueEditDialog::resetToDefault() {
    std::string next = presenter_.resetToDefault();
    isPercentMode_ = presenter_.isPercentMode();
    if (!next.empty()) {
        textModel_.setText(next, /*selectAllOnSet=*/ true);
    }
}

void ValueEditDialog::setQuickPercent(float pct) {
    std::string next = presenter_.calculateQuickPercent(pct);
    if (!next.empty()) {
        textModel_.setText(next, /*selectAllOnSet=*/ true);
    }
}

void ValueEditDialog::submit() {
    if (!isOpen_) return;
    presenter_.submit(textModel_.getText());
    close();
}

float ValueEditDialog::getSliderNormalized() const noexcept {
    if (req_.isTextMode) return 0.0f;
    if (req_.maxValue <= req_.minValue) return 0.0f;

    auto valResult = presenter_.validate(textModel_.getText());
    float curVal = valResult.isValid ? valResult.parsedValue : req_.currentValue;
    return std::clamp((curVal - req_.minValue) / (req_.maxValue - req_.minValue), 0.0f, 1.0f);
}

void ValueEditDialog::setSliderNormalized(float norm) {
    if (req_.isTextMode) return;
    norm = std::clamp(norm, 0.0f, 1.0f);
    if (isPercentMode_) {
        float pct = norm * 100.0f;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", pct);
        textModel_.setText(buf, /*selectAllOnSet=*/ false);
    } else {
        float val = req_.minValue + norm * (req_.maxValue - req_.minValue);
        if (req_.isInteger) {
            val = std::round(val);
        }
        std::string str = presenter_.formatValueForEdit(val);
        textModel_.setText(str, /*selectAllOnSet=*/ false);
    }
}

void ValueEditDialog::layout(float screenW, float screenH) {
    screenWidth_ = screenW;
    screenHeight_ = screenH;

    if (!clayCtx_.isInitialized()) {
        clayCtx_.initialize(screenW, screenH);
    } else {
        clayCtx_.setDimensions(screenW, screenH);
    }

    auto toRect = [](const Clay_BoundingBox& b) noexcept -> Rect2D {
        return Rect2D(b.x, b.y, b.width, b.height);
    };

    if (req_.isTextMode) {
        float dw = 400.0f;
        float dh = (req_.onActionLink && !req_.actionLinkLabel.empty()) ? 210.0f : 170.0f;

        clayCtx_.beginLayout();
        CLAY(CLAY_ID("ValueEditTextOverlay"), {
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(screenW), CLAY_SIZING_FIXED(screenH) },
                .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
            }
        }) {
            CLAY(CLAY_ID("ValueEditTextCard"), {
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(dw), CLAY_SIZING_FIXED(dh) },
                    .padding = { 20, 12, 12, 12 },
                    .childGap = 10,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                }
            }) {
                // Header row
                CLAY(CLAY_ID("ValueEditTextHeaderRow"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(22) },
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    }
                }) {
                    CLAY(CLAY_ID("ValueEditTextTitleArea"), {
                        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(22) } }
                    }) {}
                    CLAY(CLAY_ID("ValueEditTextCloseBtn"), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(22), CLAY_SIZING_FIXED(22) } }
                    }) {}
                }

                // Subtitle / Prompt hint (height 18)
                CLAY(CLAY_ID("ValueEditTextHintSpacer"), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(18) } }
                }) {}

                // Text Input Field
                CLAY(CLAY_ID("ValueEditTextInputField"), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(38) } }
                }) {}

                // Optional Action Link
                if (req_.onActionLink && !req_.actionLinkLabel.empty()) {
                    CLAY(CLAY_ID("ValueEditTextActionLink"), {
                        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32) } }
                    }) {}
                }

                // Spacer before footer
                CLAY(CLAY_ID("ValueEditTextFooterSpacer"), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
                }) {}

                // Footer Row (Cancel & OK)
                CLAY(CLAY_ID("ValueEditTextFooterRow"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(26) },
                        .childGap = 10,
                        .childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    }
                }) {
                    CLAY(CLAY_ID("ValueEditTextCancelBtn"), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(76), CLAY_SIZING_FIXED(26) } }
                    }) {}
                    CLAY(CLAY_ID("ValueEditTextOkBtn"), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(74), CLAY_SIZING_FIXED(26) } }
                    }) {}
                }
            }
        }
        clayCtx_.endLayout(0.016f);

        dialogBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditTextCard")).boundingBox);
        closeBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditTextCloseBtn")).boundingBox);
        inputFieldBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditTextInputField")).boundingBox);
        percentToggleBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
        defaultBtnBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
        sliderBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
        isDraggingSlider_ = false;
        quickPercentPills_.clear();

        if (req_.onActionLink && !req_.actionLinkLabel.empty()) {
            actionLinkBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditTextActionLink")).boundingBox);
        } else {
            actionLinkBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
        }

        cancelBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditTextCancelBtn")).boundingBox);
        okBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditTextOkBtn")).boundingBox);
        return;
    }

    float dw = 370.0f;
    float dh = 260.0f;

    clayCtx_.beginLayout();
    CLAY(CLAY_ID("ValueEditNumOverlay"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(screenW), CLAY_SIZING_FIXED(screenH) },
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
        }
    }) {
        CLAY(CLAY_ID("ValueEditNumCard"), {
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(dw), CLAY_SIZING_FIXED(dh) },
                .padding = { 20, 12, 12, 14 },
                .childGap = 8,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            }
        }) {
            // Header Row (Title, Default, %, Close)
            CLAY(CLAY_ID("ValueEditNumHeaderRow"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(22) },
                    .childGap = 8,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {
                CLAY(CLAY_ID("ValueEditNumTitleArea"), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(22) } }
                }) {}
                CLAY(CLAY_ID("ValueEditNumDefaultBtn"), {
                    .layout = { .sizing = { CLAY_SIZING_FIXED(70), CLAY_SIZING_FIXED(22) } }
                }) {}
                CLAY(CLAY_ID("ValueEditNumPercentBtn"), {
                    .layout = { .sizing = { CLAY_SIZING_FIXED(32), CLAY_SIZING_FIXED(22) } }
                }) {}
                CLAY(CLAY_ID("ValueEditNumCloseBtn"), {
                    .layout = { .sizing = { CLAY_SIZING_FIXED(22), CLAY_SIZING_FIXED(22) } }
                }) {}
            }

            // Subtitle / Range hint row (starts at 12 + 22 + 8 = 42, height 18 -> ends at 60)
            CLAY(CLAY_ID("ValueEditNumHintRow"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(18) } }
            }) {}

            // Input field (gap 8 -> starts at 60 + 8 = 68, height 40 -> ends at 108)
            CLAY(CLAY_ID("ValueEditNumInputField"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40) } }
            }) {}

            // Slider (gap 8 -> starts at 108 + 8 = 116, height 18 -> ends at 134)
            CLAY(CLAY_ID("ValueEditNumSlider"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(18) } }
            }) {}

            // Quick percent pills row (starts at 134 + 10 = 144, height 24 -> ends at 168)
            CLAY(CLAY_ID("ValueEditNumPillsRow"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24) },
                    .padding = { 0, 0, 2, 0 },
                    .childGap = 15,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {
                for (int i = 0; i < 5; ++i) {
                    CLAY(CLAY_IDI("ValueEditNumPill", static_cast<uint32_t>(i)), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(54), CLAY_SIZING_FIXED(24) } }
                    }) {}
                }
            }

            // Spacer before footer
            CLAY(CLAY_ID("ValueEditNumFooterSpacer"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
            }) {}

            // Footer Row (Cancel & OK)
            CLAY(CLAY_ID("ValueEditNumFooterRow"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28) },
                    .childGap = 10,
                    .childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {
                CLAY(CLAY_ID("ValueEditNumCancelBtn"), {
                    .layout = { .sizing = { CLAY_SIZING_FIXED(76), CLAY_SIZING_FIXED(28) } }
                }) {}
                CLAY(CLAY_ID("ValueEditNumOkBtn"), {
                    .layout = { .sizing = { CLAY_SIZING_FIXED(74), CLAY_SIZING_FIXED(28) } }
                }) {}
            }
        }
    }
    clayCtx_.endLayout(0.016f);

    dialogBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumCard")).boundingBox);
    closeBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumCloseBtn")).boundingBox);
    percentToggleBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumPercentBtn")).boundingBox);
    defaultBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumDefaultBtn")).boundingBox);
    actionLinkBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
    inputFieldBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumInputField")).boundingBox);
    sliderBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumSlider")).boundingBox);

    const float pcts[5] = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f};
    quickPercentPills_.clear();
    for (int i = 0; i < 5; ++i) {
        quickPercentPills_.push_back({
            toRect(Clay_GetElementData(CLAY_IDI("ValueEditNumPill", static_cast<uint32_t>(i))).boundingBox),
            pcts[i]
        });
    }

    cancelBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumCancelBtn")).boundingBox);
    okBtnBounds_ = toRect(Clay_GetElementData(CLAY_ID("ValueEditNumOkBtn")).boundingBox);
}

void ValueEditDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // Semi-transparent deep darkening overlay (fast vertex batching, 0 heap allocations)
    drawRect(r, 0.0f, 0.0f, screenWidth_, screenHeight_, 0.0f, 0.0f, 0.0f, 0.45f);

    const auto& accent = req_.accentColor;
    float dx = dialogBounds_.x;
    float dy = dialogBounds_.y;
    float dw = dialogBounds_.w;
    float dh = dialogBounds_.h;

    // 2. Dialog Chassis with drop shadow, theme background & subtle gold/accent outline
    drawRoundedRect(r, dx - 3.0f, dy - 3.0f, dw + 6.0f, dh + 6.0f, 13.0f, 0.0f, 0.0f, 0.0f, 0.55f);
    drawRoundedRect(r, dx, dy, dw, dh, 12.0f,
                    theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 0.98f);
    drawRoundedRectOutline(r, dx, dy, dw, dh, 12.0f,
                           accent.r, accent.g, accent.b, 0.85f, 1.5f);

    // 3. Header Strip: Title, Accent Dot, % Toggle, Default button, Close button
    drawCircle(r, dx + 22.0f, dy + 23.0f, 4.0f, accent.r, accent.g, accent.b, 1.0f);
    drawText(r, req_.title, dx + 32.0f, dy + 17.0f, 12.5f, accent.r, accent.g, accent.b, 1.0f);

    // Default button
    if (req_.hasDefault) {
        drawRoundedRect(r, defaultBtnBounds_.x, defaultBtnBounds_.y, defaultBtnBounds_.w, defaultBtnBounds_.h, 4.0f,
                        theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
        drawRoundedRectOutline(r, defaultBtnBounds_.x, defaultBtnBounds_.y, defaultBtnBounds_.w, defaultBtnBounds_.h, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
        drawText(r, "DEFAULT", defaultBtnBounds_.x + 10.0f, defaultBtnBounds_.y + 6.0f, 9.5f,
                 theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    }

    // % Mode Toggle Chip
    if (req_.allowPercentage) {
        if (isPercentMode_) {
            drawRoundedRect(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                            accent.r * 0.25f, accent.g * 0.25f, accent.b * 0.25f, 1.0f);
            drawRoundedRectOutline(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                                   accent.r, accent.g, accent.b, 1.0f, 1.2f);
            drawText(r, "%", percentToggleBounds_.x + 11.0f, percentToggleBounds_.y + 5.0f, 11.0f,
                     accent.r, accent.g, accent.b, 1.0f);
        } else {
            drawRoundedRect(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                            theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
            drawRoundedRectOutline(r, percentToggleBounds_.x, percentToggleBounds_.y, percentToggleBounds_.w, percentToggleBounds_.h, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.7f, 1.0f);
            drawText(r, "%", percentToggleBounds_.x + 11.0f, percentToggleBounds_.y + 5.0f, 11.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
        }
    }

    // Close button (metallic screw icon)
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_) ||
                    (std::hypot(lastMouseX_ - clCenterX, lastMouseY_ - clCenterY) <= 13.0f);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, accent);

    // Subtitle / Range hint / Action prompt
    std::string hint;
    if (req_.isTextMode) {
        hint = req_.paramName.empty() ? "Enter track name or description" : req_.paramName;
    } else if (isPercentMode_) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "PERCENT MODE (0%% - 100%% of %.2f - %.2f%s)",
                      req_.minValue, req_.maxValue, req_.unit.empty() ? "" : (" " + req_.unit).c_str());
        hint = buf;
    } else {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "RANGE: %.2f - %.2f%s",
                      req_.minValue, req_.maxValue, req_.unit.empty() ? "" : (" " + req_.unit).c_str());
        hint = buf;
    }
    drawText(r, hint, dx + 22.0f, dy + 45.0f, 9.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);

    // 4. Input Field (Recessed dark well with active glowing accent border)
    drawRoundedRect(r, inputFieldBounds_.x, inputFieldBounds_.y, inputFieldBounds_.w, inputFieldBounds_.h, 6.0f,
                    theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 1.0f);
    drawRoundedRectOutline(r, inputFieldBounds_.x, inputFieldBounds_.y, inputFieldBounds_.w, inputFieldBounds_.h, 6.0f,
                           accent.r, accent.g, accent.b, 0.95f, 1.5f);

    // Formatted Text and Selection Highlight
    const std::string& textToShow = textModel_.getText();
    float pad = 12.0f;
    float clipMinX = inputFieldBounds_.x + pad;
    float clipMaxX = inputFieldBounds_.x + inputFieldBounds_.w - pad;
    float availableW = std::max(10.0f, clipMaxX - clipMinX);
    float charW = getMonoCharAdvance(16.0f);

    // Auto-scroll containment: keep cursor strictly inside field well
    float curPixelX = static_cast<float>(textModel_.getCursor()) * charW;
    if (curPixelX < inputScrollX_) {
        inputScrollX_ = curPixelX;
    } else if (curPixelX > inputScrollX_ + availableW) {
        inputScrollX_ = curPixelX - availableW;
    }
    inputScrollX_ = std::max(0.0f, inputScrollX_);

    float textStartX = clipMinX - inputScrollX_;

    // Draw active text selection highlight if present (clipped to well)
    if (textModel_.hasSelection()) {
        int s = textModel_.getSelectionStart();
        int e = textModel_.getSelectionEnd();
        float selX1 = std::clamp(textStartX + static_cast<float>(s) * charW, clipMinX, clipMaxX);
        float selX2 = std::clamp(textStartX + static_cast<float>(e) * charW, clipMinX, clipMaxX);
        float selW = selX2 - selX1;

        if (selW > 0.5f) {
            float selY = inputFieldBounds_.y + 6.0f;
            float selH = inputFieldBounds_.h - 12.0f;
            drawRoundedRect(r, selX1, selY, selW, selH, 3.0f,
                            accent.r, accent.g, accent.b, 0.38f);
            drawRoundedRectOutline(r, selX1, selY, selW, selH, 3.0f,
                                   accent.r, accent.g, accent.b, 0.70f, 1.0f);
        }
    }

    drawMonoTextClipped(r, textToShow, textStartX, inputFieldBounds_.y + 11.0f, 16.0f,
                        clipMinX, clipMaxX, accent.r, accent.g, accent.b, 1.0f);

    // Suffix '%' or unit
    if (!req_.isTextMode) {
        if (isPercentMode_) {
            float suffixX = textStartX + 4.0f + static_cast<float>(textToShow.length()) * charW;
            if (suffixX >= clipMinX && suffixX <= clipMaxX) {
                drawMonoText(r, "%", suffixX, inputFieldBounds_.y + 11.0f, 15.0f, accent.r, accent.g, accent.b, 0.85f);
            }
        } else if (!req_.unit.empty()) {
            float suffixX = textStartX + 4.0f + static_cast<float>(textToShow.length()) * charW;
            if (suffixX >= clipMinX && suffixX <= clipMaxX) {
                drawMonoText(r, req_.unit, suffixX, inputFieldBounds_.y + 12.0f, 13.0f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
            }
        }
    }

    // Blinking Cursor
    cursorBlinkTimer_ += 0.035f;
    if (std::fmod(cursorBlinkTimer_, 1.0f) < 0.5f) {
        int cur = textModel_.getCursor();
        float curX = textStartX + static_cast<float>(cur) * charW;
        if (curX >= clipMinX && curX <= clipMaxX) {
            drawLine(r, curX, inputFieldBounds_.y + 8.0f, curX, inputFieldBounds_.y + inputFieldBounds_.h - 8.0f,
                     accent.r, accent.g, accent.b, 1.0f, 1.5f);
        }
    }

    // Optional Action Link Button (e.g. [🎨 Choose Track Icon...]) in text mode
    if (req_.isTextMode && req_.onActionLink && !req_.actionLinkLabel.empty()) {
        bool linkHov = actionLinkBounds_.contains(lastMouseX_, lastMouseY_);
        Color linkBg = linkHov ? Color(accent.r * 0.18f, accent.g * 0.18f, accent.b * 0.18f, 0.95f)
                               : Color(theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.85f);
        Color linkBdr = linkHov ? accent : Color(theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.75f);
        drawRoundedRect(r, actionLinkBounds_.x, actionLinkBounds_.y, actionLinkBounds_.w, actionLinkBounds_.h, 5.0f,
                        linkBg.r, linkBg.g, linkBg.b, linkBg.a);
        drawRoundedRectOutline(r, actionLinkBounds_.x, actionLinkBounds_.y, actionLinkBounds_.w, actionLinkBounds_.h, 5.0f,
                               linkBdr.r, linkBdr.g, linkBdr.b, linkBdr.a, linkHov ? 1.4f : 1.0f);

        float textX = actionLinkBounds_.x + 14.0f;
        if (!req_.currentIconRef.empty()) {
            float iconSize = 16.0f;
            float iconX = actionLinkBounds_.x + 10.0f;
            float iconY = actionLinkBounds_.y + (actionLinkBounds_.h - iconSize) * 0.5f;
            IconRegistry::instance().renderIcon(r, req_.currentIconRef, iconX, iconY, iconSize, linkHov ? accent : theme.textPrimary);
            textX = iconX + iconSize + 8.0f;
        } else {
            // Accent dot indicator
            drawCircle(r, actionLinkBounds_.x + 14.0f, actionLinkBounds_.y + actionLinkBounds_.h * 0.5f, 3.5f,
                       accent.r, accent.g, accent.b, linkHov ? 1.0f : 0.75f);
            textX = actionLinkBounds_.x + 24.0f;
        }

        drawText(r, req_.actionLinkLabel, textX, actionLinkBounds_.y + 9.5f, 10.5f,
                 linkHov ? accent.r : theme.textPrimary.r,
                 linkHov ? accent.g : theme.textPrimary.g,
                 linkHov ? accent.b : theme.textPrimary.b, 1.0f);
    }

    // 4b. Value Slider (Direct ergonomic slider adjustment)
    if (!req_.isTextMode && sliderBounds_.w > 0.0f) {
        float trackX = sliderBounds_.x;
        float trackW = sliderBounds_.w;
        float trackY = sliderBounds_.y + 7.0f;
        float trackH = 4.0f;
        float norm = getSliderNormalized();
        float fillW = trackW * norm;

        // Recessed track background
        drawRoundedRect(r, trackX, trackY, trackW, trackH, 2.0f,
                        theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 0.95f);
        drawRoundedRectOutline(r, trackX, trackY, trackW, trackH, 2.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.5f, 1.0f);

        // Filled active track
        if (fillW > 1.0f) {
            drawRoundedRect(r, trackX, trackY, fillW, trackH, 2.0f,
                            accent.r, accent.g, accent.b, 0.85f);
        }

        // Thumb
        float thumbX = trackX + fillW;
        float thumbY = trackY + trackH * 0.5f;
        bool thumbHover = sliderBounds_.contains(lastMouseX_, lastMouseY_) || isDraggingSlider_;
        float thumbRadius = thumbHover ? 7.5f : 6.0f;

        // Outer glow on hover / drag
        if (thumbHover) {
            drawCircle(r, thumbX, thumbY, thumbRadius + 3.0f, accent.r, accent.g, accent.b, 0.25f);
        }
        // Thumb body
        drawCircle(r, thumbX, thumbY, thumbRadius, 0.08f, 0.10f, 0.14f, 1.0f);
        drawCircle(r, thumbX, thumbY, thumbRadius - 1.5f, accent.r, accent.g, accent.b, 1.0f);
        drawCircleOutline(r, thumbX, thumbY, thumbRadius, accent.r, accent.g, accent.b, 1.0f, 1.2f);
    }

    // 5. Quick Percentage Shortcut Pills: 0%, 25%, 50%, 75%, 100% (Numeric mode only)
    if (!req_.isTextMode && req_.allowPercentage) {
        const char* pillLabels[5] = {"0%", "25%", "50%", "75%", "100%"};
        for (size_t i = 0; i < quickPercentPills_.size() && i < 5; ++i) {
            const auto& p = quickPercentPills_[i].first;
            drawRoundedRect(r, p.x, p.y, p.w, p.h, 4.0f,
                            theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
            drawRoundedRectOutline(r, p.x, p.y, p.w, p.h, 4.0f,
                                   theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
            drawText(r, pillLabels[i], p.x + 14.0f, p.y + 6.0f, 9.5f,
                     theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
        }
    }

    // 6. Action Buttons: CANCEL / OK (SAVE)
    // CANCEL button
    drawRoundedRect(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 5.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.9f);
    drawRoundedRectOutline(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 5.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);
    drawText(r, "CANCEL", cancelBtnBounds_.x + 16.0f, cancelBtnBounds_.y + 8.0f, 10.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);

    // OK / SAVE button
    drawRoundedRect(r, okBtnBounds_.x, okBtnBounds_.y, okBtnBounds_.w, okBtnBounds_.h, 5.0f,
                    accent.r, accent.g, accent.b, 1.0f);
    drawRoundedRectOutline(r, okBtnBounds_.x, okBtnBounds_.y, okBtnBounds_.w, okBtnBounds_.h, 5.0f,
                           accent.r, accent.g, accent.b, 1.0f, 1.2f);
    const char* confirmLabel = req_.isTextMode ? "SAVE" : "OK";
    float confirmTextX = req_.isTextMode ? okBtnBounds_.x + 22.0f : okBtnBounds_.x + 27.0f;
    drawText(r, confirmLabel, confirmTextX, okBtnBounds_.y + 8.0f, 11.0f,
             0.05f, 0.07f, 0.09f, 1.0f); // High contrast dark text on accent background
}

bool ValueEditDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    if (ev.action == PointerAction::Down) {
        // Click outside dialog chassis -> dismiss modal
        if (!dialogBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // Close button [x]
        float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
        float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
        if (closeBtnBounds_.contains(ev.x, ev.y) || std::hypot(ev.x - clCenterX, ev.y - clCenterY) <= 13.0f) {
            close();
            return true;
        }

        // Action Link Button (e.g. Choose Track Icon)
        if (req_.isTextMode && req_.onActionLink && actionLinkBounds_.contains(ev.x, ev.y)) {
            if (req_.onCommitText) {
                req_.onCommitText(textModel_.getText());
            }
            if (req_.onActionLink) {
                req_.onActionLink();
            }
            return true;
        }

        // % mode toggle chip
        if (!req_.isTextMode && req_.allowPercentage && percentToggleBounds_.contains(ev.x, ev.y)) {
            togglePercentMode();
            return true;
        }

        // [ DEFAULT ] button
        if (!req_.isTextMode && req_.hasDefault && defaultBtnBounds_.contains(ev.x, ev.y)) {
            formatBufferFromValue(req_.defaultValue);
            isPercentMode_ = false;
            submit();
            return true;
        }

        // Quick percentage shortcut pills
        if (!req_.isTextMode && req_.allowPercentage) {
            for (const auto& pill : quickPercentPills_) {
                if (pill.first.contains(ev.x, ev.y)) {
                    setQuickPercent(pill.second);
                    return true;
                }
            }
        }

        // CANCEL button
        if (cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            return true;
        }

        // OK / SAVE button
        if (okBtnBounds_.contains(ev.x, ev.y)) {
            submit();
            return true;
        }

        // Click inside Input Field -> Place cursor or start drag selection
        if (inputFieldBounds_.contains(ev.x, ev.y)) {
            float pad = 12.0f;
            float textStartX = inputFieldBounds_.x + pad - inputScrollX_;
            float charW = getMonoCharAdvance(16.0f);
            int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                                 0, static_cast<int>(textModel_.length()));
            textModel_.setCursor(idx, /*keepAnchor=*/ ev.mods.shift);
            isDraggingSelection_ = true;
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // Slider hit
        if (!req_.isTextMode && sliderBounds_.w > 0.0f && sliderBounds_.contains(ev.x, ev.y)) {
            isDraggingSlider_ = true;
            float norm = (ev.x - sliderBounds_.x) / sliderBounds_.w;
            setSliderNormalized(norm);
            return true;
        }

        // Intercept any click inside modal
        return true;
    }

    if (ev.action == PointerAction::Move) {
        if (isDraggingSlider_) {
            float norm = (ev.x - sliderBounds_.x) / sliderBounds_.w;
            setSliderNormalized(norm);
            return true;
        }
        if (isDraggingSelection_) {
            float pad = 12.0f;
            float textStartX = inputFieldBounds_.x + pad - inputScrollX_;
            float charW = getMonoCharAdvance(16.0f);
            int idx = std::clamp(static_cast<int>(std::round((ev.x - textStartX) / charW)),
                                 0, static_cast<int>(textModel_.length()));
            if (idx != textModel_.getCursor()) {
                textModel_.setCursor(idx, /*keepAnchor=*/ true);
                cursorBlinkTimer_ = 0.0f;
            }
            return true;
        }
    }

    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        if (isDraggingSlider_) {
            isDraggingSlider_ = false;
            return true;
        }
        if (isDraggingSelection_) {
            isDraggingSelection_ = false;
            return true;
        }
    }

    return true; // Intercept all pointer events while modal is open
}

bool ValueEditDialog::handleKey(int key, int /*scancode*/, int action, int mods) {
    if (!isOpen_ || action == 0 /* GLFW_RELEASE */) return false;

    bool isShift = (mods & 0x0001) != 0;
    bool isCtrl = (mods & 0x0002) != 0;

    // Enter / Return -> Submit
    if (key == 257 /* GLFW_KEY_ENTER */ || key == 335 /* GLFW_KEY_KP_ENTER */) {
        submit();
        return true;
    }

    // Escape -> Close
    if (key == 256 /* GLFW_KEY_ESCAPE */) {
        close();
        return true;
    }

    // Ctrl shortcuts
    if (isCtrl) {
        // Ctrl+A -> Select All
        if (key == 65 || key == 97) {
            textModel_.selectAll();
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        // Ctrl+C -> Copy selected text
        if (key == 67 || key == 99) {
            if (onCopyToClipboard && textModel_.hasSelection()) {
                onCopyToClipboard(textModel_.getSelectedText());
            }
            return true;
        }
        // Ctrl+X -> Cut selected text
        if (key == 88 || key == 120) {
            if (textModel_.hasSelection()) {
                if (onCopyToClipboard) {
                    onCopyToClipboard(textModel_.getSelectedText());
                }
                textModel_.deleteSelection();
                cursorBlinkTimer_ = 0.0f;
            }
            return true;
        }
        // Ctrl+V -> Paste
        if (key == 86 || key == 118) {
            if (onPasteFromClipboard) {
                std::string clip = onPasteFromClipboard();
                if (!clip.empty()) {
                    textModel_.insertText(clip);
                    cursorBlinkTimer_ = 0.0f;
                }
            }
            return true;
        }
    }

    // Left Arrow
    if (key == 263 /* GLFW_KEY_LEFT */) {
        textModel_.moveLeft(isShift, isCtrl);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Right Arrow
    if (key == 262 /* GLFW_KEY_RIGHT */) {
        textModel_.moveRight(isShift, isCtrl);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Home
    if (key == 268 /* GLFW_KEY_HOME */) {
        textModel_.moveHome(isShift);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // End
    if (key == 269 /* GLFW_KEY_END */) {
        textModel_.moveEnd(isShift);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Backspace
    if (key == 259 /* GLFW_KEY_BACKSPACE */) {
        textModel_.backspace(isCtrl);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Delete
    if (key == 261 /* GLFW_KEY_DELETE */) {
        textModel_.forwardDelete(isCtrl);
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Space (32)
    if (key == 32) {
        textModel_.insertChar(' ');
        cursorBlinkTimer_ = 0.0f;
        return true;
    }

    // Text mode: letters, symbols, alphanumeric
    if (req_.isTextMode) {
        // Letters A-Z
        if (key >= 65 && key <= 90) {
            char c = isShift ? static_cast<char>(key) : static_cast<char>(key + 32);
            textModel_.insertChar(c);
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // Digits / top-row symbols
        if (key >= 48 && key <= 57) {
            if (isShift) {
                const char shiftNums[] = {')', '!', '@', '#', '$', '%', '^', '&', '*', '('};
                textModel_.insertChar(shiftNums[key - 48]);
            } else {
                textModel_.insertChar(static_cast<char>('0' + (key - 48)));
            }
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // Common punctuation and symbols
        if (key == 45) { // Minus / Underscore
            textModel_.insertChar(isShift ? '_' : '-');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 61) { // Equal / Plus
            textModel_.insertChar(isShift ? '+' : '=');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 46 || key == 330) { // Period
            textModel_.insertChar(isShift ? '>' : '.');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 44) { // Comma
            textModel_.insertChar(isShift ? '<' : ',');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 47) { // Slash
            textModel_.insertChar(isShift ? '?' : '/');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 59) { // Semicolon
            textModel_.insertChar(isShift ? ':' : ';');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 39) { // Apostrophe / Quote
            textModel_.insertChar(isShift ? '"' : '\'');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 91) { // Left bracket
            textModel_.insertChar(isShift ? '{' : '[');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 93) { // Right bracket
            textModel_.insertChar(isShift ? '}' : ']');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key == 92) { // Backslash
            textModel_.insertChar(isShift ? '|' : '\\');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
        if (key >= 320 && key <= 329) { // Numpad
            textModel_.insertChar(static_cast<char>('0' + (key - 320)));
            cursorBlinkTimer_ = 0.0f;
            return true;
        }
    } else {
        // Numeric mode: Digits '0' through '9'
        if (key >= 48 && key <= 57) {
            textModel_.insertChar(static_cast<char>('0' + (key - 48)));
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // Keypad 0-9 (GLFW_KEY_KP_0 through KP_9: 320 - 329)
        if (key >= 320 && key <= 329) {
            textModel_.insertChar(static_cast<char>('0' + (key - 320)));
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // Period / Decimal point ('.' = 46, KP_DECIMAL = 330)
        if (key == 46 || key == 330) {
            std::string candidate = textModel_.getText();
            if (textModel_.hasSelection()) {
                candidate.erase(static_cast<size_t>(textModel_.getSelectionStart()),
                                static_cast<size_t>(textModel_.getSelectionLength()));
            }
            if (candidate.find('.') == std::string::npos) {
                textModel_.insertChar('.');
                cursorBlinkTimer_ = 0.0f;
            }
            return true;
        }

        // Minus / Hyphen ('-' = 45, KP_SUBTRACT = 333)
        if (key == 45 || key == 333) {
            textModel_.insertChar('-');
            cursorBlinkTimer_ = 0.0f;
            return true;
        }

        // '%' toggle or character entry
        if (key == 53 /* '%' */) {
            if (isShift) {
                togglePercentMode();
            } else {
                textModel_.insertChar('%');
                cursorBlinkTimer_ = 0.0f;
            }
            return true;
        }
    }

    return true; // Modal consumes key events
}

} // namespace eatsbits::ui
