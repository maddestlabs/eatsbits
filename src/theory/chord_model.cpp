#include "eatsbits/theory/chord_model.hpp"
#include <sstream>
#include <map>

namespace eatsbits::theory {

const char* getChordQualityDisplayName(ChordQuality q) noexcept {
    switch (q) {
        case ChordQuality::Major:           return "Major";
        case ChordQuality::Minor:           return "Minor";
        case ChordQuality::Dominant7:       return "7 (Dom)";
        case ChordQuality::Major7:          return "Maj7";
        case ChordQuality::Minor7:          return "Min7";
        case ChordQuality::Diminished:      return "Dim";
        case ChordQuality::Augmented:       return "Aug";
        case ChordQuality::HalfDiminished7: return "m7b5";
        case ChordQuality::Sus2:            return "Sus2";
        case ChordQuality::Sus4:            return "Sus4";
        case ChordQuality::Add9:            return "Add9";
        case ChordQuality::Min9:            return "Min9";
        case ChordQuality::Maj9:            return "Maj9";
        case ChordQuality::Dom9:            return "9 (Dom)";
    }
    return "Major";
}

const char* getChordQualitySymbol(ChordQuality q) noexcept {
    switch (q) {
        case ChordQuality::Major:           return "";
        case ChordQuality::Minor:           return "m";
        case ChordQuality::Dominant7:       return "7";
        case ChordQuality::Major7:          return "maj7";
        case ChordQuality::Minor7:          return "m7";
        case ChordQuality::Diminished:      return "dim";
        case ChordQuality::Augmented:       return "aug";
        case ChordQuality::HalfDiminished7: return "m7b5";
        case ChordQuality::Sus2:            return "sus2";
        case ChordQuality::Sus4:            return "sus4";
        case ChordQuality::Add9:            return "add9";
        case ChordQuality::Min9:            return "m9";
        case ChordQuality::Maj9:            return "maj9";
        case ChordQuality::Dom9:            return "9";
    }
    return "";
}

std::vector<int> getChordQualityIntervals(ChordQuality q) {
    switch (q) {
        case ChordQuality::Major:           return {0, 4, 7};
        case ChordQuality::Minor:           return {0, 3, 7};
        case ChordQuality::Dominant7:       return {0, 4, 7, 10};
        case ChordQuality::Major7:          return {0, 4, 7, 11};
        case ChordQuality::Minor7:          return {0, 3, 7, 10};
        case ChordQuality::Diminished:      return {0, 3, 6};
        case ChordQuality::Augmented:       return {0, 4, 8};
        case ChordQuality::HalfDiminished7: return {0, 3, 6, 10};
        case ChordQuality::Sus2:            return {0, 2, 7};
        case ChordQuality::Sus4:            return {0, 5, 7};
        case ChordQuality::Add9:            return {0, 4, 7, 14};
        case ChordQuality::Min9:            return {0, 3, 7, 10, 14};
        case ChordQuality::Maj9:            return {0, 4, 7, 11, 14};
        case ChordQuality::Dom9:            return {0, 4, 7, 10, 14};
    }
    return {0, 4, 7};
}

const char* getChordFollowModeName(ChordFollowMode m) noexcept {
    switch (m) {
        case ChordFollowMode::Off:       return "OFF";
        case ChordFollowMode::Chord:     return "CHORD";
        case ChordFollowMode::Bass:      return "BASS";
        case ChordFollowMode::Scale:     return "SCALE";
        case ChordFollowMode::ColorLead: return "COLOR LEAD";
    }
    return "OFF";
}

ChordFollowMode parseChordFollowMode(const std::string& name) noexcept {
    std::string lower;
    for (char c : name) lower += static_cast<char>(std::tolower(c));
    if (lower == "bass") return ChordFollowMode::Bass;
    if (lower == "chord") return ChordFollowMode::Chord;
    if (lower == "scale") return ChordFollowMode::Scale;
    if (lower == "colorlead" || lower == "lead" || lower == "color_lead") return ChordFollowMode::ColorLead;
    return ChordFollowMode::Off;
}

std::string ChordEvent::getDisplayName() const {
    return ChordTheory::formatChordName(rootPitchClass, quality, bassPitchClass);
}

std::string ChordEvent::getRootName() const {
    return ChordTheory::pitchClassNames[(rootPitchClass % 12 + 12) % 12];
}

std::string ChordEvent::getBassName() const {
    if (bassPitchClass >= 0) {
        return ChordTheory::pitchClassNames[(bassPitchClass % 12 + 12) % 12];
    }
    return "";
}

std::vector<int> ChordEvent::getPitchClasses() const {
    return ChordTheory::getPitchClasses(rootPitchClass, quality, bassPitchClass);
}

std::string ChordProgressionPreset::getRomanSummary(int keyRoot, bool isMinor) const {
    std::string out;
    for (size_t i = 0; i < chords.size(); ++i) {
        if (i > 0) out += " - ";
        int chordRoot = (keyRoot + chords[i].rootOffset) % 12;
        out += ChordTheory::getRomanNumeral(keyRoot, isMinor, chordRoot, chords[i].quality);
    }
    return out;
}

// =========================================================================
// ChordTheory Constants
// =========================================================================

const std::array<const char*, 12> ChordTheory::pitchClassNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const std::array<const char*, 12> ChordTheory::pitchClassFlatNames = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

const std::array<int, 12> ChordTheory::circleOfFifthsMajor = {
    0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5
};

const std::array<int, 12> ChordTheory::circleOfFifthsMinor = {
    9, 4, 11, 6, 1, 8, 3, 10, 5, 0, 7, 2
};

const std::array<const char*, 12> ChordTheory::circleMajorLabels = {
    "C", "G", "D", "A", "E", "B", "F#", "Db", "Ab", "Eb", "Bb", "F"
};

const std::array<const char*, 12> ChordTheory::circleMinorLabels = {
    "Am", "Em", "Bm", "F#m", "C#m", "G#m", "D#m", "Bbm", "Fm", "Cm", "Gm", "Dm"
};

const std::vector<ChordProgressionPreset>& ChordTheory::getProgressionPresets() {
    static const std::vector<ChordProgressionPreset> presets = {
        // --- POP & MAINSTREAM ---
        {
            "pop_axis", "Pop Classic (I - V - vi - IV)", "Pop",
            "The iconic Axis of Awesome progression behind hundreds of massive global hit songs.",
            {"pop", "hits", "anthem", "radio"},
            {{0, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}, {9, ChordQuality::Minor, 1.0f}, {5, ChordQuality::Major, 1.0f}}
        },
        {
            "pop_doo_wop", "50s Doo-Wop (I - vi - IV - V)", "Pop",
            "Nostalgic golden-era ballad progression made legendary by 'Stand By Me' and classic rock & roll.",
            {"50s", "ballad", "retro", "motown"},
            {{0, ChordQuality::Major, 1.0f}, {9, ChordQuality::Minor, 1.0f}, {5, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}}
        },
        {
            "pop_emotional", "Emotional Anthem (I - iii - IV - V)", "Pop",
            "Uplifting ascending progression featuring a bittersweet mediant chord.",
            {"pop", "uplifting", "climax"},
            {{0, ChordQuality::Major, 1.0f}, {4, ChordQuality::Minor, 1.0f}, {5, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}}
        },
        {
            "pop_melancholy", "Sensitive Minor (vi - IV - I - V)", "Pop",
            "Moody, emotive minor pop progression used in countless heartfelt modern tracks.",
            {"pop", "minor", "emotional"},
            {{9, ChordQuality::Minor, 1.0f}, {5, ChordQuality::Major, 1.0f}, {0, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}}
        },

        // --- SYNTHWAVE & CYBERPUNK ---
        {
            "synthwave_dark", "Cyberpunk Drive (vi - IV - I - V)", "Synthwave",
            "Relentless driving progression with high-energy minor synth energy.",
            {"synthwave", "cyberpunk", "retrowave", "drive"},
            {{9, ChordQuality::Minor, 1.0f}, {5, ChordQuality::Major, 1.0f}, {0, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}}
        },
        {
            "synthwave_outrun", "Outrun Sunset (i - VII - v - VI)", "Synthwave",
            "Nostalgic 80s arcade sunset cruising vibe with natural minor cadences.",
            {"synthwave", "outrun", "80s", "neon"},
            {{0, ChordQuality::Minor, 1.0f}, {10, ChordQuality::Major, 1.0f}, {7, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}}
        },
        {
            "synthwave_darkwave", "Darkwave Pulse (i - VI - iv - V7)", "Synthwave",
            "Ominous industrial darkwave loop with a harmonic minor dominant turnaround.",
            {"synthwave", "darkwave", "goth", "minor"},
            {{0, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}, {5, ChordQuality::Minor, 1.0f}, {7, ChordQuality::Dominant7, 1.0f}}
        },

        // --- EDM, HOUSE & TRANCE ---
        {
            "edm_progressive_house", "Progressive House (IV - I - vi - V)", "EDM",
            "Festival mainstage euphoric buildup and melodic drop progression.",
            {"edm", "house", "festival", "euphoria"},
            {{5, ChordQuality::Major, 1.0f}, {0, ChordQuality::Major, 1.0f}, {9, ChordQuality::Minor, 1.0f}, {7, ChordQuality::Major, 1.0f}}
        },
        {
            "edm_deep_house", "Deep House Nocturne (i7 - v7 - iv7 - VImaj7)", "EDM",
            "Late-night club groove with lush minor 7th chords and deep sub warmth.",
            {"edm", "deephouse", "club", "groove"},
            {{0, ChordQuality::Minor7, 1.0f}, {7, ChordQuality::Minor7, 1.0f}, {5, ChordQuality::Minor7, 1.0f}, {8, ChordQuality::Major7, 1.0f}}
        },
        {
            "edm_trance_uplift", "Trance Uplift (i - VI - VII - i)", "EDM",
            "Driving 138 BPM uplifting trance arpeggio progression with high emotional energy.",
            {"edm", "trance", "uplift", "energy"},
            {{0, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}, {10, ChordQuality::Major, 1.0f}, {0, ChordQuality::Minor, 1.0f}}
        },
        {
            "edm_future_bass", "Future Bass Lush (IVmaj9 - V - iii7 - vi7)", "EDM",
            "Glitchy vocal-chopped future bass progression with extended lush 9th chords.",
            {"edm", "futurebass", "chill", "lush"},
            {{5, ChordQuality::Maj9, 1.0f}, {7, ChordQuality::Major, 1.0f}, {4, ChordQuality::Minor7, 1.0f}, {9, ChordQuality::Minor7, 1.0f}}
        },

        // --- JAZZ, NEO-SOUL & R&B ---
        {
            "jazz_two_five_one", "Jazz Cadence (ii7 - V7 - Imaj7 - VI7)", "Jazz / Neo-Soul",
            "The foundational standard of jazz harmony with secondary dominant turnaround.",
            {"jazz", "ii-v-i", "standard", "harmony"},
            {{2, ChordQuality::Minor7, 1.0f}, {7, ChordQuality::Dominant7, 1.0f}, {0, ChordQuality::Major7, 1.0f}, {9, ChordQuality::Dominant7, 1.0f}}
        },
        {
            "neo_soul_smooth", "Neo-Soul Silk (ii9 - V9 - Imaj9 - VI7)", "Jazz / Neo-Soul",
            "Silky, warm Neo-Soul progression with rich color tones and voice leading.",
            {"neosoul", "soul", "rnb", "warm"},
            {{2, ChordQuality::Min9, 1.0f}, {7, ChordQuality::Dom9, 1.0f}, {0, ChordQuality::Maj9, 1.0f}, {9, ChordQuality::Dominant7, 1.0f}}
        },
        {
            "rnb_bedroom_jam", "R&B Slow Jam (IVmaj7 - iii7 - ii7 - Imaj7)", "Jazz / Neo-Soul",
            "Velveteen step-wise descending bass line progression for smooth R&B ballads.",
            {"rnb", "slowjam", "ballad", "descent"},
            {{5, ChordQuality::Major7, 1.0f}, {4, ChordQuality::Minor7, 1.0f}, {2, ChordQuality::Minor7, 1.0f}, {0, ChordQuality::Major7, 1.0f}}
        },
        {
            "jazz_modal_so_what", "Modal Jazz Vamp (i7 - i7 - bII7 - i7)", "Jazz / Neo-Soul",
            "Miles Davis 'So What' style Dorian modal shift with half-step chromatic tension.",
            {"jazz", "modal", "dorian", "vamp"},
            {{0, ChordQuality::Minor7, 2.0f}, {1, ChordQuality::Minor7, 1.0f}, {0, ChordQuality::Minor7, 1.0f}}
        },

        // --- LO-FI & CHILLHOP ---
        {
            "lofi_chill", "Lofi Nostalgia (Imaj9 - vi9 - ii9 - V7)", "Lo-Fi",
            "Wobbly tape saturation and rainy day study beats progression.",
            {"lofi", "chill", "study", "relax"},
            {{0, ChordQuality::Maj9, 1.0f}, {9, ChordQuality::Min9, 1.0f}, {2, ChordQuality::Min9, 1.0f}, {7, ChordQuality::Dominant7, 1.0f}}
        },
        {
            "lofi_sunset", "Lofi Sunset Breeze (IVmaj9 - iii7 - vi9 - Imaj7)", "Lo-Fi",
            "Dreamy chillhop loop with lush upper extensions and laid-back swing.",
            {"lofi", "chillhop", "sunset", "warm"},
            {{5, ChordQuality::Maj9, 1.0f}, {4, ChordQuality::Minor7, 1.0f}, {9, ChordQuality::Min9, 1.0f}, {0, ChordQuality::Major7, 1.0f}}
        },

        // --- CINEMATIC & AMBIENT ---
        {
            "cinematic_epic", "Epic Hero (i - VI - III - VII)", "Cinematic",
            "Blockbuster trailer and movie soundtrack theme with sweeping orchestral power.",
            {"cinematic", "epic", "soundtrack", "heroic"},
            {{0, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}, {3, ChordQuality::Major, 1.0f}, {10, ChordQuality::Major, 1.0f}}
        },
        {
            "cinematic_hans_ostinato", "Hans Zimmer Ostinato (i - VI - iv - VII)", "Cinematic",
            "Relentless cello string ostinato with massive harmonic tension and resolution.",
            {"cinematic", "zimmer", "strings", "tension"},
            {{0, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}, {5, ChordQuality::Minor, 1.0f}, {10, ChordQuality::Major, 1.0f}}
        },
        {
            "cinematic_ethereal", "Ethereal Dreamscape (Imaj7 - IVmaj7 - vi7 - V)", "Cinematic",
            "Spacious, ambient soundscape progression with floating reverbs and pads.",
            {"ambient", "ethereal", "pads", "space"},
            {{0, ChordQuality::Major7, 1.0f}, {5, ChordQuality::Major7, 1.0f}, {9, ChordQuality::Minor7, 1.0f}, {7, ChordQuality::Major, 1.0f}}
        },

        // --- ROCK & METAL ---
        {
            "rock_power_anthem", "Classic Rock Anthem (I - IV - V - IV)", "Rock / Metal",
            "Timeless arena rock progression with roaring overdriven rhythm guitars.",
            {"rock", "arena", "power", "guitars"},
            {{0, ChordQuality::Major, 1.0f}, {5, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}, {5, ChordQuality::Major, 1.0f}}
        },
        {
            "metal_phrygian_menace", "Phrygian Metal Riff (i - bII - i - bVII)", "Rock / Metal",
            "Heavy downtuned metal riff with the sinister half-step Phrygian flat-2nd.",
            {"metal", "phrygian", "heavy", "riff"},
            {{0, ChordQuality::Minor, 1.0f}, {1, ChordQuality::Major, 1.0f}, {0, ChordQuality::Minor, 1.0f}, {10, ChordQuality::Major, 1.0f}}
        },
        {
            "grunge_minor_drop", "90s Grunge Drop (i - VI - III - VII)", "Rock / Metal",
            "Raw, gritty grunge and alternative rock power chord progression.",
            {"grunge", "alternative", "90s", "distortion"},
            {{0, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}, {3, ChordQuality::Major, 1.0f}, {10, ChordQuality::Major, 1.0f}}
        },

        // --- LATIN & FLAMENCO ---
        {
            "andalusian", "Flamenco Descent (i - VII - VI - V7)", "Latin / Flamenco",
            "The ancient Andalusian cadence featuring dramatic step-wise downward resolution.",
            {"latin", "flamenco", "spanish", "classical"},
            {{0, ChordQuality::Minor, 1.0f}, {10, ChordQuality::Major, 1.0f}, {8, ChordQuality::Major, 1.0f}, {7, ChordQuality::Dominant7, 1.0f}}
        },
        {
            "bossa_ipanema", "Bossa Nova Ipanema (Imaj7 - II7 - ii7 - V7)", "Latin / Flamenco",
            "Classic Brazilian bossa nova with secondary dominant 9th chords and gentle nylon swing.",
            {"bossa", "latin", "brazil", "jazz"},
            {{0, ChordQuality::Major7, 1.0f}, {2, ChordQuality::Dominant7, 1.0f}, {2, ChordQuality::Minor7, 1.0f}, {7, ChordQuality::Dominant7, 1.0f}}
        },
        {
            "latin_reggaeton_bounce", "Reggaeton / Latin Trap (i - VI - III - VII)", "Latin / Flamenco",
            "Bouncing Latin urban dembow groove progression with infectious minor warmth.",
            {"reggaeton", "dembow", "latin", "trap"},
            {{0, ChordQuality::Minor, 1.0f}, {8, ChordQuality::Major, 1.0f}, {3, ChordQuality::Major, 1.0f}, {10, ChordQuality::Major, 1.0f}}
        },

        // --- ANIME & J-POP ---
        {
            "jpop_royal_road", "Royal Road / Oudo 王道 (IVmaj7 - V7 - iii7 - vi)", "Anime / J-Pop",
            "The signature 'Royal Road' progression used in anime openings and J-Pop masterpieces.",
            {"anime", "jpop", "oudo", "royalroad"},
            {{5, ChordQuality::Major7, 1.0f}, {7, ChordQuality::Dominant7, 1.0f}, {4, ChordQuality::Minor7, 1.0f}, {9, ChordQuality::Minor, 1.0f}}
        },
        {
            "jpop_just_the_two_of_us", "Shibutani Groover (IVmaj7 - III7 - vi7 - I7)", "Anime / J-Pop",
            "Funky J-Rock & City Pop progression with chromatic secondary dominant push.",
            {"citypop", "anime", "funk", "groove"},
            {{5, ChordQuality::Major7, 1.0f}, {4, ChordQuality::Dominant7, 1.0f}, {9, ChordQuality::Minor7, 1.0f}, {0, ChordQuality::Dominant7, 1.0f}}
        },
        {
            "jpop_emotional_climax", "Anime Emotional Climax (IV - V - vi - I)", "Anime / J-Pop",
            "Driving cinematic chorus explosion from high-octane anime openings.",
            {"anime", "opening", "chorus", "climax"},
            {{5, ChordQuality::Major, 1.0f}, {7, ChordQuality::Major, 1.0f}, {9, ChordQuality::Minor, 1.0f}, {0, ChordQuality::Major, 1.0f}}
        }
    };
    return presets;
}

std::string ChordTheory::formatChordName(int rootPitchClass, ChordQuality quality, int bassPitchClass) {
    int root = (rootPitchClass % 12 + 12) % 12;
    std::string rootName = pitchClassNames[root];
    std::string sym = getChordQualitySymbol(quality);

    if (bassPitchClass >= 0) {
        int bass = (bassPitchClass % 12 + 12) % 12;
        if (bass != root) {
            return rootName + sym + "/" + pitchClassNames[bass];
        }
    }
    return rootName + sym;
}

std::vector<int> ChordTheory::getPitchClasses(int rootPitchClass, ChordQuality quality, int bassPitchClass) {
    std::set<int> unique;
    if (bassPitchClass >= 0) {
        unique.insert((bassPitchClass % 12 + 12) % 12);
    }
    int root = (rootPitchClass % 12 + 12) % 12;
    for (int interval : getChordQualityIntervals(quality)) {
        unique.insert((root + interval) % 12);
    }
    return std::vector<int>(unique.begin(), unique.end());
}

std::vector<int> ChordTheory::getAuditionMidiNotes(const ChordEvent& chord) {
    const int baseMidi = 48; // C3
    int root = (chord.rootPitchClass % 12 + 12) % 12;
    int rootMidi = baseMidi + root;
    std::vector<int> notes;

    if (chord.bassPitchClass >= 0) {
        int bass = (chord.bassPitchClass % 12 + 12) % 12;
        notes.push_back(36 + bass);
    } else {
        notes.push_back(rootMidi - 12); // Add bass root an octave lower
    }

    for (int interval : getChordQualityIntervals(chord.quality)) {
        notes.push_back(rootMidi + interval);
    }
    return notes;
}

std::string ChordTheory::getRomanNumeral(int keyRootPitchClass, bool isKeyMinor, int chordRootPitchClass, ChordQuality quality) {
    int key = (keyRootPitchClass % 12 + 12) % 12;
    int chord = (chordRootPitchClass % 12 + 12) % 12;
    int diff = (chord - key + 12) % 12;

    if (!isKeyMinor) {
        // Major Key
        switch (diff) {
            case 0:
                return (quality == ChordQuality::Major || quality == ChordQuality::Major7 || quality == ChordQuality::Maj9) ? "I" : "i";
            case 2:
                return (quality == ChordQuality::Minor || quality == ChordQuality::Minor7 || quality == ChordQuality::Min9) ? "ii" : "II";
            case 4:
                return (quality == ChordQuality::Minor || quality == ChordQuality::Minor7 || quality == ChordQuality::Min9) ? "iii" : "III";
            case 5:
                return (quality == ChordQuality::Major || quality == ChordQuality::Major7 || quality == ChordQuality::Maj9) ? "IV" : "iv";
            case 7:
                return (quality == ChordQuality::Major || quality == ChordQuality::Dominant7 || quality == ChordQuality::Dom9) ? "V" : "v";
            case 9:
                return (quality == ChordQuality::Minor || quality == ChordQuality::Minor7 || quality == ChordQuality::Min9) ? "vi" : "VI";
            case 11:
                return (quality == ChordQuality::Diminished || quality == ChordQuality::HalfDiminished7) ? "vii°" : "VII";
            default:
                return pitchClassNames[chord];
        }
    } else {
        // Minor Key
        switch (diff) {
            case 0:  return "i";
            case 2:  return "ii°";
            case 3:  return "III";
            case 5:  return "iv";
            case 7:  return (quality == ChordQuality::Dominant7 || quality == ChordQuality::Major) ? "V" : "v";
            case 8:  return "VI";
            case 10: return "VII";
            default: return pitchClassNames[chord];
        }
    }
}

std::vector<int> ChordTheory::getScalePitchClassesForChord(const ChordEvent& chord) {
    int root = (chord.rootPitchClass % 12 + 12) % 12;
    std::vector<int> intervals;

    switch (chord.quality) {
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
        case ChordQuality::Min9:
            // Natural minor scale (Aeolian)
            intervals = {0, 2, 3, 5, 7, 8, 10};
            break;
        case ChordQuality::Dominant7:
        case ChordQuality::Dom9:
            // Mixolydian
            intervals = {0, 2, 4, 5, 7, 9, 10};
            break;
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
            // Locrian
            intervals = {0, 1, 3, 5, 6, 8, 10};
            break;
        case ChordQuality::Major:
        case ChordQuality::Major7:
        case ChordQuality::Maj9:
        case ChordQuality::Add9:
        case ChordQuality::Sus2:
        case ChordQuality::Sus4:
        case ChordQuality::Augmented:
        default:
            // Major scale (Ionian)
            intervals = {0, 2, 4, 5, 7, 9, 11};
            break;
    }

    std::vector<int> pcs;
    pcs.reserve(intervals.size());
    for (int i : intervals) {
        pcs.push_back((root + i) % 12);
    }
    return pcs;
}

static int findClosestPitchClass(int pitchClass, const std::vector<int>& targetPcs) {
    if (targetPcs.empty()) return pitchClass;
    int bestPc = targetPcs.front();
    int minDistance = 999;

    for (int target : targetPcs) {
        int diff = std::abs(pitchClass - target);
        int distance = std::min(diff, 12 - diff);
        if (distance < minDistance) {
            minDistance = distance;
            bestPc = target;
        }
    }
    return bestPc;
}

int ChordTheory::remapPitchForChord(int originalPitch, const ChordEvent& chord, ChordFollowMode mode) {
    if (mode == ChordFollowMode::Off) return originalPitch;

    int originalPc = (originalPitch % 12 + 12) % 12;
    int octave = originalPitch / 12;

    switch (mode) {
        case ChordFollowMode::Bass: {
            int targetPc = (chord.bassPitchClass >= 0)
                ? ((chord.bassPitchClass % 12 + 12) % 12)
                : ((chord.rootPitchClass % 12 + 12) % 12);
            return std::clamp((octave * 12) + targetPc, 0, 127);
        }
        case ChordFollowMode::Chord: {
            std::vector<int> chordPcs = chord.getPitchClasses();
            int bestPc = findClosestPitchClass(originalPc, chordPcs);
            return std::clamp((octave * 12) + bestPc, 0, 127);
        }
        case ChordFollowMode::Scale: {
            std::vector<int> scalePcs = getScalePitchClassesForChord(chord);
            int bestPc = findClosestPitchClass(originalPc, scalePcs);
            return std::clamp((octave * 12) + bestPc, 0, 127);
        }
        case ChordFollowMode::ColorLead: {
            std::vector<int> chordPcs = chord.getPitchClasses();
            std::vector<int> scalePcs = getScalePitchClassesForChord(chord);
            bool inChord = (std::find(chordPcs.begin(), chordPcs.end(), originalPc) != chordPcs.end());
            bool inScale = (std::find(scalePcs.begin(), scalePcs.end(), originalPc) != scalePcs.end());
            if (inChord || inScale) {
                return originalPitch; // Preserve valid harmonic scale/chord tone
            }
            int bestPc = findClosestPitchClass(originalPc, chordPcs);
            return std::clamp((octave * 12) + bestPc, 0, 127);
        }
        default:
            return originalPitch;
    }
}

int ChordTheory::remapPitchForChord(int originalPitch, const ChordEvent& chord, const std::string& followModeStr) {
    return remapPitchForChord(originalPitch, chord, parseChordFollowMode(followModeStr));
}

bool ChordTheory::detectChordFromPitches(
    const std::vector<int>& midiPitches,
    int& outRoot,
    ChordQuality& outQuality,
    int& outBass) {

    if (midiPitches.empty()) return false;

    // Find lowest pitch as bass candidate
    int lowestPitch = *std::min_element(midiPitches.begin(), midiPitches.end());
    int bassPc = (lowestPitch % 12 + 12) % 12;

    std::set<int> activePcs;
    for (int p : midiPitches) {
        activePcs.insert((p % 12 + 12) % 12);
    }
    // Monophonic content (fewer than 2 distinct pitch classes) is never a chord
    if (activePcs.size() < 2) return false;

    int bestRoot = bassPc;
    ChordQuality bestQuality = ChordQuality::Major;
    float bestScore = -10.0f;

    static const ChordQuality kQualitiesToTest[] = {
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Dominant7,
        ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::Diminished,
        ChordQuality::Augmented, ChordQuality::HalfDiminished7, ChordQuality::Sus2,
        ChordQuality::Sus4, ChordQuality::Add9, ChordQuality::Min9,
        ChordQuality::Maj9, ChordQuality::Dom9
    };

    for (int root : activePcs) {
        for (ChordQuality quality : kQualitiesToTest) {
            std::vector<int> intervals = getChordQualityIntervals(quality);
            std::set<int> chordPcs;
            for (int iv : intervals) {
                chordPcs.insert((root + iv) % 12);
            }

            int matches = 0;
            for (int pc : chordPcs) {
                if (activePcs.find(pc) != activePcs.end()) matches++;
            }

            int outside = 0;
            for (int pc : activePcs) {
                if (chordPcs.find(pc) == chordPcs.end()) outside++;
            }

            int thirdPc = (root + intervals[1]) % 12;
            bool hasThird = (activePcs.find(thirdPc) != activePcs.end());
            bool hasRoot = (activePcs.find(root) != activePcs.end());

            float score = (static_cast<float>(matches) / static_cast<float>(chordPcs.size()));
            if (hasRoot) score += 0.5f;
            if (hasThird) score += 0.4f;
            if (root == bassPc) score += 0.3f;
            score -= static_cast<float>(outside) * 0.25f;

            if (score > bestScore) {
                bestScore = score;
                bestRoot = root;
                bestQuality = quality;
            }
        }
    }

    // Require sufficient harmonic match confidence
    if (bestScore < 0.85f) return false;

    outRoot = bestRoot;
    outQuality = bestQuality;
    outBass = (bassPc != bestRoot) ? bassPc : -1;
    return true;
}

std::vector<ChordEvent> ChordTheory::extractChordsFromNotes(
    const std::vector<TheoryNote>& notes,
    uint32_t startBar,
    uint32_t totalBars,
    uint32_t stepsPerBar) {

    if (notes.empty()) return {};

    uint32_t effectiveTotalBars = totalBars;
    if (effectiveTotalBars == 0) {
        float maxStep = 0.0f;
        for (const auto& n : notes) {
            float end = n.startStep + n.durationSteps;
            if (end > maxStep) maxStep = end;
        }
        effectiveTotalBars = static_cast<uint32_t>(std::ceil(maxStep / static_cast<float>(stepsPerBar)));
        if (effectiveTotalBars == 0) effectiveTotalBars = 1;
    }

    std::vector<ChordEvent> extracted;

    for (uint32_t bar = 0; bar < effectiveTotalBars; ++bar) {
        float barStart = static_cast<float>(bar * stepsPerBar);
        float barEnd = static_cast<float>((bar + 1) * stepsPerBar);

        std::vector<TheoryNote> barNotes;
        for (const auto& n : notes) {
            float noteEnd = n.startStep + n.durationSteps;
            if (n.startStep < barEnd && noteEnd > barStart) {
                barNotes.push_back(n);
            }
        }

        if (barNotes.empty()) continue;

        // Check for polyphonic content: find simultaneous sounding pitches
        std::vector<int> polyPitches;
        size_t maxSimultaneous = 0;

        for (const auto& n1 : barNotes) {
            float t = std::clamp(n1.startStep + 0.05f, barStart, barEnd - 0.01f);
            std::set<int> activePcsAtT;
            std::vector<int> pitchesAtT;
            for (const auto& n2 : barNotes) {
                float n2End = n2.startStep + n2.durationSteps;
                if (t >= n2.startStep && t < n2End) {
                    activePcsAtT.insert((n2.pitch % 12 + 12) % 12);
                    pitchesAtT.push_back(n2.pitch);
                }
            }
            if (activePcsAtT.size() > maxSimultaneous) {
                maxSimultaneous = activePcsAtT.size();
                polyPitches = pitchesAtT;
            }
        }

        // True chord events require polyphonic content (at least 2 simultaneous distinct pitch classes)
        if (maxSimultaneous < 2) continue;

        int detectedRoot = 0;
        ChordQuality detectedQuality = ChordQuality::Major;
        int detectedBass = -1;

        if (detectChordFromPitches(polyPitches, detectedRoot, detectedQuality, detectedBass)) {
            ChordEvent chord;
            chord.id = "chord_extracted_" + std::to_string(startBar + bar);
            chord.startBar = startBar + bar;
            chord.barLength = 1.0f;
            chord.rootPitchClass = detectedRoot;
            chord.quality = detectedQuality;
            chord.bassPitchClass = detectedBass;

            // Merge with previous chord if identical and consecutive
            if (!extracted.empty() &&
                extracted.back().rootPitchClass == chord.rootPitchClass &&
                extracted.back().quality == chord.quality &&
                extracted.back().bassPitchClass == chord.bassPitchClass &&
                static_cast<uint32_t>(extracted.back().startBar + extracted.back().barLength) == chord.startBar) {
                extracted.back().barLength += 1.0f;
            } else {
                extracted.push_back(chord);
            }
        }
    }

    return extracted;
}

} // namespace eatsbits::theory
