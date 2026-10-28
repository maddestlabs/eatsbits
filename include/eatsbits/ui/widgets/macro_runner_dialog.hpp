#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "eatsbits/eatscript/macro_runtime.hpp"

namespace eatsbits::ui {

/**
 * Procedural Macro Runner & Generative Parameter Dialog.
 * Provides interactive UI controls (choice pills, sliders, steppers, seed randomizers)
 * for configuring and executing Eatsbits procedural song, drum, acid, and music theory macros.
 */
class MacroRunnerDialog {
public:
    MacroRunnerDialog();
    ~MacroRunnerDialog() = default;

    void open(const std::string& macroId = "macro_song_gen");
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void update(float dt);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);

    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);
    bool handleChar(unsigned int codepoint);

    void selectMacro(const std::string& macroId);
    [[nodiscard]] const std::string& getSelectedMacroId() const noexcept { return selectedMacroId_; }

    // Callbacks
    std::function<void(const eatscript::MacroDefinition& def,
                       const std::map<std::string, float>& numParams,
                       const std::map<std::string, std::string>& strParams)> onRunRequested;
    std::function<void()> onClose;

    void setStatus(const std::string& statusMsg, bool isError = false);

private:
    void randomizeSeed();
    void randomizeAll();
    void resetToDefaults();

    bool isOpen_{false};
    std::string selectedMacroId_{"macro_song_gen"};
    int selectedMacroIdx_{0};

    // Stored parameter values
    std::map<std::string, float> numValues_;
    std::map<std::string, std::string> strValues_;

    // Interactive Drag / State
    int draggingSliderIdx_{-1};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};

    // Status Message / Execution Feedback
    std::string statusMessage_;
    bool isStatusError_{false};
    float statusFadeTimer_{0.0f};

    // Layout Bounds
    Rect2D dialogBounds_{};
    Rect2D closeBtnBounds_{};
    Rect2D macroSelectBarBounds_{};
    std::vector<Rect2D> macroPillBounds_{};
    Rect2D descBoxBounds_{};
    Rect2D paramsAreaBounds_{};

    // Per-parameter visual bounds
    struct ParamVisualRow {
        std::string key;
        eatscript::MacroParamType type;
        Rect2D rowBounds;
        Rect2D labelBounds;
        Rect2D controlBounds;
        Rect2D secondaryBtnBounds; // e.g. [-] or Dice
        Rect2D tertiaryBtnBounds;  // e.g. [+]
        std::vector<Rect2D> optionPillBounds;
    };
    std::vector<ParamVisualRow> paramRows_;

    // Action button bounds
    Rect2D randomizeBtnBounds_{};
    Rect2D cancelBtnBounds_{};
    Rect2D runBtnBounds_{};
    Rect2D statusBannerBounds_{};
};

} // namespace eatsbits::ui
