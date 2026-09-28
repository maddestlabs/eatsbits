#include "eatsbits/project/eats_builtin_presets.hpp"
#include "eatsbits/project/preset_loader.hpp"
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

    std::cout << "======================================================" << std::endl;
    std::cout << " ALL 6 BUILT-IN PRESET TESTS PASSED (100%)!          " << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
