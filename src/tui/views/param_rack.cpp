#include "eatsbits/tui/views/param_rack.hpp"
#include <iomanip>
#include <sstream>
#include <cmath>

namespace eatsbits::tui {

ParamRackView::ParamRackView() {
    params_ = {
        {"BPM", 132.0f, 60.0f, 240.0f, "bpm", 1.0f},
        {"303 CUTOFF", 850.0f, 100.0f, 6000.0f, "Hz", 50.0f},
        {"303 RESO", 0.82f, 0.0f, 0.98f, "", 0.02f},
        {"SWING", 0.55f, 0.50f, 0.75f, "", 0.01f},
        {"MASTER VOL", 0.85f, 0.0f, 1.0f, "", 0.05f}
    };
}

void ParamRackView::render(CellSurface& surface, const Rect& area, audio::AudioEngine& engine, bool isFocused) {
    (void)engine;
    if (area.width < 25 || area.height < 6) return;

    Color frameCol = isFocused ? Color::AcidAmber() : Color::Gray();
    std::string title = isFocused ? "HARDWARE PARAM RACK [ACTIVE]" : "HARDWARE PARAM RACK";
    surface.drawBox(area, frameCol, Color::Black(), isFocused, title);

    int startY = area.y + 1;
    const int sliderWidth = std::max(6, area.width - 24);

    for (size_t i = 0; i < params_.size(); ++i) {
        int rowY = startY + static_cast<int>(i);
        if (rowY >= area.y + area.height - 1) break;

        const auto& p = params_[i];
        bool isCursor = isFocused && (i == selectedIndex_);

        Color labelCol = isCursor ? Color::AcidAmber() : Color::White();
        uint8_t attrs = isCursor ? static_cast<uint8_t>(TextAttr::Bold) : 0;

        // Label
        std::string label = p.name;
        while (label.size() < 12) label.push_back(' ');
        surface.drawText(area.x + 2, rowY, label, labelCol, Color::Black(), attrs);

        // Slider [██████░░░░]
        float norm = (p.value - p.minVal) / (p.maxVal - p.minVal);
        norm = std::clamp(norm, 0.0f, 1.0f);
        int filled = static_cast<int>(norm * static_cast<float>(sliderWidth));

        int sliderX = area.x + 15;
        surface.drawText(sliderX, rowY, "[", Color::DarkGray(), Color::Black());
        for (int s = 0; s < sliderWidth; ++s) {
            Color barCol = isCursor ? Color::AcidAmber() : Color::CyberCyan();
            if (s < filled) {
                surface.setCell(sliderX + 1 + s, rowY, Cell{0x2588, barCol, Color::Black(), 0});
            } else {
                surface.setCell(sliderX + 1 + s, rowY, Cell{0x2591, Color::DarkGray(), Color::Black(), 0});
            }
        }
        surface.drawText(sliderX + 1 + sliderWidth, rowY, "]", Color::DarkGray(), Color::Black());

        // Value text
        char valBuf[16];
        if (p.value >= 10.0f) {
            snprintf(valBuf, sizeof(valBuf), "%5.0f %s", p.value, p.unit.c_str());
        } else {
            snprintf(valBuf, sizeof(valBuf), "%5.2f %s", p.value, p.unit.c_str());
        }
        surface.drawText(sliderX + sliderWidth + 3, rowY, valBuf, isCursor ? Color::White() : Color::LightGray(), Color::Black());
    }
}

void ParamRackView::moveSelection(int delta) {
    if (params_.empty()) return;
    int next = static_cast<int>(selectedIndex_) + delta;
    if (next < 0) next = 0;
    if (next >= static_cast<int>(params_.size())) next = static_cast<int>(params_.size()) - 1;
    selectedIndex_ = static_cast<size_t>(next);
}

void ParamRackView::adjustValue(int steps, audio::AudioEngine& engine) {
    if (selectedIndex_ >= params_.size()) return;
    auto& p = params_[selectedIndex_];
    p.value += static_cast<float>(steps) * p.step;
    p.value = std::clamp(p.value, p.minVal, p.maxVal);
    applyParam(selectedIndex_, engine);
}

void ParamRackView::applyParam(size_t index, audio::AudioEngine& engine) {
    if (index >= params_.size()) return;
    const auto& p = params_[index];
    if (p.name == "BPM") {
        engine.getSequencer().setBpm(static_cast<double>(p.value));
    } else if (p.name == "303 CUTOFF") {
        engine.setCutoff(p.value);
    } else if (p.name == "303 RESO") {
        engine.setResonance(p.value);
    } else if (p.name == "SWING") {
        engine.getSequencer().setSwing(static_cast<double>(p.value));
    } else if (p.name == "MASTER VOL") {
        engine.setMasterVolume(p.value);
    }
}

} // namespace eatsbits::tui
