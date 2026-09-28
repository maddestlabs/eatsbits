#include "eatsbits/procgen/procedural_piano_engine.hpp"
#include "eatsbits/procgen/mulberry32_rng.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::procgen {

const char* getPianoStyleName(PianoStyle style) noexcept {
    switch (style) {
        case PianoStyle::NeoSoul: return "Neo-Soul";
        case PianoStyle::JazzBallad: return "Jazz Ballad";
        case PianoStyle::PopStrum: return "Pop Strum";
        case PianoStyle::Classical: return "Classical";
        case PianoStyle::Arpeggiated: return "Arpeggiated";
    }
    return "Neo-Soul";
}

PianoStyle parsePianoStyle(const std::string& str) noexcept {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("jazz") != std::string::npos) return PianoStyle::JazzBallad;
    if (s.find("pop") != std::string::npos || s.find("strum") != std::string::npos) return PianoStyle::PopStrum;
    if (s.find("classic") != std::string::npos) return PianoStyle::Classical;
    if (s.find("arp") != std::string::npos) return PianoStyle::Arpeggiated;
    return PianoStyle::NeoSoul;
}

const std::vector<std::string>& ProceduralPianoEngine::getAvailableStyles() {
    static const std::vector<std::string> styles = {
        "Neo-Soul",
        "Jazz Ballad",
        "Pop Strum",
        "Classical",
        "Arpeggiated"
    };
    return styles;
}

std::vector<int> ProceduralPianoEngine::computeVoiceLeading(
    const std::vector<int>& pitchClasses,
    const std::vector<int>& previousVoicing,
    int minMidi,
    int maxMidi)
{
    if (pitchClasses.empty()) return {60, 64, 67};

    // If no previous voicing, place notes in mid register around C4 (60)
    if (previousVoicing.empty()) {
        std::vector<int> voicing;
        for (size_t i = 0; i < pitchClasses.size(); ++i) {
            int pc = pitchClasses[i];
            int octave = (i == 0) ? 60 : (i <= 2 ? 60 : 72);
            int pitch = octave + pc;
            while (pitch < minMidi) pitch += 12;
            while (pitch > maxMidi) pitch -= 12;
            if (std::find(voicing.begin(), voicing.end(), pitch) == voicing.end()) {
                voicing.push_back(pitch);
            }
        }
        std::sort(voicing.begin(), voicing.end());
        if (voicing.empty()) voicing.push_back(60);
        return voicing;
    }

    // Voice leading optimization: For each pitch class, choose the octave that is closest to previous voices
    std::vector<int> voicing;
    for (size_t i = 0; i < previousVoicing.size(); ++i) {
        int targetPc = pitchClasses[i % pitchClasses.size()];
        int prevPitch = previousVoicing[i];

        // Find octave of targetPc closest to prevPitch
        int bestPitch = 60 + targetPc;
        int bestDist = 9999;
        for (int oct = 36; oct <= 96; oct += 12) {
            int cand = oct + targetPc;
            if (cand < minMidi || cand > maxMidi) continue;
            int dist = std::abs(cand - prevPitch);
            if (dist < bestDist) {
                bestDist = dist;
                bestPitch = cand;
            }
        }
        voicing.push_back(bestPitch);
    }

    std::sort(voicing.begin(), voicing.end());
    // Deduplicate
    voicing.erase(std::unique(voicing.begin(), voicing.end()), voicing.end());
    return voicing;
}

std::vector<PianoNote> ProceduralPianoEngine::generatePattern(const PianoPatternParams& params) {
    Mulberry32Rng rng(params.seed);
    std::vector<PianoNote> notes;

    auto chords = params.chords;
    if (chords.empty()) {
        // Fallback default 4-bar progression: I - vi - IV - V in C
        chords.push_back(theory::ChordEvent{"c1", 0, 1.0f, 0, theory::ChordQuality::Major});
        chords.push_back(theory::ChordEvent{"c2", 1, 1.0f, 9, theory::ChordQuality::Minor});
        chords.push_back(theory::ChordEvent{"c3", 2, 1.0f, 5, theory::ChordQuality::Major});
        chords.push_back(theory::ChordEvent{"c4", 3, 1.0f, 7, theory::ChordQuality::Dominant7});
    }

    std::vector<int> currentVoicing;
    int totalBars = std::clamp(params.bars, 1, 32);

    for (int bar = 0; bar < totalBars; ++bar) {
        // Find chord for this bar
        const theory::ChordEvent* chord = nullptr;
        for (const auto& ch : chords) {
            if (static_cast<uint32_t>(bar) >= ch.startBar &&
                static_cast<uint32_t>(bar) < ch.startBar + static_cast<uint32_t>(std::ceil(ch.barLength))) {
                chord = &ch;
                break;
            }
        }
        if (!chord) {
            chord = &chords[static_cast<size_t>(bar) % chords.size()];
        }

        double barStartBeat = static_cast<double>(bar) * 4.0;
        int root = (chord->bassPitchClass >= 0) ? chord->bassPitchClass : chord->rootPitchClass;
        auto pcs = chord->getPitchClasses();

        // 1. Left Hand: Deep Bass Foundation
        int bassPitch1 = 36 + root; // C2 (36) + root
        while (bassPitch1 > 48) bassPitch1 -= 12;
        while (bassPitch1 < 28) bassPitch1 += 12;

        if (params.addBassOctaves) {
            notes.push_back(PianoNote{static_cast<uint8_t>(bassPitch1), barStartBeat, 3.8, 0.85f, true, false});
            notes.push_back(PianoNote{static_cast<uint8_t>(bassPitch1 + 12), barStartBeat, 3.8, 0.72f, true, false});
        } else {
            notes.push_back(PianoNote{static_cast<uint8_t>(bassPitch1), barStartBeat, 3.8, 0.85f, true, false});
        }

        // 2. Right Hand: Smooth Voice Leading
        currentVoicing = computeVoiceLeading(pcs, currentVoicing, 55, 84);

        // 3. Stylistic Rhythms & Ornamentation
        switch (params.style) {
            case PianoStyle::NeoSoul: {
                // Strummed chord hit on beat 1 with rubato
                double rubato = rng.randDouble(-0.02, 0.02) * params.humanize;
                for (size_t vi = 0; vi < currentVoicing.size(); ++vi) {
                    double strumOffset = static_cast<double>(vi) * params.strumSpreadBeats;
                    float vel = std::clamp(0.72f - static_cast<float>(vi) * 0.03f + static_cast<float>(rng.randDouble(-0.04, 0.04) * params.humanize), 0.2f, 1.0f);
                    notes.push_back(PianoNote{
                        static_cast<uint8_t>(currentVoicing[vi]),
                        barStartBeat + strumOffset + rubato,
                        3.6,
                        vel,
                        false,
                        false
                    });
                }

                // Syncopated push on beat 3.5 (beat + 2.5) with 70% chance
                if (rng.chance(0.70)) {
                    double pushBeat = barStartBeat + 2.5;
                    for (size_t vi = 0; vi < currentVoicing.size(); ++vi) {
                        double strumOffset = static_cast<double>(vi) * (params.strumSpreadBeats * 0.7);
                        notes.push_back(PianoNote{
                            static_cast<uint8_t>(currentVoicing[vi]),
                            pushBeat + strumOffset,
                            1.4,
                            0.68f,
                            false,
                            false
                        });
                    }
                }
                break;
            }

            case PianoStyle::PopStrum: {
                // Driving quarter notes: beats 0, 1, 2, 3
                for (int beat = 0; beat < 4; ++beat) {
                    float beatVel = (beat % 2 == 0) ? 0.82f : 0.68f;
                    for (size_t vi = 0; vi < currentVoicing.size(); ++vi) {
                        notes.push_back(PianoNote{
                            static_cast<uint8_t>(currentVoicing[vi]),
                            barStartBeat + beat,
                            0.85,
                            beatVel,
                            false,
                            false
                        });
                    }
                }
                break;
            }

            case PianoStyle::Classical: {
                // Sustained right hand chord on beat 0
                for (size_t vi = 0; vi < currentVoicing.size(); ++vi) {
                    notes.push_back(PianoNote{
                        static_cast<uint8_t>(currentVoicing[vi]),
                        barStartBeat,
                        3.8,
                        0.75f,
                        false,
                        false
                    });
                }

                // Left hand Alberti bass on 8th notes (0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5)
                int fifth = bassPitch1 + 7;
                int third = bassPitch1 + (pcs.size() > 1 ? pcs[1] : 4);
                std::vector<int> alberti = {bassPitch1, fifth, third, fifth};
                for (int step = 0; step < 8; ++step) {
                    int p = alberti[static_cast<size_t>(step) % alberti.size()];
                    notes.push_back(PianoNote{
                        static_cast<uint8_t>(p),
                        barStartBeat + (step * 0.5),
                        0.45,
                        0.65f,
                        true,
                        true
                    });
                }
                break;
            }

            case PianoStyle::Arpeggiated: {
                // Cascading 16th-note arpeggio across 4 beats (16 steps)
                for (int step = 0; step < 16; ++step) {
                    size_t pIdx = static_cast<size_t>(step) % currentVoicing.size();
                    int pitch = currentVoicing[pIdx];
                    if (step >= 8) pitch += 12; // Octave lift in second half of bar
                    pitch = std::clamp(pitch, 48, 96);
                    float vel = (step % 4 == 0) ? 0.85f : 0.65f;
                    notes.push_back(PianoNote{
                        static_cast<uint8_t>(pitch),
                        barStartBeat + (step * 0.25),
                        0.35,
                        vel,
                        false,
                        true
                    });
                }
                break;
            }

            case PianoStyle::JazzBallad:
            default: {
                // Lush sustained chords with gentle comping
                for (size_t vi = 0; vi < currentVoicing.size(); ++vi) {
                    double strumOffset = static_cast<double>(vi) * (params.strumSpreadBeats * 1.5);
                    notes.push_back(PianoNote{
                        static_cast<uint8_t>(currentVoicing[vi]),
                        barStartBeat + strumOffset,
                        3.7,
                        0.70f,
                        false,
                        false
                    });
                }
                break;
            }
        }
    }

    return notes;
}

void ProceduralPianoEngine::populateSequencerTrack(sequencer::SequencerTrack& track, const PianoPatternParams& params) {
    track.clear();
    uint32_t numSteps = static_cast<uint32_t>(params.bars * 16);
    track.setNumSteps(numSteps);

    auto notes = generatePattern(params);
    for (const auto& n : notes) {
        // startBeat to 16th step: 1 beat = 4 steps
        uint32_t stepIdx = static_cast<uint32_t>(n.startBeat * 4.0);
        if (stepIdx < numSteps) {
            auto& sd = track.getStep(stepIdx);
            if (!sd.active) {
                sd.active = true;
                sd.note = n.pitch;
                sd.velocity = n.velocity;
                sd.gateLength = static_cast<float>(std::clamp(n.durationBeats * 0.25, 0.1, 1.0));
                sd.probability = 1.0f;
            } else {
                if (sd.note != n.pitch && std::find(sd.extraNotes.begin(), sd.extraNotes.end(), n.pitch) == sd.extraNotes.end()) {
                    sd.extraNotes.push_back(n.pitch);
                }
            }
        }
    }
}

void ProceduralPianoEngine::populateArrangerClip(ui::ArrangerTimelineClip& clip, const PianoPatternParams& params) {
    clip.notes.clear();
    clip.lengthBars = static_cast<uint32_t>(params.bars);

    auto notes = generatePattern(params);
    for (const auto& n : notes) {
        ui::ArrangerClipNote cn;
        cn.pitch = n.pitch;
        cn.startBeat = static_cast<float>(n.startBeat);
        cn.lengthBeats = static_cast<float>(n.durationBeats);
        cn.velocity = n.velocity;
        clip.notes.push_back(cn);
    }
}

} // namespace eatsbits::procgen
