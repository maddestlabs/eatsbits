#pragma once

#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../input/pointer_event.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include <string>
#include <vector>
#include <functional>

namespace eatsbits::ui {

enum class CircleDialogTab {
    Wheel,
    Presets,
    MidiExtract
};

/**
 * Studio One / Eatsbeats style interactive Circle of Fifths modal dialog.
 * Features:
 * - Dual-ring 24-sector Circle of Fifths wheel (Outer: Major, Inner: Relative Minor)
 * - Click sector to select root & quality with instant auditioning
 * - Full 14 Chord Qualities & Extensions matrix
 * - Inversion / Slash Bass note selector
 * - Duration in bars selector (0.5b, 1b, 2b, 4b)
 * - Curated 28-preset chord progression library with Roman numeral summaries
 * - MIDI to Chord Track extraction tools
 */
class CircleOfFifthsDialog {
public:
    CircleOfFifthsDialog();
    ~CircleOfFifthsDialog() = default;

    void open(uint32_t targetBar,
              int songKeyRoot = 0,
              bool isSongKeyMinor = false,
              const std::optional<theory::ChordEvent>& initialChord = std::nullopt);
    void openForChord(const theory::ChordEvent& chord, int songKeyRoot = 0, bool isSongKeyMinor = false) {
        open(chord.startBar, songKeyRoot, isSongKeyMinor, chord);
    }
    void close() noexcept { isOpen_ = false; }
    [[nodiscard]] bool isOpen() const noexcept { return isOpen_; }

    void layout(float screenW, float screenH);
    void render(BatchRenderer2D& r, const ThemeTokens& theme);
    bool handlePointer(const PointerEvent& ev);
    bool handleKey(int key, int scancode, int action, int mods);

    [[nodiscard]] uint32_t getTargetBar() const noexcept { return targetBar_; }
    [[nodiscard]] const theory::ChordEvent& getCurrentChord() const noexcept { return currentChord_; }
    [[nodiscard]] int getSelectedRoot() const noexcept { return selectedRoot_; }
    void setSelectedRoot(int root) noexcept { selectedRoot_ = root; updateCurrentChord(); }
    [[nodiscard]] theory::ChordQuality getSelectedQuality() const noexcept { return selectedQuality_; }
    void setSelectedQuality(theory::ChordQuality q) noexcept { selectedQuality_ = q; updateCurrentChord(); }
    [[nodiscard]] bool isMinorRingSelected() const noexcept {
        return selectedQuality_ == theory::ChordQuality::Minor ||
               selectedQuality_ == theory::ChordQuality::Minor7 ||
               selectedQuality_ == theory::ChordQuality::Min9;
    }
    [[nodiscard]] const Rect2D& getBounds() const noexcept { return dialogBounds_; }
    void auditionCurrentChord() {
        if (onAuditionChord) onAuditionChord(currentChord_);
    }

    std::function<void(const theory::ChordEvent& chord)> onChordApplied;
    std::function<void(const std::string& chordId)> onChordDeleted;
    std::function<void(const theory::ChordProgressionPreset& preset, uint32_t startBar)> onProgressionApplied;
    std::function<void(const theory::ChordEvent& chord)> onAuditionChord;
    std::function<void()> onExtractFromActiveTrack;
    std::function<void()> onExtractFromActiveClip;
    std::function<void()> onClose;

private:
    void updateCurrentChord();
    void renderWheelTab(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderPresetsTab(BatchRenderer2D& r, const ThemeTokens& theme);
    void renderExtractTab(BatchRenderer2D& r, const ThemeTokens& theme);
    void handleWheelClick(float localX, float localY);

    bool isOpen_{false};
    uint32_t targetBar_{0};
    int songKeyRoot_{0};
    bool isSongKeyMinor_{false};
    bool isEditingExisting_{false};

    CircleDialogTab activeTab_{CircleDialogTab::Wheel};

    int selectedRoot_{0};
    theory::ChordQuality selectedQuality_{theory::ChordQuality::Major};
    int selectedBass_{-1}; // -1 = Root
    float selectedBarLength_{1.0f};

    theory::ChordEvent currentChord_;

    float screenWidth_{1280.0f};
    float screenHeight_{800.0f};
    Rect2D dialogBounds_{0.0f, 0.0f, 680.0f, 520.0f};
    Rect2D closeBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Wheel geometry
    float wheelCenterX_{0.0f};
    float wheelCenterY_{0.0f};
    float wheelOuterRadius_{115.0f};
    float wheelInnerRadius_{66.0f};
    Rect2D auditionBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Action buttons
    Rect2D applyBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D deleteBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D cancelBtnBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Presets tab scrolling
    float presetsScrollY_{0.0f};
    int selectedPresetIdx_{0};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
};

} // namespace eatsbits::ui
