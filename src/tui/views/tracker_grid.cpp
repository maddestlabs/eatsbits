#include "eatsbits/tui/views/tracker_grid.hpp"
#include <iomanip>
#include <sstream>

namespace eatsbits::tui {

TrackerGridView::TrackerGridView() = default;

std::string TrackerGridView::formatMidiNote(uint8_t note) {
    static const char* kNoteNames[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };
    int octave = static_cast<int>(note) / 12 - 1;
    int noteIdx = static_cast<int>(note) % 12;
    std::string s = kNoteNames[noteIdx];
    s += std::to_string(octave);
    return s;
}

void TrackerGridView::render(CellSurface& surface, const Rect& area, sequencer::StepSequencer& seq, const TelemetrySnapshot& telemetry, bool isFocused) {
    if (area.width < 30 || area.height < 6) return;

    Color frameCol = isFocused ? Color::AcidAmber() : Color::Gray();
    std::string title = isFocused ? "STEP TRACKER MATRIX [ACTIVE]" : "STEP TRACKER MATRIX";
    surface.drawBox(area, frameCol, Color::Black(), isFocused, title);

    size_t numTracks = seq.getNumTracks();
    if (numTracks == 0) {
        surface.drawText(area.x + 2, area.y + 2, "No tracks active. Press [N] to add track.", Color::Gray(), Color::Black());
        return;
    }

    // Clamp cursor
    if (cursorTrack_ >= numTracks) cursorTrack_ = numTracks - 1;
    auto* curTrack = seq.getTrack(cursorTrack_);
    uint32_t trackSteps = curTrack ? curTrack->getNumSteps() : 16;
    if (cursorStep_ >= trackSteps) cursorStep_ = trackSteps - 1;

    // Viewport scrolling
    int visibleRows = area.height - 3;
    if (visibleRows <= 0) return;

    if (cursorStep_ < stepOffset_) {
        stepOffset_ = cursorStep_;
    } else if (cursorStep_ >= stepOffset_ + static_cast<uint32_t>(visibleRows)) {
        stepOffset_ = cursorStep_ - static_cast<uint32_t>(visibleRows) + 1;
    }

    // Draw Column Headers
    int curX = area.x + 1;
    int headerY = area.y + 1;
    surface.drawText(curX, headerY, "STP", Color::LightGray(), Color::DarkGray(), static_cast<uint8_t>(TextAttr::Bold));
    curX += 4;

    const int colWidth = 14;
    for (size_t t = 0; t < numTracks; ++t) {
        if (curX + colWidth > area.x + area.width - 1) break;
        auto* track = seq.getTrack(t);
        std::string name = track ? track->getName() : "Track";
        if (name.size() > 8) name = name.substr(0, 8);

        Color headerFg = (t == cursorTrack_) ? Color::AcidAmber() : Color::White();
        std::string hdr = "│ " + name;
        while (hdr.size() < static_cast<size_t>(colWidth)) hdr.push_back(' ');
        surface.drawText(curX, headerY, hdr, headerFg, Color::DarkGray(), static_cast<uint8_t>(TextAttr::Bold));
        curX += colWidth;
    }

    // Draw Rows
    for (int r = 0; r < visibleRows; ++r) {
        uint32_t step = stepOffset_ + static_cast<uint32_t>(r);
        int rowY = area.y + 2 + r;
        if (rowY >= area.y + area.height - 1) break;

        bool isPlayhead = telemetry.isPlaying && (telemetry.currentStep == step);
        bool isBeat = (step % 4 == 0);

        Color rowBg = isPlayhead ? Color::FromHex(0x3A2E10) : (isBeat ? Color::FromHex(0x18191E) : Color::Black());

        // Step number
        char stepBuf[8];
        snprintf(stepBuf, sizeof(stepBuf), "%02d ", step + 1);
        Color stepNumCol = isPlayhead ? Color::AcidYellow() : (isBeat ? Color::White() : Color::Gray());
        surface.drawText(area.x + 1, rowY, stepBuf, stepNumCol, rowBg, isBeat ? static_cast<uint8_t>(TextAttr::Bold) : 0);

        int cellX = area.x + 5;
        for (size_t t = 0; t < numTracks; ++t) {
            if (cellX + colWidth > area.x + area.width - 1) break;
            auto* track = seq.getTrack(t);
            if (!track || step >= track->getNumSteps()) {
                cellX += colWidth;
                continue;
            }

            const auto& st = track->getStep(step);
            bool isCursor = isFocused && (t == cursorTrack_) && (step == cursorStep_);

            Color cellBg = isCursor ? Color::AcidAmber() : rowBg;
            Color cellFg = isCursor ? Color::Black() : (st.active ? Color::White() : Color::Gray());

            std::string stepStr;
            if (st.active) {
                stepStr = formatMidiNote(st.note);
                stepStr += st.accent ? "!" : " ";
                stepStr += st.slide ? "/" : " ";
                int velBars = std::clamp(static_cast<int>(st.velocity * 4.0f), 1, 4);
                static const char* velGlyphs[5] = {"    ", "▪   ", "▪▪  ", "▪▪▪ ", "▪▪▪▪"};
                stepStr += velGlyphs[velBars];
            } else {
                stepStr = " ···   ··· ";
            }

            while (stepStr.size() < static_cast<size_t>(colWidth - 1)) stepStr.push_back(' ');

            surface.drawText(cellX, rowY, "│", Color::DarkGray(), rowBg);
            surface.drawText(cellX + 1, rowY, stepStr, cellFg, cellBg, isCursor ? static_cast<uint8_t>(TextAttr::Bold) : 0);

            cellX += colWidth;
        }
    }
}

void TrackerGridView::moveCursor(int deltaTrack, int deltaStep, sequencer::StepSequencer& seq) {
    size_t numTracks = seq.getNumTracks();
    if (numTracks == 0) return;

    int newTrack = static_cast<int>(cursorTrack_) + deltaTrack;
    if (newTrack < 0) newTrack = 0;
    if (newTrack >= static_cast<int>(numTracks)) newTrack = static_cast<int>(numTracks) - 1;
    cursorTrack_ = static_cast<size_t>(newTrack);

    auto* track = seq.getTrack(cursorTrack_);
    uint32_t numSteps = track ? track->getNumSteps() : 16;
    int newStep = static_cast<int>(cursorStep_) + deltaStep;
    if (newStep < 0) newStep = 0;
    if (newStep >= static_cast<int>(numSteps)) newStep = static_cast<int>(numSteps) - 1;
    cursorStep_ = static_cast<uint32_t>(newStep);
}

void TrackerGridView::toggleStep(sequencer::StepSequencer& seq) {
    auto* track = seq.getTrack(cursorTrack_);
    if (!track) return;
    auto step = track->getStep(cursorStep_);
    step.active = !step.active;
    if (step.active && step.note == 0) {
        step.note = 36; // C2 default
        step.velocity = 0.8f;
    }
    track->setStep(cursorStep_, step);
}

void TrackerGridView::changePitch(int deltaSemitones, sequencer::StepSequencer& seq) {
    auto* track = seq.getTrack(cursorTrack_);
    if (!track) return;
    auto step = track->getStep(cursorStep_);
    int newNote = static_cast<int>(step.note) + deltaSemitones;
    step.note = static_cast<uint8_t>(std::clamp(newNote, 0, 127));
    track->setStep(cursorStep_, step);
}

void TrackerGridView::toggleAccent(sequencer::StepSequencer& seq) {
    auto* track = seq.getTrack(cursorTrack_);
    if (!track) return;
    auto step = track->getStep(cursorStep_);
    step.accent = !step.accent;
    track->setStep(cursorStep_, step);
}

void TrackerGridView::toggleSlide(sequencer::StepSequencer& seq) {
    auto* track = seq.getTrack(cursorTrack_);
    if (!track) return;
    auto step = track->getStep(cursorStep_);
    step.slide = !step.slide;
    track->setStep(cursorStep_, step);
}

} // namespace eatsbits::tui
