#include "eatsbits/tui/views/param_rack.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::tui {

ParamRackView::ParamRackView() {
    params_ = {
        presenter::ParameterPresenter({"BPM", "BPM", "bpm", 60.0f, 240.0f, 132.0f, 1.0f, false, false}),
        presenter::ParameterPresenter({"303 CUTOFF", "303 CUTOFF", "Hz", 100.0f, 6000.0f, 850.0f, 50.0f, false, false}),
        presenter::ParameterPresenter({"303 RESO", "303 RESO", "", 0.0f, 0.98f, 0.82f, 0.02f, false, true}),
        presenter::ParameterPresenter({"SWING", "SWING", "", 0.50f, 0.75f, 0.55f, 0.01f, false, true}),
        presenter::ParameterPresenter({"MASTER VOL", "MASTER VOL", "", 0.0f, 1.0f, 0.85f, 0.05f, false, true})
    };
}

void ParamRackView::render(CellSurface& surface, const Rect& area, audio::AudioEngine& engine, bool isFocused) {
    (void)engine;
    if (area.width < 25 || area.height < 6) return;

    Color frameCol = isEditing_ ? Color::NeonGreen() : (isFocused ? Color::AcidAmber() : Color::Gray());
    std::string title = isEditing_ ? "HARDWARE PARAM RACK [EDITING VALUE]" :
                        (isFocused ? "HARDWARE PARAM RACK [ACTIVE]" : "HARDWARE PARAM RACK");
    surface.drawBox(area, frameCol, Color::Black(), isFocused || isEditing_, title);

    int startY = area.y + 1;
    const int sliderWidth = std::max(6, area.width - 26);

    for (size_t i = 0; i < params_.size(); ++i) {
        int rowY = startY + static_cast<int>(i);
        if (rowY >= area.y + area.height - 2) break;

        const auto& p = params_[i];
        bool isCursor = (isFocused || isEditing_) && (i == selectedIndex_);

        Color labelCol = isCursor ? (isEditing_ ? Color::NeonGreen() : Color::AcidAmber()) : Color::White();
        uint8_t attrs = isCursor ? static_cast<uint8_t>(TextAttr::Bold) : 0;

        // Parameter Name / Label
        std::string label = p.getLabel();
        while (label.size() < 12) label.push_back(' ');
        surface.drawText(area.x + 2, rowY, label, labelCol, Color::Black(), attrs);

        // Slider [██████░░░░]
        float norm = p.getNormalizedValue();
        int filled = static_cast<int>(norm * static_cast<float>(sliderWidth));

        int sliderX = area.x + 15;
        surface.drawText(sliderX, rowY, "[", Color::DarkGray(), Color::Black());
        for (int s = 0; s < sliderWidth; ++s) {
            Color barCol = isCursor ? (isEditing_ ? Color::NeonGreen() : Color::AcidAmber()) : Color::CyberCyan();
            if (s < filled) {
                surface.setCell(sliderX + 1 + s, rowY, Cell{0x2588, barCol, Color::Black(), 0});
            } else {
                surface.setCell(sliderX + 1 + s, rowY, Cell{0x2591, Color::DarkGray(), Color::Black(), 0});
            }
        }
        surface.drawText(sliderX + 1 + sliderWidth, rowY, "]", Color::DarkGray(), Color::Black());

        // Value or Active Edit Field
        if (isCursor && isEditing_) {
            std::string editPrompt = "> " + editBuffer_ + "_";
            auto valRes = editPresenter_.validate(editBuffer_);
            Color editColor = valRes.isValid ? Color::NeonGreen() : Color::DangerRed();
            surface.drawText(sliderX + sliderWidth + 3, rowY, editPrompt, editColor, Color::Black(), static_cast<uint8_t>(TextAttr::Bold));
        } else {
            const std::string& valText = p.getFormattedValue();
            surface.drawText(sliderX + sliderWidth + 3, rowY, valText, isCursor ? Color::White() : Color::LightGray(), Color::Black());
        }
    }

    // Contextual Footer hint
    int hintY = area.y + area.height - 2;
    if (isEditing_) {
        surface.drawText(area.x + 2, hintY, "[ENTER] Commit  [ESC] Cancel  [%] Percentage", Color::NeonGreen(), Color::Black());
    } else if (isFocused) {
        surface.drawText(area.x + 2, hintY, "[E/ENTER] Edit  [←/→] Step  [↑/↓] Select", Color::Gray(), Color::Black());
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
