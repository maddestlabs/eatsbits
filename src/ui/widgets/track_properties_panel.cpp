#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "eatsbits/ui/widgets/track_properties_panel.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/icon_registry.hpp"
#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/ui/floating_window_manager.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <iomanip>
#include "eatsbits/presenter/track_properties_presenter.hpp"
#include "eatsbits/audio/dsp/snes_sfxr_generator.hpp"
#include <atomic>
#include <unordered_set>

namespace {
inline bool stringEqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](char c1, char c2) noexcept {
            return std::tolower(static_cast<unsigned char>(c1)) ==
                   std::tolower(static_cast<unsigned char>(c2));
        });
}

inline eatsbits::ui::ValueEditRequest toValueEditRequest(const eatsbits::presenter::ValueEditConfig& cfg,
                                                        const eatsbits::ui::Color& accent) {
    eatsbits::ui::ValueEditRequest req;
    req.title = cfg.title;
    req.paramName = cfg.paramName;
    req.isTextMode = cfg.isTextMode;
    req.initialText = cfg.initialText;
    req.currentValue = cfg.currentValue;
    req.minValue = cfg.minValue;
    req.maxValue = cfg.maxValue;
    req.defaultValue = cfg.defaultValue;
    req.hasDefault = cfg.hasDefault;
    req.isInteger = cfg.isInteger;
    req.allowPercentage = cfg.allowPercentage;
    req.unit = cfg.unit;
    req.accentColor = accent;
    req.onCommit = cfg.onCommit;
    req.onCommitText = cfg.onCommitText;
    return req;
}
} // namespace

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace eatsbits::ui {

static const float kQuickPalette[6][3] = {
    {0.55f, 0.57f, 0.62f}, // Gray
    {0.13f, 0.96f, 0.91f}, // Cyan
    {0.15f, 0.90f, 0.40f}, // Green
    {0.95f, 0.15f, 0.85f}, // Magenta
    {0.95f, 0.22f, 0.22f}, // Red
    {1.0f,  0.88f, 0.10f}  // Yellow
};

static float computeMidiFxRackHeight(const TrackPropertiesDrawerData& data) {
    float h = 42.0f; // Header
    if (data.midiFx.empty()) {
        h += 38.0f;
    } else {
        for (const auto& fx : data.midiFx) {
            h += (fx.isExpanded ? 180.0f : 42.0f) + 8.0f;
        }
    }
    return h + 4.0f;
}

static float computeAudioFxRackHeight(const TrackPropertiesDrawerData& data) {
    float h = 42.0f; // Header
    if (data.audioFx.empty()) {
        h += 38.0f;
    } else {
        for (const auto& fx : data.audioFx) {
            h += (fx.isExpanded ? 180.0f : 42.0f) + 8.0f;
        }
    }
    return h + 4.0f;
}

static void drawModernPillSwitch(BatchRenderer2D& r, float x, float y, bool enabled, const Color& activeColor) {
    float w = 15.0f;
    float h = 26.0f;
    float rad = 7.5f;

    // Outer dark pill housing shadow
    drawRoundedRect(r, x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, rad + 1.0f, 0.05f, 0.06f, 0.09f, 0.6f);

    // Pill body
    if (enabled) {
        drawRoundedRect(r, x, y, w, h, rad, activeColor.r, activeColor.g, activeColor.b, 1.0f);
        drawRoundedRectOutline(r, x, y, w, h, rad, activeColor.r * 1.2f, activeColor.g * 1.2f, activeColor.b * 1.2f, 0.9f, 1.0f);
    } else {
        drawRoundedRect(r, x, y, w, h, rad, 0.16f, 0.18f, 0.22f, 1.0f);
        drawRoundedRectOutline(r, x, y, w, h, rad, 0.25f, 0.28f, 0.35f, 0.8f, 1.0f);
    }

    // Circular metallic slider thumb
    float thumbR = 5.8f;
    float thumbCx = x + 7.5f;
    float thumbCy = enabled ? (y + 7.5f) : (y + h - 7.5f);

    // Drop shadow
    drawCircle(r, thumbCx, thumbCy + 1.0f, thumbR + 0.8f, 0.04f, 0.05f, 0.07f, 0.65f);

    // 3-stop metallic radial gradient
    drawCircleRadial3StopGradient(r, thumbCx, thumbCy, thumbR,
                                  Color(0.98f, 0.98f, 1.0f, 1.0f),
                                  Color(0.75f, 0.78f, 0.84f, 1.0f),
                                  Color(0.38f, 0.40f, 0.46f, 1.0f),
                                  -1.5f, -1.5f, 0.50f, 20);

    // Metallic rim outline
    drawCircleOutline(r, thumbCx, thumbCy, thumbR, 0.28f, 0.30f, 0.36f, 0.95f, 1.0f);
}

TrackPropertiesPanel::TrackPropertiesPanel() {
    // Configure embedded PluginSearchDialog
    pluginDialog_.onPluginSelected = [this](PluginDialogMode mode, const PluginEntry& entry, uint32_t trackIndex) {
        if (mode == PluginDialogMode::AddInstrument) {
            if (onChangeInstrument) onChangeInstrument(trackIndex);
        } else if (mode == PluginDialogMode::AddMidiFx) {
            if (onAddMidiFx) onAddMidiFx(trackIndex);
        } else if (mode == PluginDialogMode::AddAudioFx) {
            if (onAddAudioFx) onAddAudioFx(trackIndex);
        }
    };

    // Configure embedded ColorPickerDialog
    colorPickerDialog_.onColorSelected = [this](const Color& col, size_t trkIdx) {
        if (onColorChanged) onColorChanged(static_cast<uint32_t>(trkIdx), col.r, col.g, col.b);
    };
}

GuiPanelDef createGuiPanelFromPresetDefinition(const eatsbits::project::PresetDefinition& def) {
    GuiPanelDef panel;
    panel.title = def.guiRoot.title.empty() ? (def.metadata.name.empty() ? def.metadata.id : def.metadata.name) : def.guiRoot.title;
    panel.subtitle = def.guiRoot.subtitle.empty() ? def.metadata.description : def.guiRoot.subtitle;
    panel.woodCheeks = (def.guiRoot.rackSides != "none" && !def.guiRoot.rackSides.empty());
    panel.cornerRadius = 8.0f;

    std::string bg = def.guiRoot.background;
    for (char& c : bg) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (bg == "minimal_white" || bg == "white") panel.chassisStyle = GuiChassisStyle::MinimalWhite;
    else if (bg == "silver") panel.chassisStyle = GuiChassisStyle::Silver;
    else if (bg == "walnut" || bg == "wood" || bg == "blonde_pine" || bg == "pine" || bg == "spruce_top" || bg == "spanish_cedar" || bg == "cedar") panel.chassisStyle = GuiChassisStyle::Walnut;
    else if (bg == "rosewood") panel.chassisStyle = GuiChassisStyle::Rosewood;
    else if (bg == "snes" || bg == "snes_faceplate") panel.chassisStyle = GuiChassisStyle::Snes;
    else if (bg == "grunge") panel.chassisStyle = GuiChassisStyle::Grunge;
    else if (bg == "pcb" || bg == "pcb_green") panel.chassisStyle = GuiChassisStyle::PcbGreen;
    else if (bg == "brushed_steel") panel.chassisStyle = GuiChassisStyle::BrushedSteel;
    else if (bg == "brushed_aluminum" || bg == "brushed_silver" || bg == "aluminum" || bg == "brushed_metal") panel.chassisStyle = GuiChassisStyle::BrushedAluminum;
    else if (bg == "carbon" || bg == "carbon_fibre" || bg == "carbon_fiber") panel.chassisStyle = GuiChassisStyle::Carbon;
    else if (bg == "crinkle" || bg == "crinkle_paint") panel.chassisStyle = GuiChassisStyle::CrinklePaint;
    else if (bg == "bakelite") panel.chassisStyle = GuiChassisStyle::Bakelite;
    else if (bg == "matte" || bg == "matte_powder" || bg == "powder_coat") panel.chassisStyle = GuiChassisStyle::MattePowderCoat;
    else panel.chassisStyle = GuiChassisStyle::DarkChassis;

    std::string pat = def.guiRoot.pattern;
    for (char& c : pat) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (pat == "slats" || pat == "slat" || pat == "horizontal_slats") panel.patternOverride = VectorPatternType::HorizontalSlats;
    else if (pat == "steel" || pat == "brushed_steel" || pat == "brushed") panel.patternOverride = VectorPatternType::BrushedSteel;
    else if (pat == "wood" || pat == "woodgrain" || pat == "wood_grain") panel.patternOverride = VectorPatternType::WoodGrain;
    else if (pat == "grill" || pat == "speaker_grill" || pat == "fabric") panel.patternOverride = VectorPatternType::SpeakerGrill;
    else if (pat == "patina" || pat == "vintage" || pat == "vintage_patina") panel.patternOverride = VectorPatternType::VintagePatina;
    else if (pat == "mesh" || pat == "carbon" || pat == "diagonal_mesh") panel.patternOverride = VectorPatternType::DiagonalMesh;
    else if (pat == "honeycomb" || pat == "hex" || pat == "hex_honeycomb") panel.patternOverride = VectorPatternType::HexHoneycomb;
    else if (pat == "dots" || pat == "dot" || pat == "dot_grid") panel.patternOverride = VectorPatternType::DotGrid;
    else if (pat == "circuits" || pat == "circuit" || pat == "circuit_traces" || pat == "pcb") panel.patternOverride = VectorPatternType::CircuitTraces;
    else if (pat == "none") panel.patternOverride = VectorPatternType::None;

    if (!def.guiRoot.accent.empty() && def.guiRoot.accent[0] == '#' && def.guiRoot.accent.size() >= 7) {
        try {
            unsigned int hexVal = std::stoul(def.guiRoot.accent.substr(1, 6), nullptr, 16);
            panel.accentColor = Color(
                static_cast<float>((hexVal >> 16) & 0xFF) / 255.0f,
                static_cast<float>((hexVal >> 8) & 0xFF) / 255.0f,
                static_cast<float>(hexVal & 0xFF) / 255.0f,
                1.0f
            );
        } catch (...) {
            panel.accentColor = Color(0.0f, 0.95f, 1.0f, 1.0f);
        }
    } else {
        panel.accentColor = Color(0.0f, 0.95f, 1.0f, 1.0f);
    }

    auto convertWidgetNode = [&](const eatsbits::project::GuiLayoutNode& wNode) -> GuiWidgetDef {
        GuiWidgetDef w;
        w.id = "w_" + (wNode.paramName.empty() ? wNode.label : wNode.paramName);
        w.param = wNode.paramName;
        w.label = wNode.label.empty() ? wNode.paramName : wNode.label;
        w.sublabel = wNode.sublabel;
        w.centerGlyph = wNode.centerGlyph;
        w.unit = wNode.unit;
        w.size = (wNode.size > 0.0f) ? wNode.size : 52.0f;
        w.accentColor = panel.accentColor;
        w.showValueBox = wNode.showValueBox;
        w.action = wNode.action;

        if (wNode.labelAlignment == "inline_right") {
            w.labelAlignment = KnobLayoutMode::InlineRight;
        } else if (wNode.labelAlignment == "inline_left") {
            w.labelAlignment = KnobLayoutMode::InlineLeft;
        } else if (wNode.labelAlignment == "bottom_stacked") {
            w.labelAlignment = KnobLayoutMode::BottomStacked;
        } else {
            w.labelAlignment = KnobLayoutMode::StackedVertical;
        }

        if (wNode.type == eatsbits::project::GuiNodeType::Slider) {
            w.type = GuiWidgetType::Slider;
        } else if (wNode.type == eatsbits::project::GuiNodeType::Switch) {
            w.type = GuiWidgetType::ToggleSwitch;
            std::string swStyle = wNode.hardwareStyle;
            for (char& c : swStyle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (swStyle.find("slide") != std::string::npos) w.switchStyle = SwitchStyle::RectangularSlide;
            else w.switchStyle = SwitchStyle::VintageBatLever;
        } else if (wNode.type == eatsbits::project::GuiNodeType::Nixie) {
            w.type = GuiWidgetType::NixieDisplay;
        } else if (wNode.type == eatsbits::project::GuiNodeType::ListBox) {
            w.type = GuiWidgetType::ListBox;
            w.options = wNode.options;
            w.minVal = 0.0f;
            w.maxVal = static_cast<float>(std::max(0, static_cast<int>(wNode.options.size()) - 1));
        } else if (wNode.type == eatsbits::project::GuiNodeType::Button) {
            w.type = GuiWidgetType::ActionButton;
        } else if (wNode.type == eatsbits::project::GuiNodeType::Divider) {
            w.type = GuiWidgetType::Divider;
        } else {
            w.type = GuiWidgetType::Knob;
            std::string hs = wNode.hardwareStyle.empty() ? wNode.defaultKnobStyle : wNode.hardwareStyle;
            for (char& c : hs) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (hs == "tb303_selector_silver") w.knobStyle = GuiKnobStyle::Tb303SelectorSilver;
            else if (hs == "tb303_selector_black") w.knobStyle = GuiKnobStyle::Tb303SelectorBlack;
            else if (hs.find("tb303_selector") != std::string::npos || hs.find("selector") != std::string::npos) w.knobStyle = GuiKnobStyle::Tb303SelectorSilver;
            else if (hs.find("tb303_potentiometer") != std::string::npos || hs.find("potentiometer") != std::string::npos) w.knobStyle = GuiKnobStyle::Tb303Potentiometer;
            else if (hs.find("chicken") != std::string::npos) w.knobStyle = GuiKnobStyle::ChickenHead;
            else if (hs.find("cyan") != std::string::npos) w.knobStyle = GuiKnobStyle::StudioCyan;
            else if (hs.find("ruby") != std::string::npos) w.knobStyle = GuiKnobStyle::StudioRuby;
            else if (hs.find("magenta") != std::string::npos) w.knobStyle = GuiKnobStyle::StudioMagenta;
            else if (hs.find("dark") != std::string::npos) w.knobStyle = GuiKnobStyle::StudioDark;
            else if (hs.find("bipolar") != std::string::npos) w.knobStyle = GuiKnobStyle::StudioBipolar;
            else if (hs.find("neumorphic_pink") != std::string::npos || hs.find("lens_pink") != std::string::npos) w.knobStyle = GuiKnobStyle::NeumorphicLensPink;
            else if (hs.find("neumorphic") != std::string::npos || hs.find("lens") != std::string::npos || hs.find("volcano") != std::string::npos) w.knobStyle = GuiKnobStyle::NeumorphicLens;
            else if (hs.find("vintage") != std::string::npos || hs.find("bakelite") != std::string::npos) w.knobStyle = GuiKnobStyle::BakeliteSkirt;
            else if (hs.find("fluted") != std::string::npos || hs.find("cream") != std::string::npos) w.knobStyle = GuiKnobStyle::CreamFluted;
            else if (hs.find("knurled") != std::string::npos || hs.find("anodized") != std::string::npos) w.knobStyle = GuiKnobStyle::AnodizedKnurled;
            else if (hs.find("stepped") != std::string::npos || hs.find("twotone") != std::string::npos) w.knobStyle = GuiKnobStyle::TwoToneStepped;
            else if (hs.find("tb303") != std::string::npos || hs.find("303") != std::string::npos) w.knobStyle = GuiKnobStyle::Tb303Halo;
            else if (hs.find("encoder") != std::string::npos || hs.find("modern") != std::string::npos) w.knobStyle = GuiKnobStyle::ModernEncoder;
            else if (hs.find("rogan") != std::string::npos || hs.find("moog") != std::string::npos) w.knobStyle = GuiKnobStyle::RoganMoog;
            else if (hs.find("davies") != std::string::npos) w.knobStyle = GuiKnobStyle::DaviesFluted;
            else w.knobStyle = GuiKnobStyle::StudioClean;
        }

        const auto* p = def.findParam(wNode.paramName);
        if (p) {
            w.currentVal = p->currentVal;
            w.defaultVal = p->defaultVal;
            if (w.type != GuiWidgetType::ListBox) {
                w.minVal = p->minVal;
                w.maxVal = p->maxVal;
            }
            if (wNode.type == eatsbits::project::GuiNodeType::ListBox && !wNode.options.empty()) {
                int idx = std::clamp(static_cast<int>(std::round(p->currentVal)), 0, static_cast<int>(wNode.options.size() - 1));
                w.currentVal = static_cast<float>(idx);
                w.valueDisplay = wNode.options[idx];
            } else {
                w.valueDisplay = p->getFormatted();
            }
            if (w.unit.empty()) w.unit = p->unit;
        } else {
            w.currentVal = 0.5f;
            w.defaultVal = 0.5f;
            if (w.type != GuiWidgetType::ListBox) {
                w.minVal = 0.0f;
                w.maxVal = 1.0f;
            }
            if (wNode.type == eatsbits::project::GuiNodeType::ListBox && !wNode.options.empty()) {
                w.currentVal = 0.0f;
                w.valueDisplay = wNode.options[0];
            }
        }
        return w;
    };

    auto addRowOrGroup = [&](auto& self, const eatsbits::project::GuiLayoutNode& node) -> void {
        if (node.type == eatsbits::project::GuiNodeType::Row) {
            GuiRowDef row;
            for (const auto& wNode : node.children) {
                if (wNode.type == eatsbits::project::GuiNodeType::Column) {
                    for (const auto& colChild : wNode.children) {
                        row.widgets.push_back(convertWidgetNode(colChild));
                    }
                } else {
                    row.widgets.push_back(convertWidgetNode(wNode));
                }
            }
            if (!row.widgets.empty()) {
                panel.rows.push_back(row);
            }
        } else if (node.type == eatsbits::project::GuiNodeType::Group) {
            bool hasChildRows = false;
            for (const auto& ch : node.children) {
                if (ch.type == eatsbits::project::GuiNodeType::Row) {
                    hasChildRows = true;
                    self(self, ch);
                }
            }
            if (!hasChildRows && !node.children.empty()) {
                GuiRowDef row;
                for (const auto& wNode : node.children) {
                    if (wNode.type == eatsbits::project::GuiNodeType::Column) {
                        for (const auto& colChild : wNode.children) {
                            row.widgets.push_back(convertWidgetNode(colChild));
                        }
                    } else {
                        row.widgets.push_back(convertWidgetNode(wNode));
                    }
                }
                if (!row.widgets.empty()) {
                    panel.rows.push_back(row);
                }
            }
        } else {
            if (panel.rows.empty() || panel.rows.back().widgets.size() >= 6) {
                panel.rows.push_back(GuiRowDef{});
            }
            panel.rows.back().widgets.push_back(convertWidgetNode(node));
        }
    };

    for (const auto& childNode : def.guiRoot.children) {
        addRowOrGroup(addRowOrGroup, childNode);
    }

    if (panel.rows.empty() && !def.params.empty()) {
        std::vector<std::string> paramNames;
        paramNames.reserve(def.params.size());
        for (const auto& [name, _] : def.params) {
            paramNames.push_back(name);
        }
        std::sort(paramNames.begin(), paramNames.end());
        size_t expectedRows = (paramNames.size() + 5) / 6;
        for (size_t r = 0; r < expectedRows; ++r) {
            GuiRowDef row;
            size_t startIdx = r * 6;
            size_t endIdx = std::min(paramNames.size(), startIdx + 6);
            for (size_t i = startIdx; i < endIdx; ++i) {
                const auto* p = def.findParam(paramNames[i]);
                if (!p) continue;
                GuiWidgetDef w;
                w.id = "w_" + p->name;
                w.param = p->name;
                w.label = p->name;
                w.type = GuiWidgetType::Knob;
                w.knobStyle = GuiKnobStyle::StudioClean;
                w.size = 52.0f;
                w.minVal = p->minVal;
                w.maxVal = p->maxVal;
                w.currentVal = p->currentVal;
                w.unit = p->unit;
                w.valueDisplay = p->getFormatted();
                w.accentColor = panel.accentColor;
                row.widgets.push_back(w);
            }
            if (!row.widgets.empty()) {
                panel.rows.push_back(row);
            }
        }
    }

    return panel;
}

std::vector<TrackPropertiesKnob> extractKnobsFromPresetDefinition(const eatsbits::project::PresetDefinition& def) {
    std::vector<TrackPropertiesKnob> result;
    std::unordered_set<std::string> seen;
    auto collectFromNode = [&](auto& self, const eatsbits::project::GuiLayoutNode& node) -> void {
        if (!node.paramName.empty() && seen.find(node.paramName) == seen.end()) {
            seen.insert(node.paramName);
            TrackPropertiesKnob k;
            k.name = node.paramName;
            k.label = node.label.empty() ? node.paramName : node.label;
            k.unit = node.unit;
            const auto* p = def.findParam(node.paramName);
            if (p) {
                k.value = p->getNormalized();
                if (node.type == eatsbits::project::GuiNodeType::ListBox && !node.options.empty()) {
                    int idx = std::clamp(static_cast<int>(std::round(p->currentVal)), 0, static_cast<int>(node.options.size() - 1));
                    k.display = node.options[idx];
                    k.value = (node.options.size() > 1) ? (static_cast<float>(idx) / static_cast<float>(node.options.size() - 1)) : 0.0f;
                } else {
                    k.display = p->getFormatted();
                }
                if (k.unit.empty()) k.unit = p->unit;
            } else {
                if (node.type == eatsbits::project::GuiNodeType::ListBox && !node.options.empty()) {
                    k.value = 0.0f;
                    k.display = node.options[0];
                } else {
                    k.value = 0.5f;
                    k.display = "50%";
                }
            }
            result.push_back(k);
        }
        for (const auto& ch : node.children) {
            self(self, ch);
        }
    };
    collectFromNode(collectFromNode, def.guiRoot);

    std::vector<std::string> remaining;
    for (const auto& [name, _] : def.params) {
        if (seen.find(name) == seen.end()) {
            remaining.push_back(name);
        }
    }
    std::sort(remaining.begin(), remaining.end());
    for (const auto& name : remaining) {
        const auto* p = def.findParam(name);
        if (!p) continue;
        TrackPropertiesKnob k;
        k.name = p->name;
        k.label = p->name;
        k.value = p->getNormalized();
        k.display = p->getFormatted();
        k.unit = p->unit;
        result.push_back(k);
    }
    return result;
}

void TrackMidiFxItem::populateFromPresetDefinition(const eatsbits::project::PresetDefinition& def) {
    if (!def.metadata.id.empty()) id = def.metadata.id;
    if (!def.metadata.name.empty()) name = def.metadata.name;
    if (!def.metadata.engineId.empty()) type = def.metadata.engineId;
    if (!def.guiRoot.background.empty()) background = def.guiRoot.background;

    if (!def.guiRoot.accent.empty() && def.guiRoot.accent[0] == '#' && def.guiRoot.accent.size() >= 7) {
        try {
            unsigned int hexVal = std::stoul(def.guiRoot.accent.substr(1, 6), nullptr, 16);
            accentR = static_cast<float>((hexVal >> 16) & 0xFF) / 255.0f;
            accentG = static_cast<float>((hexVal >> 8) & 0xFF) / 255.0f;
            accentB = static_cast<float>(hexVal & 0xFF) / 255.0f;
        } catch (...) {}
    }

    knobs = extractKnobsFromPresetDefinition(def);
    customGuiPanel = createGuiPanelFromPresetDefinition(def);
    eatscriptCode = def.rawScript;
}

void populateCanonicalInstrumentGuiPanel(GuiPanelDef& guiPanel,
                                         const TrackPropertiesDrawerData& data,
                                         int draggingRow,
                                         int draggingWidget) {
    std::string_view desiredTitle = data.presetTitle.empty() ? std::string_view(data.instrument) : std::string_view(data.presetTitle);
    bool titleChanged = (guiPanel.title != desiredTitle);
    if (titleChanged) {
        guiPanel.title = desiredTitle;
    }

    bool subtitleChanged = false;
    if (!data.presetSubtitle.empty()) {
        if (guiPanel.subtitle != data.presetSubtitle) {
            guiPanel.subtitle = data.presetSubtitle;
            subtitleChanged = true;
        }
    } else {
        std::string_view tn = data.trackName;
        std::string_view sep = " • ";
        std::string_view inst = data.instrument;
        size_t neededLen = tn.size() + sep.size() + inst.size();
        bool matches = (guiPanel.subtitle.size() == neededLen) &&
                       (guiPanel.subtitle.compare(0, tn.size(), tn) == 0) &&
                       (guiPanel.subtitle.compare(tn.size(), sep.size(), sep) == 0) &&
                       (guiPanel.subtitle.compare(tn.size() + sep.size(), inst.size(), inst) == 0);
        if (!matches) {
            guiPanel.subtitle.clear();
            guiPanel.subtitle.reserve(neededLen);
            guiPanel.subtitle.append(tn);
            guiPanel.subtitle.append(sep);
            guiPanel.subtitle.append(inst);
            subtitleChanged = true;
        }
    }
    guiPanel.accentColor = Color(data.r, data.g, data.b, 1.0f);

    auto findKnobVal = [&](std::string_view p1, std::string_view p2, float def) -> float {
        // Pass 1: exact parameter name match
        for (const auto& k : data.knobs) {
            if (stringEqualsIgnoreCase(k.name, p1) || (!p2.empty() && stringEqualsIgnoreCase(k.name, p2))) {
                return k.value;
            }
        }
        // Pass 2: fallback to label match ONLY if name did not match
        for (const auto& k : data.knobs) {
            if (stringEqualsIgnoreCase(k.label, p1) || (!p2.empty() && stringEqualsIgnoreCase(k.label, p2))) {
                return k.value;
            }
        }
        return def;
    };

    auto getWidgetVal = [&](std::string_view p1, std::string_view p2, float defVal, float minV, float maxV) -> float {
        // Pass 1: exact parameter name match
        for (const auto& k : data.knobs) {
            if (stringEqualsIgnoreCase(k.name, p1) || (!p2.empty() && stringEqualsIgnoreCase(k.name, p2))) {
                if (maxV > minV) {
                    return minV + std::clamp(k.value, 0.0f, 1.0f) * (maxV - minV);
                }
                return k.value;
            }
        }
        // Pass 2: fallback to label match ONLY if name did not match
        for (const auto& k : data.knobs) {
            if (stringEqualsIgnoreCase(k.label, p1) || (!p2.empty() && stringEqualsIgnoreCase(k.label, p2))) {
                if (maxV > minV) {
                    return minV + std::clamp(k.value, 0.0f, 1.0f) * (maxV - minV);
                }
                return k.value;
            }
        }
        return defVal;
    };

    auto getListBoxVal = [&](const GuiWidgetDef& wid, float def) -> float {
        float raw = findKnobVal(wid.param, "", def);
        int totalOpts = static_cast<int>(wid.options.size());
        if (totalOpts > 1 && raw > 0.0f && raw < 1.0f) {
            raw = std::round(raw * static_cast<float>(totalOpts - 1));
        }
        return std::clamp(raw, 0.0f, static_cast<float>(std::max(0, totalOpts - 1)));
    };

    auto getNixieVal = [&](const GuiWidgetDef& wid, float def) -> float {
        float raw = findKnobVal(wid.param, "", def);
        if (raw > 0.0f && raw <= 1.0f) {
            raw = raw * wid.maxVal;
        }
        return std::clamp(raw, wid.minVal, wid.maxVal);
    };

    const std::string& eng = data.instrumentEngine;

    auto containsCaseInsensitive = [](std::string_view text, std::string_view needle) noexcept -> bool {
        if (needle.empty()) return true;
        if (text.size() < needle.size()) return false;
        auto it = std::search(text.begin(), text.end(), needle.begin(), needle.end(),
            [](char ch1, char ch2) {
                return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2));
            });
        return it != text.end();
    };

    bool isClav = (stringEqualsIgnoreCase(eng, "clavinet") ||
                   containsCaseInsensitive(data.instrument, "clav") ||
                   containsCaseInsensitive(data.presetTitle, "clav") ||
                   containsCaseInsensitive(data.instrument, "d6") ||
                   containsCaseInsensitive(data.presetTitle, "d6"));

    bool is303 = !isClav && (stringEqualsIgnoreCase(eng, "tb303") ||
                             containsCaseInsensitive(data.instrument, "303") ||
                             containsCaseInsensitive(data.presetTitle, "303"));

    bool isSfxr = !isClav && !is303 && (stringEqualsIgnoreCase(eng, "sfxr") ||
                                       stringEqualsIgnoreCase(eng, "eats_sfxr") ||
                                       containsCaseInsensitive(data.instrument, "sfxr") ||
                                       containsCaseInsensitive(data.presetTitle, "sfxr"));

    bool isSnes = !isClav && !is303 && !isSfxr && (stringEqualsIgnoreCase(eng, "snes") ||
                                                  stringEqualsIgnoreCase(eng, "snes_synth") ||
                                                  stringEqualsIgnoreCase(eng, "snes_console_synth") ||
                                                  containsCaseInsensitive(data.instrument, "snes") ||
                                                  containsCaseInsensitive(data.presetTitle, "snes"));

    bool isAstral = !isClav && !is303 && !isSfxr && !isSnes && (
        stringEqualsIgnoreCase(eng, "ambient_pad") ||
        stringEqualsIgnoreCase(eng, "astral_pad") ||
        stringEqualsIgnoreCase(eng, "astral_shimmer_pad") ||
        containsCaseInsensitive(data.instrument, "astral") ||
        containsCaseInsensitive(data.presetTitle, "astral") ||
        containsCaseInsensitive(data.instrument, "shimmer") ||
        containsCaseInsensitive(data.presetTitle, "shimmer")
    );

    bool isSteelGuitar = !isClav && !is303 && !isSfxr && !isSnes && !isAstral && (
        stringEqualsIgnoreCase(eng, "acoustic_steel_guitar") ||
        stringEqualsIgnoreCase(eng, "steel_acoustic_guitar") ||
        containsCaseInsensitive(data.instrument, "steel") ||
        containsCaseInsensitive(data.presetTitle, "steel")
    );

    if (isClav) {
        guiPanel.chassisStyle = GuiChassisStyle::Walnut;
        guiPanel.woodCheeks = false;
        guiPanel.hideHeader = true;
        guiPanel.cornerRadius = 6.0f;
        guiPanel.accentColor = Color(1.0f, 0.55f, 0.0f, 1.0f);

        bool needsRebuild = titleChanged || subtitleChanged ||
                            (guiPanel.rows.size() != 3 ||
                             guiPanel.rows[0].widgets.size() != 4 ||
                             guiPanel.rows[1].widgets.size() != 5 ||
                             guiPanel.rows[2].widgets.size() != 1);

        if (needsRebuild) {
            guiPanel.rows.clear();

            // Row 1: 4 Rocker Switches (Brilliant, Treble, Medium, Soft)
            GuiRowDef r1;
            auto addRocker = [&](const std::string& id, const std::string& label, const std::string& param, float defVal) {
                GuiWidgetDef sw;
                sw.id = id;
                sw.type = GuiWidgetType::ToggleSwitch;
                sw.label = label;
                sw.param = param;
                sw.currentVal = findKnobVal(param, "", defVal);
                sw.minVal = 0.0f;
                sw.maxVal = 1.0f;
                sw.size = 48.0f;
                sw.accentColor = Color(1.0f, 0.55f, 0.0f, 1.0f);
                r1.widgets.push_back(sw);
            };
            addRocker("w_brilliant", "BRILLIANT", "Brilliant", 1.0f);
            addRocker("w_treble", "TREBLE", "Treble", 1.0f);
            addRocker("w_medium", "MEDIUM", "Medium", 0.0f);
            addRocker("w_soft", "SOFT", "Soft", 0.0f);
            guiPanel.rows.push_back(r1);

            // Row 2: Knobs (Pickup A/B, Phase A-B, Hammer, Damper, Drive)
            GuiRowDef r2;
            auto addKnob = [&](const std::string& id, const std::string& label, const std::string& param, float defVal, float minV, float maxV) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.param = param;
                k.knobStyle = GuiKnobStyle::Tb303SelectorBlack;
                k.size = 52.0f;
                k.minVal = minV;
                k.maxVal = maxV;
                k.currentVal = getWidgetVal(param, "", defVal, minV, maxV);
                k.accentColor = Color(0.95f, 0.70f, 0.20f, 1.0f);
                r2.widgets.push_back(k);
            };
            addKnob("w_pickup", "PICKUP A/B", "PickupSelect", 0.5f, 0.0f, 1.0f);
            addRocker("w_phase", "PHASE (A-B)", "PhaseInvert", 0.0f);
            addKnob("w_hammer", "HAMMER", "HammerHardness", 1.4f, 0.2f, 3.0f);
            addKnob("w_damper", "DAMPER", "DamperThump", 0.45f, 0.0f, 1.0f);
            addKnob("w_drive", "DRIVE", "Drive", 1.08f, 0.5f, 3.0f);
            guiPanel.rows.push_back(r2);

            // Row 3: Horizontal Slider (STRING SUSTAIN & YARN DAMPING)
            GuiRowDef r3;
            GuiWidgetDef sl;
            sl.id = "w_sustain";
            sl.type = GuiWidgetType::Slider;
            sl.label = "STRING SUSTAIN & YARN DAMPING";
            sl.param = "Sustain";
            sl.minVal = 0.8f;
            sl.maxVal = 0.999f;
            sl.currentVal = getWidgetVal("Sustain", "", 0.993f, sl.minVal, sl.maxVal);
            sl.accentColor = Color(0.95f, 0.70f, 0.20f, 1.0f);
            r3.widgets.push_back(sl);
            guiPanel.rows.push_back(r3);
        } else {
            for (size_t rIdx = 0; rIdx < guiPanel.rows.size(); ++rIdx) {
                auto& row = guiPanel.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    if (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx)) continue;
                    w.currentVal = getWidgetVal(w.param, "", w.currentVal, w.minVal, w.maxVal);
                }
            }
        }
        return;
    }

    if (is303) {
        guiPanel.chassisStyle = GuiChassisStyle::MinimalWhite;
        guiPanel.woodCheeks = false;
        guiPanel.hideHeader = true;
        guiPanel.cornerRadius = 6.0f;

        bool needsRebuild = titleChanged || subtitleChanged ||
                            (guiPanel.rows.size() != 2 ||
                             guiPanel.rows[0].widgets.size() != 8 ||
                             guiPanel.rows[1].widgets.size() != 8);

        if (needsRebuild) {
            guiPanel.rows.clear();

            GuiRowDef r1;
            GuiWidgetDef wWave;
            wWave.id = "w_waveform";
            wWave.type = GuiWidgetType::ToggleSwitch;
            wWave.label = "WAVEFORM";
            wWave.param = "waveform";
            wWave.switchStyle = SwitchStyle::RectangularSlide;
            wWave.switchOrientation = SwitchOrientation::Vertical;
            wWave.switchSymbolTop = SwitchSymbol::SquareWave;
            wWave.switchSymbolBottom = SwitchSymbol::SawWave;
            wWave.options = {"Saw", "Square"};
            wWave.currentVal = findKnobVal("waveform", "wave", 0.0f);
            wWave.minVal = 0.0f;
            wWave.maxVal = 1.0f;
            wWave.size = 52.0f;
            wWave.accentColor = guiPanel.accentColor;
            r1.widgets.push_back(wWave);

            r1.widgets.push_back({"div1", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r1.widgets.push_back({"w_pitch", GuiWidgetType::Knob, "PITCH", "pitch", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("pitch", "tuning", 0.5f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r1.widgets.push_back({"w_cutoff", GuiWidgetType::Knob, "CUTOFF", "cutoff", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("cutoff", "", 0.65f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r1.widgets.push_back({"w_res", GuiWidgetType::Knob, "RESONANCE", "resonance", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("resonance", "reson", 0.75f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r1.widgets.push_back({"w_env", GuiWidgetType::Knob, "ENV MOD", "envMod", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("envMod", "env", 0.60f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r1.widgets.push_back({"w_decay", GuiWidgetType::Knob, "DECAY", "decay", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("decay", "", 0.45f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r1.widgets.push_back({"w_accent", GuiWidgetType::Knob, "ACCENT", "accent", GuiKnobStyle::Tb303Potentiometer, 52.0f, findKnobVal("accent", "", 0.75f), 0.0f, 1.0f, "", guiPanel.accentColor});
            guiPanel.rows.push_back(r1);

            GuiRowDef r2;
            GuiWidgetDef wOct;
            wOct.id = "w_octave";
            wOct.type = GuiWidgetType::Knob;
            wOct.label = "OCTAVE";
            wOct.param = "octave";
            wOct.knobStyle = GuiKnobStyle::Tb303SelectorBlack;
            wOct.size = 52.0f;
            wOct.minVal = 0.0f;
            wOct.maxVal = 10.0f;
            wOct.step = 1.0f;
            wOct.currentVal = findKnobVal("octave", "", 5.0f);
            wOct.accentColor = guiPanel.accentColor;
            r2.widgets.push_back(wOct);

            r2.widgets.push_back({"div2", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_subosc", GuiWidgetType::ToggleSwitch, "SUB OSC", "subOsc", GuiKnobStyle::Standard, 48.0f, findKnobVal("subOsc", "subWaveform", 0.0f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r2.widgets.push_back({"w_subvol", GuiWidgetType::Knob, "SUB VOL", "subVol", GuiKnobStyle::MiniPotCream, 48.0f, findKnobVal("subVol", "subVolume", 0.35f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r2.widgets.push_back({"div3", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_glide", GuiWidgetType::Knob, "GLIDE CURVE", "glideCurve", GuiKnobStyle::MiniPotCream, 48.0f, findKnobVal("glideCurve", "glide", 0.40f), 0.0f, 1.0f, "", guiPanel.accentColor});
            r2.widgets.push_back({"div4", GuiWidgetType::Divider, "", "", GuiKnobStyle::Standard, 14.0f, 0.0f, 0.0f, 1.0f, "", {}});
            r2.widgets.push_back({"w_drive", GuiWidgetType::Knob, "DRIVE", "drive", GuiKnobStyle::MiniPotCream, 48.0f, findKnobVal("drive", "overdrive", 0.25f), 0.0f, 1.0f, "", guiPanel.accentColor});
            guiPanel.rows.push_back(r2);
        } else {
            for (size_t rIdx = 0; rIdx < guiPanel.rows.size(); ++rIdx) {
                auto& row = guiPanel.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    if (w.type == GuiWidgetType::Divider) continue;
                    if (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx)) continue;
                    w.currentVal = findKnobVal(w.param, "", w.currentVal);
                }
            }
        }
        return;
    }

    if (isSfxr) {
        guiPanel.chassisStyle = GuiChassisStyle::Snes;
        guiPanel.woodCheeks = false;
        guiPanel.hideHeader = true;
        guiPanel.cornerRadius = 10.0f;
        guiPanel.accentColor = Color(0.90f, 0.15f, 0.15f, 1.0f);

        bool needsRebuild = titleChanged || subtitleChanged ||
                            (guiPanel.rows.size() != 2 ||
                             guiPanel.rows[0].widgets.size() != 4 ||
                             guiPanel.rows[1].widgets.size() != 9);

        if (needsRebuild) {
            guiPanel.rows.clear();

            GuiRowDef r1;
            GuiWidgetDef lbType;
            lbType.id = "w_sfxtype";
            lbType.type = GuiWidgetType::ListBox;
            lbType.label = "SFX TYPE";
            lbType.param = "SFXType";
            lbType.options = {"Laser", "Explosion", "Powerup", "Coin", "Jump", "Hurt", "Lose", "Button", "Warp", "Mutate", "Custom SNES"};
            lbType.currentVal = getListBoxVal(lbType, 0.0f);
            lbType.minVal = 0.0f;
            lbType.maxVal = static_cast<float>(std::max(0, static_cast<int>(lbType.options.size()) - 1));
            int typeIdx = std::clamp(static_cast<int>(std::round(lbType.currentVal)), 0, static_cast<int>(lbType.options.size() - 1));
            lbType.valueDisplay = lbType.options[typeIdx];
            r1.widgets.push_back(lbType);

            GuiWidgetDef lbWave;
            lbWave.id = "w_wavetable";
            lbWave.type = GuiWidgetType::ListBox;
            lbWave.label = "WAVETABLE";
            lbWave.param = "Waveform";
            lbWave.options = {"Square", "Pulse 25%", "Pulse 12%", "Sawtooth", "Triangle", "Sine", "Organ", "Strings", "Flute", "Slap Bass", "Chime", "Noise"};
            lbWave.currentVal = getListBoxVal(lbWave, 0.0f);
            lbWave.minVal = 0.0f;
            lbWave.maxVal = static_cast<float>(std::max(0, static_cast<int>(lbWave.options.size()) - 1));
            int waveIdx = std::clamp(static_cast<int>(std::round(lbWave.currentVal)), 0, static_cast<int>(lbWave.options.size() - 1));
            lbWave.valueDisplay = lbWave.options[waveIdx];
            r1.widgets.push_back(lbWave);

            GuiWidgetDef wSeed;
            wSeed.id = "w_seed";
            wSeed.type = GuiWidgetType::NixieDisplay;
            wSeed.label = "RNG SEED";
            wSeed.param = "Seed";
            wSeed.currentVal = getNixieVal(wSeed, 42.0f);
            wSeed.minVal = 0.0f;
            wSeed.maxVal = 9999.0f;
            r1.widgets.push_back(wSeed);

            GuiWidgetDef wRand;
            wRand.id = "w_randomize";
            wRand.type = GuiWidgetType::ActionButton;
            wRand.label = "RANDOMIZE";
            wRand.action = "randomize";
            r1.widgets.push_back(wRand);
            guiPanel.rows.push_back(r1);

            GuiRowDef r2;
            auto addKnob = [&](const std::string& id, const std::string& label, const std::string& param, float defVal, float minV, float maxV, const std::string& unit) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.param = param;
                k.knobStyle = GuiKnobStyle::TwoToneStepped;
                k.size = 46.0f;
                k.currentVal = findKnobVal(param, "", defVal);
                k.minVal = minV;
                k.maxVal = maxV;
                k.unit = unit;
                k.accentColor = Color(0.90f, 0.15f, 0.15f, 1.0f);
                r2.widgets.push_back(k);
            };

            addKnob("w_attack", "ATTACK", "Attack", 0.005f / 0.5f, 0.0f, 1.0f, "s");
            addKnob("w_decay", "DECAY", "Decay", 0.35f / 2.0f, 0.0f, 1.0f, "s");
            addKnob("w_sustain", "SUSTAIN", "Sustain", 0.20f, 0.0f, 1.0f, "");
            addKnob("w_release", "RELEASE", "Release", 0.20f / 2.0f, 0.0f, 1.0f, "s");
            addKnob("w_sweep", "SWEEP", "PitchSweep", (-0.45f + 2.0f) / 4.0f, 0.0f, 1.0f, "");
            addKnob("w_vibrate", "VIB RATE", "VibratoRate", 0.0f, 0.0f, 1.0f, "Hz");
            addKnob("w_vibdepth", "VIB DEPTH", "VibratoDepth", 0.0f, 0.0f, 1.0f, "");
            addKnob("w_echoms", "ECHO MS", "EchoDelay", 120.0f / 480.0f, 0.0f, 1.0f, "ms");
            addKnob("w_echovol", "ECHO VOL", "EchoVolume", 0.35f, 0.0f, 1.0f, "");
            guiPanel.rows.push_back(r2);
        } else {
            for (size_t rIdx = 0; rIdx < guiPanel.rows.size(); ++rIdx) {
                auto& row = guiPanel.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    if (w.type == GuiWidgetType::Divider || w.type == GuiWidgetType::ActionButton) continue;
                    if (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx)) continue;
                    if (w.type == GuiWidgetType::ListBox) {
                        w.currentVal = getListBoxVal(w, w.currentVal);
                        if (!w.options.empty()) {
                            int idx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, static_cast<int>(w.options.size() - 1));
                            w.valueDisplay = w.options[idx];
                        }
                    } else if (w.type == GuiWidgetType::NixieDisplay) {
                        w.currentVal = getNixieVal(w, w.currentVal);
                    } else {
                        w.currentVal = findKnobVal(w.param, "", w.currentVal);
                    }
                }
            }
        }
        return;
    }

    if (isSnes) {
        guiPanel.chassisStyle = GuiChassisStyle::Snes;
        guiPanel.woodCheeks = false;
        guiPanel.hideHeader = true;
        guiPanel.cornerRadius = 10.0f;
        guiPanel.accentColor = Color(0.48f, 0.42f, 0.72f, 1.0f);

        bool needsRebuild = titleChanged || subtitleChanged ||
                            (guiPanel.rows.size() != 2 ||
                             guiPanel.rows[0].widgets.size() != 4 ||
                             guiPanel.rows[1].widgets.size() != 9);

        if (needsRebuild) {
            guiPanel.rows.clear();

            GuiRowDef r1;
            GuiWidgetDef lbWave;
            lbWave.id = "w_wavetable";
            lbWave.type = GuiWidgetType::ListBox;
            lbWave.label = "WAVETABLE";
            lbWave.param = "Waveform";
            lbWave.options = {"Square", "Pulse 25%", "Pulse 12%", "Sawtooth", "Triangle", "Sine", "Organ", "Strings", "Flute", "Slap Bass", "Chime", "Noise"};
            lbWave.currentVal = getListBoxVal(lbWave, 0.0f);
            lbWave.minVal = 0.0f;
            lbWave.maxVal = static_cast<float>(std::max(0, static_cast<int>(lbWave.options.size()) - 1));
            int waveIdx = std::clamp(static_cast<int>(std::round(lbWave.currentVal)), 0, static_cast<int>(lbWave.options.size() - 1));
            lbWave.valueDisplay = lbWave.options[waveIdx];
            r1.widgets.push_back(lbWave);

            GuiWidgetDef lbFilter;
            lbFilter.id = "w_filtertype";
            lbFilter.type = GuiWidgetType::ListBox;
            lbFilter.label = "FILTER TYPE";
            lbFilter.param = "FilterType";
            lbFilter.options = {"Off", "LowPass", "HighPass", "BandPass"};
            lbFilter.currentVal = getListBoxVal(lbFilter, 0.0f);
            lbFilter.minVal = 0.0f;
            lbFilter.maxVal = static_cast<float>(std::max(0, static_cast<int>(lbFilter.options.size()) - 1));
            int filtIdx = std::clamp(static_cast<int>(std::round(lbFilter.currentVal)), 0, static_cast<int>(lbFilter.options.size() - 1));
            lbFilter.valueDisplay = lbFilter.options[filtIdx];
            r1.widgets.push_back(lbFilter);

            auto addKnob1 = [&](const std::string& id, const std::string& label, const std::string& param, float defVal, float minV, float maxV, const std::string& unit) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.param = param;
                k.knobStyle = GuiKnobStyle::TwoToneStepped;
                k.size = 48.0f;
                k.currentVal = findKnobVal(param, "", defVal);
                k.minVal = minV;
                k.maxVal = maxV;
                k.unit = unit;
                k.accentColor = Color(0.48f, 0.42f, 0.72f, 1.0f);
                r1.widgets.push_back(k);
            };
            addKnob1("w_cutoff", "CUTOFF", "Cutoff", 1.0f, 0.0f, 1.0f, "");
            addKnob1("w_resonance", "RESONANCE", "Resonance", 0.0f, 0.0f, 1.0f, "");
            guiPanel.rows.push_back(r1);

            GuiRowDef r2;
            auto addKnob2 = [&](const std::string& id, const std::string& label, const std::string& param, float defVal, float minV, float maxV, const std::string& unit) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.param = param;
                k.knobStyle = GuiKnobStyle::TwoToneStepped;
                k.size = 46.0f;
                k.currentVal = findKnobVal(param, "", defVal);
                k.minVal = minV;
                k.maxVal = maxV;
                k.unit = unit;
                k.accentColor = Color(0.48f, 0.42f, 0.72f, 1.0f);
                r2.widgets.push_back(k);
            };
            addKnob2("w_attack", "ATTACK", "Attack", 0.01f, 0.0f, 1.0f, "s");
            addKnob2("w_decay", "DECAY", "Decay", 0.15f, 0.0f, 1.0f, "s");
            addKnob2("w_sustain", "SUSTAIN", "Sustain", 0.50f, 0.0f, 1.0f, "");
            addKnob2("w_release", "RELEASE", "Release", 0.12f, 0.0f, 1.0f, "s");
            addKnob2("w_vibrate", "VIB RATE", "VibratoRate", 0.28f, 0.0f, 1.0f, "Hz");
            addKnob2("w_vibdepth", "VIB DEPTH", "VibratoDepth", 0.10f, 0.0f, 1.0f, "");
            addKnob2("w_echoms", "ECHO MS", "EchoDelay", 0.29f, 0.0f, 1.0f, "ms");
            addKnob2("w_echofdbk", "ECHO FDBK", "EchoFeedback", 0.55f, 0.0f, 1.0f, "");
            addKnob2("w_echovol", "ECHO VOL", "EchoVolume", 0.40f, 0.0f, 1.0f, "");
            guiPanel.rows.push_back(r2);
        } else {
            for (size_t rIdx = 0; rIdx < guiPanel.rows.size(); ++rIdx) {
                auto& row = guiPanel.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    if (w.type == GuiWidgetType::Divider || w.type == GuiWidgetType::ActionButton) continue;
                    if (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx)) continue;
                    if (w.type == GuiWidgetType::ListBox) {
                        w.currentVal = getListBoxVal(w, w.currentVal);
                        if (!w.options.empty()) {
                            int idx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, static_cast<int>(w.options.size() - 1));
                            w.valueDisplay = w.options[idx];
                        }
                    } else {
                        w.currentVal = findKnobVal(w.param, "", w.currentVal);
                    }
                }
            }
        }
        return;
    }

    if (isAstral) {
        guiPanel.chassisStyle = GuiChassisStyle::MinimalWhite;
        guiPanel.woodCheeks = false;
        guiPanel.hideHeader = false;
        guiPanel.cornerRadius = 8.0f;
        guiPanel.accentColor = Color(0.0f, 0.90f, 1.0f, 1.0f);

        bool needsRebuild = titleChanged || subtitleChanged ||
                            (guiPanel.rows.size() != 2 ||
                             guiPanel.rows[0].widgets.size() != 4 ||
                             guiPanel.rows[1].widgets.size() != 4);

        if (needsRebuild) {
            guiPanel.rows.clear();

            // Row 1: Timbre / Filter (Cutoff, Resonance, Detune, Warmth)
            GuiRowDef r1;
            auto addStudioKnob = [&](GuiRowDef& targetRow, const std::string& id, const std::string& label,
                                     const std::string& sublabel, const std::string& param, float defVal,
                                     float minV, float maxV, const std::string& unit, GuiKnobStyle kStyle) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.sublabel = sublabel;
                k.param = param;
                k.knobStyle = kStyle;
                k.labelAlignment = KnobLayoutMode::InlineRight;
                k.size = 52.0f;
                k.minVal = minV;
                k.maxVal = maxV;
                k.currentVal = getWidgetVal(param, "", defVal, minV, maxV);
                k.unit = unit;
                k.accentColor = (kStyle == GuiKnobStyle::StudioRuby) ? Color(0.84f, 0.14f, 0.20f, 1.0f) :
                               ((kStyle == GuiKnobStyle::StudioMagenta) ? Color(0.72f, 0.12f, 0.60f, 1.0f) :
                                 Color(0.0f, 0.90f, 1.0f, 1.0f));
                targetRow.widgets.push_back(k);
            };

            addStudioKnob(r1, "w_cutoff", "CUTOFF", "FILTER", "Cutoff", 2400.0f, 100.0f, 16000.0f, "Hz", GuiKnobStyle::StudioCyan);
            addStudioKnob(r1, "w_reso", "RESO", "Q PEAK", "Resonance", 0.55f, 0.1f, 5.0f, "", GuiKnobStyle::StudioRuby);
            addStudioKnob(r1, "w_detune", "DETUNE", "7-UNISON", "Detune", 14.0f, 1.0f, 35.0f, "ct", GuiKnobStyle::StudioMagenta);
            addStudioKnob(r1, "w_warmth", "WARMTH", "DRIVE", "Warmth", 0.65f, 0.0f, 1.0f, "", GuiKnobStyle::StudioClean);
            guiPanel.rows.push_back(r1);

            // Row 2: ADSR Envelope (Attack, Decay, Sustain, Release)
            GuiRowDef r2;
            addStudioKnob(r2, "w_attack", "ATTACK", "ENVELOPE", "Attack", 0.85f, 0.05f, 4.0f, "s", GuiKnobStyle::StudioCyan);
            addStudioKnob(r2, "w_decay", "DECAY", "STAGE", "Decay", 1.5f, 0.1f, 6.0f, "s", GuiKnobStyle::StudioCyan);
            addStudioKnob(r2, "w_sustain", "SUSTAIN", "LEVEL", "Sustain", 0.85f, 0.0f, 1.0f, "", GuiKnobStyle::StudioCyan);
            addStudioKnob(r2, "w_release", "RELEASE", "TAIL", "Release", 2.5f, 0.1f, 8.0f, "s", GuiKnobStyle::StudioCyan);
            guiPanel.rows.push_back(r2);
        } else {
            for (size_t rIdx = 0; rIdx < guiPanel.rows.size(); ++rIdx) {
                auto& row = guiPanel.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    auto& w = row.widgets[wIdx];
                    if (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx)) continue;
                    w.currentVal = getWidgetVal(w.param, "", w.currentVal, w.minVal, w.maxVal);
                }
            }
        }
        return;
    }

    // Default / Builtin Engines / CLAP Plugins
    guiPanel.hideHeader = false;
    guiPanel.woodCheeks = true;
    guiPanel.cornerRadius = 6.0f;
    if (eng == "tr808" || eng == "tr909") {
        guiPanel.chassisStyle = GuiChassisStyle::Grunge;
    } else if (eng == "dx7" || eng == "dxvii_epiano" || eng == "dxvii" || eng == "eats_dx") {
        guiPanel.chassisStyle = GuiChassisStyle::DarkChassis;
    } else if (eng == "piano" || eng == "piano_physical") {
        guiPanel.chassisStyle = GuiChassisStyle::Walnut;
    } else if (eng == "snes") {
        guiPanel.chassisStyle = GuiChassisStyle::Snes;
        guiPanel.woodCheeks = false;
    } else if (eng == "c64") {
        guiPanel.chassisStyle = GuiChassisStyle::PcbGreen;
        guiPanel.woodCheeks = false;
    } else if (eng == "convolver") {
        guiPanel.chassisStyle = GuiChassisStyle::BrushedSteel;
    } else if (isSteelGuitar) {
        guiPanel.chassisStyle = GuiChassisStyle::Walnut;
        guiPanel.woodCheeks = true;
        guiPanel.cornerRadius = 8.0f;
        guiPanel.accentColor = Color(0.85f, 0.47f, 0.02f, 1.0f); // #D97706
        guiPanel.title = "STEEL-STRING ACOUSTIC";
        guiPanel.subtitle = "Morphable Body Geometry (Parlor <-> Dreadnought <-> Jumbo)";

        bool needsRebuild = titleChanged || subtitleChanged ||
                            (guiPanel.rows.size() != 3 ||
                             guiPanel.rows[0].widgets.size() != 5 ||
                             guiPanel.rows[1].widgets.size() != 1 ||
                             guiPanel.rows[2].widgets.size() != 3);

        if (needsRebuild) {
            guiPanel.rows.clear();

            // Row 1: 5 knobs (BodyProfile, PickStyle, StrumSpread, Sustain, Damping)
            GuiRowDef r1;
            auto addVintageKnob = [&](const std::string& id, const std::string& label, const std::string& param, float defVal, float minV, float maxV, const std::string& unit) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.param = param;
                k.knobStyle = GuiKnobStyle::BakeliteSkirt;
                k.size = 52.0f;
                k.minVal = minV;
                k.maxVal = maxV;
                k.unit = unit;
                k.currentVal = getWidgetVal(param, "", defVal, minV, maxV);
                k.accentColor = guiPanel.accentColor;
                r1.widgets.push_back(k);
            };
            addVintageKnob("w_body_shape", "BODY SHAPE", "BodyProfile", 0.5f, 0.0f, 1.0f, "");
            addVintageKnob("w_pick_finger", "PICK / FINGER", "PickStyle", 0.3f, 0.0f, 1.0f, "");
            addVintageKnob("w_strum_ms", "STRUM MS", "StrumSpread", 5.0f, 1.0f, 25.0f, "ms");
            addVintageKnob("w_sustain", "SUSTAIN", "Sustain", 0.996f, 0.85f, 0.9995f, "");
            addVintageKnob("w_damping", "STRING LOSS", "Damping", 0.22f, 0.05f, 0.75f, "%");
            guiPanel.rows.push_back(r1);

            // Row 2: Capsule Slider (BodyProfile)
            GuiRowDef r2;
            GuiWidgetDef sl;
            sl.id = "w_body_profile";
            sl.type = GuiWidgetType::Slider;
            sl.label = "BODY PROFILE (0.0 PARLOR/000 <-> 0.5 DREADNOUGHT <-> 1.0 JUMBO)";
            sl.param = "BodyProfile";
            sl.minVal = 0.0f;
            sl.maxVal = 1.0f;
            sl.currentVal = getWidgetVal("BodyProfile", "", 0.5f, 0.0f, 1.0f);
            sl.accentColor = guiPanel.accentColor;
            r2.widgets.push_back(sl);
            guiPanel.rows.push_back(r2);

            // Row 3: 3 knobs (BronzeSparkle, BodyWood, WoundScrape)
            GuiRowDef r3;
            auto addVintageKnob3 = [&](const std::string& id, const std::string& label, const std::string& param, float defVal, float minV, float maxV, const std::string& unit) {
                GuiWidgetDef k;
                k.id = id;
                k.type = GuiWidgetType::Knob;
                k.label = label;
                k.param = param;
                k.knobStyle = GuiKnobStyle::BakeliteSkirt;
                k.size = 52.0f;
                k.minVal = minV;
                k.maxVal = maxV;
                k.unit = unit;
                k.currentVal = getWidgetVal(param, "", defVal, minV, maxV);
                k.accentColor = guiPanel.accentColor;
                r3.widgets.push_back(k);
            };
            addVintageKnob3("w_bronze_air", "BRONZE AIR", "BronzeSparkle", 2.0f, -6.0f, 8.0f, "dB");
            addVintageKnob3("w_body_wood", "SOUNDBOARD GAIN", "BodyWood", 0.5f, 0.0f, 1.5f, "");
            addVintageKnob3("w_wound_scrape", "WOUND SCRAPE", "WoundScrape", 0.4f, 0.0f, 1.0f, "");
            guiPanel.rows.push_back(r3);
        } else {
            auto updateVal = [&](size_t r, size_t w, const std::string& param, float defVal, float minV, float maxV) {
                if (r < guiPanel.rows.size() && w < guiPanel.rows[r].widgets.size()) {
                    if (draggingRow == static_cast<int>(r) && draggingWidget == static_cast<int>(w)) return;
                    guiPanel.rows[r].widgets[w].currentVal = getWidgetVal(param, "", defVal, minV, maxV);
                }
            };
            updateVal(0, 0, "BodyProfile", 0.5f, 0.0f, 1.0f);
            updateVal(0, 1, "PickStyle", 0.3f, 0.0f, 1.0f);
            updateVal(0, 2, "StrumSpread", 5.0f, 1.0f, 25.0f);
            updateVal(0, 3, "Sustain", 0.996f, 0.85f, 0.9995f);
            updateVal(0, 4, "Damping", 0.22f, 0.05f, 0.75f);
            updateVal(1, 0, "BodyProfile", 0.5f, 0.0f, 1.0f);
            updateVal(2, 0, "BronzeSparkle", 2.0f, -6.0f, 8.0f);
            updateVal(2, 1, "BodyWood", 0.5f, 0.0f, 1.5f);
            updateVal(2, 2, "WoundScrape", 0.4f, 0.0f, 1.0f);
        }
        return;
    } else {
        guiPanel.chassisStyle = GuiChassisStyle::DarkChassis;
    }

    size_t numKnobs = data.knobs.size();
    if (numKnobs == 0) {
        guiPanel.rows.clear();
        return;
    }

    size_t totalWidgetsInRows = 0;
    for (const auto& r : guiPanel.rows) totalWidgetsInRows += r.widgets.size();

    // Multi-row knob layout supporting up to 24 parameters (CLAP plugins & synthesizers)
    size_t displayKnobs = std::min(numKnobs, size_t{24});
    size_t expectedRows = (displayKnobs + 5) / 6;

    bool needsRebuild = titleChanged || subtitleChanged ||
                        (guiPanel.rows.size() != expectedRows || totalWidgetsInRows != displayKnobs);

    if (needsRebuild) {
        guiPanel.rows.clear();
        for (size_t rowIdx = 0; rowIdx < expectedRows; ++rowIdx) {
            GuiRowDef rDef;
            size_t startK = rowIdx * 6;
            size_t endK = std::min(displayKnobs, startK + 6);
            for (size_t i = startK; i < endK; ++i) {
                const auto& k = data.knobs[i];
                GuiKnobStyle kStyle = GuiKnobStyle::Standard;
                if (eng == "tr808" || eng == "tr909") {
                    if (i == 0) kStyle = GuiKnobStyle::BakeliteSkirt;
                    else if (i == 1) kStyle = GuiKnobStyle::CreamFluted;
                    else if (i == 2) kStyle = GuiKnobStyle::AnodizedKnurled;
                    else if (i == 3) kStyle = GuiKnobStyle::TwoToneStepped;
                    else if (i == 4) kStyle = GuiKnobStyle::Tb303Halo;
                    else kStyle = GuiKnobStyle::Standard;
                } else if (eng == "dx7" || eng == "dxvii_epiano" || eng == "dxvii" || eng == "eats_dx") {
                    if (i == 0) kStyle = GuiKnobStyle::AnodizedKnurled;
                    else if (i == 1) kStyle = GuiKnobStyle::TwoToneStepped;
                    else if (i == 2) kStyle = GuiKnobStyle::BakeliteSkirt;
                    else if (i == 3) kStyle = GuiKnobStyle::CreamFluted;
                    else if (i == 4) kStyle = GuiKnobStyle::Standard;
                    else kStyle = GuiKnobStyle::Tb303Halo;
                } else {
                    kStyle = GuiKnobStyle::StudioClean;
                }

                GuiWidgetDef w;
                w.id = "w_" + k.name;
                w.type = GuiWidgetType::Knob;
                w.label = k.label;
                w.param = k.name;
                w.knobStyle = kStyle;
                w.size = (expectedRows > 1) ? 48.0f : 52.0f;
                w.currentVal = k.value;
                w.minVal = 0.0f;
                w.maxVal = 1.0f;
                w.unit = "";
                w.accentColor = guiPanel.accentColor;
                rDef.widgets.push_back(w);
            }
            guiPanel.rows.push_back(rDef);
        }
    } else {
        size_t kIdx = 0;
        for (size_t rIdx = 0; rIdx < guiPanel.rows.size(); ++rIdx) {
            auto& row = guiPanel.rows[rIdx];
            for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx, ++kIdx) {
                if (kIdx >= data.knobs.size()) break;
                if (draggingRow == static_cast<int>(rIdx) && draggingWidget == static_cast<int>(wIdx)) continue;
                row.widgets[wIdx].currentVal = (row.widgets[wIdx].maxVal > row.widgets[wIdx].minVal) ? (row.widgets[wIdx].minVal + std::clamp(data.knobs[kIdx].value, 0.0f, 1.0f) * (row.widgets[wIdx].maxVal - row.widgets[wIdx].minVal)) : data.knobs[kIdx].value;
                if (row.widgets[wIdx].label != data.knobs[kIdx].label) {
                    row.widgets[wIdx].label = data.knobs[kIdx].label;
                }
                if (row.widgets[wIdx].param != data.knobs[kIdx].name) {
                    row.widgets[wIdx].param = data.knobs[kIdx].name;
                }
                row.widgets[wIdx].accentColor = guiPanel.accentColor;
            }
        }
    }
}

void TrackPropertiesPanel::syncGuiPanelFromTrackData(const TrackPropertiesDrawerData& data) {
    if (data.knobs.empty()) {
        const_cast<TrackPropertiesDrawerData&>(data).syncKnobsIfEmpty();
    }

    const std::string& eng = data.instrumentEngine;
    auto containsCI = [](std::string_view text, std::string_view needle) noexcept -> bool {
        if (needle.empty()) return true;
        if (text.size() < needle.size()) return false;
        auto it = std::search(text.begin(), text.end(), needle.begin(), needle.end(),
            [](char ch1, char ch2) {
                return std::tolower(static_cast<unsigned char>(ch1)) == std::tolower(static_cast<unsigned char>(ch2));
            });
        return it != text.end();
    };
    bool isClav = (stringEqualsIgnoreCase(eng, "clavinet") ||
                   containsCI(data.instrument, "clav") || containsCI(data.presetTitle, "clav") ||
                   containsCI(data.instrument, "d6") || containsCI(data.presetTitle, "d6"));
    bool is303 = !isClav && (stringEqualsIgnoreCase(eng, "tb303") ||
                             containsCI(data.instrument, "303") || containsCI(data.presetTitle, "303"));
    bool isSfxr = !isClav && !is303 && (stringEqualsIgnoreCase(eng, "sfxr") ||
                                       stringEqualsIgnoreCase(eng, "eats_sfxr") ||
                                       containsCI(data.instrument, "sfxr") || containsCI(data.presetTitle, "sfxr"));
    bool isSnes = !isClav && !is303 && !isSfxr && (stringEqualsIgnoreCase(eng, "snes") ||
                                                  stringEqualsIgnoreCase(eng, "snes_synth") ||
                                                  stringEqualsIgnoreCase(eng, "snes_console_synth") ||
                                                  containsCI(data.instrument, "snes") || containsCI(data.presetTitle, "snes"));
    bool isAstral = !isClav && !is303 && !isSfxr && !isSnes && (
        stringEqualsIgnoreCase(eng, "ambient_pad") ||
        stringEqualsIgnoreCase(eng, "astral_pad") ||
        stringEqualsIgnoreCase(eng, "astral_shimmer_pad") ||
        containsCI(data.instrument, "astral") || containsCI(data.presetTitle, "astral") ||
        containsCI(data.instrument, "shimmer") || containsCI(data.presetTitle, "shimmer")
    );
    bool isSteelGuitar = !isClav && !is303 && !isSfxr && !isSnes && !isAstral && (
        stringEqualsIgnoreCase(eng, "acoustic_steel_guitar") ||
        stringEqualsIgnoreCase(eng, "steel_acoustic_guitar") ||
        containsCI(data.instrument, "steel") || containsCI(data.presetTitle, "steel")
    );
    bool isCanonical = isClav || is303 || isSfxr || isSnes || isAstral || isSteelGuitar;

    if (isCanonical) {
        populateCanonicalInstrumentGuiPanel(guiPanel_, data,
            (dragMode_ == DragMode::InstrumentKnob) ? draggingRow_ : -1,
            (dragMode_ == DragMode::InstrumentKnob) ? draggingWidget_ : -1);
        return;
    }

    if (data.customGuiPanel.has_value()) {
        guiPanel_ = *data.customGuiPanel;
        for (auto& row : guiPanel_.rows) {
            for (auto& w : row.widgets) {
                for (const auto& k : data.knobs) {
                    if (stringEqualsIgnoreCase(w.id, k.name) || stringEqualsIgnoreCase(w.label, k.name) ||
                        stringEqualsIgnoreCase(w.label, k.label) || stringEqualsIgnoreCase(w.param, k.name)) {
                        w.currentVal = (w.maxVal > w.minVal) ? (w.minVal + std::clamp(k.value, 0.0f, 1.0f) * (w.maxVal - w.minVal)) : k.value;
                        if (w.type == GuiWidgetType::ListBox && !w.options.empty()) {
                            int idx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, static_cast<int>(w.options.size() - 1));
                            w.valueDisplay = w.options[idx];
                        } else if (!k.display.empty()) {
                            w.valueDisplay = k.display;
                        }
                        break;
                    }
                }
            }
        }
        return;
    }
    if (!data.eatscriptCode.empty()) {
        auto parsed = eatsbits::project::PresetLoader::parseFromEatscript(data.eatscriptCode);
        if (!parsed.params.empty() || !parsed.guiRoot.children.empty() || !parsed.guiRoot.title.empty()) {
            guiPanel_ = createGuiPanelFromPresetDefinition(parsed);
            for (auto& row : guiPanel_.rows) {
                for (auto& w : row.widgets) {
                    for (const auto& k : data.knobs) {
                        if (stringEqualsIgnoreCase(w.id, k.name) || stringEqualsIgnoreCase(w.label, k.name) ||
                            stringEqualsIgnoreCase(w.label, k.label) || stringEqualsIgnoreCase(w.param, k.name)) {
                            w.currentVal = (w.maxVal > w.minVal) ? (w.minVal + std::clamp(k.value, 0.0f, 1.0f) * (w.maxVal - w.minVal)) : k.value;
                            if (w.type == GuiWidgetType::ListBox && !w.options.empty()) {
                                int idx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, static_cast<int>(w.options.size() - 1));
                                w.valueDisplay = w.options[idx];
                            } else if (!k.display.empty()) {
                                w.valueDisplay = k.display;
                            }
                            break;
                        }
                    }
                }
            }
            return;
        }
    }
    if (auto builtin = eatsbits::project::BuiltinPresetRegistry::createPresetById(data.instrumentEngine)) {
        guiPanel_ = createGuiPanelFromPresetDefinition(*builtin);
        for (auto& row : guiPanel_.rows) {
            for (auto& w : row.widgets) {
                for (const auto& k : data.knobs) {
                    if (stringEqualsIgnoreCase(w.id, k.name) || stringEqualsIgnoreCase(w.label, k.name) ||
                        stringEqualsIgnoreCase(w.label, k.label) || stringEqualsIgnoreCase(w.param, k.name)) {
                        w.currentVal = (w.maxVal > w.minVal) ? (w.minVal + std::clamp(k.value, 0.0f, 1.0f) * (w.maxVal - w.minVal)) : k.value;
                        if (w.type == GuiWidgetType::ListBox && !w.options.empty()) {
                            int idx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, static_cast<int>(w.options.size() - 1));
                            w.valueDisplay = w.options[idx];
                        } else if (!k.display.empty()) {
                            w.valueDisplay = k.display;
                        }
                        break;
                    }
                }
            }
        }
        return;
    }
    populateCanonicalInstrumentGuiPanel(guiPanel_, data,
        (dragMode_ == DragMode::InstrumentKnob) ? draggingRow_ : -1,
        (dragMode_ == DragMode::InstrumentKnob) ? draggingWidget_ : -1);
}

void TrackPropertiesPanel::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;

    float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
    bool isWide = (bounds_.w >= 560.0f);

    if (!clayContext_) {
        clayContext_ = std::make_unique<clay::ClayContext>();
        clayContext_->initialize(bounds_.w, 2500.0f);
    } else {
        clayContext_->setDimensions(bounds_.w, 2500.0f);
    }

    clayContext_->beginLayout();

    CLAY(CLAY_ID("TrackPropertiesRoot"), {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(bounds_.w), CLAY_SIZING_FIT(0) },
            .padding = { static_cast<uint16_t>(padding), static_cast<uint16_t>(padding), 4, 12 },
            .childGap = 8,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        }
    }) {
        if (showTrackRibbon_) {
            CLAY(CLAY_ID("RibbonCard"), {
                .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34.0f) } }
            }) {}
        }

        float headerH = isWide ? 42.0f : 38.0f;
        CLAY(CLAY_ID("HeaderCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(headerH) } }
        }) {}

        float colorH = 36.0f;
        CLAY(CLAY_ID("ColorCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(colorH) } }
        }) {}

        float eqH = 138.0f;
        CLAY(CLAY_ID("EqCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(eqH) } }
        }) {}

        float faceplateH = isWide ? 280.0f : 240.0f;
        CLAY(CLAY_ID("FaceplateCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(faceplateH) } }
        }) {}

        float chordFollowH = 88.0f;
        CLAY(CLAY_ID("ChordFollowCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(chordFollowH) } }
        }) {}

        float midiFxH = isWide ? 120.0f : 106.0f;
        CLAY(CLAY_ID("MidiFxCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(midiFxH) } }
        }) {}

        float audioFxH = isWide ? 120.0f : 106.0f;
        CLAY(CLAY_ID("AudioFxCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(audioFxH) } }
        }) {}

        float actionsH = 68.0f;
        CLAY(CLAY_ID("TrackActionsCard"), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(actionsH) } }
        }) {}
    }

    clayContext_->endLayout(0.016f);

    auto toRect = [this](const Clay_ElementData& data) -> Rect2D {
        if (!data.found) return Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
        return Rect2D(bounds_.x + data.boundingBox.x,
                      bounds_.y + data.boundingBox.y - scrollY_,
                      data.boundingBox.width,
                      data.boundingBox.height);
    };

    if (showTrackRibbon_) {
        ribbonBounds_ = toRect(clayContext_->getElementData(CLAY_ID("RibbonCard")));
    } else {
        ribbonBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
    }
    headerCardBounds_ = toRect(clayContext_->getElementData(CLAY_ID("HeaderCard")));
    colorCardBounds_ = toRect(clayContext_->getElementData(CLAY_ID("ColorCard")));
    mixerCardBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
    eqCardBounds_ = toRect(clayContext_->getElementData(CLAY_ID("EqCard")));
    faceplateBounds_ = toRect(clayContext_->getElementData(CLAY_ID("FaceplateCard")));
    chordFollowBounds_ = toRect(clayContext_->getElementData(CLAY_ID("ChordFollowCard")));
    midiFxBounds_ = toRect(clayContext_->getElementData(CLAY_ID("MidiFxCard")));
    audioFxBounds_ = toRect(clayContext_->getElementData(CLAY_ID("AudioFxCard")));
    trackActionsBounds_ = toRect(clayContext_->getElementData(CLAY_ID("TrackActionsCard")));
    colorPaletteBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);

    auto rootData = clayContext_->getElementData(CLAY_ID("TrackPropertiesRoot"));
    if (rootData.found) {
        totalContentHeight_ = rootData.boundingBox.height;
    }
    needScrollbar_ = (totalContentHeight_ > bounds_.h);
    scrollbarBounds_ = Rect2D(bounds_.x + bounds_.w - 7.0f, bounds_.y + 2.0f, 5.0f, bounds_.h - 4.0f);

    // Layout embedded dialogs only when open or uninitialized
    if (pluginDialog_.isOpen() || pluginDialog_.getBounds().w <= 0.0f) {
        pluginDialog_.layout(ctx.logicalWidth > 0.0f ? ctx.logicalWidth : 1280.0f,
                             ctx.logicalHeight > 0.0f ? ctx.logicalHeight : 800.0f);
    }
    if (colorPickerDialog_.isOpen() || colorPickerDialog_.getBounds().w <= 0.0f) {
        colorPickerDialog_.layout(ctx.logicalWidth > 0.0f ? ctx.logicalWidth : 1280.0f,
                                  ctx.logicalHeight > 0.0f ? ctx.logicalHeight : 800.0f);
    }
}

void TrackPropertiesPanel::update(float dt) noexcept {
    if (scroller_.isGliding()) {
        float dx = 0.0f, dy = 0.0f;
        scroller_.step(dt, dx, dy);
        float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
        scrollY_ = std::clamp(scrollY_ - dy, 0.0f, maxScroll);
        if (onScrollChanged) onScrollChanged(scrollY_);
    }
}

void TrackPropertiesPanel::render(BatchRenderer2D& r, const ThemeTokens& theme, TrackPropertiesDrawerData& data,
                                  float mouseX, float mouseY) {
    if (scroller_.isGliding()) {
        update(1.0f / 60.0f);
    }
    if (isLongPressActive_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - touchDownTimePoint_).count();
        if (elapsedMs >= 450) {
            isLongPressActive_ = false;
            dragMode_ = DragMode::None;
            activeKnobIndex_ = -1;
            draggingRow_ = -1;
            draggingWidget_ = -1;
            openValueEditForHit(longPressHit_, data, longPressOpenValueEdit_);
        }
    }
    data.syncKnobsIfEmpty();

    float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
    float contentX = bounds_.x + padding;
    float contentW = bounds_.w - (padding * 2.0f);
    bool isWide = (bounds_.w >= 560.0f);

    {
        // Strictly scissor panel content to bounds so it never renders past panels
        BatchRenderer2D::ScissorScope panelScissor(r, bounds_);

        // Master Console Mode (Mixer Master selected)
        if (data.isMasterSelected || data.tab == TrackPropertiesTab::Master) {
            renderMasterSection(r, theme, data, contentX, bounds_.y + 8.0f - scrollY_, contentW, mouseX, mouseY);
            if (needScrollbar_) renderScrollbar(r, theme);
            return;
        }

        // Clip Mode (Arranger Clip selected)
        if (data.tab == TrackPropertiesTab::Clip) {
            renderClipSection(r, theme, data, contentX, bounds_.y + 8.0f - scrollY_, contentW, mouseX, mouseY);
            if (needScrollbar_) renderScrollbar(r, theme);
            return;
        }

        float curCardY = bounds_.y + 8.0f - scrollY_;
        if (showTrackRibbon_) {
            renderTrackSelectorRibbon(r, theme, data);
            curCardY += ribbonBounds_.h + 6.0f;
        }

        float headH = isWide ? 40.0f : 36.0f;
        headerCardBounds_ = Rect2D(contentX, curCardY, contentW, headH);
        renderHeaderCard(r, theme, data, headerCardBounds_.x, headerCardBounds_.y, headerCardBounds_.w, isWide, mouseX, mouseY);
        curCardY += headH + 8.0f;

        colorCardBounds_ = Rect2D(contentX, curCardY, contentW, 36.0f);
        renderColorPalette(r, theme, data, colorCardBounds_.x, colorCardBounds_.y, colorCardBounds_.w);
        curCardY += 36.0f + 8.0f;

        if (data.isMixerMode) {
            eqCardBounds_ = Rect2D(contentX, curCardY, contentW, 138.0f);
            renderEqCard(r, theme, data, eqCardBounds_.x, eqCardBounds_.y, eqCardBounds_.w);
            curCardY += 138.0f + 8.0f;
        } else {
            eqCardBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
        }

        float faceplateH = data.instrumentExpanded ? (isWide ? 280.0f : 240.0f) : 38.0f;
        faceplateBounds_ = Rect2D(contentX, curCardY, contentW, faceplateH);
        renderFaceplateCard(r, theme, data, faceplateBounds_.x, faceplateBounds_.y, faceplateBounds_.w, isWide);
        curCardY += faceplateH + 8.0f;

        chordFollowBounds_ = Rect2D(contentX, curCardY, contentW, 88.0f);
        renderChordFollowCard(r, theme, data, chordFollowBounds_.x, chordFollowBounds_.y, chordFollowBounds_.w);
        curCardY += 88.0f + 8.0f;

        float mfxH = computeMidiFxRackHeight(data);
        midiFxBounds_ = Rect2D(contentX, curCardY, contentW, mfxH);
        renderMidiFxCard(r, theme, data, midiFxBounds_.x, midiFxBounds_.y, midiFxBounds_.w, isWide);
        curCardY += mfxH + 8.0f;

        float afxH = computeAudioFxRackHeight(data);
        audioFxBounds_ = Rect2D(contentX, curCardY, contentW, afxH);
        renderAudioFxCard(r, theme, data, audioFxBounds_.x, audioFxBounds_.y, audioFxBounds_.w, isWide);
        curCardY += afxH + 8.0f;

        float actionsH = 68.0f;
        trackActionsBounds_ = Rect2D(contentX, curCardY, contentW, actionsH);
        renderTrackActionsCard(r, theme, data, trackActionsBounds_.x, trackActionsBounds_.y, trackActionsBounds_.w, isWide, mouseX, mouseY);
        curCardY += actionsH + 12.0f;

        totalContentHeight_ = (curCardY + scrollY_) - (bounds_.y + 8.0f);
        needScrollbar_ = (totalContentHeight_ > bounds_.h);

        if (needScrollbar_) {
            renderScrollbar(r, theme);
        }
    }

    if (!isInsideDrawer_ && mouseX >= 0.0f && mouseY >= 0.0f) {
        std::string tip = getTooltip(mouseX, mouseY, data);
        if (!tip.empty()) {
            drawTooltipBadge(r, tip, mouseX, mouseY - 14.0f, theme, false, bounds_.x + bounds_.w);
        }
    }
}

void TrackPropertiesPanel::renderTrackSelectorRibbon(BatchRenderer2D& r, const ThemeTokens& theme,
                                                     const TrackPropertiesDrawerData& data) {
    if (ribbonBounds_.y + ribbonBounds_.h < bounds_.y || ribbonBounds_.y > bounds_.y + bounds_.h) return;

    size_t trackCount = data.allTrackNames.empty() ? 5 : data.allTrackNames.size();
    const float startX = ribbonBounds_.x + 4.0f;
    const float tabW = std::clamp((ribbonBounds_.w - 40.0f) / static_cast<float>(trackCount), 90.0f, 130.0f);
    const float tabGap = 8.0f;
    const float tabH = ribbonBounds_.h - 4.0f;

    for (size_t i = 0; i < trackCount; ++i) {
        float tx = startX + static_cast<float>(i) * (tabW + tabGap);
        if (tx + tabW > ribbonBounds_.x + ribbonBounds_.w) break;

        bool isActive = (i == data.trackIndex);
        char fallbackName[32];
        std::string_view tName;
        if (i < data.allTrackNames.size()) {
            tName = data.allTrackNames[i];
        } else {
            std::snprintf(fallbackName, sizeof(fallbackName), "Track %zu", i + 1);
            tName = fallbackName;
        }
        Color tCol = (i < data.allTrackColors.size()) ? data.allTrackColors[i] : Color(data.r, data.g, data.b);

        drawRoundedRect(r, tx, ribbonBounds_.y + 2.0f, tabW, tabH, 4.0f,
                        isActive ? (theme.primaryAccent * 0.28f) : theme.controlBackground,
                        isActive ? 0.95f : 0.70f);
        drawRoundedRectOutline(r, tx, ribbonBounds_.y + 2.0f, tabW, tabH, 4.0f,
                               isActive ? theme.primaryAccent : theme.borderSubtle,
                               isActive ? 0.95f : 0.40f, isActive ? 1.5f : 1.0f);

        // Accent indicator dot
        drawCircle(r, tx + 10.0f, ribbonBounds_.y + 2.0f + tabH * 0.5f, 3.5f,
                   tCol.r, tCol.g, tCol.b, isActive ? 1.0f : 0.6f);

        drawText(r, tName, tx + 20.0f, ribbonBounds_.y + 11.0f, 10.5f,
                 isActive ? theme.primaryAccent.r : theme.textPrimary.r,
                 isActive ? theme.primaryAccent.g : theme.textPrimary.g,
                 isActive ? theme.primaryAccent.b : theme.textPrimary.b, 1.0f);
    }
}

void TrackPropertiesPanel::renderHeaderCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                            TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide,
                                            float mouseX, float mouseY) {
    float h = isWide ? 40.0f : 36.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Track icon button prior to title (clickable to open track icon dialog)
    float iconBtnX = cx + 8.0f;
    float iconBtnW = 24.0f;
    float iconBtnH = 24.0f;
    float iconBtnY = cy + (h - iconBtnH) * 0.5f;
    bool iconHov = (mouseX >= iconBtnX && mouseX <= iconBtnX + iconBtnW &&
                    mouseY >= iconBtnY && mouseY <= iconBtnY + iconBtnH);
    if (iconHov) {
        drawRoundedRect(r, iconBtnX, iconBtnY, iconBtnW, iconBtnH, 4.0f, 1.0f, 1.0f, 1.0f, 0.08f);
    }
    float iconSize = 16.0f;
    float iconX = iconBtnX + (iconBtnW - iconSize) * 0.5f;
    float iconY = iconBtnY + (iconBtnH - iconSize) * 0.5f;
    std::string_view iconRef = data.iconRef.empty() ? std::string_view("preset:inst_synth") : std::string_view(data.iconRef);
    IconRegistry::instance().renderIcon(r, iconRef, iconX, iconY, iconSize, Color(data.r, data.g, data.b, 1.0f));

    // Right-aligned Edit icon (pencil icon, same size as title text 10.0f, NO border around it)
    float editSize = 10.0f;
    float editBtnW = 20.0f;
    float editBtnH = 20.0f;
    float editBtnX = cx + cw - 12.0f - editBtnW;
    float editBtnY = cy + (h - editBtnH) * 0.5f;
    bool editHov = (mouseX >= editBtnX - 4.0f && mouseX <= editBtnX + editBtnW + 4.0f &&
                    mouseY >= cy && mouseY <= cy + h);
    Color editColor = editHov ? theme.primaryAccent : theme.textSecondary;
    drawIconEdit(r, editBtnX + (editBtnW - editSize) * 0.5f, editBtnY + (editBtnH - editSize) * 0.5f, editSize, editColor);

    // Mute and Solo removed from Track Properties sidebar header card
    headerMuteBounds_ = Rect2D{0.0f, 0.0f, 0.0f, 0.0f};
    headerSoloBounds_ = Rect2D{0.0f, 0.0f, 0.0f, 0.0f};

    // Track Title (vertically centered on single line, size 10.0f matching clip title text)
    float titleX = iconBtnX + iconBtnW + 6.0f;
    float titleY = cy + (h - 10.0f) * 0.5f;
    drawText(r, data.trackName, titleX, titleY, 10.0f,
             theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
}

void TrackPropertiesPanel::renderMixerControlsCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                   TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float h = isWide ? 68.0f : 60.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    float sY = cy + (isWide ? 25.0f : 21.0f);
    float sH = isWide ? 18.0f : 16.0f;

    // 1. Expanded Volume Slider (takes majority of width)
    float vLabelX = cx + 14.0f;
    float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
    float panAreaW = isWide ? 85.0f : 74.0f;
    float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
    if (vTrackW < 50.0f) vTrackW = 50.0f;

    drawText(r, "VOL", vLabelX, sY + 3.0f, 10.5f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    drawRoundedRect(r, vTrackX, sY, vTrackW, sH, 3.0f, 0.06f, 0.07f, 0.09f, 1.0f);
    drawRoundedRectOutline(r, vTrackX, sY, vTrackW, sH, 3.0f, 0.20f, 0.22f, 0.28f, 1.0f, 1.0f);

    float normVol = std::clamp(data.volume / 1.5f, 0.0f, 1.0f);
    if (normVol > 0.01f) {
        drawRoundedRect(r, vTrackX + 2.0f, sY + 2.0f, (vTrackW - 4.0f) * normVol, sH - 4.0f, 2.5f,
                        data.r * 0.85f, data.g * 0.85f, data.b * 0.85f, 0.95f);
    }
    float volThumbX = vTrackX + normVol * vTrackW;
    drawRoundedRect(r, volThumbX - 4.0f, sY - 2.0f, 8.0f, sH + 4.0f, 2.0f, 0.80f, 0.82f, 0.88f, 1.0f);
    int volPct = static_cast<int>(std::round(data.volume * 100.0f));
    char volBuf[16];
    std::snprintf(volBuf, sizeof(volBuf), "%d%%", volPct);
    drawText(r, std::string_view(volBuf), vTrackX + vTrackW + 8.0f, sY + 3.0f, 10.0f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // 2. Rotary Pan Knob (right side)
    float panKcx = cx + cw - (isWide ? 40.0f : 34.0f);
    float panKcy = cy + (h * 0.5f) - (isWide ? 2.0f : 1.0f);
    float kPanRadius = isWide ? 13.5f : 12.0f;
    bool isDraggingPan = (dragMode_ == DragMode::PanKnob);

    drawCircle(r, panKcx, panKcy + 2.0f, kPanRadius + 1.0f, 0.05f, 0.06f, 0.08f, 0.35f);
    drawCircle(r, panKcx, panKcy, kPanRadius, 0.16f, 0.18f, 0.22f, 1.0f);
    drawCircleOutline(r, panKcx, panKcy, kPanRadius,
                      isDraggingPan ? theme.highlight.r : theme.borderSubtle.r,
                      isDraggingPan ? theme.highlight.g : theme.borderSubtle.g,
                      isDraggingPan ? theme.highlight.b : theme.borderSubtle.b,
                      isDraggingPan ? 1.0f : 0.7f, 1.2f);

    // Center Detent Tick Mark at 12 o'clock
    drawLine(r, panKcx, panKcy - kPanRadius - 2.0f, panKcx, panKcy - kPanRadius + 2.0f,
             0.50f, 0.55f, 0.65f, 0.8f, 1.0f);

    // Rotary Needle
    constexpr float kMaxPanAngle = 2.35619449f;  // +135 deg
    float curPanAngle = data.pan * kMaxPanAngle; // 0.0 = straight UP (12 o'clock)
    float needleX = panKcx + std::sin(curPanAngle) * (kPanRadius - 3.0f);
    float needleY = panKcy - std::cos(curPanAngle) * (kPanRadius - 3.0f);
    drawLine(r, panKcx, panKcy, needleX, needleY,
             isDraggingPan ? theme.highlight.r : (std::abs(data.pan) < 0.04f ? 0.70f : data.r),
             isDraggingPan ? theme.highlight.g : (std::abs(data.pan) < 0.04f ? 0.75f : data.g),
             isDraggingPan ? theme.highlight.b : (std::abs(data.pan) < 0.04f ? 0.85f : data.b),
             1.0f, 2.0f);
    drawCircle(r, panKcx, panKcy, 3.0f, 0.25f, 0.28f, 0.32f, 1.0f);

    // PAN Label above knob
    drawCenteredText(r, "PAN", panKcx - 24.0f, panKcy - kPanRadius - 10.0f, 48.0f, 10.0f, 8.0f,
                     theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    // Readout below knob ("C", "L25", "R40")
    char panBuf[16];
    if (std::abs(data.pan) < 0.04f) {
        std::snprintf(panBuf, sizeof(panBuf), "C");
    } else if (data.pan < 0.0f) {
        std::snprintf(panBuf, sizeof(panBuf), "L%d", static_cast<int>(std::round(-data.pan * 100.0f)));
    } else {
        std::snprintf(panBuf, sizeof(panBuf), "R%d", static_cast<int>(std::round(data.pan * 100.0f)));
    }
    drawCenteredText(r, std::string_view(panBuf), panKcx - 24.0f, panKcy + kPanRadius + 3.0f, 48.0f, 10.0f, 8.0f,
                     isDraggingPan ? theme.highlight.r : theme.primaryAccent.r,
                     isDraggingPan ? theme.highlight.g : theme.primaryAccent.g,
                     isDraggingPan ? theme.highlight.b : theme.primaryAccent.b, 1.0f);
}

void TrackPropertiesPanel::renderEqCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                        TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float eqCardH = 138.0f;
    if (cy + eqCardH < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, eqCardH, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, eqCardH, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "CHANNEL EQ (3-BAND PARAMETRIC)", cx + 12.0f, cy + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float eqTglW = 60.0f;
    float eqTglX = cx + cw - eqTglW - 10.0f;
    Color eqBg = data.eqEnabled ? (theme.primaryAccent * 0.3f) : Color(0.12f, 0.12f, 0.14f, 0.9f);
    Color eqBorder = data.eqEnabled ? theme.primaryAccent : theme.borderSubtle;
    Color eqText = data.eqEnabled ? theme.primaryAccent : theme.textMuted;
    drawButton(r, Rect2D(eqTglX, cy + 6.0f, eqTglW, 20.0f),
               data.eqEnabled ? "ACTIVE" : "BYPASS", eqBg, eqBorder, eqText, 8.5f, 3.0f, 1.0f);

    float kw3 = cw / 3.0f;
    auto drawEqKnob = [&](float kx, float ky, std::string_view label, std::string_view valStr, float normVal) {
        drawCircle(r, kx, ky, 13.0f, 0.14f, 0.16f, 0.20f, 1.0f);
        drawCircleOutline(r, kx, ky, 13.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);
        float ang = -2.35f + normVal * 4.71f;
        drawLine(r, kx, ky, kx + std::cos(ang) * 10.0f, ky + std::sin(ang) * 10.0f,
                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f, 1.8f);
        drawText(r, label, kx - static_cast<float>(label.length()) * 2.7f, ky + 16.0f, 7.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        drawText(r, valStr, kx - static_cast<float>(valStr.length()) * 2.5f, ky + 26.0f, 7.5f,
                 theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
    };

    char eqBuf[32];
    float eqRow1Y = cy + 45.0f;
    float normHpf = std::clamp((data.eqHpf - 20.0f) / 480.0f, 0.0f, 1.0f);
    std::snprintf(eqBuf, sizeof(eqBuf), "%dHz", static_cast<int>(data.eqHpf));
    drawEqKnob(cx + kw3 * 0.5f, eqRow1Y, "HPF CUT", eqBuf, normHpf);

    float normLow = std::clamp((data.eqLowGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::snprintf(eqBuf, sizeof(eqBuf), "%s%ddB", data.eqLowGain >= 0.0f ? "+" : "", static_cast<int>(data.eqLowGain));
    drawEqKnob(cx + kw3 * 1.5f, eqRow1Y, "LOW GAIN", eqBuf, normLow);

    float normHigh = std::clamp((data.eqHighGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::snprintf(eqBuf, sizeof(eqBuf), "%s%ddB", data.eqHighGain >= 0.0f ? "+" : "", static_cast<int>(data.eqHighGain));
    drawEqKnob(cx + kw3 * 2.5f, eqRow1Y, "HIGH GAIN", eqBuf, normHigh);

    float eqRow2Y = cy + 98.0f;
    float normMidF = std::clamp((data.eqMidFreq - 200.0f) / 7800.0f, 0.0f, 1.0f);
    if (data.eqMidFreq >= 1000.0f) {
        std::snprintf(eqBuf, sizeof(eqBuf), "%dkHz", static_cast<int>(data.eqMidFreq / 1000.0f));
    } else {
        std::snprintf(eqBuf, sizeof(eqBuf), "%dHz", static_cast<int>(data.eqMidFreq));
    }
    drawEqKnob(cx + kw3 * 0.5f, eqRow2Y, "MID FREQ", eqBuf, normMidF);

    float normMidG = std::clamp((data.eqMidGain + 18.0f) / 36.0f, 0.0f, 1.0f);
    std::snprintf(eqBuf, sizeof(eqBuf), "%s%ddB", data.eqMidGain >= 0.0f ? "+" : "", static_cast<int>(data.eqMidGain));
    drawEqKnob(cx + kw3 * 1.5f, eqRow2Y, "MID GAIN", eqBuf, normMidG);

    float normMidQ = std::clamp((data.eqMidQ - 0.3f) / 9.7f, 0.0f, 1.0f);
    std::snprintf(eqBuf, sizeof(eqBuf), "Q=%d", static_cast<int>(data.eqMidQ));
    drawEqKnob(cx + kw3 * 2.5f, eqRow2Y, "MID Q", eqBuf, normMidQ);
}

void TrackPropertiesPanel::renderFaceplateCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    float faceH = data.instrumentExpanded ? (isWide ? 280.0f : 240.0f) : 38.0f;
    if (cy + faceH < bounds_.y || cy > bounds_.y + bounds_.h) return;

    // Outer card container background (matching Eatsbeats original instrument layout)
    drawRoundedRect(r, cx, cy, cw, faceH, 8.0f, 0.11f, 0.12f, 0.14f, 0.98f);
    drawRoundedRectOutline(r, cx, cy, cw, faceH, 8.0f, 0.24f, 0.26f, 0.32f, 0.7f, 1.0f);

    // 1. Top Instrument Header
    float dotX = cx + 16.0f;
    float dotY = cy + 18.0f;
    float dotR = 5.0f;
    drawCircle(r, dotX, dotY, dotR + 3.0f, data.r, data.g, data.b, 0.30f);
    drawCircle(r, dotX, dotY, dotR, data.r, data.g, data.b, 1.0f);

    float fullBtnW = 46.0f;
    float fullBtnH = 22.0f;
    float fullBtnX = cx + cw - 10.0f - fullBtnW;
    float fullBtnY = cy + 7.0f;

    float detachBtnW = 28.0f;
    float detachBtnH = 22.0f;
    float detachBtnX = fullBtnX - 5.0f - detachBtnW;
    float detachBtnY = cy + 7.0f;

    float presetBtnW = 68.0f;
    float presetBtnH = 22.0f;
    float presetBtnX = detachBtnX - 5.0f - presetBtnW;
    float presetBtnY = cy + 7.0f;

    float designBtnW = 24.0f;
    float designBtnH = 22.0f;
    float designBtnX = presetBtnX - 5.0f - designBtnW;
    float designBtnY = cy + 7.0f;

    // Design icon button (links to Design section)
    drawRoundedRect(r, designBtnX, designBtnY, designBtnW, designBtnH, 4.0f, 0.13f, 0.14f, 0.18f, 0.95f);
    drawRoundedRectOutline(r, designBtnX, designBtnY, designBtnW, designBtnH, 4.0f, 0.32f, 0.36f, 0.45f, 0.85f, 1.0f);
    drawDesignChipIcon(r, designBtnX + 4.0f, designBtnY + 3.0f, 16.0f, Color(0.95f, 0.65f, 0.15f, 1.0f));

    // Preset button (opens Preset dialog)
    drawRoundedRect(r, presetBtnX, presetBtnY, presetBtnW, presetBtnH, 4.0f, 0.13f, 0.14f, 0.18f, 0.95f);
    drawRoundedRectOutline(r, presetBtnX, presetBtnY, presetBtnW, presetBtnH, 4.0f, 0.32f, 0.36f, 0.45f, 0.85f, 1.0f);
    drawSlidersTuneIcon(r, presetBtnX + 5.0f, presetBtnY + (presetBtnH - 12.0f) * 0.5f, 12.0f, theme.textSecondary);
    drawTextLeftAlignedY(r, "PRESETS", presetBtnX + 19.0f, presetBtnY + presetBtnH * 0.5f, 8.5f, theme.textSecondary);

    // Detached / Floating Window button (spawns or focuses native OS floating window)
    bool isDetached = data.isDetached || FloatingWindowManager::instance().isDetached(data.trackIndex);
    Color detachCol = isDetached ? Color(0.95f, 0.70f, 0.20f, 1.0f) : Color(0.20f, 0.85f, 0.95f, 1.0f);
    drawRoundedRect(r, detachBtnX, detachBtnY, detachBtnW, detachBtnH, 4.0f, isDetached ? 0.22f : 0.12f, isDetached ? 0.18f : 0.15f, isDetached ? 0.12f : 0.20f, 0.95f);
    drawRoundedRectOutline(r, detachBtnX, detachBtnY, detachBtnW, detachBtnH, 4.0f, detachCol.r, detachCol.g, detachCol.b, 0.85f, 1.2f);
    drawRoundedRectOutline(r, detachBtnX + 7.0f, detachBtnY + 8.0f, 10.0f, 8.0f, 1.0f, detachCol.r, detachCol.g, detachCol.b, 0.9f, 1.2f);
    drawRect(r, detachBtnX + 12.0f, detachBtnY + 5.0f, 6.0f, 6.0f, 0.12f, 0.15f, 0.20f, 1.0f);
    drawRoundedRectOutline(r, detachBtnX + 12.0f, detachBtnY + 5.0f, 6.0f, 6.0f, 1.0f, detachCol.r, detachCol.g, detachCol.b, 0.9f, 1.2f);

    // Fullscreen icon button (triggers Fullscreen Device modal)
    Color fullGold(0.95f, 0.62f, 0.10f, 1.0f);
    drawRoundedRect(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 4.0f, 0.14f, 0.12f, 0.10f, 0.95f);
    drawRoundedRectOutline(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 4.0f, fullGold.r, fullGold.g, fullGold.b, 0.85f, 1.2f);
    drawFullscreenIcon(r, fullBtnX + 5.0f, fullBtnY + (fullBtnH - 10.0f) * 0.5f, 10.0f, fullGold, 1.4f);
    drawTextLeftAlignedY(r, "FULL", fullBtnX + 18.0f, fullBtnY + fullBtnH * 0.5f, 8.5f, fullGold);

    // Instrument Title in uppercase bold with dropdown chevron
    char instTitleBuf[64];
    std::string_view rawTitle = data.presetTitle.empty() ? std::string_view(data.instrument) : std::string_view(data.presetTitle);
    size_t copyLen = std::min(rawTitle.size(), sizeof(instTitleBuf) - 1);
    for (size_t i = 0; i < copyLen; ++i) {
        instTitleBuf[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(rawTitle[i])));
    }
    instTitleBuf[copyLen] = '\0';
    float availTitleW = designBtnX - (dotX + 13.0f) - 22.0f;
    int maxChars = std::max(6, static_cast<int>(availTitleW / 7.2f));
    if (static_cast<int>(copyLen) > maxChars && maxChars >= 3 && static_cast<size_t>(maxChars) < sizeof(instTitleBuf)) {
        instTitleBuf[maxChars - 3] = '.';
        instTitleBuf[maxChars - 2] = '.';
        instTitleBuf[maxChars - 1] = '.';
        instTitleBuf[maxChars] = '\0';
        copyLen = maxChars;
    }
    std::string_view instTitle(instTitleBuf, copyLen);
    float titleX = dotX + 13.0f;
    drawText(r, instTitle, titleX, cy + 12.0f, 11.5f, 0.96f, 0.97f, 0.99f, 1.0f);

    float chevX = titleX + static_cast<float>(copyLen) * 6.5f + 8.0f;
    float chevY = cy + 18.0f;
    Color trackAccent(data.r, data.g, data.b, 1.0f);
    if (data.instrumentExpanded) {
        drawChevronUp(r, chevX, chevY, 7.0f, trackAccent, 1.6f);
    } else {
        drawChevronDown(r, chevX, chevY, 7.0f, trackAccent, 1.6f);
    }

    if (data.instrumentExpanded) {
        float guiX = cx + 8.0f;
        float guiY = cy + 34.0f;
        float guiW = cw - 16.0f;
        float guiH = faceH - 34.0f - 38.0f;

        if (isDetached) {
            // High-aesthetic DAW Proxy Card for Detached Floating Window
            Rect2D proxyRect(guiX, guiY, guiW, guiH);
            drawRoundedRect(r, proxyRect.x, proxyRect.y, proxyRect.w, proxyRect.h, 6.0f, 0.09f, 0.11f, 0.15f, 0.98f);
            drawRoundedRectOutline(r, proxyRect.x, proxyRect.y, proxyRect.w, proxyRect.h, 6.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.50f, 1.2f);

            // Glowing cyan status indicator dot
            float stDotX = proxyRect.x + 18.0f;
            float stDotY = proxyRect.y + 24.0f;
            drawCircle(r, stDotX, stDotY, 7.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.25f);
            drawCircle(r, stDotX, stDotY, 4.0f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

            // Uppercase Badge: POP-OUT WINDOW ACTIVE
            drawText(r, "POP-OUT WINDOW ACTIVE", stDotX + 12.0f, stDotY - 6.0f, 10.5f,
                     theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

            // Subtitle explanation
            std::string subTxt = "Device controls are detached in an external desktop window.";
            drawText(r, subTxt, proxyRect.x + 18.0f, stDotY + 16.0f, 9.0f,
                     theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 0.85f);

            // Action Buttons: [ ⛶ FOCUS WINDOW ] and [ ⤓ RE-ATTACH / DOCK ]
            float btnH = 30.0f;
            float btnY = proxyRect.y + proxyRect.h - btnH - 18.0f;
            float btnSpacing = 10.0f;
            float totalBtnW = proxyRect.w - 36.0f;
            float btnW = std::clamp((totalBtnW - btnSpacing) * 0.5f, 100.0f, 160.0f);

            float b1X = proxyRect.x + 18.0f;
            float b2X = b1X + btnW + btnSpacing;
            focusDetachedBtnBounds_ = Rect2D(b1X, btnY, btnW, btnH);
            reattachDetachedBtnBounds_ = Rect2D(b2X, btnY, btnW, btnH);

            // Button 1: FOCUS WINDOW
            drawRoundedRect(r, b1X, btnY, btnW, btnH, 4.0f, 0.12f, 0.18f, 0.26f, 0.95f);
            drawRoundedRectOutline(r, b1X, btnY, btnW, btnH, 4.0f,
                                   theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.80f, 1.2f);
            drawFullscreenIcon(r, b1X + 8.0f, btnY + (btnH - 12.0f) * 0.5f, 12.0f, theme.primaryAccent, 1.4f);
            drawTextLeftAlignedY(r, "FOCUS WINDOW", b1X + 24.0f, btnY + btnH * 0.5f, 9.0f, theme.primaryAccent);

            // Button 2: RE-ATTACH / DOCK
            Color dockCol(0.85f, 0.88f, 0.94f, 1.0f);
            drawRoundedRect(r, b2X, btnY, btnW, btnH, 4.0f, 0.15f, 0.16f, 0.20f, 0.95f);
            drawRoundedRectOutline(r, b2X, btnY, btnW, btnH, 4.0f, 0.35f, 0.38f, 0.48f, 0.75f, 1.0f);
            drawChevronDown(r, b2X + 12.0f, btnY + btnH * 0.5f, 8.0f, dockCol, 1.8f);
            drawTextLeftAlignedY(r, "RE-ATTACH / DOCK", b2X + 24.0f, btnY + btnH * 0.5f, 8.5f, dockCol);
        } else {
            focusDetachedBtnBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);
            reattachDetachedBtnBounds_ = Rect2D(0.0f, 0.0f, 0.0f, 0.0f);

            syncGuiPanelFromTrackData(data);
            Rect2D fpRect(guiX, guiY, guiW, guiH);
            drawGuiFaceplate(r, guiPanel_, fpRect, theme, data.scopeBuffer, data.scopeBufferCount, draggingRow_, draggingWidget_, focusedWidgetId_);
        }

        // 3. Below the GUI, right-aligned 'Change Instrument' option
        float chgBtnW = 145.0f;
        float chgBtnH = 24.0f;
        float chgBtnX = cx + cw - 8.0f - chgBtnW;
        float chgBtnY = guiY + guiH + 7.0f;
        drawRoundedRect(r, chgBtnX, chgBtnY, chgBtnW, chgBtnH, 4.0f, 0.12f, 0.14f, 0.18f, 0.95f);
        drawRoundedRectOutline(r, chgBtnX, chgBtnY, chgBtnW, chgBtnH, 4.0f, 0.28f, 0.32f, 0.40f, 0.85f, 1.0f);
        drawText(r, "⇆  CHANGE INSTRUMENT", chgBtnX + 11.0f, chgBtnY + 7.0f, 8.5f, theme.textSecondary);

        if (data.hasNativeGui) {
            float openGuiBtnW = 150.0f;
            float openGuiBtnH = 24.0f;
            float openGuiBtnX = chgBtnX - 8.0f - openGuiBtnW;
            float openGuiBtnY = chgBtnY;
            Color cyanGui(0.15f, 0.88f, 0.95f, 1.0f);
            drawRoundedRect(r, openGuiBtnX, openGuiBtnY, openGuiBtnW, openGuiBtnH, 4.0f, 0.10f, 0.16f, 0.22f, 0.95f);
            drawRoundedRectOutline(r, openGuiBtnX, openGuiBtnY, openGuiBtnW, openGuiBtnH, 4.0f, cyanGui.r, cyanGui.g, cyanGui.b, 0.85f, 1.2f);
            drawText(r, "🗔  OPEN PLUGIN GUI", openGuiBtnX + 12.0f, openGuiBtnY + 7.0f, 8.5f, cyanGui);
        }
    }
}

void TrackPropertiesPanel::renderChordFollowCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                 TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float h = 88.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "HARMONIC CHORD TRACK FOLLOW", cx + 12.0f, cy + 10.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    // 5 Mode Chips
    const char* chipLabels[5] = {"OFF", "CHORD", "BASS", "SCALE", "COLOR"};
    ChordFollowMode chipModes[5] = {ChordFollowMode::Off, ChordFollowMode::Chord, ChordFollowMode::Bass, ChordFollowMode::Scale, ChordFollowMode::ColorLead};
    float chipW = (cw - 170.0f) / 5.0f;
    float chipH = 24.0f;
    float chipY = cy + 32.0f;

    for (int i = 0; i < 5; ++i) {
        float chipX = cx + 12.0f + static_cast<float>(i) * (chipW + 5.0f);
        bool isSel = (data.chordFollowMode == chipModes[i]);
        drawRoundedRect(r, chipX, chipY, chipW, chipH, 3.5f,
                        isSel ? (theme.primaryAccent * 0.30f) : Color(0.12f, 0.14f, 0.18f, 0.9f),
                        isSel ? 0.95f : 0.65f);
        drawRoundedRectOutline(r, chipX, chipY, chipW, chipH, 3.5f,
                               isSel ? theme.primaryAccent : theme.borderSubtle,
                               isSel ? 0.95f : 0.40f, isSel ? 1.4f : 1.0f);
        drawCenteredText(r, chipLabels[i], chipX, chipY + 2.0f, chipW, chipH, 8.5f,
                         isSel ? theme.primaryAccent.r : theme.textMuted.r,
                         isSel ? theme.primaryAccent.g : theme.textMuted.g,
                         isSel ? theme.primaryAccent.b : theme.textMuted.b, 1.0f);
    }

    // Bake to MIDI action button
    float bakeW = 100.0f;
    float bakeX = cx + cw - bakeW - 12.0f;
    drawButton(r, Rect2D(bakeX, chipY, bakeW, chipH), "[ BAKE TO MIDI ]",
               Color(0.16f, 0.20f, 0.28f, 1.0f), theme.borderSubtle, theme.secondaryAccent, 8.5f, 3.5f, 1.0f);

    // Explanatory dynamic caption
    const char* desc = "Track notes play freely without chord progression transposition.";
    if (data.chordFollowMode == ChordFollowMode::Bass) {
        desc = "Follows root/bass harmonic voice leading with octave constraint.";
    } else if (data.chordFollowMode == ChordFollowMode::Chord) {
        desc = "Conforms notes to polyphonic triad/7th voicing across bars.";
    } else if (data.chordFollowMode == ChordFollowMode::Scale) {
        desc = "Constrains melodies dynamically to chord scale tones.";
    } else if (data.chordFollowMode == ChordFollowMode::ColorLead) {
        desc = "Voice leads tension color tones (9th, 11th, 13th) to resolution.";
    }
    drawText(r, desc, cx + 12.0f, cy + 64.0f, 8.5f, theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
}

void TrackPropertiesPanel::renderMidiFxCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                            TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    (void)isWide;
    float h = computeMidiFxRackHeight(data);
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    // Rack outer container
    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    Color accent = theme.primaryAccent;

    // Signal/MIDI icon glyph (3 vertical rounded bars)
    float iconX = cx + 12.0f;
    float iconY = cy + 12.0f;
    drawLine(r, iconX, iconY + 5.0f, iconX, iconY + 13.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 4.0f, iconY + 2.0f, iconX + 4.0f, iconY + 16.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 8.0f, iconY + 6.0f, iconX + 8.0f, iconY + 12.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);

    // Header Title
    char midiRackBuf[32];
    std::snprintf(midiRackBuf, sizeof(midiRackBuf), "MIDI FX RACK (%zu)", data.midiFx.size());
    drawText(r, std::string_view(midiRackBuf), cx + 28.0f, cy + 11.0f, 10.5f,
             accent.r, accent.g, accent.b, 1.0f);

    // + ADD MIDI FX Button
    float addBtnW = 95.0f;
    float addBtnH = 22.0f;
    float addBtnX = cx + cw - addBtnW - 10.0f;
    drawRoundedRect(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, 0.12f, 0.15f, 0.20f, 0.95f);
    drawRoundedRectOutline(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, accent.r, accent.g, accent.b, 0.85f, 1.0f);
    drawCenteredText(r, "+ ADD MIDI FX", addBtnX, cy + 8.0f, addBtnW, addBtnH, 8.5f, accent.r, accent.g, accent.b, 1.0f);

    if (data.midiFx.empty()) {
        float phY = cy + 38.0f;
        drawRoundedRect(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f, 0.08f, 0.09f, 0.12f, 0.65f);
        drawRoundedRectOutline(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.40f, 1.0f);
        drawText(r, "No MIDI FX loaded. Click + ADD MIDI FX to insert.", cx + 22.0f, phY + 10.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.80f);
        return;
    }

    float itemY = cy + 38.0f;
    float itemW = cw - 16.0f;
    float itemX = cx + 8.0f;

    for (size_t mi = 0; mi < data.midiFx.size(); ++mi) {
        auto& fx = data.midiFx[mi];
        fx.ensureDefaultKnobs();

        float itemH = fx.isExpanded ? 180.0f : 42.0f;
        bool isFirst = (mi == 0);
        bool isLast = (mi == data.midiFx.size() - 1);

        // Card Container
        drawRoundedRect(r, itemX, itemY, itemW, itemH, 6.0f, 0.13f, 0.14f, 0.17f, 0.98f);
        drawRoundedRectOutline(r, itemX, itemY, itemW, itemH, 6.0f,
                               fx.enabled ? accent.r : 0.20f,
                               fx.enabled ? accent.g : 0.22f,
                               fx.enabled ? accent.b : 0.28f,
                               fx.enabled ? 0.75f : 0.40f, 1.0f);

        // 1. Vertical modern pill switch
        float swX = itemX + 10.0f;
        float swY = itemY + 8.0f;
        drawModernPillSwitch(r, swX, swY, fx.enabled, accent);

        // 2. Uppercase title
        char upperNameBuf[64];
        size_t cLen = std::min(fx.name.size(), sizeof(upperNameBuf) - 1);
        for (size_t ci = 0; ci < cLen; ++ci) {
            upperNameBuf[ci] = static_cast<char>(std::toupper(static_cast<unsigned char>(fx.name[ci])));
        }
        upperNameBuf[cLen] = '\0';
        std::string_view upperName(upperNameBuf, cLen);
        float titleX = itemX + 32.0f;
        float titleY = itemY + 13.0f;
        drawText(r, upperName, titleX, titleY, 11.0f,
                 fx.enabled ? 0.96f : 0.55f,
                 fx.enabled ? 0.97f : 0.58f,
                 fx.enabled ? 0.99f : 0.65f, 1.0f);

        // 3. Chevron next to title
        float textW = static_cast<float>(cLen) * 6.8f;
        float chevX = titleX + textW + 8.0f;
        float chevY = itemY + 20.0f;
        if (fx.isExpanded) {
            drawChevronUp(r, chevX, chevY, 7.0f, accent, 1.6f);
        } else {
            drawChevronDown(r, chevX, chevY, 7.0f, accent, 1.6f);
        }

        // 4. Action buttons on right: Fullscreen (⛶), Move Up (^), Move Down (v), Trash (delete)
        float fullX = itemX + itemW - 90.0f;
        float upX = itemX + itemW - 68.0f;
        float downX = itemX + itemW - 46.0f;
        float delX = itemX + itemW - 20.0f;
        float btnCenterY = itemY + 20.0f;

        Color arrowCol = Color(0.55f, 0.60f, 0.68f, 0.85f);
        Color dimmedArrow = Color(0.30f, 0.32f, 0.38f, 0.35f);

        drawFullscreenIcon(r, fullX - 5.5f, btnCenterY - 5.5f, 11.0f, accent, 1.4f);
        drawChevronUp(r, upX, btnCenterY, 8.5f, isFirst ? dimmedArrow : arrowCol, 1.6f);
        drawChevronDown(r, downX, btnCenterY, 8.5f, isLast ? dimmedArrow : arrowCol, 1.6f);
        drawTrashIcon(r, delX, btnCenterY, 13.0f, Color(0.55f, 0.60f, 0.68f, 0.85f), 1.3f);

        // 5. If expanded: Fullscreen button + Inset Faceplate
        if (fx.isExpanded) {
            // Fullscreen button
            float fullBtnW = 54.0f;
            float fullBtnH = 20.0f;
            float fullBtnX = itemX + itemW - fullBtnW - 10.0f;
            float fullBtnY = itemY + 36.0f;
            drawRoundedRect(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, 0.12f, 0.16f, 0.22f, 0.95f);
            drawRoundedRectOutline(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, accent.r, accent.g, accent.b, 0.65f, 1.0f);
            drawFullscreenIcon(r, fullBtnX + 6.0f, fullBtnY + (fullBtnH - 11.0f) * 0.5f, 11.0f, accent, 1.4f);
            drawTextLeftAlignedY(r, "FULL", fullBtnX + 21.0f, fullBtnY + fullBtnH * 0.5f, 8.5f, accent);

            // Inset Hardware Faceplate
            float fpX = itemX + 10.0f;
            float fpY = itemY + 60.0f;
            float fpW = itemW - 20.0f;
            float fpH = 108.0f;

            if (!fx.customGuiPanel.has_value() && !fx.eatscriptCode.empty()) {
                try {
                    auto parsed = eatsbits::project::PresetLoader::parseFromEatscript(fx.eatscriptCode);
                    if (!parsed.params.empty() || !parsed.guiRoot.children.empty() || !parsed.guiRoot.title.empty()) {
                        fx.customGuiPanel = createGuiPanelFromPresetDefinition(parsed);
                    }
                } catch (...) {}
            }

            if (fx.customGuiPanel.has_value()) {
                for (auto& row : fx.customGuiPanel->rows) {
                    for (auto& w : row.widgets) {
                        for (const auto& k : fx.knobs) {
                            if (stringEqualsIgnoreCase(w.id, k.name) || stringEqualsIgnoreCase(w.label, k.name) ||
                                stringEqualsIgnoreCase(w.label, k.label) || stringEqualsIgnoreCase(w.param, k.name)) {
                                w.currentVal = (w.maxVal > w.minVal) ? (w.minVal + std::clamp(k.value, 0.0f, 1.0f) * (w.maxVal - w.minVal)) : k.value;
                                if (!k.display.empty()) w.valueDisplay = k.display;
                                break;
                            }
                        }
                    }
                }
                fx.customGuiPanel->hideHeader = true;
                Rect2D fpRect(fpX, fpY, fpW, fpH);
                drawGuiFaceplate(r, *fx.customGuiPanel, fpRect, theme, nullptr, 0, -1, -1);
            } else {
                drawRoundedRect(r, fpX, fpY, fpW, fpH, 6.0f, 0.13f, 0.15f, 0.19f, 1.0f);
                drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, 6.0f, 0.08f, 0.09f, 0.12f, 0.90f, 1.2f);

                // Knobs inside faceplate
                size_t knobCount = std::min(fx.knobs.size(), size_t{4});
                if (knobCount > 0) {
                    float colW = fpW / static_cast<float>(knobCount);
                    for (size_t ki = 0; ki < knobCount; ++ki) {
                        const auto& knob = fx.knobs[ki];
                        float kcx = fpX + (static_cast<float>(ki) + 0.5f) * colW;
                        float kcy = fpY + 36.0f;
                        float kRad = 15.0f;

                        drawCircle(r, kcx, kcy + 2.0f, kRad + 1.0f, 0.05f, 0.06f, 0.08f, 0.45f);
                        drawCircleRadial3StopGradient(r, kcx, kcy, kRad,
                                                      Color(0.85f, 0.88f, 0.92f, 1.0f),
                                                      Color(0.40f, 0.44f, 0.50f, 1.0f),
                                                      Color(0.18f, 0.20f, 0.24f, 1.0f),
                                                      -0.25f * kRad, -0.30f * kRad, 0.50f, 24);
                        drawCircleOutline(r, kcx, kcy, kRad, 0.30f, 0.34f, 0.40f, 0.9f, 1.0f);

                        constexpr float minA = -2.35619449f;
                        constexpr float maxA = 2.35619449f;
                        float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);
                        drawLine(r, kcx, kcy, kcx + std::sin(curA) * (kRad - 2.0f), kcy - std::cos(curA) * (kRad - 2.0f),
                                 accent.r, accent.g, accent.b, 1.0f, 1.8f);

                        float lblY = fpY + 58.0f;
                        drawCenteredText(r, knob.label, kcx - colW * 0.5f, lblY, colW, 14.0f, 8.5f,
                                         0.70f, 0.75f, 0.85f, 1.0f);

                        float pillW = 42.0f;
                        float pillH = 15.0f;
                        float pillX = kcx - (pillW * 0.5f);
                        float pillY = fpY + 76.0f;
                        drawRoundedRect(r, pillX, pillY, pillW, pillH, 3.0f, 0.08f, 0.09f, 0.12f, 0.85f);
                        drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, 3.0f, 0.25f, 0.28f, 0.35f, 0.70f, 1.0f);
                        drawCenteredText(r, knob.display, pillX, pillY + 1.0f, pillW, pillH, 7.5f, 0.80f, 0.84f, 0.90f, 1.0f);
                    }
                }
            }
        }

        itemY += itemH + 8.0f;
    }
}

void TrackPropertiesPanel::renderAudioFxCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float cx, float cy, float cw, bool isWide) {
    (void)isWide;
    float h = computeAudioFxRackHeight(data);
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    // Rack outer container
    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Audio FX Blue Accent (matches Eatsbeats reference image #1A73E8 / Sky Blue)
    Color accent = Color(0.20f, 0.60f, 1.0f, 1.0f);

    // Signal/waveform icon glyph (3 vertical rounded bars)
    float iconX = cx + 12.0f;
    float iconY = cy + 12.0f;
    drawLine(r, iconX, iconY + 5.0f, iconX, iconY + 13.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 4.0f, iconY + 2.0f, iconX + 4.0f, iconY + 16.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);
    drawLine(r, iconX + 8.0f, iconY + 6.0f, iconX + 8.0f, iconY + 12.0f, accent.r, accent.g, accent.b, 1.0f, 2.0f);

    // Header Title
    char audioRackBuf[32];
    std::snprintf(audioRackBuf, sizeof(audioRackBuf), "AUDIO FX RACK (%zu)", data.audioFx.size());
    drawText(r, std::string_view(audioRackBuf), cx + 28.0f, cy + 11.0f, 10.5f,
             accent.r, accent.g, accent.b, 1.0f);

    // + ADD FX Button
    float addBtnW = 85.0f;
    float addBtnH = 22.0f;
    float addBtnX = cx + cw - addBtnW - 10.0f;
    drawRoundedRect(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, 0.12f, 0.15f, 0.20f, 0.95f);
    drawRoundedRectOutline(r, addBtnX, cy + 8.0f, addBtnW, addBtnH, 4.0f, accent.r, accent.g, accent.b, 0.85f, 1.0f);
    drawCenteredText(r, "+ ADD FX", addBtnX, cy + 8.0f, addBtnW, addBtnH, 9.0f, accent.r, accent.g, accent.b, 1.0f);

    if (data.audioFx.empty()) {
        float phY = cy + 38.0f;
        drawRoundedRect(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f, 0.08f, 0.09f, 0.12f, 0.65f);
        drawRoundedRectOutline(r, cx + 10.0f, phY, cw - 20.0f, 32.0f, 4.0f,
                               theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.35f, 1.0f);
        drawText(r, "No audio effects inserted. Click + ADD FX to insert.", cx + 22.0f, phY + 10.0f, 9.0f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);
        return;
    }

    float itemY = cy + 38.0f;
    float itemW = cw - 16.0f;
    float itemX = cx + 8.0f;

    for (size_t fi = 0; fi < data.audioFx.size(); ++fi) {
        auto& fx = data.audioFx[fi];
        fx.ensureDefaultKnobs();

        float itemH = fx.isExpanded ? 180.0f : 42.0f;
        bool isFirst = (fi == 0);
        bool isLast = (fi == data.audioFx.size() - 1);

        // Card Container
        drawRoundedRect(r, itemX, itemY, itemW, itemH, 6.0f, 0.13f, 0.14f, 0.17f, 0.98f);
        drawRoundedRectOutline(r, itemX, itemY, itemW, itemH, 6.0f,
                               fx.enabled ? accent.r : 0.20f,
                               fx.enabled ? accent.g : 0.22f,
                               fx.enabled ? accent.b : 0.28f,
                               fx.enabled ? 0.75f : 0.40f, 1.0f);

        // 1. Vertical modern pill switch
        float swX = itemX + 10.0f;
        float swY = itemY + 8.0f;
        drawModernPillSwitch(r, swX, swY, fx.enabled, accent);

        // 2. Uppercase title
        char upperNameBuf[64];
        size_t cLen = std::min(fx.name.size(), sizeof(upperNameBuf) - 1);
        for (size_t ci = 0; ci < cLen; ++ci) {
            upperNameBuf[ci] = static_cast<char>(std::toupper(static_cast<unsigned char>(fx.name[ci])));
        }
        upperNameBuf[cLen] = '\0';
        std::string_view upperName(upperNameBuf, cLen);
        float titleX = itemX + 32.0f;
        float titleY = itemY + 13.0f;
        drawText(r, upperName, titleX, titleY, 11.0f,
                 fx.enabled ? 0.96f : 0.55f,
                 fx.enabled ? 0.97f : 0.58f,
                 fx.enabled ? 0.99f : 0.65f, 1.0f);

        // 3. Chevron next to title
        float textW = static_cast<float>(cLen) * 6.8f;
        float chevX = titleX + textW + 8.0f;
        float chevY = itemY + 20.0f;
        if (fx.isExpanded) {
            drawChevronUp(r, chevX, chevY, 7.0f, accent, 1.6f);
        } else {
            drawChevronDown(r, chevX, chevY, 7.0f, accent, 1.6f);
        }

        // 4. Action buttons on right: Fullscreen (⛶), Move Up (^), Move Down (v), Trash (delete)
        float fullX = itemX + itemW - 90.0f;
        float upX = itemX + itemW - 68.0f;
        float downX = itemX + itemW - 46.0f;
        float delX = itemX + itemW - 20.0f;
        float btnCenterY = itemY + 20.0f;

        Color arrowCol = Color(0.55f, 0.60f, 0.68f, 0.85f);
        Color dimmedArrow = Color(0.30f, 0.32f, 0.38f, 0.35f);

        drawFullscreenIcon(r, fullX - 5.5f, btnCenterY - 5.5f, 11.0f, accent, 1.4f);
        drawChevronUp(r, upX, btnCenterY, 8.5f, isFirst ? dimmedArrow : arrowCol, 1.6f);
        drawChevronDown(r, downX, btnCenterY, 8.5f, isLast ? dimmedArrow : arrowCol, 1.6f);
        drawTrashIcon(r, delX, btnCenterY, 13.0f, Color(0.55f, 0.60f, 0.68f, 0.85f), 1.3f);

        // 5. If expanded: Fullscreen button + Inset Faceplate
        if (fx.isExpanded) {
            // Fullscreen button
            float fullBtnW = 54.0f;
            float fullBtnH = 20.0f;
            float fullBtnX = itemX + itemW - fullBtnW - 10.0f;
            float fullBtnY = itemY + 36.0f;
            drawRoundedRect(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, 0.12f, 0.16f, 0.22f, 0.95f);
            drawRoundedRectOutline(r, fullBtnX, fullBtnY, fullBtnW, fullBtnH, 3.0f, accent.r, accent.g, accent.b, 0.65f, 1.0f);
            drawFullscreenIcon(r, fullBtnX + 6.0f, fullBtnY + (fullBtnH - 11.0f) * 0.5f, 11.0f, accent, 1.4f);
            drawTextLeftAlignedY(r, "FULL", fullBtnX + 21.0f, fullBtnY + fullBtnH * 0.5f, 8.5f, accent);

            // Inset Hardware Faceplate
            float fpX = itemX + 10.0f;
            float fpY = itemY + 60.0f;
            float fpW = itemW - 20.0f;
            float fpH = 108.0f;

            if (!fx.customGuiPanel.has_value() && !fx.eatscriptCode.empty()) {
                try {
                    auto parsed = eatsbits::project::PresetLoader::parseFromEatscript(fx.eatscriptCode);
                    if (!parsed.params.empty() || !parsed.guiRoot.children.empty() || !parsed.guiRoot.title.empty()) {
                        fx.customGuiPanel = createGuiPanelFromPresetDefinition(parsed);
                    }
                } catch (...) {}
            }

            if (fx.customGuiPanel.has_value()) {
                for (auto& row : fx.customGuiPanel->rows) {
                    for (auto& w : row.widgets) {
                        for (const auto& k : fx.knobs) {
                            if (stringEqualsIgnoreCase(w.id, k.name) || stringEqualsIgnoreCase(w.label, k.name) ||
                                stringEqualsIgnoreCase(w.label, k.label) || stringEqualsIgnoreCase(w.param, k.name)) {
                                w.currentVal = (w.maxVal > w.minVal) ? (w.minVal + std::clamp(k.value, 0.0f, 1.0f) * (w.maxVal - w.minVal)) : k.value;
                                if (!k.display.empty()) w.valueDisplay = k.display;
                                break;
                            }
                        }
                    }
                }
                fx.customGuiPanel->hideHeader = true;
                Rect2D fpRect(fpX, fpY, fpW, fpH);
                drawGuiFaceplate(r, *fx.customGuiPanel, fpRect, theme, nullptr, 0, -1, -1);
            } else {
                bool isSnes = (fx.background == "snes");
                bool isGrunge = (fx.background == "grunge");
                bool isSilver = (fx.background == "silver");

                float bgR = isSnes ? 0.85f : (isGrunge ? 0.19f : (isSilver ? 0.78f : 0.13f));
                float bgG = isSnes ? 0.84f : (isGrunge ? 0.16f : (isSilver ? 0.80f : 0.15f));
                float bgB = isSnes ? 0.81f : (isGrunge ? 0.14f : (isSilver ? 0.82f : 0.18f));

                drawRoundedRect(r, fpX, fpY, fpW, fpH, 6.0f, bgR, bgG, bgB, 1.0f);
                drawRoundedRectOutline(r, fpX, fpY, fpW, fpH, 6.0f, 0.08f, 0.09f, 0.12f, 0.90f, 1.2f);

                // Knobs inside faceplate
                size_t knobCount = std::min(fx.knobs.size(), size_t{4});
                if (knobCount > 0) {
                    float colW = fpW / static_cast<float>(knobCount);
                    for (size_t ki = 0; ki < knobCount; ++ki) {
                        const auto& knob = fx.knobs[ki];
                        float kcx = fpX + (static_cast<float>(ki) + 0.5f) * colW;
                        float kcy = fpY + 36.0f;
                        float kRad = 15.0f;

                        drawCircle(r, kcx, kcy + 2.0f, kRad + 1.0f, 0.05f, 0.06f, 0.08f, 0.45f);

                        if (isSnes) {
                            drawCircleRadial3StopGradient(r, kcx, kcy, kRad,
                                                          Color(0.98f, 0.97f, 0.95f, 1.0f),
                                                          Color(0.85f, 0.83f, 0.79f, 1.0f),
                                                          Color(0.55f, 0.53f, 0.49f, 1.0f),
                                                          -0.25f * kRad, -0.30f * kRad, 0.50f, 24);
                            drawCircleOutline(r, kcx, kcy, kRad, 0.45f, 0.43f, 0.40f, 0.9f, 1.0f);
                        } else {
                            drawCircleRadial3StopGradient(r, kcx, kcy, kRad,
                                                          Color(0.85f, 0.88f, 0.92f, 1.0f),
                                                          Color(0.40f, 0.44f, 0.50f, 1.0f),
                                                          Color(0.18f, 0.20f, 0.24f, 1.0f),
                                                          -0.25f * kRad, -0.30f * kRad, 0.50f, 24);
                            drawCircleOutline(r, kcx, kcy, kRad, 0.30f, 0.34f, 0.40f, 0.9f, 1.0f);
                        }

                        constexpr float minA = -2.35619449f;
                        constexpr float maxA = 2.35619449f;
                        float curA = minA + std::clamp(knob.value, 0.0f, 1.0f) * (maxA - minA);
                        float needleR = isSnes ? 0.38f : accent.r;
                        float needleG = isSnes ? 0.28f : accent.g;
                        float needleB = isSnes ? 0.65f : accent.b;
                        drawLine(r, kcx, kcy, kcx + std::sin(curA) * (kRad - 2.0f), kcy - std::cos(curA) * (kRad - 2.0f),
                                 needleR, needleG, needleB, 1.0f, 1.8f);

                        float lblY = fpY + 58.0f;
                        float lblR = isSnes ? 0.42f : 0.70f;
                        float lblG = isSnes ? 0.35f : 0.75f;
                        float lblB = isSnes ? 0.75f : 0.85f;
                        drawCenteredText(r, knob.label, kcx - colW * 0.5f, lblY, colW, 14.0f, 8.5f,
                                         lblR, lblG, lblB, 1.0f);

                        float pillW = 42.0f;
                        float pillH = 15.0f;
                        float pillX = kcx - (pillW * 0.5f);
                        float pillY = fpY + 76.0f;
                        if (isSnes) {
                            drawRoundedRect(r, pillX, pillY, pillW, pillH, 3.0f, 0.22f, 0.24f, 0.28f, 0.25f);
                            drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, 3.0f, 0.35f, 0.38f, 0.44f, 0.35f, 1.0f);
                            drawCenteredText(r, knob.display, pillX, pillY + 1.0f, pillW, pillH, 7.5f, 0.30f, 0.32f, 0.38f, 1.0f);
                        } else {
                            drawRoundedRect(r, pillX, pillY, pillW, pillH, 3.0f, 0.08f, 0.09f, 0.12f, 0.85f);
                            drawRoundedRectOutline(r, pillX, pillY, pillW, pillH, 3.0f, 0.25f, 0.28f, 0.35f, 0.70f, 1.0f);
                            drawCenteredText(r, knob.display, pillX, pillY + 1.0f, pillW, pillH, 7.5f, 0.80f, 0.84f, 0.90f, 1.0f);
                        }
                    }
                }
            }
        }

        itemY += itemH + 8.0f;
    }
}

void TrackPropertiesPanel::renderColorPalette(BatchRenderer2D& r, const ThemeTokens& theme,
                                              TrackPropertiesDrawerData& data, float cx, float cy, float cw) {
    float h = 36.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    drawText(r, "TRACK COLOR", cx + 12.0f, cy + 12.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);

    float startX = cx + (cw >= 560.0f ? 120.0f : 95.0f);
    float availW = (cx + cw - 14.0f) - startX;
    float swatchSpacing = availW / 7.0f;
    float scy = cy + h * 0.5f;

    // 6 Color Swatches: Gray, Cyan, Green, Magenta, Red, Yellow
    for (int p = 0; p < 6; ++p) {
        float scx = startX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
        float cr = kQuickPalette[p][0];
        float cg = kQuickPalette[p][1];
        float cb = kQuickPalette[p][2];
        bool isCurCol = (std::abs(data.r - cr) < 0.05f && std::abs(data.g - cg) < 0.05f && std::abs(data.b - cb) < 0.05f);

        drawCircle(r, scx, scy, 9.0f, cr, cg, cb, 1.0f);
        if (isCurCol) {
            drawCircleOutline(r, scx, scy, 12.5f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f);
        }
    }

    // 7th slot: Circle with '+' option to open custom color selection dialog
    float plusX = startX + 6.0f * swatchSpacing + swatchSpacing * 0.5f;
    drawCircle(r, plusX, scy, 9.0f, theme.controlWell.r, theme.controlWell.g, theme.controlWell.b, 1.0f);
    drawCircleOutline(r, plusX, scy, 9.0f, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.8f, 1.0f);
    drawLine(r, plusX - 4.5f, scy, plusX + 4.5f, scy, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.95f, 1.5f);
    drawLine(r, plusX, scy - 4.5f, plusX, scy + 4.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 0.95f, 1.5f);
}

void TrackPropertiesPanel::renderTrackActionsCard(BatchRenderer2D& r, const ThemeTokens& theme,
                                                  TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                                  bool isWide, float mouseX, float mouseY) {
    (void)isWide;
    float h = 68.0f;
    if (cy + h < bounds_.y || cy > bounds_.y + bounds_.h) return;

    drawRoundedRect(r, cx, cy, cw, h, 6.0f,
                    theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 0.95f);
    drawRoundedRectOutline(r, cx, cy, cw, h, 6.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.6f, 1.0f);

    // Header label: TRACK ACTIONS
    drawText(r, "TRACK ACTIONS", cx + 10.0f, cy + 7.5f, 9.0f,
             data.r, data.g, data.b, 1.0f);
    std::string trkNumStr = "TRACK " + std::to_string(data.trackIndex + 1) + " OF " + std::to_string(data.totalTracks);
    drawText(r, trkNumStr, cx + cw - 10.0f - (static_cast<float>(trkNumStr.size()) * 5.5f), cy + 7.5f, 8.5f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.75f);

    // Action buttons row
    float btnY = cy + 26.0f;
    float btnH = 30.0f;
    float gap = 6.0f;
    float btnW = (cw - 20.0f - (gap * 2.0f)) / 3.0f;

    float b1X = cx + 10.0f;
    float b2X = b1X + btnW + gap;
    float b3X = b2X + btnW + gap;

    bool b1Hov = (mouseX >= b1X && mouseX <= b1X + btnW && mouseY >= btnY && mouseY <= btnY + btnH);
    bool b2Hov = (mouseX >= b2X && mouseX <= b2X + btnW && mouseY >= btnY && mouseY <= btnY + btnH);
    bool b3Hov = (mouseX >= b3X && mouseX <= b3X + btnW && mouseY >= btnY && mouseY <= btnY + btnH);

    // Button 1: + ADD CLIP
    drawRoundedRect(r, b1X, btnY, btnW, btnH, 4.0f,
                    b1Hov ? 0.22f : 0.14f, b1Hov ? 0.25f : 0.16f, b1Hov ? 0.28f : 0.18f, 0.95f);
    drawRoundedRectOutline(r, b1X, btnY, btnW, btnH, 4.0f,
                           data.r, data.g, data.b, b1Hov ? 0.95f : 0.65f, 1.2f);
    drawCenteredText(r, "+ ADD CLIP", b1X, btnY + 8.0f, btnW, 14.0f, 9.5f,
                     data.r, data.g, data.b, 1.0f);

    // Button 2: DUPLICATE
    drawRoundedRect(r, b2X, btnY, btnW, btnH, 4.0f,
                    b2Hov ? 0.22f : 0.14f, b2Hov ? 0.25f : 0.16f, b2Hov ? 0.28f : 0.18f, 0.95f);
    drawRoundedRectOutline(r, b2X, btnY, btnW, btnH, 4.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, b2Hov ? 0.90f : 0.50f, 1.0f);
    drawCenteredText(r, "DUPLICATE", b2X, btnY + 8.0f, btnW, 14.0f, 9.5f,
                     b2Hov ? 1.0f : theme.textSecondary.r,
                     b2Hov ? 1.0f : theme.textSecondary.g,
                     b2Hov ? 1.0f : theme.textSecondary.b, 1.0f);

    // Button 3: DELETE (disabled if only 1 track left)
    bool canDelete = (data.totalTracks > 1);
    if (canDelete) {
        drawRoundedRect(r, b3X, btnY, btnW, btnH, 4.0f,
                        b3Hov ? 0.35f : 0.18f, b3Hov ? 0.12f : 0.10f, b3Hov ? 0.14f : 0.11f, 0.95f);
        drawRoundedRectOutline(r, b3X, btnY, btnW, btnH, 4.0f,
                               1.0f, 0.30f, 0.40f, b3Hov ? 0.95f : 0.60f, 1.2f);
        drawCenteredText(r, "DELETE", b3X, btnY + 8.0f, btnW, 14.0f, 9.5f,
                         1.0f, 0.35f, 0.45f, 1.0f);
    } else {
        drawRoundedRect(r, b3X, btnY, btnW, btnH, 4.0f,
                        0.10f, 0.10f, 0.12f, 0.60f);
        drawRoundedRectOutline(r, b3X, btnY, btnW, btnH, 4.0f,
                               0.25f, 0.25f, 0.28f, 0.40f, 1.0f);
        drawCenteredText(r, "DELETE", b3X, btnY + 8.0f, btnW, 14.0f, 9.5f,
                         0.40f, 0.40f, 0.45f, 0.6f);
    }
}

void TrackPropertiesPanel::renderMasterSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                               TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                               float mouseX, float mouseY) {
    (void)mouseX; (void)mouseY;
    float curY = cy;

    // Header card
    float headH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, headH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, headH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawRoundedRect(r, cx + 10.0f, curY + 10.0f, 5.0f, 30.0f, 2.5f, 1.0f, 0.85f, 0.0f, 1.0f);
    drawText(r, "MASTER BUS CONSOLE", cx + 24.0f, curY + 12.0f, 13.5f, 1.0f, 0.88f, 0.10f, 1.0f);
    drawText(r, "STEREO SUMMING & DYNAMICS MASTERING CHAIN", cx + 24.0f, curY + 30.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    curY += headH + 8.0f;

    // Master EQ Card
    float eqH = 100.0f;
    drawRoundedRect(r, cx, curY, cw, eqH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, eqH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "MASTER BUS EQUALIZER", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    float kw4 = cw / 4.0f;
    auto drawKnobMini = [&](float kx, float ky, const std::string& label, const std::string& valStr, float normVal) {
        KnobStyleCore miniStyle = KnobStyleCore::minimal(theme.primaryAccent);
        drawUnifiedKnob(r, kx, ky, 12.0f, normVal, miniStyle, label, valStr, &theme);
    };

    float eqRowY = curY + 44.0f;
    drawKnobMini(cx + kw4 * 0.5f, eqRowY, "SUB CUT", std::to_string(static_cast<int>(data.masterSubCut)) + "Hz", (data.masterSubCut - 20.0f) / 25.0f);
    drawKnobMini(cx + kw4 * 1.5f, eqRowY, "LOW", (data.masterLowGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterLowGain)) + "dB", (data.masterLowGain + 12.0f) / 24.0f);
    drawKnobMini(cx + kw4 * 2.5f, eqRowY, "MID", (data.masterMidGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterMidGain)) + "dB", (data.masterMidGain + 12.0f) / 24.0f);
    drawKnobMini(cx + kw4 * 3.5f, eqRowY, "HIGH", (data.masterHighGain >= 0 ? "+" : "") + std::to_string(static_cast<int>(data.masterHighGain)) + "dB", (data.masterHighGain + 12.0f) / 24.0f);
    curY += eqH + 8.0f;

    // Master Limiter Card
    float limH = 110.0f;
    drawRoundedRect(r, cx, curY, cw, limH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, limH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "BRICKWALL LIMITER & LOUDNESS", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 1.0f);

    float autoBtnW = 100.0f;
    drawButton(r, Rect2D(cx + cw - autoBtnW - 10.0f, curY + 6.0f, autoBtnW, 20.0f), "[ AUTO-MASTER ]",
               theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent, 8.5f, 3.0f, 1.0f);

    float limRowY = curY + 48.0f;
    float kw3 = cw / 3.0f;
    drawKnobMini(cx + kw3 * 0.5f, limRowY, "CEILING", std::to_string(data.masterCeilingDbfs).substr(0, 4) + "dB", (data.masterCeilingDbfs + 2.0f) / 2.0f);
    drawKnobMini(cx + kw3 * 1.5f, limRowY, "DRIVE", "+" + std::to_string(static_cast<int>(data.masterLimiterDrive)) + "dB", data.masterLimiterDrive / 12.0f);
    drawKnobMini(cx + kw3 * 2.5f, limRowY, "TARGET LUFS", std::to_string(static_cast<int>(data.masterTargetLufs)) + "LUFS", (data.masterTargetLufs + 24.0f) / 18.0f);
}

void TrackPropertiesPanel::renderClipSection(BatchRenderer2D& r, const ThemeTokens& theme,
                                             TrackPropertiesDrawerData& data, float cx, float cy, float cw,
                                             float mouseX, float mouseY) {
    (void)mouseX; (void)mouseY;
    float curY = cy;

    if (data.selectedClipIndex < 0 || data.clipName.empty()) {
        float emptyH = 120.0f;
        drawRoundedRect(r, cx, curY, cw, emptyH, 6.0f, theme.controlBackground, 0.95f);
        drawRoundedRectOutline(r, cx, curY, cw, emptyH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
        drawText(r, "NO CLIP SELECTED", cx + 16.0f, curY + 16.0f, 12.0f,
                 theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);
        drawText(r, "Select an existing clip on the timeline or create a new one.",
                 cx + 16.0f, curY + 38.0f, 9.5f,
                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.9f);
        float addBtnW = 120.0f;
        drawButton(r, Rect2D(cx + 16.0f, curY + 68.0f, addBtnW, 28.0f),
                   "+ NEW CLIP", theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent,
                   9.5f, 4.0f, 1.0f);
        return;
    }

    float headH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, headH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, headH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawRoundedRect(r, cx + 10.0f, curY + 10.0f, 5.0f, 30.0f, 2.5f, theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
    drawText(r, data.clipName, cx + 24.0f, curY + 12.0f, 13.5f, theme.textPrimary.r, theme.textPrimary.g, theme.textPrimary.b, 1.0f);
    drawText(r, "MIDI SEQUENCE CLIP", cx + 24.0f, curY + 30.0f, 9.0f,
             theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 0.85f);
    curY += headH + 8.0f;

    // Timing & Looping Card
    float timeH = 90.0f;
    drawRoundedRect(r, cx, curY, cw, timeH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, timeH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);
    drawText(r, "CLIP TIMING & PLAYBACK", cx + 12.0f, curY + 8.0f, 10.5f,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);

    std::string posStr = "Start: Bar " + std::to_string(data.clipStartBar) + "  •  Length: " + std::to_string(data.clipLengthBars) + " Bars";
    drawText(r, posStr, cx + 12.0f, curY + 32.0f, 9.5f, theme.textSecondary.r, theme.textSecondary.g, theme.textSecondary.b, 1.0f);

    float loopBtnW = 90.0f;
    drawButton(r, Rect2D(cx + 12.0f, curY + 54.0f, loopBtnW, 24.0f),
               data.clipLooped ? "LOOP: ON" : "LOOP: OFF",
               data.clipLooped ? theme.primaryAccent * 0.3f : Color(0.12f, 0.14f, 0.18f, 0.9f),
               theme.borderSubtle, theme.primaryAccent, 8.5f, 3.0f, 1.0f);

    // Edit in Piano Roll Button
    float editBtnW = 140.0f;
    drawButton(r, Rect2D(cx + cw - editBtnW - 12.0f, curY + 54.0f, editBtnW, 24.0f),
               "[ EDIT IN PIANO ROLL ]", theme.secondaryAccent * 0.25f, theme.secondaryAccent, theme.secondaryAccent,
               8.5f, 3.0f, 1.0f);
    curY += timeH + 8.0f;

    // Clip Actions Card (Duplicate & Delete Parity with Eatsbeats)
    float actH = 50.0f;
    drawRoundedRect(r, cx, curY, cw, actH, 6.0f, theme.controlBackground, 0.95f);
    drawRoundedRectOutline(r, cx, curY, cw, actH, 6.0f, theme.borderSubtle, 0.6f, 1.0f);

    float actBtnW = (cw - 32.0f) * 0.5f;
    Rect2D dupRect(cx + 12.0f, curY + 12.0f, actBtnW, 26.0f);
    Rect2D delRect(cx + 20.0f + actBtnW, curY + 12.0f, actBtnW, 26.0f);

    // Duplicate Clip button
    drawButton(r, dupRect, "DUPLICATE",
               theme.primaryAccent * 0.25f, theme.primaryAccent, theme.primaryAccent,
               9.0f, 3.5f, 1.0f);

    // Delete Clip button (crimson red)
    Color delBg(0.25f, 0.08f, 0.12f, 0.85f);
    Color delBorder(1.0f, 0.30f, 0.43f, 0.90f);
    Color delText(1.0f, 0.40f, 0.50f, 1.0f);
    drawButton(r, delRect, "DELETE",
               delBg, delBorder, delText,
               9.0f, 3.5f, 1.0f);
}

void TrackPropertiesPanel::renderScrollbar(BatchRenderer2D& r, const ThemeTokens& theme) {
    drawRoundedRect(r, scrollbarBounds_.x, scrollbarBounds_.y, scrollbarBounds_.w, scrollbarBounds_.h, 2.5f,
                    Color(0.06f, 0.07f, 0.09f, 0.85f));

    float maxScroll = (std::max)(0.0f, totalContentHeight_ - bounds_.h);
    if (maxScroll <= 0.0f) return;

    float thumbRatio = std::clamp(bounds_.h / totalContentHeight_, 0.15f, 0.90f);
    float thumbH = scrollbarBounds_.h * thumbRatio;
    float normScroll = std::clamp(scrollY_ / maxScroll, 0.0f, 1.0f);
    float thumbY = scrollbarBounds_.y + normScroll * (scrollbarBounds_.h - thumbH);

    bool isDragging = (dragMode_ == DragMode::Scrollbar);
    Color thumbColor = isDragging ? theme.primaryAccent : Color(0.40f, 0.45f, 0.55f, 0.70f);
    drawRoundedRect(r, scrollbarBounds_.x, thumbY, scrollbarBounds_.w, thumbH, 2.5f,
                    thumbColor, isDragging ? 1.0f : 0.70f);
}

std::string TrackPropertiesPanel::getTooltip(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    if (!bounds_.contains(mx, my)) return "";

    auto hit = hitTest(mx, my, data);
    if (!hit.hit) return "";

    switch (hit.area) {
        case TrackPropertiesHitArea::TrackIcon:
            return "Change Track Icon & Color";
        case TrackPropertiesHitArea::RenameButton:
            return "Edit Track Name: " + data.trackName;
        case TrackPropertiesHitArea::ColorSwatch:
            return "Select Track Color Palette";
        case TrackPropertiesHitArea::VolumeSlider: {
            int volPct = static_cast<int>(std::round(data.volume * 100.0f));
            return "Track Volume (" + std::to_string(volPct) + "%)";
        }
        case TrackPropertiesHitArea::PanKnob: {
            std::string panStr = (std::abs(data.pan) < 0.04f) ? "Center" : ((data.pan < 0.0f) ? "Left " + std::to_string(static_cast<int>(std::round(-data.pan * 100.0f))) + "%" : "Right " + std::to_string(static_cast<int>(std::round(data.pan * 100.0f))) + "%");
            return "Track Stereo Pan: " + panStr;
        }
        case TrackPropertiesHitArea::DesignButton:
            return "Open in Design View (Modular / Code)";
        case TrackPropertiesHitArea::PresetButton:
            return "Browse Presets & Sound Library";
        case TrackPropertiesHitArea::FullscreenInstrument:
            return "Open Fullscreen Device Panel (F)";
        case TrackPropertiesHitArea::ToggleInstrumentExpand:
            return data.instrumentExpanded ? "Collapse Instrument Device" : "Expand Instrument Device";
        case TrackPropertiesHitArea::ChangeInstrument:
            return "Choose Instrument Plugin or Preset";
        case TrackPropertiesHitArea::InstrumentKnob: {
            if (hit.index >= 0 && static_cast<size_t>(hit.index) < data.knobs.size()) {
                const auto& k = data.knobs[hit.index];
                return k.label + ": " + k.display;
            }
            return "Instrument Parameter";
        }
        case TrackPropertiesHitArea::ChordFollowChip: {
            const char* modes[5] = {"Off (Chromatic)", "Chord Follow", "Bass Root Follow", "Scale Snap", "Color / Harmonic Lead"};
            if (hit.index >= 0 && hit.index < 5) {
                return std::string("Harmonic Follow: ") + modes[hit.index];
            }
            return "Harmonic Chord Track Follow";
        }
        case TrackPropertiesHitArea::BakeChords:
            return "Bake Transposed Notes into MIDI Clip";
        case TrackPropertiesHitArea::AddMidiFx:
            return "Add MIDI Effect (Arpeggiator, Chords, Humanize, Scale Snap)";
        case TrackPropertiesHitArea::ToggleMidiFx:
            return "Bypass / Enable MIDI Effect";
        case TrackPropertiesHitArea::MoveMidiFxUp:
            return "Move MIDI Effect Earlier in Chain";
        case TrackPropertiesHitArea::MoveMidiFxDown:
            return "Move MIDI Effect Later in Chain";
        case TrackPropertiesHitArea::RemoveMidiFx:
            return "Remove MIDI Effect";
        case TrackPropertiesHitArea::FullscreenMidiFx:
            return "Open Fullscreen Effect Editor";
        case TrackPropertiesHitArea::ToggleMidiFxExpand:
            return "Expand / Collapse MIDI Effect";
        case TrackPropertiesHitArea::MidiFxKnob: {
            size_t mi = static_cast<size_t>(hit.index / 10);
            size_t ki = static_cast<size_t>(hit.index % 10);
            if (mi < data.midiFx.size() && ki < data.midiFx[mi].knobs.size()) {
                const auto& k = data.midiFx[mi].knobs[ki];
                return k.label + ": " + k.display;
            }
            return "MIDI FX Parameter";
        }
        case TrackPropertiesHitArea::AddAudioFx:
            return "Add Audio Effect (Reverb, Delay, Bitcrusher, Limiter)";
        case TrackPropertiesHitArea::ToggleAudioFx:
            return "Bypass / Enable Audio Effect";
        case TrackPropertiesHitArea::MoveAudioFxUp:
            return "Move Audio Effect Earlier in Chain";
        case TrackPropertiesHitArea::MoveAudioFxDown:
            return "Move Audio Effect Later in Chain";
        case TrackPropertiesHitArea::RemoveAudioFx:
            return "Remove Audio Effect";
        case TrackPropertiesHitArea::FullscreenAudioFx:
            return "Open Fullscreen Effect Editor";
        case TrackPropertiesHitArea::ToggleAudioFxExpand:
            return "Expand / Collapse Audio Effect";
        case TrackPropertiesHitArea::AudioFxKnob: {
            size_t fi = static_cast<size_t>(hit.index / 10);
            size_t ki = static_cast<size_t>(hit.index % 10);
            if (fi < data.audioFx.size() && ki < data.audioFx[fi].knobs.size()) {
                const auto& k = data.audioFx[fi].knobs[ki];
                return k.label + ": " + k.display;
            }
            return "Audio FX Parameter";
        }
        case TrackPropertiesHitArea::CustomColorPicker:
            return "Open Color Selection Dialog";
        case TrackPropertiesHitArea::EqToggle:
            return data.eqEnabled ? "Bypass 3-Band Parametric EQ" : "Activate 3-Band Parametric EQ";
        case TrackPropertiesHitArea::EqHpf: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "HPF Cut: %d Hz", static_cast<int>(data.eqHpf));
            return buf;
        }
        case TrackPropertiesHitArea::EqLowGain: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Low Gain: %s%d dB", data.eqLowGain >= 0 ? "+" : "", static_cast<int>(data.eqLowGain));
            return buf;
        }
        case TrackPropertiesHitArea::EqHighGain: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "High Gain: %s%d dB", data.eqHighGain >= 0 ? "+" : "", static_cast<int>(data.eqHighGain));
            return buf;
        }
        case TrackPropertiesHitArea::EqMidFreq: {
            static char buf[32];
            if (data.eqMidFreq >= 1000.0f) {
                std::snprintf(buf, sizeof(buf), "Mid Freq: %.1f kHz", data.eqMidFreq / 1000.0f);
            } else {
                std::snprintf(buf, sizeof(buf), "Mid Freq: %d Hz", static_cast<int>(data.eqMidFreq));
            }
            return buf;
        }
        case TrackPropertiesHitArea::EqMidGain: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Mid Gain: %s%d dB", data.eqMidGain >= 0 ? "+" : "", static_cast<int>(data.eqMidGain));
            return buf;
        }
        case TrackPropertiesHitArea::EqMidQ: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Mid Q: %.1f", data.eqMidQ);
            return buf;
        }
        case TrackPropertiesHitArea::MasterEqSubCut: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Master Sub Cut: %d Hz", static_cast<int>(data.masterSubCut));
            return buf;
        }
        case TrackPropertiesHitArea::MasterEqLow: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Master Low Gain: %s%d dB", data.masterLowGain >= 0 ? "+" : "", static_cast<int>(data.masterLowGain));
            return buf;
        }
        case TrackPropertiesHitArea::MasterEqMid: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Master Mid Gain: %s%d dB", data.masterMidGain >= 0 ? "+" : "", static_cast<int>(data.masterMidGain));
            return buf;
        }
        case TrackPropertiesHitArea::MasterEqHigh: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Master High Gain: %s%d dB", data.masterHighGain >= 0 ? "+" : "", static_cast<int>(data.masterHighGain));
            return buf;
        }
        case TrackPropertiesHitArea::MasterCeiling: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Master Ceiling: %.1f dB", data.masterCeilingDbfs);
            return buf;
        }
        case TrackPropertiesHitArea::MasterDrive: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Master Drive: +%d dB", static_cast<int>(data.masterLimiterDrive));
            return buf;
        }
        case TrackPropertiesHitArea::MasterLufs: {
            static char buf[32];
            std::snprintf(buf, sizeof(buf), "Target LUFS: %d LUFS", static_cast<int>(data.masterTargetLufs));
            return buf;
        }
        case TrackPropertiesHitArea::MuteButton:
            return data.mute ? "Unmute Track (M)" : "Mute Track (M)";
        case TrackPropertiesHitArea::SoloButton:
            return data.solo ? "Deactivate Solo (S)" : "Solo Track (S)";
        case TrackPropertiesHitArea::AddClipButton:
            return "Create New Empty Clip on Track";
        case TrackPropertiesHitArea::DuplicateTrackButton:
            return "Duplicate Track & Settings";
        case TrackPropertiesHitArea::DeleteTrackButton:
            return (data.totalTracks <= 1) ? "Cannot delete only remaining track" : "Delete Track";
        case TrackPropertiesHitArea::ClipDuplicate:
            return "Duplicate Selected Clip";
        case TrackPropertiesHitArea::ClipDelete:
            return "Delete Selected Clip";
        case TrackPropertiesHitArea::ClipEditInPianoRoll:
            return "Open Selected Clip in Piano Roll / MIDI Editor";
        case TrackPropertiesHitArea::ClipLoopToggle:
            return "Toggle Clip Loop Playback";
        case TrackPropertiesHitArea::PullTab:
            return "Properties Drawer Pull Tab";
        case TrackPropertiesHitArea::CloseButton:
            return "Close Properties (Esc)";
        case TrackPropertiesHitArea::FocusDetachedWindow:
            return "Bring Floating Window to Front (Focus)";
        case TrackPropertiesHitArea::ReattachDetachedWindow:
            return "Re-Attach / Dock Floating Window back to Track";
        case TrackPropertiesHitArea::TabTrack:
            return "Track Properties & Device Chain";
        case TrackPropertiesHitArea::TabClip:
            return "Clip Inspector & Playback Parameters";
        default:
            break;
    }
    return "";
}

TrackPropertiesHitResult TrackPropertiesPanel::hitTest(float mx, float my, const TrackPropertiesDrawerData& data) const noexcept {
    TrackPropertiesHitResult res;
    if (bounds_.w > 0.0f && bounds_.h > 0.0f && !bounds_.contains(mx, my)) {
        return res;
    }

    // Scrollbar hit
    if (scrollbarBounds_.contains(mx, my)) {
        res.hit = true;
        res.area = TrackPropertiesHitArea::ScrollbarTrack;
        return res;
    }

    // Ribbon hits
    if (showTrackRibbon_ && ribbonBounds_.contains(mx, my)) {
        size_t trackCount = data.allTrackNames.empty() ? 5 : data.allTrackNames.size();
        const float startX = ribbonBounds_.x + 4.0f;
        const float tabW = std::clamp((ribbonBounds_.w - 40.0f) / static_cast<float>(trackCount), 90.0f, 130.0f);
        const float tabGap = 8.0f;
        for (size_t i = 0; i < trackCount; ++i) {
            float tx = startX + static_cast<float>(i) * (tabW + tabGap);
            if (mx >= tx && mx <= tx + tabW) {
                res.hit = true;
                res.index = static_cast<int>(i);
                return res;
            }
        }
    }

    // Master Console Mode Hits
    if (data.isMasterSelected || data.tab == TrackPropertiesTab::Master) {
        float padding = (bounds_.w >= 560.0f) ? 14.0f : 8.0f;
        float cx = bounds_.x + padding;
        float cw = bounds_.w - (padding * 2.0f);
        float curY = bounds_.y + 8.0f - scrollY_;
        curY += 50.0f + 8.0f; // Header card

        // Master EQ Card
        float kw4 = cw / 4.0f;
        float eqRowY = curY + 44.0f;
        float k1x = cx + kw4 * 0.5f;
        if (std::hypot(mx - k1x, my - eqRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterEqSubCut;
            res.normVal = (data.masterSubCut - 20.0f) / 25.0f;
            return res;
        }
        float k2x = cx + kw4 * 1.5f;
        if (std::hypot(mx - k2x, my - eqRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterEqLow;
            res.normVal = (data.masterLowGain + 12.0f) / 24.0f;
            return res;
        }
        float k3x = cx + kw4 * 2.5f;
        if (std::hypot(mx - k3x, my - eqRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterEqMid;
            res.normVal = (data.masterMidGain + 12.0f) / 24.0f;
            return res;
        }
        float k4x = cx + kw4 * 3.5f;
        if (std::hypot(mx - k4x, my - eqRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterEqHigh;
            res.normVal = (data.masterHighGain + 12.0f) / 24.0f;
            return res;
        }
        curY += 100.0f + 8.0f;

        // Master Limiter Card
        float limRowY = curY + 48.0f;
        float kw3 = cw / 3.0f;
        float l1x = cx + kw3 * 0.5f;
        if (std::hypot(mx - l1x, my - limRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterCeiling;
            res.normVal = (data.masterCeilingDbfs + 2.0f) / 2.0f;
            return res;
        }
        float l2x = cx + kw3 * 1.5f;
        if (std::hypot(mx - l2x, my - limRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterDrive;
            res.normVal = data.masterLimiterDrive / 12.0f;
            return res;
        }
        float l3x = cx + kw3 * 2.5f;
        if (std::hypot(mx - l3x, my - limRowY) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::MasterLufs;
            res.normVal = (data.masterTargetLufs + 24.0f) / 18.0f;
            return res;
        }
        return res;
    }

    // Clip Mode Hits
    if (data.tab == TrackPropertiesTab::Clip) {
        float cx = bounds_.x + 8.0f;
        float cw = bounds_.w - 16.0f;
        float curY = bounds_.y + 8.0f - scrollY_;

        if (data.selectedClipIndex < 0 || data.clipName.empty()) {
            // Check "+ NEW CLIP" button hit
            float addBtnW = 120.0f;
            Rect2D addRect(cx + 16.0f, curY + 68.0f, addBtnW, 28.0f);
            if (addRect.contains(mx, my)) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::AddClipButton;
                return res;
            }
            return res;
        }

        float headH = 50.0f;
        curY += headH + 8.0f;

        // Timing & Looping Card
        float timeH = 90.0f;
        float loopBtnW = 90.0f;
        Rect2D loopBtn(cx + 12.0f, curY + 54.0f, loopBtnW, 24.0f);
        if (loopBtn.contains(mx, my)) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ClipLoopToggle;
            return res;
        }

        float editBtnW = 140.0f;
        Rect2D editBtn(cx + cw - editBtnW - 12.0f, curY + 54.0f, editBtnW, 24.0f);
        if (editBtn.contains(mx, my)) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ClipEditInPianoRoll;
            return res;
        }
        curY += timeH + 8.0f;

        // Clip Actions Card (Duplicate & Delete)
        float actBtnW = (cw - 32.0f) * 0.5f;
        Rect2D dupRect(cx + 12.0f, curY + 12.0f, actBtnW, 26.0f);
        Rect2D delRect(cx + 20.0f + actBtnW, curY + 12.0f, actBtnW, 26.0f);
        if (dupRect.contains(mx, my)) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ClipDuplicate;
            return res;
        }
        if (delRect.contains(mx, my)) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ClipDelete;
            return res;
        }
        return res;
    }

    // Header Card Hits: [Icon] [Title] [EDIT]
    if (headerCardBounds_.contains(mx, my)) {
        float cx = headerCardBounds_.x;
        float cy = headerCardBounds_.y;
        float cw = headerCardBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);
        float h = isWide ? 40.0f : 36.0f;

        // 1. Track icon button on left
        float iconBtnX = cx + 8.0f;
        float iconBtnW = 24.0f;
        float iconBtnH = 24.0f;
        float iconBtnY = cy + (h - iconBtnH) * 0.5f;
        if (mx >= iconBtnX && mx <= iconBtnX + iconBtnW && my >= iconBtnY && my <= iconBtnY + iconBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::TrackIcon;
            return res;
        }

        // 2. Edit icon button on right
        float editBtnW = 20.0f;
        float editBtnH = 20.0f;
        float editBtnX = cx + cw - 12.0f - editBtnW;
        float editBtnY = cy + (h - editBtnH) * 0.5f;

        if (mx >= editBtnX - 4.0f && mx <= editBtnX + editBtnW + 4.0f && my >= cy && my <= cy + h) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::RenameButton;
            return res;
        }

        // Clicking anywhere on track title text also triggers track rename / properties dialog
        res.hit = true;
        res.area = TrackPropertiesHitArea::RenameButton;
        return res;
    }

    // Color Swatches Card Hits (directly below Header Card)
    if (colorCardBounds_.contains(mx, my)) {
        float cx = colorCardBounds_.x;
        float cy = colorCardBounds_.y;
        float cw = colorCardBounds_.w;
        float startX = cx + (cw >= 560.0f ? 120.0f : 95.0f);
        float availW = (cx + cw - 14.0f) - startX;
        float swatchSpacing = availW / 7.0f;
        float scy = cy + 18.0f;
        for (int p = 0; p < 6; ++p) {
            float scx = startX + static_cast<float>(p) * swatchSpacing + swatchSpacing * 0.5f;
            if (std::hypot(mx - scx, my - scy) <= 12.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ColorSwatch;
                res.index = p;
                return res;
            }
        }
        // Circle + Custom Color Picker
        float plusX = startX + 6.0f * swatchSpacing + swatchSpacing * 0.5f;
        if (std::hypot(mx - plusX, my - scy) <= 12.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::CustomColorPicker;
            return res;
        }
    }

    // EQ Card Hits (Channel 3-Band Parametric EQ)
    if (data.isMixerMode && !data.isMasterSelected && eqCardBounds_.w > 0.0f && eqCardBounds_.contains(mx, my)) {
        float cx = eqCardBounds_.x;
        float cy = eqCardBounds_.y;
        float cw = eqCardBounds_.w;

        // Toggle ACTIVE / BYPASS button at top right
        float eqTglW = 60.0f;
        float eqTglX = cx + cw - eqTglW - 10.0f;
        if (mx >= eqTglX && mx <= eqTglX + eqTglW && my >= cy + 4.0f && my <= cy + 28.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqToggle;
            return res;
        }

        float kw3 = cw / 3.0f;
        float eqRow1Y = cy + 45.0f;
        float eqRow2Y = cy + 98.0f;

        // Row 1: HPF CUT, LOW GAIN, HIGH GAIN
        float k1x = cx + kw3 * 0.5f;
        if (std::hypot(mx - k1x, my - eqRow1Y) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqHpf;
            res.normVal = std::clamp((data.eqHpf - 20.0f) / 480.0f, 0.0f, 1.0f);
            return res;
        }
        float k2x = cx + kw3 * 1.5f;
        if (std::hypot(mx - k2x, my - eqRow1Y) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqLowGain;
            res.normVal = std::clamp((data.eqLowGain + 18.0f) / 36.0f, 0.0f, 1.0f);
            return res;
        }
        float k3x = cx + kw3 * 2.5f;
        if (std::hypot(mx - k3x, my - eqRow1Y) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqHighGain;
            res.normVal = std::clamp((data.eqHighGain + 18.0f) / 36.0f, 0.0f, 1.0f);
            return res;
        }

        // Row 2: MID FREQ, MID GAIN, MID Q
        float k4x = cx + kw3 * 0.5f;
        if (std::hypot(mx - k4x, my - eqRow2Y) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqMidFreq;
            res.normVal = std::clamp((data.eqMidFreq - 200.0f) / 7800.0f, 0.0f, 1.0f);
            return res;
        }
        float k5x = cx + kw3 * 1.5f;
        if (std::hypot(mx - k5x, my - eqRow2Y) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqMidGain;
            res.normVal = std::clamp((data.eqMidGain + 18.0f) / 36.0f, 0.0f, 1.0f);
            return res;
        }
        float k6x = cx + kw3 * 2.5f;
        if (std::hypot(mx - k6x, my - eqRow2Y) <= 18.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::EqMidQ;
            res.normVal = std::clamp((data.eqMidQ - 0.3f) / 9.7f, 0.0f, 1.0f);
            return res;
        }
    }

    // Faceplate Hits (Eatsbeats Original Instrument Layout)
    if (faceplateBounds_.contains(mx, my)) {
        float cx = faceplateBounds_.x;
        float cy = faceplateBounds_.y;
        float cw = faceplateBounds_.w;
        bool isWide = (bounds_.w >= 560.0f);
        float faceH = isWide ? 260.0f : 240.0f;

        float fullBtnW = 46.0f;
        float fullBtnH = 22.0f;
        float fullBtnX = cx + cw - 10.0f - fullBtnW;
        float fullBtnY = cy + 7.0f;

        float detachBtnW = 28.0f;
        float detachBtnH = 22.0f;
        float detachBtnX = fullBtnX - 5.0f - detachBtnW;
        float detachBtnY = cy + 7.0f;

        float presetBtnW = 68.0f;
        float presetBtnH = 22.0f;
        float presetBtnX = detachBtnX - 5.0f - presetBtnW;
        float presetBtnY = cy + 7.0f;

        float designBtnW = 24.0f;
        float designBtnH = 22.0f;
        float designBtnX = presetBtnX - 5.0f - designBtnW;
        float designBtnY = cy + 7.0f;

        // Detached Window button
        if (mx >= detachBtnX && mx <= detachBtnX + detachBtnW && my >= detachBtnY && my <= detachBtnY + detachBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::DetachInstrumentWindow;
            return res;
        }

        // Fullscreen button
        if (mx >= fullBtnX && mx <= fullBtnX + fullBtnW && my >= fullBtnY && my <= fullBtnY + fullBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::FullscreenInstrument;
            return res;
        }

        // Preset button
        if (mx >= presetBtnX && mx <= presetBtnX + presetBtnW && my >= presetBtnY && my <= presetBtnY + presetBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::PresetButton;
            return res;
        }

        // Design button
        if (mx >= designBtnX && mx <= designBtnX + designBtnW && my >= designBtnY && my <= designBtnY + designBtnH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::DesignButton;
            return res;
        }

        // Toggle Instrument Collapse / Expand by clicking title/chevron/dot
        if (mx >= cx && mx <= designBtnX - 4.0f && my >= cy && my <= cy + 34.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::ToggleInstrumentExpand;
            return res;
        }

        if (data.instrumentExpanded) {
            // Change instrument button below GUI
            float guiX = cx + 8.0f;
            float guiY = cy + 34.0f;
            float guiW = cw - 16.0f;
            float guiH = faceH - 34.0f - 38.0f;

            float chgBtnW = 145.0f;
            float chgBtnH = 24.0f;
            float chgBtnX = cx + cw - 8.0f - chgBtnW;
            float chgBtnY = guiY + guiH + 7.0f;

            if (data.hasNativeGui) {
                float openGuiBtnW = 150.0f;
                float openGuiBtnH = 24.0f;
                float openGuiBtnX = chgBtnX - 8.0f - openGuiBtnW;
                float openGuiBtnY = chgBtnY;
                if (mx >= openGuiBtnX && mx <= openGuiBtnX + openGuiBtnW && my >= openGuiBtnY && my <= openGuiBtnY + openGuiBtnH) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::OpenPluginGui;
                    return res;
                }
            }

            if (mx >= chgBtnX && mx <= chgBtnX + chgBtnW && my >= chgBtnY && my <= chgBtnY + chgBtnH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ChangeInstrument;
                return res;
            }

            bool isDetached = data.isDetached || FloatingWindowManager::instance().isDetached(data.trackIndex);
            if (isDetached) {
                if (focusDetachedBtnBounds_.contains(mx, my)) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::FocusDetachedWindow;
                    return res;
                }
                if (reattachDetachedBtnBounds_.contains(mx, my)) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::ReattachDetachedWindow;
                    return res;
                }
                return res;
            }

            // Knobs - first test guiPanel_ widget bounds if populated
            for (size_t rIdx = 0; rIdx < guiPanel_.rows.size(); ++rIdx) {
                const auto& row = guiPanel_.rows[rIdx];
                for (size_t wIdx = 0; wIdx < row.widgets.size(); ++wIdx) {
                    const auto& w = row.widgets[wIdx];
                    if (w.type == GuiWidgetType::Divider) continue;
                    if (w.bounds.contains(mx, my)) {
                        res.hit = true;
                        res.area = TrackPropertiesHitArea::InstrumentKnob;
                        res.index = static_cast<int>(rIdx * 100 + wIdx);
                        res.normVal = w.currentVal;
                        return res;
                    }
                }
            }

            size_t knobCount = std::min(data.knobs.size(), static_cast<size_t>(6));
            if (isWide) {
                float knobAreaW = guiW - 210.0f;
                float kStep = knobAreaW / static_cast<float>(std::max(size_t{1}, knobCount));
                for (size_t k = 0; k < knobCount; ++k) {
                    float kcx = guiX + static_cast<float>(k) * kStep + (kStep * 0.5f);
                    float kcy = guiY + guiH * 0.5f - 8.0f;
                    if (std::hypot(mx - kcx, my - kcy) <= 22.0f) {
                        res.hit = true;
                        res.area = TrackPropertiesHitArea::InstrumentKnob;
                        res.index = static_cast<int>(k);
                        return res;
                    }
                }
            } else {
                float colW = guiW / 3.0f;
                for (size_t k = 0; k < knobCount; ++k) {
                    int row = static_cast<int>(k / 3);
                    int col = static_cast<int>(k % 3);
                    float kcx = guiX + static_cast<float>(col) * colW + (colW * 0.5f);
                    float kcy = guiY + 28.0f + static_cast<float>(row) * 64.0f;
                    if (std::hypot(mx - kcx, my - kcy) <= 20.0f) {
                        res.hit = true;
                        res.area = TrackPropertiesHitArea::InstrumentKnob;
                        res.index = static_cast<int>(k);
                        return res;
                    }
                }
            }
        }
    }

    // Chord Follow Hits
    if (chordFollowBounds_.contains(mx, my)) {
        float cx = chordFollowBounds_.x;
        float cy = chordFollowBounds_.y;
        float cw = chordFollowBounds_.w;
        float chipW = (cw - 170.0f) / 5.0f;
        float chipH = 24.0f;
        float chipY = cy + 32.0f;

        for (int i = 0; i < 5; ++i) {
            float chipX = cx + 12.0f + static_cast<float>(i) * (chipW + 5.0f);
            if (mx >= chipX && mx <= chipX + chipW && my >= chipY && my <= chipY + chipH) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ChordFollowChip;
                res.index = i;
                return res;
            }
        }

        float bakeW = 100.0f;
        float bakeX = cx + cw - bakeW - 12.0f;
        if (mx >= bakeX && mx <= bakeX + bakeW && my >= chipY && my <= chipY + chipH) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::BakeChords;
            return res;
        }
    }

    // MIDI FX Hits
    float curCardY = chordFollowBounds_.y + chordFollowBounds_.h + 8.0f;
    float mfxH = computeMidiFxRackHeight(data);
    const_cast<TrackPropertiesPanel*>(this)->midiFxBounds_ = Rect2D(bounds_.x + 8.0f, curCardY, bounds_.w - 16.0f, mfxH);
    curCardY += mfxH + 8.0f;

    float afxH = computeAudioFxRackHeight(data);
    const_cast<TrackPropertiesPanel*>(this)->audioFxBounds_ = Rect2D(bounds_.x + 8.0f, curCardY, bounds_.w - 16.0f, afxH);

    if (midiFxBounds_.contains(mx, my)) {
        float cx = midiFxBounds_.x;
        float cy = midiFxBounds_.y;
        float cw = midiFxBounds_.w;
        float addBtnW = 95.0f;
        float addBtnX = cx + cw - addBtnW - 10.0f;
        if (mx >= addBtnX && mx <= addBtnX + addBtnW && my >= cy + 6.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::AddMidiFx;
            return res;
        }

        float itemY = cy + 38.0f;
        float itemW = cw - 16.0f;
        float itemX = cx + 8.0f;
        for (size_t mi = 0; mi < data.midiFx.size(); ++mi) {
            const auto& fx = data.midiFx[mi];
            float itemH = fx.isExpanded ? 180.0f : 42.0f;

            // Power switch toggle
            if (mx >= itemX + 6.0f && mx <= itemX + 30.0f && my >= itemY + 4.0f && my <= itemY + 36.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Move Up (^)
            if (mx >= itemX + itemW - 74.0f && mx <= itemX + itemW - 54.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveMidiFxUp;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Move Down (v)
            if (mx >= itemX + itemW - 53.0f && mx <= itemX + itemW - 34.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveMidiFxDown;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Delete trash can
            if (mx >= itemX + itemW - 32.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RemoveMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Fullscreen in header
            if (mx >= itemX + itemW - 102.0f && mx <= itemX + itemW - 74.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FullscreenMidiFx;
                res.index = static_cast<int>(mi);
                return res;
            }

            // Title or Chevron (toggle expand/collapse)
            if (mx >= itemX + 30.0f && mx <= itemX + itemW - 105.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleMidiFxExpand;
                res.index = static_cast<int>(mi);
                return res;
            }

            if (fx.isExpanded) {
                // Fullscreen button
                if (mx >= itemX + itemW - 70.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 34.0f && my <= itemY + 58.0f) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::FullscreenMidiFx;
                    res.index = static_cast<int>(mi);
                    return res;
                }

                // Inset Knobs / Faceplate
                float fpX = itemX + 10.0f;
                float fpY = itemY + 60.0f;
                float fpW = itemW - 20.0f;
                float fpH = 108.0f;
                if (fx.customGuiPanel.has_value() && !fx.customGuiPanel->rows.empty()) {
                    if (mx >= fpX && mx <= fpX + fpW && my >= fpY && my <= fpY + fpH) {
                        size_t rowCount = fx.customGuiPanel->rows.size();
                        float rowH = fpH / static_cast<float>(rowCount);
                        int rIdx = std::clamp(static_cast<int>((my - fpY) / rowH), 0, static_cast<int>(rowCount - 1));
                        const auto& row = fx.customGuiPanel->rows[rIdx];
                        if (!row.widgets.empty()) {
                            float colW = fpW / static_cast<float>(row.widgets.size());
                            int wIdx = std::clamp(static_cast<int>((mx - fpX) / colW), 0, static_cast<int>(row.widgets.size() - 1));
                            const auto& w = row.widgets[wIdx];
                            int matchedKIdx = -1;
                            for (size_t ki = 0; ki < fx.knobs.size(); ++ki) {
                                if (stringEqualsIgnoreCase(w.id, fx.knobs[ki].name) ||
                                    stringEqualsIgnoreCase(w.label, fx.knobs[ki].name) ||
                                    stringEqualsIgnoreCase(w.label, fx.knobs[ki].label) ||
                                    stringEqualsIgnoreCase(w.param, fx.knobs[ki].name)) {
                                    matchedKIdx = static_cast<int>(ki);
                                    break;
                                }
                            }
                            if (matchedKIdx >= 0) {
                                res.hit = true;
                                res.area = TrackPropertiesHitArea::MidiFxKnob;
                                res.index = static_cast<int>(mi * 10 + matchedKIdx);
                                res.normVal = fx.knobs[matchedKIdx].value;
                                return res;
                            }
                        }
                    }
                } else if (mx >= fpX && mx <= fpX + fpW && my >= fpY + 10.0f && my <= fpY + 98.0f) {
                    size_t knobCount = std::min(fx.knobs.size(), size_t{4});
                    if (knobCount > 0) {
                        float colW = fpW / static_cast<float>(knobCount);
                        int kIdx = static_cast<int>((mx - fpX) / colW);
                        if (kIdx >= 0 && kIdx < static_cast<int>(knobCount)) {
                            res.hit = true;
                            res.area = TrackPropertiesHitArea::MidiFxKnob;
                            res.index = static_cast<int>(mi * 10 + kIdx);
                            res.normVal = fx.knobs[kIdx].value;
                            return res;
                        }
                    }
                }
            }

            itemY += itemH + 8.0f;
        }
    }

    // Audio FX Hits
    if (audioFxBounds_.contains(mx, my)) {
        float cx = audioFxBounds_.x;
        float cy = audioFxBounds_.y;
        float cw = audioFxBounds_.w;
        float addBtnW = 85.0f;
        float addBtnX = cx + cw - addBtnW - 10.0f;
        if (mx >= addBtnX && mx <= addBtnX + addBtnW && my >= cy + 6.0f && my <= cy + 30.0f) {
            res.hit = true;
            res.area = TrackPropertiesHitArea::AddAudioFx;
            return res;
        }

        float itemY = cy + 38.0f;
        float itemW = cw - 16.0f;
        float itemX = cx + 8.0f;
        for (size_t fi = 0; fi < data.audioFx.size(); ++fi) {
            const auto& fx = data.audioFx[fi];
            float itemH = fx.isExpanded ? 180.0f : 42.0f;

            // Power switch toggle
            if (mx >= itemX + 6.0f && mx <= itemX + 30.0f && my >= itemY + 4.0f && my <= itemY + 36.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Move Up (^)
            if (mx >= itemX + itemW - 74.0f && mx <= itemX + itemW - 54.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveAudioFxUp;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Move Down (v)
            if (mx >= itemX + itemW - 53.0f && mx <= itemX + itemW - 34.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::MoveAudioFxDown;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Delete trash can
            if (mx >= itemX + itemW - 32.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::RemoveAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Fullscreen in header
            if (mx >= itemX + itemW - 102.0f && mx <= itemX + itemW - 74.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::FullscreenAudioFx;
                res.index = static_cast<int>(fi);
                return res;
            }

            // Title or Chevron (toggle expand/collapse)
            if (mx >= itemX + 30.0f && mx <= itemX + itemW - 105.0f && my >= itemY + 4.0f && my <= itemY + 34.0f) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::ToggleAudioFxExpand;
                res.index = static_cast<int>(fi);
                return res;
            }

            if (fx.isExpanded) {
                // Fullscreen button
                if (mx >= itemX + itemW - 70.0f && mx <= itemX + itemW - 6.0f && my >= itemY + 34.0f && my <= itemY + 58.0f) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::FullscreenAudioFx;
                    res.index = static_cast<int>(fi);
                    return res;
                }

                // Inset Knobs / Faceplate
                float fpX = itemX + 10.0f;
                float fpY = itemY + 60.0f;
                float fpW = itemW - 20.0f;
                float fpH = 108.0f;
                if (fx.customGuiPanel.has_value() && !fx.customGuiPanel->rows.empty()) {
                    if (mx >= fpX && mx <= fpX + fpW && my >= fpY && my <= fpY + fpH) {
                        size_t rowCount = fx.customGuiPanel->rows.size();
                        float rowH = fpH / static_cast<float>(rowCount);
                        int rIdx = std::clamp(static_cast<int>((my - fpY) / rowH), 0, static_cast<int>(rowCount - 1));
                        const auto& row = fx.customGuiPanel->rows[rIdx];
                        if (!row.widgets.empty()) {
                            float colW = fpW / static_cast<float>(row.widgets.size());
                            int wIdx = std::clamp(static_cast<int>((mx - fpX) / colW), 0, static_cast<int>(row.widgets.size() - 1));
                            const auto& w = row.widgets[wIdx];
                            int matchedKIdx = -1;
                            for (size_t ki = 0; ki < fx.knobs.size(); ++ki) {
                                if (stringEqualsIgnoreCase(w.id, fx.knobs[ki].name) ||
                                    stringEqualsIgnoreCase(w.label, fx.knobs[ki].name) ||
                                    stringEqualsIgnoreCase(w.label, fx.knobs[ki].label) ||
                                    stringEqualsIgnoreCase(w.param, fx.knobs[ki].name)) {
                                    matchedKIdx = static_cast<int>(ki);
                                    break;
                                }
                            }
                            if (matchedKIdx >= 0) {
                                res.hit = true;
                                res.area = TrackPropertiesHitArea::AudioFxKnob;
                                res.index = static_cast<int>(fi * 10 + matchedKIdx);
                                res.normVal = fx.knobs[matchedKIdx].value;
                                return res;
                            }
                        }
                    }
                } else if (mx >= fpX && mx <= fpX + fpW && my >= fpY + 10.0f && my <= fpY + 98.0f) {
                    size_t knobCount = std::min(fx.knobs.size(), size_t{4});
                    if (knobCount > 0) {
                        float colW = fpW / static_cast<float>(knobCount);
                        int kIdx = static_cast<int>((mx - fpX) / colW);
                        if (kIdx >= 0 && kIdx < static_cast<int>(knobCount)) {
                            res.hit = true;
                            res.area = TrackPropertiesHitArea::AudioFxKnob;
                            res.index = static_cast<int>(fi * 10 + kIdx);
                            res.normVal = fx.knobs[kIdx].value;
                            return res;
                        }
                    }
                }
            }

            itemY += itemH + 8.0f;
        }
    }

    // Track Actions Hits (+ Add Clip, Duplicate, Delete Track)
    if (trackActionsBounds_.contains(mx, my)) {
        float cx = trackActionsBounds_.x;
        float cy = trackActionsBounds_.y;
        float cw = trackActionsBounds_.w;
        float btnY = cy + 26.0f;
        float btnH = 30.0f;
        float gap = 6.0f;
        float btnW = (cw - 20.0f - (gap * 2.0f)) / 3.0f;
        float b1X = cx + 10.0f;
        float b2X = b1X + btnW + gap;
        float b3X = b2X + btnW + gap;

        if (my >= btnY && my <= btnY + btnH) {
            if (mx >= b1X && mx <= b1X + btnW) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::AddClipButton;
                return res;
            }
            if (mx >= b2X && mx <= b2X + btnW) {
                res.hit = true;
                res.area = TrackPropertiesHitArea::DuplicateTrackButton;
                return res;
            }
            if (mx >= b3X && mx <= b3X + btnW) {
                if (data.totalTracks > 1) {
                    res.hit = true;
                    res.area = TrackPropertiesHitArea::DeleteTrackButton;
                }
                return res;
            }
        }
    }

    return res;
}

bool TrackPropertiesPanel::executeHitAction(const TrackPropertiesHitResult& hit,
                                             TrackPropertiesDrawerData& data,
                                             const ViewContext& ctx) {
    if (showTrackRibbon_ && hit.area == TrackPropertiesHitArea::None) {
        if (onTrackSelected) onTrackSelected(static_cast<uint32_t>(hit.index));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::TrackIcon) {
        if (onChooseTrackIcon) onChooseTrackIcon(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::RenameButton) {
        if (ctx.onOpenValueEdit) {
            ValueEditRequest req;
            req.title = "EDIT TRACK PROPERTIES";
            req.paramName = "Track Name";
            req.isTextMode = true;
            req.initialText = data.trackName;
            req.accentColor = Color(data.r, data.g, data.b, 1.0f);
            req.onCommitText = [this, &data, trackIdx = data.trackIndex](const std::string& newName) {
                if (!newName.empty()) {
                    data.trackName = newName;
                    if (trackIdx < data.allTrackNames.size()) {
                        data.allTrackNames[trackIdx] = newName;
                    }
                    if (onTrackRenameWithText) {
                        onTrackRenameWithText(trackIdx, newName);
                    }
                }
            };
            ctx.onOpenValueEdit(req);
        }
        if (onTrackRename) onTrackRename(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ColorSwatch) {
        if (hit.index >= 0 && hit.index < 6) {
            data.r = kQuickPalette[hit.index][0];
            data.g = kQuickPalette[hit.index][1];
            data.b = kQuickPalette[hit.index][2];
            if (onColorChanged) onColorChanged(data.trackIndex, data.r, data.g, data.b);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::CustomColorPicker) {
        colorPickerDialog_.open(Color(data.r, data.g, data.b), "SELECT TRACK COLOR", data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqToggle) {
        data.eqEnabled = !data.eqEnabled;
        if (onParamChanged) onParamChanged(data.trackIndex, "eqEnabled", data.eqEnabled ? 1.0f : 0.0f);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::CodeButton) {
        if (onOpenCodeEditor) onOpenCodeEditor(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MuteButton) {
        data.mute = !data.mute;
        if (onMuteToggled) onMuteToggled(data.trackIndex, data.mute);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::SoloButton) {
        data.solo = !data.solo;
        if (onSoloToggled) onSoloToggled(data.trackIndex, data.solo);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FreezeButton) {
        data.freeze = !data.freeze;
        if (onFreezeToggled) onFreezeToggled(data.trackIndex, data.freeze);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AddClipButton) {
        if (onAddClip) onAddClip(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::DuplicateTrackButton) {
        if (onDuplicateTrack) onDuplicateTrack(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::DeleteTrackButton) {
        if (data.totalTracks > 1 && onDeleteTrack) {
            onDeleteTrack(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ClipDuplicate) {
        if (onDuplicateClip) onDuplicateClip(data.trackIndex, data.selectedClipIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ClipDelete) {
        if (onDeleteClip) onDeleteClip(data.trackIndex, data.selectedClipIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ClipEditInPianoRoll) {
        if (ctx.onJumpToClipEdit && data.selectedClipIndex >= 0) {
            ctx.onJumpToClipEdit(data.trackIndex, data.selectedClipIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ClipLoopToggle) {
        data.clipLooped = !data.clipLooped;
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::DesignButton) {
        if (onOpenDesign) {
            onOpenDesign(data.trackIndex);
        } else if (ctx.onNavigateTab) {
            ctx.onNavigateTab(WorkspaceView::Design);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::PresetButton) {
        pluginDialog_.open(PluginDialogMode::SelectPreset, data.trackName, data.trackIndex);
        if (onOpenPresets) onOpenPresets(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::InstrumentPrevPreset) {
        if (onPrevPreset) onPrevPreset();
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::InstrumentNextPreset) {
        if (onNextPreset) onNextPreset();
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::OpenPluginGui) {
        if (onOpenFullscreenDevice) {
            onOpenFullscreenDevice(data.trackIndex);
        } else if (onOpenPluginGui) {
            onOpenPluginGui(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FocusDetachedWindow) {
        if (onFocusDetachedWindow) {
            onFocusDetachedWindow(data.trackIndex);
        } else {
            FloatingWindowManager::instance().focusDetachedInstrument(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ReattachDetachedWindow) {
        if (onReattachDetachedWindow) {
            onReattachDetachedWindow(data.trackIndex);
        } else {
            FloatingWindowManager::instance().closeDetachedInstrument(data.trackIndex);
        }
        data.isDetached = false;
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::DetachInstrumentWindow) {
        if (onOpenDetachedWindow) onOpenDetachedWindow(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FullscreenInstrument) {
        if (onOpenFullscreenDevice) onOpenFullscreenDevice(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleInstrumentExpand) {
        data.instrumentExpanded = !data.instrumentExpanded;
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FullscreenMidiFx) {
        if (onOpenFullscreenMidiFx) onOpenFullscreenMidiFx(data.trackIndex, static_cast<size_t>(hit.index));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::FullscreenAudioFx) {
        if (onOpenFullscreenAudioFx) onOpenFullscreenAudioFx(data.trackIndex, static_cast<size_t>(hit.index));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ChangeInstrument) {
        pluginDialog_.open(PluginDialogMode::AddInstrument, data.trackName, data.trackIndex);
        if (onChangeInstrument) onChangeInstrument(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleAudioFxExpand) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            data.audioFx[hit.index].isExpanded = !data.audioFx[hit.index].isExpanded;
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleMidiFxExpand) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            data.midiFx[hit.index].isExpanded = !data.midiFx[hit.index].isExpanded;
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveAudioFxUp) {
        if (hit.index > 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx - 1;
            std::swap(data.audioFx[fromIdx], data.audioFx[toIdx]);
            if (onReorderAudioFx) onReorderAudioFx(data.trackIndex, fromIdx, toIdx);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveAudioFxDown) {
        if (hit.index >= 0 && hit.index + 1 < static_cast<int>(data.audioFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx + 1;
            std::swap(data.audioFx[fromIdx], data.audioFx[toIdx]);
            if (onReorderAudioFx) onReorderAudioFx(data.trackIndex, fromIdx, toIdx);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveMidiFxUp) {
        if (hit.index > 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx - 1;
            std::swap(data.midiFx[fromIdx], data.midiFx[toIdx]);
            if (onReorderMidiFx) onReorderMidiFx(data.trackIndex, fromIdx, toIdx);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MoveMidiFxDown) {
        if (hit.index >= 0 && hit.index + 1 < static_cast<int>(data.midiFx.size())) {
            size_t fromIdx = static_cast<size_t>(hit.index);
            size_t toIdx = fromIdx + 1;
            std::swap(data.midiFx[fromIdx], data.midiFx[toIdx]);
            if (onReorderMidiFx) onReorderMidiFx(data.trackIndex, fromIdx, toIdx);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ChordFollowChip) {
        ChordFollowMode modes[5] = {ChordFollowMode::Off, ChordFollowMode::Chord, ChordFollowMode::Bass, ChordFollowMode::Scale, ChordFollowMode::ColorLead};
        if (hit.index >= 0 && hit.index < 5) {
            data.chordFollowMode = modes[hit.index];
            if (onChordFollowChanged) onChordFollowChanged(data.trackIndex, data.chordFollowMode);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::BakeChords) {
        if (onBakeChords) onBakeChords(data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AddMidiFx) {
        pluginDialog_.open(PluginDialogMode::AddMidiFx, data.trackName, data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AddAudioFx) {
        pluginDialog_.open(PluginDialogMode::AddAudioFx, data.trackName, data.trackIndex);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::RemoveMidiFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            size_t fxIdx = static_cast<size_t>(hit.index);
            if (activeFxIndex_ == fxIdx) {
                activeFxIndex_ = static_cast<size_t>(-1);
                dragMode_ = DragMode::None;
            }
            if (onRemoveMidiFx) {
                onRemoveMidiFx(data.trackIndex, fxIdx);
            } else {
                data.midiFx.erase(data.midiFx.begin() + fxIdx);
                if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
            }
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::RemoveAudioFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            size_t fxIdx = static_cast<size_t>(hit.index);
            if (activeFxIndex_ == fxIdx) {
                activeFxIndex_ = static_cast<size_t>(-1);
                dragMode_ = DragMode::None;
            }
            if (onRemoveAudioFx) {
                onRemoveAudioFx(data.trackIndex, fxIdx);
            } else {
                data.audioFx.erase(data.audioFx.begin() + fxIdx);
                if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
            }
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleMidiFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.midiFx.size())) {
            data.midiFx[hit.index].enabled = !data.midiFx[hit.index].enabled;
            if (onToggleMidiFx) onToggleMidiFx(data.trackIndex, static_cast<size_t>(hit.index), data.midiFx[hit.index].enabled);
            if (onMidiFxChanged) onMidiFxChanged(data.trackIndex);
        }
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::ToggleAudioFx) {
        if (hit.index >= 0 && hit.index < static_cast<int>(data.audioFx.size())) {
            data.audioFx[hit.index].enabled = !data.audioFx[hit.index].enabled;
            if (onToggleAudioFx) onToggleAudioFx(data.trackIndex, static_cast<size_t>(hit.index), data.audioFx[hit.index].enabled);
            if (onAudioFxChanged) onAudioFxChanged(data.trackIndex);
        }
        return true;
    }

    return false;
}

bool TrackPropertiesPanel::openValueEditForHit(const TrackPropertiesHitResult& hit, TrackPropertiesDrawerData& data,
                                               const std::function<void(const ValueEditRequest&)>& onOpenValueEdit) {
    if (!onOpenValueEdit || !hit.hit) return false;

    if (hit.area == TrackPropertiesHitArea::VolumeSlider) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::Volume, data.volume, data.trackName,
            [this, &data](float val) {
                data.volume = val;
                if (onVolumeChanged) onVolumeChanged(data.trackIndex, val);
            });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::PanKnob) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::Pan, data.pan, data.trackName,
            [this, &data](float val) {
                data.pan = val;
                if (onPanChanged) onPanChanged(data.trackIndex, val);
            });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }


    if (hit.area == TrackPropertiesHitArea::InstrumentKnob) {
        int rIdx = hit.index / 100;
        int wIdx = hit.index % 100;
        std::string pName = "Param";
        std::string lName = "Param";
        float cVal = 0.5f;
        if (rIdx < static_cast<int>(guiPanel_.rows.size()) &&
            wIdx < static_cast<int>(guiPanel_.rows[rIdx].widgets.size())) {
            const auto& w = guiPanel_.rows[rIdx].widgets[wIdx];
            if (w.type == GuiWidgetType::Divider) return false;
            pName = w.param.empty() ? w.label : w.param;
            lName = w.label.empty() ? pName : w.label;
            cVal = w.currentVal;
        } else if (hit.index < static_cast<int>(data.knobs.size())) {
            pName = data.knobs[hit.index].name;
            lName = data.knobs[hit.index].label.empty() ? pName : data.knobs[hit.index].label;
            cVal = data.knobs[hit.index].value;
        }
        ValueEditRequest req;
        req.title = data.trackName + " • " + lName;
        req.paramName = lName;
        req.currentValue = cVal;
        req.minValue = 0.0f;
        req.maxValue = 1.0f;
        req.defaultValue = 0.5f;
        req.hasDefault = true;
        req.allowPercentage = true;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [this, &data, pName, rIdx, wIdx](float val) {
            if (rIdx < static_cast<int>(guiPanel_.rows.size()) &&
                wIdx < static_cast<int>(guiPanel_.rows[rIdx].widgets.size())) {
                guiPanel_.rows[rIdx].widgets[wIdx].currentVal = val;
            }
            for (auto& k : data.knobs) {
                if (stringEqualsIgnoreCase(k.name, pName)) {
                    k.value = val;
                    k.display = std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%";
                    break;
                }
            }
            if (onParamChanged) {
                onParamChanged(data.trackIndex, pName, val);
            }
        };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::AudioFxKnob) {
        size_t fxIdx = static_cast<size_t>(hit.index / 10);
        size_t kIdx = static_cast<size_t>(hit.index % 10);
        if (fxIdx < data.audioFx.size() && kIdx < data.audioFx[fxIdx].knobs.size()) {
            auto& fx = data.audioFx[fxIdx];
            auto& k = fx.knobs[kIdx];
            ValueEditRequest req;
            req.title = fx.name + " • " + (k.label.empty() ? k.name : k.label);
            req.paramName = k.name;
            req.currentValue = k.value;
            req.minValue = 0.0f;
            req.maxValue = 1.0f;
            req.defaultValue = 0.5f;
            req.hasDefault = true;
            req.allowPercentage = true;
            req.accentColor = Color(0.2f, 0.8f, 1.0f, 1.0f);
            req.onCommit = [this, &data, fxIdx, kIdx, pName = k.name](float val) {
                if (fxIdx < data.audioFx.size() && kIdx < data.audioFx[fxIdx].knobs.size()) {
                    data.audioFx[fxIdx].knobs[kIdx].value = val;
                    data.audioFx[fxIdx].knobs[kIdx].display = std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%";
                    if (pName == "drive" || pName == "Drive" || pName == "time" || pName == "decay") {
                        data.audioFx[fxIdx].drive = val;
                    } else if (pName == "mix" || pName == "Mix" || pName == "gain") {
                        data.audioFx[fxIdx].mix = val;
                    }
                }
                if (onAudioFxParamChanged) {
                    onAudioFxParamChanged(data.trackIndex, fxIdx, pName, val);
                }
            };
            onOpenValueEdit(req);
            return true;
        }
    }

    if (hit.area == TrackPropertiesHitArea::MidiFxKnob) {
        size_t fxIdx = static_cast<size_t>(hit.index / 10);
        size_t kIdx = static_cast<size_t>(hit.index % 10);
        if (fxIdx < data.midiFx.size() && kIdx < data.midiFx[fxIdx].knobs.size()) {
            auto& fx = data.midiFx[fxIdx];
            auto& k = fx.knobs[kIdx];
            ValueEditRequest req;
            req.title = fx.name + " • " + (k.label.empty() ? k.name : k.label);
            req.paramName = k.name;
            req.currentValue = k.value;
            req.minValue = 0.0f;
            req.maxValue = 1.0f;
            req.defaultValue = 0.5f;
            req.hasDefault = true;
            req.allowPercentage = true;
            req.accentColor = Color(0.9f, 0.7f, 0.2f, 1.0f);
            req.onCommit = [this, &data, fxIdx, kIdx, pName = k.name](float val) {
                if (fxIdx < data.midiFx.size() && kIdx < data.midiFx[fxIdx].knobs.size()) {
                    data.midiFx[fxIdx].knobs[kIdx].value = val;
                    data.midiFx[fxIdx].knobs[kIdx].display = std::to_string(static_cast<int>(std::round(val * 100.0f))) + "%";
                }
                if (onMidiFxParamChanged) {
                    onMidiFxParamChanged(data.trackIndex, pName, val);
                }
            };
            onOpenValueEdit(req);
            return true;
        }
    }

    if (hit.area == TrackPropertiesHitArea::EqHpf) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::EqHpf, data.eqHpf, data.trackName,
            [&data](float val) { data.eqHpf = val; });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqLowGain) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::EqLowGain, data.eqLowGain, data.trackName,
            [&data](float val) { data.eqLowGain = val; });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqHighGain) {
        auto cfg = presenter::TrackPropertiesPresenter::makeEditConfig(
            presenter::TrackParamType::EqHighGain, data.eqHighGain, data.trackName,
            [&data](float val) { data.eqHighGain = val; });
        onOpenValueEdit(toValueEditRequest(cfg, Color(data.r, data.g, data.b, 1.0f)));
        return true;
    }


    if (hit.area == TrackPropertiesHitArea::EqMidFreq) {
        ValueEditRequest req;
        req.title = data.trackName + " • Mid Freq";
        req.paramName = "Mid Freq";
        req.currentValue = data.eqMidFreq;
        req.minValue = 200.0f;
        req.maxValue = 8000.0f;
        req.defaultValue = 1000.0f;
        req.unit = "Hz";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [&data](float val) { data.eqMidFreq = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqMidGain) {
        ValueEditRequest req;
        req.title = data.trackName + " • Mid Gain";
        req.paramName = "Mid Gain";
        req.currentValue = data.eqMidGain;
        req.minValue = -18.0f;
        req.maxValue = 18.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [&data](float val) { data.eqMidGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::EqMidQ) {
        ValueEditRequest req;
        req.title = data.trackName + " • Mid Q";
        req.paramName = "Mid Q";
        req.currentValue = data.eqMidQ;
        req.minValue = 0.3f;
        req.maxValue = 10.0f;
        req.defaultValue = 1.0f;
        req.hasDefault = true;
        req.allowPercentage = false;
        req.accentColor = Color(data.r, data.g, data.b, 1.0f);
        req.onCommit = [&data](float val) { data.eqMidQ = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqSubCut) {
        ValueEditRequest req;
        req.title = "MASTER SUB CUT";
        req.paramName = "Sub Cut";
        req.currentValue = data.masterSubCut;
        req.minValue = 20.0f;
        req.maxValue = 45.0f;
        req.defaultValue = 25.0f;
        req.unit = "Hz";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterSubCut = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqLow) {
        ValueEditRequest req;
        req.title = "MASTER LOW GAIN";
        req.paramName = "Master Low Gain";
        req.currentValue = data.masterLowGain;
        req.minValue = -12.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterLowGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqMid) {
        ValueEditRequest req;
        req.title = "MASTER MID GAIN";
        req.paramName = "Master Mid Gain";
        req.currentValue = data.masterMidGain;
        req.minValue = -12.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterMidGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterEqHigh) {
        ValueEditRequest req;
        req.title = "MASTER HIGH GAIN";
        req.paramName = "Master High Gain";
        req.currentValue = data.masterHighGain;
        req.minValue = -12.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterHighGain = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterCeiling) {
        ValueEditRequest req;
        req.title = "MASTER LIMITER CEILING";
        req.paramName = "Ceiling";
        req.currentValue = data.masterCeilingDbfs;
        req.minValue = -2.0f;
        req.maxValue = 0.0f;
        req.defaultValue = -0.3f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterCeilingDbfs = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterDrive) {
        ValueEditRequest req;
        req.title = "MASTER LIMITER DRIVE";
        req.paramName = "Drive";
        req.currentValue = data.masterLimiterDrive;
        req.minValue = 0.0f;
        req.maxValue = 12.0f;
        req.defaultValue = 0.0f;
        req.unit = "dB";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterLimiterDrive = val; };
        onOpenValueEdit(req);
        return true;
    }

    if (hit.area == TrackPropertiesHitArea::MasterLufs) {
        ValueEditRequest req;
        req.title = "MASTER TARGET LUFS";
        req.paramName = "Target LUFS";
        req.currentValue = data.masterTargetLufs;
        req.minValue = -24.0f;
        req.maxValue = -6.0f;
        req.defaultValue = -14.0f;
        req.unit = "LUFS";
        req.hasDefault = true;
        req.allowPercentage = false;
        req.onCommit = [&data](float val) { data.masterTargetLufs = val; };
        onOpenValueEdit(req);
        return true;
    }

    return false;
}

void TrackPropertiesPanel::selectListBoxOption(GuiWidgetDef& w, int newIdx, TrackPropertiesDrawerData& data) {
    int totalOpts = static_cast<int>(w.options.size());
    if (totalOpts <= 0) return;
    newIdx = std::clamp(newIdx, 0, totalOpts - 1);
    w.currentVal = static_cast<float>(newIdx);
    focusedWidgetId_ = w.id;

    // Scroll offset tracking
    int visibleRows = 4;
    if (static_cast<float>(newIdx) < w.scrollOffset) {
        w.scrollOffset = static_cast<float>(newIdx);
    } else if (static_cast<float>(newIdx) >= w.scrollOffset + static_cast<float>(visibleRows)) {
        w.scrollOffset = static_cast<float>(newIdx - visibleRows + 1);
    }
    w.scrollOffset = std::clamp(w.scrollOffset, 0.0f, static_cast<float>(std::max(0, totalOpts - visibleRows)));

    float normRatio = static_cast<float>(newIdx) / std::max(1.0f, static_cast<float>(totalOpts - 1));
    for (auto& k : data.knobs) {
        if (stringEqualsIgnoreCase(k.name, w.param)) {
            k.value = normRatio;
            k.display = (newIdx < static_cast<int>(w.options.size())) ? w.options[newIdx] : std::to_string(newIdx);
            break;
        }
    }
    if (onParamChanged) {
        onParamChanged(data.trackIndex, w.param, normRatio);
    }

    auto containsSfxr = [](std::string_view s) noexcept -> bool {
        std::string lower;
        lower.reserve(s.size());
        for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        return lower.find("sfxr") != std::string::npos;
    };
    bool isSfxr = (stringEqualsIgnoreCase(data.instrumentEngine, "sfxr") ||
                   containsSfxr(data.instrument) || containsSfxr(data.presetTitle));

    if (w.id == "w_sfxtype" || stringEqualsIgnoreCase(w.param, "SFXType")) {
        uint32_t seed = 42;
        for (const auto& rw : guiPanel_.rows) {
            for (const auto& wid : rw.widgets) {
                if (wid.id == "w_seed" || stringEqualsIgnoreCase(wid.param, "Seed")) {
                    seed = static_cast<uint32_t>(std::clamp(wid.currentVal, 1.0f, 9999.0f));
                    break;
                }
            }
        }

        auto pMap = eatsbits::audio::SNESSFXRGenerator::generateParamsForType(newIdx, seed);
        auto applyParam = [&](const std::string& widId, const std::string& pName, float normVal) {
            for (auto& rw : guiPanel_.rows) {
                for (auto& wid : rw.widgets) {
                    if (wid.id == widId || stringEqualsIgnoreCase(wid.param, pName)) {
                        wid.currentVal = normVal;
                    }
                }
            }
            for (auto& k : data.knobs) {
                if (stringEqualsIgnoreCase(k.name, pName)) {
                    k.value = normVal;
                    k.display = std::to_string(static_cast<int>(std::round(normVal * 100.0f))) + "%";
                    break;
                }
            }
            if (onParamChanged) {
                onParamChanged(data.trackIndex, pName, normVal);
            }
        };

        applyParam("w_wavetable", "Waveform", pMap["Waveform"] / 11.0f);
        applyParam("w_attack", "Attack", std::clamp(pMap["Attack"] / 0.5f, 0.0f, 1.0f));
        applyParam("w_decay", "Decay", std::clamp(pMap["Decay"] / 2.0f, 0.0f, 1.0f));
        applyParam("w_sustain", "Sustain", std::clamp(pMap["Sustain"], 0.0f, 1.0f));
        applyParam("w_release", "Release", std::clamp(pMap["Release"] / 2.0f, 0.0f, 1.0f));
        applyParam("w_sweep", "PitchSweep", std::clamp((pMap["PitchSweep"] + 2.0f) / 4.0f, 0.0f, 1.0f));
        applyParam("w_vibrate", "VibratoRate", std::clamp(pMap["VibratoRate"] / 20.0f, 0.0f, 1.0f));
        applyParam("w_vibdepth", "VibratoDepth", std::clamp(pMap["VibratoDepth"], 0.0f, 1.0f));
        applyParam("w_echoms", "EchoDelay", std::clamp(pMap["EchoDelay"] / 480.0f, 0.0f, 1.0f));
        applyParam("w_echovol", "EchoVolume", std::clamp(pMap["EchoVolume"], 0.0f, 1.0f));

        if (onAuditionNote) {
            onAuditionNote(data.trackIndex, 72, 0.85f);
        }
    } else if (w.id == "w_wavetable" || stringEqualsIgnoreCase(w.param, "Waveform")) {
        if (onAuditionNote) {
            onAuditionNote(data.trackIndex, isSfxr ? 72 : 60, 0.85f);
        }
    }
}

bool TrackPropertiesPanel::handlePointer(const PointerEvent& ev, TrackPropertiesDrawerData& data,
                                         const ViewContext& ctx) {
    data.syncKnobsIfEmpty();

    // 0. Forward to embedded dialogs if open
    if (colorPickerDialog_.isOpen()) {
        if (colorPickerDialog_.handlePointer(ev)) return true;
        return true;
    }
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handlePointer(ev)) return true;
        return true;
    }

    // Mouse Wheel Scrolling
    if (ev.action == PointerAction::Scroll) {
        if (bounds_.w > 0.0f && bounds_.h > 0.0f && !bounds_.contains(ev.x, ev.y)) {
            return false;
        }
        // First check if mouse wheel is hovering over any ListBox widget
        for (auto& row : guiPanel_.rows) {
            for (auto& w : row.widgets) {
                if (w.type == GuiWidgetType::ListBox && w.bounds.contains(ev.x, ev.y)) {
                    int totalOpts = static_cast<int>(w.options.size());
                    int visibleRows = 4;
                    int maxScroll = std::max(0, totalOpts - visibleRows);
                    if (maxScroll > 0) {
                        int delta = (ev.scrollY > 0.0f) ? -1 : 1;
                        int currentScroll = static_cast<int>(std::round(w.scrollOffset));
                        w.scrollOffset = static_cast<float>(std::clamp(currentScroll + delta, 0, maxScroll));
                    }
                    focusedWidgetId_ = w.id;
                    return true;
                }
            }
        }

        float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
        scrollY_ = std::clamp(scrollY_ - ev.scrollY * 28.0f, 0.0f, maxScroll);
        if (onScrollChanged) onScrollChanged(scrollY_);
        return true;
    }

    // Pointer Down
    if (ev.action == PointerAction::Down) {
        scroller_.stop();

        if (bounds_.w > 0.0f && bounds_.h > 0.0f && !bounds_.contains(ev.x, ev.y)) {
            return false;
        }

        // Check Scrollbar Hit
        if (scrollbarBounds_.contains(ev.x, ev.y)) {
            dragMode_ = DragMode::Scrollbar;
            dragStartY_ = ev.y;
            dragStartScrollY_ = scrollY_;
            return true;
        }

        auto hit = hitTest(ev.x, ev.y, data);
        if (hit.hit) {
            // Universal Right-Click on any knob or slider opens manual ValueEditDialog
            if (ev.button == PointerButton::Right) {
                if (openValueEditForHit(hit, data, ctx.onOpenValueEdit)) {
                    return true;
                }
            }

            // Universal Left-Click double-click detection -> opens ValueEditDialog
            if (ev.button == PointerButton::Left) {
                auto now = std::chrono::steady_clock::now();
                auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClickTime_).count();
                float dist = std::hypot(ev.x - lastClickX_, ev.y - lastClickY_);
                if (lastClickArea_ == hit.area && elapsedMs > 0 && elapsedMs < 350 && dist < 6.0f) {
                    lastClickTime_ = {};
                    lastClickArea_ = TrackPropertiesHitArea::None;
                    if (openValueEditForHit(hit, data, ctx.onOpenValueEdit)) {
                        return true;
                    }
                } else {
                    lastClickTime_ = now;
                    lastClickX_ = ev.x;
                    lastClickY_ = ev.y;
                    lastClickArea_ = hit.area;
                }
            }

            // Ctrl / Cmd + Click immediately restores default value
            if (ev.mods.ctrl || ev.mods.meta) {
                if (hit.area == TrackPropertiesHitArea::VolumeSlider) {
                    data.volume = 1.0f;
                    if (onVolumeChanged) onVolumeChanged(data.trackIndex, 1.0f);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::PanKnob) {
                    data.pan = 0.0f;
                    if (onPanChanged) onPanChanged(data.trackIndex, 0.0f);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::EqHpf) {
                    data.eqHpf = 20.0f;
                    if (onParamChanged) onParamChanged(data.trackIndex, "eqHpf", 0.0f);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::EqLowGain) {
                    data.eqLowGain = 0.0f;
                    if (onParamChanged) onParamChanged(data.trackIndex, "eqLowGain", 0.5f);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::EqMidGain) {
                    data.eqMidGain = 0.0f;
                    if (onParamChanged) onParamChanged(data.trackIndex, "eqMidGain", 0.5f);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::EqHighGain) {
                    data.eqHighGain = 0.0f;
                    if (onParamChanged) onParamChanged(data.trackIndex, "eqHighGain", 0.5f);
                    return true;
                } else if (hit.area == TrackPropertiesHitArea::InstrumentKnob && hit.index >= 0 && hit.index < static_cast<int>(data.knobs.size())) {
                    data.knobs[hit.index].value = 0.5f;
                    data.knobs[hit.index].display = "50%";
                    if (onParamChanged) onParamChanged(data.trackIndex, data.knobs[hit.index].name, 0.5f);
                    return true;
                }
            }

            // Mobile / Touch Long-Press Detection (>= 0.45s hold triggers ValueEditDialog)
            if (ev.type == PointerType::Touch || ctx.isMobile) {
                if (hit.area == TrackPropertiesHitArea::VolumeSlider ||
                    hit.area == TrackPropertiesHitArea::PanKnob ||
                    hit.area == TrackPropertiesHitArea::InstrumentKnob ||
                    hit.area == TrackPropertiesHitArea::AudioFxKnob ||
                    hit.area == TrackPropertiesHitArea::MidiFxKnob ||
                    hit.area == TrackPropertiesHitArea::EqHpf ||
                    hit.area == TrackPropertiesHitArea::EqLowGain ||
                    hit.area == TrackPropertiesHitArea::EqHighGain ||
                    hit.area == TrackPropertiesHitArea::EqMidFreq ||
                    hit.area == TrackPropertiesHitArea::EqMidGain ||
                    hit.area == TrackPropertiesHitArea::EqMidQ ||
                    hit.area == TrackPropertiesHitArea::MasterEqSubCut ||
                    hit.area == TrackPropertiesHitArea::MasterEqLow ||
                    hit.area == TrackPropertiesHitArea::MasterEqMid ||
                    hit.area == TrackPropertiesHitArea::MasterEqHigh ||
                    hit.area == TrackPropertiesHitArea::MasterCeiling ||
                    hit.area == TrackPropertiesHitArea::MasterDrive ||
                    hit.area == TrackPropertiesHitArea::MasterLufs) {
                    isLongPressActive_ = true;
                    longPressTimer_ = 0.0f;
                    touchDownTimePoint_ = std::chrono::steady_clock::now();
                    longPressPos_ = Point2D{ev.x, ev.y};
                    longPressHit_ = hit;
                    longPressOpenValueEdit_ = ctx.onOpenValueEdit;
                }
            }

            // Check continuous drag controls:
            if (hit.area == TrackPropertiesHitArea::VolumeSlider) {
                dragMode_ = DragMode::VolumeSlider;
                data.volume = hit.normVal * 1.5f;
                if (onVolumeChanged) onVolumeChanged(data.trackIndex, data.volume);
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::PanKnob) {
                dragMode_ = DragMode::PanKnob;
                dragStartY_ = ev.y;
                dragStartVal_ = data.pan;
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::EqHpf ||
                hit.area == TrackPropertiesHitArea::EqLowGain ||
                hit.area == TrackPropertiesHitArea::EqHighGain ||
                hit.area == TrackPropertiesHitArea::EqMidFreq ||
                hit.area == TrackPropertiesHitArea::EqMidGain ||
                hit.area == TrackPropertiesHitArea::EqMidQ ||
                hit.area == TrackPropertiesHitArea::MasterEqSubCut ||
                hit.area == TrackPropertiesHitArea::MasterEqLow ||
                hit.area == TrackPropertiesHitArea::MasterEqMid ||
                hit.area == TrackPropertiesHitArea::MasterEqHigh ||
                hit.area == TrackPropertiesHitArea::MasterCeiling ||
                hit.area == TrackPropertiesHitArea::MasterDrive ||
                hit.area == TrackPropertiesHitArea::MasterLufs) {
                dragMode_ = DragMode::EqKnob;
                dragStartY_ = ev.y;
                if (hit.area == TrackPropertiesHitArea::EqHpf) {
                    activeParamName_ = "eqHpf";
                    dragStartVal_ = std::clamp((data.eqHpf - 20.0f) / 480.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::EqLowGain) {
                    activeParamName_ = "eqLowGain";
                    dragStartVal_ = std::clamp((data.eqLowGain + 18.0f) / 36.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::EqHighGain) {
                    activeParamName_ = "eqHighGain";
                    dragStartVal_ = std::clamp((data.eqHighGain + 18.0f) / 36.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::EqMidFreq) {
                    activeParamName_ = "eqMidFreq";
                    dragStartVal_ = std::clamp((data.eqMidFreq - 200.0f) / 7800.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::EqMidGain) {
                    activeParamName_ = "eqMidGain";
                    dragStartVal_ = std::clamp((data.eqMidGain + 18.0f) / 36.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::EqMidQ) {
                    activeParamName_ = "eqMidQ";
                    dragStartVal_ = std::clamp((data.eqMidQ - 0.3f) / 9.7f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterEqSubCut) {
                    activeParamName_ = "masterSubCut";
                    dragStartVal_ = std::clamp((data.masterSubCut - 20.0f) / 25.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterEqLow) {
                    activeParamName_ = "masterLow";
                    dragStartVal_ = std::clamp((data.masterLowGain + 12.0f) / 24.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterEqMid) {
                    activeParamName_ = "masterMid";
                    dragStartVal_ = std::clamp((data.masterMidGain + 12.0f) / 24.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterEqHigh) {
                    activeParamName_ = "masterHigh";
                    dragStartVal_ = std::clamp((data.masterHighGain + 12.0f) / 24.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterCeiling) {
                    activeParamName_ = "masterCeiling";
                    dragStartVal_ = std::clamp((data.masterCeilingDbfs + 2.0f) / 2.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterDrive) {
                    activeParamName_ = "masterDrive";
                    dragStartVal_ = std::clamp(data.masterLimiterDrive / 12.0f, 0.0f, 1.0f);
                } else if (hit.area == TrackPropertiesHitArea::MasterLufs) {
                    activeParamName_ = "masterLufs";
                    dragStartVal_ = std::clamp((data.masterTargetLufs + 24.0f) / 18.0f, 0.0f, 1.0f);
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::InstrumentKnob) {
                dragMode_ = DragMode::InstrumentKnob;
                activeKnobIndex_ = hit.index;
                dragStartY_ = ev.y;
                int rIdx = hit.index / 100;
                int wIdx = hit.index % 100;
                if (rIdx < static_cast<int>(guiPanel_.rows.size()) &&
                    wIdx < static_cast<int>(guiPanel_.rows[rIdx].widgets.size())) {
                    auto& w = guiPanel_.rows[rIdx].widgets[wIdx];
                    if (w.type == GuiWidgetType::ToggleSwitch) {
                        float newVal = (w.currentVal > 0.5f) ? 0.0f : 1.0f;
                        w.currentVal = newVal;
                        for (auto& k : data.knobs) {
                            if (stringEqualsIgnoreCase(k.name, w.param)) {
                                k.value = newVal;
                                if (w.options.size() == 2) {
                                    k.display = (newVal > 0.5f) ? w.options[1] : w.options[0];
                                } else {
                                    k.display = (newVal > 0.5f) ? "On" : "Off";
                                }
                                break;
                            }
                        }
                        if (onParamChanged) {
                            onParamChanged(data.trackIndex, w.param, newVal);
                        }
                        dragMode_ = DragMode::None;
                        return true;
                    }

                    if (w.type == GuiWidgetType::ActionButton || (w.type == GuiWidgetType::NixieDisplay && w.id == "w_seed")) {
                        // RANDOMIZE seed & parameters + immediate audition
                        uint32_t newSeed = static_cast<uint32_t>((std::rand() % 9990) + 10);

                        for (auto& rw : guiPanel_.rows) {
                            for (auto& wid : rw.widgets) {
                                if (wid.id == "w_seed" || stringEqualsIgnoreCase(wid.param, "Seed")) {
                                    wid.currentVal = static_cast<float>(newSeed);
                                }
                            }
                        }
                        for (auto& k : data.knobs) {
                            if (stringEqualsIgnoreCase(k.name, "Seed")) {
                                k.value = static_cast<float>(newSeed) / 9999.0f;
                                k.display = std::to_string(newSeed);
                                break;
                            }
                        }
                        if (onParamChanged) {
                            onParamChanged(data.trackIndex, "Seed", static_cast<float>(newSeed));
                        }

                        auto containsSfxr = [](std::string_view s) noexcept -> bool {
                            std::string lower;
                            lower.reserve(s.size());
                            for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                            return lower.find("sfxr") != std::string::npos;
                        };
                        bool isSfxr = (stringEqualsIgnoreCase(data.instrumentEngine, "sfxr") ||
                                       containsSfxr(data.instrument) || containsSfxr(data.presetTitle));

                        if (isSfxr) {
                            int sfxType = 0;
                            for (const auto& rw : guiPanel_.rows) {
                                for (const auto& wid : rw.widgets) {
                                    if (wid.id == "w_sfxtype" || stringEqualsIgnoreCase(wid.param, "SFXType")) {
                                        sfxType = std::clamp(static_cast<int>(std::round(wid.currentVal)), 0, 10);
                                        break;
                                    }
                                }
                            }

                            auto pMap = eatsbits::audio::SNESSFXRGenerator::generateParamsForType(sfxType, newSeed);
                            auto applyParam = [&](const std::string& widId, const std::string& pName, float normVal) {
                                for (auto& rw : guiPanel_.rows) {
                                    for (auto& wid : rw.widgets) {
                                        if (wid.id == widId || stringEqualsIgnoreCase(wid.param, pName)) {
                                            wid.currentVal = normVal;
                                        }
                                    }
                                }
                                for (auto& k : data.knobs) {
                                    if (stringEqualsIgnoreCase(k.name, pName)) {
                                        k.value = normVal;
                                        k.display = std::to_string(static_cast<int>(std::round(normVal * 100.0f))) + "%";
                                        break;
                                    }
                                }
                                if (onParamChanged) {
                                    onParamChanged(data.trackIndex, pName, normVal);
                                }
                            };

                            applyParam("w_wavetable", "Waveform", pMap["Waveform"] / 11.0f);
                            applyParam("w_attack", "Attack", std::clamp(pMap["Attack"] / 0.5f, 0.0f, 1.0f));
                            applyParam("w_decay", "Decay", std::clamp(pMap["Decay"] / 2.0f, 0.0f, 1.0f));
                            applyParam("w_sustain", "Sustain", std::clamp(pMap["Sustain"], 0.0f, 1.0f));
                            applyParam("w_release", "Release", std::clamp(pMap["Release"] / 2.0f, 0.0f, 1.0f));
                            applyParam("w_sweep", "PitchSweep", std::clamp((pMap["PitchSweep"] + 2.0f) / 4.0f, 0.0f, 1.0f));
                            applyParam("w_vibrate", "VibratoRate", std::clamp(pMap["VibratoRate"] / 20.0f, 0.0f, 1.0f));
                            applyParam("w_vibdepth", "VibratoDepth", std::clamp(pMap["VibratoDepth"], 0.0f, 1.0f));
                            applyParam("w_echoms", "EchoDelay", std::clamp(pMap["EchoDelay"] / 480.0f, 0.0f, 1.0f));
                            applyParam("w_echovol", "EchoVolume", std::clamp(pMap["EchoVolume"], 0.0f, 1.0f));

                            if (onAuditionNote) {
                                onAuditionNote(data.trackIndex, 72, 0.85f);
                            }
                        } else {
                            eatsbits::audio::DeterministicPRNG prng(newSeed);
                            int wave = prng.nextInt(0, 11);
                            float att = prng.nextRange(0.002f, 0.12f) / 0.5f;
                            float dec = prng.nextRange(0.10f, 0.80f) / 2.0f;
                            float sus = prng.nextRange(0.20f, 0.90f);
                            float rel = prng.nextRange(0.08f, 0.60f) / 2.0f;
                            float vibR = prng.nextRange(3.0f, 9.0f) / 20.0f;
                            float vibD = prng.nextRange(0.0f, 0.25f);
                            float echD = prng.nextRange(60.0f, 260.0f) / 480.0f;
                            float echF = prng.nextRange(0.20f, 0.70f);
                            float echV = prng.nextRange(0.15f, 0.55f);

                            auto applyParam = [&](const std::string& widId, const std::string& pName, float normVal) {
                                for (auto& rw : guiPanel_.rows) {
                                    for (auto& wid : rw.widgets) {
                                        if (wid.id == widId || stringEqualsIgnoreCase(wid.param, pName)) {
                                            wid.currentVal = normVal;
                                        }
                                    }
                                }
                                for (auto& k : data.knobs) {
                                    if (stringEqualsIgnoreCase(k.name, pName)) {
                                        k.value = normVal;
                                        k.display = std::to_string(static_cast<int>(std::round(normVal * 100.0f))) + "%";
                                        break;
                                    }
                                }
                                if (onParamChanged) {
                                    onParamChanged(data.trackIndex, pName, normVal);
                                }
                            };

                            applyParam("w_wavetable", "Waveform", static_cast<float>(wave) / 11.0f);
                            applyParam("w_attack", "Attack", att);
                            applyParam("w_decay", "Decay", dec);
                            applyParam("w_sustain", "Sustain", sus);
                            applyParam("w_release", "Release", rel);
                            applyParam("w_vibrate", "VibratoRate", vibR);
                            applyParam("w_vibdepth", "VibratoDepth", vibD);
                            applyParam("w_echoms", "EchoDelay", echD);
                            applyParam("w_echofdbk", "EchoFeedback", echF);
                            applyParam("w_echovol", "EchoVolume", echV);

                            if (onAuditionNote) {
                                onAuditionNote(data.trackIndex, 60, 0.85f);
                            }
                        }

                        dragMode_ = DragMode::None;
                        return true;
                    }

                    if (w.type == GuiWidgetType::ListBox) {
                        dragMode_ = DragMode::ListBoxDrag;
                        draggingRow_ = static_cast<int>(rIdx);
                        draggingWidget_ = static_cast<int>(wIdx);
                        dragStartX_ = ev.x;
                        dragStartY_ = ev.y;
                        dragStartVal_ = w.scrollOffset;
                        isListBoxDragging_ = false;
                        focusedWidgetId_ = w.id;
                        return true;
                    }
                    if (w.maxVal > w.minVal) {
                        dragStartVal_ = std::clamp((w.currentVal - w.minVal) / (w.maxVal - w.minVal), 0.0f, 1.0f);
                    } else {
                        dragStartVal_ = std::clamp(w.currentVal, 0.0f, 1.0f);
                    }
                    draggingRow_ = rIdx;
                    draggingWidget_ = wIdx;
                } else {
                    dragStartVal_ = (activeKnobIndex_ >= 0 && activeKnobIndex_ < static_cast<int>(data.knobs.size()))
                                        ? data.knobs[activeKnobIndex_].value
                                        : 0.5f;
                    draggingRow_ = -1;
                    draggingWidget_ = -1;
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::AudioFxKnob) {
                dragMode_ = DragMode::AudioFxKnob;
                activeFxIndex_ = static_cast<size_t>(hit.index / 10);
                activeKnobIndex_ = hit.index % 10;
                dragStartY_ = ev.y;
                if (activeFxIndex_ < data.audioFx.size() &&
                    static_cast<size_t>(activeKnobIndex_) < data.audioFx[activeFxIndex_].knobs.size()) {
                    dragStartVal_ = data.audioFx[activeFxIndex_].knobs[activeKnobIndex_].value;
                } else {
                    dragStartVal_ = 0.5f;
                }
                return true;
            }

            if (hit.area == TrackPropertiesHitArea::MidiFxKnob) {
                dragMode_ = DragMode::MidiFxKnob;
                activeFxIndex_ = static_cast<size_t>(hit.index / 10);
                activeKnobIndex_ = hit.index % 10;
                dragStartY_ = ev.y;
                if (activeFxIndex_ < data.midiFx.size() &&
                    static_cast<size_t>(activeKnobIndex_) < data.midiFx[activeFxIndex_].knobs.size()) {
                    dragStartVal_ = data.midiFx[activeFxIndex_].knobs[activeKnobIndex_].value;
                } else {
                    dragStartVal_ = 0.5f;
                }
                return true;
            }

            // Clickable button / chip / swatch targets:
            if (ev.type == PointerType::Touch) {
                // Touch: queue hit as pending; if user drags to scroll, cancel it
                pendingHitResult_ = hit;
                touchStartY_ = ev.y;
                touchStartX_ = ev.x;
                touchStartScrollY_ = scrollY_;
                touchDragCommitted_ = false;
                touchStartTimeMs_ = ev.timestampMs;
                scroller_.reset();
                scroller_.addSample(ev.x, ev.y, ev.timestampMs);
                return true;
            } else {
                // Desktop Mouse: crisp immediate trigger
                return executeHitAction(hit, data, ctx);
            }
        }

        // Hit empty area of panel: allow drag to scroll directly!
        if (bounds_.contains(ev.x, ev.y)) {
            pendingHitResult_ = TrackPropertiesHitResult{};
            touchStartY_ = ev.y;
            touchStartX_ = ev.x;
            touchStartScrollY_ = scrollY_;
            touchDragCommitted_ = (ev.type != PointerType::Touch);
            dragMode_ = DragMode::TouchScroll;
            scroller_.reset();
            scroller_.addSample(ev.x, ev.y, ev.timestampMs);
            return true;
        }
    }

    // Pointer Move
    if (ev.action == PointerAction::Move) {
        if (isLongPressActive_) {
            if (std::hypot(ev.x - longPressPos_.x, ev.y - longPressPos_.y) > 10.0f) {
                isLongPressActive_ = false;
            }
        }

        if (dragMode_ == DragMode::ListBoxDrag) {
            if (draggingRow_ >= 0 && draggingRow_ < static_cast<int>(guiPanel_.rows.size()) &&
                draggingWidget_ >= 0 && draggingWidget_ < static_cast<int>(guiPanel_.rows[draggingRow_].widgets.size())) {
                auto& w = guiPanel_.rows[draggingRow_].widgets[draggingWidget_];
                float dy = ev.y - dragStartY_;
                float slop = (ev.type == PointerType::Touch) ? 6.0f : 3.0f;
                if (!isListBoxDragging_ && std::abs(dy) > slop) {
                    isListBoxDragging_ = true;
                }
                if (isListBoxDragging_) {
                    int totalOpts = static_cast<int>(w.options.size());
                    int visibleRows = 4;
                    float boxH = std::max(24.0f, w.bounds.h - 16.0f);
                    float rowH = std::max(8.0f, (boxH - 4.0f) / static_cast<float>(visibleRows));
                    float scrollDelta = -dy / rowH;
                    float maxScroll = static_cast<float>(std::max(0, totalOpts - visibleRows));
                    w.scrollOffset = std::clamp(dragStartVal_ + scrollDelta, 0.0f, maxScroll);
                }
            }
            return true;
        }

        if (dragMode_ == DragMode::TouchScroll) {
            float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
            scrollY_ = std::clamp(touchStartScrollY_ - (ev.y - touchStartY_), 0.0f, maxScroll);
            scroller_.addSample(ev.x, ev.y, ev.timestampMs);
            if (onScrollChanged) onScrollChanged(scrollY_);
            return true;
        }

        if (pendingHitResult_.hit && !touchDragCommitted_) {
            float dy = std::abs(ev.y - touchStartY_);
            float slop = (ev.type == PointerType::Touch) ? 14.0f : 4.0f;
            if (dy > slop) {
                touchDragCommitted_ = true;
                dragMode_ = DragMode::TouchScroll;
                float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
                scrollY_ = std::clamp(touchStartScrollY_ - (ev.y - touchStartY_), 0.0f, maxScroll);
                scroller_.addSample(ev.x, ev.y, ev.timestampMs);
                if (onScrollChanged) onScrollChanged(scrollY_);
                return true;
            }
        }

        if (dragMode_ == DragMode::Scrollbar) {
            float maxScroll = std::max(0.0f, totalContentHeight_ - bounds_.h);
            if (maxScroll > 0.0f) {
                float dy = ev.y - dragStartY_;
                float scrollDelta = (dy / scrollbarBounds_.h) * totalContentHeight_;
                scrollY_ = std::clamp(dragStartScrollY_ + scrollDelta, 0.0f, maxScroll);
                if (onScrollChanged) onScrollChanged(scrollY_);
            }
            return true;
        }

        if (dragMode_ == DragMode::VolumeSlider) {
            float cx = mixerCardBounds_.x;
            float cw = mixerCardBounds_.w;
            bool isWide = (bounds_.w >= 560.0f);
            float vLabelX = cx + 14.0f;
            float vTrackX = vLabelX + (isWide ? 38.0f : 32.0f);
            float panAreaW = isWide ? 85.0f : 74.0f;
            float vTrackW = (cx + cw - panAreaW - 55.0f) - vTrackX;
            if (vTrackW < 50.0f) vTrackW = 50.0f;

            float norm = std::clamp((ev.x - vTrackX) / vTrackW, 0.0f, 1.0f);
            data.volume = norm * 1.5f;
            if (onVolumeChanged) onVolumeChanged(data.trackIndex, data.volume);
            return true;
        }

        if (dragMode_ == DragMode::PanKnob) {
            float dy = dragStartY_ - ev.y;
            float sens = ev.mods.shift ? 0.10f : 1.0f;
            data.pan = std::clamp(dragStartVal_ + (dy / 100.0f) * sens, -1.0f, 1.0f);
            if (onPanChanged) onPanChanged(data.trackIndex, data.pan);
            return true;
        }

        if (dragMode_ == DragMode::InstrumentKnob) {
            float dy = dragStartY_ - ev.y;
            float sens = ev.mods.shift ? 0.10f : 1.0f;
            float newVal = std::clamp(dragStartVal_ + (dy / 150.0f) * sens, 0.0f, 1.0f);
            std::string pName;
            bool isStepped = false;
            float stepVal = 0.0f;
            if (draggingRow_ >= 0 && draggingRow_ < static_cast<int>(guiPanel_.rows.size()) &&
                draggingWidget_ >= 0 && draggingWidget_ < static_cast<int>(guiPanel_.rows[draggingRow_].widgets.size())) {
                auto& w = guiPanel_.rows[draggingRow_].widgets[draggingWidget_];
                if (w.step > 0.0f && w.maxVal > w.minVal) {
                    float scaled = w.minVal + newVal * (w.maxVal - w.minVal);
                    scaled = w.minVal + std::round((scaled - w.minVal) / w.step) * w.step;
                    scaled = std::clamp(scaled, w.minVal, w.maxVal);
                    newVal = (scaled - w.minVal) / (w.maxVal - w.minVal);
                    w.currentVal = scaled;
                    isStepped = true;
                    stepVal = scaled;
                } else if (w.maxVal > w.minVal) {
                    w.currentVal = w.minVal + newVal * (w.maxVal - w.minVal);
                } else {
                    w.currentVal = newVal;
                }
                pName = w.param;
            } else if (activeKnobIndex_ >= 0 && activeKnobIndex_ < static_cast<int>(data.knobs.size())) {
                pName = data.knobs[activeKnobIndex_].name;
            }

            bool updated = false;
            for (auto& k : data.knobs) {
                if (!pName.empty() && stringEqualsIgnoreCase(k.name, pName)) {
                    k.value = newVal;
                    if (isStepped) {
                        k.display = std::to_string(static_cast<int>(std::round(stepVal)));
                    } else if (draggingRow_ >= 0 && draggingWidget_ >= 0) {
                        const auto& curW = guiPanel_.rows[draggingRow_].widgets[draggingWidget_];
                        k.display = curW.getFormattedValue();
                    } else {
                        k.display = std::to_string(static_cast<int>(std::round(newVal * 100.0f))) + "%";
                    }
                    updated = true;
                    break;
                }
            }
            if (!updated && !pName.empty()) {
                std::string disp;
                if (isStepped) {
                    disp = std::to_string(static_cast<int>(std::round(stepVal)));
                } else if (draggingRow_ >= 0 && draggingWidget_ >= 0) {
                    const auto& curW = guiPanel_.rows[draggingRow_].widgets[draggingWidget_];
                    disp = curW.getFormattedValue();
                } else {
                    disp = std::to_string(static_cast<int>(std::round(newVal * 100.0f))) + "%";
                }
                data.setKnobValue(pName, newVal, disp);
            }
            if (onParamChanged && !pName.empty()) {
                onParamChanged(data.trackIndex, pName, newVal);
            }
            return true;
        }

        if (dragMode_ == DragMode::EqKnob) {
            float dy = dragStartY_ - ev.y;
            float sens = ev.mods.shift ? 0.10f : 1.0f;
            float newVal = std::clamp(dragStartVal_ + (dy / 120.0f) * sens, 0.0f, 1.0f);
            if (activeParamName_ == "eqHpf") {
                data.eqHpf = 20.0f + newVal * 480.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "eqHpf", newVal);
            } else if (activeParamName_ == "eqLow" || activeParamName_ == "eqLowGain") {
                data.eqLowGain = -18.0f + newVal * 36.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "eqLowGain", newVal);
            } else if (activeParamName_ == "eqHigh" || activeParamName_ == "eqHighGain") {
                data.eqHighGain = -18.0f + newVal * 36.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "eqHighGain", newVal);
            } else if (activeParamName_ == "eqMidFreq") {
                data.eqMidFreq = 200.0f + newVal * 7800.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "eqMidFreq", newVal);
            } else if (activeParamName_ == "eqMidGain") {
                data.eqMidGain = -18.0f + newVal * 36.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "eqMidGain", newVal);
            } else if (activeParamName_ == "eqMidQ") {
                data.eqMidQ = 0.3f + newVal * 9.7f;
                if (onParamChanged) onParamChanged(data.trackIndex, "eqMidQ", newVal);
            } else if (activeParamName_ == "masterSubCut") {
                data.masterSubCut = 20.0f + newVal * 25.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterSubCut", newVal);
            } else if (activeParamName_ == "masterLow" || activeParamName_ == "masterLowGain") {
                data.masterLowGain = -12.0f + newVal * 24.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterLowGain", newVal);
            } else if (activeParamName_ == "masterMid" || activeParamName_ == "masterMidGain") {
                data.masterMidGain = -12.0f + newVal * 24.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterMidGain", newVal);
            } else if (activeParamName_ == "masterHigh" || activeParamName_ == "masterHighGain") {
                data.masterHighGain = -12.0f + newVal * 24.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterHighGain", newVal);
            } else if (activeParamName_ == "masterCeiling" || activeParamName_ == "masterCeilingDbfs") {
                data.masterCeilingDbfs = -2.0f + newVal * 2.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterCeilingDbfs", newVal);
            } else if (activeParamName_ == "masterDrive" || activeParamName_ == "masterLimiterDrive") {
                data.masterLimiterDrive = newVal * 12.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterLimiterDrive", newVal);
            } else if (activeParamName_ == "masterLufs" || activeParamName_ == "masterTargetLufs") {
                data.masterTargetLufs = -24.0f + newVal * 18.0f;
                if (onParamChanged) onParamChanged(data.trackIndex, "masterTargetLufs", newVal);
            }
            return true;
        }

        if (dragMode_ == DragMode::AudioFxKnob && activeFxIndex_ < data.audioFx.size()) {
            auto& fx = data.audioFx[activeFxIndex_];
            if (activeKnobIndex_ >= 0 && static_cast<size_t>(activeKnobIndex_) < fx.knobs.size()) {
                float dy = dragStartY_ - ev.y;
                float sens = ev.mods.shift ? 0.10f : 1.0f;
                float newVal = std::clamp(dragStartVal_ + (dy / 120.0f) * sens, 0.0f, 1.0f);
                fx.knobs[activeKnobIndex_].value = newVal;

                const auto& kname = fx.knobs[activeKnobIndex_].name;
                if (kname == "BITS" || kname == "bits") {
                    int b = 4 + static_cast<int>(std::round(newVal * 12.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(b) + " bit";
                } else if (kname == "CRUSH" || kname == "crush") {
                    int c = 1 + static_cast<int>(std::round(newVal * 31.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(c) + "x";
                } else if (kname == "DRIVE" || kname == "drive") {
                    float d = 1.0f + newVal * 9.0f;
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f x", d);
                    fx.knobs[activeKnobIndex_].display = buf;
                    fx.drive = newVal;
                } else if (kname == "MIX" || kname == "mix") {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f", newVal);
                    fx.knobs[activeKnobIndex_].display = buf;
                    fx.mix = newVal;
                } else if (kname == "TIME" || kname == "time") {
                    int ms = 10 + static_cast<int>(std::round(newVal * 990.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(ms) + " ms";
                } else if (kname == "FEEDBACK" || kname == "feedback" || kname == "FDBK") {
                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(pct) + "%";
                } else if (kname == "RATE" || kname == "rate") {
                    float hz = 0.1f + newVal * 9.9f;
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.1f Hz", hz);
                    fx.knobs[activeKnobIndex_].display = buf;
                } else if (kname == "DEPTH" || kname == "depth") {
                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(pct) + "%";
                } else {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.2f", newVal);
                    fx.knobs[activeKnobIndex_].display = buf;
                }

                if (activeKnobIndex_ == 0 && fx.knobs.size() == 2) fx.drive = newVal;

                if (onAudioFxParamChanged) {
                    onAudioFxParamChanged(data.trackIndex, activeFxIndex_, kname, newVal);
                }
                if (onAudioFxChanged) {
                    onAudioFxChanged(data.trackIndex);
                }
                return true;
            }
        }

        if (dragMode_ == DragMode::MidiFxKnob && activeFxIndex_ < data.midiFx.size()) {
            auto& fx = data.midiFx[activeFxIndex_];
            if (activeKnobIndex_ >= 0 && static_cast<size_t>(activeKnobIndex_) < fx.knobs.size()) {
                float dy = dragStartY_ - ev.y;
                float sens = ev.mods.shift ? 0.10f : 1.0f;
                float newVal = std::clamp(dragStartVal_ + (dy / 120.0f) * sens, 0.0f, 1.0f);
                fx.knobs[activeKnobIndex_].value = newVal;

                const std::string& kname = fx.knobs[activeKnobIndex_].name;
                std::string kLower = kname;
                for (char& c : kLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (kLower == "rate") {
                    int div = static_cast<int>(4 * std::pow(2.0f, std::round(newVal * 3.0f)));
                    fx.knobs[activeKnobIndex_].display = "1/" + std::to_string(div);
                } else if (kLower == "steps") {
                    int s = 1 + static_cast<int>(std::round(newVal * 15.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(s);
                } else if (kLower == "octaves") {
                    int oct = 1 + static_cast<int>(std::round(newVal * 3.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(oct);
                } else if (kLower == "gate" || kLower == "swing" || kLower == "chance" || kLower == "snap") {
                    int pct = static_cast<int>(std::round(newVal * 100.0f));
                    fx.knobs[activeKnobIndex_].display = std::to_string(pct) + "%";
                } else if (kLower == "root" || kLower == "rootkey") {
                    static const char* kNotes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
                    int rk = std::clamp(static_cast<int>(std::round(newVal * 11.0f)), 0, 11);
                    fx.rootKey = rk;
                    fx.knobs[activeKnobIndex_].display = kNotes[rk];
                } else if (kLower == "scale" || kLower == "scalemode") {
                    static const char* kScales[] = {"Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian"};
                    int sm = std::clamp(static_cast<int>(std::round(newVal * 6.0f)), 0, 6);
                    fx.scaleMode = sm;
                    fx.knobs[activeKnobIndex_].display = kScales[sm];
                } else if (kLower == "semitones" || kLower == "transpose" || kLower == "trans") {
                    int semi = static_cast<int>(std::round((newVal - 0.5f) * 24.0f));
                    fx.knobs[activeKnobIndex_].display = (semi > 0 ? "+" : "") + std::to_string(semi) + " st";
                } else if (kLower == "voicing") {
                    static const char* kVoicings[] = {"Triad", "7th", "Add9", "Sus4", "Power"};
                    int vIdx = std::clamp(static_cast<int>(std::round(newVal * 4.0f)), 0, 4);
                    fx.knobs[activeKnobIndex_].display = kVoicings[vIdx];
                } else if (kLower == "inversion") {
                    static const char* kInversions[] = {"Root", "1st", "2nd", "3rd"};
                    int invIdx = std::clamp(static_cast<int>(std::round(newVal * 3.0f)), 0, 3);
                    fx.knobs[activeKnobIndex_].display = kInversions[invIdx];
                } else {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.2f", newVal);
                    fx.knobs[activeKnobIndex_].display = buf;
                }

                if (onMidiFxParamChanged) {
                    onMidiFxParamChanged(data.trackIndex, kname, newVal);
                }
                if (onMidiFxChanged) {
                    onMidiFxChanged(data.trackIndex);
                }
                return true;
            }
        }
    }

    // Pointer Up or Cancel
    if (ev.action == PointerAction::Up || ev.action == PointerAction::Cancel) {
        isLongPressActive_ = false;
        if (dragMode_ == DragMode::ListBoxDrag) {
            if (draggingRow_ >= 0 && draggingRow_ < static_cast<int>(guiPanel_.rows.size()) &&
                draggingWidget_ >= 0 && draggingWidget_ < static_cast<int>(guiPanel_.rows[draggingRow_].widgets.size())) {
                auto& w = guiPanel_.rows[draggingRow_].widgets[draggingWidget_];
                int totalOpts = static_cast<int>(w.options.size());
                if (!isListBoxDragging_ && totalOpts > 0 && ev.action == PointerAction::Up) {
                    float boxX = w.bounds.x + 4.0f;
                    float boxY = w.bounds.y + 14.0f;
                    float boxW = w.bounds.w - 8.0f;
                    float boxH = std::max(24.0f, w.bounds.h - 16.0f);
                    float stripX = boxX + boxW - 13.0f;
                    int visibleRows = 4;
                    float rowH = (boxH - 4.0f) / static_cast<float>(visibleRows);

                    if (ev.x >= stripX && ev.x <= boxX + boxW) {
                        int maxScroll = std::max(0, totalOpts - visibleRows);
                        if (ev.y <= boxY + 14.0f) {
                            w.scrollOffset = std::max(0.0f, w.scrollOffset - 1.0f);
                        } else if (ev.y >= boxY + boxH - 14.0f) {
                            w.scrollOffset = std::min(static_cast<float>(maxScroll), w.scrollOffset + 1.0f);
                        } else {
                            float rel = (ev.y - boxY - 14.0f) / std::max(1.0f, boxH - 28.0f);
                            w.scrollOffset = std::clamp(rel * static_cast<float>(maxScroll), 0.0f, static_cast<float>(maxScroll));
                        }
                    } else if (ev.y >= boxY + 2.0f && ev.y <= boxY + boxH) {
                        int scrollStart = static_cast<int>(std::round(w.scrollOffset));
                        int clickedRow = static_cast<int>((ev.y - (boxY + 2.0f)) / rowH);
                        if (clickedRow >= 0 && clickedRow < visibleRows) {
                            int targetIdx = scrollStart + clickedRow;
                            if (targetIdx >= 0 && targetIdx < totalOpts) {
                                selectListBoxOption(w, targetIdx, data);
                            }
                        }
                    }
                }
            }
            dragMode_ = DragMode::None;
            isListBoxDragging_ = false;
            draggingRow_ = -1;
            draggingWidget_ = -1;
            return true;
        }

        if (dragMode_ == DragMode::TouchScroll) {
            dragMode_ = DragMode::None;
            scroller_.endDrag(ev.timestampMs);
            return true;
        }

        if (pendingHitResult_.hit) {
            TrackPropertiesHitResult hitToExec = pendingHitResult_;
            pendingHitResult_ = TrackPropertiesHitResult{};
            if (!touchDragCommitted_ && ev.action == PointerAction::Up) {
                return executeHitAction(hitToExec, data, ctx);
            }
            return true;
        }

        if (dragMode_ != DragMode::None) {
            dragMode_ = DragMode::None;
            activeKnobIndex_ = -1;
            draggingRow_ = -1;
            draggingWidget_ = -1;
            return true;
        }
    }

    return false;
}

bool TrackPropertiesPanel::handleKey(int key, int scancode, int action, int mods,
                                     [[maybe_unused]] const ViewContext& ctx) {
    if (colorPickerDialog_.isOpen()) {
        if (colorPickerDialog_.handleKey(key, scancode, action, mods)) return true;
        return true;
    }
    if (pluginDialog_.isOpen()) {
        if (pluginDialog_.handleKey(key, scancode, action, mods)) return true;
        return true;
    }

    if (action == 1 /* GLFW_PRESS */ || action == 2 /* GLFW_REPEAT */) {
        // If a ListBox is focused, handle arrow keys and selection
        for (auto& row : guiPanel_.rows) {
            for (auto& w : row.widgets) {
                if (w.type == GuiWidgetType::ListBox && !focusedWidgetId_.empty() &&
                    (w.id == focusedWidgetId_ || w.param == focusedWidgetId_)) {
                    int totalOpts = static_cast<int>(w.options.size());
                    if (totalOpts > 0) {
                        int currentIdx = std::clamp(static_cast<int>(std::round(w.currentVal)), 0, totalOpts - 1);
                        int newIdx = currentIdx;

                        if (key == 265 /* GLFW_KEY_UP */) {
                            newIdx = std::max(0, currentIdx - 1);
                        } else if (key == 264 /* GLFW_KEY_DOWN */) {
                            newIdx = std::min(totalOpts - 1, currentIdx + 1);
                        } else if (key == 266 /* GLFW_KEY_PAGE_UP */) {
                            newIdx = std::max(0, currentIdx - 4);
                        } else if (key == 267 /* GLFW_KEY_PAGE_DOWN */) {
                            newIdx = std::min(totalOpts - 1, currentIdx + 4);
                        } else if (key == 268 /* GLFW_KEY_HOME */) {
                            newIdx = 0;
                        } else if (key == 269 /* GLFW_KEY_END */) {
                            newIdx = totalOpts - 1;
                        } else if (key == 257 /* GLFW_KEY_ENTER */ || key == 32 /* GLFW_KEY_SPACE */) {
                            auto containsSfxr = [](std::string_view s) noexcept -> bool {
                                std::string lower;
                                lower.reserve(s.size());
                                for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                                return lower.find("sfxr") != std::string::npos;
                            };
                            bool isSfxr = (stringEqualsIgnoreCase(lastInstrumentEngine_, "sfxr") ||
                                           containsSfxr(lastInstrument_));
                            if (onAuditionNote) {
                                onAuditionNote(lastTrackIndex_, isSfxr ? 72 : 60, 0.85f);
                            }
                            return true;
                        } else if (key == 258 /* GLFW_KEY_TAB */) {
                            for (const auto& r2 : guiPanel_.rows) {
                                for (const auto& w2 : r2.widgets) {
                                    if (w2.type == GuiWidgetType::ListBox && w2.id != w.id) {
                                        focusedWidgetId_ = w2.id;
                                        return true;
                                    }
                                }
                            }
                            return true;
                        }

                        if (newIdx != currentIdx) {
                            TrackPropertiesDrawerData dummyData;
                            dummyData.trackIndex = lastTrackIndex_;
                            dummyData.instrumentEngine = lastInstrumentEngine_;
                            dummyData.instrument = lastInstrument_;
                            for (const auto& r2 : guiPanel_.rows) {
                                for (const auto& w2 : r2.widgets) {
                                    if (!w2.param.empty()) {
                                        dummyData.knobs.push_back({w2.param, w2.label, w2.currentVal, ""});
                                    }
                                }
                            }
                            selectListBoxOption(w, newIdx, dummyData);
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

} // namespace eatsbits::ui
