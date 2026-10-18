#include "eatsbits/tui/views/param_rack.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "clay.h"
#include <algorithm>
#include <cmath>

namespace eatsbits::tui {

ParamRackView::ParamRackView() {
    clayContext_ = std::make_unique<ui::clay::ClayContext>();
    clayContext_->initialize(60.0f, 20.0f);
    clayContext_->setDefaultMonospaceMeasure(1.0f, 1.0f);

    params_ = {
        presenter::ParameterPresenter({"BPM", "BPM", "bpm", 60.0f, 240.0f, 132.0f, 1.0f, false, false}),
        presenter::ParameterPresenter({"303 CUTOFF", "303 CUTOFF", "Hz", 100.0f, 6000.0f, 850.0f, 50.0f, false, false}),
        presenter::ParameterPresenter({"303 RESO", "303 RESO", "", 0.0f, 0.98f, 0.82f, 0.02f, false, true}),
        presenter::ParameterPresenter({"SWING", "SWING", "", 0.50f, 0.75f, 0.55f, 0.01f, false, true}),
        presenter::ParameterPresenter({"MASTER VOL", "MASTER VOL", "", 0.0f, 1.0f, 0.85f, 0.05f, false, true})
    };
}

ParamRackView::~ParamRackView() = default;

void ParamRackView::render(CellSurface& surface, const Rect& area, audio::AudioEngine& engine, bool isFocused) {
    (void)engine;
    if (area.width < 25 || area.height < 6) return;

    Color frameCol = isEditing_ ? Color::NeonGreen() : (isFocused ? Color::AcidAmber() : Color::Gray());
    std::string title = isEditing_ ? "HARDWARE PARAM RACK [EDITING VALUE]" :
                        (isFocused ? "HARDWARE PARAM RACK [ACTIVE]" : "HARDWARE PARAM RACK");
    surface.drawBox(area, frameCol, Color::Black(), isFocused || isEditing_, title);

    if (!clayContext_ || !clayContext_->isInitialized()) {
        return;
    }

    clayContext_->setDimensions(static_cast<float>(area.width), static_cast<float>(area.height));
    clayContext_->beginLayout();

    CLAY(CLAY_ID("ParamRackRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(static_cast<float>(area.width)), CLAY_SIZING_FIXED(static_cast<float>(area.height)) },
            .padding = { 2, 2, 1, 1 },
            .childGap = 0,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }
    }) {
        // Parameter list container (flex grow to push footer to bottom)
        CLAY(CLAY_ID("ParamRowsContainer"), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .childGap = 0,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            }
        }) {
            for (size_t i = 0; i < params_.size(); ++i) {
                const auto& p = params_[i];
                bool isCursor = (isFocused || isEditing_) && (i == selectedIndex_);
                Color labelCol = isCursor ? (isEditing_ ? Color::NeonGreen() : Color::AcidAmber()) : Color::White();

                std::string label = p.getLabel();
                while (label.size() < 12) label.push_back(' ');

                std::string valText;
                Color valColor = Color::LightGray();
                if (isCursor && isEditing_) {
                    valText = "> " + editBuffer_ + "_";
                    auto valRes = editPresenter_.validate(editBuffer_);
                    valColor = valRes.isValid ? Color::NeonGreen() : Color::DangerRed();
                } else {
                    valText = p.getFormattedValue();
                    valColor = isCursor ? Color::White() : Color::LightGray();
                }

                CLAY(CLAY_IDI("ParamRow", static_cast<uint32_t>(i)), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1.0f) },
                        .childGap = 1,
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    }
                }) {
                    CLAY(CLAY_IDI("ParamLabel", static_cast<uint32_t>(i)), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(13.0f), CLAY_SIZING_FIXED(1.0f) } }
                    }) {
                        CLAY_TEXT(ClayTuiBridge::toClayString(label), CLAY_TEXT_CONFIG({
                            .textColor = ClayTuiBridge::toClayColor(labelCol)
                        }));
                    }

                    // Flexible Slider Slot
                    CLAY(CLAY_IDI("ParamSlider", static_cast<uint32_t>(i)), {
                        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1.0f) } }
                    }) {}

                    // Formatted Value / Edit Field
                    CLAY(CLAY_IDI("ParamVal", static_cast<uint32_t>(i)), {
                        .layout = { .sizing = { CLAY_SIZING_FIXED(10.0f), CLAY_SIZING_FIXED(1.0f) } }
                    }) {
                        CLAY_TEXT(ClayTuiBridge::toClayString(valText), CLAY_TEXT_CONFIG({
                            .textColor = ClayTuiBridge::toClayColor(valColor)
                        }));
                    }
                }
            }
        }

        // Contextual Footer hint
        std::string hint = isEditing_ ? "[ENTER] Commit  [ESC] Cancel  [%] Percentage" :
                           (isFocused ? "[E/ENTER] Edit  [←/→] Step  [↑/↓] Select" : "");
        if (!hint.empty()) {
            Color hintCol = isEditing_ ? Color::NeonGreen() : Color::Gray();
            CLAY(CLAY_ID("ParamRackFooter"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1.0f) } }
            }) {
                CLAY_TEXT(ClayTuiBridge::toClayString(hint), CLAY_TEXT_CONFIG({
                    .textColor = ClayTuiBridge::toClayColor(hintCol)
                }));
            }
        }
    }

    Clay_RenderCommandArray cmds = clayContext_->endLayout(0.016f);
    clayBridge_.render(cmds, surface, area.x, area.y);

    // Draw the custom discrete block sliders inside the resolved Clay bounding boxes
    for (size_t i = 0; i < params_.size(); ++i) {
        auto sliderData = clayContext_->getElementData(CLAY_IDI("ParamSlider", static_cast<uint32_t>(i)));
        if (!sliderData.found) continue;

        Rect sliderRect = ClayTuiBridge::snapToCells(sliderData.boundingBox);
        sliderRect.x += area.x;
        sliderRect.y += area.y;

        int sliderW = sliderRect.width - 2;
        if (sliderW < 2) continue;

        const auto& p = params_[i];
        bool isCursor = (isFocused || isEditing_) && (i == selectedIndex_);
        Color barCol = isCursor ? (isEditing_ ? Color::NeonGreen() : Color::AcidAmber()) : Color::CyberCyan();

        surface.drawText(sliderRect.x, sliderRect.y, "[", Color::DarkGray(), Color::Black());
        float norm = p.getNormalizedValue();
        int filled = static_cast<int>(norm * static_cast<float>(sliderW));
        for (int s = 0; s < sliderW; ++s) {
            if (s < filled) {
                surface.setCell(sliderRect.x + 1 + s, sliderRect.y, Cell{0x2588, barCol, Color::Black(), 0});
            } else {
                surface.setCell(sliderRect.x + 1 + s, sliderRect.y, Cell{0x2591, Color::DarkGray(), Color::Black(), 0});
            }
        }
        surface.drawText(sliderRect.x + 1 + sliderW, sliderRect.y, "]", Color::DarkGray(), Color::Black());
    }
}

void ParamRackView::moveSelection(int delta) {
    if (params_.empty() || isEditing_) return;
    int next = static_cast<int>(selectedIndex_) + delta;
    if (next < 0) next = 0;
    if (next >= static_cast<int>(params_.size())) next = static_cast<int>(params_.size()) - 1;
    selectedIndex_ = static_cast<size_t>(next);
}

void ParamRackView::adjustValue(int steps, audio::AudioEngine& engine) {
    if (selectedIndex_ >= params_.size() || isEditing_) return;
    auto& p = params_[selectedIndex_];
    p.step(steps);
    applyParam(selectedIndex_, engine);
}

void ParamRackView::startEdit(audio::AudioEngine& engine) {
    if (selectedIndex_ >= params_.size()) return;
    isEditing_ = true;
    auto& param = params_[selectedIndex_];
    editPresenter_.bindToParameter(param, [this, &engine]() {
        applyParam(selectedIndex_, engine);
        isEditing_ = false;
    });
    editBuffer_ = editPresenter_.getInitialEditText();
}

void ParamRackView::cancelEdit() {
    if (!isEditing_) return;
    editPresenter_.cancel();
    isEditing_ = false;
    editBuffer_.clear();
}

void ParamRackView::commitEdit(audio::AudioEngine& engine) {
    if (!isEditing_) return;
    editPresenter_.submit(editBuffer_);
    applyParam(selectedIndex_, engine);
    isEditing_ = false;
    editBuffer_.clear();
}

bool ParamRackView::handleKey(const KeyEvent& key, audio::AudioEngine& engine) {
    if (!isEditing_) return false;

    if (key.code == KeyCode::Escape) {
        cancelEdit();
        return true;
    }
    if (key.code == KeyCode::Enter) {
        commitEdit(engine);
        return true;
    }
    if (key.code == KeyCode::Backspace) {
        if (!editBuffer_.empty()) {
            editBuffer_.pop_back();
        }
        return true;
    }
    if (key.codepoint >= 32 && key.codepoint <= 126) {
        char c = static_cast<char>(key.codepoint);
        editBuffer_.push_back(c);
        return true;
    }

    return false;
}

void ParamRackView::applyParam(size_t index, audio::AudioEngine& engine) {
    if (index >= params_.size()) return;
    const auto& p = params_[index];
    const std::string& name = p.getName();
    float val = p.getValue();

    if (name == "BPM") {
        engine.getSequencer().setBpm(static_cast<double>(val));
    } else if (name == "303 CUTOFF") {
        engine.setCutoff(val);
    } else if (name == "303 RESO") {
        engine.setResonance(val);
    } else if (name == "SWING") {
        engine.getSequencer().setSwing(static_cast<double>(val));
    } else if (name == "MASTER VOL") {
        engine.setMasterVolume(val);
    }
}

} // namespace eatsbits::tui
