#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
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

    // 6. humanize
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

    // 7. arpeggiate
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
    track.clear();
    track.setNumSteps(numSteps);

    std::mt19937 randGen(seed);
    static const int minorScale[] = {0, 3, 5, 7, 10, 12};
    std::uniform_int_distribution<size_t> noteDist(0, 5);
    std::uniform_real_distribution<float> probDist(0.0f, 1.0f);

    for (uint32_t s = 0; s < numSteps; ++s) {
        // Density approx 65% active steps
        if (probDist(randGen) < 0.35f && (s % 4 != 0)) continue;

        sequencer::StepData step;
        step.active = true;
        int interval = minorScale[noteDist(randGen)];
        int octShift = (probDist(randGen) > 0.75f) ? 12 : 0;
        step.note = static_cast<uint8_t>(std::clamp(rootPitch + interval + octShift, 24, 84));

        step.velocity = (s % 4 == 0) ? 0.95f : 0.80f;
        step.gateLength = 0.75f;

        // Slide logic: more likely after accented notes or 16th runs
        step.slide = (probDist(randGen) < 0.25f);
        // Accent on downbeats or syncopated hits
        step.accent = (probDist(randGen) < 0.30f) || (s == 0);

        track.setStep(s, step);
    }

    res.success = true;
    res.message = "Acid bassline generated.";
    return res;
}

MacroResult MacroRuntime::generateDrumPattern(sequencer::StepSequencer& sequencer,
                                             const std::string& style,
                                             uint32_t seed) {
    MacroResult res;
    size_t numTracks = sequencer.getNumTracks();
    if (numTracks == 0) {
        res.success = false;
        res.message = "No tracks available in sequencer.";
        return res;
    }

    // Track 0: Kick, Track 1: Snare / Clap, Track 2: Hi-Hat (if present)
    auto* kickTrack = sequencer.getTrack(0);
    if (kickTrack) {
        kickTrack->clear();
        for (uint32_t s = 0; s < 16; s += 4) {
            sequencer::StepData step;
            step.active = true;
            step.note = 36; // C1 Kick
            step.velocity = 0.95f;
            step.gateLength = 0.8f;
            step.accent = (s == 0);
            kickTrack->setStep(s, step);
        }
    }

    if (numTracks > 1) {
        auto* snareTrack = sequencer.getTrack(1);
        if (snareTrack) {
            snareTrack->clear();
            // Snare on 4 and 12 (0-indexed: steps 4 and 12)
            for (uint32_t s : {4u, 12u}) {
                sequencer::StepData step;
                step.active = true;
                step.note = 38; // D1 Snare
                step.velocity = 0.90f;
                step.gateLength = 0.7f;
                step.accent = true;
                snareTrack->setStep(s, step);
            }
        }
    }

    if (numTracks > 2) {
        auto* hatTrack = sequencer.getTrack(2);
        if (hatTrack) {
            hatTrack->clear();
            // Running 16th hats with dynamic velocity
            for (uint32_t s = 0; s < 16; ++s) {
                sequencer::StepData step;
                step.active = true;
                step.note = 42; // F#1 Closed Hat
                step.velocity = (s % 2 == 1) ? 0.85f : 0.60f; // Accent off-beats
                step.gateLength = 0.4f;
                hatTrack->setStep(s, step);
            }
        }
    }

    res.success = true;
    res.message = "Drum pattern generated.";
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
