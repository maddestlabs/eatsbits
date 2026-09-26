#include "eatsbits/eatscript/note_script.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace eatsbits::eatscript {

static inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n\"'");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n\"'");
    return s.substr(start, end - start + 1);
}

static inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string NoteScriptEngine::pitchToString(uint8_t midiNote) noexcept {
    static const char* kNoteNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    int semitone = midiNote % 12;
    int octave = static_cast<int>(midiNote) / 12 - 1;
    return std::string(kNoteNames[semitone]) + std::to_string(octave);
}

int NoteScriptEngine::stringToPitch(const std::string& raw) noexcept {
    std::string s = trim(raw);
    if (s.empty()) return -1;

    // Check if numeric
    bool isNumeric = true;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 0 && (s[i] == '+' || s[i] == '-')) continue;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            isNumeric = false;
            break;
        }
    }
    if (isNumeric) {
        try {
            int val = std::stoi(s);
            return std::clamp(val, 0, 127);
        } catch (...) {
            return -1;
        }
    }

    // Name-based pitch parsing: [A-G][#|b]?[octave]
    char noteChar = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    int semitone = -1;
    switch (noteChar) {
        case 'C': semitone = 0; break;
        case 'D': semitone = 2; break;
        case 'E': semitone = 4; break;
        case 'F': semitone = 5; break;
        case 'G': semitone = 7; break;
        case 'A': semitone = 9; break;
        case 'B': semitone = 11; break;
        default: return -1;
    }

    size_t idx = 1;
    if (idx < s.size()) {
        if (s[idx] == '#' || s[idx] == 's' || s[idx] == 'S') {
            semitone += 1;
            idx++;
        } else if (s[idx] == 'b' || s[idx] == 'B') {
            semitone -= 1;
            idx++;
        }
    }

    int octave = 3; // Default octave if missing
    if (idx < s.size()) {
        try {
            octave = std::stoi(s.substr(idx));
        } catch (...) {
            return -1;
        }
    }

    int midiPitch = (octave + 1) * 12 + semitone;
    return std::clamp(midiPitch, 0, 127);
}

std::vector<std::string> NoteScriptEngine::serializeTrackNotesToLines(const sequencer::SequencerTrack& track) {
    std::vector<std::string> lines;
    lines.reserve(track.getNumSteps() + 4);

    lines.push_back("# Track: \"" + track.getName() + "\" | Steps: " + std::to_string(track.getNumSteps()));
    lines.push_back("# Syntax: { pitch = \"C3\", step = 0, dur = 0.75, vel = 0.80, slide = false, accent = false }");
    lines.push_back("");

    uint32_t numSteps = track.getNumSteps();
    int activeCount = 0;

    for (uint32_t s = 0; s < numSteps; ++s) {
        const auto& step = track.getStep(s);
        if (!step.active) continue;
        activeCount++;

        std::ostringstream ss;
        ss << "{ pitch = \"" << pitchToString(step.note) << "\", "
           << "step = " << s << ", "
           << "dur = " << std::fixed << std::setprecision(2) << step.gateLength << ", "
           << "vel = " << std::fixed << std::setprecision(2) << step.velocity;

        if (step.slide) {
            ss << ", slide = true";
        }
        if (step.accent) {
            ss << ", accent = true";
        }
        if (step.probability < 0.999f) {
            ss << ", prob = " << std::fixed << std::setprecision(2) << step.probability;
        }
        ss << " }";

        lines.push_back(ss.str());
    }

    if (activeCount == 0) {
        lines.push_back("# (No active notes programmed on this track yet)");
    }

    return lines;
}

std::string NoteScriptEngine::serializeTrackNotes(const sequencer::SequencerTrack& track) {
    auto lines = serializeTrackNotesToLines(track);
    std::ostringstream ss;
    for (size_t i = 0; i < lines.size(); ++i) {
        ss << lines[i];
        if (i + 1 < lines.size()) ss << "\n";
    }
    return ss.str();
}

bool NoteScriptEngine::parseTrackNotesFromLines(const std::vector<std::string>& lines,
                                               sequencer::SequencerTrack& track,
                                               std::string* outError,
                                               int* outErrorLine) {
    // Stage parsed steps in temporary list to avoid clearing track on parse errors
    struct ParsedNote {
        uint32_t stepIdx{0};
        sequencer::StepData data;
    };
    std::vector<ParsedNote> parsedSteps;
    parsedSteps.reserve(lines.size());

    for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
        const auto& rawLine = lines[lineIdx];
        std::string line = trim(rawLine);

        // Skip blank lines and comments (#, --, //)
        if (line.empty()) continue;
        if (line[0] == '#' || (line.size() >= 2 && (line.substr(0, 2) == "--" || line.substr(0, 2) == "//"))) {
            continue;
        }

        // Must enclose in { ... }
        auto openBrace = line.find('{');
        auto closeBrace = line.rfind('}');
        if (openBrace == std::string::npos || closeBrace == std::string::npos || closeBrace <= openBrace) {
            if (outError) *outError = "Syntax error: Expected '{ ... }' dictionary format";
            if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
            return false;
        }

        std::string content = line.substr(openBrace + 1, closeBrace - openBrace - 1);
        std::stringstream ss(content);
        std::string pairToken;

        ParsedNote pn;
        pn.data.active = true;
        pn.data.note = 60;
        pn.data.velocity = 0.8f;
        pn.data.gateLength = 0.75f;
        pn.data.slide = false;
        pn.data.accent = false;
        pn.data.probability = 1.0f;

        bool hasStep = false;
        bool hasPitch = false;

        while (std::getline(ss, pairToken, ',')) {
            auto delimPos = pairToken.find('=');
            if (delimPos == std::string::npos) {
                delimPos = pairToken.find(':');
            }
            if (delimPos == std::string::npos) continue;

            std::string key = toLower(trim(pairToken.substr(0, delimPos)));
            std::string val = trim(pairToken.substr(delimPos + 1));

            if (key == "pitch" || key == "note") {
                int p = stringToPitch(val);
                if (p < 0) {
                    if (outError) *outError = "Invalid pitch format: '" + val + "'";
                    if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
                    return false;
                }
                pn.data.note = static_cast<uint8_t>(p);
                hasPitch = true;
            } else if (key == "step") {
                try {
                    int s = std::stoi(val);
                    if (s < 0 || s >= static_cast<int>(sequencer::MAX_STEPS_PER_TRACK)) {
                        if (outError) *outError = "Step index out of range (0.." + std::to_string(sequencer::MAX_STEPS_PER_TRACK - 1) + "): " + val;
                        if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
                        return false;
                    }
                    pn.stepIdx = static_cast<uint32_t>(s);
                    hasStep = true;
                } catch (...) {
                    if (outError) *outError = "Invalid step number: '" + val + "'";
                    if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
                    return false;
                }
            } else if (key == "dur" || key == "duration" || key == "gate" || key == "gatelength") {
                try {
                    float d = std::stof(val);
                    pn.data.gateLength = std::clamp(d, 0.05f, 16.0f);
                } catch (...) {
                    if (outError) *outError = "Invalid duration float: '" + val + "'";
                    if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
                    return false;
                }
            } else if (key == "vel" || key == "velocity") {
                try {
                    float v = std::stof(val);
                    pn.data.velocity = std::clamp(v, 0.01f, 1.0f);
                } catch (...) {
                    if (outError) *outError = "Invalid velocity float: '" + val + "'";
                    if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
                    return false;
                }
            } else if (key == "slide") {
                std::string lval = toLower(val);
                pn.data.slide = (lval == "true" || lval == "1" || lval == "yes");
            } else if (key == "accent") {
                std::string lval = toLower(val);
                pn.data.accent = (lval == "true" || lval == "1" || lval == "yes");
            } else if (key == "prob" || key == "probability") {
                try {
                    float pb = std::stof(val);
                    pn.data.probability = std::clamp(pb, 0.0f, 1.0f);
                } catch (...) {
                    // Ignore or warn
                }
            }
        }

        if (!hasStep) {
            if (outError) *outError = "Note definition missing 'step' attribute";
            if (outErrorLine) *outErrorLine = static_cast<int>(lineIdx) + 1;
            return false;
        }

        parsedSteps.push_back(pn);
    }

    // Success! Clear existing track steps and apply parsed notes
    track.clear();
    for (const auto& ps : parsedSteps) {
        track.setStep(ps.stepIdx, ps.data);
    }

    return true;
}

bool NoteScriptEngine::parseTrackNotes(const std::string& scriptText,
                                      sequencer::SequencerTrack& track,
                                      std::string* outError,
                                      int* outErrorLine) {
    std::vector<std::string> lines;
    std::stringstream ss(scriptText);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    return parseTrackNotesFromLines(lines, track, outError, outErrorLine);
}

} // namespace eatsbits::eatscript
