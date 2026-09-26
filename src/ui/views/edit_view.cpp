#include "eatsbits/ui/views/edit_view.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/widgets/piano_keyboard.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace eatsbits::ui {

EditView::EditView() {
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

    formatEatscriptFromNotes();
}

void EditView::setActiveTrackIndex(uint32_t idx) noexcept {
    activeTrackIndex_ = idx;
    switch (idx) {
        case 0:
            activeTrackName_ = "TB-303 Acid";
            trackColor_ = Color(0.0f, 0.95f, 1.0f); // Cyan
            break;
        case 1:
            activeTrackName_ = "TR-808 Drums";
            trackColor_ = Color(1.0f, 0.35f, 0.45f); // Red/Coral
            break;
        case 2:
            activeTrackName_ = "Sub Bass";
            trackColor_ = Color(0.95f, 0.75f, 0.10f); // Gold
            break;
        case 3:
            activeTrackName_ = "Poly Lead";
            trackColor_ = Color(0.85f, 0.25f, 0.95f); // Magenta
            break;
        case 4:
            activeTrackName_ = "Waveguide Piano";
            trackColor_ = Color(0.15f, 0.90f, 0.50f); // Green
            break;
        default:
            activeTrackName_ = "Track " + std::to_string(idx + 1);
            trackColor_ = Color(0.4f, 0.8f, 1.0f);
            break;
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

void EditView::deleteSelectedNotes() noexcept {
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [](const PianoRollNote& n) {
        return n.isSelected;
    }), notes_.end());
    formatEatscriptFromNotes();
}

void EditView::transposeSelectedNotes(int semitones) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            int newPitch = static_cast<int>(n.pitch) + semitones;
            n.pitch = static_cast<uint8_t>(std::clamp(newPitch, 0, 127));
        }
    }
    formatEatscriptFromNotes();
}

void EditView::nudgeSelectedNotes(float deltaSteps) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.startStep = std::max(0.0f, n.startStep + deltaSteps);
        }
    }
    formatEatscriptFromNotes();
}

void EditView::changeSelectedNotesDuration(float deltaDuration) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.durationSteps = std::clamp(n.durationSteps + deltaDuration, 0.25f, 16.0f);
        }
    }
    formatEatscriptFromNotes();
}

void EditView::setSelectedNotesVelocity(float velocity) noexcept {
    float v = std::clamp(velocity, 0.05f, 1.0f);
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.velocity = v;
        }
    }
    formatEatscriptFromNotes();
}

void EditView::humanizeSelectedNotes(float amount) noexcept {
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].isSelected) {
            float r = (static_cast<float>((i * 7 + 13) % 31) / 30.0f - 0.5f) * 2.0f;
            notes_[i].velocity = std::clamp(notes_[i].velocity + r * amount, 0.1f, 1.0f);
        }
    }
    formatEatscriptFromNotes();
}

void EditView::quantizeSelectedNotes(float snapSteps) noexcept {
    float snap = (snapSteps > 0.01f) ? snapSteps : 1.0f;
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.startStep = std::round(n.startStep / snap) * snap;
        }
    }
    formatEatscriptFromNotes();
}

void EditView::setSelectedNotesSlide(bool slide) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.isSlide = slide;
        }
    }
    formatEatscriptFromNotes();
}

void EditView::setSelectedNotesArticulation(const std::string& art) noexcept {
    for (auto& n : notes_) {
        if (n.isSelected) {
            n.articulation = art;
        }
    }
    formatEatscriptFromNotes();
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
    n.column = trackerSelectedCol_;
    n.effectCommand = "00";
    clearSelection();
    notes_.push_back(n);
    formatEatscriptFromNotes();
}

void EditView::deleteNoteAt(float step, uint8_t pitch) {
    notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [=](const PianoRollNote& n) {
        bool matchPitch = (n.pitch == pitch);
        bool matchStep = (std::abs(n.startStep - step) < 0.5f || (step >= n.startStep && step < n.startStep + n.durationSteps));
        return matchPitch && matchStep;
    }), notes_.end());
    formatEatscriptFromNotes();
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
    formatEatscriptFromNotes();
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
    formatEatscriptFromNotes();
}

void EditView::syncFromSequencer(const sequencer::StepSequencer& seq) {
    if (activeTrackIndex_ >= seq.getNumTracks()) return;
    const auto* track = seq.getTrack(activeTrackIndex_);
    if (!track) return;

    notes_.clear();
    for (uint32_t s = 0; s < track->getNumSteps(); ++s) {
        const auto& step = track->getStep(s);
        if (step.active) {
            PianoRollNote n;
            n.id = "n" + std::to_string(s);
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
                gn.id = "g_" + std::to_string(t) + "_" + std::to_string(s);
                gn.pitch = step.note;
                gn.startStep = static_cast<float>(s);
                gn.durationSteps = std::max(0.25f, step.gateLength);
                gn.velocity = step.velocity;
                gn.column = static_cast<int>(t);
                ghostNotes_.push_back(gn);
            }
        }
    }

    formatEatscriptFromNotes();
    autoCenterOnNotesOrDefault();
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

    // Write back notes
    for (const auto& n : notes_) {
        uint32_t s = static_cast<uint32_t>(std::clamp(std::round(n.startStep), 0.0f, 63.0f));
        if (s < track->getNumSteps()) {
            sequencer::StepData sd{};
            sd.active = true;
            sd.note = n.pitch;
            sd.velocity = n.velocity;
            sd.gateLength = n.durationSteps;
            sd.slide = n.isSlide;
            sd.accent = n.isAccent;
            track->setStep(s, sd);
        }
    }
}

void EditView::formatEatscriptFromNotes() {
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
        if (n.isSlide) ss << ", slide = true";
        if (n.isAccent) ss << ", accent = true";
        if (!n.articulation.empty() && n.articulation != "normal") ss << ", art = \"" << n.articulation << "\"";
        if (!n.lyric.empty()) ss << ", lyric = \"" << n.lyric << "\"";
        ss << " }\n";
    }

    scriptBuffer_ = ss.str();
}

void EditView::parseNotesFromEatscript() {
    std::vector<PianoRollNote> newNotes;
    std::istringstream stream(scriptBuffer_);
    std::string line;
    int idx = 0;

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

        if (line.find("slide = true") != std::string::npos) n.isSlide = true;
        if (line.find("accent = true") != std::string::npos) n.isAccent = true;

        newNotes.push_back(n);
    }

    if (!newNotes.empty()) {
        notes_ = std::move(newNotes);
    }
}

void EditView::auditionPitch(int pitch, float velocity, const ViewContext& ctx) {
    auditioningPitch_ = pitch;
    if (ctx.audioEngine) {
        ctx.audioEngine->postNoteOn(static_cast<uint8_t>(std::clamp(pitch, 0, 127)), velocity, false, false, static_cast<int>(activeTrackIndex_));
    }
}

void EditView::stopAuditionPitch(int pitch, const ViewContext& ctx) {
    if (auditioningPitch_ == pitch) {
        auditioningPitch_ = -1;
    }
    if (ctx.audioEngine) {
        ctx.audioEngine->postNoteOff(static_cast<uint8_t>(std::clamp(pitch, 0, 127)), static_cast<int>(activeTrackIndex_));
    }
}

void EditView::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    float headerH = 34.0f;
    headerBounds_ = Rect2D(bounds_.x, bounds_.y, bounds_.w, headerH);

    bool showSidebar = hasSelectedNotes();
    float sidebarW = showSidebar ? 265.0f : 0.0f;

    contentBounds_ = Rect2D(bounds_.x, bounds_.y + headerH, bounds_.w - sidebarW, bounds_.h - headerH);
    sidebarBounds_ = Rect2D(bounds_.x + bounds_.w - sidebarW, bounds_.y + headerH, sidebarW, bounds_.h - headerH);

    float btnW = ctx.isMobile ? 32.0f : 88.0f;
    float btnH = 26.0f;
    float btnY = bounds_.y + 4.0f;
    float rightMargin = bounds_.x + bounds_.w - 12.0f;

    btnScript_ = Rect2D(rightMargin - btnW, btnY, btnW, btnH);
    btnScore_ = Rect2D(btnScript_.x - btnW - 6.0f, btnY, btnW, btnH);
    btnTracker_ = Rect2D(btnScore_.x - btnW - 6.0f, btnY, btnW, btnH);
    btnPianoRoll_ = Rect2D(btnTracker_.x - btnW - 6.0f, btnY, btnW, btnH);

    ghostToggleBtn_ = Rect2D(btnPianoRoll_.x - 170.0f, btnY, 65.0f, btnH);
    ghostSliderBounds_ = Rect2D(btnPianoRoll_.x - 100.0f, btnY, 90.0f, btnH);

    float gutterW = 80.0f;
    float velocityH = 72.0f;
    pianoGutterBounds_ = Rect2D(contentBounds_.x, contentBounds_.y, gutterW, contentBounds_.h - velocityH);
    gridBounds_ = Rect2D(contentBounds_.x + gutterW, contentBounds_.y, contentBounds_.w - gutterW, contentBounds_.h - velocityH);
    velocityLaneBounds_ = Rect2D(contentBounds_.x + gutterW, contentBounds_.y + contentBounds_.h - velocityH, contentBounds_.w - gutterW, velocityH);

    float maxScroll = std::max(0.0f, static_cast<float>(maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
    scrollY_ = std::clamp(scrollY_, 0.0f, maxScroll);

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
}

void EditView::render(const ViewContext& ctx) {
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
}

void EditView::renderSubNavHeader(const ViewContext& ctx) {
    auto& r = *ctx.renderer;
    const auto& theme = *ctx.theme;

    drawRect(r, headerBounds_.x, headerBounds_.y, headerBounds_.w, headerBounds_.h, theme.panelHeader);
    drawLine(r, headerBounds_.x, headerBounds_.y + headerBounds_.h, headerBounds_.x + headerBounds_.w, headerBounds_.y + headerBounds_.h,
             theme.borderSubtle, 0.8f, 1.5f);

    drawCircle(r, headerBounds_.x + 16.0f, headerBounds_.y + 17.0f, 5.0f, trackColor_);
    std::string editTitle = ctx.isMobile ? activeTrackName_ : ("EDITING: " + activeTrackName_);
    drawText(r, editTitle, headerBounds_.x + 28.0f, headerBounds_.y + 10.0f, 11.5f, theme.textPrimary);

    if (subView_ == EditSubViewMode::PianoRoll || subView_ == EditSubViewMode::Score) {
        bool isGhostActive = (ghostOpacity_ > 0.001f);
        drawRoundedRect(r, ghostToggleBtn_.x, ghostToggleBtn_.y, ghostToggleBtn_.w, ghostToggleBtn_.h, 4.0f,
                        isGhostActive ? (theme.primaryAccent * 0.25f) : theme.controlBackground, 0.85f);
        drawRoundedRectOutline(r, ghostToggleBtn_.x, ghostToggleBtn_.y, ghostToggleBtn_.w, ghostToggleBtn_.h, 4.0f,
                               isGhostActive ? theme.primaryAccent : theme.borderSubtle, 0.9f, 1.0f);
        drawText(r, "GHOST", ghostToggleBtn_.x + 12.0f, ghostToggleBtn_.y + 7.0f, 9.5f,
                 isGhostActive ? theme.primaryAccent : theme.textMuted);

        drawRoundedRect(r, ghostSliderBounds_.x, ghostSliderBounds_.y, ghostSliderBounds_.w, ghostSliderBounds_.h, 4.0f,
                        theme.backgroundDark, 0.8f);
        float fillW = ghostSliderBounds_.w * ghostOpacity_;
        if (fillW > 0.0f) {
            drawRoundedRect(r, ghostSliderBounds_.x, ghostSliderBounds_.y, fillW, ghostSliderBounds_.h, 4.0f,
                            theme.primaryAccent * 0.35f, 0.95f);
        }
        std::string ghostPct = isGhostActive ? (std::to_string(static_cast<int>(ghostOpacity_ * 100.0f)) + "%") : "OFF";
        drawText(r, ghostPct, ghostSliderBounds_.x + 30.0f, ghostSliderBounds_.y + 7.0f, 9.5f,
                 isGhostActive ? theme.primaryAccent : theme.textMuted);
    }

    auto drawSubBtn = [&](const Rect2D& b, const std::string& label, bool active, const Color& accent) {
        if (active) {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, accent * 0.35f, 0.95f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, accent, 1.0f, 1.5f);
            drawLine(r, b.x + 3.0f, b.y + b.h - 2.0f, b.x + b.w - 3.0f, b.y + b.h - 2.0f, accent, 1.0f, 2.0f);
            drawText(r, label, b.x + 8.0f, b.y + 7.0f, 10.0f, accent);
        } else {
            drawRoundedRect(r, b.x, b.y, b.w, b.h, 4.0f, theme.controlBackground, 0.85f);
            drawRoundedRectOutline(r, b.x, b.y, b.w, b.h, 4.0f, theme.borderSubtle, 0.5f, 1.0f);
            drawText(r, label, b.x + 8.0f, b.y + 7.0f, 10.0f, theme.textMuted, 0.85f);
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
    for (int s = 0; s < 64; ++s) {
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

    // 4. Interactive Note Blocks
    for (const auto& n : notes_) {
        float nx = gridBounds_.x + n.startStep * stepWidth_ - scrollX_;
        float nw = n.durationSteps * stepWidth_ - 2.0f;
        float ny = gridBounds_.y + static_cast<float>(maxPitch_ - n.pitch) * semitoneHeight_ - scrollY_ + 1.5f;
        float nh = semitoneHeight_ - 3.0f;

        if (nx + nw < gridBounds_.x || nx > gridBounds_.x + gridBounds_.w) continue;
        if (ny + nh < gridBounds_.y || ny > gridBounds_.y + gridBounds_.h) continue;

        float alpha = n.isSelected ? 1.0f : 0.88f;
        Color baseCol = n.isSlide ? theme.highlight : trackColor_;
        float v = std::clamp(n.velocity, 0.08f, 1.0f);
        // Vary color intensity based on velocity:
        // Lower velocity = dimmer/subtler, higher velocity = bright/vivid/punchy
        float velIntensity = 0.35f + 0.65f * v;
        Color noteCol(std::clamp(baseCol.r * velIntensity, 0.0f, 1.0f),
                      std::clamp(baseCol.g * velIntensity, 0.0f, 1.0f),
                      std::clamp(baseCol.b * velIntensity, 0.0f, 1.0f),
                      baseCol.a);
        drawRoundedRect(r, nx, ny, nw, nh, 3.5f, noteCol, alpha);

        if (n.isSelected) {
            drawRoundedRectOutline(r, nx, ny, nw, nh, 3.5f, Color(1.0f, 1.0f, 1.0f), 1.0f, 2.0f);
        } else {
            drawRoundedRectOutline(r, nx, ny, nw, nh, 3.5f, noteCol * 1.3f, 0.5f + 0.4f * v, 1.0f);
        }

        std::string nName = PianoKeyboard::getNoteName(n.pitch);
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

    // 7. Left Piano Keyboard Gutter
    drawRect(r, pianoGutterBounds_.x, pianoGutterBounds_.y, pianoGutterBounds_.w, pianoGutterBounds_.h, theme.panelHeader);
    drawLine(r, pianoGutterBounds_.x + pianoGutterBounds_.w, pianoGutterBounds_.y,
             pianoGutterBounds_.x + pianoGutterBounds_.w, pianoGutterBounds_.y + pianoGutterBounds_.h,
             theme.borderSubtle, 0.9f, 1.5f);

    for (int p = minPitch_; p <= maxPitch_; ++p) {
        float py = pianoGutterBounds_.y + static_cast<float>(maxPitch_ - p) * semitoneHeight_ - scrollY_;
        if (py + semitoneHeight_ < pianoGutterBounds_.y || py > pianoGutterBounds_.y + pianoGutterBounds_.h) continue;

        bool isBlack = PianoKeyboard::isBlackKey(p);
        bool isAuditioning = (p == auditioningPitch_);
        if (isBlack) {
            Color keyCol = isAuditioning ? theme.highlight : Color(0.12f, 0.12f, 0.14f);
            drawRect(r, pianoGutterBounds_.x, py + 1.0f, pianoGutterBounds_.w * 0.65f, semitoneHeight_ - 2.0f,
                     keyCol, 1.0f);
            drawRoundedRectOutline(r, pianoGutterBounds_.x, py + 1.0f, pianoGutterBounds_.w * 0.65f, semitoneHeight_ - 2.0f,
                                   1.0f, isAuditioning ? theme.highlight : Color(0.25f, 0.25f, 0.28f), 0.8f, 1.0f);
        } else {
            Color keyCol = isAuditioning ? (theme.highlight * 0.85f) : Color(0.88f, 0.88f, 0.86f);
            drawRect(r, pianoGutterBounds_.x, py, pianoGutterBounds_.w, semitoneHeight_,
                     keyCol, 1.0f);
            drawLine(r, pianoGutterBounds_.x, py + semitoneHeight_, pianoGutterBounds_.x + pianoGutterBounds_.w, py + semitoneHeight_,
                     Color(0.35f, 0.35f, 0.35f), 0.5f, 1.0f);

            if (p % 12 == 0) {
                std::string cLabel = "C" + std::to_string(p / 12 - 1);
                drawText(r, cLabel, pianoGutterBounds_.x + pianoGutterBounds_.w - 24.0f, py + semitoneHeight_ - 15.0f, 9.5f,
                         Color(0.15f, 0.15f, 0.15f), 1.0f);
            }
        }
    }

    // 8. Velocity Lane
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

        Color vCol = n.isSelected ? theme.highlight : trackColor_;
        drawLine(r, sx, velocityLaneBounds_.y + velocityLaneBounds_.h - 6.0f, sx, sy, vCol, 0.85f, 2.0f);
        drawCircle(r, sx, sy, 4.0f, vCol);
        if (n.isSelected) {
            drawCircleOutline(r, sx, sy, 4.0f, Color(1.0f, 1.0f, 1.0f), 1.0f, 1.5f);
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

    drawText(r, "TRACKER MATRIX", toolBounds.x + 16.0f, toolBounds.y + 9.0f, 11.0f, theme.primaryAccent);

    std::string octText = "OCTAVE: " + std::to_string(trackerBaseOctave_);
    drawText(r, octText, toolBounds.x + 160.0f, toolBounds.y + 9.0f, 10.5f, theme.highlight);

    if (trackerHasBlockSelection_) {
        drawRoundedRect(r, toolBounds.x + 270.0f, toolBounds.y + 5.0f, 140.0f, 22.0f, 3.0f,
                        theme.primaryAccent * 0.25f, 0.9f);
        drawText(r, "BLOCK SELECTED", toolBounds.x + 280.0f, toolBounds.y + 9.0f, 10.0f, theme.primaryAccent);
    }

    float colHeaderY = toolBounds.y + toolH + 4.0f;
    float rowNumW = 48.0f;
    float colW = 140.0f;

    drawText(r, "STEP", contentBounds_.x + 12.0f, colHeaderY + 4.0f, 9.5f, theme.textMuted, 0.7f);

    for (int c = 0; c < 4; ++c) {
        float cx = contentBounds_.x + rowNumW + static_cast<float>(c) * (colW + 6.0f);
        bool isSelCol = (trackerSelectedCol_ == c);
        drawRoundedRect(r, cx, colHeaderY, colW, 20.0f, 3.0f,
                        isSelCol ? (theme.primaryAccent * 0.25f) : theme.controlBackground, 0.8f);
        std::string colLabel = "CH " + std::to_string(c + 1) + " (NOTE/VOL/SLD/FX)";
        drawText(r, colLabel, cx + 8.0f, colHeaderY + 4.0f, 9.5f,
                 isSelCol ? theme.highlight : theme.textPrimary);
    }

    float startRowY = colHeaderY + 26.0f;
    float rowH = 24.0f;
    int maxRows = 64;

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

        for (int c = 0; c < 4; ++c) {
            float cx = contentBounds_.x + rowNumW + static_cast<float>(c) * (colW + 6.0f);
            bool isCellSelected = (isSelectedRow && trackerSelectedCol_ == c);

            std::string noteStr = "---";
            std::string volStr = "..";
            std::string sldStr = ".";
            std::string fxStr = "00";
            bool hasNote = false;
            bool noteIsSelected = false;

            for (const auto& n : notes_) {
                if (static_cast<int>(std::round(n.startStep)) == step && n.column == c) {
                    noteStr = PianoKeyboard::getNoteName(n.pitch);
                    volStr = "V" + std::to_string(static_cast<int>(n.velocity * 99.0f));
                    sldStr = n.isSlide ? "S" : ".";
                    fxStr = n.effectCommand;
                    hasNote = true;
                    noteIsSelected = n.isSelected;
                    break;
                }
            }

            if (isCellSelected || noteIsSelected) {
                drawRoundedRect(r, cx, ry, colW, rowH - 2.0f, 3.0f,
                                theme.primaryAccent * 0.35f, 0.85f);
                drawRoundedRectOutline(r, cx, ry, colW, rowH - 2.0f, 3.0f,
                                       theme.primaryAccent, 1.0f, 1.5f);
            } else {
                drawRoundedRect(r, cx, ry, colW, rowH - 2.0f, 3.0f,
                                theme.controlBackground, 0.4f);
            }

            std::string cellText = noteStr + " " + volStr + " " + sldStr + " " + fxStr;
            Color cellCol = hasNote ? (noteIsSelected ? theme.highlight : trackColor_) : theme.textMuted;
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
    std::string clefSymbol = isTreble ? "[TREBLE G-CLEF]" : "[BASS F-CLEF]";
    drawText(r, clefSymbol, staffStartX, staffY - 26.0f, 11.5f, Color(0.15f, 0.15f, 0.15f));
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
                drawLine(r, nx - 8.0f, lcy, nx + 12.0f, lcy, Color(0.2f, 0.2f, 0.2f), 0.9f, 1.5f);
            }
            if (n.pitch >= 81) {
                float lay = staffY - lineSpacing;
                drawLine(r, nx - 8.0f, lay, nx + 12.0f, lay, Color(0.2f, 0.2f, 0.2f), 0.9f, 1.5f);
            }
        } else {
            if (n.pitch >= 60) {
                float lcy = staffY - lineSpacing;
                drawLine(r, nx - 8.0f, lcy, nx + 12.0f, lcy, Color(0.2f, 0.2f, 0.2f), 0.9f, 1.5f);
            }
            if (n.pitch <= 40) {
                float ley = staffY + 5.0f * lineSpacing;
                drawLine(r, nx - 8.0f, ley, nx + 12.0f, ley, Color(0.2f, 0.2f, 0.2f), 0.9f, 1.5f);
            }
        }

        Color headCol = n.isSelected ? theme.highlight : Color(0.12f, 0.12f, 0.12f);
        drawCircle(r, nx, ny, 6.0f, headCol);
        if (n.isSelected) {
            drawCircleOutline(r, nx, ny, 7.5f, theme.primaryAccent, 1.0f, 2.0f);
        }

        drawLine(r, nx + 5.0f, ny, nx + 5.0f, ny - 28.0f, headCol, 1.0f, 1.8f);

        std::string tag = PianoKeyboard::getNoteName(n.pitch);
        if (tag.find('#') != std::string::npos) {
            drawText(r, "#", nx - 14.0f, ny - 7.0f, 11.0f, headCol, 0.95f);
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

    float gutterW = 44.0f;
    float textStartY = contentBounds_.y + 46.0f;
    drawRect(r, contentBounds_.x, textStartY, gutterW, contentBounds_.h - 46.0f,
             theme.panelHeader, 0.95f);
    drawLine(r, contentBounds_.x + gutterW, textStartY, contentBounds_.x + gutterW, contentBounds_.y + contentBounds_.h,
             theme.borderSubtle, 0.8f, 1.5f);

    std::istringstream stream(scriptBuffer_);
    std::string line;
    int lineIdx = 1;
    float lineH = 18.0f;

    while (std::getline(stream, line)) {
        float ly = textStartY + static_cast<float>(lineIdx - 1) * lineH + 4.0f;
        if (ly + lineH > contentBounds_.y + contentBounds_.h) break;

        std::string lnStr = std::to_string(lineIdx);
        drawMonoText(r, lnStr, contentBounds_.x + 10.0f, ly, 10.0f, theme.textMuted, 0.7f);

        if (line.rfind("--", 0) == 0) {
            drawMonoText(r, line, contentBounds_.x + gutterW + 12.0f, ly, 10.5f, Color(0.45f, 0.65f, 0.75f), 0.85f);
        } else {
            drawMonoText(r, line, contentBounds_.x + gutterW + 12.0f, ly, 10.5f, theme.textPrimary, 0.95f);
        }
        lineIdx++;
    }

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
    drawRoundedRect(r, closeX, sidebarBounds_.y + 6.0f, 20.0f, 20.0f, 3.0f, theme.controlBackground, 0.8f);
    drawText(r, "X", closeX + 6.0f, sidebarBounds_.y + 10.0f, 10.0f, theme.textMuted);

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
    if (ev.action == PointerAction::Down) {
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
                for (const auto& n : notes_) {
                    if (n.isSelected) { auditionPitch(n.pitch, n.velocity, ctx); break; }
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
            panStartScrollX_ = scrollX_;
            panStartScrollY_ = scrollY_;
            return true;
        }

        // 4. Sub-view specific interactions
        if (subView_ == EditSubViewMode::PianoRoll) {
            if (pianoGutterBounds_.contains(ev.x, ev.y)) {
                float localY = ev.y - pianoGutterBounds_.y + scrollY_;
                int pitch = maxPitch_ - static_cast<int>(localY / semitoneHeight_);
                auditionPitch(pitch, 0.85f, ctx);

                if (ev.button == PointerButton::Right) {
                    for (auto& n : notes_) {
                        if (n.pitch == pitch) n.isSelected = true;
                    }
                }
                return true;
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

                        auditionPitch(n.pitch, n.velocity, ctx);
                        return true;
                    }
                }

                if (ev.button == PointerButton::Left) {
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
                        auditionPitch(n.pitch, n.velocity, ctx);
                        return true;
                    }
                }
            }
        } else if (subView_ == EditSubViewMode::Tracker) {
            float rowNumW = 48.0f;
            float colW = 140.0f;
            float startRowY = contentBounds_.y + 62.0f;
            float rowH = 24.0f;

            if (ev.y >= startRowY) {
                int clickedStep = static_cast<int>((ev.y - startRowY + trackerScrollY_) / rowH);
                float relX = ev.x - (contentBounds_.x + rowNumW);
                int clickedCol = static_cast<int>(relX / (colW + 6.0f));

                if (clickedStep >= 0 && clickedStep < 64 && clickedCol >= 0 && clickedCol < 4) {
                    trackerSelectedStep_ = clickedStep;
                    trackerSelectedCol_ = clickedCol;

                    clearSelection();
                    for (auto& n : notes_) {
                        if (static_cast<int>(std::round(n.startStep)) == clickedStep && n.column == clickedCol) {
                            n.isSelected = true;
                            auditionPitch(n.pitch, n.velocity, ctx);
                            break;
                        }
                    }
                    return true;
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

                // If empty staff area clicked, start marquee drag selection
                if (ev.button == PointerButton::Left) {
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
                return true;
            }
        }
    } else if (ev.action == PointerAction::Move) {
        if (dragMode_ == DragMode::MiddlePan) {
            scrollX_ = std::max(0.0f, panStartScrollX_ - (ev.x - dragStartPoint_.x));
            float maxScrollY = std::max(0.0f, (maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
            scrollY_ = std::clamp(panStartScrollY_ - (ev.y - dragStartPoint_.y), 0.0f, maxScrollY);
            return true;
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
            notes_[activeDragNoteIdx_].durationSteps = std::clamp(activeDragInitialDur_ + durDelta, 0.25f, 16.0f);
            return true;
        }
    } else if (ev.action == PointerAction::Up) {
        if (dragMode_ == DragMode::MarqueeSelect) {
            float dist = std::hypot(ev.x - dragStartPoint_.x, ev.y - dragStartPoint_.y);
            if (dist < 4.0f) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastGridClickTime_).count();
                float clickDist = std::hypot(ev.x - lastGridClickPos_.x, ev.y - lastGridClickPos_.y);

                if (subView_ == EditSubViewMode::PianoRoll && gridBounds_.contains(ev.x, ev.y)) {
                    if (elapsedMs < 350 && clickDist < 14.0f) {
                        // Double-click creates a note
                        float step = std::floor((ev.x - gridBounds_.x + scrollX_) / stepWidth_);
                        int pitch = maxPitch_ - static_cast<int>((ev.y - gridBounds_.y + scrollY_) / semitoneHeight_);
                        if (step >= 0.0f && step < 64.0f && pitch >= minPitch_ && pitch <= maxPitch_) {
                            addNote(static_cast<uint8_t>(pitch), step, 1.0f, 0.85f);
                            if (!notes_.empty()) {
                                notes_.back().isSelected = true;
                            }
                            auditionPitch(pitch, 0.85f, ctx);
                            if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
                        }
                        lastGridClickTime_ = std::chrono::steady_clock::time_point{};
                    } else {
                        // Single click clears selection
                        clearSelection();
                        lastGridClickTime_ = now;
                        lastGridClickPos_ = Point2D{ev.x, ev.y};
                    }
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
                if (ctx.audioEngine) syncToSequencer(ctx.audioEngine->getSequencer());
            }
            return true;
        }

        if (pianoGutterBounds_.contains(ev.x, ev.y)) {
            float localY = ev.y - pianoGutterBounds_.y + scrollY_;
            int pitch = maxPitch_ - static_cast<int>(localY / semitoneHeight_);
            stopAuditionPitch(pitch, ctx);
            return true;
        }
    } else if (ev.action == PointerAction::Scroll) {
        if (subView_ == EditSubViewMode::PianoRoll) {
            scrollX_ = std::max(0.0f, scrollX_ - ev.scrollX * 28.0f);
            float maxScrollY = std::max(0.0f, (maxPitch_ - minPitch_ + 1) * semitoneHeight_ - gridBounds_.h);
            scrollY_ = std::clamp(scrollY_ - ev.scrollY * 22.0f, 0.0f, maxScrollY);
        } else if (subView_ == EditSubViewMode::Tracker) {
            trackerScrollY_ = std::max(0.0f, trackerScrollY_ - ev.scrollY * 24.0f);
        } else if (subView_ == EditSubViewMode::Score) {
            scoreScrollX_ = std::max(0.0f, scoreScrollX_ - ev.scrollX * 34.0f);
        }
        return true;
    }

    return false;
}

bool EditView::handleKey(int key, int scancode, int action, int mods, const ViewContext& ctx) {
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

    // Script shortcut: Ctrl+Enter or F5 to compile
    if (subView_ == EditSubViewMode::Script) {
        if ((isCtrl && (key == 257 || key == 10 || key == 13)) || key == 294) {
            parseNotesFromEatscript();
            if (ctx.onShowNotification) ctx.onShowNotification("Eatscript compiled to clip notes");
            return true;
        }
    }

    if (key == 261 || key == 259) {
        if (subView_ == EditSubViewMode::Tracker) {
            notes_.erase(std::remove_if(notes_.begin(), notes_.end(), [&](const PianoRollNote& n) {
                return (static_cast<int>(std::round(n.startStep)) == trackerSelectedStep_ && n.column == trackerSelectedCol_);
            }), notes_.end());
            formatEatscriptFromNotes();
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
        if (key == 265) { trackerSelectedStep_ = std::max(0, trackerSelectedStep_ - 1); return true; } // Up
        if (key == 264) { trackerSelectedStep_ = std::min(63, trackerSelectedStep_ + 1); return true; } // Down
        if (key == 263) { trackerSelectedCol_ = std::max(0, trackerSelectedCol_ - 1); return true; } // Left
        if (key == 262) { trackerSelectedCol_ = std::min(3, trackerSelectedCol_ + 1); return true; } // Right

        if (key == 91 || key == 45) {
            trackerBaseOctave_ = std::clamp(trackerBaseOctave_ - 1, 1, 6);
            return true;
        }
        if (key == 93 || key == 61) {
            trackerBaseOctave_ = std::clamp(trackerBaseOctave_ + 1, 1, 6);
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
            return true;
        }

        int semitone = -1;
        switch (key) {
            case 90: case 122: semitone = 0; break;
            case 83: case 115: semitone = 1; break;
            case 88: case 120: semitone = 2; break;
            case 68: case 100: semitone = 3; break;
            case 67: case 99:  semitone = 4; break;
            case 86: case 118: semitone = 5; break;
            case 71: case 103: semitone = 6; break;
            case 66: case 98:  semitone = 7; break;
            case 72: case 104: semitone = 8; break;
            case 78: case 110: semitone = 9; break;
            case 74: case 106: semitone = 10; break;
            case 77: case 109: semitone = 11; break;
            case 81: case 113: semitone = 12; break;
            case 50:           semitone = 13; break;
            case 87: case 119: semitone = 14; break;
            case 51:           semitone = 15; break;
            case 69: case 101: semitone = 16; break;
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
            trackerSelectedStep_ = std::min(63, trackerSelectedStep_ + 1);
            formatEatscriptFromNotes();
            return true;
        }
    }

    return false;
}

} // namespace eatsbits::ui
