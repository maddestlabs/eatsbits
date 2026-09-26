#include "eatsbits/eatscript/dispatch_scanner.hpp"
#include <sstream>
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

NativeDispatchTarget NativeDispatchScanner::detectTarget(const std::string& scriptSource) noexcept {
    std::stringstream ss(scriptSource);
    std::string line;

    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.substr(0, 2) == "--" || trimmed.substr(0, 2) == "//") {
            continue;
        }

        // Support Eatscript metadata header directives (e.g. # @id: eats_303 or # @engine: tb303)
        if (trimmed.rfind("# @", 0) == 0) {
            auto colon = trimmed.find(':');
            if (colon != std::string::npos) {
                std::string directive = toLower(trim(trimmed.substr(3, colon - 3)));
                std::string val = toLower(trim(trimmed.substr(colon + 1)));
                if (directive == "engine" || directive == "id") {
                    if (val == "tb303" || val == "eats_303") return NativeDispatchTarget::TB303;
                    if (val == "sid" || val == "c64_sid_synth") return NativeDispatchTarget::SID;
                    if (val == "dx7" || val == "dx7_epiano") return NativeDispatchTarget::DX7;
                    if (val == "snes" || val == "snes_dsp") return NativeDispatchTarget::SNES;
                    if (val == "ym2612" || val == "genesis_ym2612") return NativeDispatchTarget::YM2612;
                    if (val == "drum_808" || val == "analog_808_kick") return NativeDispatchTarget::DrumKit808;
                    if (val == "drum_909" || val == "analog_909_snare") return NativeDispatchTarget::DrumKit909;
                    if (val == "piano_physical" || val == "concert_grand_piano") return NativeDispatchTarget::GrandPiano;
                    if (val == "upright_bass") return NativeDispatchTarget::UprightBass;
                    if (val == "spanish_guitar") return NativeDispatchTarget::SpanishGuitar;
                    if (val == "acoustic_steel_guitar") return NativeDispatchTarget::SteelGuitar;
                    if (val == "convolver" || val == "convolver_space") return NativeDispatchTarget::Convolver;
                    if (val == "delay" || val == "stereo_delay") return NativeDispatchTarget::StereoDelay;
                }
            }
            continue;
        }
        if (trimmed[0] == '#') continue;

        // Stop scanning top-level flags once inside function definitions
        if (trimmed.rfind("def ", 0) == 0 || trimmed.rfind("function ", 0) == 0) {
            break;
        }

        auto eqPos = trimmed.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eqPos));
        std::string val = toLower(trim(trimmed.substr(eqPos + 1)));

        bool isTrue = (val == "true" || val == "1" || val == "yes");
        if (!isTrue) continue;

        std::string lkey = toLower(key);
        if (lkey == "eats303" || lkey == "jc303" || lkey == "tb303") {
            return NativeDispatchTarget::TB303;
        }
        if (lkey == "sidsynth" || lkey == "sid" || lkey == "mos6581" || lkey == "mos8580" || lkey == "c64sid") {
            return NativeDispatchTarget::SID;
        }
        if (lkey == "dx7epiano" || lkey == "dx7" || lkey == "yamahadx7" || lkey == "msfa") {
            return NativeDispatchTarget::DX7;
        }
        if (lkey == "snesdsp" || lkey == "snes" || lkey == "spc700") {
            return NativeDispatchTarget::SNES;
        }
        if (lkey == "ym2612" || lkey == "opn2" || lkey == "genesisfm") {
            return NativeDispatchTarget::YM2612;
        }
        if (lkey == "analog808kick" || lkey == "procedural808" || lkey == "tr808") {
            return NativeDispatchTarget::DrumKit808;
        }
        if (lkey == "analog909snare" || lkey == "procedural909" || lkey == "tr909") {
            return NativeDispatchTarget::DrumKit909;
        }
        if (lkey == "concertgrandpiano" || lkey == "steinwaygrand" || lkey == "grandpiano") {
            return NativeDispatchTarget::GrandPiano;
        }
        if (lkey == "doublebass" || lkey == "uprightbass") {
            return NativeDispatchTarget::UprightBass;
        }
        if (lkey == "spanishguitar" || lkey == "classicalguitar") {
            return NativeDispatchTarget::SpanishGuitar;
        }
        if (lkey == "steelacousticguitar" || lkey == "steelguitar") {
            return NativeDispatchTarget::SteelGuitar;
        }
        if (lkey == "convolver" || lkey == "cabdesigner" || lkey == "impulseresponse") {
            return NativeDispatchTarget::Convolver;
        }
        if (lkey == "stereodelay" || lkey == "tapeecho") {
            return NativeDispatchTarget::StereoDelay;
        }
        if (lkey == "polysynth" || lkey == "polyblep") {
            return NativeDispatchTarget::PolySynth;
        }
    }

    return NativeDispatchTarget::None;
}

const char* NativeDispatchScanner::targetToString(NativeDispatchTarget target) noexcept {
    switch (target) {
        case NativeDispatchTarget::TB303:         return "Roland TB-303 (Diode Ladder SIMD)";
        case NativeDispatchTarget::SID:           return "Commodore 64 SID (MOS 6581/8580)";
        case NativeDispatchTarget::DX7:           return "Yamaha DX7 6-Op FM Sound Engine";
        case NativeDispatchTarget::SNES:          return "Super Nintendo S-DSP (SPC700)";
        case NativeDispatchTarget::YM2612:        return "Sega Genesis YM2612 (OPN2)";
        case NativeDispatchTarget::DrumKit808:    return "Roland TR-808 Rhythm Composer";
        case NativeDispatchTarget::DrumKit909:    return "Roland TR-909 Rhythm Composer";
        case NativeDispatchTarget::GrandPiano:    return "Bank-Bensa Concert Grand Piano";
        case NativeDispatchTarget::UprightBass:   return "Upright Double Bass (Growl/Slap)";
        case NativeDispatchTarget::SpanishGuitar: return "Spanish Classical Nylon Guitar";
        case NativeDispatchTarget::SteelGuitar:   return "Steel Acoustic Guitar (Waveguide)";
        case NativeDispatchTarget::Convolver:     return "Real-Time Convolution Reverb & Cab";
        case NativeDispatchTarget::StereoDelay:   return "Stereo Tape Delay & Echo";
        case NativeDispatchTarget::PolySynth:     return "16-Voice PolyBLEP Multi-Wave Synth";
        case NativeDispatchTarget::None:          return "Bytecode Virtual Machine (Eatscript VM)";
    }
    return "Unknown Target";
}

std::map<std::string, ExtractedParam> NativeDispatchScanner::extractParameters(const std::string& scriptSource) {
    std::map<std::string, ExtractedParam> params;
    std::stringstream ss(scriptSource);
    std::string line;
    bool inInit = false;

    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);
        if (trimmed.rfind("def init", 0) == 0 || trimmed.rfind("function init", 0) == 0) {
            inInit = true;
            continue;
        }
        if (inInit && (trimmed.rfind("def ", 0) == 0 || trimmed.rfind("function ", 0) == 0)) {
            break;
        }
        if (!inInit) continue;

        // Check for params['Name'] = Value
        auto pOpen = trimmed.find("params[");
        if (pOpen != std::string::npos) {
            auto pClose = trimmed.find(']', pOpen);
            auto eqPos = trimmed.find('=', pClose);
            if (pClose != std::string::npos && eqPos != std::string::npos) {
                std::string key = trim(trimmed.substr(pOpen + 7, pClose - pOpen - 7));
                std::string valStr = trim(trimmed.substr(eqPos + 1));
                try {
                    float v = std::stof(valStr);
                    ExtractedParam ep;
                    ep.name = key;
                    ep.defaultVal = v;
                    ep.minVal = 0.0f;
                    ep.maxVal = (v != 0.0f) ? std::abs(v) * 2.0f : 1.0f;
                    params[key] = ep;
                } catch (...) {}
                continue;
            }
        }

        // Search for eat.param("Name", min, max, default) or Param.add("Name", min, max, default)
        auto paramPos = trimmed.find(".param(");
        if (paramPos == std::string::npos) {
            paramPos = trimmed.find(".add(");
        }
        if (paramPos != std::string::npos) {
            auto openParen = trimmed.find('(', paramPos);
            auto closeParen = trimmed.find(')', openParen);
            if (openParen != std::string::npos && closeParen != std::string::npos) {
                std::string argsStr = trimmed.substr(openParen + 1, closeParen - openParen - 1);
                std::stringstream argStream(argsStr);
                std::string pName, pMin, pMax, pDef;
                std::getline(argStream, pName, ',');
                std::getline(argStream, pMin, ',');
                std::getline(argStream, pMax, ',');
                std::getline(argStream, pDef, ',');

                pName = trim(pName);
                if (!pName.empty()) {
                    ExtractedParam ep;
                    ep.name = pName;
                    try { if (!pMin.empty()) ep.minVal = std::stof(trim(pMin)); } catch (...) {}
                    try { if (!pMax.empty()) ep.maxVal = std::stof(trim(pMax)); } catch (...) {}
                    try { if (!pDef.empty()) ep.defaultVal = std::stof(trim(pDef)); } catch (...) {}
                    params[ep.name] = ep;
                }
            }
        }
    }

    return params;
}

std::map<std::string, float> NativeDispatchScanner::extractParamsFromInit(const std::string& scriptSource) {
    std::map<std::string, float> result;
    std::stringstream ss(scriptSource);
    std::string line;
    bool inInit = false;

    while (std::getline(ss, line)) {
        std::string trimmed = trim(line);
        if (trimmed.rfind("def init", 0) == 0 || trimmed.rfind("function init", 0) == 0) {
            inInit = true;
            continue;
        }
        if (inInit && (trimmed.rfind("def ", 0) == 0 || trimmed.rfind("function ", 0) == 0)) {
            break;
        }
        if (!inInit) continue;

        // Check for params['Name'] = Value or params["Name"] = Value
        auto pOpen = trimmed.find("params[");
        if (pOpen != std::string::npos) {
            auto pClose = trimmed.find(']', pOpen);
            auto eqPos = trimmed.find('=', pClose);
            if (pClose != std::string::npos && eqPos != std::string::npos) {
                std::string key = trim(trimmed.substr(pOpen + 7, pClose - pOpen - 7));
                std::string valStr = trim(trimmed.substr(eqPos + 1));
                try {
                    result[key] = std::stof(valStr);
                } catch (...) {}
                continue;
            }
        }

        // Check for "Name": eat.param("Name", min, max, default) or "Name": 123.4
        auto colonPos = trimmed.find(':');
        if (colonPos != std::string::npos) {
            std::string key = trim(trimmed.substr(0, colonPos));
            std::string rhs = trim(trimmed.substr(colonPos + 1));
            if (!key.empty() && !rhs.empty()) {
                auto paramPos = rhs.find(".param(");
                if (paramPos != std::string::npos) {
                    auto openP = rhs.find('(', paramPos);
                    auto closeP = rhs.find(')', openP);
                    if (openP != std::string::npos && closeP != std::string::npos) {
                        std::string args = rhs.substr(openP + 1, closeP - openP - 1);
                        std::stringstream as(args);
                        std::string n, minV, maxV, defV;
                        std::getline(as, n, ',');
                        std::getline(as, minV, ',');
                        std::getline(as, maxV, ',');
                        std::getline(as, defV, ',');
                        try {
                            if (!defV.empty()) result[key] = std::stof(trim(defV));
                            else if (!minV.empty()) result[key] = std::stof(trim(minV));
                        } catch (...) {}
                    }
                } else {
                    if (rhs.back() == ',') rhs.pop_back();
                    try {
                        result[key] = std::stof(trim(rhs));
                    } catch (...) {}
                }
            }
        }
    }
    return result;
}

} // namespace eatsbits::eatscript
