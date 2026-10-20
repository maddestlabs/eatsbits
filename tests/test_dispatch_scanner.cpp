#include <iostream>
#include <cassert>
#include <algorithm>
#include "eatsbits/eatscript/dispatch_scanner.hpp"
#include "eatsbits/eatscript/api_manifest.hpp"

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
    assert(DependencyScanner::detectTarget(tbScript) == NativeDispatchTarget::TB303);

    // SID
    std::string sidScript = "SIDSynth = True\n# Commodore 64 SID";
    assert(DependencyScanner::detectTarget(sidScript) == NativeDispatchTarget::SID);

    // DX7 (Legacy boolean fallback & modern Eatscript directives)
    std::string dxScript = "DX7EPiano = True\ndef init(): pass";
    assert(DependencyScanner::detectTarget(dxScript) == NativeDispatchTarget::DX7);

    std::string dxModernEngine = "# @engine: eats_dx\ndef init(): pass";
    assert(DependencyScanner::detectTarget(dxModernEngine) == NativeDispatchTarget::DX7);

    std::string dxModernId = "# @id: dxvii_synth\ndef init(): pass";
    assert(DependencyScanner::detectTarget(dxModernId) == NativeDispatchTarget::DX7);

    // SNES
    std::string snesScript = "snesDsp = True\ndef init(): pass";
    assert(DependencyScanner::detectTarget(snesScript) == NativeDispatchTarget::SNES);

    // YM2612
    std::string ymScript = "ym2612 = True\ndef init(): pass";
    assert(DependencyScanner::detectTarget(ymScript) == NativeDispatchTarget::YM2612);

    // Grand Piano
    std::string pianoScript = "ConcertGrandPiano = True";
    assert(DependencyScanner::detectTarget(pianoScript) == NativeDispatchTarget::GrandPiano);

    // Upright Bass
    std::string bassScript = "DoubleBass = True";
    assert(DependencyScanner::detectTarget(bassScript) == NativeDispatchTarget::UprightBass);

    // Disabled flag
    std::string disabledScript = "Eats303 = False\ndef process(time, freq, note, params): return 0.0";
    assert(DependencyScanner::detectTarget(disabledScript) == NativeDispatchTarget::None);

    // Pure math script (no flag)
    std::string pureScript = R"(
def init():
    return {"Pitch": 440.0}

def process(time, freq, note, params):
    return math.sin(2.0 * math.pi * freq * time)
)";
    assert(DependencyScanner::detectTarget(pureScript) == NativeDispatchTarget::None);

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

    auto params = DependencyScanner::extractParameters(script);
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

    std::string s1 = DependencyScanner::targetToString(NativeDispatchTarget::TB303);
    assert(s1.find("303") != std::string::npos);

    std::string s2 = DependencyScanner::targetToString(NativeDispatchTarget::None);
    assert(s2.find("VM") != std::string::npos);

    std::cout << "  -> Passed target string formatting tests!" << std::endl;
}

void testApiManifestRegistry() {
    std::cout << "[Test] ApiManifest registry queries..." << std::endl;

    assert(ApiManifest::hasModule("eats.dsp.TB303"));
    assert(ApiManifest::hasModule("TB303"));
    assert(ApiManifest::hasModule("eat.dsp.DiodeLadder"));
    assert(ApiManifest::hasModule("DX7"));
    assert(ApiManifest::hasModule("eats.fx.ChorusFlanger"));
    assert(ApiManifest::hasModule("Convolver"));
    assert(!ApiManifest::hasModule("NonExistentModule"));

    const auto* tb303Mod = ApiManifest::findModule("TB303");
    assert(tb303Mod != nullptr);
    assert(tb303Mod->cppHeader == "eatsbits/audio/dsp/tb303_core.hpp");
    assert(tb303Mod->cppType == "eatsbits::dsp::Tb303Core");
    assert(tb303Mod->sources.empty()); // Header-only DSP

    const auto* dx7Mod = ApiManifest::findModule("DX7");
    assert(dx7Mod != nullptr);
    assert(dx7Mod->cppHeader == "eatsbits/audio/dsp/dx7_core.hpp");
    assert(!dx7Mod->sources.empty());
    assert(dx7Mod->sources[0] == "src/audio/dsp/dx7_core.cpp");

    std::cout << "  -> Passed ApiManifest registry tests!" << std::endl;
}

void testDependencyScannerDirectivesAndAST() {
    std::cout << "[Test] DependencyScanner AST tree-shaking & metadata directives..." << std::endl;

    std::string script = R"(
# @id: acid_trip_303
# @name: Acid Trip 303 Bass
# @vendor: Eatsbits Labs
# @version: 1.2.3
# @description: Authentic Roland TB-303 Acid Bassline with Chorus
# @category: instrument
# @engine: tb303

cutoff = eat.param("Cutoff", 20.0, 18000.0, 1400.0)
resonance = eat.param("Resonance", 0.0, 1.0, 0.6)
waveform = eat.param("Waveform", 0.0, 1.0, 0.0)

filter = eat.dsp.DiodeLadder()
env = eat.dsp.ADSR()
chorus = eat.fx.ChorusFlanger()
)";

    auto result = DependencyScanner::scan(script);

    // Verify Metadata Directives
    assert(result.metadata.id == "acid_trip_303");
    assert(result.metadata.name == "Acid Trip 303 Bass");
    assert(result.metadata.vendor == "Eatsbits Labs");
    assert(result.metadata.version == "1.2.3");
    assert(result.metadata.category == "instrument");
    assert(result.metadata.engine == "tb303");

    // Verify Dispatch Target
    assert(result.dispatchTarget == NativeDispatchTarget::TB303);

    // Verify Resolved Headers
    const auto& headers = result.dependencies.headers;
    assert(std::find(headers.begin(), headers.end(), "eatsbits/audio/dsp/tb303_core.hpp") != headers.end());
    assert(std::find(headers.begin(), headers.end(), "eatsbits/audio/dsp/adsr.hpp") != headers.end());
    assert(std::find(headers.begin(), headers.end(), "eatsbits/audio/dsp/chorus_flanger.hpp") != headers.end());

    // Verify Resolved Modules
    const auto& mods = result.dependencies.targetModules;
    assert(std::find(mods.begin(), mods.end(), "eats.dsp.TB303") != mods.end());
    assert(std::find(mods.begin(), mods.end(), "eats.dsp.DiodeLadder") != mods.end());
    assert(std::find(mods.begin(), mods.end(), "eats.dsp.ADSR") != mods.end());
    assert(std::find(mods.begin(), mods.end(), "eats.fx.ChorusFlanger") != mods.end());

    // Verify Preprocessor Definitions
    const auto& defs = result.dependencies.definitions;
    assert(std::find(defs.begin(), defs.end(), "EATS_EXPORT_CLAP=1") != defs.end());

    // Verify Harvested Parameters
    assert(result.parameters.size() == 3);
    auto pCutoff = std::find_if(result.parameters.begin(), result.parameters.end(),
        [](const ExtractedParam& p) { return p.name == "Cutoff"; });
    assert(pCutoff != result.parameters.end());
    assert(pCutoff->minVal == 20.0f);
    assert(pCutoff->maxVal == 18000.0f);
    assert(pCutoff->defaultVal == 1400.0f);

    // Verify Generated CLAP Parameters
    auto clapParams = result.generateClapParamDefs();
    assert(clapParams.size() == 3);
    assert(clapParams[0].name == "Cutoff");
    assert(clapParams[0].module == "Filter");
    assert(clapParams[0].flags == 0x20); // Automatable

    assert(clapParams[2].name == "Waveform");
    assert(clapParams[2].module == "Oscillator");
    assert((clapParams[2].flags & 0x01) != 0); // Stepped
    assert((clapParams[2].flags & 0x10000) != 0); // Enum

    std::cout << "  -> Passed DependencyScanner AST & directives tests!" << std::endl;
}

int main() {
    std::cout << "=================================================" << std::endl;
    std::cout << "Running Eatsbits Dependency Scanner & Manifest Tests" << std::endl;
    std::cout << "=================================================" << std::endl;

    testDispatchDetection();
    testParameterExtraction();
    testTargetToString();
    testApiManifestRegistry();
    testDependencyScannerDirectivesAndAST();

    std::cout << "=================================================" << std::endl;
    std::cout << "ALL DEPENDENCY SCANNER TESTS PASSED! (5/5)" << std::endl;
    std::cout << "=================================================" << std::endl;

    return 0;
}
