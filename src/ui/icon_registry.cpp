#include "eatsbits/ui/icon_registry.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <iostream>

namespace eatsbits::ui {

namespace {

std::string toLower(std::string_view sv) {
    std::string s;
    s.reserve(sv.size());
    for (char c : sv) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

} // namespace

IconRegistry& IconRegistry::instance() {
    static IconRegistry s_instance;
    return s_instance;
}

IconRegistry::IconRegistry() {
    initStockLibrary();
}

void IconRegistry::registerIcon(IconDef def) {
    auto id = def.id;
    if (icons_.find(id) == icons_.end()) {
        order_.push_back(id);
    }
    icons_[id] = std::move(def);
}

const IconDef* IconRegistry::findIcon(const std::string& id) const {
    auto it = icons_.find(id);
    if (it != icons_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<const IconDef*> IconRegistry::getAllIcons() const {
    std::vector<const IconDef*> res;
    res.reserve(order_.size());
    for (const auto& id : order_) {
        auto it = icons_.find(id);
        if (it != icons_.end()) {
            res.push_back(&it->second);
        }
    }
    return res;
}

std::vector<std::string> IconRegistry::getCategories() const {
    std::vector<std::string> cats;
    for (const auto& id : order_) {
        const auto& ic = icons_.at(id);
        if (ic.isStockUiOnly) continue;
        if (std::find(cats.begin(), cats.end(), ic.category) == cats.end()) {
            cats.push_back(ic.category);
        }
    }
    return cats;
}

std::vector<const IconDef*> IconRegistry::query(std::string_view searchTerm, 
                                                std::string_view category) const {
    std::vector<const IconDef*> results;
    std::string lowerSearch = toLower(searchTerm);
    std::string lowerCat = toLower(category);

    for (const auto& id : order_) {
        const auto& ic = icons_.at(id);
        if (ic.isStockUiOnly && lowerCat != "ui") continue;

        // Check category filter
        if (!lowerCat.empty() && lowerCat != "all") {
            if (toLower(ic.category) != lowerCat) {
                continue;
            }
        }

        // Check search term
        if (lowerSearch.empty()) {
            results.push_back(&ic);
            continue;
        }

        // Match id
        if (toLower(ic.id).find(lowerSearch) != std::string::npos) {
            results.push_back(&ic);
            continue;
        }

        // Match display name
        if (toLower(ic.displayName).find(lowerSearch) != std::string::npos) {
            results.push_back(&ic);
            continue;
        }

        // Match tags
        bool tagMatched = false;
        for (const auto& tag : ic.tags) {
            if (toLower(tag).find(lowerSearch) != std::string::npos) {
                tagMatched = true;
                break;
            }
        }
        if (tagMatched) {
            results.push_back(&ic);
        }
    }

    return results;
}

std::optional<IconDef> IconRegistry::parsePastedSvg(std::string_view text, 
                                                    const std::string& customId,
                                                    const std::string& displayName) {
    if (text.empty() || text.size() > 65536) { // Reject overly huge payloads
        return std::nullopt;
    }

    std::string str(text);
    std::string pathData;
    SvgRect viewBox{0.0f, 0.0f, 24.0f, 24.0f};

    // Check if input is a full <svg> element
    auto svgPos = str.find("<svg");
    if (svgPos != std::string::npos) {
        // Try extracting viewBox="minX minY w h"
        std::regex vbRegex(R"(viewBox\s*=\s*["']([^"']+)["'])", std::regex::icase);
        std::smatch vbMatch;
        if (std::regex_search(str, vbMatch, vbRegex)) {
            std::stringstream ss(vbMatch[1].str());
            float vx, vy, vw, vh;
            if (ss >> vx >> vy >> vw >> vh) {
                viewBox = SvgRect{vx, vy, vx + vw, vy + vh};
            }
        }

        // Extract d="..." from <path ... d="..." />
        std::regex dRegex(R"(<path[^>]*\bd\s*=\s*["']([^"']+)["'])", std::regex::icase);
        std::smatch dMatch;
        if (std::regex_search(str, dMatch, dRegex)) {
            pathData = dMatch[1].str();
        } else {
            // Check without <path> tag for general d attribute
            std::regex dGenRegex(R"(\bd\s*=\s*["']([^"']+)["'])", std::regex::icase);
            if (std::regex_search(str, dMatch, dGenRegex)) {
                pathData = dMatch[1].str();
            }
        }
    } else {
        // Treat as raw 'd' string
        // Strip any leading "d=" or quotes if present
        size_t start = 0;
        while (start < str.size() && (std::isspace(str[start]) || str[start] == '"' || str[start] == '\'')) {
            start++;
        }
        if (str.substr(start, 2) == "d=" || str.substr(start, 2) == "D=") {
            start += 2;
            while (start < str.size() && (std::isspace(str[start]) || str[start] == '"' || str[start] == '\'')) {
                start++;
            }
        }
        size_t end = str.size();
        while (end > start && (std::isspace(str[end - 1]) || str[end - 1] == '"' || str[end - 1] == '\'')) {
            end--;
        }
        pathData = str.substr(start, end - start);
    }

    if (pathData.empty()) {
        return std::nullopt;
    }

    // Validate path by parsing into SvgPath
    SvgPath parsed = SvgPath::parse(pathData);
    if (parsed.empty() || parsed.getContours().empty()) {
        return std::nullopt;
    }

    IconDef def;
    def.id = customId;
    def.displayName = displayName;
    def.category = "Custom";
    def.tags = {"custom", "svg", "user"};
    def.svgPath = std::move(pathData);
    def.viewBox = viewBox;
    def.isStockUiOnly = false;

    return def;
}

void IconRegistry::renderIcon(BatchRenderer2D& r, const std::string& iconRef, 
                              const Rect2D& bounds, const Color& tint) {
    if (iconRef.empty() || bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    std::string pathData;
    SvgRect viewBox{0.0f, 0.0f, 24.0f, 24.0f};

    if (iconRef.rfind("svg:", 0) == 0) {
        pathData = iconRef.substr(4);
    } else {
        std::string iconId = iconRef;
        if (iconId.rfind("preset:", 0) == 0) {
            iconId = iconId.substr(7);
        }
        const auto* def = findIcon(iconId);
        if (def) {
            pathData = def->svgPath;
            viewBox = def->viewBox;
        } else {
            // Fallback to synth if not found
            const auto* fb = findIcon("inst_synth");
            if (fb) {
                pathData = fb->svgPath;
                viewBox = fb->viewBox;
            }
        }
    }

    if (pathData.empty()) return;

    // Cache key for triangulated geometry
    const std::string& cacheKey = pathData;
    auto it = meshCache_.find(cacheKey);
    if (it == meshCache_.end()) {
        SvgPath path = SvgPath::parse(pathData);
        CachedMesh mesh;
        mesh.triangles = path.triangulate();
        mesh.bounds = path.getBounds();
        if (mesh.bounds.isEmpty()) {
            mesh.bounds = viewBox;
        }
        it = meshCache_.emplace(cacheKey, std::move(mesh)).first;
    }

    const auto& mesh = it->second;
    const auto& vb = mesh.bounds;
    float vbW = std::max(0.001f, vb.width());
    float vbH = std::max(0.001f, vb.height());

    // Scale to fit while preserving aspect ratio with 10% padding
    float pad = std::min(bounds.w, bounds.h) * 0.10f;
    float targetW = bounds.w - pad * 2.0f;
    float targetH = bounds.h - pad * 2.0f;
    float scale = std::min(targetW / vbW, targetH / vbH);

    float offsetX = bounds.x + (bounds.w - vbW * scale) * 0.5f - vb.minX * scale;
    float offsetY = bounds.y + (bounds.h - vbH * scale) * 0.5f - vb.minY * scale;

    if (!mesh.triangles.empty()) {
        // Draw filled triangles
        const size_t numTris = mesh.triangles.size() / 3;
        for (size_t t = 0; t < numTris; ++t) {
            const auto& p0 = mesh.triangles[t * 3 + 0];
            const auto& p1 = mesh.triangles[t * 3 + 1];
            const auto& p2 = mesh.triangles[t * 3 + 2];

            r.drawTriangle(p0.x * scale + offsetX, p0.y * scale + offsetY,
                           p1.x * scale + offsetX, p1.y * scale + offsetY,
                           p2.x * scale + offsetX, p2.y * scale + offsetY,
                           tint.r, tint.g, tint.b, tint.a);
        }
    } else {
        // Fallback for stroke-based paths: draw contours as lines
        SvgPath path = SvgPath::parse(pathData);
        float lineWidth = std::max(1.0f, scale * 1.5f);
        for (const auto& contour : path.getContours()) {
            if (contour.size() < 2) continue;
            for (size_t i = 0; i < contour.size() - 1; ++i) {
                r.drawLine(contour[i].x * scale + offsetX, contour[i].y * scale + offsetY,
                           contour[i + 1].x * scale + offsetX, contour[i + 1].y * scale + offsetY,
                           tint.r, tint.g, tint.b, tint.a, lineWidth);
            }
        }
    }
}

void IconRegistry::initStockLibrary() {
    // ------------------------------------------------------------------------
    // INSTRUMENTS
    // ------------------------------------------------------------------------
    registerIcon({
        "inst_synth", "Poly Synth", "Instruments",
        {"synth", "keys", "poly", "analog", "keyboard"},
        "M 2 5 L 22 5 A 2 2 0 0 1 24 7 L 24 17 A 2 2 0 0 1 22 19 L 2 19 A 2 2 0 0 1 0 17 L 0 7 A 2 2 0 0 1 2 5 Z "
        "M 4 8 L 7 8 L 7 16 L 4 16 Z M 9 8 L 12 8 L 12 16 L 9 16 Z M 14 8 L 17 8 L 17 16 L 14 16 Z M 19 8 L 20 8 L 20 16 L 19 16 Z"
    });

    registerIcon({
        "inst_piano", "Grand Piano", "Instruments",
        {"piano", "keys", "acoustic", "grand", "electric"},
        "M 2 4 L 22 4 L 22 20 L 2 20 Z M 4 12 L 6 12 L 6 18 L 4 18 Z M 8 12 L 10 12 L 10 18 L 8 18 Z "
        "M 14 12 L 16 12 L 16 18 L 14 18 Z M 18 12 L 20 12 L 20 18 L 18 18 Z"
    });

    registerIcon({
        "inst_bass", "Synth Bass", "Instruments",
        {"bass", "sub", "low", "synthbass", "sine"},
        "M 2 12 C 4 6 8 6 10 12 C 12 18 16 18 18 12 C 20 6 22 6 24 12 L 24 15 C 22 9 20 9 18 15 C 16 21 12 21 10 15 C 8 9 4 9 2 15 Z"
    });

    registerIcon({
        "inst_guitar", "Guitar", "Instruments",
        {"guitar", "acoustic", "electric", "strings", "plucked"},
        "M 19 2 L 22 5 L 14 13 C 15 15 14 18 11 20 C 8 22 4 21 3 19 C 2 18 1 14 3 11 C 5 8 8 7 10 8 Z M 8 15 A 2 2 0 1 0 10 17 A 2 2 0 0 0 8 15 Z"
    });

    registerIcon({
        "inst_strings", "Strings", "Instruments",
        {"strings", "violin", "orchestral", "cellos", "viola"},
        "M 8 3 C 8 6 6 8 6 12 C 6 16 8 18 8 21 L 16 21 C 16 18 18 16 18 12 C 18 8 16 6 16 3 Z M 11 10 L 13 10 L 13 14 L 11 14 Z"
    });

    registerIcon({
        "inst_brass", "Brass", "Instruments",
        {"brass", "horn", "trumpet", "horns", "winds"},
        "M 3 10 L 12 10 L 21 4 L 21 20 L 12 14 L 3 14 Z M 6 7 L 9 7 L 9 10 L 6 10 Z"
    });

    registerIcon({
        "inst_vocal", "Microphone", "Instruments",
        {"vocal", "voice", "mic", "singing", "speech"},
        "M 8 6 A 4 4 0 0 1 16 6 L 16 11 A 4 4 0 0 1 8 11 Z M 5 10 L 7 10 A 5 5 0 0 0 17 10 L 19 10 A 7 7 0 0 1 13 17 L 13 20 L 16 20 L 16 22 L 8 22 L 8 20 L 11 20 L 11 17 A 7 7 0 0 1 5 10 Z"
    });

    // ------------------------------------------------------------------------
    // DRUMS & PERCUSSION
    // ------------------------------------------------------------------------
    registerIcon({
        "drum_kick", "Kick Drum", "Drums",
        {"kick", "bassdrum", "808", "punch", "drum"},
        "M 12 2 A 10 10 0 1 0 22 12 A 10 10 0 0 0 12 2 Z M 12 6 A 6 6 0 1 1 6 12 A 6 6 0 0 1 12 6 Z M 12 9 A 3 3 0 1 0 15 12 A 3 3 0 0 0 12 9 Z"
    });

    registerIcon({
        "drum_snare", "Snare Drum", "Drums",
        {"snare", "rim", "clap", "backbeat"},
        "M 2 7 L 22 7 L 22 15 L 2 15 Z M 4 4 L 7 7 L 5 7 L 2 4 Z M 20 4 L 17 7 L 19 7 L 22 4 Z M 2 17 L 22 17 L 22 19 L 2 19 Z"
    });

    registerIcon({
        "drum_hihat", "Hi-Hat", "Drums",
        {"hihat", "hats", "cymbals", "pedal", "metallic"},
        "M 12 3 L 2 9 L 22 9 Z M 2 11 L 22 11 L 12 17 Z M 11 17 L 13 17 L 13 22 L 11 22 Z"
    });

    registerIcon({
        "drum_clap", "Clap", "Drums",
        {"clap", "snap", "hands", "percussion"},
        "M 6 12 L 10 4 L 14 4 L 10 12 Z M 10 12 L 14 20 L 18 20 L 14 12 Z M 13 9 L 20 9 L 20 11 L 13 11 Z"
    });

    registerIcon({
        "drum_tom", "Tom Drum", "Drums",
        {"tom", "toms", "floor", "rack", "drum"},
        "M 3 6 L 21 6 L 19 18 L 5 18 Z M 6 9 L 18 9 L 17 15 L 7 15 Z"
    });

    registerIcon({
        "drum_machine", "Drum Machine", "Drums",
        {"machine", "step", "sequencer", "sampler", "pads", "808"},
        "M 2 4 L 22 4 L 22 20 L 2 20 Z M 4 6 L 8 6 L 8 10 L 4 10 Z M 10 6 L 14 6 L 14 10 L 10 10 Z "
        "M 16 6 L 20 6 L 20 10 L 16 10 Z M 4 12 L 8 12 L 8 16 L 4 16 Z M 10 12 L 14 12 L 14 16 L 10 16 Z M 16 12 L 20 12 L 20 16 L 16 16 Z"
    });

    // ------------------------------------------------------------------------
    // FX & MODULAR
    // ------------------------------------------------------------------------
    registerIcon({
        "fx_reverb", "Reverb", "FX & Mod",
        {"reverb", "room", "hall", "space", "echo", "ambience"},
        "M 12 4 A 8 8 0 0 1 20 12 L 18 12 A 6 6 0 0 0 12 6 Z M 12 8 A 4 4 0 0 1 16 12 L 14 12 A 2 2 0 0 0 12 10 Z "
        "M 4 12 A 8 8 0 0 1 12 4 L 12 6 A 6 6 0 0 0 6 12 Z M 8 12 A 4 4 0 0 1 12 8 L 12 10 A 2 2 0 0 0 10 12 Z M 2 12 L 22 12 L 22 14 L 2 14 Z"
    });

    registerIcon({
        "fx_delay", "Delay / Echo", "FX & Mod",
        {"delay", "echo", "repeat", "tape", "pingpong"},
        "M 12 2 A 10 10 0 1 0 22 12 L 20 12 A 8 8 0 1 1 12 4 Z M 11 6 L 13 6 L 13 12 L 17 14 L 16 16 L 11 13 Z"
    });

    registerIcon({
        "fx_filter", "Filter", "FX & Mod",
        {"filter", "cutoff", "resonance", "lowpass", "highpass"},
        "M 2 6 L 12 6 L 22 18 L 22 20 L 2 20 Z M 10 4 L 14 4 L 14 8 L 10 8 Z"
    });

    registerIcon({
        "fx_distortion", "Distortion / Drive", "FX & Mod",
        {"dist", "distortion", "overdrive", "fuzz", "clip", "saturation"},
        "M 13 2 L 4 14 L 11 14 L 9 22 L 20 10 L 13 10 Z"
    });

    registerIcon({
        "fx_compressor", "Compressor", "FX & Mod",
        {"comp", "compressor", "limiter", "dynamics", "sidechain"},
        "M 3 3 L 5 3 L 5 21 L 3 21 Z M 19 3 L 21 3 L 21 21 L 19 21 Z M 8 8 L 11 12 L 8 16 Z M 16 8 L 13 12 L 16 16 Z"
    });

    registerIcon({
        "fx_eq", "Equalizer", "FX & Mod",
        {"eq", "equalizer", "parametric", "tone", "curve"},
        "M 4 8 L 4 16 L 6 16 L 6 8 Z M 11 4 L 11 20 L 13 20 L 13 4 Z M 18 10 L 18 14 L 20 14 L 20 10 Z"
    });

    registerIcon({
        "fx_modular", "Patch / Modular", "FX & Mod",
        {"modular", "cable", "eurorack", "jack", "cv", "patch"},
        "M 12 2 A 4 4 0 0 0 8 6 L 8 10 A 4 4 0 0 0 12 14 A 4 4 0 0 0 16 10 L 16 6 A 4 4 0 0 0 12 2 Z "
        "M 11 14 L 11 20 C 11 22 13 22 13 20 L 13 14 Z M 11 5 L 13 5 L 13 8 L 11 8 Z"
    });

    // ------------------------------------------------------------------------
    // HARDWARE & GENERAL
    // ------------------------------------------------------------------------
    registerIcon({
        "hw_speaker", "Monitor Speaker", "Hardware",
        {"speaker", "monitor", "sound", "volume", "audio"},
        "M 3 9 L 8 9 L 14 4 L 14 20 L 8 15 L 3 15 Z M 17 8 A 5 5 0 0 1 17 16 L 18 18 A 7 7 0 0 0 18 6 Z"
    });

    registerIcon({
        "hw_headphones", "Headphones", "Hardware",
        {"headphones", "phones", "cue", "listen", "monitor"},
        "M 3 12 A 9 9 0 0 1 21 12 L 22 16 A 2 2 0 0 1 20 18 L 18 18 L 18 12 L 20 12 A 8 8 0 0 0 4 12 L 6 12 L 6 18 L 4 18 A 2 2 0 0 1 2 16 Z"
    });

    registerIcon({
        "hw_cassette", "Cassette Tape", "Hardware",
        {"cassette", "tape", "retro", "lofi", "analog"},
        "M 2 4 L 22 4 L 22 20 L 2 20 Z M 6 8 A 3 3 0 1 0 6 14 A 3 3 0 0 0 6 8 Z M 18 8 A 3 3 0 1 0 18 14 A 3 3 0 0 0 18 8 Z M 9 14 L 15 14 L 15 16 L 9 16 Z"
    });

    registerIcon({
        "gen_folder", "Sample Folder", "General",
        {"folder", "directory", "samples", "files", "load"},
        "M 2 4 L 9 4 L 11 7 L 22 7 L 22 20 L 2 20 Z"
    });

    registerIcon({
        "gen_star", "Favorite / Star", "General",
        {"star", "favorite", "badge", "bookmark", "best"},
        "M 12 2 L 15 8 L 22 9 L 17 14 L 18 21 L 12 18 L 6 21 L 7 14 L 2 9 L 9 8 Z"
    });

    registerIcon({
        "gen_zap", "Lightning / Energy", "General",
        {"zap", "bolt", "electric", "power", "energy"},
        "M 13 2 L 3 13 L 11 13 L 9 22 L 21 10 L 13 10 Z"
    });

    // ------------------------------------------------------------------------
    // INTERNAL UI WIDGET ICONS
    // ------------------------------------------------------------------------
    registerIcon({
        "ui_piano", "Piano Keys Widget", "UI",
        {"ui", "keyboard", "drawer"},
        "M 2 4 L 22 4 L 22 20 L 2 20 Z M 5 12 L 7 12 L 7 18 L 5 18 Z M 9 12 L 11 12 L 11 18 L 9 18 Z "
        "M 13 12 L 15 12 L 15 18 L 13 18 Z M 17 12 L 19 12 L 19 18 L 17 18 Z",
        {0, 0, 24, 24}, true
    });

    registerIcon({
        "ui_properties", "Properties Sliders Widget", "UI",
        {"ui", "sliders", "drawer", "properties"},
        "M 2 6 L 22 6 L 22 8 L 2 8 Z M 2 11 L 22 11 L 22 13 L 2 13 Z M 2 16 L 22 16 L 22 18 L 2 18 Z "
        "M 6 4 L 10 4 L 10 10 L 6 10 Z M 14 9 L 18 9 L 18 15 L 14 15 Z M 8 14 L 12 14 L 12 20 L 8 20 Z",
        {0, 0, 24, 24}, true
    });
}

} // namespace eatsbits::ui
