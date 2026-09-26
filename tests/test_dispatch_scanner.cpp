#include <iostream>
#include <cassert>
#include "eatsbits/eatscript/dispatch_scanner.hpp"

using namespace eatsbits::eatscript;

void testDispatchDetection() {
    std::cout << "[Test] Native dispatch flag detection..." << std::endl;

    // TB-303
    std::string tbScript = R"(
# @id: eats_tb303
# @name: Roland TB-303
Eats303 = True

def init():
    return {"Cutoff": eat.param("Cutoff", 20.0, 20000.0, 1400.0)}
)";
    assert(NativeDispatchScanner::detectTarget(tbScript) == NativeDispatchTarget::TB303);

    // SID
    std::string sidScript = "SIDSynth = True\n# Commodore 64 SID";
    assert(NativeDispatchScanner::detectTarget(sidScript) == NativeDispatchTarget::SID);

    // DX7
    std::string dxScript = "DX7EPiano = True\ndef init(): pass";
    assert(NativeDispatchScanner::detectTarget(dxScript) == NativeDispatchTarget::DX7);

    // SNES
    std::string snesScript = "snesDsp = True\ndef init(): pass";
    assert(NativeDispatchScanner::detectTarget(snesScript) == NativeDispatchTarget::SNES);

    // YM2612
    std::string ymScript = "ym2612 = True\ndef init(): pass";
    assert(NativeDispatchScanner::detectTarget(ymScript) == NativeDispatchTarget::YM2612);

    // Grand Piano
    std::string pianoScript = "ConcertGrandPiano = True";
    assert(NativeDispatchScanner::detectTarget(pianoScript) == NativeDispatchTarget::GrandPiano);

    // Upright Bass
    std::string bassScript = "DoubleBass = True";
    assert(NativeDispatchScanner::detectTarget(bassScript) == NativeDispatchTarget::UprightBass);

    // Disabled flag
    std::string disabledScript = "Eats303 = False\ndef process(time, freq, note, params): return 0.0";
    assert(NativeDispatchScanner::detectTarget(disabledScript) == NativeDispatchTarget::None);

    // Pure math script (no flag)
    std::string pureScript = R"(
def init():
    return {"Pitch": 440.0}

def process(time, freq, note, params):
    return math.sin(2.0 * math.pi * freq * time)
)";
    assert(NativeDispatchScanner::detectTarget(pureScript) == NativeDispatchTarget::None);

    std::cout << "  -> Passed dispatch detection tests!" << std::endl;
}

void testParameterExtraction() {
    std::cout << "[Test] Parameter extraction from def init()..." << std::endl;

    std::string script = R"(
Eats303 = True

def init():
    return {
        "Cutoff": eat.param("Cutoff", 20.0, 20000.0, 1500.0),
        "Resonance": eat.param("Resonance", 0.0, 1.0, 0.75),
        "Drive": eat.param("Drive", 0.5, 3.0, 1.2)
    }

def process(time, freq, note, params):
    return 0.0
)";

    auto params = NativeDispatchScanner::extractParameters(script);
    assert(params.size() == 3);
    assert(params.count("Cutoff") == 1);
    assert(params["Cutoff"].minVal == 20.0f);
    assert(params["Cutoff"].maxVal == 20000.0f);
    assert(params["Cutoff"].defaultVal == 1500.0f);

    assert(params.count("Resonance") == 1);
    assert(params["Resonance"].defaultVal == 0.75f);

    std::cout << "  -> Passed parameter extraction tests!" << std::endl;
}

void testTargetToString() {
    std::cout << "[Test] Target to display string formatting..." << std::endl;

    std::string s1 = NativeDispatchScanner::targetToString(NativeDispatchTarget::TB303);
    assert(s1.find("303") != std::string::npos);

    std::string s2 = NativeDispatchScanner::targetToString(NativeDispatchTarget::None);
    assert(s2.find("VM") != std::string::npos);

    std::cout << "  -> Passed target string formatting tests!" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "Running Eatsbits Native Dispatch Scanner Tests" << std::endl;
    std::cout << "=================================================" << std::endl;

    testDispatchDetection();
    testParameterExtraction();
    testTargetToString();

    std::cout << "=================================================" << std::endl;
    std::cout << "ALL DISPATCH SCANNER TESTS PASSED! (3/3)" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
