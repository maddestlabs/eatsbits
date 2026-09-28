#include "eatsbits/procgen/song_archetypes.hpp"
#include <algorithm>

namespace eatsbits::procgen {

const char* getFunctionalRoleName(FunctionalRole role) noexcept {
    switch (role) {
        case FunctionalRole::Rhythm: return "Rhythm";
        case FunctionalRole::Foundation: return "Foundation";
        case FunctionalRole::HarmonicTexture: return "HarmonicTexture";
        case FunctionalRole::PrimaryMelody: return "PrimaryMelody";
        case FunctionalRole::Counterpoint: return "Counterpoint";
    }
    return "PrimaryMelody";
}

FunctionalRole parseFunctionalRole(const std::string& str) noexcept {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("drum") != std::string::npos || s.find("rhythm") != std::string::npos ||
        s.find("beat") != std::string::npos || s.find("percuss") != std::string::npos) {
        return FunctionalRole::Rhythm;
    }
    if (s.find("bass") != std::string::npos || s.find("foundation") != std::string::npos ||
        s.find("sub") != std::string::npos || s.find("root") != std::string::npos) {
        return FunctionalRole::Foundation;
    }
    if (s.find("chord") != std::string::npos || s.find("texture") != std::string::npos ||
        s.find("pad") != std::string::npos || s.find("strum") != std::string::npos) {
        return FunctionalRole::HarmonicTexture;
    }
    if (s.find("counter") != std::string::npos || s.find("answer") != std::string::npos ||
        s.find("harmony") != std::string::npos) {
        return FunctionalRole::Counterpoint;
    }
    return FunctionalRole::PrimaryMelody;
}

const char* getTextureTypeName(TextureType type) noexcept {
    switch (type) {
        case TextureType::Sustained: return "Sustained";
        case TextureType::Strummed: return "Strummed";
        case TextureType::Arpeggiated: return "Arpeggiated";
        case TextureType::Stabs: return "Stabs";
    }
    return "Sustained";
}

TextureType parseTextureType(const std::string& str) noexcept {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("strum") != std::string::npos || s.find("pluck") != std::string::npos) return TextureType::Strummed;
    if (s.find("arp") != std::string::npos) return TextureType::Arpeggiated;
    if (s.find("stab") != std::string::npos || s.find("punch") != std::string::npos) return TextureType::Stabs;
    return TextureType::Sustained;
}

const char* getMelodyStyleName(MelodyStyle style) noexcept {
    switch (style) {
        case MelodyStyle::Lyrical: return "Lyrical";
        case MelodyStyle::HeroicAnthem: return "HeroicAnthem";
        case MelodyStyle::SyncopatedRiff: return "SyncopatedRiff";
        case MelodyStyle::CascadingRun: return "CascadingRun";
        case MelodyStyle::FolkBallad: return "FolkBallad";
        case MelodyStyle::Bluesy: return "Bluesy";
        case MelodyStyle::Atmospheric: return "Atmospheric";
        case MelodyStyle::Auto: return "Auto";
    }
    return "Auto";
}

MelodyStyle parseMelodyStyle(const std::string& str) noexcept {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s.find("hero") != std::string::npos || s.find("anthem") != std::string::npos) return MelodyStyle::HeroicAnthem;
    if (s.find("sync") != std::string::npos || s.find("riff") != std::string::npos || s.find("funk") != std::string::npos) return MelodyStyle::SyncopatedRiff;
    if (s.find("cascad") != std::string::npos || s.find("run") != std::string::npos || s.find("chip") != std::string::npos) return MelodyStyle::CascadingRun;
    if (s.find("folk") != std::string::npos || s.find("ballad") != std::string::npos) return MelodyStyle::FolkBallad;
    if (s.find("blue") != std::string::npos || s.find("soul") != std::string::npos) return MelodyStyle::Bluesy;
    if (s.find("atmos") != std::string::npos || s.find("ambient") != std::string::npos) return MelodyStyle::Atmospheric;
    if (s.find("lyric") != std::string::npos || s.find("vocal") != std::string::npos) return MelodyStyle::Lyrical;
    return MelodyStyle::Auto;
}

const char* getSectionTypeName(SectionType type) noexcept {
    switch (type) {
        case SectionType::Intro: return "Intro";
        case SectionType::Verse: return "Verse";
        case SectionType::Chorus: return "Chorus";
        case SectionType::Breakdown: return "Breakdown";
        case SectionType::Drop: return "Drop";
        case SectionType::Outro: return "Outro";
    }
    return "Verse";
}

// ─────────────────────────────────────────────────────────────────────────────
// SongArchetypeRegistry Implementation
// ─────────────────────────────────────────────────────────────────────────────

std::vector<SongArchetype> SongArchetypeRegistry::archetypes_{};
bool SongArchetypeRegistry::initialized_{false};

void SongArchetypeRegistry::initialize() {
    if (initialized_ && !archetypes_.empty()) return;
    archetypes_.clear();

    // 1. Acid Techno
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_acid_techno";
        bp.title = "Warehouse Acid Techno";
        bp.category = "Electronic";
        bp.genre = "Acid Techno";
        bp.bpm = 135.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 0; // C
        bp.mode = "phrygian";
        bp.tags = {"acid", "techno", "tb303", "tr909", "electronic", "underground"};

        // Form: Intro (4) -> Verse (8) -> Breakdown (4) -> Drop (8) -> Outro (4) = 28 bars
        bp.sections = {
            {"Intro", SectionType::Intro, 0, 4, 0.40, {}},
            {"Acid Build", SectionType::Verse, 4, 8, 0.75, {}},
            {"Hypnotic Breakdown", SectionType::Breakdown, 12, 4, 0.50, {}},
            {"Warehouse Peak Drop", SectionType::Drop, 16, 8, 1.00, {}},
            {"Outro", SectionType::Outro, 24, 4, 0.35, {}}
        };

        bp.ensemble = {
            {"drums", "909 Drum Kit", "tr909", FunctionalRole::Rhythm, TextureType::Stabs, 0.90f, 0.0f, 1.0f, 0.25f, 0.25f, {}},
            {"acid_bass", "TB-303 Acid Bass", "tb303", FunctionalRole::Foundation, TextureType::Arpeggiated, 0.88f, 0.0f, 0.0f, 0.85f, 1.0f, {}},
            {"synth_stabs", "Resonant Acid Lead", "tb303", FunctionalRole::PrimaryMelody, TextureType::Stabs, 0.82f, 0.15f, 0.0f, 1.0f, 0.45f, {}},
            {"perc_metal", "Open Hi-Hat & Ride", "tr909", FunctionalRole::Counterpoint, TextureType::Stabs, 0.75f, -0.20f, 1.0f, 0.8f, 0.0f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    // 2. Synthwave / Retrowave
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_synthwave";
        bp.title = "Outrun Neon Highway";
        bp.category = "Electronic";
        bp.genre = "Synthwave / Retrowave";
        bp.bpm = 124.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 9; // A Minor
        bp.mode = "minor";
        bp.tags = {"synthwave", "retrowave", "80s", "outrun", "analog"};

        bp.sections = {
            {"Sunset Intro", SectionType::Intro, 0, 4, 0.45, {}},
            {"Driving Verse", SectionType::Verse, 4, 8, 0.70, {}},
            {"Neon Chorus", SectionType::Chorus, 12, 8, 0.95, {}},
            {"Horizon Outro", SectionType::Outro, 20, 4, 0.40, {}}
        };

        bp.ensemble = {
            {"drums", "Electronic Drum Kit", "tr808", FunctionalRole::Rhythm, TextureType::Stabs, 0.90f, 0.0f, 1.0f, 0.15f, 0.6f, {}},
            {"bass", "Model D Synth Bass", "synth_bass", FunctionalRole::Foundation, TextureType::Arpeggiated, 0.86f, 0.0f, 0.0f, 0.9f, 1.0f, {}},
            {"pads", "Analog Poly Pad", "dx7", FunctionalRole::HarmonicTexture, TextureType::Sustained, 0.80f, -0.10f, 0.8f, 0.2f, 0.9f, {}},
            {"lead", "Outrun Arp Lead", "dx7", FunctionalRole::PrimaryMelody, TextureType::Arpeggiated, 0.82f, 0.15f, 1.0f, 0.6f, 0.0f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    // 3. Lofi Hip Hop
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_lofi_hiphop";
        bp.title = "Rainy Day Chillhop";
        bp.category = "Urban / Chill";
        bp.genre = "Lofi Hip Hop";
        bp.bpm = 84.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 2; // D Minor
        bp.mode = "minor";
        bp.tags = {"lofi", "hiphop", "chill", "jazz", "cozy", "study"};

        bp.sections = {
            {"Vinyl Intro", SectionType::Intro, 0, 4, 0.40, {}},
            {"Mellow Verse", SectionType::Verse, 4, 6, 0.70, {}},
            {"Warm Chorus", SectionType::Chorus, 10, 4, 0.85, {}},
            {"Rain Outro", SectionType::Outro, 14, 2, 0.30, {}}
        };

        bp.ensemble = {
            {"drums", "Boom Bap Dusty Kit", "gm_drums", FunctionalRole::Rhythm, TextureType::Stabs, 0.88f, 0.0f, 0.8f, 0.4f, 0.2f, {}},
            {"bass", "Acoustic Upright Bass", "acoustic_bass", FunctionalRole::Foundation, TextureType::Sustained, 0.85f, 0.0f, 0.7f, 0.5f, 0.2f, {}},
            {"keys", "Felt Rhodes Piano", "rhodes", FunctionalRole::HarmonicTexture, TextureType::Strummed, 0.80f, -0.15f, 0.4f, 0.7f, 0.5f, {}},
            {"lead", "Lyrical Vibraphone", "vibraphone", FunctionalRole::PrimaryMelody, TextureType::Sustained, 0.78f, 0.15f, 1.0f, 0.75f, 0.1f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    // 4. Cyberpunk Electro
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_cyberpunk";
        bp.title = "Neon Underworld";
        bp.category = "Electronic";
        bp.genre = "Cyberpunk Electro";
        bp.bpm = 130.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 4; // E Minor
        bp.mode = "minor";
        bp.tags = {"cyberpunk", "electro", "dark", "dystopian", "industrial"};

        bp.sections = {
            {"Dark Boot Intro", SectionType::Intro, 0, 4, 0.50, {}},
            {"Industrial Verse", SectionType::Verse, 4, 8, 0.80, {}},
            {"Cyber Drop", SectionType::Drop, 12, 8, 1.00, {}},
            {"Fadeout Outro", SectionType::Outro, 20, 4, 0.40, {}}
        };

        bp.ensemble = {
            {"drums", "Heavy Distorted Beats", "tr808", FunctionalRole::Rhythm, TextureType::Stabs, 0.92f, 0.0f, 0.9f, 0.1f, 0.3f, {}},
            {"bass", "Sawtooth Sub Growl", "tb303", FunctionalRole::Foundation, TextureType::Stabs, 0.90f, 0.0f, 0.0f, 0.8f, 0.8f, {}},
            {"stabs", "Cyberpunk FM Plucks", "ym2612", FunctionalRole::HarmonicTexture, TextureType::Stabs, 0.82f, -0.20f, 0.7f, 0.0f, 1.0f, {}},
            {"lead", "Screaming Acid Lead", "tb303", FunctionalRole::PrimaryMelody, TextureType::Arpeggiated, 0.84f, 0.20f, 1.0f, 0.9f, 0.0f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    // 5. Ambient Drone
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_ambient_drone";
        bp.title = "Ethereal Deep Space";
        bp.category = "Atmospheric";
        bp.genre = "Ambient Drone";
        bp.bpm = 68.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 2; // D Dorian
        bp.mode = "dorian";
        bp.tags = {"ambient", "drone", "space", "meditation", "ethereal"};

        bp.sections = {
            {"Atmospheric Inception", SectionType::Intro, 0, 8, 0.30, {}},
            {"Modal Horizon", SectionType::Verse, 8, 8, 0.55, {}},
            {"Ethereal Expansion", SectionType::Breakdown, 16, 8, 0.70, {}},
            {"Dissolution Outro", SectionType::Outro, 24, 8, 0.25, {}}
        };

        bp.ensemble = {
            {"percussion", "Suspended Shaker / Gong", "gm_drums", FunctionalRole::Rhythm, TextureType::Sustained, 0.65f, 0.0f, 0.3f, 0.6f, 0.8f, {}},
            {"drone_bass", "Sub Drone Foundation", "synth_bass", FunctionalRole::Foundation, TextureType::Sustained, 0.80f, 0.0f, 0.2f, 0.3f, 0.7f, {}},
            {"pad_texture", "Lush Crystalline Pad", "dx7", FunctionalRole::HarmonicTexture, TextureType::Sustained, 0.85f, -0.15f, 0.5f, 0.8f, 0.9f, {}},
            {"flute_lead", "Floating Ocarina / Flute", "snes_synth", FunctionalRole::PrimaryMelody, TextureType::Sustained, 0.78f, 0.20f, 0.4f, 1.0f, 0.7f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    // 6. SNES 16-Bit Adventure
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_snes_adventure";
        bp.title = "16-Bit Overworld Hero";
        bp.category = "Retro Console";
        bp.genre = "SNES 16-Bit Adventure";
        bp.bpm = 134.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 0; // C Major
        bp.mode = "major";
        bp.tags = {"snes", "16bit", "jrpg", "adventure", "retro", "nostalgia"};

        bp.sections = {
            {"Fanfare Intro", SectionType::Intro, 0, 4, 0.60, {}},
            {"Overworld Journey", SectionType::Verse, 4, 8, 0.80, {}},
            {"Heroic Triumph Chorus", SectionType::Chorus, 12, 8, 0.98, {}},
            {"Restful Outro", SectionType::Outro, 20, 4, 0.45, {}}
        };

        bp.ensemble = {
            {"drums", "SNES Drum Kit", "snes_drums", FunctionalRole::Rhythm, TextureType::Stabs, 0.88f, 0.0f, 0.9f, 0.15f, 0.15f, {}},
            {"bass", "SNES Slap Bass", "snes_synth", FunctionalRole::Foundation, TextureType::Stabs, 0.86f, 0.0f, 0.9f, 0.5f, 0.0f, {}},
            {"strings", "SNES Strings Pad", "snes_synth", FunctionalRole::HarmonicTexture, TextureType::Sustained, 0.80f, -0.15f, 0.4f, 0.3f, 0.9f, {}},
            {"lead_flute", "SNES Hero Flute", "snes_synth", FunctionalRole::PrimaryMelody, TextureType::Sustained, 0.84f, 0.15f, 0.0f, 0.8f, 0.8f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    // 7. C64 SID 8-Bit Chiptune
    {
        SongStructureBlueprint bp;
        bp.id = "archetype_c64_chiptune";
        bp.title = "C64 Cybernetic Odyssey";
        bp.category = "Retro Console";
        bp.genre = "C64 SID 8-Bit Chiptune";
        bp.bpm = 138.0;
        bp.meter = "4/4";
        bp.rootPitchClass = 9; // A Minor
        bp.mode = "minor";
        bp.tags = {"c64", "sid", "8bit", "chiptune", "demoscene", "retro"};

        bp.sections = {
            {"Arp Countdown Intro", SectionType::Intro, 0, 4, 0.65, {}},
            {"Rolling Bass Verse", SectionType::Verse, 4, 8, 0.85, {}},
            {"High Voltage Chorus", SectionType::Chorus, 12, 8, 1.00, {}},
            {"System Halt Outro", SectionType::Outro, 20, 4, 0.40, {}}
        };

        bp.ensemble = {
            {"drums", "SID Noise Percussion", "sid_synth", FunctionalRole::Rhythm, TextureType::Stabs, 0.88f, 0.0f, 0.4f, 0.4f, 0.7f, {}},
            {"bass", "SID PWM Bass", "sid_synth", FunctionalRole::Foundation, TextureType::Arpeggiated, 0.88f, 0.0f, 0.2f, 0.7f, 0.9f, {}},
            {"arps", "SID Triple Arp", "sid_synth", FunctionalRole::HarmonicTexture, TextureType::Arpeggiated, 0.82f, -0.15f, 0.8f, 0.4f, 0.8f, {}},
            {"lead", "SID Sync Lead", "sid_synth", FunctionalRole::PrimaryMelody, TextureType::Sustained, 0.84f, 0.15f, 0.9f, 0.8f, 0.2f, {}}
        };

        archetypes_.emplace_back(std::move(bp));
    }

    initialized_ = true;
}

const std::vector<SongArchetype>& SongArchetypeRegistry::getAllArchetypes() {
    if (!initialized_) initialize();
    return archetypes_;
}

const SongArchetype* SongArchetypeRegistry::getById(const std::string& id) {
    if (!initialized_) initialize();
    std::string clean = id;
    std::transform(clean.begin(), clean.end(), clean.begin(), ::tolower);

    for (const auto& a : archetypes_) {
        std::string aId = a.getId();
        std::transform(aId.begin(), aId.end(), aId.begin(), ::tolower);
        if (aId == clean) return &a;
    }
    return nullptr;
}

std::vector<const SongArchetype*> SongArchetypeRegistry::findByCategory(const std::string& category) {
    if (!initialized_) initialize();
    std::string clean = category;
    std::transform(clean.begin(), clean.end(), clean.begin(), ::tolower);

    std::vector<const SongArchetype*> results;
    for (const auto& a : archetypes_) {
        std::string cat = a.getCategory();
        std::transform(cat.begin(), cat.end(), cat.begin(), ::tolower);
        if (cat.find(clean) != std::string::npos) {
            results.push_back(&a);
        }
    }
    return results;
}

std::vector<const SongArchetype*> SongArchetypeRegistry::findByTag(const std::string& tag) {
    if (!initialized_) initialize();
    std::string clean = tag;
    std::transform(clean.begin(), clean.end(), clean.begin(), ::tolower);

    std::vector<const SongArchetype*> results;
    for (const auto& a : archetypes_) {
        for (const auto& t : a.getTags()) {
            std::string tClean = t;
            std::transform(tClean.begin(), tClean.end(), tClean.begin(), ::tolower);
            if (tClean.find(clean) != std::string::npos) {
                results.push_back(&a);
                break;
            }
        }
    }
    return results;
}

void SongArchetypeRegistry::registerArchetype(SongArchetype archetype) {
    if (!initialized_) initialize();
    archetypes_.push_back(std::move(archetype));
}

void SongArchetypeRegistry::clear() {
    archetypes_.clear();
    initialized_ = false;
}

} // namespace eatsbits::procgen
