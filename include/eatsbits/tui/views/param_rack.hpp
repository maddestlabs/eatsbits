#pragma once

#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <string>
#include <vector>

namespace eatsbits::tui {

struct ParamItem {
    std::string name;
    float value{0.5f};
    float minVal{0.0f};
    float maxVal{1.0f};
    std::string unit;
    float step{0.05f};
};

class ParamRackView {
public:
    ParamRackView();
    ~ParamRackView() = default;

    void render(CellSurface& surface, const Rect& area, audio::AudioEngine& engine, bool isFocused);

    void moveSelection(int delta);
    void adjustValue(int steps, audio::AudioEngine& engine);

    size_t getSelectedIndex() const { return selectedIndex_; }

private:
    std::vector<ParamItem> params_;
    size_t selectedIndex_{0};

    void syncParams(audio::AudioEngine& engine);
    void applyParam(size_t index, audio::AudioEngine& engine);
};

} // namespace eatsbits::tui
