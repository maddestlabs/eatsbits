#include "eatsbits/ui/widgets/macro_runner_dialog.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>
#include <cmath>

namespace eatsbits::ui {

MacroRunnerDialog::MacroRunnerDialog() {
    selectMacro("macro_song_gen");
}

void MacroRunnerDialog::open(const std::string& macroId) {
    isOpen_ = true;
    draggingSliderIdx_ = -1;
    statusMessage_.clear();
    selectMacro(macroId);
}

void MacroRunnerDialog::selectMacro(const std::string& macroId) {
    const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
    if (macros.empty()) return;

    selectedMacroId_ = macroId;
    selectedMacroIdx_ = 0;
    for (size_t i = 0; i < macros.size(); ++i) {
        if (macros[i].id == macroId) {
            selectedMacroIdx_ = static_cast<int>(i);
            break;
        }
    }

    resetToDefaults();
}

void MacroRunnerDialog::resetToDefaults() {
    const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
    if (!def) return;

    numValues_.clear();
    strValues_.clear();

    for (const auto& p : def->params) {
        if (p.type == eatscript::MacroParamType::Choice) {
            int idx = std::clamp(static_cast<int>(p.defaultVal), 0, static_cast<int>(p.options.size() - 1));
            strValues_[p.key] = (!p.options.empty() && idx >= 0 && idx < static_cast<int>(p.options.size()))
                                ? p.options[idx] : "";
            numValues_[p.key] = static_cast<float>(idx);
        } else {
            numValues_[p.key] = p.defaultVal;
        }
    }
}

void MacroRunnerDialog::randomizeSeed() {
    static std::mt19937 rng(1337);
    std::uniform_int_distribution<uint32_t> dist(1, 999999);
    numValues_["Seed"] = static_cast<float>(dist(rng));
}

void MacroRunnerDialog::randomizeAll() {
    const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
    if (!def) return;

    static std::mt19937 rng(42);
    for (const auto& p : def->params) {
        if (p.key == "Seed") {
            std::uniform_int_distribution<uint32_t> dist(1, 999999);
            numValues_["Seed"] = static_cast<float>(dist(rng));
        } else if (p.type == eatscript::MacroParamType::Choice && !p.options.empty()) {
            std::uniform_int_distribution<size_t> dist(0, p.options.size() - 1);
            size_t idx = dist(rng);
            strValues_[p.key] = p.options[idx];
            numValues_[p.key] = static_cast<float>(idx);
        } else if (p.type == eatscript::MacroParamType::Slider) {
            std::uniform_real_distribution<float> dist(p.minVal, p.maxVal);
            numValues_[p.key] = dist(rng);
        }
    }
}

void MacroRunnerDialog::setStatus(const std::string& statusMsg, bool isError) {
    statusMessage_ = statusMsg;
    isStatusError_ = isError;
    statusFadeTimer_ = 5.0f;
}

void MacroRunnerDialog::update(float dt) {
    if (statusFadeTimer_ > 0.0f) {
        statusFadeTimer_ -= dt;
        if (statusFadeTimer_ <= 0.0f) {
            statusFadeTimer_ = 0.0f;
        }
    }
}

void MacroRunnerDialog::layout(float screenW, float screenH) {
    const float w = std::min(740.0f, screenW - 32.0f);
    const float h = std::min(580.0f, screenH - 32.0f);
    const float x = (screenW - w) * 0.5f;
    const float y = (screenH - h) * 0.5f;

    dialogBounds_ = Rect2D(x, y, w, h);
    closeBtnBounds_ = Rect2D(x + w - 42.0f, y + 16.0f, 24.0f, 24.0f);

    // 1. Macro quick switcher pills
    const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
    const float macroBarY = y + 54.0f;
    macroSelectBarBounds_ = Rect2D(x + 24.0f, macroBarY, w - 48.0f, 30.0f);

    macroPillBounds_.clear();
    const float pillGap = 6.0f;
    const size_t pillCount = macros.size();
    const float pillW = std::min(130.0f, (w - 48.0f - (pillCount - 1) * pillGap) / std::max<size_t>(1, pillCount));
    for (size_t i = 0; i < pillCount; ++i) {
        macroPillBounds_.push_back(Rect2D(x + 24.0f + i * (pillW + pillGap), macroBarY, pillW, 26.0f));
    }

    // 2. Macro description box
    const float descY = macroBarY + 36.0f;
    descBoxBounds_ = Rect2D(x + 24.0f, descY, w - 48.0f, 32.0f);

    // 3. Parameters list area
    const float paramsY = descY + 42.0f;
    const float footerH = 64.0f;
    const float paramsH = (y + h - footerH) - paramsY;
    paramsAreaBounds_ = Rect2D(x + 24.0f, paramsY, w - 48.0f, paramsH);

    // Compute parameter row bounds
    const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
    paramRows_.clear();
    if (def) {
        float curY = paramsY + 4.0f;
        const float rowH = 40.0f;
        const float labelW = 180.0f;
        const float ctrlX = x + 24.0f + labelW + 12.0f;
        const float ctrlW = (w - 48.0f) - labelW - 12.0f;

        for (const auto& p : def->params) {
            ParamVisualRow r;
            r.key = p.key;
            r.type = p.type;
            r.rowBounds = Rect2D(x + 24.0f, curY, w - 48.0f, rowH - 4.0f);
            r.labelBounds = Rect2D(x + 24.0f, curY + 6.0f, labelW, 24.0f);

            if (p.type == eatscript::MacroParamType::Slider) {
                r.controlBounds = Rect2D(ctrlX, curY + 6.0f, ctrlW - 70.0f, 22.0f);
                r.secondaryBtnBounds = Rect2D(ctrlX + ctrlW - 60.0f, curY + 6.0f, 60.0f, 22.0f); // Value text
            } else if (p.type == eatscript::MacroParamType::Choice) {
                // Choice: render left/right navigation and active label
                r.secondaryBtnBounds = Rect2D(ctrlX, curY + 5.0f, 26.0f, 24.0f); // [<]
                r.controlBounds = Rect2D(ctrlX + 32.0f, curY + 5.0f, ctrlW - 64.0f, 24.0f); // label
                r.tertiaryBtnBounds = Rect2D(ctrlX + ctrlW - 26.0f, curY + 5.0f, 26.0f, 24.0f); // [>]
            } else if (p.type == eatscript::MacroParamType::IntStep) {
                r.secondaryBtnBounds = Rect2D(ctrlX, curY + 5.0f, 28.0f, 24.0f); // [-]
                r.controlBounds = Rect2D(ctrlX + 34.0f, curY + 5.0f, 90.0f, 24.0f); // Value
                r.tertiaryBtnBounds = Rect2D(ctrlX + 130.0f, curY + 5.0f, 28.0f, 24.0f); // [+]
                if (p.key == "Seed") {
                    r.optionPillBounds.push_back(Rect2D(ctrlX + 168.0f, curY + 5.0f, 90.0f, 24.0f)); // Dice button
                }
            } else if (p.type == eatscript::MacroParamType::BoolToggle) {
                r.controlBounds = Rect2D(ctrlX, curY + 5.0f, 90.0f, 24.0f);
            }

            paramRows_.push_back(r);
            curY += rowH;
        }
    }

    // 4. Footer action buttons
    const float footerY = y + h - 54.0f;
    randomizeBtnBounds_ = Rect2D(x + 24.0f, footerY, 130.0f, 34.0f);
    statusBannerBounds_ = Rect2D(x + 164.0f, footerY, w - 48.0f - 140.0f - 240.0f, 34.0f);
    cancelBtnBounds_ = Rect2D(x + w - 24.0f - 240.0f, footerY, 80.0f, 34.0f);
    runBtnBounds_ = Rect2D(x + w - 24.0f - 150.0f, footerY, 150.0f, 34.0f);
}

void MacroRunnerDialog::render(BatchRenderer2D& r, const ThemeTokens& theme) {
    if (!isOpen_) return;

    // 1. Semi-transparent backdrop overlay
    drawRect(r, 0.0f, 0.0f, 4000.0f, 4000.0f, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Dialog Chassis with Purple/Neon Accent Glow
    const Color macroAccent(0.74f, 0.0f, 1.0f, 1.0f); // Eatsbeats #BD00FF signature
    drawRoundedRect(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 14.0f, theme.panelBackground);
    drawRoundedRectOutline(r, dialogBounds_.x, dialogBounds_.y, dialogBounds_.w, dialogBounds_.h, 14.0f, macroAccent, 1.8f);

    // 3. Header Title & Category
    const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
    drawText(r, "PROCEDURAL DAW MACRO RUNNER", dialogBounds_.x + 24.0f, dialogBounds_.y + 20.0f, 13.0f, macroAccent);
    std::string sub = def ? (def->category + " Macro - " + def->name) : "Algorithmic song and track synthesis";
    drawText(r, sub, dialogBounds_.x + 24.0f, dialogBounds_.y + 36.0f, 9.5f, theme.textMuted);

    // Close button
    float clCenterX = closeBtnBounds_.x + closeBtnBounds_.w * 0.5f;
    float clCenterY = closeBtnBounds_.y + closeBtnBounds_.h * 0.5f;
    bool closeHov = closeBtnBounds_.contains(lastMouseX_, lastMouseY_);
    drawScrewCloseButton(r, clCenterX, clCenterY, 9.0f, closeHov, macroAccent);

    // 4. Macro Selector Bar
    const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
    for (size_t i = 0; i < macroPillBounds_.size() && i < macros.size(); ++i) {
        const auto& pill = macroPillBounds_[i];
        bool isSel = (static_cast<int>(i) == selectedMacroIdx_);
        bool isHov = pill.contains(lastMouseX_, lastMouseY_);

        Color pillBg = isSel ? macroAccent.withAlpha(0.28f) : (isHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.5f));
        drawRoundedRect(r, pill.x, pill.y, pill.w, pill.h, 6.0f, pillBg);
        if (isSel) {
            drawRoundedRectOutline(r, pill.x, pill.y, pill.w, pill.h, 6.0f, macroAccent, 1.2f);
        }

        std::string shortTitle = macros[i].name;
        if (shortTitle.length() > 14) shortTitle = shortTitle.substr(0, 12) + "..";
        Color textCol = isSel ? Color(1.0f, 1.0f, 1.0f, 1.0f) : (isHov ? theme.textPrimary : theme.textMuted);
        drawCenteredText(r, shortTitle, pill, 9.0f, textCol);
    }

    // 5. Description Box
    drawRoundedRect(r, descBoxBounds_.x, descBoxBounds_.y, descBoxBounds_.w, descBoxBounds_.h, 6.0f, theme.panelHeader.withAlpha(0.35f));
    if (def) {
        drawText(r, def->description, descBoxBounds_.x + 10.0f, descBoxBounds_.y + 11.0f, 9.5f, theme.textPrimary);
    }

    // 6. Parameter Rows
    for (const auto& row : paramRows_) {
        // Subtle row separator background on hover
        bool rowHov = row.rowBounds.contains(lastMouseX_, lastMouseY_);
        if (rowHov) {
            drawRoundedRect(r, row.rowBounds.x, row.rowBounds.y, row.rowBounds.w, row.rowBounds.h, 4.0f, Color(1.0f, 1.0f, 1.0f, 0.03f));
        }

        // Label
        drawText(r, row.key, row.labelBounds.x + 4.0f, row.labelBounds.y + 5.0f, 10.0f, theme.textPrimary);

        // Control per type
        if (row.type == eatscript::MacroParamType::Slider) {
            // Slider Track
            drawRoundedRect(r, row.controlBounds.x, row.controlBounds.y + 7.0f, row.controlBounds.w, 8.0f, 4.0f, theme.panelHeader);
            
            // Slider Fill
            float val = numValues_[row.key];
            float minVal = 0.0f, maxVal = 1.0f;
            if (def) {
                for (const auto& dp : def->params) {
                    if (dp.key == row.key) { minVal = dp.minVal; maxVal = dp.maxVal; break; }
                }
            }
            float norm = (maxVal > minVal) ? std::clamp((val - minVal) / (maxVal - minVal), 0.0f, 1.0f) : 0.0f;
            drawRoundedRect(r, row.controlBounds.x, row.controlBounds.y + 7.0f, row.controlBounds.w * norm, 8.0f, 4.0f, macroAccent);

            // Slider Thumb
            float thumbX = row.controlBounds.x + row.controlBounds.w * norm;
            drawCircle(r, thumbX, row.controlBounds.y + 11.0f, 7.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));

            // Numeric Readout
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << val;
            drawCenteredText(r, ss.str(), row.secondaryBtnBounds, 9.5f, theme.textMuted);
        } else if (row.type == eatscript::MacroParamType::Choice) {
            // Left Arrow [<]
            bool lHov = row.secondaryBtnBounds.contains(lastMouseX_, lastMouseY_);
            drawRoundedRect(r, row.secondaryBtnBounds.x, row.secondaryBtnBounds.y, row.secondaryBtnBounds.w, row.secondaryBtnBounds.h, 4.0f,
                            lHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.6f));
            drawCenteredText(r, "<", row.secondaryBtnBounds, 10.0f, theme.textPrimary);

            // Active Choice Label
            drawRoundedRect(r, row.controlBounds.x, row.controlBounds.y, row.controlBounds.w, row.controlBounds.h, 4.0f, theme.panelHeader.withAlpha(0.4f));
            std::string choiceStr = strValues_[row.key];
            drawCenteredText(r, choiceStr, row.controlBounds, 9.5f, macroAccent);

            // Right Arrow [>]
            bool rHov = row.tertiaryBtnBounds.contains(lastMouseX_, lastMouseY_);
            drawRoundedRect(r, row.tertiaryBtnBounds.x, row.tertiaryBtnBounds.y, row.tertiaryBtnBounds.w, row.tertiaryBtnBounds.h, 4.0f,
                            rHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.6f));
            drawCenteredText(r, ">", row.tertiaryBtnBounds, 10.0f, theme.textPrimary);
        } else if (row.type == eatscript::MacroParamType::IntStep) {
            // [-] Button
            bool mHov = row.secondaryBtnBounds.contains(lastMouseX_, lastMouseY_);
            drawRoundedRect(r, row.secondaryBtnBounds.x, row.secondaryBtnBounds.y, row.secondaryBtnBounds.w, row.secondaryBtnBounds.h, 4.0f,
                            mHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.6f));
            drawCenteredText(r, "-", row.secondaryBtnBounds, 11.0f, theme.textPrimary);

            // Number Value Display
            drawRoundedRect(r, row.controlBounds.x, row.controlBounds.y, row.controlBounds.w, row.controlBounds.h, 4.0f, theme.panelHeader.withAlpha(0.4f));
            int intVal = static_cast<int>(numValues_[row.key]);
            drawCenteredText(r, std::to_string(intVal), row.controlBounds, 10.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));

            // [+] Button
            bool pHov = row.tertiaryBtnBounds.contains(lastMouseX_, lastMouseY_);
            drawRoundedRect(r, row.tertiaryBtnBounds.x, row.tertiaryBtnBounds.y, row.tertiaryBtnBounds.w, row.tertiaryBtnBounds.h, 4.0f,
                            pHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.6f));
            drawCenteredText(r, "+", row.tertiaryBtnBounds, 11.0f, theme.textPrimary);

            // Seed Randomize Button (if present)
            if (!row.optionPillBounds.empty()) {
                const auto& dBtn = row.optionPillBounds[0];
                bool dHov = dBtn.contains(lastMouseX_, lastMouseY_);
                drawRoundedRect(r, dBtn.x, dBtn.y, dBtn.w, dBtn.h, 4.0f, dHov ? macroAccent.withAlpha(0.35f) : macroAccent.withAlpha(0.18f));
                drawCenteredText(r, "🎲 Roll", dBtn, 9.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
            }
        } else if (row.type == eatscript::MacroParamType::BoolToggle) {
            bool on = (numValues_[row.key] > 0.5f);
            Color togBg = on ? macroAccent.withAlpha(0.35f) : theme.panelHeader;
            drawRoundedRect(r, row.controlBounds.x, row.controlBounds.y, row.controlBounds.w, row.controlBounds.h, 4.0f, togBg);
            drawCenteredText(r, on ? "ENABLED" : "DISABLED", row.controlBounds, 9.0f, on ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted);
        }
    }

    // 7. Footer Action Buttons
    // [🎲 Randomize All]
    bool rHov = randomizeBtnBounds_.contains(lastMouseX_, lastMouseY_);
    drawRoundedRect(r, randomizeBtnBounds_.x, randomizeBtnBounds_.y, randomizeBtnBounds_.w, randomizeBtnBounds_.h, 6.0f,
                    rHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.6f));
    drawCenteredText(r, "🎲 Randomize All", randomizeBtnBounds_, 9.5f, theme.textPrimary);

    // Status message
    if (!statusMessage_.empty()) {
        Color sCol = isStatusError_ ? Color(1.0f, 0.3f, 0.3f, 1.0f) : Color(0.2f, 1.0f, 0.5f, 1.0f);
        drawCenteredText(r, statusMessage_, statusBannerBounds_, 9.5f, sCol);
    }

    // [Cancel]
    bool cHov = cancelBtnBounds_.contains(lastMouseX_, lastMouseY_);
    drawRoundedRect(r, cancelBtnBounds_.x, cancelBtnBounds_.y, cancelBtnBounds_.w, cancelBtnBounds_.h, 6.0f,
                    cHov ? theme.panelHeader : theme.panelHeader.withAlpha(0.5f));
    drawCenteredText(r, "Cancel", cancelBtnBounds_, 10.0f, theme.textMuted);

    // [✨ Run Macro & Generate]
    bool runHov = runBtnBounds_.contains(lastMouseX_, lastMouseY_);
    Color runBg = runHov ? macroAccent : macroAccent.withAlpha(0.85f);
    drawRoundedRect(r, runBtnBounds_.x, runBtnBounds_.y, runBtnBounds_.w, runBtnBounds_.h, 6.0f, runBg);
    drawCenteredText(r, "✨ Run Macro", runBtnBounds_, 11.0f, Color(1.0f, 1.0f, 1.0f, 1.0f));
}

bool MacroRunnerDialog::handlePointer(const PointerEvent& ev) {
    if (!isOpen_) return false;

    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    if (ev.action == PointerAction::Down && ev.button == PointerButton::Left) {
        // 1. Close button
        if (closeBtnBounds_.contains(ev.x, ev.y)) {
            close();
            if (onClose) onClose();
            return true;
        }

        // 2. Macro switcher pills
        for (size_t i = 0; i < macroPillBounds_.size(); ++i) {
            if (macroPillBounds_[i].contains(ev.x, ev.y)) {
                const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
                if (i < macros.size()) {
                    selectMacro(macros[i].id);
                }
                return true;
            }
        }

        // 3. Randomize All
        if (randomizeBtnBounds_.contains(ev.x, ev.y)) {
            randomizeAll();
            return true;
        }

        // 4. Cancel button
        if (cancelBtnBounds_.contains(ev.x, ev.y)) {
            close();
            if (onClose) onClose();
            return true;
        }

        // 5. Run Macro Button
        if (runBtnBounds_.contains(ev.x, ev.y)) {
            const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
            if (def && onRunRequested) {
                onRunRequested(*def, numValues_, strValues_);
            }
            return true;
        }

        // 6. Parameter rows interaction
        const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
        for (size_t i = 0; i < paramRows_.size(); ++i) {
            auto& row = paramRows_[i];

            if (row.type == eatscript::MacroParamType::Slider) {
                if (row.controlBounds.contains(ev.x, ev.y)) {
                    draggingSliderIdx_ = static_cast<int>(i);
                    float norm = std::clamp((ev.x - row.controlBounds.x) / row.controlBounds.w, 0.0f, 1.0f);
                    if (def && i < def->params.size()) {
                        numValues_[row.key] = def->params[i].minVal + norm * (def->params[i].maxVal - def->params[i].minVal);
                    }
                    return true;
                }
            } else if (row.type == eatscript::MacroParamType::Choice) {
                if (def && i < def->params.size() && !def->params[i].options.empty()) {
                    const auto& opts = def->params[i].options;
                    int curIdx = static_cast<int>(numValues_[row.key]);

                    if (row.secondaryBtnBounds.contains(ev.x, ev.y)) { // [<]
                        curIdx = (curIdx - 1 + static_cast<int>(opts.size())) % static_cast<int>(opts.size());
                        numValues_[row.key] = static_cast<float>(curIdx);
                        strValues_[row.key] = opts[curIdx];
                        return true;
                    }
                    if (row.tertiaryBtnBounds.contains(ev.x, ev.y)) { // [>]
                        curIdx = (curIdx + 1) % static_cast<int>(opts.size());
                        numValues_[row.key] = static_cast<float>(curIdx);
                        strValues_[row.key] = opts[curIdx];
                        return true;
                    }
                }
            } else if (row.type == eatscript::MacroParamType::IntStep) {
                if (def && i < def->params.size()) {
                    const auto& dp = def->params[i];
                    if (row.secondaryBtnBounds.contains(ev.x, ev.y)) { // [-]
                        numValues_[row.key] = std::max(dp.minVal, numValues_[row.key] - dp.step);
                        return true;
                    }
                    if (row.tertiaryBtnBounds.contains(ev.x, ev.y)) { // [+]
                        numValues_[row.key] = std::min(dp.maxVal, numValues_[row.key] + dp.step);
                        return true;
                    }
                    if (!row.optionPillBounds.empty() && row.optionPillBounds[0].contains(ev.x, ev.y)) { // [🎲 Roll]
                        randomizeSeed();
                        return true;
                    }
                }
            } else if (row.type == eatscript::MacroParamType::BoolToggle) {
                if (row.controlBounds.contains(ev.x, ev.y)) {
                    numValues_[row.key] = (numValues_[row.key] > 0.5f) ? 0.0f : 1.0f;
                    return true;
                }
            }
        }

        // Click outside dialog chassis closes
        if (!dialogBounds_.contains(ev.x, ev.y)) {
            close();
            if (onClose) onClose();
            return true;
        }
    } else if (ev.action == PointerAction::Move) {
        if (draggingSliderIdx_ >= 0 && draggingSliderIdx_ < static_cast<int>(paramRows_.size())) {
            const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
            const auto& row = paramRows_[draggingSliderIdx_];
            float norm = std::clamp((ev.x - row.controlBounds.x) / row.controlBounds.w, 0.0f, 1.0f);
            if (def && draggingSliderIdx_ < static_cast<int>(def->params.size())) {
                numValues_[row.key] = def->params[draggingSliderIdx_].minVal + norm * (def->params[draggingSliderIdx_].maxVal - def->params[draggingSliderIdx_].minVal);
            }
            return true;
        }
    } else if (ev.action == PointerAction::Up) {
        draggingSliderIdx_ = -1;
    }

    return true; // Modal captures pointer
}

bool MacroRunnerDialog::handleKey(int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;
    if (!isOpen_) return false;

    if (action == 1) { // GLFW_PRESS
        if (key == 256) { // GLFW_KEY_ESCAPE
            close();
            if (onClose) onClose();
            return true;
        }
        if (key == 257) { // GLFW_KEY_ENTER
            const auto* def = eatscript::MacroRuntime::getMacroById(selectedMacroId_);
            if (def && onRunRequested) {
                onRunRequested(*def, numValues_, strValues_);
            }
            return true;
        }
    }
    return true;
}

bool MacroRunnerDialog::handleChar(unsigned int codepoint) {
    (void)codepoint;
    return isOpen_;
}

} // namespace eatsbits::ui
