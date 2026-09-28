#include "eatsbits/procgen/procedural_ensemble_engine.hpp"
#include "eatsbits/procgen/mulberry32_rng.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::procgen {

namespace {

theory::ChordEvent getChordAtBar(const std::vector<theory::ChordEvent>& chords, uint32_t bar, int defaultRoot, bool isMinor) {
    for (const auto& ch : chords) {
        if (bar >= ch.startBar && bar < ch.startBar + static_cast<uint32_t>(std::ceil(ch.barLength))) {
            return ch;
        }
    }
    theory::ChordEvent fallback;
    fallback.startBar = bar;
    fallback.barLength = 2.0f;
    fallback.rootPitchClass = defaultRoot;
    fallback.quality = isMinor ? theory::ChordQuality::Minor7 : theory::ChordQuality::Major7;
    return fallback;
}

std::vector<ui::ArrangerClipNote> renderMelodyNotes(
    const SectionBlueprint& section,
    const std::vector<theory::ChordEvent>& chords,
    int rootPitchClass,
    bool isMinor,
    double energy,
    uint32_t seed)
{
    Mulberry32Rng rng(seed);
    std::vector<ui::ArrangerClipNote> notes;

    if (energy <= 0.10) return notes; // Tacet

    std::vector<int> pentatonic = isMinor
        ? std::vector<int>{0, 3, 5, 7, 10}
        : std::vector<int>{0, 2, 4, 7, 9};

    uint32_t startBar = section.startBar;
    uint32_t lengthBars = section.lengthBars;

    if (section.type == SectionType::Intro) {
        // Sparse 2-note motif in the second bar
        if (lengthBars >= 2) {
            float noteBeat = 4.0f + 2.0f;
            notes.push_back(ui::ArrangerClipNote{static_cast<uint8_t>(72 + rootPitchClass + 7), noteBeat, 2.0f, 0.70f});
            notes.push_back(ui::ArrangerClipNote{static_cast<uint8_t>(72 + rootPitchClass + (isMinor ? 3 : 4)), noteBeat + 2.0f, 3.5f, 0.65f});
        }
        return notes;
    }

    // Melodic phrasing: Antecedent / Consequent across 2-bar pairs
    for (uint32_t b = 0; b < lengthBars; b += 2) {
        auto ch = getChordAtBar(chords, startBar + b, rootPitchClass, isMinor);
        auto chordPcs = ch.getPitchClasses();

        float barBeat = static_cast<float>(b * 4);

        if (section.type == SectionType::Drop || energy >= 0.85) {
            // High energy driving hook: 16th and 8th notes
            std::vector<float> phraseSteps = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.5f, 6.0f};
            for (float st : phraseSteps) {
                if (rng.chance(0.85)) {
                    int interval = rng.pick(chordPcs);
                    int pitch = 72 + interval;
                    notes.push_back(ui::ArrangerClipNote{
                        static_cast<uint8_t>(std::clamp(pitch, 60, 96)),
                        barBeat + (st * 0.5f),
                        0.45f,
                        0.85f
                    });
                }
            }
            // Climax resolution note on bar + 1
            int climPitch = 72 + rootPitchClass + 12;
            notes.push_back(ui::ArrangerClipNote{
                static_cast<uint8_t>(std::clamp(climPitch, 60, 96)),
                barBeat + 4.0f + 0.5f,
                3.0f,
                0.90f
            });
        } else {
            // Lyrical phrasing with breath / space
            std::vector<float> beats = {0.5f, 1.5f, 2.5f, 3.5f};
            for (float bt : beats) {
                if (rng.chance(0.75)) {
                    int interval = rng.pick(pentatonic);
                    int pitch = 72 + rootPitchClass + interval;
                    notes.push_back(ui::ArrangerClipNote{
                        static_cast<uint8_t>(std::clamp(pitch, 60, 96)),
                        barBeat + bt,
                        0.9f,
                        0.78f
                    });
                }
            }
            // Answering note on bar + 1
            int ansPitch = 72 + rootPitchClass + 7;
            notes.push_back(ui::ArrangerClipNote{
                static_cast<uint8_t>(std::clamp(ansPitch, 60, 96)),
                barBeat + 4.0f + 1.0f,
                2.5f,
                0.75f
            });
        }
    }

    return notes;
}

} // anonymous namespace

EnsembleRenderResult ProceduralEnsembleEngine::renderBlueprint(
    const SongStructureBlueprint& blueprint,
    const EnsembleRenderParams& params)
{
    EnsembleRenderResult result;
    result.bpm = blueprint.bpm;
    result.songKey = theory::ChordTheory::pitchClassNames[blueprint.rootPitchClass] + std::string(" ") + blueprint.mode;
    result.totalBars = blueprint.getTotalBars();

    bool isMinor = (blueprint.mode.find("minor") != std::string::npos ||
                    blueprint.mode.find("phrygian") != std::string::npos ||
                    blueprint.mode.find("dorian") != std::string::npos);

    // 1. Build Global Chord Track across all sections
    std::vector<theory::ChordEvent> globalChords;
    uint32_t runningBar = 0;
    for (size_t secIdx = 0; secIdx < blueprint.sections.size(); ++secIdx) {
        const auto& sec = blueprint.sections[secIdx];
        if (!sec.chords.empty()) {
            uint32_t sBar = 0;
            size_t cIdx = 0;
            while (sBar < sec.lengthBars) {
                auto ch = sec.chords[cIdx % sec.chords.size()];
                ch.startBar = runningBar + sBar;
                globalChords.push_back(ch);
                sBar += static_cast<uint32_t>(std::max(1.0f, ch.barLength));
                cIdx++;
            }
        } else {
            // Generate standard diatonic progression for section
            uint32_t sBar = 0;
            std::vector<int> offsets = isMinor ? std::vector<int>{0, 8, 3, 10} : std::vector<int>{0, 7, 9, 5};
            size_t offIdx = 0;
            while (sBar < sec.lengthBars) {
                theory::ChordEvent ch;
                ch.id = "ens_chord_" + std::to_string(runningBar + sBar);
                ch.startBar = runningBar + sBar;
                ch.barLength = 2.0f;
                ch.rootPitchClass = (blueprint.rootPitchClass + offsets[offIdx % offsets.size()]) % 12;
                ch.quality = isMinor ? theory::ChordQuality::Minor7 : theory::ChordQuality::Major7;
                globalChords.push_back(ch);
                sBar += 2;
                offIdx++;
            }
        }
        runningBar += sec.lengthBars;
    }
    result.chordTrack = globalChords;

    // 2. Initialize Arranger Tracks
    uint32_t trackIdx = 0;
    for (const auto& tb : blueprint.ensemble) {
        ui::ArrangerTimelineTrack at;
        at.name = tb.name;
        at.instrument = tb.name;
        at.instrumentEngine = tb.presetId;
        at.volume = tb.defaultVolume;
        at.pan = tb.defaultPan;
        at.r = tb.colorR;
        at.g = tb.colorG;
        at.b = tb.colorB;
        result.tracks.push_back(std::move(at));
        trackIdx++;
    }

    // 3. Render Sections Track by Track
    uint32_t totalNotesGenerated = 0;

    for (size_t tIdx = 0; tIdx < blueprint.ensemble.size(); ++tIdx) {
        const auto& tb = blueprint.ensemble[tIdx];
        auto& arrTrack = result.tracks[tIdx];

        for (size_t secIdx = 0; secIdx < blueprint.sections.size(); ++secIdx) {
            const auto& section = blueprint.sections[secIdx];
            if (section.energy <= 0.05) continue; // Instrument tacet

            uint32_t seed = params.seed + static_cast<uint32_t>(secIdx * 77) + static_cast<uint32_t>(tIdx * 19);

            ui::ArrangerTimelineClip clip;
            clip.id = "clip_" + tb.trackId + "_" + std::to_string(secIdx);
            clip.name = section.name + " (" + tb.name + ")";
            clip.trackIndex = static_cast<uint32_t>(tIdx);
            clip.startBar = section.startBar + 1; // 1-indexed arranger timeline
            clip.lengthBars = section.lengthBars;
            clip.r = tb.colorR;
            clip.g = tb.colorG;
            clip.b = tb.colorB;

            switch (tb.role) {
                case FunctionalRole::Rhythm: {
                    DrumPatternParams dp;
                    dp.bars = static_cast<int>(section.lengthBars);
                    dp.density = section.energy;
                    dp.swing = params.swing;
                    dp.humanize = params.humanize;
                    dp.seed = seed;
                    if (blueprint.genre.find("Acid") != std::string::npos || blueprint.genre.find("Electro") != std::string::npos) {
                        dp.style = "House / Disco (4-on-Floor)";
                    } else if (blueprint.genre.find("Lofi") != std::string::npos) {
                        dp.style = "Hip-Hop / Boom-Bap";
                    } else if (blueprint.genre.find("Synthwave") != std::string::npos) {
                        dp.style = "House / Disco (4-on-Floor)";
                    } else {
                        dp.style = "Funk / Breakbeat";
                    }
                    ProceduralDrumEngine::populateArrangerClip(clip, dp);
                    break;
                }

                case FunctionalRole::Foundation: {
                    if (tb.presetId == "tb303" || blueprint.genre.find("Acid") != std::string::npos) {
                        AcidPatternParams ap;
                        ap.bars = static_cast<int>(section.lengthBars);
                        ap.density = section.energy;
                        ap.rootPitchClass = blueprint.rootPitchClass;
                        ap.scaleName = blueprint.mode.find("phrygian") != std::string::npos ? "Phrygian (Dark)" : "Minor Pentatonic";
                        ap.seed = seed;
                        ProceduralAcidEngine::populateArrangerClip(clip, ap);
                    } else {
                        // Walking or sustaining bassline from chord roots
                        for (uint32_t b = 0; b < section.lengthBars; ++b) {
                            auto ch = getChordAtBar(globalChords, section.startBar + b, blueprint.rootPitchClass, isMinor);
                            int rootPc = (ch.bassPitchClass >= 0) ? ch.bassPitchClass : ch.rootPitchClass;
                            int pitch = 36 + rootPc;
                            float barBeat = static_cast<float>(b * 4);

                            clip.notes.push_back(ui::ArrangerClipNote{static_cast<uint8_t>(pitch), barBeat, 3.5f, 0.88f});
                            if (section.energy >= 0.70) {
                                // Add 5th or octave bounce on beat 2.5
                                clip.notes.push_back(ui::ArrangerClipNote{static_cast<uint8_t>(pitch + 7), barBeat + 2.5f, 1.2f, 0.72f});
                            }
                        }
                    }
                    break;
                }

                case FunctionalRole::HarmonicTexture: {
                    PianoPatternParams pp;
                    pp.bars = static_cast<int>(section.lengthBars);
                    pp.seed = seed;
                    pp.humanize = params.humanize;
                    for (uint32_t b = 0; b < section.lengthBars; ++b) {
                        pp.chords.push_back(getChordAtBar(globalChords, section.startBar + b, blueprint.rootPitchClass, isMinor));
                    }
                    if (tb.textureType == TextureType::Arpeggiated) {
                        pp.style = PianoStyle::Arpeggiated;
                    } else if (tb.textureType == TextureType::Strummed) {
                        pp.style = PianoStyle::NeoSoul;
                    } else if (tb.textureType == TextureType::Stabs) {
                        pp.style = PianoStyle::PopStrum;
                    } else {
                        pp.style = PianoStyle::JazzBallad;
                    }
                    ProceduralPianoEngine::populateArrangerClip(clip, pp);
                    break;
                }

                case FunctionalRole::PrimaryMelody: {
                    clip.notes = renderMelodyNotes(section, globalChords, blueprint.rootPitchClass, isMinor, section.energy, seed);
                    break;
                }

                case FunctionalRole::Counterpoint: {
                    // Counterpoint: rhythmic answering notes on offbeats
                    auto leadNotes = renderMelodyNotes(section, globalChords, blueprint.rootPitchClass, isMinor, section.energy * 0.7, seed + 99);
                    for (auto& n : leadNotes) {
                        n.pitch = static_cast<uint8_t>(std::clamp(static_cast<int>(n.pitch) - 12, 48, 84)); // 1 octave lower
                        n.startBeat += 1.0f; // Answering 1 beat behind
                        n.velocity *= 0.85f;
                        clip.notes.push_back(n);
                    }
                    break;
                }
            }

            totalNotesGenerated += static_cast<uint32_t>(clip.notes.size());
            arrTrack.clips.push_back(std::move(clip));
        }
    }

    result.totalNotes = totalNotesGenerated;
    result.message = "Successfully rendered \"" + blueprint.title + "\" (" + std::to_string(blueprint.ensemble.size()) +
                     " tracks, " + std::to_string(result.totalBars) + " bars, " + std::to_string(totalNotesGenerated) + " notes).";
    return result;
}

EnsembleRenderResult ProceduralEnsembleEngine::renderArchetype(
    const std::string& archetypeId,
    const EnsembleRenderParams& params)
{
    const auto* archetype = SongArchetypeRegistry::getById(archetypeId);
    if (!archetype) {
        // Fallback to first available archetype
        const auto& all = SongArchetypeRegistry::getAllArchetypes();
        if (!all.empty()) {
            archetype = &all.front();
        }
    }

    if (!archetype) {
        EnsembleRenderResult res;
        res.success = false;
        res.message = "No matching song archetype found for ID: " + archetypeId;
        return res;
    }

    return renderBlueprint(archetype->getBlueprint(), params);
}

} // namespace eatsbits::procgen
