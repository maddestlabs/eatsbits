#include "eatsbits/project/eats_builtin_presets.hpp"
#include "eatsbits/project/preset_loader.hpp"
#include "eatsbits/project/preset_manager.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits::project;

void testCatalogMetadata() {
    std::cout << "[Test 1] Testing BuiltinPresetRegistry Catalog Metadata..." << std::endl;

    const auto& catalog = BuiltinPresetRegistry::getCatalogInfo();
    assert(catalog.size() >= 30);

    for (const auto& info : catalog) {
        (void)info;
        assert(!info.id.empty());
        assert(!info.name.empty());
        assert(!info.description.empty());
        assert(!info.defaultIcon.empty());
    }

    std::cout << "  -> Catalog contains " << catalog.size() << " registered presets with verified metadata!" << std::endl;
}

void testPhysicalModelingPresets() {
    std::cout << "[Test 2] Testing Physical Modeling Instrument Instantiation..." << std::endl;

    auto physicalPresets = BuiltinPresetRegistry::getPhysicalModelingPresets();
    assert(physicalPresets.size() >= 25);

    for (const auto& p : physicalPresets) {
        assert(!p.metadata.id.empty());
        assert(!p.metadata.name.empty());
        assert(!p.params.empty());
        assert(p.guiRoot.type == GuiNodeType::Panel);
        assert(!p.guiRoot.children.empty());

        // Validate parameter boundaries
        for (const auto& [paramName, param] : p.params) {
            (void)paramName;
            assert(param.minVal <= param.maxVal);
            assert(param.defaultVal >= param.minVal && param.defaultVal <= param.maxVal);
        }
    }

    std::cout << "  -> Successfully generated " << physicalPresets.size() << " physical modeling instruments!" << std::endl;
}

void testFamilyFiltering() {
    std::cout << "[Test 3] Testing Preset Filtering by Physical Modeling Family..." << std::endl;

    auto keyboards = BuiltinPresetRegistry::getPresetsByFamily(PhysicalModelFamily::Keyboards);
    assert(keyboards.size() >= 5); // Grand, Upright, Honky-Tonk, Harpsichord, Clavinet

    auto plucked = BuiltinPresetRegistry::getPresetsByFamily(PhysicalModelFamily::PluckedStrings);
    assert(plucked.size() >= 7); // Spanish, Steel, 12-String, Lute, Banjo, Mandolin, Ukulele, Dobro

    auto bowed = BuiltinPresetRegistry::getPresetsByFamily(PhysicalModelFamily::BowedStrings);
    assert(bowed.size() >= 5); // Violin, Viola, Cello, Double Bass, String Ensemble

    auto tunedPerc = BuiltinPresetRegistry::getPresetsByFamily(PhysicalModelFamily::TunedPercussion);
    assert(tunedPerc.size() >= 6); // Glockenspiel, Bells, Vibraphone, Xylophone, Music Box, Agogo

    auto speech = BuiltinPresetRegistry::getPresetsByFamily(PhysicalModelFamily::VocalAndSpeech);
    assert(!speech.empty()); // TTS Voice Synth

    std::cout << "  -> Family grouping: Keyboards (" << keyboards.size() 
              << "), Plucked (" << plucked.size()
              << "), Bowed (" << bowed.size()
              << "), Tuned Perc (" << tunedPerc.size()
              << "), Speech (" << speech.size() << ") verified!" << std::endl;
}

void testPresetByIdLookup() {
    std::cout << "[Test 4] Testing createPresetById Canonical Lookup..." << std::endl;

    auto grand = BuiltinPresetRegistry::createPresetById("concert_grand_piano");
    assert(grand.has_value());
    assert(grand->metadata.name == "CONCERT GRAND PIANO");

    auto violin = BuiltinPresetRegistry::createPresetById("solo_violin");
    assert(violin.has_value());
    assert(violin->metadata.name == "Solo Violin");
    assert(violin->params.count("BowPressure") == 1);

    auto tts = BuiltinPresetRegistry::createPresetById("tts_voice_synth");
    assert(tts.has_value());
    assert(tts->metadata.name == "TTS Voice Synth");
    assert(tts->params.count("pitch") == 1);

    auto missing = BuiltinPresetRegistry::createPresetById("non_existent_preset_xyz");
    assert(!missing.has_value());

    std::cout << "  -> Canonical ID lookup verified!" << std::endl;
}

void testHardwareGuiLayoutBounds() {
    std::cout << "[Test 5] Testing Hardware GUI Tree Layout Calculation..." << std::endl;

    auto organ = BuiltinPresetRegistry::createPipeOrgan();
    assert(organ.guiRoot.type == GuiNodeType::Panel);

    // Compute layout on a standard 1200x650 canvas
    PresetLoader::computeLayoutBounds(organ.guiRoot, 40.0f, 75.0f, 1200.0f, 650.0f);
    assert(organ.guiRoot.boundsW == 1200.0f);
    assert(organ.guiRoot.boundsH == 650.0f);
    assert(!organ.guiRoot.children.empty());

    // Check row child bounds
    const auto& row = organ.guiRoot.children[0];
    (void)row;
    assert(row.type == GuiNodeType::Row);
    assert(row.boundsW > 0.0f);
    assert(row.boundsH > 0.0f);

    std::cout << "  -> GUI layout node geometry verified!" << std::endl;
}

void testFilesystemPresetDirectoryLoading() {
    std::cout << "[Test 6] Testing File-Based .eats Preset Directory Loading..." << std::endl;

    auto loadedPresets = PresetLoader::loadDirectory("presets/instruments");
    // Verify copied .eats presets are loaded from presets/instruments
    assert(loadedPresets.size() >= 30);

    bool foundAcousticBass = false;
    bool foundFlamenco = false;
    bool foundRhodes = false;

    for (const auto& p : loadedPresets) {
        if (p.metadata.id == "acoustic_bass") foundAcousticBass = true;
        if (p.metadata.id == "flamenco_guitar") foundFlamenco = true;
        if (p.metadata.id == "rhodes_epiano") foundRhodes = true;
    }

    assert(foundAcousticBass);
    assert(foundFlamenco);
    assert(foundRhodes);

    std::cout << "  -> Loaded " << loadedPresets.size() << " .eats presets from filesystem successfully!" << std::endl;
}

void testAcousticBassPhysicalModelPreset() {
    std::cout << "[Test 7] Testing Acoustic Bass Guitar Physical Model Preset..." << std::endl;

    auto bassOpt = BuiltinPresetRegistry::createPresetById("acoustic_bass");
    assert(bassOpt.has_value());
    const auto& bass = *bassOpt;

    assert(bass.metadata.id == "acoustic_bass");
    assert(bass.metadata.name == "ACOUSTIC BASS GUITAR");
    assert(bass.metadata.category == "instrument");
    assert(bass.metadata.engineId == "acoustic_bass");

    // Parameter checks
    assert(bass.params.count("PluckForce") == 1);
    assert(bass.params.count("NailClick") == 1);
    assert(bass.params.count("Sustain") == 1);
    assert(bass.params.count("Damping") == 1);
    assert(bass.params.count("AcousticAir") == 1);
    assert(bass.params.count("Drive") == 1);

    assert(bass.params.at("PluckForce").defaultVal == 1.30f);
    assert(bass.params.at("NailClick").defaultVal == 0.35f);
    assert(bass.params.at("Sustain").defaultVal == 0.995f);
    assert(bass.params.at("Damping").defaultVal == 0.28f);
    assert(bass.params.at("AcousticAir").defaultVal == 1.5f);
    assert(bass.params.at("Drive").defaultVal == 1.02f);

    // GUI checks
    assert(bass.guiRoot.type == GuiNodeType::Panel);
    assert(bass.guiRoot.title == "ACOUSTIC BASS GUITAR");
    assert(bass.guiRoot.subtitle == "Bronze-Wound String & Dreadnought Hollow Body");
    assert(bass.guiRoot.background == "blonde_pine");
    assert(bass.guiRoot.rackSides == "rosewood");
    assert(!bass.guiRoot.children.empty());

    // Script & DSP checks
    assert(bass.rawScript.find("def init():") != std::string::npos);
    assert(bass.rawScript.find("def gui():") != std::string::npos);
    assert(bass.rawScript.find("def graph():") != std::string::npos);
    assert(bass.rawScript.find("def process(") != std::string::npos);

    // Verify accurate modular nodes and absence of generic template
    assert(bass.rawScript.find("eat.node.pluck") != std::string::npos);
    assert(bass.rawScript.find("eat.node.waveguide") != std::string::npos);
    assert(bass.rawScript.find("eat.node.modal_bank") != std::string::npos);
    assert(bass.rawScript.find("eat.node.mixer") != std::string::npos);
    assert(bass.rawScript.find("eat.node.eq") != std::string::npos);
    assert(bass.rawScript.find("eat.node.preamp") != std::string::npos);
    assert(bass.rawScript.find("eat.node.out") != std::string::npos);

    // Must NOT be the generic fallback subtractive synth
    assert(bass.rawScript.find("SYNTH OSC") == std::string::npos);
    assert(bass.rawScript.find("LADDER FILTER") == std::string::npos);
    assert(bass.rawScript.find("VCA AMPLIFIER") == std::string::npos);

    // Verify filesystem loader also parsed acoustic_bass.eats properly
    PresetDefinition diskDef;
    bool loaded = PresetLoader::loadFromFile("presets/instruments/acoustic_bass.eats", diskDef);
    assert(loaded);
    assert(diskDef.params.size() >= 6);
    assert(diskDef.rawScript.find("def graph():") != std::string::npos);

    std::cout << "  -> Acoustic Bass Guitar physical model preset verified with authentic DSP graph!" << std::endl;
}

void testSingleDxviiEpianoPreset() {
    std::cout << "[Test 8] Testing DXVII E-Piano Uniqueness & Preset Manager..." << std::endl;
    PresetManager::instance().initialize();
    const auto& allPresets = PresetManager::instance().getAllPresets();

    int dxviiCount = 0;
    for (const auto& p : allPresets) {
        if (p.name == "DXVII E-Piano") {
            dxviiCount++;
            assert(p.id == "dxvii_epiano");
        }
    }
    assert(dxviiCount == 1);

    // Verify findPreset supports both dxvii_epiano and legacy dx7_epiano
    const auto* pItem = PresetManager::instance().findPreset("dxvii_epiano");
    assert(pItem != nullptr);
    assert(pItem->name == "DXVII E-Piano");

    const auto* pLegacy = PresetManager::instance().findPreset("dx7_epiano");
    assert(pLegacy != nullptr);
    assert(pLegacy->id == "dxvii_epiano");

    // Verify modern script content without buggy legacy params dict syntax
    PresetDefinition def;
    bool ok = PresetManager::instance().loadPresetDefinition("dxvii_epiano", def);
    assert(ok);
    assert(def.params.count("Algorithm") == 1);
    assert(def.rawScript.find("def init():") != std::string::npos);
    assert(def.rawScript.find("params['Algorithm']") == std::string::npos); // No buggy old script

    std::cout << "  -> Exactly 1 DXVII E-Piano preset verified without duplicates or legacy bugs!" << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << " Running Eatsbits Subsystem 9: Preset Library Tests" << std::endl;
    std::cout << "======================================================" << std::endl;

    testCatalogMetadata();
    testPhysicalModelingPresets();
    testFamilyFiltering();
    testPresetByIdLookup();
    testHardwareGuiLayoutBounds();
    testFilesystemPresetDirectoryLoading();
    testAcousticBassPhysicalModelPreset();
    testSingleDxviiEpianoPreset();

    std::cout << "======================================================" << std::endl;
    std::cout << " ALL 8 BUILT-IN PRESET TESTS PASSED (100%)!          " << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
