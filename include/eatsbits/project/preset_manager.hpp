#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include "eatsbits/project/preset_loader.hpp"
#include "eatsbits/project/eats_builtin_presets.hpp"

namespace eatsbits::project {

/**
 * @brief Descriptor for a preset script or built-in instrument/effect.
 */
struct PresetItem {
    std::string id;
    std::string name;
    std::string category;     // "INSTRUMENTS", "DRUMS", "AUDIO FX", "MIDI FX", "MIDI SEQ", "UTILITY"
    std::string subCategory;  // e.g. "Physical Modeling", "Keys", "Plucked", "Bowed", "Bass", "Synthesizers"
    std::string engineTag;    // e.g. "Physical Modeling", "Diode Ladder", "FM Synth", "BBD Chorus", "PCM Sampler"
    std::string description;
    std::string author{"Eatsbeats / Eatsbits"};
    std::string filePath{""};
    std::string rawScript{""};
    std::vector<std::string> tags{};
    float colorR{0.13f};
    float colorG{0.96f};
    float colorB{0.91f};
    bool isBuiltin{false};
};

/**
 * @brief Centralized manager for cataloging, indexing, and loading all presets from
 * filesystem directories (`presets/`) and built-in physical modeling registries.
 */
class PresetManager {
public:
    static PresetManager& instance();

    PresetManager(const PresetManager&) = delete;
    PresetManager& operator=(const PresetManager&) = delete;

    /**
     * @brief Discovers and indexes all presets across web and desktop paths.
     */
    void initialize(const std::string& customPresetsDir = "");
    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

    /**
     * @brief Returns complete list of all cataloged presets.
     */
    [[nodiscard]] const std::vector<PresetItem>& getAllPresets() const noexcept { return presets_; }

    /**
     * @brief Returns presets filtered by primary category ("INSTRUMENTS", "DRUMS", "AUDIO FX", etc.)
     */
    [[nodiscard]] std::vector<PresetItem> getPresetsByCategory(const std::string& category) const;

    /**
     * @brief Performs real-time fuzzy/substring search across name, description, tags, engine, and author.
     */
    [[nodiscard]] std::vector<PresetItem> searchPresets(const std::string& query, const std::string& category = "ALL") const;

    /**
     * @brief Looks up a preset item by its canonical ID.
     */
    [[nodiscard]] const PresetItem* findPreset(const std::string& id) const;

    /**
     * @brief Loads and parses the full PresetDefinition (parameters + GUI layout hierarchy).
     */
    bool loadPresetDefinition(const std::string& id, PresetDefinition& outDef) const;
    bool loadPresetDefinition(const PresetItem& item, PresetDefinition& outDef) const;

    /**
     * @brief Manually scans an additional directory for `.eats` preset files.
     */
    size_t scanDirectory(const std::string& dirPath, const std::string& defaultCategory = "");

    [[nodiscard]] size_t getPresetCount() const noexcept { return presets_.size(); }
    [[nodiscard]] size_t getCountByCategory(const std::string& category) const;

    /**
     * @brief Resolves signature category color tokens matching original Eatsbeats design system.
     */
    static void getCategoryColor(const std::string& category, float& r, float& g, float& b);

private:
    PresetManager() = default;

    void registerBuiltinPresets();
    void scanPresetsFolder(const std::string& basePath);
    void addOrUpdatePreset(const PresetItem& item);

    bool initialized_{false};
    std::vector<PresetItem> presets_;
    std::unordered_map<std::string, size_t> idToIndex_;
    mutable std::unordered_map<std::string, PresetDefinition> definitionCache_;
};

} // namespace eatsbits::project
