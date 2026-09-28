#ifndef EATS_NOTE_SPLITTER_DIALOG_HPP
#define EATS_NOTE_SPLITTER_DIALOG_HPP

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "eatsbits/sequencer/note_splitter_engine.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

/**
 * NoteSplitterDialog: Modal dialog to preview and execute polyphonic chord voice
 * distribution into separate melodic tracks (Lead, Harmony, Bass, Clefs, Drums).
 */
class NoteSplitterDialog {
public:
    NoteSplitterDialog();
    ~NoteSplitterDialog() = default;

    void open(size_t sourceTrackIdx, const std::string& trackName, const std::vector<eatscript::MidiNote>& notes);
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] size_t getSourceTrackIndex() const noexcept { return sourceTrackIndex_; }
    [[nodiscard]] sequencer::SplitMode getSplitMode() const noexcept { return mode_; }
    [[nodiscard]] const sequencer::SplitParams& getParams() const noexcept { return params_; }
    [[nodiscard]] bool getRemoveSourceTrack() const noexcept { return removeSourceTrack_; }
    [[nodiscard]] const std::vector<sequencer::SplitTrackResult>& getPreviewResults() const noexcept { return previewResults_; }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }

    std::function<void(size_t sourceTrackIdx, sequencer::SplitMode mode, const sequencer::SplitParams& params, bool removeSource)> onSplitConfirmed;

private:
    void updatePreview();

    bool isOpen_{false};
    size_t sourceTrackIndex_{0};
    std::string sourceTrackName_{""};
    std::vector<eatscript::MidiNote> sourceNotes_{};

    sequencer::SplitMode mode_{sequencer::SplitMode::ThreeWayVoice};
    sequencer::SplitParams params_{};
    bool removeSourceTrack_{false};

    std::vector<sequencer::SplitTrackResult> previewResults_{};

    Rect2D dialogBounds_{};
    std::vector<Rect2D> modeTabBounds_{};
    Rect2D previewCardBounds_{};
    Rect2D paramSlider1Bounds_{};
    Rect2D paramSlider2Bounds_{};
    Rect2D removeToggleBounds_{};
    Rect2D cancelBtnBounds_{};
    Rect2D splitBtnBounds_{};

    bool isDraggingSlider1_{false};
    bool isDraggingSlider2_{false};
};

} // namespace eatsbits::ui

#endif // EATS_NOTE_SPLITTER_DIALOG_HPP
