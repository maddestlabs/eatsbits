#ifndef EATS_PRESET_LOADER_HPP
#define EATS_PRESET_LOADER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace eatsbits::project {

struct PresetMetadata {
    std::string id{"generic_synth"};
    std::string name{"Generic Synth"};
    std::string category{"instrument"};
    std::string description{""};
    std::string engineId{""};
};

struct PresetParam {
    std::string name;
    float minVal{0.0f};
    float maxVal{1.0f};
    float defaultVal{0.5f};
    float currentVal{0.5f};
    float step{0.0f};
    std::string unit{""};
    bool allowVariance{true};

    [[nodiscard]] float getNormalized() const noexcept {
        if (maxVal <= minVal) return 0.0f;
        return std::clamp((currentVal - minVal) / (maxVal - minVal), 0.0f, 1.0f);
    }

    void setNormalized(float norm) noexcept {
        norm = std::clamp(norm, 0.0f, 1.0f);
        currentVal = minVal + norm * (maxVal - minVal);
        if (step > 0.0f) {
            currentVal = minVal + std::round((currentVal - minVal) / step) * step;
            currentVal = std::clamp(currentVal, minVal, maxVal);
        }
    }

    [[nodiscard]] std::string getFormatted() const {
        std::ostringstream oss;
        if (std::abs(currentVal) >= 1000.0f) {
            oss << std::fixed << std::setprecision(1) << (currentVal / 1000.0f) << "k";
        } else if (std::abs(currentVal) >= 10.0f) {
            oss << std::fixed << std::setprecision(0) << currentVal;
        } else {
            oss << std::fixed << std::setprecision(2) << currentVal;
        }
        if (!unit.empty()) {
            oss << " " << unit;
        }
        return oss.str();
    }
};

enum class GuiNodeType {
    Panel,
    Row,
    Column,
    Group,
    Knob,
    Nixie,
    Slider,
    Scope,
    Divider,
    Spacer
};

struct GuiLayoutNode {
    GuiNodeType type{GuiNodeType::Panel};
    std::string title;
    std::string subtitle;
    std::string background{"dark"}; // "minimal_white", "dark", "silver", "wood", "grunge"
    std::string accent{"#00FFE0"};
    std::string defaultKnobStyle{"chrome"}; // "chrome", "standard", "hardware", "minimalWhite"

    // Layout properties
    std::string orientation{"horizontal"}; // "horizontal", "vertical"
    std::string align{"center"}; // "start", "center", "end", "space_around", "space_between"
    std::string crossAlign{"center"};

    // Widget specific properties
    std::string paramName;
    std::string label;
    std::string unit;
    float size{52.0f};
    std::string hardwareStyle{"standard"}; // "tb303_selector", "tb303_potentiometer", "chromeFluted", "vintageBakelite"
    bool showValue{true};

    // Computed layout bounds (in local/faceplate coordinate space)
    float boundsX{0.0f};
    float boundsY{0.0f};
    float boundsW{0.0f};
    float boundsH{0.0f};

    std::string rackSides{"none"}; // "none", "rosewood", "aluminum", "dark_walnut"
    std::vector<GuiLayoutNode> children;
};

struct PresetDefinition {
    PresetMetadata metadata;
    std::map<std::string, PresetParam> params;
    GuiLayoutNode guiRoot;
    GuiLayoutNode compactGuiRoot;
    std::string rawScript;

    PresetParam* findParam(const std::string& name) {
        auto it = params.find(name);
        return (it != params.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] const PresetParam* findParam(const std::string& name) const {
        auto it = params.find(name);
        return (it != params.end()) ? &it->second : nullptr;
    }
};

/**
 * Loads, parses, and manages Eatsbeats `.eats` preset files,
 * extracting parameter dictionaries and declarative GUI trees.
 */
class PresetLoader {
public:
    static PresetDefinition parseFromEatscript(const std::string& scriptSource);
    static bool loadFromFile(const std::string& filePath, PresetDefinition& outPreset);
    static std::vector<PresetDefinition> loadDirectory(const std::string& dirPath);

    // Built-in presets (ensuring out-of-the-box availability without filesystem dependency)
    static std::vector<PresetDefinition> getBuiltinPresets();
    static PresetDefinition createEats303Preset();
    static PresetDefinition createAnalog808KickPreset();
    static PresetDefinition createAnalog909SnarePreset();
    static PresetDefinition createC64SidPreset();
    static PresetDefinition createYamahaDx7Preset();
    static PresetDefinition createSnesPreset();
    static PresetDefinition createYm2612Preset();
    static PresetDefinition createConvolverPreset();
    static PresetDefinition createStereoDelayPreset();
    static PresetDefinition createConcertGrandPianoPreset();
    static PresetDefinition createUprightBassPreset();
    static PresetDefinition createSpanishGuitarPreset();
    static PresetDefinition createSteelAcousticGuitarPreset();

    // Compute bounding boxes for declarative layout hierarchy
    static void computeLayoutBounds(GuiLayoutNode& node, float x, float y, float availableW, float availableH);
};

} // namespace eatsbits::project

#endif // EATS_PRESET_LOADER_HPP
