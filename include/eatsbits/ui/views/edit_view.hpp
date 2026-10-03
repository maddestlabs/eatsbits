#pragma once

#include "view_base.hpp"
#include <vector>
#include <string>
#include <set>
#include <functional>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::sequencer {
    class StepSequencer;
    class SequencerTrack;
}

namespace eatsbits::ui {
    struct ArrangerTimelineClip;
    struct ArrangerTimelineTrack;
}

#include "../score/score_layout_engine.hpp"
#include "../widgets/circle_of_fifths_dialog.hpp"
#include "../widgets/text_editor_widget.hpp"
#include "eatsbits/theory/chord_model.hpp"

namespace eatsbits::ui {

enum class EditSubViewMode {
    PianoRoll,
    Tracker,
    Score,
    Script
};

struct PianoRollNote {
    std::string id{"n0"};
    uint8_t pitch{60};
    float startStep{0.0f};
    float durationSteps{1.0f};
    float velocity{0.85f};
    bool isSelected{false};
    bool isSlide{false};
    bool isAccent{false};
    std::string articulation{"normal"};
    std::string lyric{""};
    int column{0};
    std::string effectCommand{"00"};
};

/**
 * EditView: Comprehensive musical editing workstation aligned with Eatsbeats.
 * Features 4 switchable sub-views:
 * - Piano Roll: FL Studio style 2D grid, velocity stalks, marquee selection, note move/resize, live auditioning
 * - Tracker: FastTracker 2 / Renoise hexadecimal matrix, QWERTY note entry, octave selector, block edit
 * - Score: SMuFL notation engraving, treble/bass staff, clef/duration/accidental tools, ledger lines
 * - Script: Bi-directional Eatscript declarative clip notation, syntax highlighting, sync to sequencer
 * Plus:
 * - Ghost Notes opacity slider & toggle across background tracks
 * - Contextual sliding Note Inspector Sidebar (single-note & multi-note batch editing)
 */
class EditView : public ViewBase {
public:
    EditView();
    ~EditView() override = default;

    void layout(const Rect2D& bounds, const ViewContext& ctx) override;
    void render(const ViewContext& ctx) override;
    bool handlePointer(const PointerEvent& ev, const ViewContext& ctx) override;
    bool handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) override;
    bool handleChar(char32_t codepoint, const ViewContext& ctx) override;

    [[nodiscard]] TextEditorWidget& getScriptEditor() noexcept { return scriptEditor_; }
    [[nodiscard]] const TextEditorWidget& getScriptEditor() const noexcept { return scriptEditor_; }

    // Sub-view mode
    void setSubView(EditSubViewMode mode) noexcept { subView_ = mode; }
    [[nodiscard]] EditSubViewMode getSubView() const noexcept { return subView_; }

    // Ghost notes
    void setGhostNotesOpacity(float opacity) noexcept { ghostOpacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    [[nodiscard]] float getGhostNotesOpacity() const noexcept { return ghostOpacity_; }
    void toggleGhostNotes() noexcept;

    // Track binding
    [[nodiscard]] uint32_t getActiveTrackIndex() const noexcept { return activeTrackIndex_; }
    void setActiveTrackIndex(uint32_t idx) noexcept;
    void setActiveTrackInfo(const std::string& name, float r, float g, float b) {
        activeTrackName_ = name;
        trackColor_ = Color(r, g, b);
    }

    // Note operations
    [[nodiscard]] const std::vector<PianoRollNote>& getNotes() const noexcept { return notes_; }
    std::vector<PianoRollNote>& getNotes() noexcept { return notes_; }

    [[nodiscard]] bool hasSelectedNotes() const noexcept;
    [[nodiscard]] size_t getSelectedNoteCount() const noexcept;
    void selectAllNotes() noexcept;
    void clearSelection() noexcept;
    void invertSelection() noexcept;
    void deleteSelectedNotes() noexcept;

    void transposeSelectedNotes(int semitones) noexcept;
    void nudgeSelectedNotes(float deltaSteps) noexcept;
    void changeSelectedNotesDuration(float deltaDuration) noexcept;
    void setSelectedNotesVelocity(float velocity) noexcept;
    void humanizeSelectedNotes(float amount = 0.15f) noexcept;
    void quantizeSelectedNotes(float snapSteps = 1.0f) noexcept;
    void setSelectedNotesSlide(bool slide) noexcept;
    void setSelectedNotesArticulation(const std::string& art) noexcept;

    void addNote(uint8_t pitch, float startStep, float durationSteps = 1.0f, float velocity = 0.85f, bool isSlide = false, bool isAccent = false);
    void deleteNoteAt(float step, uint8_t pitch);
    void retrogradeSelectedNotes() noexcept;
    void invertSelectedNotesPitch() noexcept;

    // Synchronize with audio engine sequencer track
    void syncFromSequencer(const sequencer::StepSequencer& seq);
    void syncToSequencer(sequencer::StepSequencer& seq);

    // Synchronize with Arranger Clips
    [[nodiscard]] int getActiveClipIndex() const noexcept { return activeClipIndex_; }
    void setActiveClipIndex(int idx) noexcept { activeClipIndex_ = idx; }
    [[nodiscard]] const std::string& getActiveClipName() const noexcept { return activeClipName_; }
    void setActiveClipName(std::string name) { activeClipName_ = std::move(name); }
    void loadFromArrangerClip(const ArrangerTimelineClip& clip,
                              const std::string& trackName,
                              const Color& trackColor,
                              const std::vector<ArrangerTimelineTrack>& allTracks);
    void writeBackToArrangerClip(ArrangerTimelineClip& clip) const;

    std::function<void(uint32_t trackIdx, int clipIdx)> onNotesChanged;

    // Auto-center viewport vertically on existing notes (Eatsbeats parity)
    void autoCenterOnNotesOrDefault() noexcept;

    // Harmonic Analysis & Circle of Fifths Integration
    void updateDetectedChords();
    [[nodiscard]] const std::vector<theory::ChordEvent>& getDetectedChords() const noexcept { return detectedChords_; }
    CircleOfFifthsDialog& getCircleOfFifthsDialog() noexcept { return circleOfFifthsDialog_; }

private:
    // Render passes
    void renderSubNavHeader(const ViewContext& ctx);
    void renderPianoRoll(const ViewContext& ctx);
    void renderTracker(const ViewContext& ctx);
    void renderScore(const ViewContext& ctx);
    void renderScript(const ViewContext& ctx);
    void renderNoteInspectorSidebar(const ViewContext& ctx);

    // Sidebar sub-sections
    void renderSingleNoteSidebar(const ViewContext& ctx, PianoRollNote& note);
    void renderMultiNoteSidebar(const ViewContext& ctx);

    // Helpers
    void formatEatscriptFromNotes();
    void parseNotesFromEatscript();
    void auditionPitch(int pitch, float velocity, const ViewContext& ctx);
    void stopAuditionPitch(int pitch, const ViewContext& ctx);

    // State
    EditSubViewMode subView_{EditSubViewMode::PianoRoll};
    float ghostOpacity_{0.35f};
    float lastNonZeroGhostOpacity_{0.35f};
    int auditioningPitch_{-1};

    uint32_t activeTrackIndex_{0};
    int activeClipIndex_{-1};
    std::string activeTrackName_{"TB-303 Acid"};
    std::string activeClipName_{""};
    Color trackColor_{0.0f, 0.95f, 1.0f};

    ScoreClef scoreClef_{ScoreClef::Auto};
    ScoreNoteType scoreDuration_{ScoreNoteType::Quarter};
    ScoreAccidental scoreAccidental_{ScoreAccidental::Natural};

    int trackerBaseOctave_{4};
    int trackerSelectedStep_{0};
    int trackerSelectedCol_{0};
    int trackerBlockAnchorStep_{-1};
    int trackerBlockAnchorCol_{-1};
    bool trackerHasBlockSelection_{false};

    // Piano Roll layout & viewport
    float stepWidth_{28.0f};
    float semitoneHeight_{22.0f};
    int minPitch_{24}; // C1
    int maxPitch_{84}; // C6
    float scrollX_{0.0f};
    float scrollY_{200.0f}; // Centered around middle octaves
    float trackerScrollY_{0.0f};
    float scoreScrollX_{0.0f};

    // Interaction drag modes
    enum class DragMode {
        None,
        MoveNotes,
        ResizeNotes,
        MarqueeSelect,
        MiddlePan,
        TouchPan,
        TouchGridPending,
        VelocityDrag,
        GhostSlider,
        SidebarPositionSlider,
        SidebarDurationSlider,
        SidebarVelocitySlider,
        TrackerBlockSelect
    };
    DragMode dragMode_{DragMode::None};

    KineticScroller kineticScroller_;
    Point2D touchDownPos_{0.0f, 0.0f};
    double touchDownTimeMs_{0.0};
    std::chrono::steady_clock::time_point touchDownTimePoint_{};
    bool touchLongPressArmed_{false};
    bool touchPanCommitted_{false};

    Point2D dragStartPoint_{0.0f, 0.0f};
    Point2D marqueeCurrentPoint_{0.0f, 0.0f};
    std::chrono::steady_clock::time_point lastGridClickTime_{};
    Point2D lastGridClickPos_{0.0f, 0.0f};
    float panStartScrollX_{0.0f};
    float panStartScrollY_{0.0f};
    int activeDragNoteIdx_{-1};
    float activeDragInitialStep_{0.0f};
    int activeDragInitialPitch_{60};
    float activeDragInitialDur_{1.0f};
    std::vector<std::pair<float, int>> batchDragOrigins_; // step, pitch for selected

    // Layout bounds
    Rect2D headerBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D contentBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D sidebarBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D pianoGutterBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D gridBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D velocityLaneBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Sub-nav buttons
    Rect2D btnPianoRoll_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnTracker_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnScore_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnScript_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D ghostToggleBtn_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D ghostSliderBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    // Score toolbar buttons
    Rect2D btnClefAuto_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnClefTreble_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnClefBass_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDurWhole_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDurHalf_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDurQuarter_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDurEighth_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnDur16th_{0.0f, 0.0f, 0.0f, 0.0f};

    // Script toolbar buttons
    Rect2D btnScriptApply_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D btnScriptRevert_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D scriptEditorBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    std::string scriptBuffer_{""};
    TextEditorWidget scriptEditor_;

    // Note collections
    std::vector<PianoRollNote> notes_;
    std::vector<PianoRollNote> ghostNotes_;

    // Harmonic Analysis & Circle of Fifths
    std::vector<theory::ChordEvent> detectedChords_;
    CircleOfFifthsDialog circleOfFifthsDialog_;
    Rect2D chordStripBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    Rect2D chordHeaderBadgeBounds_{0.0f, 0.0f, 0.0f, 0.0f};

    bool isSyncing_{false};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
    float sidebarScrollY_{0.0f};
};

} // namespace eatsbits::ui
