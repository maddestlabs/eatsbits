#pragma once

#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/input_parser.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/presenter/parameter_presenter.hpp"
#include "eatsbits/presenter/value_edit_presenter.hpp"
#include <string>
#include <vector>

namespace eatsbits::tui {

class ParamRackView {
public:
    ParamRackView();
    ~ParamRackView() = default;

    void render(CellSurface& surface, const Rect& area, audio::AudioEngine& engine, bool isFocused);

    void moveSelection(int delta);
    void adjustValue(int steps, audio::AudioEngine& engine);

    [[nodiscard]] bool isEditing() const noexcept { return isEditing_; }
    void startEdit(audio::AudioEngine& engine);
    void cancelEdit();
    void commitEdit(audio::AudioEngine& engine);
    bool handleKey(const KeyEvent& key, audio::AudioEngine& engine);

    [[nodiscard]] size_t getSelectedIndex() const noexcept { return selectedIndex_; }
    [[nodiscard]] size_t getParamCount() const noexcept { return params_.size(); }
    [[nodiscard]] const presenter::ParameterPresenter* getSelectedParam() const noexcept {
        return (selectedIndex_ < params_.size()) ? &params_[selectedIndex_] : nullptr;
    }
    [[nodiscard]] const std::string& getEditBuffer() const noexcept { return editBuffer_; }

private:
    std::vector<presenter::ParameterPresenter> params_;
    size_t selectedIndex_{0};

    bool isEditing_{false};
    std::string editBuffer_{""};
    presenter::ValueEditPresenter editPresenter_{};

    void applyParam(size_t index, audio::AudioEngine& engine);
};

} // namespace eatsbits::tui
