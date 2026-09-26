#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/project/eats_serializer.hpp"
#include "eatsbits/project/project_file.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include <cassert>
#include <iostream>

using namespace eatsbits::eatscript;
using namespace eatsbits::project;
using namespace eatsbits::audio;
using namespace eatsbits::sequencer;

void testMacroRuntimeExecution() {
    std::cout << "[Test 1] MacroRuntime execution..." << std::endl;
    MacroRuntime runtime;

    const auto& macros = MacroRuntime::getBuiltinMacros();
    assert(macros.size() >= 4);

    AudioEngine engine;
    StepSequencer seq;
    seq.addTrack("Acid Track", 0, 16);

    std::string script = R"(
# Macro Script Test
eat.daw.log("Starting macro test...")
eat.daw.set_tempo(142.0)
eat.daw.set_swing(0.58)
eat.daw.generate_acid(root=38, steps=16)
eat.daw.log("Macro test done.")
)";

    MacroResult res = runtime.execute(script, engine, seq);
    assert(res.success);
    assert(res.logs.size() == 5);
    assert(seq.getBpm() == 142.0);
    assert(seq.getSwing() == 0.58);

    // Track 0 should now have generated acid steps
    auto* trk = seq.getTrack(0);
    assert(trk != nullptr);
    int activeCount = 0;
    for (uint32_t s = 0; s < 16; ++s) {
        if (trk->getStep(s).active) activeCount++;
    }
    assert(activeCount > 0);
    std::cout << "  Passed (active acid steps generated: " << activeCount << ")." << std::endl;
}

void testMacroDrumGeneration() {
    std::cout << "[Test 2] Macro drum generation..." << std::endl;
    StepSequencer seq;
    seq.addTrack("Kick", 1, 16);
    seq.addTrack("Snare", 2, 16);
    seq.addTrack("Hat", 3, 16);

    MacroResult res = MacroRuntime::generateDrumPattern(seq, "Techno", 123);
    assert(res.success);

    // Kick on 0, 4, 8, 12
    auto* kick = seq.getTrack(0);
    assert(kick->getStep(0).active && kick->getStep(0).note == 36);
    assert(kick->getStep(4).active && kick->getStep(4).note == 36);
    assert(kick->getStep(8).active && kick->getStep(8).note == 36);
    assert(kick->getStep(12).active && kick->getStep(12).note == 36);

    // Snare on 4, 12
    auto* snare = seq.getTrack(1);
    assert(snare->getStep(4).active && snare->getStep(4).note == 38);
    assert(snare->getStep(12).active && snare->getStep(12).note == 38);

    // Hat on all 16 steps
    auto* hat = seq.getTrack(2);
    for (uint32_t s = 0; s < 16; ++s) {
        assert(hat->getStep(s).active && hat->getStep(s).note == 42);
    }
    std::cout << "  Passed." << std::endl;
}

void testEatsProjectSerializer() {
    std::cout << "[Test 3] EatsProjectSerializer .eats container roundtrip..." << std::endl;
    AudioGraph graph;
    StepSequencer seq;
    seq.setBpm(138.0);
    seq.setSwing(0.55);

    size_t tIdx = seq.addTrack("303 Bass", 0, 16);
    auto* trk = seq.getTrack(tIdx);
    assert(trk != nullptr);

    std::string scriptSource = "Eats303 = True\nCutoff = 2200.0\nResonance = 0.85";
    trk->setEatscriptCode(scriptSource);

    StepData s0;
    s0.active = true; s0.note = 36; s0.velocity = 0.95f; s0.slide = true; s0.accent = true;
    trk->setStep(0, s0);

    StepData s4;
    s4.active = true; s4.note = 48; s4.velocity = 0.80f; s4.slide = false; s4.accent = false;
    trk->setStep(4, s4);

    // Serialize to authentic .eats string
    std::string eatsContent = EatsProjectSerializer::serialize(graph, seq, "Cyber Acid", 138.0, 0.55);
    assert(EatsProjectSerializer::isEatsScriptFormat(eatsContent));
    assert(eatsContent.find("return eatsbeats.song {") != std::string::npos);
    assert(eatsContent.find("Cyber Acid") != std::string::npos);
    assert(eatsContent.find("Eats303 = True") != std::string::npos);

    // Deserialize into fresh state
    AudioGraph graph2;
    StepSequencer seq2;
    std::string outTitle;
    double outBpm = 0.0, outSwing = 0.0;

    bool ok = EatsProjectSerializer::deserialize(eatsContent, graph2, seq2, outTitle, outBpm, outSwing);
    assert(ok);
    assert(outTitle == "Cyber Acid");
    assert(outBpm == 138.0);
    assert(outSwing == 0.55);

    assert(seq2.getNumTracks() == 1);
    auto* trk2 = seq2.getTrack(0);
    assert(trk2 != nullptr);
    assert(trk2->getName() == "303 Bass");
    assert(trk2->getEatscriptCode() == scriptSource);
    assert(trk2->getStep(0).active && trk2->getStep(0).note == 36 && trk2->getStep(0).slide && trk2->getStep(0).accent);
    assert(trk2->getStep(4).active && trk2->getStep(4).note == 48 && !trk2->getStep(4).slide);

    std::cout << "  Passed." << std::endl;
}

void testProjectFileEatsFormatIntegration() {
    std::cout << "[Test 4] ProjectFile .eats file extension integration..." << std::endl;
    AudioGraph graph;
    StepSequencer seq;
    seq.setBpm(128.0);
    seq.addTrack("Synth Track", 0, 16);

    std::string tempPath = "test_project_temp.eats";
    bool saved = ProjectFile::saveToFile(tempPath, graph, seq, "Temp Eats Song", 128.0, 0.50);
    assert(saved);

    AudioGraph loadedGraph;
    StepSequencer loadedSeq;
    std::string loadedTitle;
    double loadedBpm = 0.0, loadedSwing = 0.0;

    bool loaded = ProjectFile::loadFromFile(tempPath, loadedGraph, loadedSeq, loadedTitle, loadedBpm, loadedSwing);
    assert(loaded);
    assert(loadedTitle == "Temp Eats Song");
    assert(loadedBpm == 128.0);
    assert(loadedSeq.getNumTracks() == 1);

    // Clean up temporary file
    std::remove(tempPath.c_str());
    std::cout << "  Passed." << std::endl;
}

int main() {
    std::cout << "=== Running Macro Runtime & .eats Format Tests ===" << std::endl;
    testMacroRuntimeExecution();
    testMacroDrumGeneration();
    testEatsProjectSerializer();
    testProjectFileEatsFormatIntegration();
    std::cout << "All Macro Runtime & .eats Format Tests Passed Successfully!" << std::endl;
    return 0;
}
