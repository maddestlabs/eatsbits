#include "eatsbits/procgen/procedural_song_engine.hpp"
#include "eatsbits/procgen/mulberry32_rng.hpp"
#include "eatsbits/theory/chord_model.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::procgen {

namespace {

std::vector<SectionBlueprint> buildSectionPlan(int totalBars, const std::string& structure) {
    std::vector<SectionBlueprint> sections;

    if (structure.find("Seamless") != std::string::npos || totalBars <= 4) {
        sections.push_back(SectionBlueprint{"Main Groove", SectionType::Verse, 0, static_cast<uint32_t>(totalBars), 0.80, {}});
        return sections;
    }

    if (structure.find("Groove Loop") != std::string::npos || totalBars <= 8) {
        uint32_t half = static_cast<uint32_t>(totalBars / 2);
        sections.push_back(SectionBlueprint{"Verse", SectionType::Verse, 0, half, 0.70, {}});
        sections.push_back(SectionBlueprint{"Chorus", SectionType::Chorus, half, static_cast<uint32_t>(totalBars) - half, 0.95, {}});
        return sections;
    }

    // Full Arrangement
    if (totalBars == 16) {
        sections = {
            SectionBlueprint{"Intro", SectionType::Intro, 0, 4, 0.40, {}},
            SectionBlueprint{"Verse", SectionType::Verse, 4, 6, 0.70, {}},
            SectionBlueprint{"Chorus", SectionType::Chorus, 10, 4, 0.95, {}},
            SectionBlueprint{"Outro", SectionType::Outro, 14, 2, 0.35, {}}
        };
    } else if (totalBars == 24) {
        sections = {
            SectionBlueprint{"Intro", SectionType::Intro, 0, 4, 0.45, {}},
            SectionBlueprint{"Verse", SectionType::Verse, 4, 8, 0.75, {}},
            SectionBlueprint{"Chorus", SectionType::Chorus, 12, 8, 0.95, {}},
            SectionBlueprint{"Outro", SectionType::Outro, 20, 4, 0.40, {}}
        };
    } else {
        // 32 bars or custom length
        sections = {
            SectionBlueprint{"Intro", SectionType::Intro, 0, 4, 0.40, {}},
            SectionBlueprint{"Verse 1", SectionType::Verse, 4, 8, 0.70, {}},
            SectionBlueprint{"Chorus", SectionType::Chorus, 12, 8, 0.95, {}},
            SectionBlueprint{"Breakdown", SectionType::Breakdown, 20, 8, 0.55, {}},
            SectionBlueprint{"Outro", SectionType::Outro, 28, 4, 0.35, {}}
        };
    }

    return sections;
}

std::string matchArchetypeId(const std::string& styleName) {
    std::string s = styleName;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s.find("acid") != std::string::npos) return "archetype_acid_techno";
    if (s.find("synthwave") != std::string::npos || s.find("retrowave") != std::string::npos) return "archetype_synthwave";
    if (s.find("lofi") != std::string::npos || s.find("lo-fi") != std::string::npos || s.find("chill") != std::string::npos) return "archetype_lofi_hiphop";
    if (s.find("cyber") != std::string::npos) return "archetype_cyberpunk";
    if (s.find("ambient") != std::string::npos || s.find("drone") != std::string::npos) return "archetype_ambient_drone";
    if (s.find("snes") != std::string::npos || s.find("16-bit") != std::string::npos) return "archetype_snes_adventure";
    if (s.find("c64") != std::string::npos || s.find("sid") != std::string::npos || s.find("chiptune") != std::string::npos) return "archetype_c64_chiptune";

    return "archetype_lofi_hiphop";
}

} // anonymous namespace

const std::vector<std::string>& ProceduralSongEngine::getAvailableStyles() {
    static const std::vector<std::string> styles = {
        "Lo-Fi Hip Hop",
        "Synthwave / Retrowave",
        "Acid Techno",
        "Cyberpunk Electro",
        "Ambient Drone",
        "SNES 16-Bit Adventure",
        "C64 SID 8-Bit Chiptune"
    };
    return styles;
}

const std::vector<std::string>& ProceduralSongEngine::getAvailableStructures() {
    static const std::vector<std::string> structures = {
        "Full Arrangement (Intro-Verse-Chorus-Outro)",
        "Groove Loop (Verse-Chorus)",
        "Seamless Loop"
    };
    return structures;
}

SongGenerationResult ProceduralSongEngine::generateSong(const SongGenerationParams& params) {
    SongArchetypeRegistry::initialize();

    std::string archId = matchArchetypeId(params.style);
    const auto* archetype = SongArchetypeRegistry::getById(archId);
    if (!archetype) {
        archetype = &SongArchetypeRegistry::getAllArchetypes().front();
    }

    SongStructureBlueprint bp = archetype->getBlueprint();

    // Override length bars & section plan if requested
    int numBars = (params.bars > 0) ? params.bars : static_cast<int>(bp.getTotalBars());
    bp.sections = buildSectionPlan(numBars, params.structure);

    // Override BPM if specified
    if (params.bpm > 20.0) {
        bp.bpm = params.bpm;
    }

    // Override root pitch class if specified (0..11)
    if (params.rootPitchClass >= 0 && params.rootPitchClass <= 11) {
        bp.rootPitchClass = params.rootPitchClass;
    } else {
        // Derive root key deterministically from seed
        Mulberry32Rng seedRng(params.seed);
        std::vector<int> candidateRoots = {0, 2, 5, 7, 9}; // C, D, F, G, A
        bp.rootPitchClass = seedRng.pick(candidateRoots);
    }

    EnsembleRenderParams ensParams;
    ensParams.seed = params.seed;
    ensParams.swing = (params.swing >= 0.0) ? params.swing : (archId == "archetype_lofi_hiphop" ? 0.35 : 0.08);
    ensParams.humanize = params.humanize;

    auto ensembleRes = ProceduralEnsembleEngine::renderBlueprint(bp, ensParams);

    SongGenerationResult res;
    res.success = ensembleRes.success;
    res.message = ensembleRes.message;
    res.bpm = ensembleRes.bpm;
    res.songKey = ensembleRes.songKey;
    res.affectedTracks = static_cast<uint32_t>(ensembleRes.tracks.size());
    res.affectedNotes = ensembleRes.totalNotes;
    res.affectedChords = static_cast<uint32_t>(ensembleRes.chordTrack.size());
    res.generatedTracks = std::move(ensembleRes.tracks);
    res.generatedChords = std::move(ensembleRes.chordTrack);

    return res;
}

SongGenerationResult ProceduralSongEngine::generateToSequencer(
    sequencer::StepSequencer& sequencer,
    const SongGenerationParams& params)
{
    auto res = generateSong(params);
    if (!res.success) return res;

    uint32_t maxSteps = static_cast<uint32_t>(params.bars * 16);

    // Clear and size sequencer tracks to match arrangement
    while (sequencer.getNumTracks() < res.generatedTracks.size()) {
        sequencer.addTrack("Track " + std::to_string(sequencer.getNumTracks() + 1), 0, maxSteps);
    }

    for (size_t tIdx = 0; tIdx < res.generatedTracks.size(); ++tIdx) {
        const auto& genTrack = res.generatedTracks[tIdx];
        auto* seqTrack = sequencer.getTrack(static_cast<uint32_t>(tIdx));
        if (!seqTrack) continue;

        seqTrack->setName(genTrack.name);
        seqTrack->setVolume(genTrack.volume);
        seqTrack->setPan(genTrack.pan);
        seqTrack->clear();
        seqTrack->setNumSteps(maxSteps);

        // Populate steps from clips
        for (const auto& clip : genTrack.clips) {
            uint32_t clipStartStep = (clip.startBar > 0 ? clip.startBar - 1 : 0) * 16;
            for (const auto& note : clip.notes) {
                // startBeat to step: 1 beat = 4 steps
                uint32_t noteStep = clipStartStep + static_cast<uint32_t>(note.startBeat * 4.0f);
                if (noteStep < maxSteps) {
                    auto& sd = seqTrack->getStep(noteStep);
                    if (!sd.active) {
                        sd.active = true;
                        sd.note = note.pitch;
                        sd.velocity = note.velocity;
                        sd.gateLength = static_cast<float>(std::clamp(note.lengthBeats * 0.25f, 0.1f, 1.0f));
                        sd.probability = 1.0f;
                    } else if (sd.note != note.pitch && std::find(sd.extraNotes.begin(), sd.extraNotes.end(), note.pitch) == sd.extraNotes.end()) {
                        sd.extraNotes.push_back(note.pitch);
                    }
                }
            }
        }
    }

    return res;
}

SongGenerationResult ProceduralSongEngine::generateDefaultDemo(const std::string& style, uint32_t seed) {
    SongGenerationParams params;
    params.style = style;
    params.bars = 16;
    params.structure = "Full Arrangement (Intro-Verse-Chorus-Outro)";
    params.seed = seed;
    return generateSong(params);
}

} // namespace eatsbits::procgen
