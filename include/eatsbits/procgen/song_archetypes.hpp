#ifndef EATS_SONG_ARCHETYPES_HPP
#define EATS_SONG_ARCHETYPES_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>
#include "../theory/chord_model.hpp"

namespace eatsbits::procgen {

/// The 5 universal functional roles an instrument track performs in an ensemble.
enum class FunctionalRole {
    Rhythm,           // Percussion / drums / rhythm machine
    Foundation,       // Bassline / root / sub / pedal drone
    HarmonicTexture,  // Sustained pads, strummed chords, arpeggios, stabs
    PrimaryMelody,    // Lyrical melody / hook / solo lead
    Counterpoint      // Counter-melody / harmonic answering / 3rds/6ths doubling
};

[[nodiscard]] const char* getFunctionalRoleName(FunctionalRole role) noexcept;
[[nodiscard]] FunctionalRole parseFunctionalRole(const std::string& str) noexcept;

/// Rhythmic / voicing execution style for harmonic texture tracks.
enum class TextureType {
    Sustained,
    Strummed,
    Arpeggiated,
    Stabs
};

[[nodiscard]] const char* getTextureTypeName(TextureType type) noexcept;
[[nodiscard]] TextureType parseTextureType(const std::string& str) noexcept;

/// Stylistic aesthetic and phrasing behavior for lead melody generation.
enum class MelodyStyle {
    Lyrical,        // Expressive, soulful, vocal-like phrasing with dynamic breath/rests
    HeroicAnthem,   // Bold, triumphant leaps (4ths, 5ths, octaves), dotted figures
    SyncopatedRiff, // Funky, driving, offbeat anticipations and punchy hooks
    CascadingRun,   // Flowing scalar runs, rapid undulating arpeggios
    FolkBallad,     // Pastoral, lilting pentatonic with graceful ornaments
    Bluesy,         // Expressive blue notes (b3, b5, b7), bent slides
    Atmospheric,    // Spacious, floating, long breath/tails in high register
    Auto            // Intelligently chosen based on genre, tempo, and seed
};

[[nodiscard]] const char* getMelodyStyleName(MelodyStyle style) noexcept;
[[nodiscard]] MelodyStyle parseMelodyStyle(const std::string& str) noexcept;

/// Section types within a musical form.
enum class SectionType {
    Intro,
    Verse,
    Chorus,
    Breakdown,
    Drop,
    Outro
};

[[nodiscard]] const char* getSectionTypeName(SectionType type) noexcept;

/// Phrasing note unit extracted from an exemplar track.
struct ArchetypeNotePhrase {
    double relativeStep{0.0};
    double durationSteps{1.0};
    int pitch{60};
    int pitchInterval{0}; // Semitone offset relative to song root pitch class
    double velocity{0.80};
    bool isSlide{false};
    bool isAccent{false};
};

/// Track profile extracted from an exemplar song.
struct ArchetypeTrackProfile {
    std::string trackId;
    std::string name;
    std::string presetId;
    FunctionalRole role{FunctionalRole::PrimaryMelody};
    std::vector<ArchetypeNotePhrase> phrases;
};

/// Section blueprint in a song structure.
struct SectionBlueprint {
    std::string name{"Verse"};
    SectionType type{SectionType::Verse};
    uint32_t startBar{0};
    uint32_t lengthBars{4};
    double energy{0.75}; // 0.0 to 1.0 energy level
    std::vector<theory::ChordEvent> chords;
};

/// Blueprint specification for a single instrument track in an ensemble.
struct EnsembleTrackBlueprint {
    std::string trackId;
    std::string name;
    std::string presetId;
    FunctionalRole role{FunctionalRole::Rhythm};
    TextureType textureType{TextureType::Sustained};
    float defaultVolume{0.85f};
    float defaultPan{0.0f};
    float colorR{1.0f}, colorG{0.5f}, colorB{0.0f};
    std::map<std::string, double> defaultParams;
};

/// Structured architectural blueprint for a complete multi-track song arrangement.
struct SongStructureBlueprint {
    std::string id;
    std::string title;
    std::string category;
    std::string genre;
    double bpm{120.0};
    std::string meter{"4/4"};
    int stepsPerBar{16};
    int rootPitchClass{0}; // 0 = C
    std::string mode{"minor"};
    std::vector<std::string> tags;
    std::vector<SectionBlueprint> sections;
    std::vector<EnsembleTrackBlueprint> ensemble;

    [[nodiscard]] uint32_t getTotalBars() const noexcept {
        uint32_t bars = 0;
        for (const auto& s : sections) bars += s.lengthBars;
        return bars;
    }
};

/// Complete Song Archetype model.
class SongArchetype {
public:
    SongArchetype() = default;
    explicit SongArchetype(SongStructureBlueprint blueprint)
        : blueprint_(std::move(blueprint)) {}

    [[nodiscard]] const std::string& getId() const noexcept { return blueprint_.id; }
    [[nodiscard]] const std::string& getTitle() const noexcept { return blueprint_.title; }
    [[nodiscard]] const std::string& getCategory() const noexcept { return blueprint_.category; }
    [[nodiscard]] const std::string& getGenre() const noexcept { return blueprint_.genre; }
    [[nodiscard]] double getBpm() const noexcept { return blueprint_.bpm; }
    [[nodiscard]] const std::string& getMeter() const noexcept { return blueprint_.meter; }
    [[nodiscard]] int getRootPitchClass() const noexcept { return blueprint_.rootPitchClass; }
    [[nodiscard]] const std::string& getMode() const noexcept { return blueprint_.mode; }
    [[nodiscard]] const std::vector<std::string>& getTags() const noexcept { return blueprint_.tags; }
    [[nodiscard]] const SongStructureBlueprint& getBlueprint() const noexcept { return blueprint_; }
    [[nodiscard]] SongStructureBlueprint& getBlueprint() noexcept { return blueprint_; }

    [[nodiscard]] const std::vector<ArchetypeTrackProfile>& getProfiles() const noexcept { return profiles_; }
    void addProfile(ArchetypeTrackProfile profile) { profiles_.push_back(std::move(profile)); }

private:
    SongStructureBlueprint blueprint_;
    std::vector<ArchetypeTrackProfile> profiles_;
};

/// Registry of built-in and user song archetypes.
class SongArchetypeRegistry {
public:
    /// Initializes built-in exemplar archetypes (Acid Techno, Synthwave, Lofi, Cyberpunk, Ambient, SNES, C64).
    static void initialize();

    /// Returns all registered song archetypes.
    [[nodiscard]] static const std::vector<SongArchetype>& getAllArchetypes();

    /// Looks up archetype by ID.
    [[nodiscard]] static const SongArchetype* getById(const std::string& id);

    /// Finds archetypes by category (e.g. "Electronic", "Retro", "Ambient").
    [[nodiscard]] static std::vector<const SongArchetype*> findByCategory(const std::string& category);

    /// Finds archetypes by tag (e.g. "acid", "synthwave", "lofi").
    [[nodiscard]] static std::vector<const SongArchetype*> findByTag(const std::string& tag);

    /// Registers a new archetype.
    static void registerArchetype(SongArchetype archetype);

    /// Clears registry (useful for testing).
    static void clear();

private:
    static std::vector<SongArchetype> archetypes_;
    static bool initialized_;
};

} // namespace eatsbits::procgen

#endif // EATS_SONG_ARCHETYPES_HPP
