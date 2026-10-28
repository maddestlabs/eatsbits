#include "eatsbits/ui/views/edit_view.hpp"
#include "eatsbits/ui/views/arranger_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/score/score_glyphs.hpp"
#include "eatsbits/ui/widgets/piano_keyboard.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "clay.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <map>
#include <unordered_set>
#include <unordered_map>

namespace eatsbits::ui {

EditView::EditView() {
    clayContext_ = std::make_unique<clay::ClayContext>();
    clayContext_->initialize(1280.0f, 720.0f);

    // Initial rich melodic pattern (Acid bass sequence)
    notes_ = {
        {"n0", 36,  0.0f, 1.0f, 0.90f, false, false, false, "normal", "", 0, "00"},
        {"n1", 36,  2.0f, 1.0f, 0.85f, false, false, false, "normal", "", 0, "00"},
        {"n2", 48,  3.0f, 0.75f, 1.00f, false, true,  true,  "staccato", "", 0, "00"},
        {"n3", 39,  4.0f, 1.0f, 0.80f, false, false, false, "normal", "", 0, "00"},
        {"n4", 41,  6.0f, 1.5f, 0.95f, false, true,  false, "legato", "", 0, "00"},
        {"n5", 43,  8.0f, 1.0f, 0.85f, false, false, false, "normal", "", 0, "00"},
        {"n6", 36, 10.0f, 1.0f, 0.90f, false, false, false, "normal", "", 0, "00"},
        {"n7", 46, 12.0f, 0.5f, 0.95f, false, false, true,  "staccato", "", 0, "00"},
        {"n8", 48, 14.0f, 1.5f, 1.00f, false, true,  false, "legato", "", 0, "00"}
    };

    // Ghost chord progression from background synth tracks
    ghostNotes_ = {
        {"g0", 60,  0.0f, 8.0f, 0.60f, false, false, false, "normal", "", 1, "00"},
        {"g1", 63,  0.0f, 8.0f, 0.60f, false, false, false, "normal", "", 1, "00"},
        {"g2", 67,  0.0f, 8.0f, 0.60f, false, false, false, "normal", "", 1, "00"},
        {"g3", 58,  8.0f, 8.0f, 0.60f, false, false, false, "normal", "", 1, "00"},
        {"g4", 62,  8.0f, 8.0f, 0.60f, false, false, false, "normal", "", 1, "00"},
        {"g5", 65,  8.0f, 8.0f, 0.60f, false, false, false, "normal", "", 1, "00"}
    };

    scriptEditor_.onCompileTriggered = [this]() {
        parseNotesFromEatscript();
    };
    scriptEditor_.onTextChanged = [this](const std::string& text) {
        scriptBuffer_ = text;
    };

    formatEatscriptFromNotes();

    PianoKeyboardConfig pcfg;
    pcfg.orientation = KeyboardOrientation::Vertical;
    pcfg.minPitch = minPitch_;
    pcfg.maxPitch = maxPitch_;
    pcfg.keyHeight = semitoneHeight_;
    pcfg.keyWidth = 80.0f;
    pcfg.blackKeyRatio = 0.64f;
    pcfg.showOctaveLabels = true;
    pianoKeyboard_.setConfig(pcfg);

    trackerCellDialog_.onApply = [this](int step, int lane, uint8_t pitch, float velocity, bool isSlide, const std::string& fxCmd,
                                        const std::unordered_map<std::string, std::string>& chipParams) {
        bool found = false;
        for (auto& n : notes_) {
            if (static_cast<int>(std::round(n.startStep)) == step && n.column == lane) {
                n.pitch = pitch;
                n.velocity = velocity;
                n.isSlide = isSlide;
                n.effectCommand = fxCmd;
                n.customParams = chipParams;
                for (const auto& [k, v] : chipParams) {
                    registerTrackerParamCol(k);
                }
                found = true;
                break;
            }
        }
        if (!found) {
            PianoRollNote n;
            n.id = "n" + std::to_string(notes_.size());
            n.pitch = pitch;
            n.startStep = static_cast<float>(step);
            n.durationSteps = 1.0f;
            n.velocity = velocity;
            n.isSlide = isSlide;
            n.effectCommand = fxCmd;
            n.column = lane;
            n.customParams = chipParams;
            for (const auto& [k, v] : chipParams) {
                registerTrackerParamCol(k);
            }
            notes_.push_back(n);
        }
        formatEatscriptFromNotes();
        if (onNotesChanged) {
            onNotesChanged(activeTrackIndex_, activeClipIndex_);
        }
    };

    trackerCellDialog_.onDelete = [this](int step, int lane) {
        notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [=](const PianoRollNote& n) {
            return static_cast<int>(std::round(n.startStep)) == step && n.column == lane;
        }), notes_.end());
        formatEatscriptFromNotes();
        if (onNotesChanged) {
            onNotesChanged(activeTrackIndex_, activeClipIndex_);
        }
    };
}

EditView::~EditView() = default;

void EditView::openTrackerCellDialog(int step, int lane) {
    const PianoRollNote* match = nullptr;
    for (const auto& n : notes_) {
        if (static_cast<int>(std::round(n.startStep)) == step && n.column == lane) {
            match = &n;
            break;
        }
    }
    std::vector<std::string> customKeys;
    for (const auto& col : trackerCustomCols_) {
        customKeys.push_back(col.key);
    }
    trackerCellDialog_.open(step, lane, activeTrackName_, match, customKeys);
}

bool EditView::insertTrackerNote(uint8_t pitch, float velocity, const ViewContext* ctx) {
    if (subView_ != EditSubViewMode::Tracker) return false;
    if (trackerHasBlockSelection_) return false;

    bool found = false;
    for (auto& n : notes_) {
        if (static_cast<int>(std::round(n.startStep)) == trackerSelectedStep_ && n.column == trackerSelectedCol_) {
            n.pitch = pitch;
            n.velocity = std::clamp(velocity, 0.05f, 1.0f);
            found = true;
            break;
        }
    }
    if (!found) {
        PianoRollNote n;
        n.id = "n" + std::to_string(notes_.size());
        n.pitch = pitch;
        n.startStep = static_cast<float>(trackerSelectedStep_);
        n.durationSteps = 1.0f;
        n.velocity = std::clamp(velocity, 0.05f, 1.0f);
        n.column = trackerSelectedCol_;
        notes_.push_back(n);
    }

    if (ctx) {
        auditionPitch(pitch, velocity, *ctx);
    }

    if (trackerEditStep_ > 0) {
        trackerSelectedStep_ = (trackerSelectedStep_ + trackerEditStep_) % 64;
        ensureTrackerCursorVisible();
    }

    formatEatscriptFromNotes();
    if (onNotesChanged) {
        onNotesChanged(activeTrackIndex_, activeClipIndex_);
    } else if (ctx && ctx->audioEngine) {
        syncToSequencer(ctx->audioEngine->getSequencer());
    }
    return true;
}

void EditView::setSubView(EditSubViewMode mode) noexcept {
    if (subView_ != mode) {
        pianoKeyboard_.releaseAllNotes(nullptr, activeTrackIndex_);
    }

    // Bi-directional synchronization:
    // If leaving Script tab, parse text editor buffer into notes
    if (subView_ == EditSubViewMode::Script && mode != EditSubViewMode::Script) {
        parseNotesFromEatscript();
        scriptEditor_.onFocusLost();
        if (onRequestClearFocus) {
            onRequestClearFocus();
        }
    }
    // If entering Script tab, format the latest notes into the script editor
    if (mode == EditSubViewMode::Script && subView_ != EditSubViewMode::Script) {
        formatEatscriptFromNotes();
    }
    // If not in Script mode, ensure script editor does not retain focus
    if (mode != EditSubViewMode::Script) {
        scriptEditor_.onFocusLost();
        if (onRequestClearFocus) {
            onRequestClearFocus();
        }
    }

    subView_ = mode;
    if (subView_ == EditSubViewMode::Tracker) {
        updateTrackerLanesFromNotes();
    }
    if (onSubViewChanged) {
        onSubViewChanged(subView_);
    }
}

void EditView::updateTrackerLanesFromNotes() noexcept {
    std::unordered_map<int, int> stepCount;
    int maxPoly = 0;
    for (const auto& n : notes_) {
        int s = static_cast<int>(std::round(n.startStep));
        stepCount[s]++;
        if (stepCount[s] > maxPoly) maxPoly = stepCount[s];
        if (n.column + 1 > maxPoly) maxPoly = n.column + 1;
    }
    trackerNumLanes_ = std::max({2, maxPoly, trackerNumLanes_});
    if (trackerSelectedCol_ >= trackerNumLanes_) {
        trackerSelectedCol_ = trackerNumLanes_ - 1;
    }
}

void EditView::addTrackerLane() noexcept {
    if (trackerNumLanes_ < 16) {
        trackerNumLanes_++;
        trackerSelectedCol_ = trackerNumLanes_ - 1;
        ensureTrackerCursorVisible();
    }
}

void EditView::removeTrackerLane() noexcept {
    if (trackerNumLanes_ <= 2) return;
    int colToRemove = trackerNumLanes_ - 1;
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [=](const PianoRollNote& n) {
        return n.column == colToRemove;
    }), notes_.end());
    trackerNumLanes_--;
    if (trackerSelectedCol_ >= trackerNumLanes_) {
        trackerSelectedCol_ = trackerNumLanes_ - 1;
    }
    ensureTrackerCursorVisible();
    formatEatscriptFromNotes();
    if (onNotesChanged) {
        onNotesChanged(activeTrackIndex_, activeClipIndex_);
    }
}

void EditView::registerTrackerParamCol(const std::string& key) {
    if (key.empty()) return;
    for (const auto& c : trackerCustomCols_) {
        if (c.key == key) return;
    }
    TrackerParamCol col;
    col.key = key;
    std::string h = key;
    for (auto& ch : h) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    col.header = h.substr(0, 3);
    trackerCustomCols_.push_back(col);
}

void EditView::setActiveTrackIndex(uint32_t idx) noexcept {
    if (activeTrackIndex_ == idx) return;
    pianoKeyboard_.releaseAllNotes(nullptr, activeTrackIndex_);
    activeTrackIndex_ = idx;
    switch (idx) {
        case 0:
            activeTrackName_ = "303 Acid Bass";
            trackColor_ = Color(1.0f, 0.55f, 0.0f); // Warm Orange
            break;
        case 1:
            activeTrackName_ = "TR-808 Kit";
            trackColor_ = Color(0.13f, 0.96f, 0.91f); // Cyan
            break;
        case 2:
            activeTrackName_ = "TR-909 Drive";
            trackColor_ = Color(1.0f, 0.16f, 0.43f); // Pink/Red
            break;
        case 3:
            activeTrackName_ = "DXVII E-Piano";
            trackColor_ = Color(0.62f, 0.31f, 0.87f); // Purple
            break;
        case 4:
            activeTrackName_ = "Acoustic Bass";
            trackColor_ = Color(0.83f, 0.69f, 0.22f); // Gold / Amber
            break;
        default:
            activeTrackName_ = "Track " + std::to_string(idx + 1);
            trackColor_ = Color(0.4f, 0.8f, 1.0f);
            break;
    }
}

void EditView::setChordFollowMode(theory::ChordFollowMode mode) noexcept {
    chordFollowMode_ = mode;
    updateEvaluatedNotes();
}

void EditView::setMidiFxRack(std::vector<eatscript::MidiFxInsert> rack) {
    midiFxRack_ = std::move(rack);
    updateEvaluatedNotes();
}

void EditView::setChordTrack(std::vector<theory::ChordEvent> chords) {
    chordTrack_ = std::move(chords);
    updateEvaluatedNotes();
}

bool EditView::hasActiveChordFollowOrMidiFx() const noexcept {
    if (chordFollowMode_ != theory::ChordFollowMode::Off) return true;
    for (const auto& fx : midiFxRack_) {
        if (fx.enabled) return true;
    }
    return false;
}

void EditView::updateEvaluatedNotes() {
    evaluatedFxNotes_.clear();
    if (!hasActiveChordFollowOrMidiFx() || notes_.empty()) {
        return;
    }

    std::vector<eatscript::MidiNote> baseMidiNotes;
    baseMidiNotes.reserve(notes_.size());
    for (size_t i = 0; i < notes_.size(); ++i) {
        const auto& n = notes_[i];
        eatscript::MidiNote mn;
        mn.id = n.id.empty() ? ("n" + std::to_string(i)) : n.id;
        mn.pitch = n.pitch;
        mn.startStep = n.startStep;
        mn.durationSteps = n.durationSteps;
        mn.velocity = n.velocity;
        baseMidiNotes.push_back(mn);
    }

    eatscript::TimeContext timeCtx{};
    timeCtx.bpm = 120.0;
    timeCtx.songKeyRoot = 0;
    timeCtx.isSongKeyMinor = false;
    if (!chordTrack_.empty()) {
        const auto& ch = chordTrack_.front();
        timeCtx.activeChordRoot = ch.rootPitchClass;
        timeCtx.activeChordQuality = theory::getChordQualityDisplayName(ch.quality);
        timeCtx.chordPitchClasses = ch.getPitchClasses();
        timeCtx.bassPitchClass = (ch.bassPitchClass >= 0) ? ch.bassPitchClass : ch.rootPitchClass;
    }

    std::vector<eatscript::MidiNote> transformed = baseMidiNotes;

    // 1. Evaluate Chord Track Follow Mode
    if (chordFollowMode_ != theory::ChordFollowMode::Off) {
        for (auto& mn : transformed) {
            float noteBar = static_cast<float>(mn.startStep) / 16.0f;
            const theory::ChordEvent* chord = nullptr;
            for (const auto& ch : chordTrack_) {
                float chStart = static_cast<float>(ch.startBar);
                float chEnd = chStart + static_cast<float>(ch.barLength);
                if (noteBar >= chStart && noteBar < chEnd) {
                    chord = &ch;
                    break;
                }
            }
            if (!chord && !chordTrack_.empty()) {
                chord = &chordTrack_.front();
            }
            if (chord) {
                int remapped = theory::ChordTheory::remapPitchForChord(mn.pitch, *chord, chordFollowMode_);
                mn.pitch = static_cast<uint8_t>(std::clamp(remapped, 0, 127));
            }
        }
    }

    // 2. Evaluate active MIDI FX chain
    bool anyMidiFx = false;
    for (const auto& fx : midiFxRack_) {
        if (fx.enabled) {
            anyMidiFx = true;
            break;
        }
    }
    if (anyMidiFx) {
        transformed = eatscript::MidiPipelineEngine::processPipeline(midiFxRack_, transformed, timeCtx);
    }

    evaluatedFxNotes_.reserve(transformed.size());
    for (size_t i = 0; i < transformed.size(); ++i) {
        const auto& mn = transformed[i];
        PianoRollNote pn;
        pn.id = mn.id.empty() ? ("ev_" + std::to_string(i)) : mn.id;
        pn.pitch = mn.pitch;
        pn.startStep = static_cast<float>(mn.startStep);
        pn.durationSteps = static_cast<float>(mn.durationSteps);
        pn.velocity = static_cast<float>(mn.velocity);
        pn.isSelected = false;
        pn.isSlide = false;
        pn.isAccent = false;
        evaluatedFxNotes_.push_back(pn);
    }
}

void EditView::bakeChordFollowAndMidiFx() {
    if (!hasActiveChordFollowOrMidiFx()) return;

    updateEvaluatedNotes();

    if (!evaluatedFxNotes_.empty()) {
        notes_.clear();
        for (size_t i = 0; i < evaluatedFxNotes_.size(); ++i) {
            PianoRollNote pn = evaluatedFxNotes_[i];
            pn.id = "n" + std::to_string(i);
            pn.isSelected = false;
            notes_.push_back(pn);
        }
    }

    chordFollowMode_ = theory::ChordFollowMode::Off;
    for (auto& fx : midiFxRack_) {
        fx.enabled = false;
    }
    evaluatedFxNotes_.clear();

    formatEatscriptFromNotes();

    if (onBakeRequested) {
        onBakeRequested(activeTrackIndex_, activeClipIndex_);
    } else if (onNotesChanged) {
        onNotesChanged(activeTrackIndex_, activeClipIndex_);
    }
}

void EditView::toggleGhostNotes() noexcept {
    if (ghostOpacity_ > 0.001f) {
        lastNonZeroGhostOpacity_ = ghostOpacity_;
        ghostOpacity_ = 0.0f;
    } else {
        ghostOpacity_ = (lastNonZeroGhostOpacity_ > 0.05f) ? lastNonZeroGhostOpacity_ : 0.35f;
    }
}

bool EditView::hasSelectedNotes() const noexcept {
    for (const auto& n : notes_) {
        if (n.isSelected) return true;
    }
    return false;
}

size_t EditView::getSelectedNoteCount() const noexcept {
    size_t count = 0;
    for (const auto& n : notes_) {
        if (n.isSelected) count++;
    }
    return count;
}

void EditView::selectAllNotes() noexcept {
    for (auto& n : notes_) {
        n.isSelected = true;
    }
}

void EditView::clearSelection() noexcept {
    for (auto& n : notes_) {
        n.isSelected = false;
    }
    trackerHasBlockSelection_ = false;
    trackerBlockAnchorStep_ = -1;
    trackerBlockAnchorCol_ = -1;
}

void EditView::invertSelection() noexcept {
    for (auto& n : notes_) {
        n.isSelected = !n.isSelected;
    }
}

void EditView::onNotesMutated() {
    formatEatscriptFromNotes();
    scriptEditor_.setText(scriptBuffer_);
    updateEvaluatedNotes();
    if (subView_ == EditSubViewMode::Tracker) {
        updateTrackerLanesFromNotes();
    }
    if (onNotesChanged) {
        onNotesChanged(activeTrackIndex_, activeClipIndex_);
    }
}

void EditView::deleteSelectedNotes() noexcept {
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [](const PianoRollNote& n) {
        return n.isSelected;
    }), notes_.end());
    onNotesMutated();
}

void EditView::transposeSelectedNotes(int semitones) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            int newPitch = static_cast<int>(n.pitch) + semitones;
            n.pitch = static_cast<uint8_t>(std::clamp(newPitch, 0, 127));
        }
    }
    onNotesMutated();
}

void EditView::nudgeSelectedNotes(float deltaSteps) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.startStep = std::max(0.0f, n.startStep + deltaSteps);
        }
    }
    onNotesMutated();
}

void EditView::changeSelectedNotesDuration(float deltaDuration) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.durationSteps = std::clamp(n.durationSteps + deltaDuration, 0.25f, 64.0f);
        }
    }
    onNotesMutated();
}

void EditView::setSelectedNotesVelocity(float velocity) noexcept {
    float v = std::clamp(velocity, 0.05f, 1.0f);
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.velocity = v;
        }
    }
    onNotesMutated();
}

void EditView::humanizeSelectedNotes(float amount) noexcept {
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].isSelected) {
            float r = (static_cast<float>((i * 7 + 13) % 31) / 30.0f - 0.5f) * 2.0f;
            notes_[i].velocity = std::clamp(notes_[i].velocity + r * amount, 0.1f, 1.0f);
        }
    }
    onNotesMutated();
}

void EditView::quantizeSelectedNotes(float snapSteps) noexcept {
    float snap = (snapSteps > 0.01f) ? snapSteps : 1.0f;
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.startStep = std::round(n.startStep / snap) * snap;
        }
    }
    onNotesMutated();
}

void EditView::setSelectedNotesSlide(bool slide) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.isSlide = slide;
        }
    }
    onNotesMutated();
}

void EditView::setSelectedNotesArticulation(const std::string& art) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.articulation = art;
        }
    }
    onNotesMutated();
}

void EditView::addNote(uint8_t pitch, float startStep, float durationSteps, float velocity, bool isSlide, bool isAccent) {
    PianoRollNote n;
    n.id = "n" + std::to_string(notes_.size() + 1);
    n.pitch = pitch;
    n.startStep = std::max(0.0f, startStep);
    n.durationSteps = std::max(0.25f, durationSteps);
    n.velocity = std::clamp(velocity, 0.05f, 1.0f);
    n.isSelected = true;
    n.isSlide = isSlide;
    n.isAccent = isAccent;
    n.articulation = "normal";

    std::unordered_set<int> usedCols;
    for (const auto& existing : notes_) {
        if (std::abs(existing.startStep - n.startStep) < 0.25f) {
            usedCols.insert(existing.column);
        }
    }
    int col = 0;
    while (usedCols.count(col) > 0) {
        col++;
    }
    n.column = col;
    if (n.column + 1 > trackerNumLanes_) {
        trackerNumLanes_ = n.column + 1;
    }

    n.effectCommand = "00";
    clearSelection();
    notes_.push_back(n);
    formatEatscriptFromNotes();
    if (onNotesChanged) onNotesChanged(activeTrackIndex_, activeClipIndex_);
}

void EditView::ensureTrackerCursorVisible() noexcept {
    float rowH = 24.0f;
    float visibleH = std::max(100.0f, contentBounds_.h - 62.0f);
    float stepY = static_cast<float>(trackerSelectedStep_) * rowH;
    float margin = 3.0f * rowH;

    if (stepY < trackerScrollY_ + margin) {
        trackerScrollY_ = std::max(0.0f, stepY - margin);
    } else if (stepY + rowH > trackerScrollY_ + visibleH - margin) {
        trackerScrollY_ = std::max(0.0f, stepY + rowH - visibleH + margin);
    }

    float rowNumW = 48.0f;
    float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
    float colW = 140.0f + extraParamsW;
    float colSpacing = 6.0f;
    float colLeft = static_cast<float>(trackerSelectedCol_) * (colW + colSpacing);
    float colRight = colLeft + colW;
    float visibleW = std::max(100.0f, contentBounds_.w - rowNumW);

    if (colLeft < trackerScrollX_) {
        trackerScrollX_ = std::max(0.0f, colLeft);
    } else if (colRight > trackerScrollX_ + visibleW) {
        trackerScrollX_ = std::max(0.0f, colRight - visibleW + 16.0f);
    }
}

void EditView::deleteNoteAt(float step, uint8_t pitch) {
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [=](const PianoRollNote& n) {
        bool matchPitch = (n.pitch == pitch);
        bool matchStep = (std::abs(n.startStep - step) < 0.5f || (step >= n.startStep && step < n.startStep + n.durationSteps));
        return matchPitch && matchStep;
    }), notes_.end());
    formatEatscriptFromNotes();
    if (onNotesChanged) onNotesChanged(activeTrackIndex_, activeClipIndex_);
}

void EditView::retrogradeSelectedNotes() noexcept {
    std::vector<size_t> selIndices;
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].isSelected) selIndices.push_back(i);
    }
    if (selIndices.size() < 2) return;

    float minStep = 1e9f;
    float maxEndStep = -1e9f;
    for (size_t idx : selIndices) {
        minStep = std::min(minStep, notes_[idx].startStep);
        maxEndStep = std::max(maxEndStep, notes_[idx].startStep + notes_[idx].durationSteps);
    }

    for (size_t idx : selIndices) {
        float oldEnd = notes_[idx].startStep + notes_[idx].durationSteps;
        notes_[idx].startStep = minStep + (maxEndStep - oldEnd);
    }
    onNotesMutated();
}

void EditView::invertSelectedNotesPitch() noexcept {
    std::vector<size_t> selIndices;
    float sumPitch = 0.0f;
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].isSelected) {
            selIndices.push_back(i);
            sumPitch += static_cast<float>(notes_[i].pitch);
        }
    }
    if (selIndices.empty()) return;

    float avgPitch = sumPitch / static_cast<float>(selIndices.size());
    for (size_t idx : selIndices) {
        int inverted = static_cast<int>(std::round(2.0f * avgPitch - static_cast<float>(notes_[idx].pitch)));
        notes_[idx].pitch = static_cast<uint8_t>(std::clamp(inverted, 0, 127));
    }
    onNotesMutated();
}

void EditView::syncFromSequencer(const sequencer::StepSequencer& seq) {
    if (activeTrackIndex_ >= seq.getNumTracks()) return;
    const auto* track = seq.getTrack(activeTrackIndex_);
    if (!track) return;

    isSyncing_ = true;
    notes_.clear();
    for (uint32_t s = 0; s < track->getNumSteps(); ++s) {
        const auto& step = track->getStep(s);
        if (step.active) {
            PianoRollNote n;
            n.id = "n" + std::to_string(notes_.size());
            n.pitch = step.note;
            n.startStep = static_cast<float>(s);
            n.durationSteps = std::max(0.25f, step.gateLength);
            n.velocity = step.velocity;
            n.isSelected = track->isStepSelected(s);
            n.isSlide = step.slide;
            n.isAccent = step.accent;
            n.articulation = "normal";
            n.column = 0;
            n.effectCommand = "00";
            notes_.push_back(n);

            for (size_t ei = 0; ei < step.extraNotes.size(); ++ei) {
                PianoRollNote en = n;
                en.id = "n" + std::to_string(notes_.size());
                en.pitch = step.extraNotes[ei];
                notes_.push_back(en);
            }
        }
    }

    // Ghost notes from other tracks
    ghostNotes_.clear();
    for (uint32_t t = 0; t < seq.getNumTracks(); ++t) {
        if (t == activeTrackIndex_) continue;
        const auto* oth = seq.getTrack(t);
        if (!oth) continue;
        for (uint32_t s = 0; s < oth->getNumSteps(); ++s) {
            const auto& step = oth->getStep(s);
            if (step.active) {
                PianoRollNote gn;
                gn.id = "g_" + std::to_string(t) + "_" + std::to_string(ghostNotes_.size());
                gn.pitch = step.note;
                gn.startStep = static_cast<float>(s);
                gn.durationSteps = std::max(0.25f, step.gateLength);
                gn.velocity = step.velocity;
                gn.column = static_cast<int>(t);
                ghostNotes_.push_back(gn);

                for (uint8_t ex : step.extraNotes) {
                    PianoRollNote gen = gn;
                    gen.id = "g_" + std::to_string(t) + "_" + std::to_string(ghostNotes_.size());
                    gen.pitch = ex;
                    ghostNotes_.push_back(gen);
                }
            }
        }
    }

    formatEatscriptFromNotes();
    autoCenterOnNotesOrDefault();
    isSyncing_ = false;
}

void EditView::loadFromArrangerClip(const ArrangerTimelineClip& clip,
                                    const std::string& trackName,
                                    const Color& trackColor,
                                    const std::vector<ArrangerTimelineTrack>& allTracks,
                                    theory::ChordFollowMode chordFollowMode,
                                    const std::vector<eatscript::MidiFxInsert>& midiFxRack,
                                    const std::vector<theory::ChordEvent>& chordTrack) {
    isSyncing_ = true;
    activeTrackIndex_ = clip.trackIndex;
    activeClipName_ = clip.name;
    activeTrackName_ = trackName;
    trackColor_ = trackColor;
    chordFollowMode_ = chordFollowMode;
    midiFxRack_ = midiFxRack;
    chordTrack_ = chordTrack;

    notes_.clear();
    if (!clip.eatscriptCode.empty()) {
        scriptBuffer_ = clip.eatscriptCode;
        scriptEditor_.setText(scriptBuffer_);
        parseNotesFromEatscript();
    } else {
        std::unordered_map<int, int> stepColMap;
        int maxPoly = 0;
        for (size_t i = 0; i < clip.notes.size(); ++i) {
            const auto& cn = clip.notes[i];
            PianoRollNote pn;
            pn.id = "n" + std::to_string(i);
            pn.pitch = cn.pitch;
            pn.startStep = cn.startBeat * 4.0f;
            pn.durationSteps = std::max(0.25f, cn.lengthBeats * 4.0f);
            pn.velocity = cn.velocity;
            pn.isSelected = false;
            pn.isSlide = cn.slide;
            pn.isAccent = cn.accent;
            pn.articulation = "normal";
            int sInt = static_cast<int>(std::round(pn.startStep));
            int cIdx = stepColMap[sInt]++;
            pn.column = cIdx;
            if (cIdx + 1 > maxPoly) maxPoly = cIdx + 1;
            pn.effectCommand = "00";
            notes_.push_back(pn);
        }
        trackerNumLanes_ = std::max(2, maxPoly);
        formatEatscriptFromNotes();
    }

    // Ghost notes from other tracks' clips
    ghostNotes_.clear();
    for (size_t t = 0; t < allTracks.size(); ++t) {
        if (t == activeTrackIndex_) continue;
        const auto& othTrack = allTracks[t];
        for (const auto& othClip : othTrack.clips) {
            for (size_t ci = 0; ci < othClip.notes.size(); ++ci) {
                const auto& cn = othClip.notes[ci];
                PianoRollNote gn;
                gn.id = "g_" + std::to_string(t) + "_" + std::to_string(ci);
                gn.pitch = cn.pitch;
                gn.startStep = cn.startBeat * 4.0f;
                gn.durationSteps = std::max(0.25f, cn.lengthBeats * 4.0f);
                gn.velocity = cn.velocity;
                gn.column = static_cast<int>(t);
                ghostNotes_.push_back(gn);
            }
        }
    }

    updateEvaluatedNotes();
    scrollX_ = 0.0f;
    autoCenterOnNotesOrDefault();
    isSyncing_ = false;
}

void EditView::writeBackToArrangerClip(ArrangerTimelineClip& clip) const {
    clip.notes.clear();
    for (const auto& pn : notes_) {
        ArrangerClipNote an;
        an.pitch = pn.pitch;
        an.startBeat = pn.startStep / 4.0f;
        an.lengthBeats = pn.durationSteps / 4.0f;
        an.velocity = pn.velocity;
        an.slide = pn.isSlide;
        an.accent = pn.isAccent;
        clip.notes.push_back(an);
    }
    clip.eatscriptCode = scriptBuffer_;
}

void EditView::autoCenterOnNotesOrDefault() noexcept {
    float viewportH = (gridBounds_.h > 50.0f) ? gridBounds_.h : (contentBounds_.h > 120.0f ? (contentBounds_.h - 72.0f) : 550.0f);
    float maxScroll = std::max(0.0f, static_cast<float>(maxPitch_ - minPitch_ + 1) * semitoneHeight_ - viewportH);

    float targetY = 0.0f;
    if (!notes_.empty()) {
        int minP = 127;
        int maxP = 0;
        for (const auto& n : notes_) {
            if (n.pitch < minP) minP = n.pitch;
            if (n.pitch > maxP) maxP = n.pitch;
        }
        float midPitch = static_cast<float>(minP + maxP) * 0.5f;
        float midKeyIdx = static_cast<float>(maxPitch_) - midPitch;
        targetY = (midKeyIdx * semitoneHeight_) - (viewportH * 0.5f) + (semitoneHeight_ * 0.5f);
    } else {
        // Default to centering around C4 (MIDI pitch 60)
        const int defaultPitch = 60; // C4
        float c4KeyIdx = static_cast<float>(maxPitch_ - defaultPitch);
        targetY = (c4KeyIdx * semitoneHeight_) - (viewportH * 0.5f) + (semitoneHeight_ * 0.5f);
    }

    scrollY_ = std::clamp(targetY, 0.0f, maxScroll);
}

void EditView::syncToSequencer(sequencer::StepSequencer& seq) {
    if (activeTrackIndex_ >= seq.getNumTracks()) return;
    auto* track = seq.getTrack(activeTrackIndex_);
    if (!track) return;

    // Reset steps
    for (uint32_t s = 0; s < track->getNumSteps(); ++s) {
        sequencer::StepData sd{};
        sd.active = false;
        track->setStep(s, sd);
    }

    // Group notes by step
    std::map<uint32_t, std::vector<PianoRollNote>> stepBuckets;
    for (const auto& n : notes_) {
        uint32_t s = static_cast<uint32_t>(std::clamp(std::round(n.startStep), 0.0f, static_cast<float>(track->getNumSteps() - 1)));
        stepBuckets[s].push_back(n);
    }

    for (const auto& [s, noteList] : stepBuckets) {
        if (noteList.empty() || s >= track->getNumSteps()) continue;
        sequencer::StepData sd{};
        sd.active = true;
        sd.note = noteList[0].pitch;
        sd.velocity = noteList[0].velocity;
        float maxDur = 0.0f;
        for (const auto& n : noteList) {
            maxDur = std::max(maxDur, n.durationSteps);
        }
        sd.gateLength = std::clamp(maxDur, 0.1f, 128.0f);
        sd.slide = noteList[0].isSlide;
        sd.accent = noteList[0].isAccent;
        for (size_t i = 1; i < noteList.size(); ++i) {
            sd.extraNotes.push_back(noteList[i].pitch);
        }
        track->setStep(s, sd);
    }
}

void EditView::formatEatscriptFromNotes() {
    updateEvaluatedNotes();
    std::ostringstream ss;
    ss << "-- Clip: \"" << activeTrackName_ << "\" | Events: " << notes_.size() << "\n";
    ss << "-- Syntax: { pitch = \"C4\", step = 0.0, dur = 1.0, vel = 0.85, slide = true }\n\n";

    auto sorted = notes_;
    std::sort(sorted.begin(), sorted.end(), [](const PianoRollNote& a, const PianoRollNote& b) {
        if (a.startStep != b.startStep) return a.startStep < b.startStep;
        return a.pitch < b.pitch;
    });

    for (const auto& n : sorted) {
        std::string pName = PianoKeyboard::getNoteName(n.pitch);
        ss << "{ pitch = \"" << pName << "\", step = " << std::fixed << std::setprecision(1) << n.startStep
           << ", dur = " << n.durationSteps << ", vel = " << std::setprecision(2) << n.velocity;
        if (n.column > 0) ss << ", lane = " << n.column;
        if (n.isSlide) ss << ", slide = true";
        if (n.isAccent) ss << ", accent = true";
        if (!n.articulation.empty() && n.articulation != "normal") ss << ", art = \"" << n.articulation << "\"";
        if (!n.lyric.empty()) ss << ", lyric = \"" << n.lyric << "\"";
        if (!n.effectCommand.empty() && n.effectCommand != "00") ss << ", fx = \"" << n.effectCommand << "\"";
        for (const auto& [k, v] : n.customParams) {
            if (!k.empty() && !v.empty()) {
                ss << ", " << k << " = \"" << v << "\"";
            }
        }
        ss << " }\n";
    }

    scriptBuffer_ = ss.str();
    scriptEditor_.setText(scriptBuffer_);
    if (!isSyncing_ && onNotesChanged) {
        onNotesChanged(activeTrackIndex_, activeClipIndex_);
    }
}

void EditView::parseNotesFromEatscript() {
    std::vector<PianoRollNote> newNotes;
    std::istringstream stream(scriptBuffer_);
    std::string line;
    int idx = 0;
    std::unordered_map<int, int> stepColMap;

    while (std::getline(stream, line)) {
        if (line.find('{') == std::string::npos || line.find("pitch") == std::string::npos) continue;

        PianoRollNote n;
        n.id = "n" + std::to_string(idx++);

        auto pPos = line.find("pitch");
        if (pPos != std::string::npos) {
            auto q1 = line.find('"', pPos);
            auto q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string pStr = line.substr(q1 + 1, q2 - q1 - 1);
                if (!pStr.empty()) {
                    char name = pStr[0];
                    bool sharp = (pStr.size() > 1 && pStr[1] == '#');
                    int base = 0;
                    switch (name) {
                        case 'C': case 'c': base = 0; break;
                        case 'D': case 'd': base = 2; break;
                        case 'E': case 'e': base = 4; break;
                        case 'F': case 'f': base = 5; break;
                        case 'G': case 'g': base = 7; break;
                        case 'A': case 'a': base = 9; break;
                        case 'B': case 'b': base = 11; break;
                    }
                    if (sharp) base += 1;
                    int oct = 4;
                    size_t octIdx = sharp ? 2 : 1;
                    if (octIdx < pStr.size() && (std::isdigit(pStr[octIdx]) || pStr[octIdx] == '-')) {
                        oct = std::stoi(pStr.substr(octIdx));
                    }
                    n.pitch = static_cast<uint8_t>(std::clamp((oct + 1) * 12 + base, 0, 127));
                }
            }
        }

        auto sPos = line.find("step");
        if (sPos != std::string::npos) {
            auto eq = line.find('=', sPos);
            if (eq != std::string::npos) {
                n.startStep = std::stof(line.substr(eq + 1));
            }
        }

        auto dPos = line.find("dur");
        if (dPos != std::string::npos) {
            auto eq = line.find('=', dPos);
            if (eq != std::string::npos) {
                n.durationSteps = std::stof(line.substr(eq + 1));
            }
        }

        auto vPos = line.find("vel");
        if (vPos != std::string::npos) {
            auto eq = line.find('=', vPos);
            if (eq != std::string::npos) {
                n.velocity = std::stof(line.substr(eq + 1));
            }
        }

        auto lPos = line.find("lane");
        if (lPos == std::string::npos) lPos = line.find("col");
        if (lPos != std::string::npos) {
            auto eq = line.find('=', lPos);
            if (eq != std::string::npos) {
                try {
                    n.column = std::stoi(line.substr(eq + 1));
                } catch (...) {
                    n.column = 0;
                }
            }
        } else {
            int sInt = static_cast<int>(std::round(n.startStep));
            n.column = stepColMap[sInt]++;
        }

        auto fxPos = line.find("fx");
        if (fxPos != std::string::npos) {
            auto q1 = line.find('"', fxPos);
            auto q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos) {
                n.effectCommand = line.substr(q1 + 1, q2 - q1 - 1);
            }
        }

        if (line.find("slide = true") != std::string::npos) n.isSlide = true;
        if (line.find("accent = true") != std::string::npos) n.isAccent = true;

        const std::vector<std::string> knownChipKeys = {"echo", "alg", "fb", "pan", "mod", "cut", "res"};
        for (const auto& ck : knownChipKeys) {
            auto pos = line.find(ck + " =");
            if (pos == std::string::npos) pos = line.find(ck + "=");
            if (pos != std::string::npos) {
                auto eq = line.find('=', pos);
                if (eq != std::string::npos) {
                    auto afterEq = line.find_first_not_of(" \t", eq + 1);
                    if (afterEq != std::string::npos) {
                        std::string valStr;
                        if (line[afterEq] == '"') {
                            auto endQ = line.find('"', afterEq + 1);
                            if (endQ != std::string::npos) {
                                valStr = line.substr(afterEq + 1, endQ - afterEq - 1);
                            }
                        } else {
                            auto endVal = line.find_first_of(",} \t\r\n", afterEq);
                            valStr = (endVal != std::string::npos) ? line.substr(afterEq, endVal - afterEq) : line.substr(afterEq);
                        }
                        if (!valStr.empty()) {
                            n.customParams[ck] = valStr;
                            registerTrackerParamCol(ck);
                        }
                    }
                }
            }
        }

        newNotes.push_back(n);
    }

    if (!newNotes.empty()) {
        notes_ = std::move(newNotes);
        updateTrackerLanesFromNotes();
        if (!isSyncing_ && onNotesChanged) {
            onNotesChanged(activeTrackIndex_, activeClipIndex_);
        }
    }
}

void EditView::auditionPitch(int pitch, float velocity, const ViewContext& ctx, bool isPreEvaluated) {
    (void)isPreEvaluated;
    auditioningPitch_ = pitch;
    if (ctx.audioEngine) {
        ctx.audioEngine->postNoteOn(static_cast<uint8_t>(std::clamp(pitch, 0, 127)), velocity, false, false, static_cast<int>(activeTrackIndex_), false);
    }
    pianoKeyboard_.triggerNoteOn(pitch, velocity, nullptr, activeTrackIndex_);
}

void EditView::stopAuditionPitch(int pitch, const ViewContext& ctx, bool isPreEvaluated) {
    (void)isPreEvaluated;
    if (auditioningPitch_ == pitch) {
        auditioningPitch_ = -1;
    }
    if (ctx.audioEngine) {
        ctx.audioEngine->postNoteOff(static_cast<uint8_t>(std::clamp(pitch, 0, 127)), static_cast<int>(activeTrackIndex_), false);
    }
    pianoKeyboard_.triggerNoteOff(pitch, nullptr, activeTrackIndex_);
}

void EditView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    float headerH = 34.0f;
    headerBounds_ = Rect2D(bounds_.x, bounds_.y, bounds_.w, headerH);

    bool showSidebar = hasSelectedNotes();
    float sidebarW = showSidebar ? 265.0f : 0.0f;
    float gutterW = 80.0f;
    float velocityH = 72.0f;

    if (clayContext_ && clayContext_->isInitialized()) {
        clayContext_->setDimensions(bounds_.w, bounds_.h);
        clayContext_->beginLayout();

        CLAY(CLAY_ID("EditViewRoot"), {
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(bounds_.w), CLAY_SIZING_FIXED(bounds_.h) },
                .childGap = 0,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            }
        }) {
            // Header
            CLAY(CLAY_ID("EditHeader"), {
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(bounds_.w), CLAY_SIZING_FIXED(headerH) },
                }
            }) {}

            // Main row: Content + Note Inspector Sidebar
            CLAY(CLAY_ID("EditBody"), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .childGap = 0,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                }
            }) {
                CLAY(CLAY_ID("EditContent"), {
                    .layout = {
                        .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                        .childGap = 0,
                        .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    }
                }) {
                    CLAY(CLAY_ID("EditRollRow"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                            .childGap = 0,
                            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        }
                    }) {
                        CLAY(CLAY_ID("PianoGutter"), {
                            .layout = {
                                .sizing = { CLAY_SIZING_FIXED(gutterW), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                        CLAY(CLAY_ID("EditGrid"), {
                            .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                            }
                        }) {}
                    }

                    CLAY(CLAY_ID("VelocityLaneRow"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(velocityH) },
                            .childGap = 0,
                            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        }
                    }) {
                        CLAY(CLAY_ID("VelocityGutterSpacer"), {
                            .layout = {
                                .sizing = { CLAY_SIZING_FIXED(gutterW), CLAY_SIZING_FIXED(velocityH) },
                            }
                        }) {}
                        CLAY(CLAY_ID("VelocityLane"), {
                            .layout = {
                                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(velocityH) },
                            }
                        }) {}
                    }
                }

                if (showSidebar) {
                    CLAY(CLAY_ID("NoteInspectorSidebar"), {
                        .layout = {
                            .sizing = { CLAY_SIZING_FIXED(sidebarW), CLAY_SIZING_GROW(0) },
                        }
                    }) {}
                }
            }
        }

        clayContext_->endLayout(0.016f);

        auto hdrData = clayContext_->getElementData(CLAY_ID("EditHeader"));
        if (hdrData.found) {
            headerBounds_ = Rect2D(bounds_.x + hdrData.boundingBox.x, bounds_.y + hdrData.boundingBox.y,
                                   hdrData.boundingBox.width, hdrData.boundingBox.height);
        }
        auto cntData = clayContext_->getElementData(CLAY_ID("EditContent"));
        if (cntData.found) {
            contentBounds_ = Rect2D(bounds_.x + cntData.boundingBox.x, bounds_.y + cntData.boundingBox.y,
                                    cntData.boundingBox.width, cntData.boundingBox.height);
        }
        auto sbData = clayContext_->getElementData(CLAY_ID("NoteInspectorSidebar"));
        if (sbData.found) {
            sidebarBounds_ = Rect2D(bounds_.x + sbData.boundingBox.x, bounds_.y + sbData.boundingBox.y,
                                    sbData.boundingBox.width, sbData.boundingBox.height);
        } else {
            sidebarBounds_ = Rect2D(bounds_.x + bounds_.w, bounds_.y + headerH, 0.0f, bounds_.h - headerH);
        }
        auto gutData = clayContext_->getElementData(CLAY_ID("PianoGutter"));
        if (gutData.found) {
            pianoGutterBounds_ = Rect2D(bounds_.x + gutData.boundingBox.x, bounds_.y + gutData.boundingBox.y,
                                       gutData.boundingBox.width, gutData.boundingBox.height);
        }
        auto gridData = clayContext_->getElementData(CLAY_ID("EditGrid"));
        if (gridData.found) {
            gridBounds_ = Rect2D(bounds_.x + gridData.boundingBox.x, bounds_.y + gridData.boundingBox.y,
                                 gridData.boundingBox.width, gridData.boundingBox.height);
        }
        auto velData = clayContext_->getElementData(CLAY_ID("VelocityLane"));
        if (velData.found) {
            velocityLaneBounds_ = Rect2D(bounds_.x + velData.boundingBox.x, bounds_.y + velData.boundingBox.y,
                                         velData.boundingBox.width, velData.boundingBox.height);
        }
    } else {
        contentBounds_ = Rect2D(bounds_.x, bounds_.y + headerH, bounds_.w - sidebarW, bounds_.h - headerH);
        sidebarBounds_ = Rect2D(bounds_.x + bounds_.w - sidebarW, bounds_.y + headerH, sidebarW, bounds_.h - headerH);

        pianoGutterBounds_ = Rect2D(contentBounds_.x, contentBounds_.y, gutterW, contentBounds_.h - velocityH);
        gridBounds_ = Rect2D(contentBounds_.x + gutterW, contentBounds_.y, contentBounds_.w - gutterW, contentBounds_.h - velocityH);
        velocityLaneBounds_ = Rect2D(contentBounds_.x + gutterW, contentBounds_.y + contentBounds_.h - velocityH, contentBounds_.w - gutterW, velocityH);
    }

    float maxScroll = std::max(0.0f, static_cast<float>(maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
    scrollY_ = std::clamp(scrollY_, 0.0f, maxScroll);

    pianoKeyboard_.setMinPitch(minPitch_);
    pianoKeyboard_.setMaxPitch(maxPitch_);
    pianoKeyboard_.setKeyHeight(semitoneHeight_);
    pianoKeyboard_.setKeyWidth(pianoGutterBounds_.w);

    float scToolY = contentBounds_.y + 6.0f;
    btnClefAuto_ = Rect2D(contentBounds_.x + 12.0f, scToolY, 52.0f, 24.0f);
    btnClefTreble_ = Rect2D(btnClefAuto_.x + 56.0f, scToolY, 58.0f, 24.0f);
    btnClefBass_ = Rect2D(btnClefTreble_.x + 62.0f, scToolY, 52.0f, 24.0f);

    float durStartX = btnClefBass_.x + 70.0f;
    btnDurWhole_ = Rect2D(durStartX, scToolY, 36.0f, 24.0f);
    btnDurHalf_ = Rect2D(btnDurWhole_.x + 40.0f, scToolY, 36.0f, 24.0f);
    btnDurQuarter_ = Rect2D(btnDurHalf_.x + 40.0f, scToolY, 36.0f, 24.0f);
    btnDurEighth_ = Rect2D(btnDurQuarter_.x + 40.0f, scToolY, 36.0f, 24.0f);
    btnDur16th_ = Rect2D(btnDurEighth_.x + 40.0f, scToolY, 36.0f, 24.0f);

    btnScriptApply_ = Rect2D(contentBounds_.x + 14.0f, contentBounds_.y + 8.0f, 150.0f, 26.0f);
    btnScriptRevert_ = Rect2D(btnScriptApply_.x + 158.0f, contentBounds_.y + 8.0f, 120.0f, 26.0f);

    scriptEditorBounds_ = Rect2D(contentBounds_.x + 10.0f, contentBounds_.y + 42.0f,
                                 std::max(0.0f, contentBounds_.w - 20.0f),
                                 std::max(60.0f, contentBounds_.h - 74.0f));
    scriptEditor_.layout(scriptEditorBounds_, ctx);

    // Sub-view Switcher & Ghost/Bake Tool Buttons in Top Header Bar
    float btnH = 26.0f;
    float btnY = headerBounds_.y + (headerBounds_.h - btnH) * 0.5f;

    float rightMargin = (headerBounds_.w >= 1200.0f) ? 22.0f : 10.0f;
    float btnW = (headerBounds_.w >= 1200.0f) ? 98.0f : ((headerBounds_.w < 900.0f || ctx.isMobile) ? 58.0f : 80.0f);
    float btnGap = (headerBounds_.w >= 1200.0f) ? 12.0f : 6.0f;

    float curRightX = headerBounds_.x + headerBounds_.w - rightMargin;

    curRightX -= btnW;
    btnScript_ = Rect2D(curRightX, btnY, btnW, btnH);

    curRightX -= (btnGap + btnW);
    btnScore_ = Rect2D(curRightX, btnY, btnW, btnH);

    curRightX -= (btnGap + btnW);
    btnTracker_ = Rect2D(curRightX, btnY, btnW, btnH);

    curRightX -= (btnGap + btnW);
    btnPianoRoll_ = Rect2D(curRightX, btnY, btnW, btnH);

    float toolRightX = btnPianoRoll_.x - 16.0f;
    float ghostSliderW = 46.0f;
    toolRightX -= ghostSliderW;
    ghostSliderBounds_ = Rect2D(toolRightX, btnY, ghostSliderW, btnH);

    float ghostToggleW = 50.0f;
    toolRightX -= (ghostToggleW + 4.0f);
    ghostToggleBtn_ = Rect2D(toolRightX, btnY, ghostToggleW, btnH);

    float bakeW = 74.0f;
    toolRightX -= (bakeW + 8.0f);
    btnBake_ = Rect2D(toolRightX, btnY, bakeW, btnH);
}

void EditView::render(const ViewContext& ctx) {
    if (kineticScroller_.isGliding()) {
        float dx = 0.0f, dy = 0.0f;
        kineticScroller_.step(ctx.dt > 0.0f ? ctx.dt : 0.016f, dx, dy);
        if (subView_ == EditSubViewMode::PianoRoll) {
            scrollX_ = (std::max)(0.0f, scrollX_ - dx);
            float maxScrollY = (std::max)(0.0f, (maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
            scrollY_ = std::clamp(scrollY_ - dy, 0.0f, maxScrollY);
        } else if (subView_ == EditSubViewMode::Score) {
            scoreScrollX_ = (std::max)(0.0f, scoreScrollX_ - dx);
        } else if (subView_ == EditSubViewMode::Tracker) {
            trackerScrollY_ = (std::max)(0.0f, trackerScrollY_ - dy);
        }
    }

    if (dragMode_ == DragMode::TouchGridPending && !touchPanCommitted_ && !touchLongPressArmed_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - touchDownTimePoint_).count();
        if (elapsedMs >= 400) {
            touchLongPressArmed_ = true;
            dragMode_ = DragMode::MarqueeSelect;
            dragStartPoint_ = touchDownPos_;
            marqueeCurrentPoint_ = touchDownPos_;
            clearSelection();
        }
    }

    renderSubNavHeader(ctx);

    switch (subView_) {
        case EditSubViewMode::PianoRoll:
            renderPianoRoll(ctx);
            break;
        case EditSubViewMode::Tracker:
            renderTracker(ctx);
            break;
        case EditSubViewMode::Score:
            renderScore(ctx);
            break;
        case EditSubViewMode::Script:
            renderScript(ctx);
            break;
    }

    if (hasSelectedNotes()) {
        renderNoteInspectorSidebar(ctx);
    }

    if (trackerCellDialog_.isOpen()) {
        trackerCellDialog_.layout(bounds_.w, bounds_.h);
        trackerCellDialog_.render(*ctx.renderer, *ctx.theme);
    }
}

void EditView::renderSubNavHeader(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h, theme.panelHeader);
    drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h, headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
             theme.borderSubtle, 0.8f, 1.5f);

    drawCircle(r, headerBounds_.x + 16.0f, headerBounds_.y + 17.0f, 5.0f, trackColor_);
    std::string editTitle = ctx.isMobile ? activeTrackName_ : ("EDITING: " + activeTrackName_);
    if (!activeClipName_.empty()) {
        editTitle += " - " + activeClipName_;
    }
    drawText(r, editTitle, headerBounds_.x + 28.0f, headerBounds_.y + 10.0f, 11.5f, theme.textPrimary);

    if (subView_ == EditSubViewMode::PianoRoll || subView_ == EditSubViewMode::Score) {
        if (hasActiveChordFollowOrMidiFx()) {
            drawRoundedRect(r, btnBake_.x, btnBake_.y, btnBake_.w, btnBake_.h, 4.0f, Color(0.24f, 0.18f, 0.06f, 0.95f), 0.95f);
            drawRoundedRectOutline(r, btnBake_.x, btnBake_.y, btnBake_.w, btnBake_.h, 4.0f, Color(1.0f, 0.82f, 0.20f, 0.90f), 1.0f, 1.5f);
            drawCenteredText(r, "[ ⚡ BAKE ]", btnBake_, 9.5f, Color(1.0f, 0.88f, 0.30f));
        }

        bool isGhostActive = (ghostOpacity_ > 0.001f);
        drawRoundedRect(r, ghostToggleBtn_.x, ghostToggleBtn_.y, ghostToggleBtn_.w, ghostToggleBtn_.h, 4.0f,
                        isGhostActive ? (theme.primaryAccent * 0.25f) : theme.controlBackground, 0.85f);
        drawRoundedRectOutline(r, ghostToggleBtn_.x, ghostToggleBtn_.y, ghostToggleBtn_.w, ghostToggleBtn_.h, 4.0f,
                               isGhostActive ? theme.primaryAccent : theme.borderSubtle, 0.9f, 1.0f);
        drawCenteredText(r, "GHOST", ghostToggleBtn_, 9.5f,
                         isGhostActive ? theme.primaryAccent : theme.textMuted);

        drawRoundedRect(r, ghostSliderBounds_.x, ghostSliderBounds_.y, ghostSliderBounds_.w, ghostSliderBounds_.h, 4.0f,
                        theme.backgroundDark, 0.8f);
        float fillW = ghostSliderBounds_.w * ghostOpacity_;
        if (fillW > 0.0f) {
            drawRoundedRect(r, ghostSliderBounds_.x, ghostSliderBounds_.y, fillW, ghostSliderBounds_.h, 4.0f,
                            theme.primaryAccent * 0.35f, 0.95f);
        }
        std::string ghostPct = isGhostActive ? (std::to_string(static_cast<int>(ghostOpacity_ * 100.0f)) + "%") : "OFF";
        drawCenteredText(r, ghostPct, ghostSliderBounds_, 9.5f,
                         isGhostActive ? theme.primaryAccent : theme.textMuted);
    }

    auto drawSubBtn = [&](const Rect2D& b, const std::string& label, bool active, const Color& accent) {
        if (active) {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, accent * 0.35f, 0.95f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, accent, 1.0f, 1.5f);
            drawLine(r, b.x + 3.0f, b.y + b.h - 2.0f, b.x + b.w - 3.0f, b.y + b.h - 2.0f, accent, 1.0f, 2.0f);
            drawCenteredText(r, label, b, 10.0f, accent);
        } else {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, theme.controlBackground, 0.85f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, theme.borderSubtle, 0.5f, 1.0f);
            drawCenteredText(r, label, b, 10.0f, theme.textMuted);
        }
    };

    drawSubBtn(btnPianoRoll_, ctx.isMobile ? "PIANO" : "PIANO ROLL", subView_ == EditSubViewMode::PianoRoll, theme.primaryAccent);
    drawSubBtn(btnTracker_, ctx.isMobile ? "TRACK" : "TRACKER", subView_ == EditSubViewMode::Tracker, theme.secondaryAccent);
    drawSubBtn(btnScore_, ctx.isMobile ? "SCORE" : "SCORE", subView_ == EditSubViewMode::Score, theme.secondaryAccent);
    drawSubBtn(btnScript_, ctx.isMobile ? "SCRIPT" : "SCRIPT", subView_ == EditSubViewMode::Script, theme.primaryAccent);
}

void EditView::renderPianoRoll(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    // 1-6. Grid Area (Background, Rows, Steps, Ghosts, FX Evaluated Notes, Note Blocks, Marquee, Playhead)
    {
        BatchRenderer2D::ScissorScope gridScissor(r, gridBounds_);

        // 1. Grid Background
        drawRect(r, gridBounds_.x, gridBounds_.y, gridBounds_.w, gridBounds_.h, theme.backgroundDark);

        for (int p = minPitch_; p <= maxPitch_; ++p) {
            float py = gridBounds_.y + static_cast<float>(maxPitch_ - p) * semitoneHeight_ - scrollY_;
            if (py + semitoneHeight_ < gridBounds_.y || py > gridBounds_.y + gridBounds_.h) continue;

            bool isBlack = PianoKeyboard::isBlackKey(p);
            if (isBlack) {
                drawRect(r, gridBounds_.x, py, gridBounds_.w, semitoneHeight_, theme.backgroundDark * 0.75f, 0.7f);
            } else {
                drawRect(r, gridBounds_.x, py, gridBounds_.w, semitoneHeight_, theme.panelBackground * 0.85f, 0.35f);
            }

            float alpha = (p % 12 == 0) ? 0.6f : 0.2f;
            drawLine(r, gridBounds_.x, py + semitoneHeight_, gridBounds_.x + gridBounds_.w, py + semitoneHeight_,
                     theme.gridLineMinor, alpha, (p % 12 == 0) ? 1.5f : 1.0f);
        }

        // 2. Vertical Grid Steps & Bar Lines
        float maxNoteStep = 64.0f;
        for (const auto& n : notes_) {
            maxNoteStep = std::max(maxNoteStep, n.startStep + n.durationSteps);
        }
        int totalGridSteps = std::clamp(static_cast<int>(std::ceil(maxNoteStep / 16.0f) * 16.0f), 64, 128);

        for (int s = 0; s < totalGridSteps; ++s) {
            float sx = gridBounds_.x + static_cast<float>(s) * stepWidth_ - scrollX_;
            if (sx < gridBounds_.x || sx > gridBounds_.x + gridBounds_.w) continue;

            bool isBar = (s % 16 == 0);
            bool isBeat = (s % 4 == 0);
            float alpha = isBar ? 0.8f : (isBeat ? 0.4f : 0.15f);
            float lineW = isBar ? 1.8f : 1.0f;

            drawLine(r, sx, gridBounds_.y, sx, gridBounds_.y + gridBounds_.h,
                     isBar ? theme.primaryAccent : theme.gridLineMinor, alpha, lineW);

            if (isBeat) {
                std::string stepLabel = std::to_string(s / 4 + 1) + (isBar ? ".1" : ("." + std::to_string(s % 4 + 1)));
                drawText(r, stepLabel, sx + 4.0f, gridBounds_.y + 4.0f, 8.5f,
                         isBar ? theme.highlight : theme.textMuted, 0.7f);
            }
        }

        // 3. Ghost Notes
        if (ghostOpacity_ > 0.01f) {
            for (const auto& gn : ghostNotes_) {
                float gx = gridBounds_.x + gn.startStep * stepWidth_ - scrollX_;
                float gw = gn.durationSteps * stepWidth_ - 2.0f;
                float gy = gridBounds_.y + static_cast<float>(maxPitch_ - gn.pitch) * semitoneHeight_ - scrollY_ + 2.0f;
                float gh = semitoneHeight_ - 4.0f;

                if (gx + gw < gridBounds_.x || gx > gridBounds_.x + gridBounds_.w) continue;
                if (gy + gh < gridBounds_.y || gy > gridBounds_.y + gridBounds_.h) continue;

                drawRoundedRect(r, gx, gy, gw, gh, 3.0f, theme.secondaryAccent, 0.35f * ghostOpacity_);
                drawRoundedRectOutline(r, gx, gy, gw, gh, 3.0f, theme.secondaryAccent, 0.75f * ghostOpacity_, 1.0f);
            }
        }

        // 3.5 Real-Time Harmonic Chord Follow & MIDI FX Evaluated Notes Layer (Eatsbeats parity)
        if (hasActiveChordFollowOrMidiFx()) {
            for (const auto& ev : evaluatedFxNotes_) {
                bool matchesBase = false;
                for (const auto& bn : notes_) {
                    if (std::abs(bn.startStep - ev.startStep) < 0.05f && bn.pitch == ev.pitch) {
                        matchesBase = true;
                        break;
                    }
                }
                if (matchesBase) continue; // Unchanged base note

                float ex = gridBounds_.x + ev.startStep * stepWidth_ - scrollX_;
                float ew = std::max(6.0f, ev.durationSteps * stepWidth_ - 2.0f);
                float ey = gridBounds_.y + static_cast<float>(maxPitch_ - ev.pitch) * semitoneHeight_ - scrollY_ + 1.5f;
                float eh = semitoneHeight_ - 3.0f;

                if (ex + ew < gridBounds_.x || ex > gridBounds_.x + gridBounds_.w) continue;
                if (ey + eh < gridBounds_.y || ey > gridBounds_.y + gridBounds_.h) continue;

                bool isChordMode = (chordFollowMode_ != theory::ChordFollowMode::Off);
                Color fxBodyCol = isChordMode ? Color(0.95f, 0.72f, 0.12f, 0.28f) : Color(0.0f, 0.88f, 0.98f, 0.25f);
                Color fxOutlineCol = isChordMode ? Color(1.0f, 0.85f, 0.25f, 0.92f) : Color(0.0f, 0.95f, 1.0f, 0.92f);

                drawRoundedRect(r, ex, ey, ew, eh, 3.5f, fxBodyCol, 0.90f);
                drawRoundedRectOutline(r, ex, ey, ew, eh, 3.5f, fxOutlineCol, 0.95f, 1.5f);

                std::string evLabel = PianoKeyboard::getNoteName(ev.pitch);
                if (isChordMode) evLabel += " ~CHORD";
                else evLabel += " ~FX";

                drawText(r, evLabel, ex + 4.0f, ey + 3.5f, 9.0f, fxOutlineCol, 0.95f);
            }
        }

        // 4. Interactive Note Blocks
        for (const auto& n : notes_) {
            float nx = gridBounds_.x + n.startStep * stepWidth_ - scrollX_;
            float nw = n.durationSteps * stepWidth_ - 2.0f;
            float ny = gridBounds_.y + static_cast<float>(maxPitch_ - n.pitch) * semitoneHeight_ - scrollY_ + 1.5f;
            float nh = semitoneHeight_ - 3.0f;

            if (nx + nw < gridBounds_.x || nx > gridBounds_.x + gridBounds_.w) continue;
            if (ny + nh < gridBounds_.y || ny > gridBounds_.y + gridBounds_.h) continue;

            // Check if pitch was harmonized/remapped by Chord Follow or MIDI FX
            int harmonizedPitch = n.pitch;
            bool isPitchRemapped = false;
            if (hasActiveChordFollowOrMidiFx()) {
                for (const auto& ev : evaluatedFxNotes_) {
                    if (std::abs(ev.startStep - n.startStep) < 0.05f) {
                        harmonizedPitch = ev.pitch;
                        if (harmonizedPitch != n.pitch) {
                            isPitchRemapped = true;
                        }
                        break;
                    }
                }
            }

            float alpha = n.isSelected ? 1.0f : 0.88f;
            Color baseCol = trackColor_;
            float v = std::clamp(n.velocity, 0.08f, 1.0f);
            float velIntensity = 0.35f + 0.65f * v;
            Color noteCol(std::clamp(baseCol.r * velIntensity, 0.0f, 1.0f),
                          std::clamp(baseCol.g * velIntensity, 0.0f, 1.0f),
                          std::clamp(baseCol.b * velIntensity, 0.0f, 1.0f),
                          baseCol.a);
            drawRoundedRect(r, nx, ny, nw, nh, 3.5f, noteCol, alpha);

            if (n.isSelected) {
                drawRoundedRectOutline(r, nx, ny, nw, nh, 3.5f, Color(1.0f, 1.0f, 1.0f), 1.0f, 2.0f);
            } else if (isPitchRemapped) {
                Color harmCol = (chordFollowMode_ != theory::ChordFollowMode::Off)
                    ? Color(1.0f, 0.84f, 0.20f, 0.95f)
                    : Color(0.0f, 0.95f, 1.0f, 0.95f);
                drawRoundedRectOutline(r, nx, ny, nw, nh, 3.5f, harmCol, 1.0f, 1.8f);

                float targetY = gridBounds_.y + static_cast<float>(maxPitch_ - harmonizedPitch) * semitoneHeight_ - scrollY_ + 1.5f;
                drawLine(r, nx + nw * 0.5f, ny + nh * 0.5f, nx + nw * 0.5f, targetY + nh * 0.5f,
                         harmCol * 0.7f, 0.55f, 1.0f);
            } else {
                drawRoundedRectOutline(r, nx, ny, nw, nh, 3.5f, noteCol * 1.3f, 0.5f + 0.4f * v, 1.0f);
            }

            std::string nName = PianoKeyboard::getNoteName(n.pitch);
            if (isPitchRemapped) {
                nName += " -> " + PianoKeyboard::getNoteName(harmonizedPitch);
            } else if (chordFollowMode_ != theory::ChordFollowMode::Off) {
                nName += " [H]";
            }
            if (n.isSlide) nName += " ~SLD";
            drawText(r, nName, nx + 4.0f, ny + 3.5f, 9.5f, Color(0.1f, 0.1f, 0.1f), 0.95f);

            if (n.isSlide) {
                drawLine(r, nx + 4.0f, ny + nh - 4.0f, nx + std::min(nw - 4.0f, 32.0f), ny + 4.0f,
                         Color(0.2f, 0.2f, 0.2f), 0.75f, 2.0f);
            }
            if (n.isAccent) {
                drawCircle(r, nx + nw - 8.0f, ny + 6.0f, 2.5f, Color(0.95f, 0.2f, 0.2f));
            }

            if (nw > 14.0f) {
                drawLine(r, nx + nw - 4.0f, ny + 3.0f, nx + nw - 4.0f, ny + nh - 3.0f,
                         Color(0.0f, 0.0f, 0.0f), 0.45f, 1.5f);
            }
        }

        // 5. Marquee Selection Rectangle
        if (dragMode_ == DragMode::MarqueeSelect) {
            float mx = std::min(dragStartPoint_.x, marqueeCurrentPoint_.x);
            float my = std::min(dragStartPoint_.y, marqueeCurrentPoint_.y);
            float mw = std::abs(marqueeCurrentPoint_.x - dragStartPoint_.x);
            float mh = std::abs(marqueeCurrentPoint_.y - dragStartPoint_.y);

            drawRect(r, mx, my, mw, mh, theme.primaryAccent, 0.18f);
            drawRoundedRectOutline(r, mx, my, mw, mh, 2.0f, theme.primaryAccent, 0.9f, 1.5f);
        }

        // 6. Playhead Indicator
        if (ctx.audioEngine && ctx.audioEngine->getSequencer().isPlaying()) {
            float currentStep = static_cast<float>(ctx.audioEngine->getSequencer().getTransport().getCurrentStep());
            float px = gridBounds_.x + currentStep * stepWidth_ - scrollX_;
            if (px >= gridBounds_.x && px <= gridBounds_.x + gridBounds_.w) {
                drawLine(r, px, gridBounds_.y, px, gridBounds_.y + gridBounds_.h,
                         theme.playActive, 0.95f, 2.0f);
                drawRoundedRect(r, px - 5.0f, gridBounds_.y, 10.0f, 8.0f, 2.0f, theme.playActive, 1.0f);
            }
        }
    }

    // 7. Left Piano Keyboard Gutter (FL Studio styled unified PianoKeyboard)
    pianoKeyboard_.render(r, pianoGutterBounds_, theme, scrollY_);

    // 8. Velocity Lane
    {
        BatchRenderer2D::ScissorScope velScissor(r, velocityLaneBounds_);
        drawRect(r, velocityLaneBounds_.x, velocityLaneBounds_.y, velocityLaneBounds_.w, velocityLaneBounds_.h,
                 theme.panelBackground * 0.8f);
        drawLine(r, velocityLaneBounds_.x, velocityLaneBounds_.y, velocityLaneBounds_.x + velocityLaneBounds_.w, velocityLaneBounds_.y,
                 theme.borderSubtle, 0.85f, 1.5f);

        drawText(r, "VELOCITY", velocityLaneBounds_.x + 8.0f, velocityLaneBounds_.y + 6.0f, 9.0f,
                 theme.textMuted, 0.7f);

        for (const auto& n : notes_) {
            float sx = velocityLaneBounds_.x + n.startStep * stepWidth_ - scrollX_ + 4.0f;
            if (sx < velocityLaneBounds_.x || sx > velocityLaneBounds_.x + velocityLaneBounds_.w) continue;

            float stalkH = (velocityLaneBounds_.h - 26.0f) * n.velocity;
            float sy = velocityLaneBounds_.y + velocityLaneBounds_.h - stalkH - 6.0f;

            Color vCol = n.isSelected ? (trackColor_ * 1.3f) : trackColor_;
            drawLine(r, sx, velocityLaneBounds_.y + velocityLaneBounds_.h - 6.0f, sx, sy, vCol, 0.85f, 2.0f);
            drawCircle(r, sx, sy, 4.0f, vCol);
            if (n.isSelected) {
                drawCircleOutline(r, sx, sy, 4.0f, Color(1.0f, 1.0f, 1.0f), 1.0f, 1.5f);
            }
        }
    }
}

void EditView::renderTracker(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, contentBounds_.x, contentBounds_.y, contentBounds_.w, contentBounds_.h, theme.backgroundDark);

    float toolH = 32.0f;
    Rect2D toolBounds(contentBounds_.x, contentBounds_.y, contentBounds_.w, toolH);
    drawRect(r, toolBounds.x, toolBounds.y, toolBounds.w, toolBounds.h, theme.panelHeader, 0.95f);
    drawLine(r, toolBounds.x, toolBounds.y + toolH, toolBounds.x + toolBounds.w, toolBounds.y + toolH,
             theme.borderSubtle, 0.8f, 1.5f);

    std::string octText = "OCT: " + std::to_string(trackerBaseOctave_);
    drawText(r, octText, toolBounds.x + 12.0f, toolBounds.y + 9.0f, 10.0f, theme.highlight);

    btnOctDown_ = Rect2D(toolBounds.x + 62.0f, toolBounds.y + 5.0f, 20.0f, 22.0f);
    btnOctUp_ = Rect2D(toolBounds.x + 86.0f, toolBounds.y + 5.0f, 20.0f, 22.0f);
    drawRoundedRect(r, btnOctDown_.x, btnOctDown_.y, btnOctDown_.w, btnOctDown_.h, 3.0f,
                    trackerBaseOctave_ > 0 ? theme.controlBackground : (theme.controlBackground * 0.4f), 0.8f);
    drawText(r, "-", btnOctDown_.x + 7.0f, btnOctDown_.y + 5.0f, 11.0f,
             trackerBaseOctave_ > 0 ? theme.textPrimary : theme.textMuted);
    drawRoundedRect(r, btnOctUp_.x, btnOctUp_.y, btnOctUp_.w, btnOctUp_.h, 3.0f,
                    trackerBaseOctave_ < 8 ? theme.controlBackground : (theme.controlBackground * 0.4f), 0.8f);
    drawText(r, "+", btnOctUp_.x + 6.0f, btnOctUp_.y + 5.0f, 11.0f,
             trackerBaseOctave_ < 8 ? theme.textPrimary : theme.textMuted);

    std::string stepText = "STEP: " + std::to_string(trackerEditStep_);
    drawText(r, stepText, toolBounds.x + 118.0f, toolBounds.y + 9.0f, 10.0f, theme.secondaryAccent);

    btnStepDown_ = Rect2D(toolBounds.x + 170.0f, toolBounds.y + 5.0f, 20.0f, 22.0f);
    btnStepUp_ = Rect2D(toolBounds.x + 194.0f, toolBounds.y + 5.0f, 20.0f, 22.0f);
    drawRoundedRect(r, btnStepDown_.x, btnStepDown_.y, btnStepDown_.w, btnStepDown_.h, 3.0f,
                    trackerEditStep_ > 0 ? theme.controlBackground : (theme.controlBackground * 0.4f), 0.8f);
    drawText(r, "-", btnStepDown_.x + 7.0f, btnStepDown_.y + 5.0f, 11.0f,
             trackerEditStep_ > 0 ? theme.textPrimary : theme.textMuted);
    drawRoundedRect(r, btnStepUp_.x, btnStepUp_.y, btnStepUp_.w, btnStepUp_.h, 3.0f, theme.controlBackground, 0.8f);
    drawText(r, "+", btnStepUp_.x + 6.0f, btnStepUp_.y + 5.0f, 11.0f, theme.textPrimary);

    std::string laneText = "LANES: " + std::to_string(trackerNumLanes_);
    drawText(r, laneText, toolBounds.x + 226.0f, toolBounds.y + 9.0f, 10.0f, theme.primaryAccent);

    btnLaneDown_ = Rect2D(toolBounds.x + 286.0f, toolBounds.y + 5.0f, 20.0f, 22.0f);
    btnLaneUp_ = Rect2D(toolBounds.x + 310.0f, toolBounds.y + 5.0f, 20.0f, 22.0f);
    drawRoundedRect(r, btnLaneDown_.x, btnLaneDown_.y, btnLaneDown_.w, btnLaneDown_.h, 3.0f,
                    trackerNumLanes_ > 2 ? theme.controlBackground : (theme.controlBackground * 0.4f), 0.8f);
    drawText(r, "-", btnLaneDown_.x + 7.0f, btnLaneDown_.y + 5.0f, 11.0f,
             trackerNumLanes_ > 2 ? theme.textPrimary : theme.textMuted);
    drawRoundedRect(r, btnLaneUp_.x, btnLaneUp_.y, btnLaneUp_.w, btnLaneUp_.h, 3.0f, theme.controlBackground, 0.8f);
    drawText(r, "+", btnLaneUp_.x + 6.0f, btnLaneUp_.y + 5.0f, 11.0f, theme.textPrimary);

    btnToggleParams_ = Rect2D(toolBounds.x + 342.0f, toolBounds.y + 5.0f, 82.0f, 22.0f);
    drawRoundedRect(r, btnToggleParams_.x, btnToggleParams_.y, btnToggleParams_.w, btnToggleParams_.h, 3.0f,
                    trackerShowCustomParams_ ? (theme.primaryAccent * 0.35f) : theme.controlBackground, 0.8f);
    drawText(r, trackerShowCustomParams_ ? "CHIP FX: ON" : "CHIP FX", btnToggleParams_.x + 8.0f, btnToggleParams_.y + 6.0f, 9.5f,
             trackerShowCustomParams_ ? theme.highlight : theme.textMuted);

    btnFollow_ = Rect2D(toolBounds.x + toolBounds.w - 105.0f, toolBounds.y + 5.0f, 96.0f, 22.0f);
    drawRoundedRect(r, btnFollow_.x, btnFollow_.y, btnFollow_.w, btnFollow_.h, 3.0f,
                    trackerFollowPlayback_ ? (theme.playActive * 0.35f) : theme.controlBackground, 0.8f);
    drawText(r, trackerFollowPlayback_ ? "FOLLOW: ON" : "FOLLOW: OFF",
             btnFollow_.x + 12.0f, btnFollow_.y + 6.0f, 9.5f,
             trackerFollowPlayback_ ? theme.playActive : theme.textMuted);

    if (trackerHasBlockSelection_) {
        drawRoundedRect(r, toolBounds.x + 432.0f, toolBounds.y + 5.0f, 105.0f, 22.0f, 3.0f,
                        theme.primaryAccent * 0.25f, 0.9f);
        drawText(r, "BLOCK SELECTED", toolBounds.x + 438.0f, toolBounds.y + 9.0f, 9.5f, theme.primaryAccent);
    }

    float colHeaderY = toolBounds.y + toolH + 4.0f;
    float rowNumW = 48.0f;
    float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
    float colW = 140.0f + extraParamsW;
    float colSpacing = 6.0f;

    drawText(r, "STEP", contentBounds_.x + 12.0f, colHeaderY + 4.0f, 9.5f, theme.textMuted, 0.7f);

    float matrixStartX = contentBounds_.x + rowNumW;
    float matrixW = std::max(0.0f, contentBounds_.w - rowNumW);

    {
        BatchRenderer2D::ScissorScope colHeaderScissor(r, matrixStartX, colHeaderY, matrixW, 24.0f);
        for (int c = 0; c < trackerNumLanes_; ++c) {
            float cx = matrixStartX + static_cast<float>(c) * (colW + colSpacing) - trackerScrollX_;
            if (cx + colW < matrixStartX || cx > contentBounds_.x + contentBounds_.w) continue;

            bool isSelCol = (trackerSelectedCol_ == c);
            drawRoundedRect(r, cx, colHeaderY, colW, 20.0f, 3.0f,
                            isSelCol ? (theme.primaryAccent * 0.25f) : theme.controlBackground, 0.8f);

            std::string colLabel = "LANE " + std::to_string(c + 1);
            if (trackerShowCustomParams_ && !trackerCustomCols_.empty()) {
                colLabel += " (NOTE/V/S/FX";
                for (const auto& cp : trackerCustomCols_) {
                    colLabel += "/" + cp.header;
                }
                colLabel += ")";
            } else {
                colLabel += " (NOTE/VOL/SLD/FX)";
            }
            drawText(r, colLabel, cx + 6.0f, colHeaderY + 4.0f, 9.0f,
                     isSelCol ? theme.highlight : theme.textPrimary);
        }
    }

    float startRowY = colHeaderY + 26.0f;
    float rowH = 24.0f;
    int maxRows = 64;

    if (trackerFollowPlayback_ && ctx.audioEngine && ctx.audioEngine->getSequencer().isPlaying()) {
        uint32_t curStep = ctx.audioEngine->getSequencer().getTransport().getCurrentStep() % 64;
        float curStepY = static_cast<float>(curStep) * rowH;
        float visibleH = std::max(100.0f, contentBounds_.h - 62.0f);
        trackerScrollY_ = std::max(0.0f, curStepY - visibleH * 0.5f);
    }

    BatchRenderer2D::ScissorScope matrixScissor(r, contentBounds_.x, startRowY, contentBounds_.w, std::max(0.0f, contentBounds_.y + contentBounds_.h - startRowY));

    for (int step = 0; step < maxRows; ++step) {
        float ry = startRowY + static_cast<float>(step) * rowH - trackerScrollY_;
        if (ry + rowH < startRowY || ry > contentBounds_.y + contentBounds_.h) continue;

        bool isBeat = (step % 4 == 0);
        bool isSelectedRow = (trackerSelectedStep_ == step);
        bool isPlayheadStep = (ctx.audioEngine && ctx.audioEngine->getSequencer().isPlaying() &&
                               (ctx.audioEngine->getSequencer().getTransport().getCurrentStep() % 64) == static_cast<uint32_t>(step));

        if (isPlayheadStep) {
            drawRect(r, contentBounds_.x + 2.0f, ry, contentBounds_.w - 4.0f, rowH - 2.0f,
                     theme.playActive, 0.22f);
            drawRect(r, contentBounds_.x + 2.0f, ry, 6.0f, rowH - 2.0f, theme.playActive, 1.0f);
        } else if (isSelectedRow) {
            drawRect(r, contentBounds_.x + 4.0f, ry, contentBounds_.w - 8.0f, rowH - 2.0f,
                     theme.primaryAccent, 0.15f);
        } else if (isBeat) {
            drawRect(r, contentBounds_.x + 4.0f, ry, contentBounds_.w - 8.0f, rowH - 2.0f,
                     theme.panelBackground, 0.4f);
        }

        std::string stepStr = (step < 10 ? "0" : "") + std::to_string(step);
        drawMonoText(r, stepStr, contentBounds_.x + 14.0f, ry + 5.0f, 10.5f,
                     isPlayheadStep ? theme.playActive : (isSelectedRow ? theme.highlight : (isBeat ? theme.highlight : theme.textMuted)),
                     isPlayheadStep ? 1.0f : (isSelectedRow ? 1.0f : (isBeat ? 0.9f : 0.6f)));

        for (int c = 0; c < trackerNumLanes_; ++c) {
            float cx = matrixStartX + static_cast<float>(c) * (colW + colSpacing) - trackerScrollX_;
            if (cx + colW < matrixStartX || cx > contentBounds_.x + contentBounds_.w) continue;

            bool isCellSelected = (isSelectedRow && trackerSelectedCol_ == c);
            bool inBlock = false;
            if (trackerHasBlockSelection_) {
                int minStep = std::min(trackerBlockAnchorStep_, trackerSelectedStep_);
                int maxStep = std::max(trackerBlockAnchorStep_, trackerSelectedStep_);
                int minCol = std::min(trackerBlockAnchorCol_, trackerSelectedCol_);
                int maxCol = std::max(trackerBlockAnchorCol_, trackerSelectedCol_);
                if (step >= minStep && step <= maxStep && c >= minCol && c <= maxCol) {
                    inBlock = true;
                }
            }

            std::string noteStr = "---";
            std::string volStr = "..";
            std::string sldStr = ".";
            std::string fxStr = "00";
            bool hasNote = false;
            bool noteIsSelected = false;
            const PianoRollNote* matchedNote = nullptr;

            for (const auto& n : notes_) {
                if (static_cast<int>(std::round(n.startStep)) == step && n.column == c) {
                    noteStr = PianoKeyboard::getNoteName(n.pitch);
                    volStr = "V" + std::to_string(static_cast<int>(n.velocity * 99.0f));
                    sldStr = n.isSlide ? "S" : ".";
                    fxStr = n.effectCommand;
                    hasNote = true;
                    noteIsSelected = n.isSelected;
                    matchedNote = &n;
                    break;
                }
            }

            if (isCellSelected || noteIsSelected || inBlock) {
                drawRoundedRect(r, cx, ry, colW, rowH - 2.0f, 3.0f,
                                theme.primaryAccent * 0.35f, 0.85f);
                drawRoundedRectOutline(r, cx, ry, colW, rowH - 2.0f, 3.0f,
                                       theme.primaryAccent, 1.0f, 1.5f);
            } else {
                drawRoundedRect(r, cx, ry, colW, rowH - 2.0f, 3.0f,
                                theme.controlBackground, 0.4f);
            }

            std::string cellText = noteStr + " " + volStr + " " + sldStr + " " + fxStr;
            if (trackerShowCustomParams_) {
                for (const auto& cp : trackerCustomCols_) {
                    std::string val = "..";
                    if (matchedNote) {
                        auto it = matchedNote->customParams.find(cp.key);
                        if (it != matchedNote->customParams.end() && !it->second.empty()) {
                            val = it->second;
                            if (val.size() > 2) val = val.substr(0, 2);
                        }
                    }
                    cellText += " " + val;
                }
            }

            Color cellCol = hasNote ? (noteIsSelected ? (trackColor_ * 1.3f) : trackColor_) : theme.textMuted;
            drawMonoText(r, cellText, cx + 8.0f, ry + 5.0f, 10.5f, cellCol, hasNote ? 1.0f : 0.6f);
        }
    }
}

void EditView::renderScore(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, contentBounds_.x, contentBounds_.y, contentBounds_.w, contentBounds_.h,
             Color(0.96f, 0.96f, 0.94f));

    float toolH = 34.0f;
    drawRect(r, contentBounds_.x, contentBounds_.y, contentBounds_.w, toolH, theme.panelHeader);
    drawLine(r, contentBounds_.x, contentBounds_.y + toolH, contentBounds_.x + contentBounds_.w, contentBounds_.y + toolH,
             theme.borderSubtle, 0.9f, 1.5f);

    auto drawScoreToolBtn = [&](const Rect2D& b, const std::string& label, bool active) {
        if (active) {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 3.0f, theme.primaryAccent * 0.35f, 0.95f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 3.0f, theme.primaryAccent, 1.0f, 1.0f);
            drawText(r, label, b.x + 6.0f, b.y + 6.0f, 9.5f, theme.primaryAccent);
        } else {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 3.0f, theme.controlBackground, 0.8f);
            drawText(r, label, b.x + 6.0f, b.y + 6.0f, 9.5f, theme.textMuted, 0.8f);
        }
    };

    drawScoreToolBtn(btnClefAuto_, "AUTO", scoreClef_ == ScoreClef::Auto);
    drawScoreToolBtn(btnClefTreble_, "TREBLE", scoreClef_ == ScoreClef::Treble);
    drawScoreToolBtn(btnClefBass_, "BASS", scoreClef_ == ScoreClef::Bass);

    drawScoreToolBtn(btnDurWhole_, "1/1", scoreDuration_ == ScoreNoteType::Whole);
    drawScoreToolBtn(btnDurHalf_, "1/2", scoreDuration_ == ScoreNoteType::Half);
    drawScoreToolBtn(btnDurQuarter_, "1/4", scoreDuration_ == ScoreNoteType::Quarter);
    drawScoreToolBtn(btnDurEighth_, "1/8", scoreDuration_ == ScoreNoteType::Eighth);
    drawScoreToolBtn(btnDur16th_, "1/16", scoreDuration_ == ScoreNoteType::Sixteenth);

    float staffStartX = contentBounds_.x + 50.0f;
    float staffEndX = contentBounds_.x + contentBounds_.w - 40.0f;
    float staffY = contentBounds_.y + 110.0f;
    float lineSpacing = 14.0f;

    bool isTreble = (scoreClef_ == ScoreClef::Treble) || (scoreClef_ == ScoreClef::Auto && activeTrackIndex_ != 0 && activeTrackIndex_ != 2);
    ScoreVectorGlyphs::setBatchRenderer(&r);
    Color clefCol(0.15f, 0.15f, 0.18f, 0.95f);

    if (isTreble) {
        // Treble G-clef centered around G4 (2nd line from bottom = line 3 from top = staffY + 3.0f * lineSpacing)
        ScoreVectorGlyphs::drawTrebleClef(staffStartX + 12.0f, staffY + 3.0f * lineSpacing, lineSpacing,
                                          clefCol.r, clefCol.g, clefCol.b, clefCol.a);
    } else {
        // Bass F-clef centered around F3 (2nd line from top = line 1 from top = staffY + 1.0f * lineSpacing)
        ScoreVectorGlyphs::drawBassClef(staffStartX + 12.0f, staffY + 1.0f * lineSpacing, lineSpacing,
                                        clefCol.r, clefCol.g, clefCol.b, clefCol.a);
    }

    drawText(r, "4 / 4", staffStartX + 120.0f, staffY + 16.0f, 15.0f, Color(0.2f, 0.2f, 0.2f));

    for (int l = 0; l < 5; ++l) {
        float ly = staffY + static_cast<float>(l) * lineSpacing;
        drawLine(r, staffStartX, ly, staffEndX, ly, Color(0.18f, 0.18f, 0.18f), 1.0f, 1.5f);
    }

    float scoreStepW = 34.0f;
    for (int m = 0; m <= 4; ++m) {
        float barX = staffStartX + 160.0f + static_cast<float>(m * 16) * scoreStepW - scoreScrollX_;
        if (barX >= staffStartX && barX <= staffEndX) {
            drawLine(r, barX, staffY, barX, staffY + 4.0f * lineSpacing, Color(0.2f, 0.2f, 0.2f), 1.0f, 2.0f);
        }
    }

    for (const auto& n : notes_) {
        float nx = staffStartX + 160.0f + n.startStep * scoreStepW - scoreScrollX_;
        if (nx < staffStartX + 50.0f || nx > staffEndX) continue;

        int refPitch = isTreble ? 71 : 50;
        float ny = staffY + 2.0f * lineSpacing - static_cast<float>(static_cast<int>(n.pitch) - refPitch) * (lineSpacing * 0.5f);

        // Ledger lines
        if (isTreble) {
            if (n.pitch <= 60) {
                float lcy = staffY + 5.0f * lineSpacing;
                ScoreVectorGlyphs::drawLedgerLine(nx, lcy, lineSpacing, 0.2f, 0.2f, 0.2f, 0.9f);
            }
            if (n.pitch >= 81) {
                float lay = staffY - lineSpacing;
                ScoreVectorGlyphs::drawLedgerLine(nx, lay, lineSpacing, 0.2f, 0.2f, 0.2f, 0.9f);
            }
        } else {
            if (n.pitch >= 60) {
                float lcy = staffY - lineSpacing;
                ScoreVectorGlyphs::drawLedgerLine(nx, lcy, lineSpacing, 0.2f, 0.2f, 0.2f, 0.9f);
            }
            if (n.pitch <= 40) {
                float ley = staffY + 5.0f * lineSpacing;
                ScoreVectorGlyphs::drawLedgerLine(nx, ley, lineSpacing, 0.2f, 0.2f, 0.2f, 0.9f);
            }
        }

        Color headCol = n.isSelected ? (trackColor_ * 1.3f) : Color(0.12f, 0.12f, 0.15f);

        // Classical SMuFL notehead & stem rendering
        if (n.durationSteps >= 16.0f) {
            // Whole note (open oval with carved center, no stem)
            ScoreVectorGlyphs::drawWholeNotehead(nx, ny, lineSpacing, headCol.r, headCol.g, headCol.b, 1.0f);
        } else if (n.durationSteps >= 8.0f) {
            // Half note (open oval with stem)
            ScoreVectorGlyphs::drawHalfNotehead(nx, ny, lineSpacing, headCol.r, headCol.g, headCol.b, 1.0f);
            bool stemUp = (ny > staffY + 2.0f * lineSpacing);
            ScoreVectorGlyphs::drawStem(nx, ny, lineSpacing, stemUp, headCol.r, headCol.g, headCol.b, 1.0f, 3.2f);
        } else {
            // Quarter / 8th / 16th note (filled notehead with stem)
            ScoreVectorGlyphs::drawBlackNotehead(nx, ny, lineSpacing, headCol.r, headCol.g, headCol.b, 1.0f);
            bool stemUp = (ny > staffY + 2.0f * lineSpacing);
            ScoreVectorGlyphs::drawStem(nx, ny, lineSpacing, stemUp, headCol.r, headCol.g, headCol.b, 1.0f, 3.2f);
            if (n.durationSteps <= 2.0f) {
                float flagEndY = stemUp ? (ny - lineSpacing * 3.2f) : (ny + lineSpacing * 3.2f);
                float flagStartX = nx + (stemUp ? (lineSpacing * 0.60f) : -(lineSpacing * 0.60f));
                r.drawLine(flagStartX, flagEndY, flagStartX + (stemUp ? 6.0f : -6.0f), flagEndY + (stemUp ? 8.0f : -8.0f),
                           headCol.r, headCol.g, headCol.b, 1.0f, 1.8f);
            }
        }

        if (n.isSelected) {
            drawCircleOutline(r, nx, ny, lineSpacing * 0.8f, theme.primaryAccent, 1.0f, 2.0f);
        }

        std::string tag = PianoKeyboard::getNoteName(n.pitch);
        if (tag.find('#') != std::string::npos) {
            ScoreVectorGlyphs::drawSharp(nx - 13.0f, ny, lineSpacing * 0.90f, headCol.r, headCol.g, headCol.b, 0.95f);
        } else if (tag.find('b') != std::string::npos) {
            ScoreVectorGlyphs::drawFlat(nx - 13.0f, ny, lineSpacing * 0.90f, headCol.r, headCol.g, headCol.b, 0.95f);
        }
        drawText(r, tag, nx - 8.0f, ny + 10.0f, 9.0f, Color(0.35f, 0.35f, 0.35f), 0.9f);
    }

    // Playhead indicator in Score View
    if (ctx.audioEngine && ctx.audioEngine->getSequencer().isPlaying()) {
        float curStep = static_cast<float>(ctx.audioEngine->getSequencer().getTransport().getCurrentStep());
        float pScoreX = staffStartX + 160.0f + curStep * scoreStepW - scoreScrollX_;
        if (pScoreX >= staffStartX + 150.0f && pScoreX <= staffEndX) {
            drawLine(r, pScoreX, staffY - 20.0f, pScoreX, staffY + 4.0f * lineSpacing + 24.0f,
                     theme.playActive, 0.95f, 2.0f);
            drawRoundedRect(r, pScoreX - 5.0f, staffY - 20.0f, 10.0f, 8.0f, 2.0f, theme.playActive, 1.0f);
        }
    }

    // Marquee Selection Rectangle in Score View
    if (dragMode_ == DragMode::MarqueeSelect) {
        float mx = std::min(dragStartPoint_.x, marqueeCurrentPoint_.x);
        float my = std::min(dragStartPoint_.y, marqueeCurrentPoint_.y);
        float mw = std::abs(marqueeCurrentPoint_.x - dragStartPoint_.x);
        float mh = std::abs(marqueeCurrentPoint_.y - dragStartPoint_.y);

        drawRect(r, mx, my, mw, mh, theme.primaryAccent, 0.18f);
        drawRoundedRectOutline(r, mx, my, mw, mh, 2.0f, theme.primaryAccent, 0.9f, 1.5f);
    }
}

void EditView::renderScript(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, contentBounds_.x, contentBounds_.y, contentBounds_.w, contentBounds_.h,
             theme.backgroundDark * 0.7f);

    drawRoundedRect(r, btnScriptApply_.x, btnScriptApply_.y, btnScriptApply_.w, btnScriptApply_.h, 4.0f,
                    theme.primaryAccent * 0.3f, 0.95f);
    drawRoundedRectOutline(r, btnScriptApply_.x, btnScriptApply_.y, btnScriptApply_.w, btnScriptApply_.h, 4.0f,
                           theme.primaryAccent, 1.0f, 1.2f);
    drawText(r, "APPLY (Ctrl+Enter / F5)", btnScriptApply_.x + 12.0f, btnScriptApply_.y + 7.0f, 10.0f,
             theme.primaryAccent);

    drawRoundedRect(r, btnScriptRevert_.x, btnScriptRevert_.y, btnScriptRevert_.w, btnScriptRevert_.h, 4.0f,
                    theme.controlBackground, 0.85f);
    drawRoundedRectOutline(r, btnScriptRevert_.x, btnScriptRevert_.y, btnScriptRevert_.w, btnScriptRevert_.h, 4.0f,
                           theme.borderSubtle, 0.8f, 1.0f);
    drawText(r, "REVERT / SYNC", btnScriptRevert_.x + 14.0f, btnScriptRevert_.y + 7.0f, 10.0f,
             theme.textPrimary);

    scriptEditor_.render(ctx);

    float statusH = 24.0f;
    Rect2D statusBounds(contentBounds_.x, contentBounds_.y + contentBounds_.h - statusH, contentBounds_.w, statusH);
    drawRect(r, statusBounds.x, statusBounds.y, statusBounds.w, statusBounds.h, theme.panelHeader);
    drawLine(r, statusBounds.x, statusBounds.y, statusBounds.x + statusBounds.w, statusBounds.y, theme.borderSubtle, 0.8f, 1.0f);
    std::string statusText = "STATUS: " + std::to_string(notes_.size()) + " events parsed | Press [APPLY] or Ctrl+Enter / F5 to compile";
    drawText(r, statusText, statusBounds.x + 14.0f, statusBounds.y + 6.0f, 9.5f, theme.highlight);
}

void EditView::renderNoteInspectorSidebar(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, sidebarBounds_.x, sidebarBounds_.y, sidebarBounds_.w, sidebarBounds_.h,
             theme.panelHeader, 0.98f);
    drawLine(r, sidebarBounds_.x, sidebarBounds_.y, sidebarBounds_.x, sidebarBounds_.y + sidebarBounds_.h,
             theme.secondaryAccent, 0.85f, 2.0f);

    drawCircle(r, sidebarBounds_.x + 14.0f, sidebarBounds_.y + 16.0f, 4.5f, trackColor_);

    size_t count = getSelectedNoteCount();
    bool isSingle = (count == 1);
    std::string title = isSingle ? "NOTE INSPECTOR" : "MULTI-NOTE INSPECTOR";
    drawText(r, title, sidebarBounds_.x + 26.0f, sidebarBounds_.y + 10.0f, 11.5f, theme.textPrimary);

    float delX = sidebarBounds_.x + sidebarBounds_.w - 56.0f;
    drawRoundedRect(r, delX, sidebarBounds_.y + 6.0f, 24.0f, 20.0f, 3.0f,
                    Color(0.85f, 0.2f, 0.2f) * 0.25f, 0.9f);
    drawText(r, "DEL", delX + 3.0f, sidebarBounds_.y + 10.0f, 9.0f, Color(1.0f, 0.35f, 0.35f));

    float closeX = sidebarBounds_.x + sidebarBounds_.w - 28.0f;
    Rect2D closeBtn(closeX, sidebarBounds_.y + 6.0f, 20.0f, 20.0f);
    float cx = closeX + 10.0f;
    float cy = sidebarBounds_.y + 16.0f;
    bool closeHov = closeBtn.contains(lastMouseX_, lastMouseY_) || std::hypot(lastMouseX_ - cx, lastMouseY_ - cy) <= 12.0f;
    drawScrewCloseButton(r, cx, cy, 8.5f, closeHov, theme.primaryAccent);

    drawLine(r, sidebarBounds_.x, sidebarBounds_.y + 32.0f, sidebarBounds_.x + sidebarBounds_.w, sidebarBounds_.y + 32.0f,
             theme.borderSubtle, 0.8f, 1.0f);

    PianoRollNote* firstSelected = nullptr;
    for (auto& n : notes_) {
        if (n.isSelected) {
            firstSelected = &n;
            break;
        }
    }

    if (isSingle && firstSelected) {
        renderSingleNoteSidebar(ctx, *firstSelected);
    } else {
        renderMultiNoteSidebar(ctx);
    }
}

void EditView::renderSingleNoteSidebar(const ViewContext& ctx, PianoRollNote& note) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    float cardY = sidebarBounds_.y + 40.0f;
    drawRoundedRect(r, sidebarBounds_.x + 10.0f, cardY, sidebarBounds_.w - 20.0f, 32.0f, 5.0f,
                    trackColor_ * 0.2f, 0.9f);
    drawRoundedRectOutline(r, sidebarBounds_.x + 10.0f, cardY, sidebarBounds_.w - 20.0f, 32.0f, 5.0f,
                           trackColor_, 0.8f, 1.2f);

    std::string pitchStr = PianoKeyboard::getNoteName(note.pitch);
    drawText(r, pitchStr, sidebarBounds_.x + 18.0f, cardY + 8.0f, 13.5f, trackColor_);

    std::string stepInfo = "Step " + std::to_string(static_cast<int>(note.startStep)) + " (" +
                           std::to_string(note.durationSteps).substr(0, 4) + " st)";
    drawText(r, stepInfo, sidebarBounds_.x + 110.0f, cardY + 10.0f, 10.5f, theme.textPrimary);

    float pY = cardY + 44.0f;
    drawText(r, "PITCH TRANSPOSE", sidebarBounds_.x + 12.0f, pY, 9.5f, theme.textMuted, 0.85f);

    float btnY = pY + 14.0f;
    auto drawSmallBtn = [&](float x, const std::string& lbl) {
        drawRoundedRect(r, x, btnY, 34.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
        drawRoundedRectOutline(r, x, btnY, 34.0f, 22.0f, 3.5f, theme.borderSubtle, 0.7f, 1.0f);
        drawText(r, lbl, x + 6.0f, btnY + 5.0f, 10.0f, theme.textPrimary);
    };

    drawSmallBtn(sidebarBounds_.x + 12.0f, "-12");
    drawSmallBtn(sidebarBounds_.x + 50.0f, "-1");

    drawRoundedRect(r, sidebarBounds_.x + 88.0f, btnY, 68.0f, 22.0f, 3.5f, theme.primaryAccent * 0.25f, 0.9f);
    drawText(r, pitchStr, sidebarBounds_.x + 108.0f, btnY + 5.0f, 11.0f, theme.primaryAccent);

    drawSmallBtn(sidebarBounds_.x + 160.0f, "+1");
    drawSmallBtn(sidebarBounds_.x + 198.0f, "+12");

    float posSectionY = btnY + 34.0f;
    drawText(r, "POSITION (START STEP)", sidebarBounds_.x + 12.0f, posSectionY, 9.5f, theme.textMuted, 0.85f);
    std::string curStepStr = "Step " + std::to_string(note.startStep).substr(0, 4);
    drawText(r, curStepStr, sidebarBounds_.x + 12.0f, posSectionY + 16.0f, 11.0f, theme.highlight);

    drawSmallBtn(sidebarBounds_.x + 130.0f, "-STEP");
    drawSmallBtn(sidebarBounds_.x + 172.0f, "+STEP");

    float lenSectionY = posSectionY + 48.0f;
    drawText(r, "LENGTH / DURATION", sidebarBounds_.x + 12.0f, lenSectionY, 9.5f, theme.textMuted, 0.85f);
    std::string curDurStr = std::to_string(note.durationSteps).substr(0, 4) + " steps";
    drawText(r, curDurStr, sidebarBounds_.x + 12.0f, lenSectionY + 16.0f, 11.0f, theme.primaryAccent);

    drawSmallBtn(sidebarBounds_.x + 130.0f, "-LEN");
    drawSmallBtn(sidebarBounds_.x + 172.0f, "+LEN");

    float velSectionY = lenSectionY + 48.0f;
    int velPct = static_cast<int>(note.velocity * 100.0f);
    std::string velHeader = "VELOCITY (" + std::to_string(velPct) + "%)";
    drawText(r, velHeader, sidebarBounds_.x + 12.0f, velSectionY, 9.5f, theme.textMuted, 0.85f);

    float audBtnX = sidebarBounds_.x + sidebarBounds_.w - 42.0f;
    drawRoundedRect(r, audBtnX, velSectionY - 4.0f, 26.0f, 22.0f, 3.0f, theme.primaryAccent * 0.25f, 0.9f);
    drawText(r, ">", audBtnX + 8.0f, velSectionY + 1.0f, 11.0f, theme.primaryAccent);

    float velBarY = velSectionY + 16.0f;
    drawRoundedRect(r, sidebarBounds_.x + 12.0f, velBarY, sidebarBounds_.w - 24.0f, 8.0f, 4.0f, theme.controlBackground, 0.8f);
    float velFillW = (sidebarBounds_.w - 24.0f) * note.velocity;
    drawRoundedRect(r, sidebarBounds_.x + 12.0f, velBarY, velFillW, 8.0f, 4.0f, theme.primaryAccent, 0.95f);

    float typeSectionY = velBarY + 22.0f;
    drawText(r, "NOTE TYPE / GLISSANDO", sidebarBounds_.x + 12.0f, typeSectionY, 9.5f, theme.textMuted, 0.85f);

    float typeBtnY = typeSectionY + 14.0f;
    float typeBtnW = (sidebarBounds_.w - 28.0f) * 0.5f;

    bool isReg = !note.isSlide;
    drawRoundedRect(r, sidebarBounds_.x + 12.0f, typeBtnY, typeBtnW, 26.0f, 4.0f,
                    isReg ? (theme.primaryAccent * 0.3f) : theme.controlBackground, 0.9f);
    drawRoundedRectOutline(r, sidebarBounds_.x + 12.0f, typeBtnY, typeBtnW, 26.0f, 4.0f,
                           isReg ? theme.primaryAccent : theme.borderSubtle, 0.9f, 1.0f);
    drawText(r, "REGULAR", sidebarBounds_.x + 36.0f, typeBtnY + 7.0f, 10.0f,
             isReg ? theme.primaryAccent : theme.textMuted);

    bool isSlide = note.isSlide;
    drawRoundedRect(r, sidebarBounds_.x + 16.0f + typeBtnW, typeBtnY, typeBtnW, 26.0f, 4.0f,
                    isSlide ? (theme.highlight * 0.3f) : theme.controlBackground, 0.9f);
    drawRoundedRectOutline(r, sidebarBounds_.x + 16.0f + typeBtnW, typeBtnY, typeBtnW, 26.0f, 4.0f,
                           isSlide ? theme.highlight : theme.borderSubtle, 0.9f, 1.0f);
    drawText(r, "GLISS / BEND", sidebarBounds_.x + 28.0f + typeBtnW, typeBtnY + 7.0f, 10.0f,
             isSlide ? theme.highlight : theme.textMuted);

    float artSectionY = typeBtnY + 38.0f;
    drawText(r, "ARTICULATION", sidebarBounds_.x + 12.0f, artSectionY, 9.5f, theme.textMuted, 0.85f);

    const char* arts[] = {"NORMAL", "PIZZ", "STACC", "LEGATO", "SLAP", "FLAM"};
    float chipX = sidebarBounds_.x + 12.0f;
    float chipY = artSectionY + 14.0f;
    for (int i = 0; i < 6; ++i) {
        bool isAct = (note.articulation == arts[i] || (i == 0 && note.articulation == "normal"));
        drawRoundedRect(r, chipX, chipY, 36.0f, 20.0f, 3.0f,
                        isAct ? (theme.primaryAccent * 0.3f) : theme.controlBackground, 0.9f);
        drawText(r, arts[i], chipX + 4.0f, chipY + 5.0f, 8.5f,
                 isAct ? theme.primaryAccent : theme.textMuted);
        chipX += 40.0f;
        if (i == 2) {
            chipX = sidebarBounds_.x + 12.0f;
            chipY += 24.0f;
        }
    }
}

void EditView::renderMultiNoteSidebar(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    size_t count = getSelectedNoteCount();

    float cardY = sidebarBounds_.y + 40.0f;
    drawRoundedRect(r, sidebarBounds_.x + 10.0f, cardY, sidebarBounds_.w - 20.0f, 32.0f, 5.0f,
                    theme.primaryAccent * 0.2f, 0.9f);
    drawRoundedRectOutline(r, sidebarBounds_.x + 10.0f, cardY, sidebarBounds_.w - 20.0f, 32.0f, 5.0f,
                           theme.primaryAccent, 0.8f, 1.2f);

    std::string countStr = std::to_string(count) + " Notes Selected";
    drawText(r, countStr, sidebarBounds_.x + 18.0f, cardY + 9.0f, 12.0f, theme.primaryAccent);

    drawRoundedRect(r, sidebarBounds_.x + sidebarBounds_.w - 95.0f, cardY + 6.0f, 80.0f, 20.0f, 3.0f,
                    theme.controlBackground, 0.9f);
    drawText(r, "BATCH MODE", sidebarBounds_.x + sidebarBounds_.w - 86.0f, cardY + 10.0f, 9.0f, theme.textPrimary);

    float pY = cardY + 44.0f;
    drawText(r, "BATCH PITCH TRANSPOSE", sidebarBounds_.x + 12.0f, pY, 9.5f, theme.textMuted, 0.85f);

    float btnY = pY + 14.0f;
    auto drawSmallBtn = [&](float x, const std::string& lbl) {
        drawRoundedRect(r, x, btnY, 34.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
        drawRoundedRectOutline(r, x, btnY, 34.0f, 22.0f, 3.5f, theme.borderSubtle, 0.7f, 1.0f);
        drawText(r, lbl, x + 6.0f, btnY + 5.0f, 10.0f, theme.textPrimary);
    };

    drawSmallBtn(sidebarBounds_.x + 12.0f, "-12");
    drawSmallBtn(sidebarBounds_.x + 50.0f, "-1");

    drawRoundedRect(r, sidebarBounds_.x + 88.0f, btnY, 68.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
    drawText(r, "+- PITCH", sidebarBounds_.x + 100.0f, btnY + 5.0f, 10.0f, theme.primaryAccent);

    drawSmallBtn(sidebarBounds_.x + 160.0f, "+1");
    drawSmallBtn(sidebarBounds_.x + 198.0f, "+12");

    float posSectionY = btnY + 34.0f;
    drawText(r, "BATCH POSITION & DURATION", sidebarBounds_.x + 12.0f, posSectionY, 9.5f, theme.textMuted, 0.85f);

    float nudgeY = posSectionY + 14.0f;
    drawRoundedRect(r, sidebarBounds_.x + 12.0f, nudgeY, 52.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
    drawText(r, "-STEP", sidebarBounds_.x + 18.0f, nudgeY + 5.0f, 9.5f, theme.textPrimary);

    drawRoundedRect(r, sidebarBounds_.x + 68.0f, nudgeY, 52.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
    drawText(r, "+STEP", sidebarBounds_.x + 74.0f, nudgeY + 5.0f, 9.5f, theme.textPrimary);

    drawRoundedRect(r, sidebarBounds_.x + 124.0f, nudgeY, 48.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
    drawText(r, "-LEN", sidebarBounds_.x + 130.0f, nudgeY + 5.0f, 9.5f, theme.textPrimary);

    drawRoundedRect(r, sidebarBounds_.x + 176.0f, nudgeY, 48.0f, 22.0f, 3.5f, theme.controlBackground, 0.9f);
    drawText(r, "+LEN", sidebarBounds_.x + 182.0f, nudgeY + 5.0f, 9.5f, theme.textPrimary);

    float velSectionY = nudgeY + 34.0f;
    drawText(r, "BATCH VELOCITY PRESETS", sidebarBounds_.x + 12.0f, velSectionY, 9.5f, theme.textMuted, 0.85f);

    float velBtnY = velSectionY + 14.0f;
    const char* vPresets[] = {"25%", "50%", "75%", "100%", "HUMAN"};
    float vx = sidebarBounds_.x + 12.0f;
    for (int i = 0; i < 5; ++i) {
        float bw = (i == 4) ? 52.0f : 40.0f;
        drawRoundedRect(r, vx, velBtnY, bw, 22.0f, 3.5f, theme.controlBackground, 0.9f);
        drawText(r, vPresets[i], vx + 6.0f, velBtnY + 5.0f, 9.5f, theme.textPrimary);
        vx += bw + 4.0f;
    }

    float utilSectionY = velBtnY + 34.0f;
    drawText(r, "SELECTION UTILITIES", sidebarBounds_.x + 12.0f, utilSectionY, 9.5f, theme.textMuted, 0.85f);

    float uY = utilSectionY + 14.0f;
    auto drawUtilBtn = [&](float x, float y, float w, const std::string& lbl) {
        drawRoundedRect(r, x, y, w, 24.0f, 3.5f, theme.controlBackground, 0.9f);
        drawRoundedRectOutline(r, x, y, w, 24.0f, 3.5f, theme.borderSubtle, 0.8f, 1.0f);
        drawText(r, lbl, x + 8.0f, y + 6.0f, 9.5f, theme.primaryAccent);
    };

    drawUtilBtn(sidebarBounds_.x + 12.0f, uY, 110.0f, "QUANTIZE (1/16)");
    drawUtilBtn(sidebarBounds_.x + 128.0f, uY, 108.0f, "SET ALL BEND");

    drawUtilBtn(sidebarBounds_.x + 12.0f, uY + 28.0f, 110.0f, "SELECT ALL");
    drawUtilBtn(sidebarBounds_.x + 128.0f, uY + 28.0f, 108.0f, "INVERT");

    drawUtilBtn(sidebarBounds_.x + 12.0f, uY + 56.0f, 110.0f, "RETROGRADE");
    drawUtilBtn(sidebarBounds_.x + 128.0f, uY + 56.0f, 108.0f, "INVERT PITCH");
}

bool EditView::handlePointer(const PointerEvent& ev, const ViewContext& ctx) {
    lastMouseX_ = ev.x;
    lastMouseY_ = ev.y;

    if (trackerCellDialog_.isOpen()) {
        trackerCellDialog_.onAudition = [this, &ctx](uint8_t p, float v) {
            auditionPitch(p, v, ctx);
        };
        if (trackerCellDialog_.handlePointer(ev)) {
            return true;
        }
        return true;
    }

    if (ev.action == PointerAction::Down) {
        // Clear text editor focus if clicking outside the script editor or in a non-script subview
        if (subView_ != EditSubViewMode::Script || !scriptEditorBounds_.contains(ev.x, ev.y)) {
            if (ctx.focusManager && ctx.focusManager->hasFocus(&scriptEditor_)) {
                ctx.focusManager->clearFocus();
            }
            scriptEditor_.onFocusLost();
            if (onRequestClearFocus) {
                onRequestClearFocus();
            }
        }

        // Bake Chord Follow & MIDI FX button
        if (hasActiveChordFollowOrMidiFx() && btnBake_.contains(ev.x, ev.y)) {
            bakeChordFollowAndMidiFx();
            return true;
        }

        // 1. Sub-View Switcher buttons
        if (btnPianoRoll_.contains(ev.x, ev.y)) { setSubView(EditSubViewMode::PianoRoll); return true; }
        if (btnTracker_.contains(ev.x, ev.y)) { setSubView(EditSubViewMode::Tracker); return true; }
        if (btnScore_.contains(ev.x, ev.y)) { setSubView(EditSubViewMode::Score); return true; }
        if (btnScript_.contains(ev.x, ev.y)) { setSubView(EditSubViewMode::Script); return true; }

        // Ghost Notes controls
        if (ghostToggleBtn_.contains(ev.x, ev.y)) {
            toggleGhostNotes();
            return true;
        }
        if (ghostSliderBounds_.contains(ev.x, ev.y)) {
            float rel = (ev.x - ghostSliderBounds_.x) / ghostSliderBounds_.w;
            setGhostNotesOpacity(rel);
            dragMode_ = DragMode::GhostSlider;
            return true;
        }

        // 2. Note Inspector Sidebar interactions (if open)
        if (hasSelectedNotes() && sidebarBounds_.contains(ev.x, ev.y)) {
            float closeX = sidebarBounds_.x + sidebarBounds_.w - 28.0f;
            Rect2D closeBtn(closeX, sidebarBounds_.y + 6.0f, 20.0f, 20.0f);
            if (closeBtn.contains(ev.x, ev.y)) {
                clearSelection();
                return true;
            }

            float delX = sidebarBounds_.x + sidebarBounds_.w - 56.0f;
            Rect2D delBtn(delX, sidebarBounds_.y + 6.0f, 24.0f, 20.0f);
            if (delBtn.contains(ev.x, ev.y)) {
                deleteSelectedNotes();
                return true;
            }

            float btnY = sidebarBounds_.y + 98.0f;
            if (Rect2D(sidebarBounds_.x + 12.0f, btnY, 34.0f, 22.0f).contains(ev.x, ev.y)) { transposeSelectedNotes(-12); return true; }
            if (Rect2D(sidebarBounds_.x + 50.0f, btnY, 34.0f, 22.0f).contains(ev.x, ev.y)) { transposeSelectedNotes(-1); return true; }
            if (Rect2D(sidebarBounds_.x + 160.0f, btnY, 34.0f, 22.0f).contains(ev.x, ev.y)) { transposeSelectedNotes(1); return true; }
            if (Rect2D(sidebarBounds_.x + 198.0f, btnY, 34.0f, 22.0f).contains(ev.x, ev.y)) { transposeSelectedNotes(12); return true; }

            float nudgeY = btnY + 48.0f;
            if (Rect2D(sidebarBounds_.x + 130.0f, nudgeY, 38.0f, 22.0f).contains(ev.x, ev.y)) { nudgeSelectedNotes(-1.0f); return true; }
            if (Rect2D(sidebarBounds_.x + 172.0f, nudgeY, 38.0f, 22.0f).contains(ev.x, ev.y)) { nudgeSelectedNotes(1.0f); return true; }

            float durY = nudgeY + 48.0f;
            if (Rect2D(sidebarBounds_.x + 130.0f, durY, 38.0f, 22.0f).contains(ev.x, ev.y)) { changeSelectedNotesDuration(-0.25f); return true; }
            if (Rect2D(sidebarBounds_.x + 172.0f, durY, 38.0f, 22.0f).contains(ev.x, ev.y)) { changeSelectedNotesDuration(0.25f); return true; }

            float audBtnX = sidebarBounds_.x + sidebarBounds_.w - 42.0f;
            if (Rect2D(audBtnX, durY + 44.0f, 26.0f, 22.0f).contains(ev.x, ev.y)) {
                for (size_t ni = 0; ni < notes_.size(); ++ni) {
                    const auto& n = notes_[ni];
                    if (n.isSelected) {
                        int audPitch = (hasActiveChordFollowOrMidiFx() && ni < evaluatedFxNotes_.size()) ? evaluatedFxNotes_[ni].pitch : n.pitch;
                        auditionPitch(audPitch, n.velocity, ctx, hasActiveChordFollowOrMidiFx());
                        break;
                    }
                }
                return true;
            }

            float typeBtnY = durY + 88.0f;
            float typeBtnW = (sidebarBounds_.w - 28.0f) * 0.5f;
            if (Rect2D(sidebarBounds_.x + 12.0f, typeBtnY, typeBtnW, 26.0f).contains(ev.x, ev.y)) { setSelectedNotesSlide(false); return true; }
            if (Rect2D(sidebarBounds_.x + 16.0f + typeBtnW, typeBtnY, typeBtnW, 26.0f).contains(ev.x, ev.y)) { setSelectedNotesSlide(true); return true; }

            float artSectionY = typeBtnY + 38.0f;
            float chipX = sidebarBounds_.x + 12.0f;
            float chipY = artSectionY + 14.0f;
            const char* arts[] = {"NORMAL", "PIZZ", "STACC", "LEGATO", "SLAP", "FLAM"};
            for (int i = 0; i < 6; ++i) {
                if (Rect2D(chipX, chipY, 36.0f, 20.0f).contains(ev.x, ev.y)) {
                    setSelectedNotesArticulation(arts[i]);
                    return true;
                }
                chipX += 40.0f;
                if (i == 2) {
                    chipX = sidebarBounds_.x + 12.0f;
                    chipY += 24.0f;
                }
            }

            float velPresetY = btnY + 82.0f;
            if (Rect2D(sidebarBounds_.x + 12.0f, velPresetY, 40.0f, 22.0f).contains(ev.x, ev.y)) { setSelectedNotesVelocity(0.25f); return true; }
            if (Rect2D(sidebarBounds_.x + 56.0f, velPresetY, 40.0f, 22.0f).contains(ev.x, ev.y)) { setSelectedNotesVelocity(0.50f); return true; }
            if (Rect2D(sidebarBounds_.x + 100.0f, velPresetY, 40.0f, 22.0f).contains(ev.x, ev.y)) { setSelectedNotesVelocity(0.75f); return true; }
            if (Rect2D(sidebarBounds_.x + 144.0f, velPresetY, 40.0f, 22.0f).contains(ev.x, ev.y)) { setSelectedNotesVelocity(1.00f); return true; }
            if (Rect2D(sidebarBounds_.x + 188.0f, velPresetY, 52.0f, 22.0f).contains(ev.x, ev.y)) { humanizeSelectedNotes(0.15f); return true; }

            float uY = velPresetY + 48.0f;
            if (Rect2D(sidebarBounds_.x + 12.0f, uY, 110.0f, 24.0f).contains(ev.x, ev.y)) { quantizeSelectedNotes(1.0f); return true; }
            if (Rect2D(sidebarBounds_.x + 128.0f, uY, 108.0f, 24.0f).contains(ev.x, ev.y)) { setSelectedNotesSlide(true); return true; }
            if (Rect2D(sidebarBounds_.x + 12.0f, uY + 28.0f, 110.0f, 24.0f).contains(ev.x, ev.y)) { selectAllNotes(); return true; }
            if (Rect2D(sidebarBounds_.x + 128.0f, uY + 28.0f, 108.0f, 24.0f).contains(ev.x, ev.y)) { invertSelection(); return true; }
            if (Rect2D(sidebarBounds_.x + 12.0f, uY + 56.0f, 110.0f, 24.0f).contains(ev.x, ev.y)) { retrogradeSelectedNotes(); return true; }
            if (Rect2D(sidebarBounds_.x + 128.0f, uY + 56.0f, 108.0f, 24.0f).contains(ev.x, ev.y)) { invertSelectedNotesPitch(); return true; }

            return true;
        }

        // 3. Middle-Mouse Panning
        if (ev.button == PointerButton::Middle) {
            dragMode_ = DragMode::MiddlePan;
            dragStartPoint_ = Point2D{ev.x, ev.y};
            panStartScrollX_ = (subView_ == EditSubViewMode::Tracker) ? trackerScrollX_ : scrollX_;
            panStartScrollY_ = (subView_ == EditSubViewMode::Tracker) ? trackerScrollY_ : scrollY_;
            return true;
        }

        // 4. Sub-view specific interactions
        if (subView_ == EditSubViewMode::PianoRoll) {
            if (pianoGutterBounds_.contains(ev.x, ev.y)) {
                if (ev.button == PointerButton::Right) {
                    auto hit = pianoKeyboard_.hitTest(ev.x, ev.y, pianoGutterBounds_.x, pianoGutterBounds_.y,
                                                      pianoGutterBounds_.w, pianoGutterBounds_.h, scrollY_);
                    if (hit.hit) {
                        for (auto& n : notes_) {
                            if (n.pitch == hit.pitch) n.isSelected = true;
                        }
                    }
                    return true;
                }
                if (pianoKeyboard_.handlePointer(ev, pianoGutterBounds_, scrollY_, ctx.audioEngine, activeTrackIndex_)) {
                    auditioningPitch_ = -1;
                    return true;
                }
            }

            if (gridBounds_.contains(ev.x, ev.y)) {
                for (size_t i = 0; i < notes_.size(); ++i) {
                    auto& n = notes_[i];
                    float nx = gridBounds_.x + n.startStep * stepWidth_ - scrollX_;
                    float nw = n.durationSteps * stepWidth_ - 2.0f;
                    float ny = gridBounds_.y + static_cast<float>(maxPitch_ - n.pitch) * semitoneHeight_ - scrollY_ + 1.5f;
                    float nh = semitoneHeight_ - 3.0f;

                    Rect2D noteRect(nx, ny, nw, nh);
                    if (noteRect.contains(ev.x, ev.y)) {
                        lastGridClickHadNote_ = true;
                        if (ev.button == PointerButton::Right) {
                            deleteNoteAt(n.startStep, n.pitch);
                            return true;
                        }

                        if (ev.x >= nx + nw - 10.0f) {
                            dragMode_ = DragMode::ResizeNotes;
                            activeDragNoteIdx_ = static_cast<int>(i);
                            activeDragInitialDur_ = n.durationSteps;
                            dragStartPoint_ = Point2D{ev.x, ev.y};
                            return true;
                        }

                        if (!n.isSelected) {
                            clearSelection();
                            n.isSelected = true;
                        }
                        dragMode_ = DragMode::MoveNotes;
                        activeDragNoteIdx_ = static_cast<int>(i);
                        activeDragInitialStep_ = n.startStep;
                        activeDragInitialPitch_ = n.pitch;
                        dragStartPoint_ = Point2D{ev.x, ev.y};

                        batchDragOrigins_.clear();
                        for (const auto& sel : notes_) {
                            if (sel.isSelected) batchDragOrigins_.push_back({sel.startStep, sel.pitch});
                        }

                        auditionPitch((hasActiveChordFollowOrMidiFx() && i < evaluatedFxNotes_.size()) ? evaluatedFxNotes_[i].pitch : n.pitch, n.velocity, ctx, hasActiveChordFollowOrMidiFx());
                        return true;
                    }
                }

                // Check double-click / double-tap on Down to create note and immediately arm resizing if held
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGridClickTime_).count();
                float clickDist = std::hypot(ev.x - lastGridClickPos_.x, ev.y - lastGridClickPos_.y);
                bool isDouble = (elapsedMs < 400 && clickDist < 25.0f && !lastGridClickHadNote_);

                if (isDouble) {
                    float step = std::floor((ev.x - gridBounds_.x + scrollX_) / stepWidth_);
                    int pitch = maxPitch_ - static_cast<int>((ev.y - gridBounds_.y + scrollY_) / semitoneHeight_);
                    if (step >= 0.0f && step < 128.0f && pitch >= minPitch_ && pitch <= maxPitch_) {
                        clearSelection();
                        addNote(static_cast<uint8_t>(pitch), step, 1.0f, 0.85f);
                        if (!notes_.empty()) {
                            notes_.back().isSelected = true;
                            dragMode_ = DragMode::ResizeNotes;
                            activeDragNoteIdx_ = static_cast<int>(notes_.size() - 1);
                            activeDragInitialDur_ = 1.0f;
                            dragStartPoint_ = Point2D{ev.x, ev.y};
                        }
                        auditionPitch(pitch, 0.85f, ctx);
                        if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
                    }
                    lastGridClickTime_ = std::chrono::steady_clock::time_point{};
                    lastGridClickHadNote_ = false;
                    return true;
                }

                // Not double-click: record single click/tap
                lastGridClickTime_ = now;
                lastGridClickPos_ = Point2D{ev.x, ev.y};
                lastGridClickHadNote_ = false;

                if (ev.type == PointerType::Touch || ctx.isMobile) {
                    // Mobile: single tap/touch directly pans the piano roll just like middle mouse
                    clearSelection();
                    dragMode_ = DragMode::TouchPan;
                    dragStartPoint_ = Point2D{ev.x, ev.y};
                    panStartScrollX_ = scrollX_;
                    panStartScrollY_ = scrollY_;
                    kineticScroller_.reset();
                    kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                    return true;
                } else if (ev.button == PointerButton::Left) {
                    clearSelection();
                    dragMode_ = DragMode::MarqueeSelect;
                    dragStartPoint_ = Point2D{ev.x, ev.y};
                    marqueeCurrentPoint_ = Point2D{ev.x, ev.y};
                    return true;
                }
            }

            if (velocityLaneBounds_.contains(ev.x, ev.y)) {
                for (auto& n : notes_) {
                    float sx = velocityLaneBounds_.x + n.startStep * stepWidth_ - scrollX_ + 4.0f;
                    if (std::abs(ev.x - sx) < 8.0f) {
                        float v = 1.0f - ((ev.y - velocityLaneBounds_.y) / velocityLaneBounds_.h);
                        n.velocity = std::clamp(v, 0.05f, 1.0f);
                        int audP = (hasActiveChordFollowOrMidiFx() && &n >= notes_.data() && static_cast<size_t>(&n - notes_.data()) < evaluatedFxNotes_.size())
                            ? evaluatedFxNotes_[&n - notes_.data()].pitch : n.pitch;
                        auditionPitch(audP, n.velocity, ctx, hasActiveChordFollowOrMidiFx());
                        return true;
                    }
                }
            }
        } else if (subView_ == EditSubViewMode::Tracker) {
            if (btnOctDown_.contains(ev.x, ev.y)) {
                trackerBaseOctave_ = std::clamp(trackerBaseOctave_ - 1, 0, 8);
                return true;
            }
            if (btnOctUp_.contains(ev.x, ev.y)) {
                trackerBaseOctave_ = std::clamp(trackerBaseOctave_ + 1, 0, 8);
                return true;
            }
            if (btnStepDown_.contains(ev.x, ev.y)) {
                if (trackerEditStep_ > 4) trackerEditStep_ = 4;
                else if (trackerEditStep_ > 2) trackerEditStep_ = 2;
                else if (trackerEditStep_ > 1) trackerEditStep_ = 1;
                else if (trackerEditStep_ > 0) trackerEditStep_ = 0;
                return true;
            }
            if (btnStepUp_.contains(ev.x, ev.y)) {
                if (trackerEditStep_ == 0) trackerEditStep_ = 1;
                else if (trackerEditStep_ == 1) trackerEditStep_ = 2;
                else if (trackerEditStep_ == 2) trackerEditStep_ = 4;
                else if (trackerEditStep_ == 4) trackerEditStep_ = 8;
                return true;
            }
            if (btnLaneDown_.contains(ev.x, ev.y)) {
                removeTrackerLane();
                return true;
            }
            if (btnLaneUp_.contains(ev.x, ev.y)) {
                addTrackerLane();
                return true;
            }
            if (btnToggleParams_.contains(ev.x, ev.y)) {
                trackerShowCustomParams_ = !trackerShowCustomParams_;
                if (trackerShowCustomParams_ && trackerCustomCols_.empty()) {
                    registerTrackerParamCol("echo");
                    registerTrackerParamCol("alg");
                    registerTrackerParamCol("fb");
                }
                ensureTrackerCursorVisible();
                return true;
            }
            if (btnFollow_.contains(ev.x, ev.y)) {
                trackerFollowPlayback_ = !trackerFollowPlayback_;
                return true;
            }

            float rowNumW = 48.0f;
            float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
            float colW = 140.0f + extraParamsW;
            float colSpacing = 6.0f;
            float startRowY = contentBounds_.y + 62.0f;
            float rowH = 24.0f;

            if (ev.y >= startRowY) {
                int clickedStep = static_cast<int>((ev.y - startRowY + trackerScrollY_) / rowH);
                float relX = ev.x - (contentBounds_.x + rowNumW) + trackerScrollX_;
                int clickedCol = static_cast<int>(relX / (colW + colSpacing));

                if (clickedStep >= 0 && clickedStep < 64) {
                    if (ev.x < contentBounds_.x + rowNumW && ev.x >= contentBounds_.x) {
                        // Click in STEP number lane -> select step
                        trackerSelectedStep_ = clickedStep;
                        trackerHasBlockSelection_ = false;
                        clearSelection();
                        for (auto& n : notes_) {
                            if (static_cast<int>(std::round(n.startStep)) == clickedStep && n.column == trackerSelectedCol_) {
                                n.isSelected = true;
                                int audP = (hasActiveChordFollowOrMidiFx() && &n >= notes_.data() && static_cast<size_t>(&n - notes_.data()) < evaluatedFxNotes_.size())
                                    ? evaluatedFxNotes_[&n - notes_.data()].pitch : n.pitch;
                                auditionPitch(audP, n.velocity, ctx, hasActiveChordFollowOrMidiFx());
                                break;
                            }
                        }
                        ensureTrackerCursorVisible();
                        return true;
                    }
                    if (clickedCol >= 0 && clickedCol < trackerNumLanes_) {
                        if (ev.type == PointerType::Touch || ctx.isMobile) {
                            dragMode_ = DragMode::TouchGridPending;
                            touchDownPos_ = Point2D{ev.x, ev.y};
                            touchDownTimePoint_ = std::chrono::steady_clock::now();
                            dragStartPoint_ = Point2D{ev.x, ev.y};
                            panStartScrollX_ = trackerScrollX_;
                            panStartScrollY_ = trackerScrollY_;
                            touchPanCommitted_ = false;
                            touchLongPressArmed_ = false;
                            trackerBlockAnchorStep_ = clickedStep;
                            trackerBlockAnchorCol_ = clickedCol;
                            kineticScroller_.reset();
                            kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                            return true;
                        } else if (ev.button == PointerButton::Left) {
                            dragMode_ = DragMode::TrackerBlockSelect;
                            dragStartPoint_ = Point2D{ev.x, ev.y};
                            trackerBlockAnchorStep_ = clickedStep;
                            trackerBlockAnchorCol_ = clickedCol;
                            trackerSelectedStep_ = clickedStep;
                            trackerSelectedCol_ = clickedCol;
                            trackerHasBlockSelection_ = false;
                            return true;
                        }
                    }
                }
            }
        } else if (subView_ == EditSubViewMode::Score) {
            if (btnClefAuto_.contains(ev.x, ev.y)) { scoreClef_ = ScoreClef::Auto; return true; }
            if (btnClefTreble_.contains(ev.x, ev.y)) { scoreClef_ = ScoreClef::Treble; return true; }
            if (btnClefBass_.contains(ev.x, ev.y)) { scoreClef_ = ScoreClef::Bass; return true; }

            if (btnDurWhole_.contains(ev.x, ev.y)) { scoreDuration_ = ScoreNoteType::Whole; return true; }
            if (btnDurHalf_.contains(ev.x, ev.y)) { scoreDuration_ = ScoreNoteType::Half; return true; }
            if (btnDurQuarter_.contains(ev.x, ev.y)) { scoreDuration_ = ScoreNoteType::Quarter; return true; }
            if (btnDurEighth_.contains(ev.x, ev.y)) { scoreDuration_ = ScoreNoteType::Eighth; return true; }
            if (btnDur16th_.contains(ev.x, ev.y)) { scoreDuration_ = ScoreNoteType::Sixteenth; return true; }

            float staffStartX = contentBounds_.x + 50.0f;
            float staffEndX = contentBounds_.x + contentBounds_.w - 40.0f;
            float staffY = contentBounds_.y + 110.0f;
            float scoreStepW = 34.0f;
            float lineSpacing = 14.0f;
            bool isTreble = (scoreClef_ == ScoreClef::Treble) || (scoreClef_ == ScoreClef::Auto && activeTrackIndex_ != 0 && activeTrackIndex_ != 2);
            int refPitch = isTreble ? 71 : 50;

            if (ev.x >= staffStartX + 150.0f && ev.x <= staffEndX && ev.y >= staffY - 40.0f && ev.y <= staffY + 5.0f * lineSpacing + 40.0f) {
                // Check if existing note head was clicked
                for (size_t i = 0; i < notes_.size(); ++i) {
                    auto& n = notes_[i];
                    float nx = staffStartX + 160.0f + n.startStep * scoreStepW - scoreScrollX_;
                    float ny = staffY + 2.0f * lineSpacing - static_cast<float>(static_cast<int>(n.pitch) - refPitch) * (lineSpacing * 0.5f);
                    float d = std::hypot(ev.x - nx, ev.y - ny);
                    if (d <= 12.0f) {
                        if (ev.button == PointerButton::Right) {
                            deleteNoteAt(n.startStep, n.pitch);
                            if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
                            return true;
                        }
                        if (!n.isSelected) {
                            clearSelection();
                            n.isSelected = true;
                        }
                        auditionPitch(n.pitch, n.velocity, ctx);
                        return true;
                    }
                }

                // If empty staff area clicked, start marquee drag selection or touch pan
                if (ev.type == PointerType::Touch) {
                    dragMode_ = DragMode::TouchGridPending;
                    touchDownPos_ = Point2D{ev.x, ev.y};
                    touchDownTimeMs_ = ev.timestampMs;
                    touchDownTimePoint_ = std::chrono::steady_clock::now();
                    touchLongPressArmed_ = false;
                    touchPanCommitted_ = false;
                    dragStartPoint_ = Point2D{ev.x, ev.y};
                    panStartScrollX_ = scoreScrollX_;
                    panStartScrollY_ = 0.0f;
                    kineticScroller_.reset();
                    kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                    return true;
                } else if (ev.button == PointerButton::Left) {
                    clearSelection();
                    dragMode_ = DragMode::MarqueeSelect;
                    dragStartPoint_ = Point2D{ev.x, ev.y};
                    marqueeCurrentPoint_ = Point2D{ev.x, ev.y};
                    return true;
                }
            }
        } else if (subView_ == EditSubViewMode::Script) {
            if (btnScriptApply_.contains(ev.x, ev.y)) {
                parseNotesFromEatscript();
                if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
                if (ctx.onShowNotification) ctx.onShowNotification("Eatscript compiled to clip notes");
                return true;
            }
            if (btnScriptRevert_.contains(ev.x, ev.y)) {
                formatEatscriptFromNotes();
                scriptEditor_.setText(scriptBuffer_);
                return true;
            }
            if (scriptEditor_.handlePointer(ev, ctx)) {
                return true;
            }
        }
    } else if (ev.action == PointerAction::Move) {
        if (subView_ == EditSubViewMode::PianoRoll && pianoKeyboard_.hasActiveTouches()) {
            if (pianoKeyboard_.handlePointer(ev, pianoGutterBounds_, scrollY_, ctx.audioEngine, activeTrackIndex_)) {
                return true;
            }
        }
        if (subView_ == EditSubViewMode::Script) {
            if (scriptEditor_.handlePointer(ev, ctx)) return true;
        }

        if (dragMode_ == DragMode::TouchGridPending) {
            float dist = std::hypot(ev.x - touchDownPos_.x, ev.y - touchDownPos_.y);
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - touchDownTimePoint_).count();

            if (subView_ == EditSubViewMode::Tracker) {
                float rowNumW = 48.0f;
                float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
                float colW = 140.0f + extraParamsW;
                float colSpacing = 6.0f;
                float startRowY = contentBounds_.y + 62.0f;
                float rowH = 24.0f;

                if (elapsedMs >= 350 && dist <= 14.0f) {
                    touchLongPressArmed_ = true;
                    dragMode_ = DragMode::TrackerBlockSelect;
                    dragStartPoint_ = touchDownPos_;
                    trackerHasBlockSelection_ = true;
                    int curStep = std::clamp(static_cast<int>((ev.y - startRowY + trackerScrollY_) / rowH), 0, 63);
                    float relX = ev.x - (contentBounds_.x + rowNumW) + trackerScrollX_;
                    int curCol = std::clamp(static_cast<int>(relX / (colW + colSpacing)), 0, trackerNumLanes_ - 1);
                    trackerSelectedStep_ = curStep;
                    trackerSelectedCol_ = curCol;

                    int minStep = std::min(trackerBlockAnchorStep_, trackerSelectedStep_);
                    int maxStep = std::max(trackerBlockAnchorStep_, trackerSelectedStep_);
                    int minCol = std::min(trackerBlockAnchorCol_, trackerSelectedCol_);
                    int maxCol = std::max(trackerBlockAnchorCol_, trackerSelectedCol_);
                    for (auto& n : notes_) {
                        int nStep = static_cast<int>(std::round(n.startStep));
                        n.isSelected = (nStep >= minStep && nStep <= maxStep && n.column >= minCol && n.column <= maxCol);
                    }
                    return true;
                } else if (dist > 12.0f) {
                    touchPanCommitted_ = true;
                    dragMode_ = DragMode::TouchPan;
                    dragStartPoint_ = touchDownPos_;
                    kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                }
            } else {
                if (elapsedMs >= 380 && dist <= 14.0f) {
                    touchLongPressArmed_ = true;
                    dragMode_ = DragMode::MarqueeSelect;
                    dragStartPoint_ = touchDownPos_;
                    marqueeCurrentPoint_ = Point2D{ev.x, ev.y};
                    clearSelection();
                    return true;
                } else if (dist > 14.0f) {
                    touchPanCommitted_ = true;
                    dragMode_ = DragMode::TouchPan;
                    kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
                }
            }
        }

        if (dragMode_ == DragMode::TouchPan) {
            kineticScroller_.addSample(ev.x, ev.y, ev.timestampMs);
            if (subView_ == EditSubViewMode::PianoRoll) {
                scrollX_ = (std::max)(0.0f, panStartScrollX_ - (ev.x - dragStartPoint_.x));
                float maxScrollY = (std::max)(0.0f, (maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
                scrollY_ = std::clamp(panStartScrollY_ - (ev.y - dragStartPoint_.y), 0.0f, maxScrollY);
            } else if (subView_ == EditSubViewMode::Score) {
                scoreScrollX_ = (std::max)(0.0f, panStartScrollX_ - (ev.x - dragStartPoint_.x));
            } else if (subView_ == EditSubViewMode::Tracker) {
                float rowNumW = 48.0f;
                float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
                float colW = 140.0f + extraParamsW;
                float colSpacing = 6.0f;
                float rowH = 24.0f;
                float totalLanesW = static_cast<float>(trackerNumLanes_) * (colW + colSpacing);
                float maxScrollX = std::max(0.0f, totalLanesW - (contentBounds_.w - rowNumW));
                float maxScrollY = std::max(0.0f, 64.0f * rowH - (contentBounds_.h - 62.0f));

                trackerScrollX_ = std::clamp(panStartScrollX_ - (ev.x - dragStartPoint_.x), 0.0f, maxScrollX);
                trackerScrollY_ = std::clamp(panStartScrollY_ - (ev.y - dragStartPoint_.y), 0.0f, maxScrollY);
            }
            return true;
        }

        if (dragMode_ == DragMode::MiddlePan) {
            if (subView_ == EditSubViewMode::Tracker) {
                float rowNumW = 48.0f;
                float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
                float colW = 140.0f + extraParamsW;
                float colSpacing = 6.0f;
                float rowH = 24.0f;
                float totalLanesW = static_cast<float>(trackerNumLanes_) * (colW + colSpacing);
                float maxScrollX = std::max(0.0f, totalLanesW - (contentBounds_.w - rowNumW));
                float maxScrollY = std::max(0.0f, 64.0f * rowH - (contentBounds_.h - 62.0f));

                trackerScrollX_ = std::clamp(panStartScrollX_ - (ev.x - dragStartPoint_.x), 0.0f, maxScrollX);
                trackerScrollY_ = std::clamp(panStartScrollY_ - (ev.y - dragStartPoint_.y), 0.0f, maxScrollY);
            } else {
                scrollX_ = (std::max)(0.0f, panStartScrollX_ - (ev.x - dragStartPoint_.x));
                float maxScrollY = (std::max)(0.0f, (maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
                scrollY_ = std::clamp(panStartScrollY_ - (ev.y - dragStartPoint_.y), 0.0f, maxScrollY);
            }
            return true;
        }

        if (dragMode_ == DragMode::TrackerBlockSelect) {
            float dist = std::hypot(ev.x - dragStartPoint_.x, ev.y - dragStartPoint_.y);
            if (dist >= 6.0f) {
                trackerHasBlockSelection_ = true;
                float rowNumW = 48.0f;
                float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
                float colW = 140.0f + extraParamsW;
                float colSpacing = 6.0f;
                float startRowY = contentBounds_.y + 62.0f;
                float rowH = 24.0f;

                int curStep = std::clamp(static_cast<int>((ev.y - startRowY + trackerScrollY_) / rowH), 0, 63);
                float relX = ev.x - (contentBounds_.x + rowNumW) + trackerScrollX_;
                int curCol = std::clamp(static_cast<int>(relX / (colW + colSpacing)), 0, trackerNumLanes_ - 1);
                trackerSelectedStep_ = curStep;
                trackerSelectedCol_ = curCol;

                int minStep = std::min(trackerBlockAnchorStep_, trackerSelectedStep_);
                int maxStep = std::max(trackerBlockAnchorStep_, trackerSelectedStep_);
                int minCol = std::min(trackerBlockAnchorCol_, trackerSelectedCol_);
                int maxCol = std::max(trackerBlockAnchorCol_, trackerSelectedCol_);

                for (auto& n : notes_) {
                    int nStep = static_cast<int>(std::round(n.startStep));
                    n.isSelected = (nStep >= minStep && nStep <= maxStep && n.column >= minCol && n.column <= maxCol);
                }
                return true;
            }
        }

        if (dragMode_ == DragMode::GhostSlider) {
            float rel = (ev.x - ghostSliderBounds_.x) / ghostSliderBounds_.w;
            setGhostNotesOpacity(rel);
            return true;
        }

        if (dragMode_ == DragMode::MarqueeSelect) {
            marqueeCurrentPoint_ = Point2D{ev.x, ev.y};
            float mx = std::min(dragStartPoint_.x, marqueeCurrentPoint_.x);
            float my = std::min(dragStartPoint_.y, marqueeCurrentPoint_.y);
            float mw = std::abs(marqueeCurrentPoint_.x - dragStartPoint_.x);
            float mh = std::abs(marqueeCurrentPoint_.y - dragStartPoint_.y);
            Rect2D mRect(mx, my, mw, mh);

            if (subView_ == EditSubViewMode::PianoRoll) {
                for (auto& n : notes_) {
                    float nx = gridBounds_.x + n.startStep * stepWidth_ - scrollX_;
                    float nw = n.durationSteps * stepWidth_;
                    float ny = gridBounds_.y + static_cast<float>(maxPitch_ - n.pitch) * semitoneHeight_ - scrollY_;
                    float nh = semitoneHeight_;
                    n.isSelected = mRect.intersects(Rect2D(nx, ny, nw, nh));
                }
            } else if (subView_ == EditSubViewMode::Score) {
                float staffStartX = contentBounds_.x + 50.0f;
                float staffY = contentBounds_.y + 110.0f;
                float scoreStepW = 34.0f;
                float lineSpacing = 14.0f;
                bool isTreble = (scoreClef_ == ScoreClef::Treble) || (scoreClef_ == ScoreClef::Auto && activeTrackIndex_ != 0 && activeTrackIndex_ != 2);
                int refPitch = isTreble ? 71 : 50;

                for (auto& n : notes_) {
                    float nx = staffStartX + 160.0f + n.startStep * scoreStepW - scoreScrollX_;
                    float ny = staffY + 2.0f * lineSpacing - static_cast<float>(static_cast<int>(n.pitch) - refPitch) * (lineSpacing * 0.5f);
                    n.isSelected = mRect.intersects(Rect2D(nx - 8.0f, ny - 8.0f, 16.0f, 16.0f));
                }
            }
            return true;
        }

        if (dragMode_ == DragMode::MoveNotes && activeDragNoteIdx_ >= 0) {
            float deltaX = ev.x - dragStartPoint_.x;
            float deltaY = ev.y - dragStartPoint_.y;
            float stepDelta = std::round(deltaX / stepWidth_);
            int pitchDelta = -static_cast<int>(std::round(deltaY / semitoneHeight_));

            size_t selIdx = 0;
            for (auto& n : notes_) {
                if (n.isSelected && selIdx < batchDragOrigins_.size()) {
                    n.startStep = std::max(0.0f, batchDragOrigins_[selIdx].first + stepDelta);
                    n.pitch = static_cast<uint8_t>(std::clamp(batchDragOrigins_[selIdx].second + pitchDelta, 0, 127));
                    selIdx++;
                }
            }
            return true;
        }

        if (dragMode_ == DragMode::ResizeNotes && activeDragNoteIdx_ >= 0 && activeDragNoteIdx_ < static_cast<int>(notes_.size())) {
            float deltaX = ev.x - dragStartPoint_.x;
            float durDelta = deltaX / stepWidth_;
            notes_[activeDragNoteIdx_].durationSteps = std::clamp(activeDragInitialDur_ + durDelta, 0.25f, 64.0f);
            return true;
        }
    } else if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        if (subView_ == EditSubViewMode::Script) {
            if (scriptEditor_.handlePointer(ev, ctx)) return true;
        }

        if (dragMode_ == DragMode::TouchPan) {
            kineticScroller_.endDrag(ev.timestampMs);
            dragMode_ = DragMode::None;
            return true;
        }

        if (dragMode_ == DragMode::TrackerBlockSelect) {
            dragMode_ = DragMode::None;
            if (!trackerHasBlockSelection_) {
                int clickedStep = trackerBlockAnchorStep_;
                int clickedCol = trackerBlockAnchorCol_;
                if (clickedStep >= 0 && clickedStep < 64 && clickedCol >= 0 && clickedCol < trackerNumLanes_) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTrackerClickTime_).count();
                    float clickDist = std::hypot(ev.x - lastTrackerClickPos_.x, ev.y - lastTrackerClickPos_.y);
                    bool isDouble = (elapsedMs < 400 && clickDist < 25.0f && lastTrackerClickedStep_ == clickedStep && lastTrackerClickedCol_ == clickedCol);

                    trackerSelectedStep_ = clickedStep;
                    trackerSelectedCol_ = clickedCol;

                    if (isDouble) {
                        openTrackerCellDialog(clickedStep, clickedCol);
                        trackerCellDialog_.onAudition = [this, &ctx](uint8_t p, float v) {
                            auditionPitch(p, v, ctx);
                        };
                        lastTrackerClickTime_ = std::chrono::steady_clock::time_point{};
                        return true;
                    }

                    lastTrackerClickTime_ = now;
                    lastTrackerClickPos_ = Point2D{ev.x, ev.y};
                    lastTrackerClickedStep_ = clickedStep;
                    lastTrackerClickedCol_ = clickedCol;

                    clearSelection();
                    for (auto& n : notes_) {
                        if (static_cast<int>(std::round(n.startStep)) == clickedStep && n.column == clickedCol) {
                            n.isSelected = true;
                            int audP = (hasActiveChordFollowOrMidiFx() && &n >= notes_.data() && static_cast<size_t>(&n - notes_.data()) < evaluatedFxNotes_.size())
                                ? evaluatedFxNotes_[&n - notes_.data()].pitch : n.pitch;
                            auditionPitch(audP, n.velocity, ctx, hasActiveChordFollowOrMidiFx());
                            break;
                        }
                    }
                    ensureTrackerCursorVisible();
                    return true;
                }
            }
            return true;
        }

        if (dragMode_ == DragMode::TouchGridPending) {
            dragMode_ = DragMode::None;
            if (ev.action == PointerAction::Up) {
                if (subView_ == EditSubViewMode::Tracker) {
                    float rowNumW = 48.0f;
                    float extraParamsW = (trackerShowCustomParams_ ? static_cast<float>(trackerCustomCols_.size()) * 26.0f : 0.0f);
                    float colW = 140.0f + extraParamsW;
                    float colSpacing = 6.0f;
                    float startRowY = contentBounds_.y + 62.0f;
                    float rowH = 24.0f;

                    int clickedStep = static_cast<int>((touchDownPos_.y - startRowY + trackerScrollY_) / rowH);
                    float relX = touchDownPos_.x - (contentBounds_.x + rowNumW) + trackerScrollX_;
                    int clickedCol = static_cast<int>(relX / (colW + colSpacing));

                    if (clickedStep >= 0 && clickedStep < 64 && clickedCol >= 0 && clickedCol < trackerNumLanes_) {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTrackerClickTime_).count();
                        float clickDist = std::hypot(ev.x - lastTrackerClickPos_.x, ev.y - lastTrackerClickPos_.y);
                        bool isDouble = (elapsedMs < 400 && clickDist < 25.0f && lastTrackerClickedStep_ == clickedStep && lastTrackerClickedCol_ == clickedCol);

                        trackerSelectedStep_ = clickedStep;
                        trackerSelectedCol_ = clickedCol;
                        trackerHasBlockSelection_ = false;

                        if (isDouble) {
                            openTrackerCellDialog(clickedStep, clickedCol);
                            trackerCellDialog_.onAudition = [this, &ctx](uint8_t p, float v) {
                                auditionPitch(p, v, ctx);
                            };
                            lastTrackerClickTime_ = std::chrono::steady_clock::time_point{};
                            return true;
                        }

                        lastTrackerClickTime_ = now;
                        lastTrackerClickPos_ = Point2D{ev.x, ev.y};
                        lastTrackerClickedStep_ = clickedStep;
                        lastTrackerClickedCol_ = clickedCol;

                        clearSelection();
                        for (auto& n : notes_) {
                            if (static_cast<int>(std::round(n.startStep)) == clickedStep && n.column == clickedCol) {
                                n.isSelected = true;
                                int audP = (hasActiveChordFollowOrMidiFx() && &n >= notes_.data() && static_cast<size_t>(&n - notes_.data()) < evaluatedFxNotes_.size())
                                    ? evaluatedFxNotes_[&n - notes_.data()].pitch : n.pitch;
                                auditionPitch(audP, n.velocity, ctx, hasActiveChordFollowOrMidiFx());
                                break;
                            }
                        }
                        ensureTrackerCursorVisible();
                        return true;
                    }
                } else if (subView_ == EditSubViewMode::PianoRoll && gridBounds_.contains(ev.x, ev.y)) {
                    auto now = std::chrono::steady_clock::now();
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGridClickTime_).count();
                    float clickDist = std::hypot(ev.x - lastGridClickPos_.x, ev.y - lastGridClickPos_.y);

                    if (elapsedMs < 350 && clickDist < 20.0f) {
                        float step = std::floor((ev.x - gridBounds_.x + scrollX_) / stepWidth_);
                        int pitch = maxPitch_ - static_cast<int>((ev.y - gridBounds_.y + scrollY_) / semitoneHeight_);
                        if (step >= 0.0f && step < 128.0f && pitch >= minPitch_ && pitch <= maxPitch_) {
                            addNote(static_cast<uint8_t>(pitch), step, 1.0f, 0.85f);
                            if (!notes_.empty()) {
                                notes_.back().isSelected = true;
                            }
                            auditionPitch(pitch, 0.85f, ctx);
                            if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
                        }
                        lastGridClickTime_ = std::chrono::steady_clock::time_point{};
                    } else {
                        clearSelection();
                        lastGridClickTime_ = now;
                        lastGridClickPos_ = Point2D{ev.x, ev.y};
                    }
                }
            }
            return true;
        }

        if (dragMode_ == DragMode::MarqueeSelect) {
            float dist = std::hypot(ev.x - dragStartPoint_.x, ev.y - dragStartPoint_.y);
            if (dist < 4.0f) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGridClickTime_).count();
                float clickDist = std::hypot(ev.x - lastGridClickPos_.x, ev.y - lastGridClickPos_.y);

                if (subView_ == EditSubViewMode::PianoRoll && gridBounds_.contains(ev.x, ev.y)) {
                    // Double-click note creation and hold-to-resize are handled on Down.
                    // Preserve lastGridClickTime_ and lastGridClickPos_ for the second click Down event.
                } else if (subView_ == EditSubViewMode::Score) {
                    float staffStartX = contentBounds_.x + 50.0f;
                    float staffEndX = contentBounds_.x + contentBounds_.w - 40.0f;
                    float staffY = contentBounds_.y + 110.0f;
                    float scoreStepW = 34.0f;
                    float lineSpacing = 14.0f;
                    if (ev.x >= staffStartX + 150.0f && ev.x <= staffEndX && ev.y >= staffY - 40.0f && ev.y <= staffY + 5.0f * lineSpacing + 40.0f) {
                        if (elapsedMs < 350 && clickDist < 14.0f) {
                            // Double-click creates a note on score staff
                            float step = std::floor((ev.x - (staffStartX + 160.0f) + scoreScrollX_) / scoreStepW);
                            bool isTreble = (scoreClef_ == ScoreClef::Treble) || (scoreClef_ == ScoreClef::Auto && activeTrackIndex_ != 0 && activeTrackIndex_ != 2);
                            int refPitch = isTreble ? 71 : 50;
                            float relStaffY = (staffY + 2.0f * lineSpacing - ev.y) / (lineSpacing * 0.5f);
                            int pitch = std::clamp(refPitch + static_cast<int>(std::round(relStaffY)), 24, 96);
                            float dur = 1.0f;
                            switch (scoreDuration_) {
                                case ScoreNoteType::Whole: dur = 16.0f; break;
                                case ScoreNoteType::Half: dur = 8.0f; break;
                                case ScoreNoteType::Quarter: dur = 4.0f; break;
                                case ScoreNoteType::Eighth: dur = 2.0f; break;
                                case ScoreNoteType::Sixteenth: dur = 1.0f; break;
                                default: dur = 1.0f; break;
                            }
                            addNote(static_cast<uint8_t>(pitch), step, dur, 0.85f);
                            if (!notes_.empty()) {
                                notes_.back().isSelected = true;
                            }
                            auditionPitch(pitch, 0.85f, ctx);
                            if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
                            lastGridClickTime_ = std::chrono::steady_clock::time_point{};
                        } else {
                            // Single click clears selection
                            clearSelection();
                            lastGridClickTime_ = now;
                            lastGridClickPos_ = Point2D{ev.x, ev.y};
                        }
                    }
                }
            }
        }

        if (dragMode_ != DragMode::None) {
            bool modifiedNotes = (dragMode_ == DragMode::MoveNotes || dragMode_ == DragMode::ResizeNotes);
            dragMode_ = DragMode::None;
            if (modifiedNotes) {
                formatEatscriptFromNotes();
                if (onNotesChanged) onNotesChanged(activeTrackIndex_, activeClipIndex_);
                else if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
            }
            return true;
        }

        if (pianoKeyboard_.hasActiveTouches()) {
            if (pianoKeyboard_.handlePointer(ev, pianoGutterBounds_, scrollY_, ctx.audioEngine, activeTrackIndex_)) {
                return true;
            }
        }
        if (pianoGutterBounds_.contains(ev.x, ev.y)) {
            float localY = ev.y - pianoGutterBounds_.y + scrollY_;
            int pitch = maxPitch_ - static_cast<int>(localY / semitoneHeight_);
            stopAuditionPitch(pitch, ctx);
            return true;
        }
        if (auditioningPitch_ >= 0) {
            stopAuditionPitch(auditioningPitch_, ctx);
            return true;
        }
    } else if (ev.action == PointerAction::Scroll) {
        if (hasSelectedNotes() && sidebarBounds_.contains(ev.x, ev.y)) {
            float maxScroll = std::max(0.0f, 400.0f - sidebarBounds_.h);
            sidebarScrollY_ = std::clamp(sidebarScrollY_ - ev.scrollY * 24.0f, 0.0f, maxScroll);
            return true;
        }
        if (subView_ == EditSubViewMode::PianoRoll) {
            scrollX_ = std::max(0.0f, scrollX_ - ev.scrollX * 28.0f);
            float maxScrollY = std::max(0.0f, (maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
            scrollY_ = std::clamp(scrollY_ - ev.scrollY * 22.0f, 0.0f, maxScrollY);
        } else if (subView_ == EditSubViewMode::Tracker) {
            trackerScrollY_ = std::max(0.0f, trackerScrollY_ - ev.scrollY * 24.0f);
            if (std::abs(ev.scrollX) > 0.01f) {
                trackerScrollX_ = std::max(0.0f, trackerScrollX_ - ev.scrollX * 28.0f);
            }
        } else if (subView_ == EditSubViewMode::Score) {
            scoreScrollX_ = std::max(0.0f, scoreScrollX_ - ev.scrollX * 34.0f);
        } else if (subView_ == EditSubViewMode::Script) {
            if (scriptEditor_.handlePointer(ev, ctx)) return true;
        }
        return true;
    }

    return false;
}

bool EditView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
    if (trackerCellDialog_.isOpen()) {
        trackerCellDialog_.onAudition = [this, &ctx](uint8_t p, float v) {
            auditionPitch(p, v, ctx);
        };
        if (trackerCellDialog_.handleKey(key, scancode, action, mods)) {
            return true;
        }
        return true;
    }

    if (subView_ == EditSubViewMode::Script) {
        if (scriptEditor_.handleKey(key, scancode, action, mods, ctx)) {
            return true;
        }
        if (action == 1) {
            bool isCtrl = (mods & 2) != 0;
            if ((isCtrl && (key == 257 || key == 10 || key == 13)) || key == 294) {
                parseNotesFromEatscript();
                if (ctx.onShowNotification) ctx.onShowNotification("Eatscript compiled to clip notes");
                return true;
            }
        }
        return false;
    }

    if (action != 1) return false;

    bool isCtrl = (mods & 2) != 0;

    // Spacebar toggles playback
    if (key == 32) {
        if (ctx.audioEngine) {
            auto& seq = ctx.audioEngine->getSequencer();
            if (seq.isPlaying()) {
                seq.stop();
            } else {
                seq.start();
            }
            return true;
        }
    }

    if (key == 261 || key == 259) {
        if (subView_ == EditSubViewMode::Tracker) {
            notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [&](const PianoRollNote& n) {
                return (static_cast<int>(std::round(n.startStep)) == trackerSelectedStep_ && n.column == trackerSelectedCol_);
            }), notes_.end());
            if (trackerEditStep_ > 0) {
                trackerSelectedStep_ = (trackerSelectedStep_ + trackerEditStep_) % 64;
                ensureTrackerCursorVisible();
            }
            formatEatscriptFromNotes();
            if (onNotesChanged) onNotesChanged(activeTrackIndex_, activeClipIndex_);
            else if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
            return true;
        }
        if (hasSelectedNotes()) {
            deleteSelectedNotes();
            return true;
        }
    }

    if (isCtrl && (key == 65 || key == 97)) {
        selectAllNotes();
        return true;
    }

    if (key == 256) {
        clearSelection();
        return true;
    }

    if (subView_ == EditSubViewMode::Tracker) {
        if (key == 265) { // Up
            trackerSelectedStep_ = (trackerSelectedStep_ == 0) ? 63 : trackerSelectedStep_ - 1;
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 264) { // Down
            trackerSelectedStep_ = (trackerSelectedStep_ + 1) % 64;
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 263) { // Left
            trackerSelectedCol_ = std::max(0, trackerSelectedCol_ - 1);
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 262) { // Right
            trackerSelectedCol_ = std::min(trackerNumLanes_ - 1, trackerSelectedCol_ + 1);
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 258) { // Tab
            if (mods & 1) { // Shift+Tab
                trackerSelectedCol_ = (trackerSelectedCol_ + trackerNumLanes_ - 1) % trackerNumLanes_;
            } else {
                trackerSelectedCol_ = (trackerSelectedCol_ + 1) % trackerNumLanes_;
            }
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 266) { // PageUp (-16 steps)
            trackerSelectedStep_ = (trackerSelectedStep_ < 16) ? 0 : trackerSelectedStep_ - 16;
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 267) { // PageDown (+16 steps)
            trackerSelectedStep_ = std::min(63, trackerSelectedStep_ + 16);
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 268) { // Home
            trackerSelectedStep_ = 0;
            ensureTrackerCursorVisible();
            return true;
        }
        if (key == 269) { // End
            trackerSelectedStep_ = 63;
            ensureTrackerCursorVisible();
            return true;
        }

        if (key == 91 || key == 45) {
            trackerBaseOctave_ = std::clamp(trackerBaseOctave_ - 1, 0, 8);
            return true;
        }
        if (key == 93 || key == 61) {
            trackerBaseOctave_ = std::clamp(trackerBaseOctave_ + 1, 0, 8);
            return true;
        }

        if ((mods & 1) && (key == 83 || key == 115)) {
            for (auto& n : notes_) {
                if (static_cast<int>(std::round(n.startStep)) == trackerSelectedStep_ && n.column == trackerSelectedCol_) {
                    n.isSlide = !n.isSlide;
                    break;
                }
            }
            formatEatscriptFromNotes();
            if (onNotesChanged) onNotesChanged(activeTrackIndex_, activeClipIndex_);
            else if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
            return true;
        }

        int semitone = -1;
        switch (key) {
            // Lower Octave
            case 90: case 122: semitone = 0; break;  // Z -> C
            case 83: case 115: semitone = 1; break;  // S -> C#
            case 88: case 120: semitone = 2; break;  // X -> D
            case 68: case 100: semitone = 3; break;  // D -> D#
            case 67: case 99:  semitone = 4; break;  // C -> E
            case 86: case 118: semitone = 5; break;  // V -> F
            case 71: case 103: semitone = 6; break;  // G -> F#
            case 66: case 98:  semitone = 7; break;  // B -> G
            case 72: case 104: semitone = 8; break;  // H -> G#
            case 78: case 110: semitone = 9; break;  // N -> A
            case 74: case 106: semitone = 10; break; // J -> A#
            case 77: case 109: semitone = 11; break; // M -> B

            // Upper Octave (+1 Octave)
            case 81: case 113: semitone = 12; break; // Q -> C
            case 50:           semitone = 13; break; // 2 -> C#
            case 87: case 119: semitone = 14; break; // W -> D
            case 51:           semitone = 15; break; // 3 -> D#
            case 69: case 101: semitone = 16; break; // E -> E
            case 82: case 114: semitone = 17; break; // R -> F
            case 53:           semitone = 18; break; // 5 -> F#
            case 84: case 116: semitone = 19; break; // T -> G
            case 54:           semitone = 20; break; // 6 -> G#
            case 89: case 121: semitone = 21; break; // Y -> A
            case 55:           semitone = 22; break; // 7 -> A#
            case 85: case 117: semitone = 23; break; // U -> B
            case 73: case 105: semitone = 24; break; // I -> C (+2 Octaves)
            case 57:           semitone = 25; break; // 9 -> C#
            case 79: case 111: semitone = 26; break; // O -> D
            case 48:           semitone = 27; break; // 0 -> D#
            case 80: case 112: semitone = 28; break; // P -> E
        }

        if (semitone >= 0) {
            uint8_t pitch = static_cast<uint8_t>(std::clamp((trackerBaseOctave_ + 1) * 12 + semitone, 0, 127));
            bool found = false;
            for (auto& n : notes_) {
                if (static_cast<int>(std::round(n.startStep)) == trackerSelectedStep_ && n.column == trackerSelectedCol_) {
                    n.pitch = pitch;
                    n.velocity = 0.85f;
                    found = true;
                    break;
                }
            }
            if (!found) {
                PianoRollNote n;
                n.id = "n" + std::to_string(notes_.size());
                n.pitch = pitch;
                n.startStep = static_cast<float>(trackerSelectedStep_);
                n.durationSteps = 1.0f;
                n.velocity = 0.85f;
                n.column = trackerSelectedCol_;
                notes_.push_back(n);
            }

            auditionPitch(pitch, 0.85f, ctx);
            if (trackerEditStep_ > 0) {
                trackerSelectedStep_ = (trackerSelectedStep_ + trackerEditStep_) % 64;
                ensureTrackerCursorVisible();
            }
            formatEatscriptFromNotes();
            if (onNotesChanged) onNotesChanged(activeTrackIndex_, activeClipIndex_);
            else if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
            return true;
        }
    }

    return false;
}

bool EditView::handleChar(char32_t codepoint, const ViewContext& ctx) {
    if (subView_ == EditSubViewMode::Script) {
        return scriptEditor_.handleChar(codepoint, ctx);
    }
    return false;
}

} // namespace eatsbits::ui
