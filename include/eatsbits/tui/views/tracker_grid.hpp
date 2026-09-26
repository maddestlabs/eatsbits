#pragma once

#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/audio_telemetry.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"

namespace eatsbits::tui {

class TrackerGridView {
public:
    TrackerGridView();
    ~TrackerGridView() = default;

    void render(CellSurface& surface, const Rect& area, sequencer::StepSequencer& seq, const TelemetrySnapshot& telemetry, bool isFocused);

    void moveCursor(int deltaTrack, int deltaStep, sequencer::StepSequencer& seq);
    void toggleStep(sequencer::StepSequencer& seq);
    void changePitch(int deltaSemitones, sequencer::StepSequencer& seq);
    void toggleAccent(sequencer::StepSequencer& seq);
    void toggleSlide(sequencer::StepSequencer& seq);

    size_t getSelectedTrack() const { return cursorTrack_; }
    uint32_t getSelectedStep() const { return cursorStep_; }

private:
    size_t cursorTrack_{0};
    uint32_t cursorStep_{0};
    uint32_t stepOffset_{0};

    static std::string formatMidiNote(uint8_t note);
};

} // namespace eatsbits::tui
