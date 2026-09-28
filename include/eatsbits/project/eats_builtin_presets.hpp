#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <unordered_map>
#include "eatsbits/project/preset_loader.hpp"

namespace eatsbits::project {

/**
 * @brief Categorization for physical modeling and studio presets.
 */
enum class PresetCategory {
    Instrument,
    AudioFx,
    Drums,
    MidiFx,
    MidiSeq,
    Utility
};

enum class PhysicalModelFamily {
    Keyboards,       // Grand Piano, Upright, Honky-Tonk, Harpsichord, Clavinet
    PluckedStrings,  // Spanish Guitar, Steel Guitar, 12-String, Banjo, Mandolin, Ukulele, Lute
    BowedStrings,    // Violin, Viola, Cello, Double Bass, String Ensemble
    BassInstruments, // Upright Bass, Acoustic Bass, Fretless Bass
    TunedPercussion, // Glockenspiel, Vibraphone, Xylophone, Music Box, Bells
    WindAndOrgan,    // Pipe Organ, Flute, Reeds
    VocalAndSpeech,  // TTS Voice Synth, Formant Resonators
    ElectronicSynth, // TB-303, DX7, SID, SNES, YM2612
    EffectsAndOther  // Delays, Reverbs, Dynamic Processors
};

/**
 * @brief Metadata descriptor for a built-in or file-based preset.
 */
struct PresetInfo {
    std::string id;
    std::string name;
    PresetCategory category{PresetCategory::Instrument};
    PhysicalModelFamily family{PhysicalModelFamily::ElectronicSynth};
    std::string description;
    std::string author{"Eatsbits DSP Team"};
    std::string defaultIcon{"preset:inst_synth"};
};

/**
 * @brief Comprehensive registry of physical modeling instruments and built-in EatScript presets.
 * Exposes a catalog of physical acoustic instruments (pianos, bowed strings,
 * plucked guitars, organs, tuned percussion, and speech vocalizers).
 */
class BuiltinPresetRegistry {
public:
    /**
     * @brief Returns complete list of all cataloged preset descriptors.
     */
    static const std::vector<PresetInfo>& getCatalogInfo();

    /**
     * @brief Returns all physical modeling instrument presets.
     */
    static std::vector<PresetDefinition> getPhysicalModelingPresets();

    /**
     * @brief Finds and creates a preset definition by its canonical ID.
     */
    static std::optional<PresetDefinition> createPresetById(const std::string& id);

    /**
     * @brief Returns all presets belonging to a specific physical modeling family.
     */
    static std::vector<PresetDefinition> getPresetsByFamily(PhysicalModelFamily family);

    /**
     * @brief Returns all presets belonging to a general category.
     */
    static std::vector<PresetDefinition> getPresetsByCategory(PresetCategory category);

    // --- Individual Physical Modeling Instrument Builders ---
    static PresetDefinition createFeltUprightPiano();
    static PresetDefinition createHonkyTonkPiano();
    static PresetDefinition createHarpsichordCembalo();
    static PresetDefinition createClavinetD6();
    static PresetDefinition createTwelveStringGuitar();
    static PresetDefinition createRenaissanceLute();
    static PresetDefinition createBluegrassBanjo();
    static PresetDefinition createFolkMandolin();
    static PresetDefinition createHawaiianUkulele();
    static PresetDefinition createDobroResonator();
    static PresetDefinition createSoloViolin();
    static PresetDefinition createSoloViola();
    static PresetDefinition createSoloCello();
    static PresetDefinition createDoubleBass();
    static PresetDefinition createStringEnsemble();
    static PresetDefinition createFretlessBass();
    static PresetDefinition createPipeOrgan();
    static PresetDefinition createGlockenspiel();
    static PresetDefinition createTubularBells();
    static PresetDefinition createVibraphone();
    static PresetDefinition createXylophone();
    static PresetDefinition createMusicBox();
    static PresetDefinition createAgogoBell();
    static PresetDefinition createTtsVoiceSynth();
};

} // namespace eatsbits::project
