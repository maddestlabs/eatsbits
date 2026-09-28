#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/procgen/procedural_acid_engine.hpp"
#include "eatsbits/procgen/procedural_drum_engine.hpp"
#include "eatsbits/procgen/procedural_song_engine.hpp"
#include <random>
#include <sstream>
#include <regex>
#include <iostream>

namespace eatsbits::eatscript {

MacroRuntime::MacroRuntime() = default;

const std::vector<MacroDefinition>& MacroRuntime::getBuiltinMacros() {
    static const std::vector<MacroDefinition> builtin = {
        {
            "macro_acid_gen",
            "Generate Acid 303 Bassline",
            "Generative",
            "Procedurally synthesizes an authentic 16-step TB-303 acid sequence with slides, accents, and octave jumps.",
            R"(# Procedural TB-303 Acid Bassline Generator
eat.daw.log("Generating Acid 303 sequence...")
eat.daw.set_tempo(135.0)
eat.daw.generate_acid(root=36, steps=16)
eat.daw.log("Acid 303 sequence generated successfully.")
)"
        },
        {
            "macro_drum_techno",
            "Generate 909 Techno Drums",
            "Generative",
            "Programs a driving 909-style drum pattern across Kick, Snare, and Hi-Hat tracks.",
            R"(# Procedural 909 Techno Groove
eat.daw.log("Generating 909 Techno drums...")
eat.daw.generate_drums(style="techno")
eat.daw.log("909 Techno drum pattern initialized.")
)"
        },
        {
            "macro_humanize_all",
            "Humanize All Tracks",
            "Mixing",
            "Applies subtle micro-timing swing and organic velocity variations to all pattern steps.",
            R"(# Organic Humanizer Macro
eat.daw.log("Humanizing all active sequencer tracks...")
eat.daw.humanize(timing=0.03, velocity=0.12)
eat.daw.log("Humanization applied.")
)"
        },
        {
            "macro_arp_track",
            "Arpeggiate Active Track",
            "Music Theory",
            "Transforms active track notes into a syncopated 16th-note arpeggio.",
            R"(# Track Arpeggiator
eat.daw.log("Arpeggiating active track...")
eat.daw.arpeggiate(rate=1.0, octaves=2, pattern="updown")
eat.daw.log("Arpeggiation complete.")
)"
        },
        {
            "macro_song_gen",
            "Generate Full Procedural Song",
            "Generative",
            "Synthesizes a complete multi-track song arrangement across Drums, Bass, Chords, and Lead melody.",
            R"(# Procedural Song Architect
eat.daw.log("Generating full procedural song arrangement...")
eat.daw.generate_song(style="Lo-Fi Hip Hop", bars=16, seed=42)
eat.daw.log("Procedural song generated successfully.")
)"
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

} // namespace eatsbits::eatscript
