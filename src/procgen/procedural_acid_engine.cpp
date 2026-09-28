#include "eatsbits/procgen/procedural_acid_engine.hpp"
#include "eatsbits/procgen/mulberry32_rng.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::procgen {

namespace {

struct AcidStep {
    bool active{false};
    int pitch{36};
    bool isSlide{false};
    bool isAccent{false};
    bool isTie{false};
};

int pickPlausiblePitch(Mulberry32Rng& rng, const std::vector<int>& availablePitches, int lastPitch, double octaveJumpProb) {
    if (availablePitches.empty()) return 36;
    if (rng.chance(octaveJumpProb)) {
        int shift = rng.chance(0.5) ? 12 : -12;
        int candidate = lastPitch + shift;
        if (std::find(availablePitches.begin(), availablePitches.end(), candidate) != availablePitches.end()) {
            return candidate;
        }
    }
    // Stepwise motion or root return
    std::vector<int> candidates;
    for (int p : availablePitches) {
        if (std::abs(p - lastPitch) <= 7) {
            candidates.push_back(p);
        }
    }
    if (!candidates.empty() && rng.chance(0.70)) {
        return rng.pick(candidates);
    }
    return rng.pick(availablePitches);
}

std::vector<AcidStep> generateBarTemplate(
    Mulberry32Rng& rng,
    const std::vector<int>& availablePitches,
    int rootPitch,
    const std::vector<int>& scaleIntervals,
    double density,
    double syncopationBias,
    double octaveJumpProb,
    double tieProb,
    double slideProb,
    double accentProb)
{
    (void)scaleIntervals;
    std::vector<AcidStep> bar(16);
    int currentPitch = rootPitch;

    for (int s = 0; s < 16; ++s) {
        bool isDownbeat = (s % 4 == 0);
        bool isOffbeat = (s % 2 != 0);

        double stepDensity = density;
        if (s == 0) {
            stepDensity = std::min(1.0, density + 0.35); // Root downbeat emphasis
        } else if (isOffbeat) {
            stepDensity = std::clamp(density * (0.6 + syncopationBias * 0.6), 0.1, 0.95);
        } else if (!isDownbeat) {
            stepDensity = density * 0.85;
        }

        bool hasNote = rng.chance(stepDensity);
        if (!hasNote) {
            bar[s] = AcidStep{false, currentPitch, false, false, false};
            continue;
        }

        bool isTie = false;
        if (s > 0 && bar[s - 1].active && rng.chance(tieProb)) {
            isTie = true;
        }

        if (!isTie) {
            if (s == 0 && rng.chance(0.80)) {
                currentPitch = rootPitch;
            } else {
                currentPitch = pickPlausiblePitch(rng, availablePitches, currentPitch, octaveJumpProb);
            }
        }

        bool isAccent = false;
        if (!isTie) {
            if (s == 0 && rng.chance(0.75)) {
                isAccent = true;
            } else if (isDownbeat) {
                isAccent = rng.chance(accentProb * 1.2);
            } else {
                isAccent = rng.chance(accentProb);
            }
        }

        bool isSlide = rng.chance(slideProb);

        bar[s] = AcidStep{true, currentPitch, isSlide, isAccent, isTie};
    }
    return bar;
}

} // anonymous namespace

const std::vector<std::string>& ProceduralAcidEngine::getAvailableStyles() {
    static const std::vector<std::string> styles = {
        "Classic Chicago Acid (1987)",
        "90s Acid Trance / Goa",
        "Hard Acid Techno (Warehouse)",
        "Wonky IDM / Brain-Dance",
        "Electro Acid Funk",
        "Minimalist Hypnotic Acid"
    };
    return styles;
}

const std::vector<std::string>& ProceduralAcidEngine::getAvailableScales() {
    static const std::vector<std::string> scales = {
        "Minor Pentatonic",
        "Phrygian (Dark)",
        "Dorian (Groovy)",
        "Natural Minor",
        "Acid Blues (Flatted 5th)",
        "Whole Tone (Trippy)"
    };
    return scales;
}

std::vector<int> ProceduralAcidEngine::getScaleIntervals(const std::string& scaleName) {
    if (scaleName == "Phrygian (Dark)") {
        return {0, 1, 3, 5, 7, 8, 10};
    } else if (scaleName == "Dorian (Groovy)") {
        return {0, 2, 3, 5, 7, 9, 10};
    } else if (scaleName == "Natural Minor") {
        return {0, 2, 3, 5, 7, 8, 10};
    } else if (scaleName == "Acid Blues (Flatted 5th)") {
        return {0, 3, 5, 6, 7, 10};
    } else if (scaleName == "Whole Tone (Trippy)") {
        return {0, 2, 4, 6, 8, 10};
    }
    // Default Minor Pentatonic
    return {0, 3, 5, 7, 10};
}

std::vector<AcidNote> ProceduralAcidEngine::generatePattern(const AcidPatternParams& params) {
    Mulberry32Rng rng(params.seed);
    std::vector<AcidNote> notes;
    const auto scaleIntervals = getScaleIntervals(params.scaleName);

    // Build available MIDI pitches
    std::vector<int> availablePitches;
    int octRange = std::clamp(params.octaveRange, 1, 3);
    for (int oct = 0; oct < octRange; ++oct) {
        for (int interval : scaleIntervals) {
            int pitch = params.baseMidiOctave + (oct * 12) + params.rootPitchClass + interval;
            if (pitch <= 84 && std::find(availablePitches.begin(), availablePitches.end(), pitch) == availablePitches.end()) {
                availablePitches.push_back(pitch);
            }
        }
    }
    std::sort(availablePitches.begin(), availablePitches.end());

    // Style-specific behavioral multipliers
    double syncopationBias = 0.70;
    double octaveJumpProb = 0.50;
    double turnaroundFillProb = 0.60;
    double repetitionTendency = 0.60;
    double styleTieMultiplier = 1.00;

    if (params.style == "90s Acid Trance / Goa") {
        syncopationBias = 0.85; octaveJumpProb = 0.60; turnaroundFillProb = 0.80; repetitionTendency = 0.25; styleTieMultiplier = 0.65;
    } else if (params.style == "Hard Acid Techno (Warehouse)") {
        syncopationBias = 0.60; octaveJumpProb = 0.35; turnaroundFillProb = 0.65; repetitionTendency = 0.70; styleTieMultiplier = 0.80;
    } else if (params.style == "Wonky IDM / Brain-Dance") {
        syncopationBias = 0.90; octaveJumpProb = 0.75; turnaroundFillProb = 0.90; repetitionTendency = 0.15; styleTieMultiplier = 1.25;
    } else if (params.style == "Electro Acid Funk") {
        syncopationBias = 0.80; octaveJumpProb = 0.45; turnaroundFillProb = 0.50; repetitionTendency = 0.55; styleTieMultiplier = 1.15;
    } else if (params.style == "Minimalist Hypnotic Acid") {
        syncopationBias = 0.45; octaveJumpProb = 0.25; turnaroundFillProb = 0.30; repetitionTendency = 0.80; styleTieMultiplier = 1.50;
    }

    int totalBars = std::clamp(params.bars, 1, 8);
    int totalSteps = totalBars * 16;
    double effectiveTieProb = std::clamp(params.tieProb * styleTieMultiplier, 0.0, 0.85);

    auto referenceBar = generateBarTemplate(
        rng, availablePitches, params.baseMidiOctave + params.rootPitchClass,
        scaleIntervals, params.density, syncopationBias, octaveJumpProb,
        effectiveTieProb, params.slideProb, params.accentProb
    );

    std::vector<AcidStep> steps;
    steps.reserve(totalSteps);

    for (int bar = 0; bar < totalBars; ++bar) {
        bool isTurnaroundBar = (bar % 2 == 1);
        for (int stepInBar = 0; stepInBar < 16; ++stepInBar) {
            const auto& refStep = referenceBar[stepInBar];
            if (bar == 0) {
                steps.push_back(refStep);
            } else {
                bool isLast4Steps = (stepInBar >= 12);
                if (isTurnaroundBar && isLast4Steps && rng.chance(turnaroundFillProb)) {
                    bool newNote = rng.chance(0.85);
                    if (newNote) {
                        int pitch = pickPlausiblePitch(rng, availablePitches, steps.empty() ? availablePitches.front() : steps.back().pitch, 0.70);
                        bool isSlideTurn = rng.chance(params.slideProb * 1.3);
                        steps.push_back(AcidStep{true, pitch, isSlideTurn, rng.chance(0.60), false});
                    } else {
                        steps.push_back(AcidStep{false, refStep.pitch, false, false, false});
                    }
                } else if (rng.chance(repetitionTendency)) {
                    steps.push_back(refStep);
                } else {
                    if (!refStep.active) {
                        bool activate = rng.chance(params.density * 0.3);
                        steps.push_back(AcidStep{activate, refStep.pitch, rng.chance(params.slideProb), rng.chance(params.accentProb), false});
                    } else {
                        int pitch = refStep.pitch;
                        if (rng.chance(0.30) && !availablePitches.empty()) {
                            int shift = rng.chance(0.5) ? 12 : -12;
                            if (std::find(availablePitches.begin(), availablePitches.end(), pitch + shift) != availablePitches.end()) {
                                pitch += shift;
                            }
                        }
                        bool isTieStep = refStep.isTie ? rng.chance(0.70) : rng.chance(effectiveTieProb * 0.4);
                        steps.push_back(AcidStep{true, pitch, rng.chance(params.slideProb), refStep.isAccent || rng.chance(params.accentProb * 0.5), isTieStep});
                    }
                }
            }
        }
    }

    // Convert step array to AcidNote objects with authentic TB-303 gate & slide rules
    int i = 0;
    while (i < totalSteps) {
        auto current = steps[i];
        if (!current.active) {
            ++i;
            continue;
        }

        double startStep = static_cast<double>(i);
        int tieCount = 0;
        int j = i + 1;
        while (j < totalSteps && steps[j].active && steps[j].isTie && steps[j].pitch == current.pitch && !steps[j].isSlide) {
            tieCount++;
            j++;
        }

        bool hasFollower = (j < totalSteps) && steps[j].active;
        bool followerIsSlide = hasFollower && (steps[j].isSlide || (current.isSlide && steps[j].pitch != current.pitch));

        double durationSteps;
        if (followerIsSlide) {
            durationSteps = static_cast<double>(1 + tieCount) + 0.05; // 0.05 step overlap for legato glide
            steps[j].isSlide = true;
        } else if (hasFollower && current.isSlide) {
            durationSteps = static_cast<double>(1 + tieCount) + 0.05;
            steps[j].isSlide = true;
        } else {
            durationSteps = std::max(0.60, static_cast<double>(1 + tieCount) - 0.35);
        }

        float velocity = current.isAccent ? 0.95f : 0.68f;

        notes.push_back(AcidNote{
            static_cast<uint8_t>(std::clamp(current.pitch, 0, 127)),
            startStep,
            durationSteps,
            velocity,
            current.isSlide,
            current.isAccent
        });

        i = j;
    }

    return notes;
}

void ProceduralAcidEngine::populateSequencerTrack(sequencer::SequencerTrack& track, const AcidPatternParams& params) {
    track.clear();
    uint32_t numSteps = static_cast<uint32_t>(params.bars * 16);
    track.setNumSteps(numSteps);

    auto notes = generatePattern(params);
    for (const auto& n : notes) {
        uint32_t stepIdx = static_cast<uint32_t>(n.startStep);
        if (stepIdx < numSteps) {
            sequencer::StepData sd;
            sd.active = true;
            sd.note = n.pitch;
            sd.velocity = n.velocity;
            sd.gateLength = static_cast<float>(std::clamp(n.durationSteps, 0.1, 1.0));
            sd.slide = n.isSlide;
            sd.accent = n.isAccent;
            sd.probability = 1.0f;
            track.setStep(stepIdx, sd);
        }
    }
}

void ProceduralAcidEngine::populateArrangerClip(ui::ArrangerTimelineClip& clip, const AcidPatternParams& params) {
    clip.notes.clear();
    clip.lengthBars = static_cast<uint32_t>(params.bars);

    auto notes = generatePattern(params);
    for (const auto& n : notes) {
        ui::ArrangerClipNote cn;
        cn.pitch = n.pitch;
        cn.startBeat = static_cast<float>(n.startStep * 0.25); // 16th steps -> beats (4 steps = 1 beat)
        cn.lengthBeats = static_cast<float>(n.durationSteps * 0.25);
        cn.velocity = n.velocity;
        clip.notes.push_back(cn);
    }
}

} // namespace eatsbits::procgen
