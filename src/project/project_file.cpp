#include "eatsbits/project/project_file.hpp"
#include "eatsbits/project/eats_serializer.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/biquad_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>
#include <variant>
#include <cctype>

namespace eatsbits::project {

// =========================================================================
// Minimalistic Embedded JSON Parser & Serializer (Zero External Dependencies)
// =========================================================================
namespace json {

enum class Type { Null, Bool, Number, String, Array, Object };

struct Value;
using Object = std::map<std::string, Value>;
using Array = std::vector<Value>;

struct Value {
    Type type{Type::Null};
    std::variant<std::monostate, bool, double, std::string, Array, Object> data;

    Value() = default;
    Value(std::nullptr_t) : type(Type::Null) {}
    Value(bool b) : type(Type::Bool), data(b) {}
    Value(int n) : type(Type::Number), data(static_cast<double>(n)) {}
    Value(unsigned int n) : type(Type::Number), data(static_cast<double>(n)) {}
    Value(double n) : type(Type::Number), data(n) {}
    Value(const char* s) : type(Type::String), data(std::string(s)) {}
    Value(std::string s) : type(Type::String), data(std::move(s)) {}
    Value(Array a) : type(Type::Array), data(std::move(a)) {}
    Value(Object o) : type(Type::Object), data(std::move(o)) {}

    [[nodiscard]] bool isObject() const noexcept { return type == Type::Object; }
    [[nodiscard]] bool isArray() const noexcept { return type == Type::Array; }
    [[nodiscard]] bool isString() const noexcept { return type == Type::String; }
    [[nodiscard]] bool isNumber() const noexcept { return type == Type::Number; }
    [[nodiscard]] bool isBool() const noexcept { return type == Type::Bool; }

    [[nodiscard]] const Object& asObject() const { return std::get<Object>(data); }
    [[nodiscard]] const Array& asArray() const { return std::get<Array>(data); }
    [[nodiscard]] const std::string& asString() const { return std::get<std::string>(data); }
    [[nodiscard]] double asDouble(double def = 0.0) const noexcept {
        return isNumber() ? std::get<double>(data) : def;
    }
    [[nodiscard]] int asInt(int def = 0) const noexcept {
        return isNumber() ? static_cast<int>(std::get<double>(data)) : def;
    }
    [[nodiscard]] bool asBool(bool def = false) const noexcept {
        return isBool() ? std::get<bool>(data) : def;
    }

    [[nodiscard]] bool contains(const std::string& key) const noexcept {
        if (!isObject()) return false;
        const auto& obj = asObject();
        return obj.find(key) != obj.end();
    }

    [[nodiscard]] const Value& operator[](const std::string& key) const {
        static const Value nullVal;
        if (!isObject()) return nullVal;
        const auto& obj = asObject();
        auto it = obj.find(key);
        return (it != obj.end()) ? it->second : nullVal;
    }
};

static std::string escapeString(const std::string& s) {
    std::ostringstream o;
    o << '"';
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) <= 0x1f) {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    o << '"';
    return o.str();
}

static std::string stringify(const Value& val, int indent = 0) {
    std::string ind(indent * 2, ' ');
    switch (val.type) {
        case Type::Null: return "null";
        case Type::Bool: return val.asBool() ? "true" : "false";
        case Type::Number: {
            std::ostringstream ss;
            ss << std::setprecision(8) << val.asDouble();
            return ss.str();
        }
        case Type::String: return escapeString(val.asString());
        case Type::Array: {
            const auto& arr = val.asArray();
            if (arr.empty()) return "[]";
            std::string s = "[\n";
            for (size_t i = 0; i < arr.size(); ++i) {
                s += ind + "  " + stringify(arr[i], indent + 1);
                if (i + 1 < arr.size()) s += ",";
                s += "\n";
            }
            s += ind + "]";
            return s;
        }
        case Type::Object: {
            const auto& obj = val.asObject();
            if (obj.empty()) return "{}";
            std::string s = "{\n";
            size_t i = 0;
            for (const auto& [k, v] : obj) {
                s += ind + "  " + escapeString(k) + ": " + stringify(v, indent + 1);
                if (++i < obj.size()) s += ",";
                s += "\n";
            }
            s += ind + "}";
            return s;
        }
    }
    return "null";
}

class Parser {
public:
    explicit Parser(std::string text) : src_(std::move(text)) {}

    Value parse() {
        skipWhitespace();
        return parseValue();
    }

private:
    void skipWhitespace() {
        while (pos_ < src_.size() && (std::isspace(src_[pos_]) || src_[pos_] == '\r' || src_[pos_] == '\n')) {
            pos_++;
        }
    }

    Value parseValue() {
        skipWhitespace();
        if (pos_ >= src_.size()) return Value{};

        char c = src_[pos_];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || std::isdigit(c)) return parseNumber();

        // Advance past unhandled character to avoid stalling
        pos_++;
        return Value{};
    }

    Value parseObject() {
        pos_++; // skip '{'
        Object obj;
        skipWhitespace();
        if (pos_ < src_.size() && src_[pos_] == '}') {
            pos_++;
            return Value{obj};
        }

        while (pos_ < src_.size()) {
            skipWhitespace();
            if (pos_ < src_.size() && src_[pos_] == '}') {
                pos_++;
                break;
            }
            if (src_[pos_] != '"') {
                pos_++;
                continue;
            }
            std::string key = parseString().asString();
            skipWhitespace();
            if (pos_ < src_.size() && src_[pos_] == ':') pos_++;
            skipWhitespace();
            Value val = parseValue();
            obj[std::move(key)] = std::move(val);

            skipWhitespace();
            if (pos_ < src_.size() && src_[pos_] == ',') {
                pos_++;
            } else if (pos_ < src_.size() && src_[pos_] == '}') {
                pos_++;
                break;
            } else {
                if (pos_ < src_.size()) pos_++;
            }
        }
        return Value{obj};
    }

    Value parseArray() {
        pos_++; // skip '['
        Array arr;
        skipWhitespace();
        if (pos_ < src_.size() && src_[pos_] == ']') {
            pos_++;
            return Value{arr};
        }

        while (pos_ < src_.size()) {
            skipWhitespace();
            if (pos_ < src_.size() && src_[pos_] == ']') {
                pos_++;
                break;
            }
            arr.push_back(parseValue());
            skipWhitespace();
            if (pos_ < src_.size() && src_[pos_] == ',') {
                pos_++;
            } else if (pos_ < src_.size() && src_[pos_] == ']') {
                pos_++;
                break;
            } else {
                if (pos_ < src_.size()) pos_++;
            }
        }
        return Value{arr};
    }

    Value parseString() {
        pos_++; // skip opening '"'
        std::string s;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') break;
            if (c == '\\' && pos_ < src_.size()) {
                char esc = src_[pos_++];
                switch (esc) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        // basic 4-digit hex escape
                        if (pos_ + 4 <= src_.size()) {
                            std::string hexStr = src_.substr(pos_, 4);
                            pos_ += 4;
                            int code = std::stoi(hexStr, nullptr, 16);
                            if (code < 128) s += static_cast<char>(code);
                        }
                        break;
                    }
                    default: s += esc; break;
                }
            } else {
                s += c;
            }
        }
        return Value{s};
    }

    Value parseNumber() {
        size_t start = pos_;
        if (pos_ < src_.size() && src_[pos_] == '-') pos_++;
        while (pos_ < src_.size() && (std::isdigit(src_[pos_]) || src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' || src_[pos_] == '+' || src_[pos_] == '-')) {
            pos_++;
        }
        double num = std::stod(src_.substr(start, pos_ - start));
        return Value{num};
    }

    Value parseBool() {
        if (src_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return Value{true};
        }
        if (src_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return Value{false};
        }
        return Value{false};
    }

    Value parseNull() {
        if (src_.substr(pos_, 4) == "null") {
            pos_ += 4;
        }
        return Value{};
    }

    std::string src_;
    size_t pos_{0};
};

} // namespace json

// =========================================================================
// Project Serialization Implementation
// =========================================================================

std::string ProjectFile::serializeJson(const audio::AudioGraph& graph,
                                       const sequencer::StepSequencer& sequencer,
                                       const std::string& title,
                                       double bpm,
                                       double swing,
                                       int songKeyRoot,
                                       bool isSongKeyMinor,
                                       const std::vector<theory::ChordEvent>& chordTrack) {
    json::Object root;
    root["format"] = "eatsbits";
    root["version"] = 1;
    root["title"] = title;
    root["bpm"] = bpm;
    root["swing"] = swing;
    root["songKeyRoot"] = static_cast<double>(songKeyRoot);
    root["isSongKeyMinor"] = isSongKeyMinor;

    if (!chordTrack.empty()) {
        json::Array chordsArr;
        for (const auto& c : chordTrack) {
            json::Object cObj;
            cObj["id"] = c.id;
            cObj["startBar"] = static_cast<double>(c.startBar);
            cObj["barLength"] = static_cast<double>(c.barLength);
            cObj["rootPitchClass"] = static_cast<double>(c.rootPitchClass);
            cObj["quality"] = static_cast<double>(static_cast<int>(c.quality));
            cObj["bassPitchClass"] = static_cast<double>(c.bassPitchClass);
            chordsArr.push_back(std::move(cObj));
        }
        root["chordTrack"] = std::move(chordsArr);
    }

    // 1. Serialize Graph
    json::Object graphObj;
    graphObj["outputNodeId"] = static_cast<double>(graph.getOutputNodeId());

    json::Array nodesArr;
    const auto& nodes = graph.getNodes();
    for (const auto& [id, node] : nodes) {
        if (!node) continue;
        json::Object nObj;
        nObj["id"] = static_cast<double>(id);
        nObj["name"] = node->getName();

        // Identify node type and custom params
        if (auto* tb = dynamic_cast<audio::Tb303Node*>(node.get())) {
            nObj["type"] = "tb303";
        } else if (auto* dk = dynamic_cast<audio::DrumKitNode*>(node.get())) {
            nObj["type"] = "drum_kit";
        } else if (auto* ps = dynamic_cast<audio::PolySynthNode*>(node.get())) {
            nObj["type"] = "poly_synth";
        } else if (auto* bq = dynamic_cast<audio::BiquadNode*>(node.get())) {
            nObj["type"] = "biquad";
        } else if (auto* dl = dynamic_cast<audio::DelayNode*>(node.get())) {
            nObj["type"] = "delay";
            nObj["delayTimeMs"] = dl->getDelayTimeMs();
            nObj["feedback"] = dl->getFeedback();
            nObj["dryWet"] = dl->getDryWet();
        } else if (auto* gn = dynamic_cast<audio::GainNode*>(node.get())) {
            nObj["type"] = "gain";
            nObj["volume"] = gn->getVolume();
            nObj["pan"] = gn->getPan();
            nObj["muted"] = gn->isMuted();
        } else if (auto* es = dynamic_cast<audio::EatscriptNode*>(node.get())) {
            nObj["type"] = "eatscript";
            nObj["script"] = es->getScript();
        } else {
            nObj["type"] = "generic";
        }

        nodesArr.push_back(std::move(nObj));
    }
    graphObj["nodes"] = std::move(nodesArr);

    // Connections
    json::Array connArr;
    for (const auto& conn : graph.getConnections()) {
        json::Object cObj;
        cObj["srcNode"] = static_cast<double>(conn.srcNode);
        cObj["srcPort"] = static_cast<double>(conn.srcPort);
        cObj["dstNode"] = static_cast<double>(conn.dstNode);
        cObj["dstPort"] = static_cast<double>(conn.dstPort);
        connArr.push_back(std::move(cObj));
    }
    graphObj["connections"] = std::move(connArr);
    root["graph"] = std::move(graphObj);

    // 2. Serialize Sequencer
    json::Object seqObj;
    seqObj["activePattern"] = static_cast<double>(sequencer.getActivePatternIndex());

    json::Array patternsArr;
    for (size_t p = 0; p < sequencer.getNumPatterns(); ++p) {
        const auto& pat = sequencer.getPattern(p);
        json::Object pObj;
        pObj["name"] = pat.name;

        json::Array tracksArr;
        for (const auto& track : pat.tracks) {
            json::Object tObj;
            tObj["name"] = track.getName();
            tObj["icon"] = track.getIconRef();
            tObj["targetNodeId"] = static_cast<double>(track.getTargetNodeId());
            tObj["numSteps"] = static_cast<double>(track.getNumSteps());
            tObj["muted"] = track.isMuted();
            tObj["solo"] = track.isSolo();
            tObj["transpose"] = static_cast<double>(track.getTranspose());

            json::Array stepsArr;
            for (uint32_t s = 0; s < track.getNumSteps(); ++s) {
                const auto& step = track.getStep(s);
                if (!step.active) continue;

                json::Object sObj;
                sObj["step"] = static_cast<double>(s);
                sObj["note"] = static_cast<double>(step.note);
                sObj["vel"] = static_cast<double>(step.velocity);
                sObj["gate"] = static_cast<double>(step.gateLength);
                sObj["slide"] = step.slide;
                sObj["accent"] = step.accent;
                sObj["prob"] = static_cast<double>(step.probability);
                if (step.paramLockId > 0) {
                    sObj["pId"] = static_cast<double>(step.paramLockId);
                    sObj["pVal"] = static_cast<double>(step.paramLockValue);
                }
                stepsArr.push_back(std::move(sObj));
            }
            tObj["steps"] = std::move(stepsArr);
            tracksArr.push_back(std::move(tObj));
        }
        pObj["tracks"] = std::move(tracksArr);
        patternsArr.push_back(std::move(pObj));
    }
    seqObj["patterns"] = std::move(patternsArr);
    root["sequencer"] = std::move(seqObj);

    return json::stringify(json::Value{root});
}

bool ProjectFile::deserializeJson(const std::string& jsonStr,
                                  audio::AudioGraph& graph,
                                  sequencer::StepSequencer& sequencer,
                                  std::string& outTitle,
                                  double& outBpm,
                                  double& outSwing) {
    int dummyKey = 0;
    bool dummyMinor = false;
    std::vector<theory::ChordEvent> dummyChords;
    return deserializeJson(jsonStr, graph, sequencer, outTitle, outBpm, outSwing,
                           dummyKey, dummyMinor, dummyChords);
}

bool ProjectFile::deserializeJson(const std::string& jsonStr,
                                  audio::AudioGraph& graph,
                                  sequencer::StepSequencer& sequencer,
                                  std::string& outTitle,
                                  double& outBpm,
                                  double& outSwing,
                                  int& outSongKeyRoot,
                                  bool& outIsSongKeyMinor,
                                  std::vector<theory::ChordEvent>& outChordTrack) {
    json::Parser parser(jsonStr);
    json::Value root = parser.parse();
    if (!root.isObject()) {
        return false;
    }

    outTitle = root["title"].isString() ? root["title"].asString() : "Untitled";
    outBpm = root["bpm"].asDouble(135.0);
    outSwing = root["swing"].asDouble(0.50);

    outSongKeyRoot = root["songKeyRoot"].isNumber() ? root["songKeyRoot"].asInt(0) : 0;
    outIsSongKeyMinor = root["isSongKeyMinor"].isBool() ? root["isSongKeyMinor"].asBool() : false;

    outChordTrack.clear();
    const auto& chordsArrVal = root["chordTrack"];
    if (chordsArrVal.isArray()) {
        for (const auto& item : chordsArrVal.asArray()) {
            if (!item.isObject()) continue;
            theory::ChordEvent c;
            c.id = item["id"].isString() ? item["id"].asString() : "chord";
            c.startBar = static_cast<uint32_t>(item["startBar"].asInt(0));
            c.barLength = static_cast<float>(item["barLength"].asDouble(1.0));
            c.rootPitchClass = item["rootPitchClass"].asInt(0);
            c.quality = static_cast<theory::ChordQuality>(item["quality"].asInt(0));
            c.bassPitchClass = item["bassPitchClass"].asInt(-1);
            outChordTrack.push_back(c);
        }
    }

    sequencer.setBpm(outBpm);
    sequencer.setSwing(outSwing);

    // 1. Rebuild Graph
    graph.clear();
    std::map<uint32_t, audio::NodeId> oldToNewId;

    const auto& graphVal = root["graph"];
    if (graphVal.isObject()) {
        const auto& nodesVal = graphVal["nodes"];
        if (nodesVal.isArray()) {
            for (const auto& nVal : nodesVal.asArray()) {
                if (!nVal.isObject()) continue;
                uint32_t oldId = static_cast<uint32_t>(nVal["id"].asInt(0));
                std::string type = nVal["type"].asString();
                std::string name = nVal["name"].asString();

                std::shared_ptr<audio::GraphNode> newNode;
                if (type == "tb303") {
                    newNode = std::make_shared<audio::Tb303Node>(name.empty() ? "TB303" : name);
                } else if (type == "drum_kit") {
                    newNode = std::make_shared<audio::DrumKitNode>(name.empty() ? "DrumKit" : name);
                } else if (type == "poly_synth") {
                    newNode = std::make_shared<audio::PolySynthNode>(name.empty() ? "PolySynth" : name);
                } else if (type == "biquad") {
                    newNode = std::make_shared<audio::BiquadNode>(name.empty() ? "Filter" : name);
                } else if (type == "delay") {
                    auto dl = std::make_shared<audio::DelayNode>(name.empty() ? "Delay" : name);
                    if (nVal.contains("delayTimeMs")) dl->setDelayTimeMs(static_cast<float>(nVal["delayTimeMs"].asDouble()));
                    if (nVal.contains("feedback")) dl->setFeedback(static_cast<float>(nVal["feedback"].asDouble()));
                    if (nVal.contains("dryWet")) dl->setDryWet(static_cast<float>(nVal["dryWet"].asDouble()));
                    newNode = dl;
                } else if (type == "gain") {
                    auto gn = std::make_shared<audio::GainNode>(name.empty() ? "Gain" : name);
                    if (nVal.contains("volume")) gn->setVolume(static_cast<float>(nVal["volume"].asDouble()));
                    if (nVal.contains("pan")) gn->setPan(static_cast<float>(nVal["pan"].asDouble()));
                    if (nVal.contains("muted")) gn->setMute(nVal["muted"].asBool());
                    newNode = gn;
                } else if (type == "eatscript") {
                    auto es = std::make_shared<audio::EatscriptNode>(name.empty() ? "Eatscript" : name);
                    if (nVal.contains("script")) {
                        es->setScript(nVal["script"].asString());
                    }
                    newNode = es;
                }

                if (newNode) {
                    audio::NodeId newId = graph.addNode(newNode);
                    oldToNewId[oldId] = newId;
                }
            }
        }

        // Reconnect cables
        const auto& connVal = graphVal["connections"];
        if (connVal.isArray()) {
            for (const auto& cVal : connVal.asArray()) {
                if (!cVal.isObject()) continue;
                uint32_t oldSrc = static_cast<uint32_t>(cVal["srcNode"].asInt(0));
                uint32_t srcPort = static_cast<uint32_t>(cVal["srcPort"].asInt(0));
                uint32_t oldDst = static_cast<uint32_t>(cVal["dstNode"].asInt(0));
                uint32_t dstPort = static_cast<uint32_t>(cVal["dstPort"].asInt(0));

                if (oldToNewId.count(oldSrc) && oldToNewId.count(oldDst)) {
                    graph.connect(oldToNewId[oldSrc], srcPort, oldToNewId[oldDst], dstPort);
                }
            }
        }

        // Set output node
        if (graphVal.contains("outputNodeId")) {
            uint32_t oldOut = static_cast<uint32_t>(graphVal["outputNodeId"].asInt(0));
            if (oldToNewId.count(oldOut)) {
                graph.setOutputNode(oldToNewId[oldOut]);
            }
        }
        graph.compile();
    }

    // 2. Rebuild Sequencer
    const auto& seqVal = root["sequencer"];
    if (seqVal.isObject()) {
        const auto& patternsVal = seqVal["patterns"];
        if (patternsVal.isArray()) {
            // Clear existing patterns
            sequencer.clearPatterns();

            for (const auto& pVal : patternsVal.asArray()) {
                if (!pVal.isObject()) continue;
                std::string pName = pVal["name"].asString();
                size_t pIdx = sequencer.addPattern(pName.empty() ? "Pattern" : pName);
                auto& pat = sequencer.getPattern(pIdx);
                pat.tracks.clear();

                const auto& tracksVal = pVal["tracks"];
                if (tracksVal.isArray()) {
                    for (const auto& tVal : tracksVal.asArray()) {
                        if (!tVal.isObject()) continue;
                        std::string tName = tVal["name"].asString();
                        uint32_t oldTargetId = static_cast<uint32_t>(tVal["targetNodeId"].asInt(0));
                        audio::NodeId newTargetId = oldToNewId.count(oldTargetId) ? oldToNewId[oldTargetId] : 0;
                        uint32_t numSteps = static_cast<uint32_t>(tVal["numSteps"].asInt(16));

                        sequencer::SequencerTrack track(tName, newTargetId, numSteps);
                        track.setMuted(tVal["muted"].asBool(false));
                        track.setSolo(tVal["solo"].asBool(false));
                        track.setTranspose(static_cast<int8_t>(tVal["transpose"].asInt(0)));
                        if (tVal.contains("icon")) {
                            track.setIconRef(tVal["icon"].asString());
                        }

                        const auto& stepsVal = tVal["steps"];
                        if (stepsVal.isArray()) {
                            for (const auto& sVal : stepsVal.asArray()) {
                                if (!sVal.isObject()) continue;
                                uint32_t sIdx = static_cast<uint32_t>(sVal["step"].asInt(0));
                                sequencer::StepData step;
                                step.active = true;
                                step.note = static_cast<uint8_t>(sVal["note"].asInt(60));
                                step.velocity = static_cast<float>(sVal["vel"].asDouble(0.8));
                                step.gateLength = static_cast<float>(sVal["gate"].asDouble(0.75));
                                step.slide = sVal["slide"].asBool(false);
                                step.accent = sVal["accent"].asBool(false);
                                step.probability = static_cast<float>(sVal["prob"].asDouble(1.0));
                                if (sVal.contains("pId")) {
                                    step.paramLockId = static_cast<uint32_t>(sVal["pId"].asInt(0));
                                    step.paramLockValue = static_cast<float>(sVal["pVal"].asDouble(0.0));
                                }
                                track.setStep(sIdx, step);
                            }
                        }
                        pat.tracks.push_back(std::move(track));
                    }
                }
            }

            if (seqVal.contains("activePattern")) {
                sequencer.setActivePatternIndex(static_cast<uint32_t>(seqVal["activePattern"].asInt(0)));
            }
        }
    }

    return true;
}

bool ProjectFile::saveToFile(const std::string& filePath,
                             const audio::AudioGraph& graph,
                             const sequencer::StepSequencer& sequencer,
                             const std::string& title,
                             double bpm,
                             double swing) {
    std::string content;
    if (filePath.size() >= 5 && filePath.substr(filePath.size() - 5) == ".eats") {
        content = EatsProjectSerializer::serialize(graph, sequencer, title, bpm, swing);
    } else {
        content = serializeJson(graph, sequencer, title, bpm, swing);
    }

    std::ofstream out(filePath);
    if (!out.is_open()) return false;
    out << content;
    return true;
}

bool ProjectFile::loadFromFile(const std::string& filePath,
                             audio::AudioGraph& graph,
                             sequencer::StepSequencer& sequencer,
                             std::string& outTitle,
                             double& outBpm,
                             double& outSwing) {
    std::ifstream in(filePath);
    if (!in.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    if (EatsProjectSerializer::isEatsScriptFormat(content)) {
        return EatsProjectSerializer::deserialize(content, graph, sequencer, outTitle, outBpm, outSwing);
    }
    return deserializeJson(content, graph, sequencer, outTitle, outBpm, outSwing);
}

} // namespace eatsbits::project
