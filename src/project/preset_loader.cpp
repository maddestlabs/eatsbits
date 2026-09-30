#include "eatsbits/project/preset_loader.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
#include <cctype>
#include <iostream>
#include <filesystem>

namespace eatsbits::project {

namespace {

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Lightweight JSON/Python Dict Tokenizer for def gui():
enum class TokType {
    LBrace, RBrace, LBracket, RBracket, Colon, Comma,
    String, Number, Bool, Identifier, Eof
};

struct Token {
    TokType type{TokType::Eof};
    std::string str;
    double num{0.0};
    bool bVal{false};
};

class DictLexer {
public:
    explicit DictLexer(std::string text) : src_(std::move(text)) {}

    Token next() {
        skipWhitespaceAndComments();
        if (pos_ >= src_.size()) return {TokType::Eof, "", 0.0, false};

        char c = src_[pos_++];
        if (c == '{') return {TokType::LBrace, "{", 0.0, false};
        if (c == '}') return {TokType::RBrace, "}", 0.0, false};
        if (c == '[') return {TokType::LBracket, "[", 0.0, false};
        if (c == ']') return {TokType::RBracket, "]", 0.0, false};
        if (c == ':') return {TokType::Colon, ":", 0.0, false};
        if (c == ',') return {TokType::Comma, ",", 0.0, false};

        if (c == '"' || c == '\'') {
            char quote = c;
            std::string s;
            while (pos_ < src_.size() && src_[pos_] != quote) {
                if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
                    pos_++;
                }
                s += src_[pos_++];
            }
            if (pos_ < src_.size()) pos_++; // skip close quote
            return {TokType::String, s, 0.0, false};
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+') {
            size_t start = pos_ - 1;
            while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' || src_[pos_] == '-' || src_[pos_] == '+')) {
                pos_++;
            }
            std::string numStr = src_.substr(start, pos_ - start);
            try {
                double val = std::stod(numStr);
                return {TokType::Number, numStr, val, false};
            } catch (...) {
                return {TokType::Number, numStr, 0.0, false};
            }
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos_ - 1;
            while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
                pos_++;
            }
            std::string ident = src_.substr(start, pos_ - start);
            if (ident == "True" || ident == "true") return {TokType::Bool, ident, 1.0, true};
            if (ident == "False" || ident == "false") return {TokType::Bool, ident, 0.0, false};
            return {TokType::Identifier, ident, 0.0, false};
        }

        return {TokType::Eof, "", 0.0, false};
    }

private:
    void skipWhitespaceAndComments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) {
                pos_++;
                continue;
            }
            if (c == '#') {
                while (pos_ < src_.size() && src_[pos_] != '\n') {
                    pos_++;
                }
                continue;
            }
            break;
        }
    }

    std::string src_;
    size_t pos_{0};
};

class DictParser {
public:
    explicit DictParser(const std::string& text) : lexer_(text) {
        advance();
    }

    GuiLayoutNode parseGuiNode() {
        GuiLayoutNode node{};
        if (current_.type != TokType::LBrace) {
            return node;
        }
        advance(); // eat '{'

        while (current_.type != TokType::RBrace && current_.type != TokType::Eof) {
            if (current_.type != TokType::String && current_.type != TokType::Identifier) {
                advance();
                continue;
            }
            std::string key = current_.str;
            advance(); // eat key

            if (current_.type == TokType::Colon) {
                advance(); // eat ':'
            }

            if (key == "panel") {
                node.type = GuiNodeType::Panel;
                GuiLayoutNode panelChild = parseGuiNode();
                node.title = panelChild.title;
                node.subtitle = panelChild.subtitle;
                node.background = panelChild.background;
                node.accent = panelChild.accent;
                node.defaultKnobStyle = panelChild.defaultKnobStyle;
                node.children = std::move(panelChild.children);
            } else if (key == "type") {
                std::string t = current_.str;
                advance();
                if (t == "row") node.type = GuiNodeType::Row;
                else if (t == "column") node.type = GuiNodeType::Column;
                else if (t == "group") node.type = GuiNodeType::Group;
                else if (t == "knob") node.type = GuiNodeType::Knob;
                else if (t == "nixie") node.type = GuiNodeType::Nixie;
                else if (t == "slider") node.type = GuiNodeType::Slider;
                else if (t == "scope") node.type = GuiNodeType::Scope;
                else if (t == "divider") node.type = GuiNodeType::Divider;
                else if (t == "spacer") node.type = GuiNodeType::Spacer;
            } else if (key == "title") {
                node.title = current_.str; advance();
            } else if (key == "subtitle") {
                node.subtitle = current_.str; advance();
            } else if (key == "background") {
                node.background = current_.str; advance();
            } else if (key == "accent") {
                node.accent = current_.str; advance();
            } else if (key == "knobStyle" || key == "style") {
                node.defaultKnobStyle = current_.str;
                node.hardwareStyle = current_.str;
                advance();
            } else if (key == "hardware") {
                node.hardwareStyle = current_.str; advance();
            } else if (key == "param") {
                node.paramName = current_.str; advance();
            } else if (key == "label") {
                node.label = current_.str; advance();
            } else if (key == "unit") {
                node.unit = current_.str; advance();
            } else if (key == "size") {
                node.size = static_cast<float>(current_.num); advance();
            } else if (key == "showValue") {
                node.showValue = current_.bVal; advance();
            } else if (key == "orientation") {
                node.orientation = current_.str; advance();
            } else if (key == "align") {
                node.align = current_.str; advance();
            } else if (key == "crossAlign") {
                node.crossAlign = current_.str; advance();
            } else if (key == "children" || key == "layout") {
                if (current_.type == TokType::LBracket) {
                    advance(); // eat '['
                    while (current_.type != TokType::RBracket && current_.type != TokType::Eof) {
                        if (current_.type == TokType::LBrace) {
                            node.children.push_back(parseGuiNode());
                        } else {
                            advance();
                        }
                        if (current_.type == TokType::Comma) advance();
                    }
                    if (current_.type == TokType::RBracket) advance(); // eat ']'
                } else {
                    skipValue();
                }
            } else {
                skipValue();
            }

            if (current_.type == TokType::Comma) {
                advance();
            }
        }

        if (current_.type == TokType::RBrace) {
            advance(); // eat '}'
        }

        return node;
    }

private:
    void advance() {
        current_ = lexer_.next();
    }

    void skipValue() {
        if (current_.type == TokType::LBrace) {
            int depth = 1;
            advance();
            while (depth > 0 && current_.type != TokType::Eof) {
                if (current_.type == TokType::LBrace) depth++;
                else if (current_.type == TokType::RBrace) depth--;
                advance();
            }
        } else if (current_.type == TokType::LBracket) {
            int depth = 1;
            advance();
            while (depth > 0 && current_.type != TokType::Eof) {
                if (current_.type == TokType::LBracket) depth++;
                else if (current_.type == TokType::RBracket) depth--;
                advance();
            }
        } else {
            advance();
        }
    }

    DictLexer lexer_;
    Token current_;
};

GuiLayoutNode convertValueToGuiNode(const eatscript::Value& val) {
    GuiLayoutNode node{};
    if (!val.isDict()) return node;

    // Check "panel" wrapper if top-level
    if (val.hasKey("panel") && val.get("panel").isDict()) {
        return convertValueToGuiNode(val.get("panel"));
    }

    std::string t = val.get("type", eatscript::Value("panel")).asString();
    if (t == "panel") node.type = GuiNodeType::Panel;
    else if (t == "row") node.type = GuiNodeType::Row;
    else if (t == "column") node.type = GuiNodeType::Column;
    else if (t == "group") node.type = GuiNodeType::Group;
    else if (t == "knob") node.type = GuiNodeType::Knob;
    else if (t == "nixie") node.type = GuiNodeType::Nixie;
    else if (t == "slider") node.type = GuiNodeType::Slider;
    else if (t == "scope") node.type = GuiNodeType::Scope;
    else if (t == "divider") node.type = GuiNodeType::Divider;
    else if (t == "spacer") node.type = GuiNodeType::Spacer;

    node.title = val.get("title").asString();
    node.subtitle = val.get("subtitle").asString();
    node.background = val.get("background").asString();
    node.accent = val.get("accent").asString();

    if (val.hasKey("knobStyle")) {
        node.defaultKnobStyle = val.get("knobStyle").asString();
        node.hardwareStyle = node.defaultKnobStyle;
    } else if (val.hasKey("style")) {
        node.defaultKnobStyle = val.get("style").asString();
        node.hardwareStyle = node.defaultKnobStyle;
    }
    if (val.hasKey("hardware")) {
        node.hardwareStyle = val.get("hardware").asString();
    }
    node.paramName = val.get("param").asString();
    node.label = val.get("label").asString();
    node.unit = val.get("unit").asString();
    node.size = val.get("size", eatscript::Value(0.0)).asFloat();
    node.showValue = val.get("showValue", eatscript::Value(false)).asBoolean();
    node.orientation = val.get("orientation").asString();
    node.align = val.get("align").asString();
    node.crossAlign = val.get("crossAlign").asString();

    eatscript::Value childrenVal = val.hasKey("children") ? val.get("children") : val.get("layout");
    if (childrenVal.isList()) {
        for (const auto& childVal : childrenVal.listVal) {
            node.children.push_back(convertValueToGuiNode(childVal));
        }
    }
    return node;
}

} // anonymous namespace

PresetDefinition PresetLoader::parseFromEatscript(const std::string& scriptSource) {
    PresetDefinition def{};
    def.rawScript = scriptSource;

    std::istringstream stream(scriptSource);
    std::string line;

    // 1. Parse Comments & Directives (# @key: value)
    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.rfind("# @", 0) == 0) {
            size_t colon = trimmed.find(':');
            if (colon != std::string::npos) {
                std::string directive = trim(trimmed.substr(3, colon - 3));
                std::string value = trim(trimmed.substr(colon + 1));
                if (directive == "id") def.metadata.id = value;
                else if (directive == "name") def.metadata.name = value;
                else if (directive == "category") def.metadata.category = value;
                else if (directive == "description") def.metadata.description = value;
                else if (directive == "engine") def.metadata.engineId = value;
            }
        }
    }

    // 2. Evaluate AST via Evaluator
    eatscript::Evaluator evaluator;
    evaluator.evaluateSource(scriptSource);

    if (evaluator.hasFunction("init")) {
        eatscript::Value initRes = evaluator.callFunction("init");
        if (initRes.isDict()) {
            for (const auto& [key, pVal] : initRes.dictVal) {
                if (pVal.isDict()) {
                    PresetParam p{};
                    p.name = pVal.get("name", eatscript::Value(key)).asString();
                    p.minVal = pVal.get("min", eatscript::Value(0.0)).asFloat();
                    p.maxVal = pVal.get("max", eatscript::Value(1.0)).asFloat();
                    p.defaultVal = pVal.get("default", eatscript::Value(p.minVal)).asFloat();
                    p.currentVal = p.defaultVal;
                    p.step = pVal.get("step", eatscript::Value(0.0)).asFloat();
                    p.unit = pVal.get("unit", eatscript::Value("")).asString();
                    p.allowVariance = pVal.get("allow_variance", eatscript::Value(true)).asBoolean(true);
                    def.params[key] = p;
                }
            }
        }
    }

    // Fallback: If AST evaluation extracted no params, use legacy regex
    if (def.params.empty()) {
        std::regex paramRegex(R"param("([^"]+)"\s*:\s*eat\.param\s*\(\s*"([^"]+)"\s*,\s*([+-]?[\d\.]+)\s*,\s*([+-]?[\d\.]+)\s*,\s*([+-]?[\d\.]+)([^)]*)\))param");
        auto words_begin = std::sregex_iterator(scriptSource.begin(), scriptSource.end(), paramRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
            std::smatch match = *it;
            std::string key = match[1].str();
            std::string name = match[2].str();
            float minVal = std::stof(match[3].str());
            float maxVal = std::stof(match[4].str());
            float defVal = std::stof(match[5].str());
            std::string extra = match[6].str();

            PresetParam p{};
            p.name = name;
            p.minVal = minVal;
            p.maxVal = maxVal;
            p.defaultVal = defVal;
            p.currentVal = defVal;

            std::regex stepRegex(R"(step\s*=\s*([+-]?[\d\.]+))");
            std::smatch stepMatch;
            if (std::regex_search(extra, stepMatch, stepRegex)) p.step = std::stof(stepMatch[1].str());

            std::regex unitRegex(R"rx(unit\s*=\s*"([^"]*)")rx");
            std::smatch unitMatch;
            if (std::regex_search(extra, unitMatch, unitRegex)) p.unit = unitMatch[1].str();

            std::regex varRegex(R"(allow_variance\s*=\s*(True|False|true|false))");
            std::smatch varMatch;
            if (std::regex_search(extra, varMatch, varRegex)) {
                std::string b = varMatch[1].str();
                p.allowVariance = (b == "True" || b == "true");
            }
            def.params[key] = p;
        }
    }

    // 3. Evaluate Hardware GUI in def gui():
    if (evaluator.hasFunction("gui")) {
        eatscript::Value guiRes = evaluator.callFunction("gui");
        if (guiRes.isDict()) {
            def.guiRoot = convertValueToGuiNode(guiRes);
            if (def.guiRoot.title.empty()) {
                def.guiRoot.title = def.metadata.name;
            }
        }
    }

    // Fallback: If AST evaluation produced empty guiRoot, use DictParser
    if (def.guiRoot.children.empty() && def.guiRoot.title.empty()) {
        size_t guiPos = scriptSource.find("def gui():");
        if (guiPos != std::string::npos) {
            size_t returnPos = scriptSource.find("return", guiPos);
            if (returnPos != std::string::npos) {
                size_t bracePos = scriptSource.find('{', returnPos);
                if (bracePos != std::string::npos) {
                    std::string guiSnippet = scriptSource.substr(bracePos);
                    DictParser parser(guiSnippet);
                    def.guiRoot = parser.parseGuiNode();
                    if (def.guiRoot.title.empty()) {
                        def.guiRoot.title = def.metadata.name;
                    }
                }
            }
        }
    }

    if (def.rawScript.find(" = True") == std::string::npos && def.rawScript.find(" = true") == std::string::npos) {
        if (def.metadata.id == "eats_303" || def.metadata.engineId == "tb303") {
            def.rawScript = "Eats303 = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "analog_808_kick" || def.metadata.engineId == "drum_808") {
            def.rawScript = "Analog808Kick = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "analog_909_snare" || def.metadata.engineId == "drum_909") {
            def.rawScript = "Analog909Snare = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "stereo_delay" || def.metadata.engineId == "delay") {
            def.rawScript = "StereoDelay = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "c64_sid_synth" || def.metadata.engineId == "sid") {
            def.rawScript = "SIDSynth = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "dx7_epiano" || def.metadata.engineId == "dx7") {
            def.rawScript = "DX7EPiano = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "snes_dsp" || def.metadata.engineId == "snes") {
            def.rawScript = "snesDsp = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "genesis_ym2612" || def.metadata.engineId == "ym2612") {
            def.rawScript = "ym2612 = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "convolver_space" || def.metadata.engineId == "convolver") {
            def.rawScript = "Convolver = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "concert_grand_piano" || def.metadata.engineId == "piano_physical") {
            def.rawScript = "ConcertGrandPiano = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "upright_bass") {
            def.rawScript = "DoubleBass = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "spanish_guitar") {
            def.rawScript = "SpanishGuitar = True\n\n" + def.rawScript;
        } else if (def.metadata.id == "acoustic_steel_guitar") {
            def.rawScript = "SteelAcousticGuitar = True\n\n" + def.rawScript;
        }
    }

    return def;
}

bool PresetLoader::loadFromFile(const std::string& filePath, PresetDefinition& outPreset) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    outPreset = parseFromEatscript(buffer.str());
    return true;
}

std::vector<PresetDefinition> PresetLoader::loadDirectory(const std::string& dirPath) {
    std::vector<PresetDefinition> presets;
    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec)) {
        return presets;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".eats") {
            PresetDefinition def;
            if (loadFromFile(entry.path().string(), def)) {
                presets.push_back(std::move(def));
            }
        }
    }
    return presets;
}

PresetDefinition PresetLoader::createEats303Preset() {
    PresetDefinition p{};
    p.metadata.id = "eats_303";
    p.metadata.name = "EATS-303 ACID BASSLINE";
    p.metadata.category = "instrument";
    p.metadata.description = "Authentic Roland TB-303 acid bassline with 24dB diode ladder filter, slide, accent, and distortion.";
    p.metadata.engineId = "tb303";

    p.params["Waveform"] = {"Waveform", 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["Pitch"] = {"Pitch", -12.0f, 12.0f, 0.0f, 0.0f, 1.0f, "st", false};
    p.params["Cutoff"] = {"Cutoff", 200.0f, 4500.0f, 1400.0f, 1400.0f, 0.0f, "Hz", true};
    p.params["Resonance"] = {"Resonance", 0.5f, 16.0f, 9.2f, 9.2f, 0.0f, "", true};
    p.params["EnvMod"] = {"EnvMod", 0.0f, 1.0f, 0.75f, 0.75f, 0.0f, "", true};
    p.params["Decay"] = {"Decay", 0.05f, 1.2f, 0.28f, 0.28f, 0.0f, "s", true};
    p.params["Accent"] = {"Accent", 0.0f, 1.0f, 0.78f, 0.78f, 0.0f, "", true};
    p.params["Octave"] = {"Octave", -2.0f, 0.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["SubWaveform"] = {"SubWaveform", 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["SubVolume"] = {"SubVolume", 0.0f, 1.0f, 0.35f, 0.35f, 0.0f, "", true};
    p.params["GlideCurve"] = {"GlideCurve", 0.0f, 2.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["Drive"] = {"Drive", 0.0f, 1.0f, 0.25f, 0.25f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "EATS-303 ACID BASSLINE";
    p.guiRoot.subtitle = "Authentic Diode Ladder Filter • 60ms Slide Portamento";
    p.guiRoot.background = "minimal_white";
    p.guiRoot.accent = "#000000";

    // Row 1
    GuiLayoutNode row1{};
    row1.type = GuiNodeType::Row;
    row1.align = "space_around";
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Waveform", "WAVEFORM", "", 48.0f, "tb303_selector", false});
    row1.children.push_back({GuiNodeType::Divider});
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Pitch", "PITCH", "st", 48.0f, "tb303_potentiometer", false});
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Cutoff", "CUTOFF", "Hz", 48.0f, "tb303_potentiometer", false});
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Resonance", "RESONANCE", "", 48.0f, "tb303_potentiometer", false});
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "EnvMod", "ENV MOD", "", 48.0f, "tb303_potentiometer", false});
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Decay", "DECAY", "s", 48.0f, "tb303_potentiometer", false});
    row1.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Accent", "ACCENT", "", 48.0f, "tb303_potentiometer", false});

    // Row 2
    GuiLayoutNode row2{};
    row2.type = GuiNodeType::Row;
    row2.align = "space_around";
    row2.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Octave", "OCTAVE", "", 48.0f, "tb303_selector", false});
    row2.children.push_back({GuiNodeType::Divider});
    row2.children.push_back({GuiNodeType::Switch, "", "", "", "", "hardware", "", "", "", "SubWaveform", "SUB OSC", "", 48.0f, "vertical", false});
    row2.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "SubVolume", "SUB VOL", "", 48.0f, "cream_fluted", false});
    row2.children.push_back({GuiNodeType::Divider});
    row2.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "GlideCurve", "GLIDE CURVE", "", 48.0f, "cream_fluted", false});
    row2.children.push_back({GuiNodeType::Divider});
    row2.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Drive", "DRIVE", "", 48.0f, "cream_fluted", false});

    p.guiRoot.children.push_back(row1);
    p.guiRoot.children.push_back(row2);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: eats_303\n"
        "# @name: EATS-303 ACID BASSLINE\n"
        "# @category: instrument\n"
        "# @engine: tb303\n\n"
        "Eats303 = True\n\n"
        "def init():\n"
        "    params['Waveform'] = 0.0\n"
        "    params['Pitch'] = 0.0\n"
        "    params['Cutoff'] = 1400.0\n"
        "    params['Resonance'] = 9.2\n"
        "    params['EnvMod'] = 0.75\n"
        "    params['Decay'] = 0.28\n"
        "    params['Accent'] = 0.78\n"
        "    params['Drive'] = 0.25\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Roland TB-303 Diode Ladder Filter Engine\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createAnalog808KickPreset() {
    PresetDefinition p{};
    p.metadata.id = "analog_808_kick";
    p.metadata.name = "EATS-808 ANALOG BASS DRUM";
    p.metadata.category = "drum";
    p.metadata.description = "Authentic Bridged-T resonant circuit with pitch drop sweep and tone control.";
    p.metadata.engineId = "drum_808";

    p.params["Tune"] = {"Tune", 35.0f, 75.0f, 46.0f, 46.0f, 0.0f, "Hz", true};
    p.params["StartFreq"] = {"StartFreq", 80.0f, 220.0f, 140.0f, 140.0f, 0.0f, "Hz", true};
    p.params["PitchDecay"] = {"PitchDecay", 0.01f, 0.15f, 0.045f, 0.045f, 0.0f, "s", true};
    p.params["Decay"] = {"Decay", 0.1f, 4.0f, 0.85f, 0.85f, 0.0f, "s", true};
    p.params["Tone"] = {"Tone", 100.0f, 800.0f, 220.0f, 220.0f, 0.0f, "Hz", true};
    p.params["Click"] = {"Click", 0.0f, 1.0f, 0.15f, 0.15f, 0.0f, "", true};
    p.params["Overdrive"] = {"Overdrive", 0.8f, 3.0f, 1.25f, 1.25f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "EATS-808 ANALOG BASS DRUM";
    p.guiRoot.subtitle = "Authentic Bridged-T Resonant Sine Circuit • Click Transient";
    p.guiRoot.background = "dark";
    p.guiRoot.accent = "#FF6600"; // 808 Orange

    // Knob Row
    GuiLayoutNode knobRow{};
    knobRow.type = GuiNodeType::Row;
    knobRow.align = "space_around";
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Tune", "TUNE", "Hz", 52.0f, "chromeFluted", true});
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "StartFreq", "PUNCH", "Hz", 52.0f, "chromeFluted", true});
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "PitchDecay", "SWEEP", "s", 52.0f, "chromeFluted", true});
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Decay", "DECAY", "s", 52.0f, "chromeFluted", true});
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Tone", "TONE", "Hz", 52.0f, "chromeFluted", true});
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Click", "CLICK", "", 52.0f, "chromeFluted", true});
    knobRow.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Overdrive", "DRIVE", "", 52.0f, "chromeFluted", true});

    p.guiRoot.children.push_back(knobRow);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: analog_808_kick\n"
        "# @name: EATS-808 ANALOG BASS DRUM\n"
        "# @category: drum\n"
        "# @engine: drum_808\n\n"
        "Analog808Kick = True\n\n"
        "def init():\n"
        "    params['Tune'] = 46.0\n"
        "    params['StartFreq'] = 140.0\n"
        "    params['PitchDecay'] = 0.045\n"
        "    params['Decay'] = 0.85\n"
        "    params['Tone'] = 220.0\n"
        "    params['Click'] = 0.15\n"
        "    params['Overdrive'] = 1.25\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Bridged-T Resonant Circuit Bass Drum\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createAnalog909SnarePreset() {
    PresetDefinition p{};
    p.metadata.id = "analog_909_snare";
    p.metadata.name = "EATS-909 ANALOG SNARE";
    p.metadata.category = "drum";
    p.metadata.description = "Dual resonant sine body shells plus highpass snappy wire noise generator.";
    p.metadata.engineId = "drum_909";

    p.params["Tune"] = {"Tune", 150.0f, 320.0f, 220.0f, 220.0f, 0.0f, "Hz", true};
    p.params["Snappy"] = {"Snappy", 0.0f, 1.0f, 0.70f, 0.70f, 0.0f, "", true};
    p.params["Decay"] = {"Decay", 0.05f, 0.8f, 0.32f, 0.32f, 0.0f, "s", true};
    p.params["Tone"] = {"Tone", 500.0f, 4000.0f, 1800.0f, 1800.0f, 0.0f, "Hz", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "EATS-909 ANALOG SNARE DRUM";
    p.guiRoot.subtitle = "Dual Resonant Shells • Snappy White Noise Matrix";
    p.guiRoot.background = "silver";
    p.guiRoot.accent = "#0088FF";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.align = "space_around";
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Tune", "TUNE", "Hz", 52.0f, "twoToneStepped", true});
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Snappy", "SNAPPY", "", 52.0f, "twoToneStepped", true});
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Tone", "TONE", "Hz", 52.0f, "twoToneStepped", true});
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "hardware", "", "", "", "Decay", "DECAY", "s", 52.0f, "twoToneStepped", true});

    p.guiRoot.children.push_back(row);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: analog_909_snare\n"
        "# @name: EATS-909 ANALOG SNARE\n"
        "# @category: drum\n"
        "# @engine: drum_909\n\n"
        "Analog909Snare = True\n\n"
        "def init():\n"
        "    params['Tune'] = 220.0\n"
        "    params['Snappy'] = 0.70\n"
        "    params['Decay'] = 0.32\n"
        "    params['Tone'] = 1800.0\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Dual Resonant Shells & Snappy Noise Matrix\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createStereoDelayPreset() {
    PresetDefinition p{};
    p.metadata.id = "stereo_delay";
    p.metadata.name = "STUDIO STEREO TAPE DELAY";
    p.metadata.category = "audio_fx";
    p.metadata.description = "Stereo cross-feedback echo with tape saturation and highpass damping.";
    p.metadata.engineId = "delay";

    p.params["TimeMs"] = {"TimeMs", 10.0f, 1000.0f, 320.0f, 320.0f, 0.0f, "ms", true};
    p.params["Feedback"] = {"Feedback", 0.0f, 0.95f, 0.45f, 0.45f, 0.0f, "", true};
    p.params["Damping"] = {"Damping", 1000.0f, 18000.0f, 8000.0f, 8000.0f, 0.0f, "Hz", true};
    p.params["Mix"] = {"Mix", 0.0f, 1.0f, 0.35f, 0.35f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "STUDIO STEREO TAPE ECHO";
    p.guiRoot.subtitle = "Cross-Feedback Echo • Analog Tape Saturation";
    p.guiRoot.background = "dark";
    p.guiRoot.accent = "#00FFE0";

    GuiLayoutNode row{};
    row.type = GuiNodeType::Row;
    row.align = "space_around";
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "TimeMs", "TIME", "ms", 52.0f, "chromeFluted", true});
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Feedback", "FEEDBACK", "", 52.0f, "chromeFluted", true});
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Damping", "DAMPING", "Hz", 52.0f, "chromeFluted", true});
    row.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Mix", "DRY/WET", "", 52.0f, "chromeFluted", true});

    p.guiRoot.children.push_back(row);

    p.rawScript =
        "# Eatsbeats Audio FX Definition\n"
        "# @id: stereo_delay\n"
        "# @name: STUDIO STEREO TAPE DELAY\n"
        "# @category: audio_fx\n"
        "# @engine: delay\n\n"
        "StereoDelay = True\n\n"
        "def init():\n"
        "    params['TimeMs'] = 320.0\n"
        "    params['Feedback'] = 0.45\n"
        "    params['Damping'] = 8000.0\n"
        "    params['Mix'] = 0.35\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Stereo Tape Saturation Delay Echo\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createC64SidPreset() {
    PresetDefinition p{};
    p.metadata.id = "c64_sid_synth";
    p.metadata.name = "COMMODORE 64 SID SYNTH";
    p.metadata.category = "instrument";
    p.metadata.description = "Authentic MOS 6581 / 8580 Commodore 64 Sound Interface Device: 12-bit PWM, 23-bit Galois LFSR noise, hardware ADSR, and 12dB/oct resonant filter.";
    p.metadata.engineId = "sid";

    p.params["Waveform"] = {"Waveform", 0.0f, 5.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["PulseWidth"] = {"PulseWidth", 100.0f, 4000.0f, 2048.0f, 2048.0f, 0.0f, "", true};
    p.params["PwmRate"] = {"PwmRate", 0.1f, 10.0f, 1.6f, 1.6f, 0.0f, "Hz", true};
    p.params["PwmDepth"] = {"PwmDepth", 0.0f, 1.0f, 0.45f, 0.45f, 0.0f, "", true};
    p.params["ArpMode"] = {"ArpMode", 0.0f, 5.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["GlideSpeed"] = {"GlideSpeed", 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, "s", true};
    p.params["ChipModel"] = {"ChipModel", 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["FilterMode"] = {"FilterMode", 0.0f, 4.0f, 0.0f, 0.0f, 1.0f, "", false};
    p.params["Cutoff"] = {"Cutoff", 100.0f, 2047.0f, 1350.0f, 1350.0f, 0.0f, "", true};
    p.params["Resonance"] = {"Resonance", 0.0f, 15.0f, 9.0f, 9.0f, 0.0f, "", true};
    p.params["Overdrive"] = {"Overdrive", 1.0f, 3.0f, 1.2f, 1.2f, 0.0f, "", true};
    p.params["Attack"] = {"Attack", 0.0f, 15.0f, 1.0f, 1.0f, 1.0f, "", false};
    p.params["Decay"] = {"Decay", 0.0f, 15.0f, 6.0f, 6.0f, 1.0f, "", false};
    p.params["Sustain"] = {"Sustain", 0.0f, 15.0f, 12.0f, 12.0f, 1.0f, "", false};
    p.params["Release"] = {"Release", 0.0f, 15.0f, 5.0f, 5.0f, 1.0f, "", false};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "COMMODORE 64 — SID SYNTHESIZER";
    p.guiRoot.subtitle = "MOS 6581 / 8580 3-Voice Chiptune Device • 23-Bit Galois LFSR";
    p.guiRoot.background = "c64_breadbin";
    p.guiRoot.accent = "#6C5EB5";

    // Row 1: Oscillator & PWM
    GuiLayoutNode gOsc{};
    gOsc.type = GuiNodeType::Group;
    gOsc.title = "OSCILLATOR & HARDWARE PWM";
    GuiLayoutNode rOsc{};
    rOsc.type = GuiNodeType::Row;
    rOsc.align = "space_around";
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Waveform", "WAVEFORM", "", 52.0f, "twoToneStepped", false});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "PulseWidth", "PULSE WIDTH", "", 52.0f, "chromeFluted", true});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "PwmRate", "PWM RATE", "Hz", 52.0f, "chromeFluted", true});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "PwmDepth", "PWM DEPTH", "", 52.0f, "chromeFluted", true});
    gOsc.children.push_back(rOsc);

    // Row 2: Arpeggiator & Filter
    GuiLayoutNode gFlt{};
    gFlt.type = GuiNodeType::Group;
    gFlt.title = "12dB/OCT RESONANT FILTER & HARDWARE ADSR";
    GuiLayoutNode rFlt{};
    rFlt.type = GuiNodeType::Row;
    rFlt.align = "space_around";
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "ArpMode", "ARP (50/60Hz)", "", 52.0f, "twoToneStepped", false});
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Cutoff", "CUTOFF", "", 52.0f, "chromeFluted", true});
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Resonance", "RESONANCE", "", 52.0f, "chromeFluted", true});
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Attack", "ATTACK", "", 52.0f, "twoToneStepped", true});
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Decay", "DECAY", "", 52.0f, "twoToneStepped", true});
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "twoToneStepped", true});
    rFlt.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Release", "RELEASE", "", 52.0f, "twoToneStepped", true});
    gFlt.children.push_back(rFlt);

    p.guiRoot.children.push_back(gOsc);
    p.guiRoot.children.push_back(gFlt);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: c64_sid_synth\n"
        "# @name: COMMODORE 64 SID SYNTH\n"
        "# @category: instrument\n"
        "# @engine: sid\n\n"
        "SIDSynth = True\n\n"
        "def init():\n"
        "    params['Waveform'] = 0.0\n"
        "    params['PulseWidth'] = 2048.0\n"
        "    params['PwmRate'] = 1.6\n"
        "    params['PwmDepth'] = 0.45\n"
        "    params['ArpMode'] = 0.0\n"
        "    params['GlideSpeed'] = 0.0\n"
        "    params['ChipModel'] = 0.0\n"
        "    params['FilterMode'] = 0.0\n"
        "    params['Cutoff'] = 1350.0\n"
        "    params['Resonance'] = 9.0\n"
        "    params['Overdrive'] = 1.2\n"
        "    params['Attack'] = 1.0\n"
        "    params['Decay'] = 6.0\n"
        "    params['Sustain'] = 12.0\n"
        "    params['Release'] = 5.0\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native MOS 6581/8580 12-Bit PWM & 23-Bit Galois LFSR\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createYamahaDx7Preset() {
    PresetDefinition p{};
    p.metadata.id = "dx7_epiano";
    p.metadata.name = "YAMAHA DX7 E.PIANO 1";
    p.metadata.category = "instrument";
    p.metadata.description = "Iconic 1983 6-operator digital FM electric piano. Algorithm 5 with 3-stack carriers and metallic tine modulators.";
    p.metadata.engineId = "dx7";

    p.params["Algorithm"] = {"Algorithm", 1.0f, 32.0f, 5.0f, 5.0f, 1.0f, "", false};
    p.params["Feedback"] = {"Feedback", 0.0f, 7.0f, 6.0f, 6.0f, 1.0f, "", false};
    p.params["Brightness"] = {"Brightness", 0.0f, 2.0f, 1.0f, 1.0f, 0.0f, "", true};
    p.params["TineBell"] = {"TineBell", 0.0f, 2.0f, 0.85f, 0.85f, 0.0f, "", true};
    p.params["BodyWarmth"] = {"BodyWarmth", 0.0f, 2.0f, 1.0f, 1.0f, 0.0f, "", true};
    p.params["Volume"] = {"Volume", 0.0f, 2.0f, 0.85f, 0.85f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "YAMAHA DX7 — 6-OPERATOR FM SYNTH";
    p.guiRoot.subtitle = "Algorithm 5: E.PIANO 1 • 3-Stack Dynamic FM • 1983 Voice";
    p.guiRoot.background = "dx7_faceplate";
    p.guiRoot.accent = "#00A887";

    // Row 1: Algorithm & Feedback
    GuiLayoutNode gAlg{};
    gAlg.type = GuiNodeType::Group;
    gAlg.title = "FM ALGORITHM & OPERATOR FEEDBACK";
    GuiLayoutNode rAlg{};
    rAlg.type = GuiNodeType::Row;
    rAlg.align = "space_around";
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Algorithm", "ALGORITHM", "", 52.0f, "twoToneStepped", false});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Feedback", "FEEDBACK", "", 52.0f, "chromeFluted", false});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Volume", "MASTER VOL", "", 52.0f, "chromeFluted", true});
    gAlg.children.push_back(rAlg);

    // Row 2: Timbre & Harmonic Modulators
    GuiLayoutNode gTim{};
    gTim.type = GuiNodeType::Group;
    gTim.title = "TIMBRE MACROS & HARMONIC SHAPING";
    GuiLayoutNode rTim{};
    rTim.type = GuiNodeType::Row;
    rTim.align = "space_around";
    rTim.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Brightness", "BRIGHTNESS", "", 52.0f, "chromeFluted", true});
    rTim.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "TineBell", "TINE BELL", "", 52.0f, "chromeFluted", true});
    rTim.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "BodyWarmth", "BODY WARMTH", "", 52.0f, "chromeFluted", true});
    gTim.children.push_back(rTim);

    p.guiRoot.children.push_back(gAlg);
    p.guiRoot.children.push_back(gTim);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: dx7_epiano\n"
        "# @name: YAMAHA DX7 E.PIANO 1\n"
        "# @category: instrument\n"
        "# @engine: dx7\n\n"
        "DX7EPiano = True\n\n"
        "def init():\n"
        "    params['Algorithm'] = 5.0\n"
        "    params['Feedback'] = 6.0\n"
        "    params['Brightness'] = 1.0\n"
        "    params['TineBell'] = 0.85\n"
        "    params['BodyWarmth'] = 1.0\n"
        "    params['Volume'] = 0.85\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native 6-Operator Dynamic Phase Modulation Engine\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createSnesPreset() {
    PresetDefinition p{};
    p.metadata.id = "snes_dsp";
    p.metadata.name = "SUPER NINTENDO S-DSP";
    p.metadata.category = "instrument";
    p.metadata.description = "Authentic Sony SPC700 16-bit sound chip with 12 BRR waveforms, 8-tap FIR echo, PMOD modulation, and LFSR noise.";
    p.metadata.engineId = "snes";

    p.params["Waveform"] = {"Waveform", 0.0f, 11.0f, 1.0f, 1.0f, 1.0f, "", false};
    p.params["Attack"] = {"Attack", 0.001f, 1.0f, 0.005f, 0.005f, 0.0f, "s", true};
    p.params["Decay"] = {"Decay", 0.01f, 1.0f, 0.25f, 0.25f, 0.0f, "s", true};
    p.params["Sustain"] = {"Sustain", 0.0f, 1.0f, 0.40f, 0.40f, 0.0f, "", true};
    p.params["Release"] = {"Release", 0.01f, 1.0f, 0.20f, 0.20f, 0.0f, "s", true};
    p.params["EchoDelay"] = {"EchoDelay", 16.0f, 480.0f, 120.0f, 120.0f, 16.0f, "ms", false};
    p.params["EchoFeedback"] = {"EchoFeedback", 0.0f, 1.0f, 0.45f, 0.45f, 0.0f, "", true};
    p.params["EchoVolume"] = {"EchoVolume", 0.0f, 1.0f, 0.40f, 0.40f, 0.0f, "", true};
    p.params["Volume"] = {"Volume", 0.0f, 2.0f, 0.85f, 0.85f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "SUPER NINTENDO S-SMP / SPC700";
    p.guiRoot.subtitle = "16-Bit Sony S-DSP • 8-Tap FIR Hardware Echo • 32kHz Sample Clock";
    p.guiRoot.background = "snes_faceplate";
    p.guiRoot.accent = "#7A6CB8";

    // Row 1: S-DSP BRR Synthesis & Envelope
    GuiLayoutNode gOsc{};
    gOsc.type = GuiNodeType::Group;
    gOsc.title = "S-DSP BRR SYNTHESIS & HARDWARE ENVELOPE";
    GuiLayoutNode rOsc{};
    rOsc.type = GuiNodeType::Row;
    rOsc.align = "space_around";
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Waveform", "WAVEFORM", "", 52.0f, "twoToneStepped", false});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Attack", "ATTACK", "s", 52.0f, "chromeFluted", true});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Decay", "DECAY", "s", 52.0f, "chromeFluted", true});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "chromeFluted", true});
    rOsc.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Release", "RELEASE", "s", 52.0f, "chromeFluted", true});
    gOsc.children.push_back(rOsc);

    // Row 2: 8-Tap FIR Echo & Master Gain
    GuiLayoutNode gEcho{};
    gEcho.type = GuiNodeType::Group;
    gEcho.title = "8-TAP FIR HARDWARE ECHO & REVERB";
    GuiLayoutNode rEcho{};
    rEcho.type = GuiNodeType::Row;
    rEcho.align = "space_around";
    rEcho.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "EchoDelay", "ECHO DELAY", "ms", 52.0f, "twoToneStepped", false});
    rEcho.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "EchoFeedback", "FEEDBACK", "", 52.0f, "chromeFluted", true});
    rEcho.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "EchoVolume", "ECHO VOL", "", 52.0f, "chromeFluted", true});
    rEcho.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Volume", "MASTER VOL", "", 52.0f, "chromeFluted", true});
    gEcho.children.push_back(rEcho);

    p.guiRoot.children.push_back(gOsc);
    p.guiRoot.children.push_back(gEcho);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: snes_dsp\n"
        "# @name: SUPER NINTENDO S-DSP\n"
        "# @category: instrument\n"
        "# @engine: snes\n\n"
        "snesDsp = True\n\n"
        "def init():\n"
        "    params['Waveform'] = 1.0\n"
        "    params['Attack'] = 0.005\n"
        "    params['Decay'] = 0.25\n"
        "    params['Sustain'] = 0.40\n"
        "    params['Release'] = 0.20\n"
        "    params['EchoDelay'] = 120.0\n"
        "    params['EchoFeedback'] = 0.45\n"
        "    params['EchoVolume'] = 0.40\n"
        "    params['Volume'] = 0.85\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Sony SPC700 16-Bit S-DSP & 8-Tap FIR Echo\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createYm2612Preset() {
    PresetDefinition p{};
    p.metadata.id = "genesis_ym2612";
    p.metadata.name = "SEGA GENESIS YM2612";
    p.metadata.category = "instrument";
    p.metadata.description = "Iconic 1988 Sega Mega Drive 4-operator FM sound chip. Algorithm 4 slap bass with Operator 1 self-feedback.";
    p.metadata.engineId = "ym2612";

    p.params["Algorithm"] = {"Algorithm", 0.0f, 7.0f, 4.0f, 4.0f, 1.0f, "", false};
    p.params["Feedback"] = {"Feedback", 0.0f, 7.0f, 5.0f, 5.0f, 1.0f, "", false};
    p.params["Op1TL"] = {"Op1TL", 0.0f, 127.0f, 24.0f, 24.0f, 1.0f, "", true};
    p.params["Op2TL"] = {"Op2TL", 0.0f, 127.0f, 0.0f, 0.0f, 1.0f, "", true};
    p.params["Op3TL"] = {"Op3TL", 0.0f, 127.0f, 18.0f, 18.0f, 1.0f, "", true};
    p.params["Op4TL"] = {"Op4TL", 0.0f, 127.0f, 6.0f, 6.0f, 1.0f, "", true};
    p.params["Attack"] = {"Attack", 0.001f, 1.0f, 0.002f, 0.002f, 0.0f, "s", true};
    p.params["Decay"] = {"Decay", 0.01f, 1.0f, 0.35f, 0.35f, 0.0f, "s", true};
    p.params["Sustain"] = {"Sustain", 0.0f, 1.0f, 0.30f, 0.30f, 0.0f, "", true};
    p.params["Release"] = {"Release", 0.01f, 1.0f, 0.25f, 0.25f, 0.0f, "s", true};
    p.params["Volume"] = {"Volume", 0.0f, 2.0f, 0.80f, 0.80f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "SEGA GENESIS / MEGA DRIVE — YM2612";
    p.guiRoot.subtitle = "4-Operator OPN2 FM Sound Chip • High Definition Audio • 1988";
    p.guiRoot.background = "ym2612_faceplate";
    p.guiRoot.accent = "#E5A823";

    // Row 1: 4-Op FM Algorithm & Operator Total Level
    GuiLayoutNode gAlg{};
    gAlg.type = GuiNodeType::Group;
    gAlg.title = "4-OPERATOR FM MATRIX & OPERATOR LEVELS";
    GuiLayoutNode rAlg{};
    rAlg.type = GuiNodeType::Row;
    rAlg.align = "space_around";
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Algorithm", "ALGORITHM", "", 52.0f, "twoToneStepped", false});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Feedback", "FEEDBACK", "", 52.0f, "chromeFluted", false});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Op1TL", "OP1 LEVEL", "", 52.0f, "chromeFluted", true});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Op2TL", "OP2 LEVEL", "", 52.0f, "chromeFluted", true});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Op3TL", "OP3 LEVEL", "", 52.0f, "chromeFluted", true});
    rAlg.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Op4TL", "OP4 LEVEL", "", 52.0f, "chromeFluted", true});
    gAlg.children.push_back(rAlg);

    // Row 2: Global ADSR & Master Gain
    GuiLayoutNode gAdsr{};
    gAdsr.type = GuiNodeType::Group;
    gAdsr.title = "GLOBAL HARDWARE ADSR & MASTER GAIN";
    GuiLayoutNode rAdsr{};
    rAdsr.type = GuiNodeType::Row;
    rAdsr.align = "space_around";
    rAdsr.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Attack", "ATTACK", "s", 52.0f, "chromeFluted", true});
    rAdsr.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Decay", "DECAY", "s", 52.0f, "chromeFluted", true});
    rAdsr.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "chromeFluted", true});
    rAdsr.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Release", "RELEASE", "s", 52.0f, "chromeFluted", true});
    rAdsr.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Volume", "MASTER VOL", "", 52.0f, "chromeFluted", true});
    gAdsr.children.push_back(rAdsr);

    p.guiRoot.children.push_back(gAlg);
    p.guiRoot.children.push_back(gAdsr);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: genesis_ym2612\n"
        "# @name: SEGA GENESIS YM2612\n"
        "# @category: instrument\n"
        "# @engine: ym2612\n\n"
        "ym2612 = True\n\n"
        "def init():\n"
        "    params['Algorithm'] = 4.0\n"
        "    params['Feedback'] = 5.0\n"
        "    params['Op1TL'] = 24.0\n"
        "    params['Op2TL'] = 0.0\n"
        "    params['Op3TL'] = 18.0\n"
        "    params['Op4TL'] = 6.0\n"
        "    params['Attack'] = 0.002\n"
        "    params['Decay'] = 0.35\n"
        "    params['Sustain'] = 0.30\n"
        "    params['Release'] = 0.25\n"
        "    params['Volume'] = 0.80\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Yamaha YM2612 OPN2 4-Operator FM Chip\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createConvolverPreset() {
    PresetDefinition p{};
    p.metadata.id = "convolver_space";
    p.metadata.name = "ACOUSTIC CONVOLUTION REVERB";
    p.metadata.category = "audio_fx";
    p.metadata.description = "Zero-latency real-time convolution reverb with physics-based procedural room simulation and speaker cabinet emulation.";
    p.metadata.engineId = "convolver";

    p.params["Preset"] = {"Preset", 0.0f, 12.0f, 1.0f, 1.0f, 1.0f, "", false};
    p.params["PreDelay"] = {"PreDelay", 0.0f, 100.0f, 12.0f, 12.0f, 0.0f, "ms", true};
    p.params["Decay"] = {"Decay", 0.1f, 2.5f, 1.0f, 1.0f, 0.0f, "x", true};
    p.params["HighCut"] = {"HighCut", 500.0f, 20000.0f, 8500.0f, 8500.0f, 0.0f, "Hz", true};
    p.params["LowCut"] = {"LowCut", 20.0f, 2000.0f, 80.0f, 80.0f, 0.0f, "Hz", true};
    p.params["Mix"] = {"Mix", 0.0f, 1.0f, 0.35f, 0.35f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "CONVOLUTION REVERB & SPACE DESIGNER";
    p.guiRoot.subtitle = "Procedural Geometric ISM • Velvet Noise Tail • Speaker Cabinet Sim";
    p.guiRoot.background = "convolver_faceplate";
    p.guiRoot.accent = "#00E5FF";

    // Row 1: Space & Geometry
    GuiLayoutNode gSpace{};
    gSpace.type = GuiNodeType::Group;
    gSpace.title = "ACOUSTIC SPACE & GEOMETRY";
    GuiLayoutNode rSpace{};
    rSpace.type = GuiNodeType::Row;
    rSpace.align = "space_around";
    rSpace.children.push_back({GuiNodeType::Knob, "", "", "", "", "vintage", "", "", "", "Preset", "SPACE / CAB", "", 52.0f, "twoToneStepped", false});
    rSpace.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "PreDelay", "PRE-DELAY", "ms", 52.0f, "chromeFluted", true});
    rSpace.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Decay", "DECAY TIME", "x", 52.0f, "chromeFluted", true});
    gSpace.children.push_back(rSpace);

    // Row 2: Filtering & Mix
    GuiLayoutNode gFilter{};
    gFilter.type = GuiNodeType::Group;
    gFilter.title = "DAMPING FILTER & BLEND";
    GuiLayoutNode rFilter{};
    rFilter.type = GuiNodeType::Row;
    rFilter.align = "space_around";
    rFilter.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "HighCut", "HIGH DAMP", "Hz", 52.0f, "chromeFluted", true});
    rFilter.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "LowCut", "LOW CUT", "Hz", 52.0f, "chromeFluted", true});
    rFilter.children.push_back({GuiNodeType::Knob, "", "", "", "", "chrome", "", "", "", "Mix", "WET / DRY", "", 52.0f, "chromeFluted", true});
    gFilter.children.push_back(rFilter);

    p.guiRoot.children.push_back(gSpace);
    p.guiRoot.children.push_back(gFilter);

    p.rawScript =
        "# Eatsbeats Audio FX Definition\n"
        "# @id: convolver_space\n"
        "# @name: ACOUSTIC CONVOLUTION REVERB\n"
        "# @category: audio_fx\n"
        "# @engine: convolver\n\n"
        "Convolver = True\n\n"
        "def init():\n"
        "    params['Preset'] = 1.0\n"
        "    params['PreDelay'] = 12.0\n"
        "    params['Decay'] = 1.0\n"
        "    params['HighCut'] = 8500.0\n"
        "    params['LowCut'] = 80.0\n"
        "    params['Mix'] = 0.35\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Geometric ISM Velvet Noise Convolution Engine\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createConcertGrandPianoPreset() {
    PresetDefinition p{};
    p.metadata.id = "concert_grand_piano";
    p.metadata.name = "CONCERT GRAND PIANO";
    p.metadata.category = "instrument";
    p.metadata.description = "Physical modeling of a 9-foot Concert Grand Piano: non-linear felt hammer compression, coupled trichord string waveguides with inharmonic dispersion, 2D spruce soundboard modal resonance, and sympathetic pedal resonance.";
    p.metadata.engineId = "piano_physical";

    p.params["HammerHardness"] = {"HammerHardness", 0.1f, 3.0f, 0.85f, 0.85f, 0.0f, "", true};
    p.params["Brightness"] = {"Brightness", 0.0f, 1.0f, 0.50f, 0.50f, 0.0f, "", true};
    p.params["Sustain"] = {"Sustain", 0.5f, 1.2f, 1.0f, 1.0f, 0.0f, "", true};
    p.params["PedalReso"] = {"PedalReso", 0.0f, 2.0f, 0.55f, 0.55f, 0.0f, "", true};
    p.params["LidOpen"] = {"LidOpen", -6.0f, 6.0f, 2.0f, 2.0f, 0.0f, "dB", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "CONCERT GRAND PIANO";
    p.guiRoot.subtitle = "Stanford CCRMA / Bank-Bensa Commuted Waveguide Model";
    p.guiRoot.accent = "#D4AF37";
    p.guiRoot.background = "piano_ebony";
    p.guiRoot.rackSides = "rosewood";

    GuiLayoutNode gHammer{};
    gHammer.type = GuiNodeType::Group;
    gHammer.title = "HAMMER MECHANICS & FELT";
    GuiLayoutNode rHammer{};
    rHammer.type = GuiNodeType::Row;
    rHammer.align = "space_around";
    rHammer.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "HammerHardness", "HARDNESS", "", 52.0f, "vintage", true});
    rHammer.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "Brightness", "BRIGHTNESS", "", 52.0f, "vintage", true});
    gHammer.children.push_back(rHammer);

    GuiLayoutNode gBody{};
    gBody.type = GuiNodeType::Group;
    gBody.title = "SOUNDBOARD & RESONANCE";
    GuiLayoutNode rBody{};
    rBody.type = GuiNodeType::Row;
    rBody.align = "space_around";
    rBody.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "vintage", true});
    rBody.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "PedalReso", "PEDAL RESO", "", 52.0f, "vintage", true});
    rBody.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "LidOpen", "LID OPEN", "dB", 52.0f, "vintage", true});
    gBody.children.push_back(rBody);

    p.guiRoot.children.push_back(gHammer);
    p.guiRoot.children.push_back(gBody);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: concert_grand_piano\n"
        "# @name: CONCERT GRAND PIANO\n"
        "# @category: instrument\n"
        "# @engine: piano_physical\n\n"
        "ConcertGrandPiano = True\n\n"
        "def init():\n"
        "    params['HammerHardness'] = 0.85\n"
        "    params['Brightness'] = 0.50\n"
        "    params['Sustain'] = 1.0\n"
        "    params['PedalReso'] = 0.55\n"
        "    params['LidOpen'] = 2.0\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Bank-Bensa Commuted Waveguide Concert Grand Model\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createUprightBassPreset() {
    PresetDefinition p{};
    p.metadata.id = "upright_bass";
    p.metadata.name = "UPRIGHT DOUBLE BASS";
    p.metadata.category = "instrument";
    p.metadata.description = "Physical model of an acoustic 3/4 Upright Double Bass: heavy gut string side-finger pull (Hunt-Crossley elastodynamics), 4-point Hermite cubic waveguide with inharmonic dispersion, non-linear ebony fingerboard collision (fretless growl & slap), and 3/4 carved spruce body modal cavity resonator.";
    p.metadata.engineId = "upright_bass";

    p.params["FingerMass"] = {"FingerMass", 0.5f, 4.0f, 2.2f, 2.2f, 0.0f, "", true};
    p.params["SlapClick"] = {"SlapClick", 0.0f, 2.0f, 0.35f, 0.35f, 0.0f, "", true};
    p.params["Sustain"] = {"Sustain", 0.85f, 0.999f, 0.992f, 0.992f, 0.0f, "", true};
    p.params["StringDamp"] = {"StringDamp", 0.02f, 0.95f, 0.28f, 0.28f, 0.0f, "", true};
    p.params["Dispersion"] = {"Dispersion", 0.0f, 0.85f, 0.22f, 0.22f, 0.0f, "", true};
    p.params["ActionHeight"] = {"ActionHeight", 1.0f, 8.0f, 3.2f, 3.2f, 0.0f, "mm", true};
    p.params["BodyWarmth"] = {"BodyWarmth", 0.1f, 2.0f, 0.75f, 0.75f, 0.0f, "", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "UPRIGHT DOUBLE BASS";
    p.guiRoot.subtitle = "3/4 Carved Spruce Cavity, Gut Waveguide & Ebony Slap";
    p.guiRoot.accent = "#FF9933";
    p.guiRoot.background = "warm_amber";
    p.guiRoot.rackSides = "rosewood";

    GuiLayoutNode gPluck{};
    gPluck.type = GuiNodeType::Group;
    gPluck.title = "FINGERTIP PLUCK & SLAP";
    GuiLayoutNode rPluck{};
    rPluck.type = GuiNodeType::Row;
    rPluck.align = "space_around";
    rPluck.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "FingerMass", "FINGER MASS", "", 52.0f, "vintage", true});
    rPluck.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "SlapClick", "SLAP / GROWL", "", 52.0f, "vintage", true});
    rPluck.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "ActionHeight", "ACTION", "mm", 52.0f, "vintage", true});
    gPluck.children.push_back(rPluck);

    GuiLayoutNode gWaveguide{};
    gWaveguide.type = GuiNodeType::Group;
    gWaveguide.title = "GUT WAVEGUIDE & BODY CAVITY";
    GuiLayoutNode rWaveguide{};
    rWaveguide.type = GuiNodeType::Row;
    rWaveguide.align = "space_around";
    rWaveguide.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "vintage", true});
    rWaveguide.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "StringDamp", "DAMPING", "", 52.0f, "vintage", true});
    rWaveguide.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "Dispersion", "DISPERSION", "", 52.0f, "vintage", true});
    rWaveguide.children.push_back({GuiNodeType::Knob, "", "", "", "", "amber", "", "", "", "BodyWarmth", "BODY WARMTH", "", 52.0f, "vintage", true});
    gWaveguide.children.push_back(rWaveguide);

    p.guiRoot.children.push_back(gPluck);
    p.guiRoot.children.push_back(gWaveguide);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: upright_bass\n"
        "# @name: UPRIGHT DOUBLE BASS\n"
        "# @category: instrument\n"
        "# @engine: upright_bass\n\n"
        "DoubleBass = True\n\n"
        "def init():\n"
        "    params['FingerMass'] = 2.2\n"
        "    params['SlapClick'] = 0.35\n"
        "    params['Sustain'] = 0.992\n"
        "    params['StringDamp'] = 0.28\n"
        "    params['Dispersion'] = 0.22\n"
        "    params['ActionHeight'] = 3.2\n"
        "    params['BodyWarmth'] = 0.75\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Hunt-Crossley Gut Waveguide & Cavity Resonator\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createSpanishGuitarPreset() {
    PresetDefinition p{};
    p.metadata.id = "spanish_guitar";
    p.metadata.name = "SPANISH CLASSICAL GUITAR";
    p.metadata.category = "instrument";
    p.metadata.description = "Physical modeling of a Spanish concert classical guitar: nylon trebles & silver-wound bass waveguides, fingertip flesh vs fingernail ramp attack, Spanish Torres fan-braced acoustic body resonator (98Hz Helmholtz air cavity & 196Hz soundboard plate), and cedar/spruce tone balance.";
    p.metadata.engineId = "spanish_guitar";

    p.params["FleshNail"] = {"FleshNail", 0.0f, 1.0f, 0.40f, 0.40f, 0.0f, "", true};
    p.params["StrumSpread"] = {"StrumSpread", 1.0f, 40.0f, 4.0f, 4.0f, 0.0f, "ms", true};
    p.params["Sustain"] = {"Sustain", 0.85f, 0.999f, 0.9955f, 0.9955f, 0.0f, "", true};
    p.params["BodyDamp"] = {"BodyDamp", 0.01f, 0.95f, 0.24f, 0.24f, 0.0f, "", true};
    p.params["WoodTone"] = {"WoodTone", -6.0f, 6.0f, 1.5f, 1.5f, 0.0f, "dB", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "SPANISH CLASSICAL GUITAR";
    p.guiRoot.subtitle = "Spanish Fan-Bracing & Nylon Waveguide Physical Model";
    p.guiRoot.accent = "#E5A65E";
    p.guiRoot.background = "spanish_cedar";
    p.guiRoot.rackSides = "rosewood";

    GuiLayoutNode gPluck{};
    gPluck.type = GuiNodeType::Group;
    gPluck.title = "PLUCK MECHANICS";
    GuiLayoutNode rPluck{};
    rPluck.type = GuiNodeType::Row;
    rPluck.align = "space_around";
    rPluck.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "FleshNail", "FLESH / NAIL", "", 52.0f, "vintage", true});
    rPluck.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "StrumSpread", "STRUM SPREAD", "ms", 52.0f, "vintage", true});
    gPluck.children.push_back(rPluck);

    GuiLayoutNode gBody{};
    gBody.type = GuiNodeType::Group;
    gBody.title = "TORRES FAN-BRACED BODY";
    GuiLayoutNode rBody{};
    rBody.type = GuiNodeType::Row;
    rBody.align = "space_around";
    rBody.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "vintage", true});
    rBody.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "BodyDamp", "BODY DAMP", "", 52.0f, "vintage", true});
    rBody.children.push_back({GuiNodeType::Knob, "", "", "", "", "gold", "", "", "", "WoodTone", "WOOD TONE", "dB", 52.0f, "vintage", true});
    gBody.children.push_back(rBody);

    p.guiRoot.children.push_back(gPluck);
    p.guiRoot.children.push_back(gBody);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: spanish_guitar\n"
        "# @name: SPANISH CLASSICAL GUITAR\n"
        "# @category: instrument\n"
        "# @engine: spanish_guitar\n\n"
        "SpanishGuitar = True\n\n"
        "def init():\n"
        "    params['FleshNail'] = 0.40\n"
        "    params['StrumSpread'] = 4.0\n"
        "    params['Sustain'] = 0.9955\n"
        "    params['BodyDamp'] = 0.24\n"
        "    params['WoodTone'] = 1.5\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Torres Fan-Braced Classical Guitar Physical Model\n"
        "    pass\n";

    return p;
}

PresetDefinition PresetLoader::createSteelAcousticGuitarPreset() {
    PresetDefinition p{};
    p.metadata.id = "acoustic_steel_guitar";
    p.metadata.name = "STEEL STRING ACOUSTIC GUITAR";
    p.metadata.category = "instrument";
    p.metadata.description = "Physical modeling of a steel-string acoustic guitar: phosphor bronze string waveguide, flatpick vs fingerpick attack, and dreadnought body resonance.";
    p.metadata.engineId = "acoustic_steel_guitar";

    p.params["PickBite"] = {"PickBite", 0.0f, 3.0f, 1.0f, 1.0f, 0.0f, "", true};
    p.params["StrumSpread"] = {"StrumSpread", 1.0f, 40.0f, 8.0f, 8.0f, 0.0f, "ms", true};
    p.params["Sustain"] = {"Sustain", 0.85f, 0.999f, 0.995f, 0.995f, 0.0f, "", true};
    p.params["BodyDamp"] = {"BodyDamp", 0.01f, 0.95f, 0.20f, 0.20f, 0.0f, "", true};
    p.params["AirSparkle"] = {"AirSparkle", -6.0f, 12.0f, 2.0f, 2.0f, 0.0f, "dB", true};

    p.guiRoot.type = GuiNodeType::Panel;
    p.guiRoot.title = "STEEL STRING ACOUSTIC GUITAR";
    p.guiRoot.subtitle = "Phosphor Bronze Waveguide & Dreadnought Cavity";
    p.guiRoot.accent = "#CD7F32";
    p.guiRoot.background = "spruce_top";
    p.guiRoot.rackSides = "rosewood";

    GuiLayoutNode gPick{};
    gPick.type = GuiNodeType::Group;
    gPick.title = "PLECTRUM ATTACK";
    GuiLayoutNode rPick{};
    rPick.type = GuiNodeType::Row;
    rPick.align = "space_around";
    rPick.children.push_back({GuiNodeType::Knob, "", "", "", "", "bronze", "", "", "", "PickBite", "PICK BITE", "", 52.0f, "vintage", true});
    rPick.children.push_back({GuiNodeType::Knob, "", "", "", "", "bronze", "", "", "", "StrumSpread", "STRUM SPREAD", "ms", 52.0f, "vintage", true});
    gPick.children.push_back(rPick);

    GuiLayoutNode gReso{};
    gReso.type = GuiNodeType::Group;
    gReso.title = "DREADNOUGHT BODY RESONANCE";
    GuiLayoutNode rReso{};
    rReso.type = GuiNodeType::Row;
    rReso.align = "space_around";
    rReso.children.push_back({GuiNodeType::Knob, "", "", "", "", "bronze", "", "", "", "Sustain", "SUSTAIN", "", 52.0f, "vintage", true});
    rReso.children.push_back({GuiNodeType::Knob, "", "", "", "", "bronze", "", "", "", "BodyDamp", "DAMPING", "", 52.0f, "vintage", true});
    rReso.children.push_back({GuiNodeType::Knob, "", "", "", "", "bronze", "", "", "", "AirSparkle", "AIR SPARKLE", "dB", 52.0f, "vintage", true});
    gReso.children.push_back(rReso);

    p.guiRoot.children.push_back(gPick);
    p.guiRoot.children.push_back(gReso);

    p.rawScript =
        "# Eatsbeats Instrument Definition\n"
        "# @id: acoustic_steel_guitar\n"
        "# @name: STEEL STRING ACOUSTIC GUITAR\n"
        "# @category: instrument\n"
        "# @engine: acoustic_steel_guitar\n\n"
        "SteelAcousticGuitar = True\n\n"
        "def init():\n"
        "    params['PickBite'] = 1.0\n"
        "    params['StrumSpread'] = 8.0\n"
        "    params['Sustain'] = 0.995\n"
        "    params['BodyDamp'] = 0.20\n"
        "    params['AirSparkle'] = 2.0\n\n"
        "def process(time, freq, note, params):\n"
        "    # Native Phosphor Bronze Waveguide & Dreadnought Model\n"
        "    pass\n";

    return p;
}

std::vector<PresetDefinition> PresetLoader::getBuiltinPresets() {
    return {
        createEats303Preset(),
        createC64SidPreset(),
        createYamahaDx7Preset(),
        createSnesPreset(),
        createYm2612Preset(),
        createConvolverPreset(),
        createAnalog808KickPreset(),
        createAnalog909SnarePreset(),
        createStereoDelayPreset(),
        createConcertGrandPianoPreset(),
        createUprightBassPreset(),
        createSpanishGuitarPreset(),
        createSteelAcousticGuitarPreset()
    };
}

void PresetLoader::computeLayoutBounds(GuiLayoutNode& node, float x, float y, float availableW, float availableH) {
    node.boundsX = x;
    node.boundsY = y;
    node.boundsW = availableW;
    node.boundsH = availableH;

    if (node.type == GuiNodeType::Panel) {
        // Leave room for top banner header (title, subtitle, preset switcher)
        float headerH = (availableH <= 250.0f) ? 28.0f : 54.0f;
        float contentY = y + headerH;
        float contentH = std::max(availableH - headerH, 80.0f);

        size_t n = node.children.size();
        if (n > 0) {
            float rowH = contentH / static_cast<float>(n);
            for (size_t i = 0; i < n; ++i) {
                computeLayoutBounds(node.children[i], x + 20.0f, contentY + i * rowH, availableW - 40.0f, rowH);
            }
        }
    } else if (node.type == GuiNodeType::Row) {
        size_t n = node.children.size();
        if (n > 0) {
            float slotW = availableW / static_cast<float>(n);
            for (size_t i = 0; i < n; ++i) {
                computeLayoutBounds(node.children[i], x + i * slotW, y, slotW, availableH);
            }
        }
    } else if (node.type == GuiNodeType::Column) {
        size_t n = node.children.size();
        if (n > 0) {
            float slotH = availableH / static_cast<float>(n);
            for (size_t i = 0; i < n; ++i) {
                computeLayoutBounds(node.children[i], x, y + i * slotH, availableW, slotH);
            }
        }
    } else if (node.type == GuiNodeType::Group) {
        float headerH = 28.0f;
        float innerY = y + headerH;
        float innerH = std::max(availableH - headerH - 8.0f, 40.0f);
        size_t n = node.children.size();
        if (n > 0) {
            float slotW = (availableW - 20.0f) / static_cast<float>(n);
            for (size_t i = 0; i < n; ++i) {
                computeLayoutBounds(node.children[i], x + 10.0f + i * slotW, innerY, slotW, innerH);
            }
        }
    } else if (node.type == GuiNodeType::Knob || node.type == GuiNodeType::Nixie) {
        // Centered within available cell
        node.boundsW = availableW;
        node.boundsH = availableH;
    }
}

} // namespace eatsbits::project
