#include <iostream>
#include <cassert>
#include <cmath>
#include "eatsbits/eatscript/note_script.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"

using namespace eatsbits;
using namespace eatsbits::eatscript;
using namespace eatsbits::sequencer;

void testPitchConversion() {
    std::cout << "[Test] Pitch to string and string to pitch conversion..." << std::endl;

    assert(NoteScriptEngine::pitchToString(60) == "C4");
    assert(NoteScriptEngine::pitchToString(48) == "C3");
    assert(NoteScriptEngine::pitchToString(36) == "C2");
    assert(NoteScriptEngine::pitchToString(61) == "C#4");
    assert(NoteScriptEngine::pitchToString(71) == "B4");

    assert(NoteScriptEngine::stringToPitch("C4") == 60);
    assert(NoteScriptEngine::stringToPitch("c4") == 60);
    assert(NoteScriptEngine::stringToPitch("C3") == 48);
    assert(NoteScriptEngine::stringToPitch("C2") == 36);
    assert(NoteScriptEngine::stringToPitch("C#4") == 61);
    assert(NoteScriptEngine::stringToPitch("Db4") == 61);
    assert(NoteScriptEngine::stringToPitch("D#2") == 39);
    assert(NoteScriptEngine::stringToPitch("Eb2") == 39);
    assert(NoteScriptEngine::stringToPitch("60") == 60);
    assert(NoteScriptEngine::stringToPitch("48") == 48);
    assert(NoteScriptEngine::stringToPitch("invalid") == -1);
    assert(NoteScriptEngine::stringToPitch("") == -1);

    std::cout << "  -> Passed pitch conversion tests!" << std::endl;
}

void testSerializeTrackNotes() {
    std::cout << "[Test] Serialize track notes to Eatscript lines..." << std::endl;

    SequencerTrack track("Acid 303", 1, 16);

    StepData s0;
    s0.active = true;
    s0.note = 48; // C3
    s0.velocity = 0.85f;
    s0.gateLength = 0.75f;
    s0.slide = false;
    s0.accent = false;
    track.setStep(0, s0);

    StepData s3;
    s3.active = true;
    s3.note = 51; // D#3
    s3.velocity = 0.90f;
    s3.gateLength = 1.0f;
    s3.slide = true;
    s3.accent = true;
    track.setStep(3, s3);

    std::string script = NoteScriptEngine::serializeTrackNotes(track);
    assert(!script.empty());
    assert(script.find("Track: \"Acid 303\"") != std::string::npos);
    assert(script.find("pitch = \"C3\"") != std::string::npos);
    assert(script.find("step = 0") != std::string::npos);
    assert(script.find("pitch = \"D#3\"") != std::string::npos);
    assert(script.find("step = 3") != std::string::npos);
    assert(script.find("slide = true") != std::string::npos);
    assert(script.find("accent = true") != std::string::npos);

    std::cout << "  -> Serialized output:\n" << script << std::endl;
    std::cout << "  -> Passed serialization tests!" << std::endl;
}

void testParseTrackNotes() {
    std::cout << "[Test] Parse Eatscript notes into SequencerTrack..." << std::endl;

    std::string script = R"(
# Procedural Bassline Script
# Syntax: { pitch = "C3", step = 0, dur = 0.75, vel = 0.80, slide = false, accent = false }

{ pitch = "C3", step = 0, dur = 0.75, vel = 0.80, slide = false, accent = false }
{ pitch = "D#3", step = 2, dur = 0.50, vel = 0.90, slide = true, accent = true }
{ pitch = "F3", step = 4, dur = 0.80, vel = 0.85, slide = false, accent = false }
{ pitch = 55, step = 7, dur = 1.00, vel = 0.95, slide = true, accent = false } # G3
)";

    SequencerTrack track("Synth Lead", 2, 16);
    std::string err;
    int errLine = -1;

    bool ok = NoteScriptEngine::parseTrackNotes(script, track, &err, &errLine);
    if (!ok) {
        std::cerr << "Parse error on line " << errLine << ": " << err << std::endl;
    }
    assert(ok);

    const auto& st0 = track.getStep(0);
    assert(st0.active && st0.note == 48);
    assert(std::abs(st0.velocity - 0.80f) < 0.01f);
    assert(!st0.slide && !st0.accent);

    const auto& st2 = track.getStep(2);
    assert(st2.active && st2.note == 51);
    assert(st2.slide && st2.accent);

    const auto& st4 = track.getStep(4);
    assert(st4.active && st4.note == 53);

    const auto& st7 = track.getStep(7);
    assert(st7.active && st7.note == 55); // numeric 55 -> G3
    assert(st7.slide && !st7.accent);

    // Step 1 should be inactive
    assert(!track.getStep(1).active);

    std::cout << "  -> Passed parse tests!" << std::endl;
}

void testRoundtrip() {
    std::cout << "[Test] Full roundtrip: Track -> Serialize -> Parse -> Verify..." << std::endl;

    SequencerTrack trk1("Original", 1, 16);
    for (uint32_t s = 0; s < 16; s += 2) {
        StepData sd;
        sd.active = true;
        sd.note = static_cast<uint8_t>(40 + s);
        sd.velocity = 0.5f + static_cast<float>(s) * 0.03f;
        sd.gateLength = 0.6f;
        sd.slide = (s % 4 == 0);
        sd.accent = (s % 6 == 0);
        trk1.setStep(s, sd);
    }

    std::string serialized = NoteScriptEngine::serializeTrackNotes(trk1);
    SequencerTrack trk2("Parsed", 1, 16);
    std::string err;
    int errLine = 0;
    bool ok = NoteScriptEngine::parseTrackNotes(serialized, trk2, &err, &errLine);
    assert(ok);

    for (uint32_t s = 0; s < 16; ++s) {
        const auto& a = trk1.getStep(s);
        const auto& b = trk2.getStep(s);
        assert(a.active == b.active);
        if (a.active) {
            assert(a.note == b.note);
            assert(std::abs(a.velocity - b.velocity) < 0.02f);
            assert(std::abs(a.gateLength - b.gateLength) < 0.02f);
            assert(a.slide == b.slide);
            assert(a.accent == b.accent);
        }
    }

    std::cout << "  -> Passed roundtrip tests!" << std::endl;
}

void testErrorHandling() {
    std::cout << "[Test] Error handling on invalid note script syntax..." << std::endl;

    SequencerTrack track("Test", 1, 16);
    std::string err;
    int errLine = 0;

    // Missing step
    std::string badScript1 = "{ pitch = \"C3\", dur = 0.5 }";
    assert(!NoteScriptEngine::parseTrackNotes(badScript1, track, &err, &errLine));
    assert(err.find("missing 'step'") != std::string::npos);

    // Invalid pitch
    std::string badScript2 = "{ pitch = \"H99\", step = 1 }";
    assert(!NoteScriptEngine::parseTrackNotes(badScript2, track, &err, &errLine));
    assert(err.find("Invalid pitch") != std::string::npos);

    // Out of range step
    std::string badScript3 = "{ pitch = \"C3\", step = 999 }";
    assert(!NoteScriptEngine::parseTrackNotes(badScript3, track, &err, &errLine));
    assert(err.find("out of range") != std::string::npos);

    // Missing braces
    std::string badScript4 = "pitch = \"C3\", step = 1";
    assert(!NoteScriptEngine::parseTrackNotes(badScript4, track, &err, &errLine));
    assert(err.find("Expected '{ ... }'") != std::string::npos);

    std::cout << "  -> Passed error handling tests!" << std::endl;
}

void testTrackEatscriptCode() {
    std::cout << "[Test] Track Eatscript code association..." << std::endl;

    SequencerTrack track("Acid 303", 1, 16);
    assert(track.getEatscriptCode().empty());

    std::string acidScript = R"(
Eats303 = True
def init():
    return {"Cutoff": eat.param("Cutoff", 20.0, 20000.0, 1400.0)}
)";
    track.setEatscriptCode(acidScript);
    assert(track.getEatscriptCode() == acidScript);

    std::cout << "  -> Passed track Eatscript code association test!" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "Running Eatsbits Note Script Engine Test Suite" << std::endl;
    std::cout << "=================================================" << std::endl;

    testPitchConversion();
    testSerializeTrackNotes();
    testParseTrackNotes();
    testRoundtrip();
    testErrorHandling();
    testTrackEatscriptCode();

    std::cout << "=================================================" << std::endl;
    std::cout << "ALL NOTE SCRIPT TESTS PASSED SUCCESSFULLY! (6/6)" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
