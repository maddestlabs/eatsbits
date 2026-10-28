#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/procgen/procedural_acid_engine.hpp"
#include "eatsbits/procgen/procedural_drum_engine.hpp"
#include "eatsbits/procgen/procedural_song_engine.hpp"
#include <random>
#include <sstream>
#include <regex>
#include <iostream>

#include "eatsbits/procgen/procedural_ensemble_engine.hpp"
#include "eatsbits/procgen/procedural_piano_engine.hpp"
#include "eatsbits/procgen/song_archetypes.hpp"
#include "eatsbits/theory/chord_model.hpp"

namespace eatsbits::eatscript {

MacroRuntime::MacroRuntime() = default;

const MacroDefinition* MacroRuntime::getMacroById(const std::string& id) {
    const auto& list = getBuiltinMacros();
    for (const auto& m : list) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

const std::vector<MacroDefinition>& MacroRuntime::getBuiltinMacros() {
    static const std::vector<MacroDefinition> builtin = {
        {
            "macro_song_gen",
            "Procedural Song Architect",
            "Generative",
            "Synthesizes a complete multi-track song arrangement across Drums, Bass, Chords, and Lead melody.",
            R"(# Procedural Song Architect
eat.daw.log("Generating full procedural song arrangement...")
eat.daw.generate_song(style="Lo-Fi Hip Hop", bars=16, seed=42)
eat.daw.log("Procedural song generated successfully.")
)",
            {
                {"Style", "Style / Genre", MacroParamType::Choice, 0.0f, 6.0f, 0.0f, 1.0f, {
                    "Lo-Fi Hip Hop", "Synthwave / Retrowave", "Acid Techno",
                    "Cyberpunk Electro", "Ambient Drone", "SNES 16-Bit Adventure", "C64 SID 8-Bit Chiptune"
                }},
                {"Structure", "Song Structure", MacroParamType::Choice, 0.0f, 2.0f, 0.0f, 1.0f, {
                    "Full Arrangement (Intro-Verse-Chorus-Outro)",
                    "Groove Loop (Verse-Chorus)",
                    "Seamless Loop"
                }},
                {"Bars", "Timeline Length (Bars)", MacroParamType::IntStep, 4.0f, 32.0f, 16.0f, 4.0f, {}},
                {"Seed", "RNG Seed", MacroParamType::IntStep, 1.0f, 999999.0f, 42.0f, 1.0f, {}},
                {"Swing", "Groove Swing", MacroParamType::Slider, 0.0f, 1.0f, 0.25f, 0.05f, {}},
                {"Humanize", "Micro-Timing Humanize", MacroParamType::Slider, 0.0f, 1.0f, 0.20f, 0.05f, {}}
            }
        },
        {
            "macro_ensemble_arranger",
            "Dynamic Ensemble Arranger",
            "Generative",
            "Composes dynamic arrangements across variable-N ensembles (duets, 3-voice trackers, 5-track battle themes, ambient dungeon soundscapes).",
            R"(# Procedural Ensemble Arranger
eat.daw.log("Arranging dynamic ensemble...")
eat.daw.set_tempo(120.0)
eat.daw.log("Ensemble arrangement complete.")
)",
            {
                {"Template", "Ensemble Archetype", MacroParamType::Choice, 0.0f, 5.0f, 0.0f, 1.0f, {
                    "RPG Tavern (Lute & Flute Duet, 3/4)",
                    "Evolving Action Theme (Calm to Drop)",
                    "Ambient Ethereal Dungeon (Harp & Pad)",
                    "C64 3-Voice Tracker (Bass & Lead)",
                    "Neo-Soul Trio (Upright & Rhodes)",
                    "Orchestral Battle March"
                }},
                {"Bars", "Timeline Length (Bars)", MacroParamType::IntStep, 8.0f, 32.0f, 16.0f, 8.0f, {}},
                {"Seed", "RNG Seed", MacroParamType::IntStep, 1.0f, 999999.0f, 42.0f, 1.0f, {}},
                {"Swing", "Swing Factor", MacroParamType::Slider, 0.0f, 1.0f, 0.15f, 0.05f, {}},
                {"Humanize", "Humanize Amount", MacroParamType::Slider, 0.0f, 1.0f, 0.20f, 0.05f, {}}
            }
        },
        {
            "macro_acid_gen",
            "Procedural TB-303 Acid Generator",
            "Generative",
            "Procedurally synthesizes an authentic 16-step TB-303 acid sequence with slides, accents, and octave jumps.",
            R"(# Procedural TB-303 Acid Bassline Generator
eat.daw.log("Generating Acid 303 sequence...")
eat.daw.set_tempo(135.0)
eat.daw.generate_acid(root=36, steps=16)
eat.daw.log("Acid 303 sequence generated successfully.")
)",
            {
                {"Root", "Root Key", MacroParamType::Choice, 0.0f, 11.0f, 0.0f, 1.0f, {
                    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
                }},
                {"Octave", "Base Octave", MacroParamType::IntStep, 1.0f, 4.0f, 2.0f, 1.0f, {}},
                {"Bars", "Pattern Bars", MacroParamType::IntStep, 1.0f, 4.0f, 1.0f, 1.0f, {}},
                {"AccentDensity", "Accent Density", MacroParamType::Slider, 0.0f, 1.0f, 0.35f, 0.05f, {}},
                {"SlideDensity", "Portamento Slide Density", MacroParamType::Slider, 0.0f, 1.0f, 0.30f, 0.05f, {}},
                {"Seed", "RNG Seed", MacroParamType::IntStep, 1.0f, 999999.0f, 42.0f, 1.0f, {}}
            }
        },
        {
            "macro_drum_techno",
            "Procedural Drum Groover",
            "Generative",
            "Programs a genre-authentic drum pattern across Kick, Snare, Hi-Hats, and Percussion tracks.",
            R"(# Procedural Drum Groove
eat.daw.log("Generating drum groove...")
eat.daw.generate_drums(style="techno")
eat.daw.log("Drum pattern initialized.")
)",
            {
                {"Style", "Drum Style", MacroParamType::Choice, 0.0f, 4.0f, 4.0f, 1.0f, {
                    "House / Disco (4-on-Floor)", "Hip-Hop / Boom-Bap", "Funk / Breakbeat", "Trap / Halftime", "Techno"
                }},
                {"Bars", "Pattern Bars", MacroParamType::IntStep, 1.0f, 4.0f, 1.0f, 1.0f, {}},
                {"Swing", "Swing Factor", MacroParamType::Slider, 0.0f, 1.0f, 0.20f, 0.05f, {}},
                {"FillDensity", "Turnaround Fill Density", MacroParamType::Slider, 0.0f, 1.0f, 0.25f, 0.05f, {}},
                {"GhostProb", "Ghost Note Probability", MacroParamType::Slider, 0.0f, 1.0f, 0.20f, 0.05f, {}},
                {"Seed", "RNG Seed", MacroParamType::IntStep, 1.0f, 999999.0f, 42.0f, 1.0f, {}}
            }
        },
        {
            "macro_piano_comp",
            "Procedural Piano Composition",
            "Music Theory",
            "Composes harmonic piano accompaniments with voice leading, jazz chords, and expressive left/right hand patterns.",
            R"(# Procedural Piano Composition
eat.daw.log("Composing procedural piano...")
eat.daw.log("Piano composition complete.")
)",
            {
                {"Style", "Piano Style", MacroParamType::Choice, 0.0f, 4.0f, 0.0f, 1.0f, {
                    "NeoSoul", "JazzBallad", "PopStrum", "Classical", "Arpeggiated"
                }},
                {"Bars", "Length (Bars)", MacroParamType::IntStep, 2.0f, 16.0f, 4.0f, 2.0f, {}},
                {"Humanize", "Velocity & Rubato Humanize", MacroParamType::Slider, 0.0f, 1.0f, 0.20f, 0.05f, {}},
                {"StrumSpread", "Strum Spread (Beats)", MacroParamType::Slider, 0.005f, 0.08f, 0.035f, 0.005f, {}},
                {"BassOctaves", "Add Left-Hand Bass Octaves", MacroParamType::BoolToggle, 0.0f, 1.0f, 1.0f, 1.0f, {}},
                {"Seed", "RNG Seed", MacroParamType::IntStep, 1.0f, 999999.0f, 101.0f, 1.0f, {}}
            }
        },
        {
            "macro_harmonic_progression",
            "Harmonic Progression Generator",
            "Music Theory",
            "Generates Roman numeral / modal chord progressions across the project chord track.",
            R"(# Harmonic Progression Generator
eat.daw.log("Generating harmonic chord progression...")
eat.daw.log("Progression generated.")
)",
            {
                {"Genre", "Progression Genre", MacroParamType::Choice, 0.0f, 4.0f, 0.0f, 1.0f, {
                    "Synthwave", "Pop Anthem", "Deep House / Club", "Jazz / Neo-Soul", "Classic EDM"
                }},
                {"Bars", "Length (Bars)", MacroParamType::IntStep, 4.0f, 32.0f, 8.0f, 4.0f, {}},
                {"ConformTracks", "Conform Existing Tracks to Chords", MacroParamType::BoolToggle, 0.0f, 1.0f, 1.0f, 1.0f, {}}
            }
        },
        {
            "macro_global_transpose",
            "Global Chord-Aware Transpose",
            "Arrangement",
            "Transposes all tracks, clips, and chord track events across the entire project with scale/chord adherence.",
            R"(# Global Transpose
eat.daw.log("Transposing song...")
eat.daw.log("Transpose complete.")
)",
            {
                {"Semitones", "Pitch Shift (Semitones)", MacroParamType::IntStep, -12.0f, 12.0f, 2.0f, 1.0f, {}},
                {"HarmonicMode", "Harmonic Mode", MacroParamType::Choice, 0.0f, 2.0f, 0.0f, 1.0f, {
                    "Strict Chromatic", "Snap to Scale", "Smart Chord Shift"
                }},
                {"UpdateKey", "Update Project Song Key", MacroParamType::BoolToggle, 0.0f, 1.0f, 1.0f, 1.0f, {}}
            }
        },
        {
            "macro_humanize_all",
            "Groove & Velocity Humanizer",
            "Mixing",
            "Applies subtle micro-timing swing and organic velocity variations to all pattern steps.",
            R"(# Organic Humanizer Macro
eat.daw.log("Humanizing all active sequencer tracks...")
eat.daw.humanize(timing=0.03, velocity=0.12)
eat.daw.log("Humanization applied.")
)",
            {
                {"TimingJitter", "Timing Jitter", MacroParamType::Slider, 0.00f, 0.10f, 0.03f, 0.005f, {}},
                {"VelocityJitter", "Velocity Jitter", MacroParamType::Slider, 0.00f, 0.30f, 0.12f, 0.01f, {}},
                {"Seed", "RNG Seed", MacroParamType::IntStep, 1.0f, 999999.0f, 42.0f, 1.0f, {}}
            }
        },
        {
            "macro_arp_track",
            "Active Track Arpeggiator",
            "Music Theory",
            "Transforms active track notes into a syncopated 16th-note arpeggio.",
            R"(# Track Arpeggiator
eat.daw.log("Arpeggiating active track...")
eat.daw.arpeggiate(rate=1.0, octaves=2, pattern="updown")
eat.daw.log("Arpeggiation complete.")
)",
            {
                {"Rate", "Arp Rate", MacroParamType::Choice, 0.0f, 3.0f, 0.0f, 1.0f, {
                    "1/16 Note", "1/8 Note", "1/4 Note", "1/32 Note"
                }},
                {"Octaves", "Octave Span", MacroParamType::IntStep, 1.0f, 4.0f, 2.0f, 1.0f, {}},
                {"Pattern", "Direction Pattern", MacroParamType::Choice, 0.0f, 3.0f, 2.0f, 1.0f, {
                    "Up", "Down", "UpDown", "Random"
                }}
            }
        }
    };
    return builtin;
}

MacroResult MacroRuntime::execute(const std::string& macroScript,
                                  audio::AudioEngine& engine,
                                  sequencer::StepSequencer& sequencer) {
    (void)engine;
    MacroResult res;
    res.success = true;

    Evaluator eval;

    // 1. log
    auto logFn = [&res](const std::vector<Value>& args, const auto&) -> Value {
        std::string msg = args.empty() ? "" : args[0].asString();
        res.logs.push_back(msg);
        return Value();
    };
    eval.registerMemberFunction("eat.daw", "log", logFn);
    eval.registerMemberFunction("project", "log", logFn);

    // 2. set_tempo
    auto tempoFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        double bpm = resolveArg(args, kwargs, 0, "bpm", Value(120.0)).asNumber(120.0);
        sequencer.setBpm(bpm);
        std::ostringstream ss;
        ss << "Tempo set to " << bpm;
        if (bpm == static_cast<int>(bpm)) ss << ".0";
        ss << " BPM";
        res.logs.push_back(ss.str());
        return Value(bpm);
    };
    eval.registerMemberFunction("eat.daw", "set_tempo", tempoFn);
    eval.registerMemberFunction("project", "set_tempo", tempoFn);

    // 3. set_swing
    auto swingFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        double swing = resolveArg(args, kwargs, 0, "swing", Value(0.5)).asNumber(0.5);
        sequencer.setSwing(swing);
        std::ostringstream ss;
        ss << "Swing set to " << swing;
        res.logs.push_back(ss.str());
        return Value(swing);
    };
    eval.registerMemberFunction("eat.daw", "set_swing", swingFn);
    eval.registerMemberFunction("project", "set_swing", swingFn);

    // 4. generate_acid
    auto acidFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        int root = resolveArg(args, kwargs, 0, "root", Value(36)).asInt(36);
        uint32_t steps = static_cast<uint32_t>(resolveArg(args, kwargs, 1, "steps", Value(16)).asInt(16));
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 2, "seed", Value(42)).asInt(42));
        if (sequencer.getNumTracks() > 0) {
            auto* t = sequencer.getTrack(0);
            if (t) generateAcidBassline(*t, root, steps, seed);
            res.logs.push_back("Generated Acid 303 Bassline on Track 1");
        }
        return Value(true);
    };
    eval.registerMemberFunction("eat.daw", "generate_acid", acidFn);
    eval.registerMemberFunction("project", "generate_acid", acidFn);

    // 5. generate_drums
    auto drumsFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        std::string style = resolveArg(args, kwargs, 0, "style", Value("Techno")).asString();
        if (style.empty()) style = "Techno";
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 1, "seed", Value(42)).asInt(42));
        generateDrumPattern(sequencer, style, seed);
        res.logs.push_back("Generated drum pattern: " + style);
        return Value(true);
    };
    eval.registerMemberFunction("eat.daw", "generate_drums", drumsFn);
    eval.registerMemberFunction("project", "generate_drums", drumsFn);

    // 6. generate_song
    auto songFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        std::string style = resolveArg(args, kwargs, 0, "style", Value("Lo-Fi Hip Hop")).asString();
        uint32_t bars = static_cast<uint32_t>(resolveArg(args, kwargs, 1, "bars", Value(16)).asInt(16));
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 2, "seed", Value(42)).asInt(42));
        auto r = generateProceduralSong(sequencer, style, bars, seed);
        res.logs.push_back(r.message);
        return Value(r.success);
    };
    eval.registerMemberFunction("eat.daw", "generate_song", songFn);
    eval.registerMemberFunction("project", "generate_song", songFn);

    // 7. humanize
    auto humanizeFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        float timing = resolveArg(args, kwargs, 0, "timing", Value(0.03)).asFloat(0.03f);
        float vel = resolveArg(args, kwargs, 1, "velocity", Value(0.12)).asFloat(0.12f);
        uint32_t seed = static_cast<uint32_t>(resolveArg(args, kwargs, 2, "seed", Value(42)).asInt(42));
        humanizeAllTracks(sequencer, timing, vel, seed);
        res.logs.push_back("Humanized all tracks");
        return Value(true);
    };
    eval.registerMemberFunction("eat.daw", "humanize", humanizeFn);
    eval.registerMemberFunction("project", "humanize", humanizeFn);

    // 8. arpeggiate
    auto arpFn = [&res, &sequencer](const std::vector<Value>& args, const auto& kwargs) -> Value {
        double rate = resolveArg(args, kwargs, 0, "rate", Value(1.0)).asNumber(1.0);
        int octaves = resolveArg(args, kwargs, 1, "octaves", Value(2)).asInt(2);
        std::string pattern = resolveArg(args, kwargs, 2, "pattern", Value("updown")).asString();
        if (pattern.empty()) pattern = "updown";
        if (sequencer.getNumTracks() > 0) {
            auto* t = sequencer.getTrack(0);
            if (t) arpeggiateTrack(*t, rate, octaves, pattern);
            res.logs.push_back("Arpeggiated Track 1");
        }
        return Value(true);
    };
    eval.registerMemberFunction("eat.daw", "arpeggiate", arpFn);
    eval.registerMemberFunction("project", "arpeggiate", arpFn);

    eval.evaluateSource(macroScript);
    if (!eval.getLastError().empty()) {
        res.success = false;
        res.message = "Macro error: " + eval.getLastError();
    } else {
        res.message = "Macro execution completed successfully.";
    }
    return res;
}

MacroResult MacroRuntime::generateAcidBassline(sequencer::SequencerTrack& track,
                                              int rootPitch,
                                              uint32_t numSteps,
                                              uint32_t seed) {
    MacroResult res;
    procgen::AcidPatternParams params;
    params.baseMidiOctave = (rootPitch / 12) * 12;
    params.rootPitchClass = rootPitch % 12;
    params.bars = static_cast<int>(std::max(1u, numSteps / 16));
    params.seed = seed;
    procgen::ProceduralAcidEngine::populateSequencerTrack(track, params);

    res.success = true;
    res.message = "Authentic TB-303 Acid bassline generated.";
    return res;
}

MacroResult MacroRuntime::generateDrumPattern(sequencer::StepSequencer& sequencer,
                                             const std::string& style,
                                             uint32_t seed) {
    MacroResult res;
    if (sequencer.getNumTracks() == 0) {
        res.success = false;
        res.message = "No tracks available in sequencer.";
        return res;
    }

    auto* track = sequencer.getTrack(0);
    if (!track) {
        res.success = false;
        res.message = "Failed to access sequencer track.";
        return res;
    }

    procgen::DrumPatternParams params;
    params.bars = 1;
    params.seed = seed;
    std::string s = style;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("boom") != std::string::npos || s.find("hip") != std::string::npos) {
        params.style = "Hip-Hop / Boom-Bap";
    } else if (s.find("funk") != std::string::npos) {
        params.style = "Funk / Breakbeat";
    } else if (s.find("trap") != std::string::npos) {
        params.style = "Trap / Halftime";
    } else {
        params.style = "House / Disco (4-on-Floor)";
    }

    procgen::ProceduralDrumEngine::populateSequencerTrack(*track, params);

    res.success = true;
    res.message = "Procedural drum pattern generated.";
    return res;
}

MacroResult MacroRuntime::generateProceduralSong(sequencer::StepSequencer& sequencer,
                                                 const std::string& style,
                                                 uint32_t bars,
                                                 uint32_t seed) {
    MacroResult res;
    procgen::SongGenerationParams params;
    params.style = style;
    params.bars = static_cast<int>(bars);
    params.seed = seed;

    auto genRes = procgen::ProceduralSongEngine::generateToSequencer(sequencer, params);
    res.success = genRes.success;
    res.message = genRes.message;
    return res;
}

MacroResult MacroRuntime::humanizeAllTracks(sequencer::StepSequencer& sequencer,
                                           float timingJitter,
                                           float velocityJitter,
                                           uint32_t seed) {
    MacroResult res;
    size_t numTracks = sequencer.getNumTracks();
    std::mt19937 randGen(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    for (size_t t = 0; t < numTracks; ++t) {
        auto* track = sequencer.getTrack(t);
        if (!track) continue;

        for (uint32_t s = 0; s < track->getNumSteps(); ++s) {
            auto& step = track->getStep(s);
            if (!step.active) continue;

            float vJitter = dist(randGen) * velocityJitter;
            step.velocity = std::clamp(step.velocity + vJitter, 0.1f, 1.0f);

            float gJitter = dist(randGen) * timingJitter;
            step.gateLength = std::clamp(step.gateLength + gJitter, 0.1f, 1.0f);
        }
    }

    res.success = true;
    res.message = "Humanization applied to all tracks.";
    return res;
}

MacroResult MacroRuntime::arpeggiateTrack(sequencer::SequencerTrack& track,
                                         double rate,
                                         int octaves,
                                         const std::string& pattern) {
    MacroResult res;
    std::vector<MidiNote> notes = MidiPipelineEngine::trackToNotes(track);
    if (notes.empty()) {
        res.success = false;
        res.message = "No notes to arpeggiate.";
        return res;
    }

    std::vector<MidiNote> arped = MidiPipelineEngine::applyArpeggiator(notes, rate, octaves, pattern);
    MidiPipelineEngine::notesToTrack(arped, track);

    res.success = true;
    res.message = "Track arpeggiated.";
    return res;
}

MacroResult MacroRuntime::executeParameterized(const std::string& macroId,
                                              const std::map<std::string, float>& numParams,
                                              const std::map<std::string, std::string>& strParams,
                                              audio::AudioEngine& engine,
                                              sequencer::StepSequencer& sequencer,
                                              uint32_t targetTrackIndex) {
    (void)engine;
    MacroResult res;
    res.success = true;

    auto getNum = [&](const std::string& k, float def) -> float {
        auto it = numParams.find(k);
        return it != numParams.end() ? it->second : def;
    };
    auto getStr = [&](const std::string& k, const std::string& def) -> std::string {
        auto it = strParams.find(k);
        return it != strParams.end() ? it->second : def;
    };

    if (macroId == "macro_song_gen" || macroId == "macro_procedural_song") {
        procgen::SongGenerationParams p;
        p.style = getStr("Style", "Lo-Fi Hip Hop");
        p.structure = getStr("Structure", "Full Arrangement (Intro-Verse-Chorus-Outro)");
        p.bars = static_cast<int>(getNum("Bars", 16.0f));
        p.seed = static_cast<uint32_t>(getNum("Seed", 42.0f));
        p.swing = getNum("Swing", -1.0f);
        p.humanize = getNum("Humanize", 0.20f);

        auto songRes = procgen::ProceduralSongEngine::generateSong(p);
        if (songRes.success) {
            procgen::ProceduralSongEngine::generateToSequencer(sequencer, p);
            sequencer.setBpm(songRes.bpm);
            res.bpm = songRes.bpm;
            res.songKey = songRes.songKey;
            res.affectedTracks = songRes.affectedTracks;
            res.affectedNotes = songRes.affectedNotes;
            res.affectedChords = songRes.affectedChords;
            res.generatedTracks = songRes.generatedTracks;
            res.generatedChords = songRes.generatedChords;
            res.message = songRes.message;
        } else {
            res.success = false;
            res.message = songRes.message.empty() ? "Procedural song generation failed." : songRes.message;
        }
        return res;
    }

    if (macroId == "macro_ensemble_arranger") {
        std::string tmpl = getStr("Template", "RPG Tavern (Lute & Flute Duet, 3/4)");
        procgen::EnsembleRenderParams ep;
        ep.seed = static_cast<uint32_t>(getNum("Seed", 42.0f));
        ep.swing = getNum("Swing", 0.15f);
        ep.humanize = getNum("Humanize", 0.20f);
        ep.generateChordTrack = true;

        std::string archId = "archetype_lofi_hiphop";
        if (tmpl.find("Tavern") != std::string::npos || tmpl.find("RPG") != std::string::npos) {
            archId = "archetype_snes_adventure";
        } else if (tmpl.find("Action") != std::string::npos) {
            archId = "archetype_cyberpunk";
        } else if (tmpl.find("Dungeon") != std::string::npos || tmpl.find("Ambient") != std::string::npos) {
            archId = "archetype_ambient_drone";
        } else if (tmpl.find("C64") != std::string::npos || tmpl.find("Tracker") != std::string::npos) {
            archId = "archetype_c64_chiptune";
        } else if (tmpl.find("Neo-Soul") != std::string::npos || tmpl.find("Jazz") != std::string::npos) {
            archId = "archetype_lofi_hiphop";
        } else if (tmpl.find("March") != std::string::npos || tmpl.find("Orchestral") != std::string::npos) {
            archId = "archetype_acid_techno";
        }

        auto ensRes = procgen::ProceduralEnsembleEngine::renderArchetype(archId, ep);
        if (ensRes.success) {
            res.bpm = ensRes.bpm;
            res.songKey = ensRes.songKey;
            res.affectedTracks = static_cast<uint32_t>(ensRes.tracks.size());
            res.affectedNotes = ensRes.totalNotes;
            res.affectedChords = static_cast<uint32_t>(ensRes.chordTrack.size());
            res.generatedTracks = ensRes.tracks;
            res.generatedChords = ensRes.chordTrack;
            res.message = "Arranged dynamic ensemble: " + tmpl;
            sequencer.setBpm(ensRes.bpm);
        } else {
            res.success = false;
            res.message = ensRes.message;
        }
        return res;
    }

    if (macroId == "macro_acid_gen") {
        std::string rootStr = getStr("Root", "C");
        static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int rootPC = 0;
        for (int i = 0; i < 12; ++i) {
            if (rootStr == noteNames[i]) { rootPC = i; break; }
        }
        int octave = static_cast<int>(getNum("Octave", 2.0f));
        int rootPitch = (octave + 1) * 12 + rootPC;
        int bars = static_cast<int>(getNum("Bars", 1.0f));
        uint32_t seed = static_cast<uint32_t>(getNum("Seed", 42.0f));

        procgen::AcidPatternParams p;
        p.rootPitchClass = rootPC;
        p.baseMidiOctave = (rootPitch / 12) * 12;
        p.bars = bars;
        p.accentProb = getNum("AccentDensity", 0.35f);
        p.slideProb = getNum("SlideDensity", 0.30f);
        p.seed = seed;

        while (sequencer.getNumTracks() <= targetTrackIndex) {
            sequencer.addTrack("Acid 303", 0, static_cast<uint32_t>(bars * 16));
        }
        auto* track = sequencer.getTrack(targetTrackIndex);
        if (track) {
            track->setName("Acid 303");
            procgen::ProceduralAcidEngine::populateSequencerTrack(*track, p);
            res.affectedTracks = 1;
            res.affectedNotes = static_cast<uint32_t>(bars * 12);
            res.message = "Generated TB-303 Acid Bassline on Track " + std::to_string(targetTrackIndex + 1) +
                          " (" + rootStr + " " + std::to_string(octave) + ").";
        }
        return res;
    }

    if (macroId == "macro_drum_techno" || macroId == "macro_drum_groover") {
        procgen::DrumPatternParams p;
        p.style = getStr("Style", "Techno");
        p.bars = static_cast<int>(getNum("Bars", 1.0f));
        p.swing = getNum("Swing", 0.20f);
        p.fillDensity = getNum("FillDensity", 0.25f);
        p.ghostProb = getNum("GhostProb", 0.20f);
        p.seed = static_cast<uint32_t>(getNum("Seed", 42.0f));

        while (sequencer.getNumTracks() <= targetTrackIndex) {
            sequencer.addTrack("Drums", 0, static_cast<uint32_t>(p.bars * 16));
        }
        auto* track = sequencer.getTrack(targetTrackIndex);
        if (track) {
            track->setName("Drums (" + p.style + ")");
            procgen::ProceduralDrumEngine::populateSequencerTrack(*track, p);
            res.affectedTracks = 1;
            res.affectedNotes = static_cast<uint32_t>(p.bars * 16);
            res.message = "Generated drum groove on Track " + std::to_string(targetTrackIndex + 1) + ": " + p.style + ".";
        }
        return res;
    }

    if (macroId == "macro_piano_comp") {
        procgen::PianoPatternParams p;
        p.style = procgen::parsePianoStyle(getStr("Style", "NeoSoul"));
        p.bars = static_cast<int>(getNum("Bars", 4.0f));
        p.humanize = getNum("Humanize", 0.20f);
        p.strumSpreadBeats = getNum("StrumSpread", 0.035f);
        p.addBassOctaves = getNum("BassOctaves", 1.0f) > 0.5f;
        p.seed = static_cast<uint32_t>(getNum("Seed", 101.0f));

        while (sequencer.getNumTracks() <= targetTrackIndex) {
            sequencer.addTrack("Piano", 0, static_cast<uint32_t>(p.bars * 16));
        }
        auto* track = sequencer.getTrack(targetTrackIndex);
        if (track) {
            track->setName("Piano Comp");
            procgen::ProceduralPianoEngine::populateSequencerTrack(*track, p);
            res.affectedTracks = 1;
            res.affectedNotes = static_cast<uint32_t>(p.bars * 8);
            res.message = "Composed piano accompaniment on Track " + std::to_string(targetTrackIndex + 1) + ".";
        }
        return res;
    }

    if (macroId == "macro_harmonic_progression") {
        int bars = static_cast<int>(getNum("Bars", 8.0f));
        std::string genre = getStr("Genre", "Synthwave");

        struct ChordStep {
            int rootOffset;
            theory::ChordQuality quality;
        };
        std::vector<ChordStep> prog;
        int tonicRoot = 0; // C

        if (genre == "Synthwave") {
            tonicRoot = 9; // A minor
            prog = {
                {0, theory::ChordQuality::Minor},
                {8, theory::ChordQuality::Major},  // F
                {3, theory::ChordQuality::Major},  // C
                {10, theory::ChordQuality::Major} // G
            };
        } else if (genre.find("Lo-Fi") != std::string::npos || genre.find("Hip") != std::string::npos) {
            tonicRoot = 0; // C major
            prog = {
                {2, theory::ChordQuality::Min9},
                {7, theory::ChordQuality::Dominant7},
                {0, theory::ChordQuality::Maj9},
                {9, theory::ChordQuality::Minor7}
            };
        } else if (genre == "Neo-Soul") {
            tonicRoot = 0;
            prog = {
                {5, theory::ChordQuality::Maj9},
                {4, theory::ChordQuality::Minor7},
                {2, theory::ChordQuality::Min9},
                {0, theory::ChordQuality::Maj9}
            };
        } else if (genre.find("Pop") != std::string::npos) {
            tonicRoot = 0;
            prog = {
                {0, theory::ChordQuality::Major},
                {7, theory::ChordQuality::Major},
                {9, theory::ChordQuality::Minor},
                {5, theory::ChordQuality::Major}
            };
        } else if (genre == "EDM") {
            tonicRoot = 9; // A minor
            prog = {
                {0, theory::ChordQuality::Minor},
                {8, theory::ChordQuality::Major},
                {5, theory::ChordQuality::Minor},
                {7, theory::ChordQuality::Minor}
            };
        } else if (genre == "Jazz") {
            tonicRoot = 0;
            prog = {
                {2, theory::ChordQuality::Minor7},
                {7, theory::ChordQuality::Dominant7},
                {0, theory::ChordQuality::Major7},
                {9, theory::ChordQuality::Dominant7}
            };
        } else {
            // Dark Cyberpunk
            tonicRoot = 0;
            prog = {
                {0, theory::ChordQuality::Minor},
                {1, theory::ChordQuality::Major},
                {0, theory::ChordQuality::Minor},
                {8, theory::ChordQuality::Major}
            };
        }

        uint32_t curBar = 0;
        float barLen = (bars <= 4) ? 1.0f : 2.0f;

        size_t pIdx = 0;
        while (curBar < static_cast<uint32_t>(bars)) {
            const auto& step = prog[pIdx % prog.size()];
            theory::ChordEvent ev;
            ev.id = "chord_" + std::to_string(res.generatedChords.size());
            ev.startBar = curBar;
            ev.barLength = barLen;
            ev.rootPitchClass = (tonicRoot + step.rootOffset) % 12;
            ev.quality = step.quality;
            res.generatedChords.push_back(ev);
            curBar += static_cast<uint32_t>(barLen);
            pIdx++;
        }

        res.affectedChords = static_cast<uint32_t>(res.generatedChords.size());
        res.message = "Generated " + std::to_string(res.affectedChords) + " harmonic chords (" + genre + ").";
        return res;
    }

    if (macroId == "macro_global_transpose") {
        int semitones = static_cast<int>(getNum("Semitones", 2.0f));
        for (size_t t = 0; t < sequencer.getNumTracks(); ++t) {
            auto* trk = sequencer.getTrack(t);
            if (!trk) continue;
            for (uint32_t s = 0; s < trk->getNumSteps(); ++s) {
                auto& step = trk->getStep(s);
                if (step.active) {
                    step.note = static_cast<uint8_t>(std::clamp(static_cast<int>(step.note) + semitones, 0, 127));
                    res.affectedNotes++;
                }
            }
            res.affectedTracks++;
        }
        res.message = "Transposed project by " + std::to_string(semitones) + " semitones.";
        return res;
    }

    if (macroId == "macro_humanize_all") {
        float timing = getNum("TimingJitter", 0.03f);
        float vel = getNum("VelocityJitter", 0.12f);
        uint32_t seed = static_cast<uint32_t>(getNum("Seed", 42.0f));
        return humanizeAllTracks(sequencer, timing, vel, seed);
    }

    if (macroId == "macro_arp_track") {
        double rate = 1.0;
        std::string rStr = getStr("Rate", "1/16 Note");
        if (rStr.find("1/8") != std::string::npos) rate = 0.5;
        else if (rStr.find("1/4") != std::string::npos) rate = 0.25;
        else if (rStr.find("1/32") != std::string::npos) rate = 2.0;

        int octaves = static_cast<int>(getNum("Octaves", 2.0f));
        std::string pattern = getStr("Pattern", "UpDown");
        if (targetTrackIndex < sequencer.getNumTracks()) {
            auto* t = sequencer.getTrack(targetTrackIndex);
            if (t) return arpeggiateTrack(*t, rate, octaves, pattern);
        } else if (sequencer.getNumTracks() > 0) {
            auto* t = sequencer.getTrack(0);
            if (t) return arpeggiateTrack(*t, rate, octaves, pattern);
        }
        res.message = "No track available to arpeggiate.";
        return res;
    }

    const auto* def = getMacroById(macroId);
    if (def) {
        return execute(def->scriptCode, engine, sequencer);
    }

    res.success = false;
    res.message = "Unknown macro: " + macroId;
    return res;
}

} // namespace eatsbits::eatscript
