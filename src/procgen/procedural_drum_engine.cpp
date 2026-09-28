#include "eatsbits/procgen/procedural_drum_engine.hpp"
#include "eatsbits/procgen/mulberry32_rng.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::procgen {

namespace {

float humanizeVel(float baseVel, double humanize, Mulberry32Rng& rng) {
    float jitter = static_cast<float>(rng.randDouble(-0.12, 0.12) * humanize);
    return std::clamp(baseVel + jitter, 0.20f, 1.0f);
}

void addNote(std::vector<DrumNote>& notes, uint8_t pitch, double startStep, double durationSteps, float velocity) {
    notes.push_back(DrumNote{pitch, startStep, durationSteps, velocity});
}

void generateFillStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, Mulberry32Rng& rng) {
    // Tom / Snare cascade fill during steps 12, 13, 14, 15
    switch (stepInBar) {
        case 12:
            addNote(notes, 38, noteStart, 1.0, 0.85f); // Snare
            break;
        case 13:
            addNote(notes, 50, noteStart, 1.0, 0.88f); // High Tom
            break;
        case 14:
            addNote(notes, 47, noteStart, 1.0, 0.90f); // Mid Tom
            break;
        case 15:
            addNote(notes, 43, noteStart, 1.0, 0.95f); // Low Floor Tom
            if (rng.chance(0.5)) {
                addNote(notes, 38, noteStart + 0.5, 0.5, 0.80f); // Quick snare flam
            }
            break;
    }
}

void generateFourOnFloorStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    (void)density;
    // Four on the Floor Kick: 0, 4, 8, 12
    if (stepInBar % 4 == 0) {
        addNote(notes, 36, noteStart, 1.0, humanizeVel(0.95f, humanize, rng)); // Kick 1
    }

    // Snare / Clap on Backbeat: 4, 12
    if (stepInBar == 4 || stepInBar == 12) {
        addNote(notes, 38, noteStart, 1.0, humanizeVel(0.92f, humanize, rng)); // Snare
        if (rng.chance(0.85)) {
            addNote(notes, 39, noteStart, 1.0, humanizeVel(0.85f, humanize, rng)); // Hand Clap
        }
    } else if (rng.chance(ghostProb * 0.4)) {
        addNote(notes, 38, noteStart, 0.5, humanizeVel(0.40f, humanize, rng)); // Ghost snare
    }

    // Offbeat Open Hi-Hat: 2, 6, 10, 14
    if (stepInBar % 4 == 2) {
        addNote(notes, 46, noteStart, 1.0, humanizeVel(0.88f, humanize, rng)); // Open Hat
    } else {
        // Closed Hat on 16ths
        addNote(notes, 42, noteStart, 0.8, humanizeVel(0.70f, humanize, rng)); // Closed Hat
    }
}

void generateBoomBapStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    // Kick: 0, and syncopated beat 10 or 11
    if (stepInBar == 0 || (stepInBar == 10 && rng.chance(density)) || (stepInBar == 6 && rng.chance(density * 0.6))) {
        addNote(notes, 36, noteStart, 1.2, humanizeVel(0.92f, humanize, rng));
    }

    // Snare: 4, 12
    if (stepInBar == 4 || stepInBar == 12) {
        addNote(notes, 38, noteStart, 1.0, humanizeVel(0.95f, humanize, rng));
    } else if (stepInBar == 14 && rng.chance(ghostProb)) {
        addNote(notes, 38, noteStart, 0.6, humanizeVel(0.45f, humanize, rng)); // Ghost snare
    }

    // Swung Hi-Hat
    if (stepInBar % 2 == 0) {
        addNote(notes, 42, noteStart, 0.9, humanizeVel(0.80f, humanize, rng));
    } else if (rng.chance(0.75)) {
        addNote(notes, 42, noteStart, 0.6, humanizeVel(0.60f, humanize, rng));
    }
}

void generateFunkStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    // Syncopated Kick: 0, 6, 10
    if (stepInBar == 0 || stepInBar == 6 || (stepInBar == 10 && rng.chance(density))) {
        addNote(notes, 36, noteStart, 1.0, humanizeVel(0.90f, humanize, rng));
    }

    // Backbeat Snare: 4, 12
    if (stepInBar == 4 || stepInBar == 12) {
        addNote(notes, 38, noteStart, 1.0, humanizeVel(0.95f, humanize, rng));
    } else if (stepInBar == 7 || stepInBar == 11 || stepInBar == 15) {
        if (rng.chance(ghostProb)) {
            addNote(notes, 38, noteStart, 0.6, humanizeVel(0.42f, humanize, rng)); // Ghost snare
        }
    }

    // 16th Hi-Hats with accent on downbeats
    float hatVel = (stepInBar % 4 == 0) ? 0.85f : 0.65f;
    if (stepInBar == 14 && rng.chance(0.7)) {
        addNote(notes, 46, noteStart, 1.0, humanizeVel(0.85f, humanize, rng)); // Open Hat
    } else {
        addNote(notes, 42, noteStart, 0.8, humanizeVel(hatVel, humanize, rng));
    }
}

void generateTrapStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    (void)ghostProb;
    // Rolling 808 Kick: 0, 7, 10, 13
    if (stepInBar == 0 || stepInBar == 7 || (stepInBar == 10 && rng.chance(density))) {
        addNote(notes, 36, noteStart, 1.5, humanizeVel(0.95f, humanize, rng));
    }

    // Halftime Snare/Clap on step 8
    if (stepInBar == 8) {
        addNote(notes, 38, noteStart, 1.0, humanizeVel(0.95f, humanize, rng));
        addNote(notes, 39, noteStart, 1.0, humanizeVel(0.90f, humanize, rng));
    }

    // Fast Hi-Hat Rolls
    if (stepInBar == 14 || stepInBar == 15) {
        // 32nd note triplet roll
        addNote(notes, 42, noteStart, 0.4, humanizeVel(0.70f, humanize, rng));
        addNote(notes, 42, noteStart + 0.5, 0.4, humanizeVel(0.75f, humanize, rng));
    } else {
        addNote(notes, 42, noteStart, 0.8, humanizeVel(0.80f, humanize, rng));
    }
}

void generateRockStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    (void)density; (void)ghostProb;
    // Kick: 0, 8, and occasionally 10
    if (stepInBar == 0 || stepInBar == 8 || (stepInBar == 10 && rng.chance(0.4))) {
        addNote(notes, 36, noteStart, 1.0, humanizeVel(0.95f, humanize, rng));
    }

    // Snare: 4, 12
    if (stepInBar == 4 || stepInBar == 12) {
        addNote(notes, 38, noteStart, 1.0, humanizeVel(0.98f, humanize, rng));
    }

    // 8th-note driving hats
    if (stepInBar % 2 == 0) {
        addNote(notes, 42, noteStart, 0.9, humanizeVel(0.82f, humanize, rng));
    }
}

void generateJazzStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    (void)density; (void)ghostProb;
    // Feathered kick on all 4 quarter beats at very low velocity
    if (stepInBar % 4 == 0) {
        addNote(notes, 36, noteStart, 1.0, humanizeVel(0.35f, humanize, rng));
    }

    // Hi-Hat foot pedal on 2 and 4 (steps 4 and 12)
    if (stepInBar == 4 || stepInBar == 12) {
        addNote(notes, 44, noteStart, 1.0, humanizeVel(0.75f, humanize, rng)); // Pedal Hi-Hat
    }

    // Ride Cymbal 1 (51) "spang-a-lang" rhythm: 0, 4, 6, 8, 12, 14
    if (stepInBar == 0 || stepInBar == 4 || stepInBar == 6 || stepInBar == 8 || stepInBar == 12 || stepInBar == 14) {
        float rVel = (stepInBar == 6 || stepInBar == 14) ? 0.65f : 0.85f;
        addNote(notes, 51, noteStart, 1.2, humanizeVel(rVel, humanize, rng));
    }
}

void generateConsoleStep(std::vector<DrumNote>& notes, int stepInBar, double noteStart, double density, double ghostProb, double humanize, Mulberry32Rng& rng) {
    (void)ghostProb;
    // Fast action console groove
    if (stepInBar == 0 || stepInBar == 6 || stepInBar == 8 || (stepInBar == 12 && rng.chance(density))) {
        addNote(notes, 36, noteStart, 1.0, humanizeVel(0.92f, humanize, rng));
    }
    if (stepInBar == 4 || stepInBar == 12) {
        addNote(notes, 38, noteStart, 1.0, humanizeVel(0.95f, humanize, rng));
    }
    if (stepInBar % 2 == 0) {
        addNote(notes, 42, noteStart, 0.8, humanizeVel(0.78f, humanize, rng));
    }
}

} // anonymous namespace

const std::vector<std::string>& ProceduralDrumEngine::getAvailableStyles() {
    static const std::vector<std::string> styles = {
        "House / Disco (4-on-Floor)",
        "Hip-Hop / Boom-Bap",
        "Funk / Breakbeat",
        "Trap / Halftime",
        "Rock / Pop",
        "Jazz / Swing",
        "16-Bit Console Action",
        "8-Bit Chiptune / C64 SID"
    };
    return styles;
}

std::vector<DrumNote> ProceduralDrumEngine::generatePattern(const DrumPatternParams& params) {
    Mulberry32Rng rng(params.seed);
    std::vector<DrumNote> notes;
    int totalBars = std::clamp(params.bars, 1, 16);
    int totalSteps = totalBars * 16;

    for (int bar = 0; bar < totalBars; ++bar) {
        bool isFillBar = (bar % 4 == 3) && rng.chance(params.fillDensity);
        int barStartStep = bar * 16;

        for (int stepInBar = 0; stepInBar < 16; ++stepInBar) {
            int step = barStartStep + stepInBar;
            bool isLast4Steps = (stepInBar >= 12);

            // Swing micro-timing on odd 16th steps
            double timeOffset = 0.0;
            if (stepInBar % 2 != 0) {
                timeOffset += params.swing * 0.33;
            }
            timeOffset += (rng.randDouble(-0.06, 0.06) * params.humanize);
            double noteStart = std::clamp(static_cast<double>(step) + timeOffset, 0.0, static_cast<double>(totalSteps));

            if (isFillBar && isLast4Steps) {
                generateFillStep(notes, stepInBar, noteStart, rng);
                continue;
            }

            if (params.style == "House / Disco (4-on-Floor)") {
                generateFourOnFloorStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            } else if (params.style == "Hip-Hop / Boom-Bap") {
                generateBoomBapStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            } else if (params.style == "Funk / Breakbeat") {
                generateFunkStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            } else if (params.style == "Trap / Halftime") {
                generateTrapStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            } else if (params.style == "Rock / Pop") {
                generateRockStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            } else if (params.style == "Jazz / Swing") {
                generateJazzStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            } else {
                generateConsoleStep(notes, stepInBar, noteStart, params.density, params.ghostProb, params.humanize, rng);
            }

            // First beat of first bar: Crash cymbal accent
            if (step == 0) {
                addNote(notes, 49, 0.0, 4.0, 0.95f); // Crash Cymbal 1
            }
        }
    }

    return notes;
}

void ProceduralDrumEngine::populateSequencerTrack(sequencer::SequencerTrack& track, const DrumPatternParams& params) {
    track.clear();
    uint32_t numSteps = static_cast<uint32_t>(params.bars * 16);
    track.setNumSteps(numSteps);

    auto notes = generatePattern(params);
    for (const auto& n : notes) {
        uint32_t stepIdx = static_cast<uint32_t>(n.startStep);
        if (stepIdx < numSteps) {
            auto& sd = track.getStep(stepIdx);
            if (!sd.active) {
                sd.active = true;
                sd.note = n.pitch;
                sd.velocity = n.velocity;
                sd.gateLength = static_cast<float>(std::clamp(n.durationSteps, 0.1, 1.0));
                sd.probability = 1.0f;
            } else {
                // Polyphonic drum layering (e.g. Kick + Hat or Snare + Clap)
                if (sd.note != n.pitch && std::find(sd.extraNotes.begin(), sd.extraNotes.end(), n.pitch) == sd.extraNotes.end()) {
                    sd.extraNotes.push_back(n.pitch);
                }
            }
        }
    }
}

void ProceduralDrumEngine::populateArrangerClip(ui::ArrangerTimelineClip& clip, const DrumPatternParams& params) {
    clip.notes.clear();
    clip.lengthBars = static_cast<uint32_t>(params.bars);

    auto notes = generatePattern(params);
    for (const auto& n : notes) {
        ui::ArrangerClipNote cn;
        cn.pitch = n.pitch;
        cn.startBeat = static_cast<float>(n.startStep * 0.25);
        cn.lengthBeats = static_cast<float>(n.durationSteps * 0.25);
        cn.velocity = n.velocity;
        clip.notes.push_back(cn);
    }
}

} // namespace eatsbits::procgen
