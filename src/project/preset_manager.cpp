#include "eatsbits/project/preset_manager.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace eatsbits::project {

namespace {

static std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::string toUpper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::string toLower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool stringContainsInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return (it != haystack.end());
}

static std::string humanizeIdentifier(const std::string& id) {
    std::string res;
    bool capNext = true;
    for (char c : id) {
        if (c == '_' || c == '-') {
            res += ' ';
            capNext = true;
        } else {
            res += capNext ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
            capNext = false;
        }
    }
    return res;
}

static std::string normalizeCategory(const std::string& cat, const std::string& folderHint = "") {
    std::string clean = toLower(cat);
    if (clean.empty() && !folderHint.empty()) {
        clean = toLower(folderHint);
    }

    if (clean.find("drum") != std::string::npos || clean.find("perc") != std::string::npos) {
        return "DRUMS";
    }
    if (clean.find("audio") != std::string::npos || clean.find("effect") != std::string::npos ||
        clean.find("fx") != std::string::npos && clean.find("midi") == std::string::npos) {
        return "AUDIO FX";
    }
    if (clean.find("midi_seq") != std::string::npos || clean.find("seq") != std::string::npos ||
        clean.find("pattern") != std::string::npos) {
        return "MIDI SEQ";
    }
    if (clean.find("midi") != std::string::npos) {
        return "MIDI FX";
    }
    if (clean.find("util") != std::string::npos || clean.find("macro") != std::string::npos ||
        clean.find("action") != std::string::npos || clean.find("split") != std::string::npos) {
        return "UTILITY";
    }
    return "INSTRUMENTS";
}

} // namespace

PresetManager& PresetManager::instance() {
    static PresetManager s_instance;
    return s_instance;
}

void PresetManager::getCategoryColor(const std::string& category, float& r, float& g, float& b) {
    std::string cat = toUpper(category);
    if (cat == "INSTRUMENTS" || cat == "INSTRUMENT" || cat == "SYNTH" || cat == "SYNTHS") {
        r = 0.13f; g = 0.96f; b = 0.91f; // Cyan
    } else if (cat == "DRUMS" || cat == "DRUM" || cat == "PERCUSSION") {
        r = 1.0f; g = 0.0f; b = 0.48f; // Neon Pink / Coral
    } else if (cat == "AUDIO FX" || cat == "AUDIO_FX" || cat == "EFFECTS" || cat == "AUDIO") {
        r = 0.74f; g = 0.0f; b = 1.0f; // Magenta / Deep Violet
    } else if (cat == "MIDI FX" || cat == "MIDI_FX") {
        r = 1.0f; g = 0.84f; b = 0.0f; // Gold / Amber
    } else if (cat == "MIDI SEQ" || cat == "MIDI_SEQ" || cat == "SEQUENCE") {
        r = 0.0f; g = 0.90f; b = 0.46f; // Emerald Green
    } else if (cat == "UTILITY" || cat == "MACRO" || cat == "ACTION") {
        r = 0.62f; g = 0.31f; b = 0.87f; // Violet
    } else {
        r = 0.13f; g = 0.96f; b = 0.91f; // Default Cyan
    }
}

void PresetManager::initialize(const std::string& customPresetsDir) {
    if (initialized_ && customPresetsDir.empty()) {
        return;
    }

    presets_.clear();
    idToIndex_.clear();
    definitionCache_.clear();

    // 1. Candidate paths to locate presets folder across environments
    std::vector<std::string> candidateBases;
    if (!customPresetsDir.empty()) {
        candidateBases.push_back(customPresetsDir);
    }
    candidateBases.push_back("/presets");                  // WebAssembly preloaded mount
    candidateBases.push_back("./presets");                  // Relative current working dir
    candidateBases.push_back("presets");                    // Simple relative
    candidateBases.push_back("../presets");                 // Relative from build/
    candidateBases.push_back("../../presets");              // Deep relative
    candidateBases.push_back("c:/git/eatsbits/presets");    // Windows repo path
    candidateBases.push_back("c:/git/eatsbeats/presets");   // Eatsbeats fallback

    std::string foundBasePath;
    for (const auto& path : candidateBases) {
        std::error_code ec;
        if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec)) {
            foundBasePath = path;
            break;
        }
    }

    if (!foundBasePath.empty()) {
        scanPresetsFolder(foundBasePath);
    }

    // 2. Register Built-in Physical Modeling and Synthesizer Instruments
    registerBuiltinPresets();

    initialized_ = true;
}

void PresetManager::scanPresetsFolder(const std::string& basePath) {
    std::error_code ec;

    // Scan subcategories
    const std::pair<std::string, std::string> subDirs[] = {
        {"instruments", "INSTRUMENTS"},
        {"drums", "DRUMS"},
        {"audio_fx", "AUDIO FX"},
        {"midi_fx", "MIDI FX"},
        {"midi_seq", "MIDI SEQ"},
        {"utility", "UTILITY"}
    };

    for (const auto& [sub, defaultCat] : subDirs) {
        std::filesystem::path dir = std::filesystem::path(basePath) / sub;
        if (std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec)) {
            scanDirectory(dir.string(), defaultCat);
        }
    }

    // Scan root of presets folder for any stray .eats
    for (const auto& entry : std::filesystem::directory_iterator(basePath, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".eats") {
            PresetItem item;
            item.filePath = entry.path().string();
            item.id = entry.path().stem().string();
            item.name = humanizeIdentifier(item.id);
            item.category = "INSTRUMENTS";
            getCategoryColor(item.category, item.colorR, item.colorG, item.colorB);

            // Read file header
            std::ifstream file(item.filePath);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                item.rawScript = buffer.str();

                std::istringstream stream(item.rawScript);
                std::string line;
                while (std::getline(stream, line)) {
                    std::string trimmed = trim(line);
                    if (trimmed.rfind("# @", 0) == 0 || trimmed.rfind("#@", 0) == 0) {
                        size_t colon = trimmed.find(':');
                        if (colon != std::string::npos) {
                            size_t tagStart = (trimmed[1] == '@') ? 2 : 3;
                            std::string key = toLower(trim(trimmed.substr(tagStart, colon - tagStart)));
                            std::string val = trim(trimmed.substr(colon + 1));
                            if (key == "id") item.id = val;
                            else if (key == "name") item.name = val;
                            else if (key == "category") item.category = normalizeCategory(val, "INSTRUMENTS");
                            else if (key == "description") item.description = val;
                            else if (key == "engine") item.engineTag = val;
                            else if (key == "author") item.author = val;
                        }
                    }
                }
            }
            getCategoryColor(item.category, item.colorR, item.colorG, item.colorB);
            addOrUpdatePreset(item);
        }
    }
}

size_t PresetManager::scanDirectory(const std::string& dirPath, const std::string& defaultCategory) {
    size_t count = 0;
    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec)) return 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".eats") {
            PresetItem item;
            item.filePath = entry.path().string();
            item.id = entry.path().stem().string();
            item.name = humanizeIdentifier(item.id);
            item.category = defaultCategory.empty() ? "INSTRUMENTS" : defaultCategory;

            std::ifstream file(item.filePath);
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                item.rawScript = buffer.str();

                std::istringstream stream(item.rawScript);
                std::string line;
                while (std::getline(stream, line)) {
                    std::string trimmed = trim(line);
                    if (trimmed.rfind("# @", 0) == 0 || trimmed.rfind("#@", 0) == 0) {
                        size_t colon = trimmed.find(':');
                        if (colon != std::string::npos) {
                            size_t tagStart = (trimmed[1] == '@') ? 2 : 3;
                            std::string key = toLower(trim(trimmed.substr(tagStart, colon - tagStart)));
                            std::string val = trim(trimmed.substr(colon + 1));
                            if (key == "id") item.id = val;
                            else if (key == "name") item.name = val;
                            else if (key == "category") item.category = normalizeCategory(val, defaultCategory);
                            else if (key == "description") item.description = val;
                            else if (key == "engine") item.engineTag = val;
                            else if (key == "author") item.author = val;
                            else if (key == "tags") {
                                std::istringstream tagStream(val);
                                std::string t;
                                while (std::getline(tagStream, t, ',')) {
                                    item.tags.push_back(trim(t));
                                }
                            }
                        }
                    }
                }
            }

            if (item.engineTag.empty()) {
                if (item.category == "DRUMS") item.engineTag = "Drum Voice / PCM";
                else if (item.category == "AUDIO FX") item.engineTag = "DSP Effect";
                else if (item.category == "MIDI FX") item.engineTag = "MIDI Processor";
                else if (item.category == "MIDI SEQ") item.engineTag = "Step Sequence";
                else if (item.category == "UTILITY") item.engineTag = "Macro / Utility";
                else item.engineTag = "Synthesizer";
            }

            getCategoryColor(item.category, item.colorR, item.colorG, item.colorB);
            addOrUpdatePreset(item);
            count++;
        }
    }
    return count;
}

void PresetManager::registerBuiltinPresets() {
    // Physical Modeling Catalog
    const auto& catalog = BuiltinPresetRegistry::getCatalogInfo();
    for (const auto& info : catalog) {
        auto it = idToIndex_.find(info.id);
        if (it != idToIndex_.end()) {
            // Preset was found in files: enrich with catalog metadata if missing
            auto& existing = presets_[it->second];
            if (existing.description.empty()) existing.description = info.description;
            if (existing.author == "Eatsbeats / Eatsbits") existing.author = info.author;
            existing.isBuiltin = true;
            continue;
        }

        // Add built-in instrument definition
        PresetItem item;
        item.id = info.id;
        item.name = info.name;
        item.category = (info.category == PresetCategory::Drums) ? "DRUMS" :
                        (info.category == PresetCategory::AudioFx) ? "AUDIO FX" :
                        (info.category == PresetCategory::MidiFx) ? "MIDI FX" :
                        (info.category == PresetCategory::MidiSeq) ? "MIDI SEQ" :
                        (info.category == PresetCategory::Utility) ? "UTILITY" : "INSTRUMENTS";

        switch (info.family) {
            case PhysicalModelFamily::Keyboards:
                item.subCategory = "Keyboards";
                item.engineTag = "Acoustic / Waveguide";
                break;
            case PhysicalModelFamily::PluckedStrings:
                item.subCategory = "Plucked Strings";
                item.engineTag = "Commuted Waveguide";
                break;
            case PhysicalModelFamily::BowedStrings:
                item.subCategory = "Bowed Strings";
                item.engineTag = "Stick-Slip Friction";
                break;
            case PhysicalModelFamily::BassInstruments:
                item.subCategory = "Bass";
                item.engineTag = "Resonant String";
                break;
            case PhysicalModelFamily::TunedPercussion:
                item.subCategory = "Tuned Percussion";
                item.engineTag = "Modal Bar Resonator";
                break;
            case PhysicalModelFamily::WindAndOrgan:
                item.subCategory = "Wind & Organ";
                item.engineTag = "Jet Flue Resonator";
                break;
            case PhysicalModelFamily::VocalAndSpeech:
                item.subCategory = "Vocal & Speech";
                item.engineTag = "Acoustic Vocal Tract";
                break;
            case PhysicalModelFamily::ElectronicSynth:
                item.subCategory = "Electronic Synth";
                item.engineTag = "Analog & Digital DSP";
                break;
            default:
                item.subCategory = "DSP";
                item.engineTag = "Physical Modeling";
                break;
        }

        item.description = info.description;
        item.author = info.author;
        item.isBuiltin = true;
        getCategoryColor(item.category, item.colorR, item.colorG, item.colorB);
        addOrUpdatePreset(item);
    }
}

void PresetManager::addOrUpdatePreset(const PresetItem& item) {
    auto it = idToIndex_.find(item.id);
    if (it != idToIndex_.end()) {
        presets_[it->second] = item;
    } else {
        idToIndex_[item.id] = presets_.size();
        presets_.push_back(item);
    }
}

std::vector<PresetItem> PresetManager::getPresetsByCategory(const std::string& category) const {
    if (category.empty() || toUpper(category) == "ALL") {
        return presets_;
    }

    std::string targetCat = toUpper(category);
    std::vector<PresetItem> results;
    for (const auto& item : presets_) {
        if (toUpper(item.category) == targetCat) {
            results.push_back(item);
        }
    }
    return results;
}

std::vector<PresetItem> PresetManager::searchPresets(const std::string& query, const std::string& category) const {
    std::string q = trim(query);
    std::string cat = toUpper(trim(category));

    std::vector<PresetItem> results;
    results.reserve(presets_.size());

    for (const auto& item : presets_) {
        // Category filter
        if (cat != "ALL" && !cat.empty()) {
            if (toUpper(item.category) != cat && toUpper(item.subCategory) != cat) {
                continue;
            }
        }

        // Query filter
        if (!q.empty()) {
            bool matches = stringContainsInsensitive(item.name, q) ||
                           stringContainsInsensitive(item.description, q) ||
                           stringContainsInsensitive(item.engineTag, q) ||
                           stringContainsInsensitive(item.author, q) ||
                           stringContainsInsensitive(item.id, q) ||
                           stringContainsInsensitive(item.subCategory, q);

            if (!matches) {
                for (const auto& tag : item.tags) {
                    if (stringContainsInsensitive(tag, q)) {
                        matches = true;
                        break;
                    }
                }
            }
            if (!matches) continue;
        }

        results.push_back(item);
    }
    return results;
}

const PresetItem* PresetManager::findPreset(const std::string& id) const {
    auto it = idToIndex_.find(id);
    if (it != idToIndex_.end()) {
        return &presets_[it->second];
    }
    return nullptr;
}

bool PresetManager::loadPresetDefinition(const std::string& id, PresetDefinition& outDef) const {
    const PresetItem* item = findPreset(id);
    if (!item) return false;
    return loadPresetDefinition(*item, outDef);
}

bool PresetManager::loadPresetDefinition(const PresetItem& item, PresetDefinition& outDef) const {
    // 1. Check in-memory definition cache
    auto it = definitionCache_.find(item.id);
    if (it != definitionCache_.end()) {
        outDef = it->second;
        return true;
    }

    // 2. Load from EatScript if script content exists
    if (!item.rawScript.empty()) {
        try {
            outDef = PresetLoader::parseFromEatscript(item.rawScript);
            definitionCache_[item.id] = outDef;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[PresetManager] Error parsing preset script '" << item.id << "': " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[PresetManager] Unknown error parsing preset script '" << item.id << "'" << std::endl;
        }
    }

    // 3. Load from file if filePath exists
    if (!item.filePath.empty()) {
        if (PresetLoader::loadFromFile(item.filePath, outDef)) {
            definitionCache_[item.id] = outDef;
            return true;
        }
    }

    // 4. Fallback to BuiltinPresetRegistry if it's a built-in instrument
    auto builtinOpt = BuiltinPresetRegistry::createPresetById(item.id);
    if (builtinOpt.has_value()) {
        outDef = *builtinOpt;
        definitionCache_[item.id] = outDef;
        return true;
    }

    return false;
}

size_t PresetManager::getCountByCategory(const std::string& category) const {
    if (category.empty() || toUpper(category) == "ALL") {
        return presets_.size();
    }
    std::string targetCat = toUpper(category);
    size_t count = 0;
    for (const auto& p : presets_) {
        if (toUpper(p.category) == targetCat) {
            count++;
        }
    }
    return count;
}

} // namespace eatsbits::project
