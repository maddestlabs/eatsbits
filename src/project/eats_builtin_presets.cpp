#include "eatsbits/project/eats_builtin_presets.hpp"
#include <algorithm>
#include <optional>

namespace eatsbits::project {

static const std::vector<PresetInfo> s_catalog = {
    // Keyboards
    {"concert_grand_piano", "Concert Grand Piano", PresetCategory::Instrument, PhysicalModelFamily::Keyboards, "Physically modeled 9-foot concert grand piano with spruce soundboard resonance and duplex scaling.", "Eatsbits DSP", "preset:inst_piano"},
    {"felt_upright_piano", "Felt Upright Piano", PresetCategory::Instrument, PhysicalModelFamily::Keyboards, "Intimate felt-damped upright piano with warm mechanical hammer noise and binaural perspective.", "Eatsbits DSP", "preset:inst_piano"},
    {"honky_tonk_piano", "Honky-Tonk Piano", PresetCategory::Instrument, PhysicalModelFamily::Keyboards, "Dual-unison detuned barroom piano with bright metallic tack hammer strikes.", "Eatsbits DSP", "preset:inst_piano"},
    {"harpsichord_cembalo", "Harpsichord Cembalo", PresetCategory::Instrument, PhysicalModelFamily::Keyboards, "Historical baroque cembalo with plectrum quill string pluck and metallic resonance.", "Eatsbits DSP", "preset:inst_piano"},
    {"clavinet_d6", "Clavinet D6", PresetCategory::Instrument, PhysicalModelFamily::Keyboards, "Electromechanical funk clavinet with rubber-tipped tangent strike and dual magnetic pickups.", "Eatsbits DSP", "preset:inst_piano"},

    // Plucked Strings
    {"spanish_guitar", "Spanish Nylon Guitar", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Classical nylon string guitar with Spanish fan-braced resonant cedar body cavity.", "Eatsbits DSP", "preset:inst_guitar"},
    {"steel_acoustic_guitar", "Steel Acoustic Guitar", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Phosphor bronze dreadnought acoustic guitar with body impulse resonance.", "Eatsbits DSP", "preset:inst_guitar"},
    {"twelve_string_guitar", "12-String Acoustic Guitar", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Rich 12-string guitar with octave & unison course doubling and natural chorus.", "Eatsbits DSP", "preset:inst_guitar"},
    {"renaissance_lute", "Renaissance Lute", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "8-course historical gut-string lute with delicate teardrop-shaped acoustic body.", "Eatsbits DSP", "preset:inst_guitar"},
    {"bluegrass_banjo", "Bluegrass 5-String Banjo", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Resonant mylar drumhead banjo with bright percussive attack and fingerpick twang.", "Eatsbits DSP", "preset:inst_guitar"},
    {"folk_mandolin", "Folk Mandolin", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Carved spruce archtop mandolin with paired unison steel strings and tremolo picking.", "Eatsbits DSP", "preset:inst_guitar"},
    {"hawaiian_ukulele", "Hawaiian Koa Ukulele", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Soprano ukulele with solid koa body, warm nylon strings, and tropical chime.", "Eatsbits DSP", "preset:inst_guitar"},
    {"dobro_resonator", "Dobro Resonator Guitar", PresetCategory::Instrument, PhysicalModelFamily::PluckedStrings, "Acoustic slide guitar with spun aluminum mechanical resonator cone.", "Eatsbits DSP", "preset:inst_guitar"},

    // Bowed Strings
    {"solo_violin", "Solo Violin", PresetCategory::Instrument, PhysicalModelFamily::BowedStrings, "Expressive solo violin with continuous bow friction stick-slip and maple body cavity.", "Eatsbits DSP", "preset:inst_strings"},
    {"solo_viola", "Solo Viola", PresetCategory::Instrument, PhysicalModelFamily::BowedStrings, "Warm tenor viola with dark woody acoustic resonance and bow rosin dynamics.", "Eatsbits DSP", "preset:inst_strings"},
    {"solo_cello", "Solo Cello", PresetCategory::Instrument, PhysicalModelFamily::BowedStrings, "Deep resonant cello with expressive vibrato and cello soundbox resonance.", "Eatsbits DSP", "preset:inst_strings"},
    {"double_bass", "Orchestral Double Bass", PresetCategory::Instrument, PhysicalModelFamily::BowedStrings, "Sub-harmonic contrabass with massive body cavity resonance and low bow rumble.", "Eatsbits DSP", "preset:inst_strings"},
    {"string_ensemble", "String Ensemble", PresetCategory::Instrument, PhysicalModelFamily::BowedStrings, "Polyphonic symphonic bowed string section with stereo room diffusion.", "Eatsbits DSP", "preset:inst_strings"},

    // Bass
    {"upright_bass", "Acoustic Upright Bass", PresetCategory::Instrument, PhysicalModelFamily::BassInstruments, "Pizzicato acoustic upright jazz bass with woody body thump and fingerboard clicks.", "Eatsbits DSP", "preset:inst_bass"},
    {"fretless_bass", "Fretless Bass", PresetCategory::Instrument, PhysicalModelFamily::BassInstruments, "Singing electric fretless bass with iconic 'mwah' midrange formant bloom.", "Eatsbits DSP", "preset:inst_bass"},

    // Wind & Organ
    {"pipe_organ", "Church Pipe Organ", PresetCategory::Instrument, PhysicalModelFamily::WindAndOrgan, "Great cathedral pipe organ with flue/reed stops and acoustic hall reverberation.", "Eatsbits DSP", "preset:inst_organ"},

    // Tuned Percussion & Bells
    {"glockenspiel", "Concert Glockenspiel", PresetCategory::Instrument, PhysicalModelFamily::TunedPercussion, "Hard steel tone bars with sparkling metallic overtones and crystal clarity.", "Eatsbits DSP", "preset:inst_bell"},
    {"tubular_bells", "Tubular Bells", PresetCategory::Instrument, PhysicalModelFamily::TunedPercussion, "Heavy brass orchestral chimes with inharmonic metal tube resonance.", "Eatsbits DSP", "preset:inst_bell"},
    {"vibraphone", "Concert Vibraphone", PresetCategory::Instrument, PhysicalModelFamily::TunedPercussion, "Aluminum alloy tone bars with motorized tremolo butterflies and resonator tubes.", "Eatsbits DSP", "preset:inst_bell"},
    {"xylophone", "Orchestral Xylophone", PresetCategory::Instrument, PhysicalModelFamily::TunedPercussion, "Honduran rosewood tuned bars struck with hard plastic mallets.", "Eatsbits DSP", "preset:inst_bell"},
    {"music_box", "Vintage Music Box", PresetCategory::Instrument, PhysicalModelFamily::TunedPercussion, "Plucked tuned steel comb teeth inside a small resonant wooden chest.", "Eatsbits DSP", "preset:inst_bell"},
    {"agogo_bell", "Latin Agogo Bell", PresetCategory::Instrument, PhysicalModelFamily::TunedPercussion, "Dual-pitched African/Brazilian forged steel bells with dry metallic punch.", "Eatsbits DSP", "preset:inst_bell"},

    // Speech & Formant
    {"tts_voice_synth", "TTS Voice Synth", PresetCategory::Instrument, PhysicalModelFamily::VocalAndSpeech, "Acoustic vocal tract formant synthesizer with Natural, Robot, and Whisper modes.", "Eatsbits DSP", "preset:inst_vocal"},

    // Electronic Synths
    {"eats_303", "Eats-303 Acid Bassline", PresetCategory::Instrument, PhysicalModelFamily::ElectronicSynth, "Authentic Roland TB-303 diode ladder filter emulation with accent and slide.", "Eatsbits DSP", "preset:inst_303"},
    {"c64_sid_synth", "C64 MOS 6581 SID Synth", PresetCategory::Instrument, PhysicalModelFamily::ElectronicSynth, "Commodore 64 chiptune synthesizer with wave combining, ring mod, and analog multi-mode filter.", "Eatsbits DSP", "preset:inst_synth"},
    {"dx7_epiano", "Yamaha DX7 FM E-Piano", PresetCategory::Instrument, PhysicalModelFamily::ElectronicSynth, "6-Operator classic 1983 frequency modulation electric piano.", "Eatsbits DSP", "preset:inst_epiano"},
    {"snes_console_synth", "SNES Sony SPC700 Console", PresetCategory::Instrument, PhysicalModelFamily::ElectronicSynth, "16-bit Super Nintendo 32kHz BRR sample synth with 8-tap FIR gaussian echo.", "Eatsbits DSP", "preset:inst_snes"},
    {"ym2612_synth", "Sega Genesis YM2612 FM", PresetCategory::Instrument, PhysicalModelFamily::ElectronicSynth, "Sega Genesis 4-Operator OPN2 FM synthesis with 8 routing algorithms.", "Eatsbits DSP", "preset:inst_genesis"}
};

const std::vector<PresetInfo>& BuiltinPresetRegistry::getCatalogInfo() {
    return s_catalog;
}

std::vector<PresetDefinition> BuiltinPresetRegistry::getPhysicalModelingPresets() {
    return {
        PresetLoader::createConcertGrandPianoPreset(),
        createFeltUprightPiano(),
        createHonkyTonkPiano(),
        createHarpsichordCembalo(),
        createClavinetD6(),
        PresetLoader::createSpanishGuitarPreset(),
        PresetLoader::createSteelAcousticGuitarPreset(),
        createTwelveStringGuitar(),
        createRenaissanceLute(),
        createBluegrassBanjo(),
        createFolkMandolin(),
        createHawaiianUkulele(),
        createDobroResonator(),
        createSoloViolin(),
        createSoloViola(),
        createSoloCello(),
        createDoubleBass(),
        createStringEnsemble(),
        PresetLoader::createUprightBassPreset(),
        createFretlessBass(),
        createPipeOrgan(),
        createGlockenspiel(),
        createTubularBells(),
        createVibraphone(),
        createXylophone(),
        createMusicBox(),
        createAgogoBell(),
        createTtsVoiceSynth()
    };
}

std::optional<PresetDefinition> BuiltinPresetRegistry::createPresetById(const std::string& id) {
    if (id == "concert_grand_piano") return PresetLoader::createConcertGrandPianoPreset();
    if (id == "felt_upright_piano") return createFeltUprightPiano();
    if (id == "honky_tonk_piano") return createHonkyTonkPiano();
    if (id == "harpsichord_cembalo") return createHarpsichordCembalo();
    if (id == "clavinet_d6") return createClavinetD6();

    if (id == "spanish_guitar") return PresetLoader::createSpanishGuitarPreset();
    if (id == "steel_acoustic_guitar") return PresetLoader::createSteelAcousticGuitarPreset();
    if (id == "twelve_string_guitar") return createTwelveStringGuitar();
    if (id == "renaissance_lute") return createRenaissanceLute();
    if (id == "bluegrass_banjo") return createBluegrassBanjo();
    if (id == "folk_mandolin") return createFolkMandolin();
    if (id == "hawaiian_ukulele") return createHawaiianUkulele();
    if (id == "dobro_resonator") return createDobroResonator();

    if (id == "solo_violin") return createSoloViolin();
    if (id == "solo_viola") return createSoloViola();
    if (id == "solo_cello") return createSoloCello();
    if (id == "double_bass") return createDoubleBass();
    if (id == "string_ensemble") return createStringEnsemble();

    if (id == "upright_bass") return PresetLoader::createUprightBassPreset();
    if (id == "fretless_bass") return createFretlessBass();

    if (id == "pipe_organ") return createPipeOrgan();

    if (id == "glockenspiel") return createGlockenspiel();
    if (id == "tubular_bells" || id == "tinkle_bell") return createTubularBells();
    if (id == "vibraphone") return createVibraphone();
    if (id == "xylophone") return createXylophone();
    if (id == "music_box") return createMusicBox();
    if (id == "agogo_bell") return createAgogoBell();

    if (id == "tts_voice_synth") return createTtsVoiceSynth();

    if (id == "eats_303") return PresetLoader::createEats303Preset();
    if (id == "c64_sid_synth") return PresetLoader::createC64SidPreset();
    if (id == "dx7_epiano") return PresetLoader::createYamahaDx7Preset();
    if (id == "snes_console_synth") return PresetLoader::createSnesPreset();
    if (id == "ym2612_synth") return PresetLoader::createYm2612Preset();

    return std::nullopt;
}

std::vector<PresetDefinition> BuiltinPresetRegistry::getPresetsByFamily(PhysicalModelFamily family) {
    std::vector<PresetDefinition> result;
    for (const auto& item : s_catalog) {
        if (item.family == family) {
            auto p = createPresetById(item.id);
            if (p) result.push_back(std::move(*p));
        }
    }
    return result;
}

std::vector<PresetDefinition> BuiltinPresetRegistry::getPresetsByCategory(PresetCategory category) {
    std::vector<PresetDefinition> result;
    for (const auto& item : s_catalog) {
        if (item.category == category) {
            auto p = createPresetById(item.id);
            if (p) result.push_back(std::move(*p));
        }
    }
    return result;
}

static GuiLayoutNode makeKnob(std::string param, std::string label, std::string unit, float size = 52.0f) {
    GuiLayoutNode k{};
    k.type = GuiNodeType::Knob;
    k.paramName = std::move(param);
    k.label = std::move(label);
    k.unit = std::move(unit);
    k.size = size;
    return k;
}

PresetDefinition BuiltinPresetRegistry::createFeltUprightPiano() {
    PresetDefinition p{};
    p.metadata.id = "felt_upright_piano";
    p.metadata.name = "Felt Upright Piano";
    p.metadata.category = "instruments";
    p.metadata.description = "Intimate felt-damped upright piano with warm mechanical hammer noise and binaural perspective.";

    p.params["Tone"] = {"Tone", 0.1f, 1.0f, 0.45f, 0.45f, 0.01f, "", true};
    p.params["FeltDamp"] = {"FeltDamp", 0.0f, 1.0f, 0.70f, 0.70f, 0.01f, "", true};
    p.params["HammerNoise"] = {"HammerNoise", 0.0f, 1.0f, 0.40f, 0.40f, 0.01f, "", true};
    p.params["RoomReverb"] = {"RoomReverb", 0.0f, 1.0f, 0.35f, 0.35f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "FELT UPRIGHT PIANO";
    p.guiRoot.subtitle = "Intimate Damped Acoustic Piano";
    p.guiRoot.accent = "#8B5A2B";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Tone", "TONE", ""));
    row.children.push_back(makeKnob("FeltDamp", "FELT", ""));
    row.children.push_back(makeKnob("HammerNoise", "HAMMER", ""));
    row.children.push_back(makeKnob("RoomReverb", "ROOM", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createHonkyTonkPiano() {
    PresetDefinition p{};
    p.metadata.id = "honky_tonk_piano";
    p.metadata.name = "Honky-Tonk Piano";
    p.metadata.category = "instruments";
    p.metadata.description = "Dual-unison detuned barroom piano with bright metallic tack hammer strikes.";

    p.params["Detune"] = {"Detune", 0.0f, 1.0f, 0.65f, 0.65f, 0.01f, "ct", true};
    p.params["TackBrite"] = {"TackBrite", 0.0f, 1.0f, 0.75f, 0.75f, 0.01f, "", true};
    p.params["Body"] = {"Body", 0.0f, 1.0f, 0.50f, 0.50f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.2f, 4.0f, 1.8f, 1.8f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "HONKY-TONK PIANO";
    p.guiRoot.subtitle = "Barroom Tack-Hammer Detuned Acoustic";
    p.guiRoot.accent = "#D4AF37";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Detune", "DETUNE", "ct"));
    row.children.push_back(makeKnob("TackBrite", "BRIGHT", ""));
    row.children.push_back(makeKnob("Body", "BODY", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createHarpsichordCembalo() {
    PresetDefinition p{};
    p.metadata.id = "harpsichord_cembalo";
    p.metadata.name = "Harpsichord Cembalo";
    p.metadata.category = "instruments";
    p.metadata.description = "Historical baroque cembalo with plectrum quill string pluck and metallic resonance.";

    p.params["PluckForce"] = {"PluckForce", 0.1f, 1.0f, 0.8f, 0.8f, 0.01f, "", true};
    p.params["QuillSnap"] = {"QuillSnap", 0.0f, 1.0f, 0.6f, 0.6f, 0.01f, "", true};
    p.params["Resonance"] = {"Resonance", 0.1f, 1.0f, 0.7f, 0.7f, 0.01f, "", true};
    p.params["ReleaseNoise"] = {"ReleaseNoise", 0.0f, 1.0f, 0.3f, 0.3f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "HARPSICHORD CEMBALO";
    p.guiRoot.subtitle = "Baroque Quill-Plucked Plectrum Waveguide";
    p.guiRoot.accent = "#C5A059";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("PluckForce", "PLUCK", ""));
    row.children.push_back(makeKnob("QuillSnap", "SNAP", ""));
    row.children.push_back(makeKnob("Resonance", "RESON", ""));
    row.children.push_back(makeKnob("ReleaseNoise", "RELEASE", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createClavinetD6() {
    PresetDefinition p{};
    p.metadata.id = "clavinet_d6";
    p.metadata.name = "Clavinet D6";
    p.metadata.category = "instruments";
    p.metadata.description = "Electromechanical funk clavinet with rubber-tipped tangent strike and dual magnetic pickups.";

    p.params["Drive"] = {"Drive", 1.0f, 5.0f, 1.5f, 1.5f, 0.05f, "x", true};
    p.params["PickupM"] = {"PickupM", 0.0f, 1.0f, 0.7f, 0.7f, 0.01f, "", true};
    p.params["Brilliance"] = {"Brilliance", 0.1f, 2.0f, 1.2f, 1.2f, 0.05f, "", true};
    p.params["WahEnv"] = {"WahEnv", 0.0f, 1.0f, 0.5f, 0.5f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "CLAVINET D6";
    p.guiRoot.subtitle = "Electromechanical Tangent String Model";
    p.guiRoot.accent = "#E05A2B";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Drive", "DRIVE", "x"));
    row.children.push_back(makeKnob("PickupM", "PICKUP", ""));
    row.children.push_back(makeKnob("Brilliance", "BRILL", ""));
    row.children.push_back(makeKnob("WahEnv", "WAH", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createTwelveStringGuitar() {
    PresetDefinition p{};
    p.metadata.id = "twelve_string_guitar";
    p.metadata.name = "12-String Acoustic Guitar";
    p.metadata.category = "instruments";
    p.metadata.description = "Rich 12-string guitar with octave & unison course doubling and natural chorus.";

    p.params["OctaveSpread"] = {"OctaveSpread", 0.0f, 1.0f, 0.8f, 0.8f, 0.01f, "", true};
    p.params["BodyAir"] = {"BodyAir", 0.0f, 1.0f, 0.55f, 0.55f, 0.01f, "", true};
    p.params["PickHardness"] = {"PickHardness", 0.1f, 1.0f, 0.7f, 0.7f, 0.01f, "", true};
    p.params["Chorusing"] = {"Chorusing", 0.0f, 1.0f, 0.6f, 0.6f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "12-STRING ACOUSTIC GUITAR";
    p.guiRoot.subtitle = "Double-Course Octave Waveguide Model";
    p.guiRoot.accent = "#C19A6B";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("OctaveSpread", "OCTAVE", ""));
    row.children.push_back(makeKnob("BodyAir", "BODY", ""));
    row.children.push_back(makeKnob("PickHardness", "PICK", ""));
    row.children.push_back(makeKnob("Chorusing", "CHORUS", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createRenaissanceLute() {
    PresetDefinition p{};
    p.metadata.id = "renaissance_lute";
    p.metadata.name = "Renaissance Lute";
    p.metadata.category = "instruments";
    p.metadata.description = "8-course historical gut-string lute with delicate teardrop-shaped acoustic body.";

    p.params["GutWarmth"] = {"GutWarmth", 0.1f, 1.0f, 0.75f, 0.75f, 0.01f, "", true};
    p.params["BodyResonance"] = {"BodyResonance", 0.1f, 1.0f, 0.60f, 0.60f, 0.01f, "", true};
    p.params["PluckAngle"] = {"PluckAngle", 0.0f, 1.0f, 0.40f, 0.40f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "RENAISSANCE LUTE";
    p.guiRoot.subtitle = "Teardrop Gut-String Acoustic Model";
    p.guiRoot.accent = "#CD853F";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("GutWarmth", "WARMTH", ""));
    row.children.push_back(makeKnob("BodyResonance", "RESON", ""));
    row.children.push_back(makeKnob("PluckAngle", "ANGLE", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createBluegrassBanjo() {
    PresetDefinition p{};
    p.metadata.id = "bluegrass_banjo";
    p.metadata.name = "Bluegrass 5-String Banjo";
    p.metadata.category = "instruments";
    p.metadata.description = "Resonant mylar drumhead banjo with bright percussive attack and fingerpick twang.";

    p.params["HeadTension"] = {"HeadTension", 0.1f, 1.0f, 0.85f, 0.85f, 0.01f, "", true};
    p.params["Twang"] = {"Twang", 0.1f, 2.0f, 1.30f, 1.30f, 0.05f, "", true};
    p.params["Decay"] = {"Decay", 0.1f, 2.0f, 0.60f, 0.60f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "BLUEGRASS BANJO";
    p.guiRoot.subtitle = "Mylar Membrane & String Waveguide";
    p.guiRoot.accent = "#E6C280";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("HeadTension", "TENSION", ""));
    row.children.push_back(makeKnob("Twang", "TWANG", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createFolkMandolin() {
    PresetDefinition p{};
    p.metadata.id = "folk_mandolin";
    p.metadata.name = "Folk Mandolin";
    p.metadata.category = "instruments";
    p.metadata.description = "Carved spruce archtop mandolin with paired unison steel strings and tremolo picking.";

    p.params["Chime"] = {"Chime", 0.1f, 1.0f, 0.70f, 0.70f, 0.01f, "", true};
    p.params["BodyAir"] = {"BodyAir", 0.1f, 1.0f, 0.50f, 0.50f, 0.01f, "", true};
    p.params["PickTransient"] = {"PickTransient", 0.1f, 1.0f, 0.80f, 0.80f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "FOLK MANDOLIN";
    p.guiRoot.subtitle = "Double-Course Carved Archtop Model";
    p.guiRoot.accent = "#8B4513";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Chime", "CHIME", ""));
    row.children.push_back(makeKnob("BodyAir", "BODY", ""));
    row.children.push_back(makeKnob("PickTransient", "PICK", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createHawaiianUkulele() {
    PresetDefinition p{};
    p.metadata.id = "hawaiian_ukulele";
    p.metadata.name = "Hawaiian Koa Ukulele";
    p.metadata.category = "instruments";
    p.metadata.description = "Soprano ukulele with solid koa body, warm nylon strings, and tropical chime.";

    p.params["Warmth"] = {"Warmth", 0.1f, 1.0f, 0.65f, 0.65f, 0.01f, "", true};
    p.params["KoaBody"] = {"KoaBody", 0.1f, 1.0f, 0.70f, 0.70f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.2f, 3.0f, 1.20f, 1.20f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "HAWAIIAN UKULELE";
    p.guiRoot.subtitle = "Soprano Solid Koa Waveguide Model";
    p.guiRoot.accent = "#DAA520";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Warmth", "WARMTH", ""));
    row.children.push_back(makeKnob("KoaBody", "KOA", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createDobroResonator() {
    PresetDefinition p{};
    p.metadata.id = "dobro_resonator";
    p.metadata.name = "Dobro Resonator Guitar";
    p.metadata.category = "instruments";
    p.metadata.description = "Acoustic slide guitar with spun aluminum mechanical resonator cone.";

    p.params["ConeResonance"] = {"ConeResonance", 0.1f, 1.0f, 0.80f, 0.80f, 0.01f, "", true};
    p.params["SlideTone"] = {"SlideTone", 0.1f, 2.0f, 1.20f, 1.20f, 0.05f, "", true};
    p.params["Decay"] = {"Decay", 0.3f, 4.0f, 2.20f, 2.20f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "DOBRO RESONATOR";
    p.guiRoot.subtitle = "Spun Aluminum Cone Slide Waveguide";
    p.guiRoot.accent = "#A9A9A9";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("ConeResonance", "CONE", ""));
    row.children.push_back(makeKnob("SlideTone", "SLIDE", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createSoloViolin() {
    PresetDefinition p{};
    p.metadata.id = "solo_violin";
    p.metadata.name = "Solo Violin";
    p.metadata.category = "instruments";
    p.metadata.description = "Expressive solo violin with continuous bow friction stick-slip and maple body cavity.";

    p.params["BowPressure"] = {"BowPressure", 0.1f, 2.0f, 0.85f, 0.85f, 0.05f, "", true};
    p.params["RosinFriction"] = {"RosinFriction", 0.1f, 1.0f, 0.65f, 0.65f, 0.01f, "", true};
    p.params["VibratoDepth"] = {"VibratoDepth", 0.0f, 1.0f, 0.40f, 0.40f, 0.01f, "", true};
    p.params["BodyAcoustic"] = {"BodyAcoustic", 0.1f, 1.0f, 0.70f, 0.70f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "SOLO VIOLIN";
    p.guiRoot.subtitle = "Continuous Stick-Slip Bowed String";
    p.guiRoot.accent = "#B22222";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("BowPressure", "PRESSURE", ""));
    row.children.push_back(makeKnob("RosinFriction", "ROSIN", ""));
    row.children.push_back(makeKnob("VibratoDepth", "VIBRATO", ""));
    row.children.push_back(makeKnob("BodyAcoustic", "BODY", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createSoloViola() {
    PresetDefinition p{};
    p.metadata.id = "solo_viola";
    p.metadata.name = "Solo Viola";
    p.metadata.category = "instruments";
    p.metadata.description = "Warm tenor viola with dark woody acoustic resonance and bow rosin dynamics.";

    p.params["Warmth"] = {"Warmth", 0.1f, 1.0f, 0.80f, 0.80f, 0.01f, "", true};
    p.params["BowPressure"] = {"BowPressure", 0.1f, 2.0f, 0.90f, 0.90f, 0.05f, "", true};
    p.params["Vibrato"] = {"Vibrato", 0.0f, 1.0f, 0.35f, 0.35f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "SOLO VIOLA";
    p.guiRoot.subtitle = "Warm Tenor Bowed String Model";
    p.guiRoot.accent = "#800000";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Warmth", "WARMTH", ""));
    row.children.push_back(makeKnob("BowPressure", "BOW", ""));
    row.children.push_back(makeKnob("Vibrato", "VIBRATO", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createSoloCello() {
    PresetDefinition p{};
    p.metadata.id = "solo_cello";
    p.metadata.name = "Solo Cello";
    p.metadata.category = "instruments";
    p.metadata.description = "Deep resonant cello with expressive vibrato and cello soundbox resonance.";

    p.params["BodyResonance"] = {"BodyResonance", 0.1f, 1.0f, 0.85f, 0.85f, 0.01f, "", true};
    p.params["BowAttack"] = {"BowAttack", 0.1f, 1.0f, 0.60f, 0.60f, 0.01f, "", true};
    p.params["Vibrato"] = {"Vibrato", 0.0f, 1.0f, 0.50f, 0.50f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "SOLO CELLO";
    p.guiRoot.subtitle = "Deep Resonant Cello Acoustic Cavity";
    p.guiRoot.accent = "#5B3A29";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("BodyResonance", "RESON", ""));
    row.children.push_back(makeKnob("BowAttack", "ATTACK", ""));
    row.children.push_back(makeKnob("Vibrato", "VIBRATO", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createDoubleBass() {
    PresetDefinition p{};
    p.metadata.id = "double_bass";
    p.metadata.name = "Orchestral Double Bass";
    p.metadata.category = "instruments";
    p.metadata.description = "Sub-harmonic contrabass with massive body cavity resonance and low bow rumble.";

    p.params["SubWeight"] = {"SubWeight", 0.1f, 1.0f, 0.90f, 0.90f, 0.01f, "", true};
    p.params["BowGrit"] = {"BowGrit", 0.1f, 1.0f, 0.70f, 0.70f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.5f, 5.0f, 2.50f, 2.50f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "DOUBLE BASS";
    p.guiRoot.subtitle = "Symphonic Contrabass Bowed Model";
    p.guiRoot.accent = "#3E2723";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("SubWeight", "WEIGHT", ""));
    row.children.push_back(makeKnob("BowGrit", "GRIT", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createStringEnsemble() {
    PresetDefinition p{};
    p.metadata.id = "string_ensemble";
    p.metadata.name = "String Ensemble";
    p.metadata.category = "instruments";
    p.metadata.description = "Polyphonic symphonic bowed string section with stereo room diffusion.";

    p.params["EnsembleWidth"] = {"EnsembleWidth", 0.0f, 1.0f, 0.85f, 0.85f, 0.01f, "", true};
    p.params["AttackTime"] = {"AttackTime", 0.05f, 2.0f, 0.35f, 0.35f, 0.05f, "s", true};
    p.params["HallSpace"] = {"HallSpace", 0.0f, 1.0f, 0.60f, 0.60f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "STRING ENSEMBLE";
    p.guiRoot.subtitle = "Symphonic Section Multi-Voice Model";
    p.guiRoot.accent = "#795548";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("EnsembleWidth", "WIDTH", ""));
    row.children.push_back(makeKnob("AttackTime", "ATTACK", "s"));
    row.children.push_back(makeKnob("HallSpace", "HALL", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createFretlessBass() {
    PresetDefinition p{};
    p.metadata.id = "fretless_bass";
    p.metadata.name = "Fretless Bass";
    p.metadata.category = "instruments";
    p.metadata.description = "Singing electric fretless bass with iconic 'mwah' midrange formant bloom.";

    p.params["Mwah"] = {"Mwah", 0.0f, 1.0f, 0.75f, 0.75f, 0.01f, "", true};
    p.params["Growl"] = {"Growl", 0.1f, 1.0f, 0.65f, 0.65f, 0.01f, "", true};
    p.params["FingerClick"] = {"FingerClick", 0.0f, 1.0f, 0.30f, 0.30f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "FRETLESS BASS";
    p.guiRoot.subtitle = "Singing Electric Midrange Bloom Model";
    p.guiRoot.accent = "#607D8B";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Mwah", "MWAH", ""));
    row.children.push_back(makeKnob("Growl", "GROWL", ""));
    row.children.push_back(makeKnob("FingerClick", "CLICK", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createPipeOrgan() {
    PresetDefinition p{};
    p.metadata.id = "pipe_organ";
    p.metadata.name = "Church Pipe Organ";
    p.metadata.category = "instruments";
    p.metadata.description = "Great cathedral pipe organ with flue/reed stops and acoustic hall reverberation.";

    p.params["Principal8"] = {"Principal8", 0.0f, 1.0f, 0.8f, 0.8f, 0.01f, "", true};
    p.params["Octave4"] = {"Octave4", 0.0f, 1.0f, 0.6f, 0.6f, 0.01f, "", true};
    p.params["Mixture"] = {"Mixture", 0.0f, 1.0f, 0.4f, 0.4f, 0.01f, "", true};
    p.params["WindAir"] = {"WindAir", 0.0f, 1.0f, 0.25f, 0.25f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "CHURCH PIPE ORGAN";
    p.guiRoot.subtitle = "Great Flue & Reed Acoustic Wind Model";
    p.guiRoot.accent = "#8B7355";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("Principal8", "8' DIAP", ""));
    row.children.push_back(makeKnob("Octave4", "4' OCT", ""));
    row.children.push_back(makeKnob("Mixture", "MIXTURE", ""));
    row.children.push_back(makeKnob("WindAir", "WIND", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createGlockenspiel() {
    PresetDefinition p{};
    p.metadata.id = "glockenspiel";
    p.metadata.name = "Concert Glockenspiel";
    p.metadata.category = "instruments";
    p.metadata.description = "Hard steel tone bars with sparkling metallic overtones and crystal clarity.";

    p.params["MalletHardness"] = {"MalletHardness", 0.1f, 1.0f, 0.9f, 0.9f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.2f, 4.0f, 1.8f, 1.8f, 0.05f, "s", true};
    p.params["Sparkle"] = {"Sparkle", 0.1f, 2.0f, 1.2f, 1.2f, 0.05f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "CONCERT GLOCKENSPIEL";
    p.guiRoot.subtitle = "Tuned Steel Bar Acoustic Waveguide";
    p.guiRoot.accent = "#00CED1";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("MalletHardness", "MALLET", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));
    row.children.push_back(makeKnob("Sparkle", "SPARKLE", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createTubularBells() {
    PresetDefinition p{};
    p.metadata.id = "tubular_bells";
    p.metadata.name = "Tubular Bells";
    p.metadata.category = "instruments";
    p.metadata.description = "Heavy brass orchestral chimes with inharmonic metal tube resonance.";

    p.params["StrikeForce"] = {"StrikeForce", 0.1f, 1.0f, 0.85f, 0.85f, 0.01f, "", true};
    p.params["Inharmonics"] = {"Inharmonics", 0.0f, 1.0f, 0.65f, 0.65f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.5f, 8.0f, 3.5f, 3.5f, 0.1f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "TUBULAR BELLS";
    p.guiRoot.subtitle = "Symphonic Brass Chime Modal Resonator";
    p.guiRoot.accent = "#D4AF37";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("StrikeForce", "STRIKE", ""));
    row.children.push_back(makeKnob("Inharmonics", "INHARM", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createVibraphone() {
    PresetDefinition p{};
    p.metadata.id = "vibraphone";
    p.metadata.name = "Concert Vibraphone";
    p.metadata.category = "instruments";
    p.metadata.description = "Aluminum alloy tone bars with motorized tremolo butterflies and resonator tubes.";

    p.params["MotorSpeed"] = {"MotorSpeed", 0.0f, 8.0f, 4.5f, 4.5f, 0.1f, "Hz", true};
    p.params["MotorDepth"] = {"MotorDepth", 0.0f, 1.0f, 0.65f, 0.65f, 0.01f, "", true};
    p.params["BarWarmth"] = {"BarWarmth", 0.1f, 1.0f, 0.70f, 0.70f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.5f, 6.0f, 3.0f, 3.0f, 0.1f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "CONCERT VIBRAPHONE";
    p.guiRoot.subtitle = "Aluminum Bar Motor Tremolo Model";
    p.guiRoot.accent = "#4682B4";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("MotorSpeed", "SPEED", "Hz"));
    row.children.push_back(makeKnob("MotorDepth", "DEPTH", ""));
    row.children.push_back(makeKnob("BarWarmth", "WARMTH", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createXylophone() {
    PresetDefinition p{};
    p.metadata.id = "xylophone";
    p.metadata.name = "Orchestral Xylophone";
    p.metadata.category = "instruments";
    p.metadata.description = "Honduran rosewood tuned bars struck with hard plastic mallets.";

    p.params["RosewoodWood"] = {"RosewoodWood", 0.1f, 1.0f, 0.85f, 0.85f, 0.01f, "", true};
    p.params["MalletClick"] = {"MalletClick", 0.1f, 1.0f, 0.75f, 0.75f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.1f, 2.0f, 0.45f, 0.45f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "ORCHESTRAL XYLOPHONE";
    p.guiRoot.subtitle = "Rosewood Tuned Bar Acoustic Waveguide";
    p.guiRoot.accent = "#8B4513";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("RosewoodWood", "WOOD", ""));
    row.children.push_back(makeKnob("MalletClick", "CLICK", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createMusicBox() {
    PresetDefinition p{};
    p.metadata.id = "music_box";
    p.metadata.name = "Vintage Music Box";
    p.metadata.category = "instruments";
    p.metadata.description = "Plucked tuned steel comb teeth inside a small resonant wooden chest.";

    p.params["PluckTone"] = {"PluckTone", 0.1f, 1.0f, 0.80f, 0.80f, 0.01f, "", true};
    p.params["BoxResonance"] = {"BoxResonance", 0.1f, 1.0f, 0.60f, 0.60f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.2f, 3.5f, 1.50f, 1.50f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "VINTAGE MUSIC BOX";
    p.guiRoot.subtitle = "Steel Comb Tooth Waveguide Model";
    p.guiRoot.accent = "#9370DB";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("PluckTone", "PLUCK", ""));
    row.children.push_back(makeKnob("BoxResonance", "CHEST", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createAgogoBell() {
    PresetDefinition p{};
    p.metadata.id = "agogo_bell";
    p.metadata.name = "Latin Agogo Bell";
    p.metadata.category = "instruments";
    p.metadata.description = "Dual-pitched African/Brazilian forged steel bells with dry metallic punch.";

    p.params["PitchSplit"] = {"PitchSplit", 0.1f, 1.0f, 0.50f, 0.50f, 0.01f, "", true};
    p.params["MetalRing"] = {"MetalRing", 0.1f, 1.0f, 0.65f, 0.65f, 0.01f, "", true};
    p.params["Decay"] = {"Decay", 0.05f, 1.5f, 0.35f, 0.35f, 0.05f, "s", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "LATIN AGOGO BELL";
    p.guiRoot.subtitle = "Dual Forged Steel Bell Modal Model";
    p.guiRoot.accent = "#FF8C00";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("PitchSplit", "PITCH", ""));
    row.children.push_back(makeKnob("MetalRing", "RING", ""));
    row.children.push_back(makeKnob("Decay", "DECAY", "s"));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

PresetDefinition BuiltinPresetRegistry::createTtsVoiceSynth() {
    PresetDefinition p{};
    p.metadata.id = "tts_voice_synth";
    p.metadata.name = "TTS Voice Synth";
    p.metadata.category = "instruments";
    p.metadata.description = "Tactile minimalist vocal formant synthesizer and speech engine with 3-column ceramic layout: glottal pulse oscillator, vocal tract F1/F2/F3 formant filters, breath air turbulence, prosody dynamics, and analog character saturation.";

    p.params["pitch"] = {"pitch", 0.25f, 4.0f, 1.0f, 1.0f, 0.01f, "", true};
    p.params["tone"] = {"tone", 0.1f, 3.0f, 1.0f, 1.0f, 0.05f, "", true};
    p.params["volume"] = {"volume", 0.0f, 2.0f, 1.0f, 1.0f, 0.01f, "", true};
    p.params["space"] = {"space", 0.0f, 1.0f, 0.3f, 0.3f, 0.01f, "", true};
    p.params["air"] = {"air", 0.0f, 1.0f, 0.15f, 0.15f, 0.01f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "TTS VOICE SYNTH";
    p.guiRoot.subtitle = "Vocal Speech & Formant Synthesizer";
    p.guiRoot.accent = "#D9603B";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.children.push_back(makeKnob("pitch", "PITCH", ""));
    row.children.push_back(makeKnob("tone", "TONE", ""));
    row.children.push_back(makeKnob("volume", "LEVEL", "", 64.0f));
    row.children.push_back(makeKnob("space", "SPACE", ""));
    row.children.push_back(makeKnob("air", "AIR", ""));

    p.guiRoot.children.push_back(row);
    p.compactGuiRoot = p.guiRoot;
    return p;
}

} // namespace eatsbits::project
