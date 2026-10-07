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
        // Sleek synthesizer with top LED knobs and 5 crisp white keys
        "M 2 5 L 22 5 L 22 8 L 2 8 Z "
        "M 4 2 L 6 2 L 6 4 L 4 4 Z M 11 2 L 13 2 L 13 4 L 11 4 Z M 18 2 L 20 2 L 20 4 L 18 4 Z "
        "M 2 10 L 5 10 L 5 21 L 2 21 Z "
        "M 6.2 10 L 9.2 10 L 9.2 21 L 6.2 21 Z "
        "M 10.4 10 L 13.4 10 L 13.4 21 L 10.4 21 Z "
        "M 14.6 10 L 17.6 10 L 17.6 21 L 14.6 21 Z "
        "M 18.8 10 L 21.8 10 L 21.8 21 L 18.8 21 Z"
    });

    registerIcon({
        "inst_piano", "Grand Piano", "Instruments",
        {"piano", "keys", "acoustic", "grand", "electric"},
        // Grand piano silhouette with propped open lid, curved body & legs
        "M 2 13 L 22 13 L 22 10 C 22 5 17 3 11 3 L 2 3 Z "
        "M 12 3 L 20 0.5 L 20.8 1.8 L 13.5 4 Z "
        "M 4 14 L 4 20 L 5.8 20 L 5.8 14 Z "
        "M 18.2 14 L 18.2 20 L 20 20 L 20 14 Z "
        "M 11.1 14 L 11.1 18.5 L 12.9 18.5 L 12.9 14 Z"
    });

    registerIcon({
        "inst_bass", "Synth Bass", "Instruments",
        {"bass", "sub", "low", "synthbass", "sine"},
        // Heavy oscillating sub-bass sine wave with punchy sub pulses
        "M 2 12 C 4 3 8 3 10 12 C 12 21 16 21 18 12 C 19 7 21 7 22 12 L 22 15.5 C 20 9.5 18 9.5 17 15.5 C 15 23.5 11 23.5 9 15.5 C 7 6.5 5 6.5 2 15.5 Z "
        "M 4 19 L 8 19 L 8 21.5 L 4 21.5 Z "
        "M 10 19 L 14 19 L 14 21.5 L 10 21.5 Z "
        "M 16 19 L 20 19 L 20 21.5 L 16 21.5 Z"
    });

    registerIcon({
        "inst_guitar", "Guitar", "Instruments",
        {"guitar", "acoustic", "electric", "strings", "plucked"},
        // Electric guitar / axe body with pickguard and angled neck
        "M 5 21 C 2.5 21 2 18 2 15 C 2 12 4 11 5 10 C 3 8 4 5 7 5 L 8 8 C 8 10 10 10.5 11 10 L 19 2 L 21 2 L 21 4 L 13 12 C 12.5 13 13.5 15 15 15 L 18 16 C 18 19 14.5 20 12.5 19 C 11.5 20 9.5 21 5 21 Z "
        "M 19 1 L 22 1 L 22 3 L 19 3 Z"
    });

    registerIcon({
        "inst_strings", "Strings", "Instruments",
        {"strings", "violin", "orchestral", "cellos", "viola"},
        // Violin / Cello body silhouette with carved C-bout waist and scroll
        "M 11 1 L 13 1 L 13 5 L 11 5 Z "
        "M 9 5 L 15 5 C 13.5 6.5 13.5 8 15.5 9 C 18.5 10.5 19 13 19 15.5 C 19 19.5 16 22 12 22 C 8 22 5 19.5 5 15.5 C 5 13 5.5 10.5 8.5 9 C 10.5 8 10.5 6.5 9 5 Z "
        "M 11.2 18 L 12.8 18 L 12.5 21 L 11.5 21 Z"
    });

    registerIcon({
        "inst_brass", "Brass", "Instruments",
        {"brass", "horn", "trumpet", "horns", "winds"},
        // Trumpet with 3 piston valves and expanding bell flare
        "M 2 11 L 4 11 L 4 13 L 2 13 Z "
        "M 4 11.5 L 9 11.5 L 9 9.5 L 10.5 9.5 L 10.5 13.5 L 9 13.5 L 9 12.5 L 4 12.5 Z "
        "M 11.5 8 L 13 8 L 13 15 L 11.5 15 Z "
        "M 14 9 L 15.5 9 L 15.5 14 L 14 14 Z "
        "M 16 11.5 L 19 10 L 22 6 L 22 18 L 19 14 L 16 12.5 Z"
    });

    registerIcon({
        "inst_vocal", "Microphone", "Instruments",
        {"vocal", "voice", "mic", "singing", "speech"},
        // Studio condenser microphone capsule with shockmount cradle
        "M 9 2 L 15 2 C 16.5 2 17.5 3 17.5 4.5 L 17.5 10 C 17.5 11.5 16.5 12.5 15 12.5 L 9 12.5 C 7.5 12.5 6.5 11.5 6.5 10 L 6.5 4.5 C 6.5 3 7.5 2 9 2 Z "
        "M 4 8 L 5.5 8 L 5.5 10 C 5.5 13.5 8.5 16 12 16 C 15.5 16 18.5 13.5 18.5 10 L 18.5 8 L 20 8 L 20 10 C 20 14.5 16.5 17.5 12.8 17.5 L 12.8 20.5 L 16 20.5 L 16 22 L 8 22 L 8 20.5 L 11.2 20.5 L 11.2 17.5 C 7.5 17.5 4 14.5 4 10 Z"
    });

    registerIcon({
        "inst_chiptune", "8-Bit Chiptune", "Instruments",
        {"chiptune", "8bit", "arcade", "game", "retro", "nes"},
        // Retro gamepad D-pad and twin action buttons
        "M 3 9 L 6 9 L 6 6 L 9 6 L 9 9 L 12 9 L 12 12 L 9 12 L 9 15 L 6 15 L 6 12 L 3 12 Z "
        "M 14.5 13 A 2 2 0 1 0 14.5 17 A 2 2 0 0 0 14.5 13 Z "
        "M 19 8.5 A 2 2 0 1 0 19 12.5 A 2 2 0 0 0 19 8.5 Z"
    });

    registerIcon({
        "inst_organ", "Tonewheel Organ", "Instruments",
        {"organ", "b3", "tonewheel", "church", "rotary"},
        // Classic B3 drawbars and dual manual keyboard
        "M 4 2 L 6 2 L 6 7 L 4 7 Z M 8 2 L 10 2 L 10 10 L 8 10 Z M 12 2 L 14 2 L 14 5 L 12 5 Z M 16 2 L 18 2 L 18 9 L 16 9 Z "
        "M 3 12 L 21 12 L 21 15 L 3 15 Z M 3 17 L 21 17 L 21 21 L 3 21 Z"
    });

    // ------------------------------------------------------------------------
    // DRUMS & PERCUSSION
    // ------------------------------------------------------------------------
    registerIcon({
        "drum_kick", "Kick Drum", "Drums",
        {"kick", "bassdrum", "808", "punch", "drum"},
        // Bass drum with angled spurs and beater mallet
        "M 4 10.5 A 7.5 7.5 0 1 0 19 10.5 A 7.5 7.5 0 0 0 4 10.5 Z "
        "M 5 15 L 2 20.5 L 3.8 21 L 6.5 16.5 Z "
        "M 18 15 L 21 20.5 L 19.2 21 L 16.5 16.5 Z "
        "M 10 9.5 L 13.5 9.5 L 13.5 12 L 10 12 Z "
        "M 11.2 12 L 12.2 12 L 12 20 L 11 20 Z "
        "M 9 20 L 14 20 L 14 22 L 9 22 Z"
    });

    registerIcon({
        "drum_snare", "Snare Drum", "Drums",
        {"snare", "rim", "clap", "backbeat"},
        // Snare drum with dual rims, lugs and crossed striking sticks
        "M 3 8 L 21 8 L 21 10 L 3 10 Z "
        "M 4 11 L 20 11 L 20 15 L 4 15 Z "
        "M 3 16 L 21 16 L 21 18 L 3 18 Z "
        "M 6 9.5 L 7.5 9.5 L 7.5 16.5 L 6 16.5 Z "
        "M 11.25 9.5 L 12.75 9.5 L 12.75 16.5 L 11.25 16.5 Z "
        "M 16.5 9.5 L 18 9.5 L 18 16.5 L 16.5 16.5 Z "
        "M 2 3 L 12 7.5 L 11.5 8.5 L 1.5 4 Z "
        "M 22 3 L 12 7.5 L 12.5 8.5 L 22.5 4 Z"
    });

    registerIcon({
        "drum_hihat", "Hi-Hat", "Drums",
        {"hihat", "hats", "cymbals", "pedal", "metallic"},
        // Dual cymbals on stand with center rod and pedal
        "M 12 4 L 3 8 L 21 8 Z "
        "M 3 10 L 21 10 L 12 12.5 Z "
        "M 11.25 2 L 12.75 2 L 12.75 20 L 11.25 20 Z "
        "M 12 17 L 7 22 L 8.5 22.5 L 12 18.5 L 15.5 22.5 L 17 22 Z "
        "M 9.5 20.5 L 14.5 20.5 L 14.5 22 L 9.5 22 Z"
    });

    registerIcon({
        "drum_clap", "Clap", "Drums",
        {"clap", "snap", "hands", "percussion"},
        // Dynamic acoustic shockwave impact burst
        "M 11 1 L 13 1 L 12 5.5 Z "
        "M 11 23 L 13 23 L 12 18.5 Z "
        "M 1 11 L 1 13 L 5.5 12 Z "
        "M 23 11 L 23 13 L 18.5 12 Z "
        "M 4 4 L 6 4 L 8.5 7 Z "
        "M 20 4 L 18 4 L 15.5 7 Z "
        "M 4 20 L 6 20 L 8.5 17 Z "
        "M 20 20 L 18 20 L 15.5 17 Z "
        "M 10 10 L 14 10 L 14 14 L 10 14 Z"
    });

    registerIcon({
        "drum_tom", "Tom Drum", "Drums",
        {"tom", "toms", "floor", "rack", "drum"},
        // Deep rack tom drum angled with mount
        "M 3 6 L 21 6 L 21 8.5 L 3 8.5 Z "
        "M 4 9.5 L 20 9.5 L 18 18 L 6 18 Z "
        "M 5 18.8 L 19 18.8 L 18.5 20.5 L 5.5 20.5 Z "
        "M 10 12.5 L 14 12.5 L 14 14.5 L 10 14.5 Z"
    });

    registerIcon({
        "drum_machine", "Drum Machine", "Drums",
        {"machine", "step", "sequencer", "sampler", "pads", "808"},
        // 9-Pad MPC sampler matrix with top display
        "M 3 2.5 L 21 2.5 L 21 5.5 L 3 5.5 Z "
        "M 3 7 L 7.8 7 L 7.8 10.8 L 3 10.8 Z "
        "M 9.6 7 L 14.4 7 L 14.4 10.8 L 9.6 10.8 Z "
        "M 16.2 7 L 21 7 L 21 10.8 L 16.2 10.8 Z "
        "M 3 12.3 L 7.8 12.3 L 7.8 16.1 L 3 16.1 Z "
        "M 9.6 12.3 L 14.4 12.3 L 14.4 16.1 L 9.6 16.1 Z "
        "M 16.2 12.3 L 21 12.3 L 21 16.1 L 16.2 16.1 Z "
        "M 3 17.6 L 7.8 17.6 L 7.8 21.4 L 3 21.4 Z "
        "M 9.6 17.6 L 14.4 17.6 L 14.4 21.4 L 9.6 21.4 Z "
        "M 16.2 17.6 L 21 17.6 L 21 21.4 L 16.2 21.4 Z"
    });

    // ------------------------------------------------------------------------
    // FX & MODULAR
    // ------------------------------------------------------------------------
    registerIcon({
        "fx_reverb", "Reverb", "FX & Mod",
        {"reverb", "room", "hall", "space", "echo", "ambience"},
        // Acoustic parabolic expanding reflections
        "M 12 11 A 3 3 0 0 1 15 14 L 13.5 14 A 1.5 1.5 0 0 0 12 12.5 A 1.5 1.5 0 0 0 10.5 14 L 9 14 A 3 3 0 0 1 12 11 Z "
        "M 12 7 A 7 7 0 0 1 19 14 L 17.5 14 A 5.5 5.5 0 0 0 12 8.5 A 5.5 5.5 0 0 0 6.5 14 L 5 14 A 7 7 0 0 1 12 7 Z "
        "M 12 3 A 11 11 0 0 1 23 14 L 21.5 14 A 9.5 9.5 0 0 0 12 4.5 A 9.5 9.5 0 0 0 2.5 14 L 1 14 A 11 11 0 0 1 12 3 Z "
        "M 2 16 L 22 16 L 22 17.5 L 2 17.5 Z M 4 19.5 L 20 19.5 L 20 20.8 L 4 20.8 Z"
    });

    registerIcon({
        "fx_delay", "Delay / Echo", "FX & Mod",
        {"delay", "echo", "repeat", "tape", "pingpong"},
        // Clock dial repeat loop with trailing echo taps
        "M 12 2 A 10 10 0 1 0 22 12 L 20 12 A 8 8 0 1 1 12 4 Z "
        "M 11 6 L 13 6 L 13 12 L 17 14 L 16 16 L 11 13 Z "
        "M 21 6 A 4 4 0 0 1 22 10 L 23.5 9.5 A 5.5 5.5 0 0 0 22 4.5 Z"
    });

    registerIcon({
        "fx_filter", "Filter", "FX & Mod",
        {"filter", "cutoff", "resonance", "lowpass", "highpass"},
        // 24dB resonant lowpass filter frequency curve
        "M 2 8 L 10 8 C 11.5 8 12.5 3 13.5 3 C 14.5 3 15 7 16 10 L 22 21 L 19.5 21 L 15 12 C 14.5 10 14 6 13.5 6 C 13 6 12 10 10.5 10 L 2 10 Z "
        "M 2 21 L 18 21 L 18 19.5 L 2 19.5 Z"
    });

    registerIcon({
        "fx_distortion", "Distortion / Drive", "FX & Mod",
        {"dist", "distortion", "overdrive", "fuzz", "clip", "saturation"},
        // High-energy lightning drive bolt
        "M 13 2 L 4 13 L 11 13 L 9 22 L 20 10 L 13 10 Z"
    });

    registerIcon({
        "fx_compressor", "Compressor", "FX & Mod",
        {"comp", "compressor", "limiter", "dynamics", "sidechain"},
        // Dynamic clamping limiter arrows
        "M 3 3 L 5.5 3 L 5.5 21 L 3 21 Z "
        "M 18.5 3 L 21 3 L 21 21 L 18.5 21 Z "
        "M 8 7 L 12 12 L 8 17 Z "
        "M 16 7 L 12 12 L 16 17 Z"
    });

    registerIcon({
        "fx_eq", "Equalizer", "FX & Mod",
        {"eq", "equalizer", "parametric", "tone", "curve"},
        // Graphic parametric EQ fader sliders
        "M 3 4 L 5 4 L 5 20 L 3 20 Z "
        "M 2 11 L 6 11 L 6 13 L 2 13 Z "
        "M 11 4 L 13 4 L 13 20 L 11 20 Z "
        "M 10 7 L 14 7 L 14 9 L 10 9 Z "
        "M 19 4 L 21 4 L 21 20 L 19 20 Z "
        "M 18 14 L 22 14 L 22 16 L 18 16 Z"
    });

    registerIcon({
        "fx_chorus", "Chorus / Ensemble", "FX & Mod",
        {"chorus", "ensemble", "flanger", "phaser", "width", "mod"},
        // Dual phase-shifted stereo chorus ribbons
        "M 2 8 C 5 2 9 2 12 8 C 15 14 19 14 22 8 L 22 10.5 C 19 16.5 15 16.5 12 10.5 C 9 4.5 5 4.5 2 10.5 Z "
        "M 2 13.5 C 5 7.5 9 7.5 12 13.5 C 15 19.5 19 19.5 22 13.5 L 22 16 C 19 22 15 22 12 16 C 9 10 5 10 2 16 Z"
    });

    registerIcon({
        "fx_modular", "Patch / Modular", "FX & Mod",
        {"modular", "cable", "eurorack", "jack", "cv", "patch"},
        // Eurorack 3.5mm jack socket and patch plug
        "M 12 2 A 4.5 4.5 0 0 0 7.5 6.5 L 7.5 10 A 4.5 4.5 0 0 0 12 14.5 A 4.5 4.5 0 0 0 16.5 10 L 16.5 6.5 A 4.5 4.5 0 0 0 12 2 Z "
        "M 10.5 14.5 L 10.5 21 C 10.5 22.5 13.5 22.5 13.5 21 L 13.5 14.5 Z "
        "M 10.5 5 L 13.5 5 L 13.5 8 L 10.5 8 Z"
    });

    // ------------------------------------------------------------------------
    // HARDWARE & GENERAL
    // ------------------------------------------------------------------------
    registerIcon({
        "hw_speaker", "Monitor Speaker", "Hardware",
        {"speaker", "monitor", "sound", "volume", "audio"},
        // Studio acoustic monitor speaker radiating waves
        "M 2 9 L 6 9 L 12 4 L 12 20 L 6 15 L 2 15 Z "
        "M 15 8 C 16.5 9.2 17.5 10.5 17.5 12 C 17.5 13.5 16.5 14.8 15 16 L 16.2 17.5 C 18.2 15.8 19.5 14 19.5 12 C 19.5 10 18.2 8.2 16.2 6.5 Z "
        "M 18.5 5 C 20.8 7 22 9.3 22 12 C 22 14.7 20.8 17 18.5 19 L 19.7 20.5 C 22.5 18.2 24 15.3 24 12 C 24 8.7 22.5 5.8 19.7 3.5 Z"
    });

    registerIcon({
        "hw_headphones", "Headphones", "Hardware",
        {"headphones", "phones", "cue", "listen", "monitor"},
        // Studio monitoring headphones
        "M 4 12 A 8 8 0 0 1 20 12 L 21.5 12 A 9.5 9.5 0 0 0 2.5 12 Z "
        "M 2 11.5 L 5.5 11.5 L 5.5 18 L 2 18 Z "
        "M 18.5 11.5 L 22 11.5 L 22 18 L 18.5 18 Z "
        "M 3.5 9.5 L 5.5 9.5 L 5.5 11.5 L 3.5 11.5 Z "
        "M 18.5 9.5 L 20.5 9.5 L 20.5 11.5 L 18.5 11.5 Z"
    });

    registerIcon({
        "hw_cassette", "Cassette Tape", "Hardware",
        {"cassette", "tape", "retro", "lofi", "analog"},
        // Vintage cassette tape with twin reels, bridge and label
        "M 3 4 L 21 4 L 21 6.5 L 3 6.5 Z "
        "M 3 6.5 L 5 6.5 L 5 17.5 L 3 17.5 Z "
        "M 19 6.5 L 21 6.5 L 21 17.5 L 19 17.5 Z "
        "M 6.5 9 A 2.5 2.5 0 1 0 6.5 14 A 2.5 2.5 0 0 0 6.5 9 Z "
        "M 17.5 9 A 2.5 2.5 0 1 0 17.5 14 A 2.5 2.5 0 0 0 17.5 9 Z "
        "M 9.5 10.5 L 14.5 10.5 L 14.5 12.5 L 9.5 12.5 Z "
        "M 6 16 L 18 16 L 16 20.5 L 8 20.5 Z"
    });

    registerIcon({
        "hw_midi", "MIDI DIN", "Hardware",
        {"midi", "din", "cable", "sync", "interface", "hardware"},
        // 5-Pin DIN MIDI port
        "M 12 2 A 10 10 0 1 0 22 12 L 20 12 A 8 8 0 1 1 12 4 Z "
        "M 6.5 9 A 1.2 1.2 0 1 0 6.5 11.4 A 1.2 1.2 0 0 0 6.5 9 Z "
        "M 9 6.5 A 1.2 1.2 0 1 0 9 8.9 A 1.2 1.2 0 0 0 9 6.5 Z "
        "M 12 5.5 A 1.2 1.2 0 1 0 12 7.9 A 1.2 1.2 0 0 0 12 5.5 Z "
        "M 15 6.5 A 1.2 1.2 0 1 0 15 8.9 A 1.2 1.2 0 0 0 15 6.5 Z "
        "M 17.5 9 A 1.2 1.2 0 1 0 17.5 11.4 A 1.2 1.2 0 0 0 17.5 9 Z "
        "M 11 16.5 L 13 16.5 L 13 19 L 11 19 Z"
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
        "M 2 5 L 22 5 L 22 8 L 2 8 Z "
        "M 2 10 L 5 10 L 5 21 L 2 21 Z "
        "M 6.2 10 L 9.2 10 L 9.2 21 L 6.2 21 Z "
        "M 10.4 10 L 13.4 10 L 13.4 21 L 10.4 21 Z "
        "M 14.6 10 L 17.6 10 L 17.6 21 L 14.6 21 Z "
        "M 18.8 10 L 21.8 10 L 21.8 21 L 18.8 21 Z",
        {0, 0, 24, 24}, true
    });

    registerIcon({
        "ui_properties", "Properties Sliders Widget", "UI",
        {"ui", "sliders", "drawer", "properties"},
        "M 2 6 L 22 6 L 22 8 L 2 8 Z M 2 11 L 22 11 L 22 13 L 2 13 Z M 2 16 L 22 16 L 22 18 L 2 18 Z "
        "M 6 4 L 10 4 L 10 10 L 6 10 Z M 14 9 L 18 9 L 18 15 L 14 15 Z M 8 14 L 12 14 L 12 20 L 8 20 Z",
        {0, 0, 24, 24}, true
    });

    // ------------------------------------------------------------------------
    // FLUTTER-PARITY BROWSER WORKSTATION HUB & CATEGORY ICONS
    // ------------------------------------------------------------------------
    // Tab 1: Project Assets (Icons.inventory_2_outlined)
    registerIcon({
        "ui_tab_assets", "Project Assets Tab", "UI",
        {"ui", "browser", "assets", "inventory", "box"},
        "M 3 3 L 21 3 L 21 6.5 L 3 6.5 Z "
        "M 4 7.5 L 6.5 7.5 L 6.5 21 L 4 21 Z "
        "M 17.5 7.5 L 20 7.5 L 20 21 L 17.5 21 Z "
        "M 6.5 18.5 L 17.5 18.5 L 17.5 21 L 6.5 21 Z "
        "M 9 12 L 15 12 L 15 14.5 L 9 14.5 Z",
        {0, 0, 24, 24}, true
    });

    // Tab 2: Script & Engine Library (Icons.code)
    registerIcon({
        "ui_tab_script", "Script Library Tab", "UI",
        {"ui", "browser", "script", "code", "engine"},
        "M 8 5 L 2.5 12 L 8 19 L 9.5 17.5 L 5.5 12 L 9.5 6.5 Z "
        "M 16 5 L 14.5 6.5 L 18.5 12 L 14.5 17.5 L 16 19 L 21.5 12 Z "
        "M 13.5 4 L 9.5 20 L 11 20 L 15 4 Z",
        {0, 0, 24, 24}, true
    });

    // Tab 3: Preset Library (Icons.library_music_outlined)
    registerIcon({
        "ui_tab_preset", "Preset Library Tab", "UI",
        {"ui", "browser", "preset", "library", "soundpatch"},
        "M 2 6 L 4 6 L 4 21 L 2 21 Z "
        "M 4 19 L 17 19 L 17 21 L 4 21 Z "
        "M 6 2 L 22 2 L 22 4 L 6 4 Z "
        "M 6 4 L 8 4 L 8 18 L 6 18 Z "
        "M 20 4 L 22 4 L 22 18 L 20 18 Z "
        "M 6 16 L 22 16 L 22 18 L 6 18 Z "
        "M 15 6 L 17 6 L 17 12 L 15 12 Z "
        "M 12 11 L 16 11 L 16 14.5 L 12 14.5 Z",
        {0, 0, 24, 24}, true
    });

    // Tab 4: Expansion Packs (Icons.cloud_download_outlined)
    registerIcon({
        "ui_tab_packs", "Expansion Packs Tab", "UI",
        {"ui", "browser", "packs", "download", "cloud"},
        "M 3 15 L 7 15 L 7 17 L 3 17 Z "
        "M 17 15 L 21 15 L 21 17 L 17 17 Z "
        "M 9 5 L 15 5 L 15 7 L 9 7 Z "
        "M 5 10 L 9 6 L 10.5 7.5 L 6.5 11.5 Z "
        "M 19 10 L 17.5 11.5 L 13.5 7.5 L 15 6 Z "
        "M 3 11 L 5 11 L 5 15 L 3 15 Z "
        "M 19 11 L 21 11 L 21 15 L 19 15 Z "
        "M 11 9 L 13 9 L 13 14 L 11 14 Z "
        "M 8.5 13.5 L 12 18.5 L 15.5 13.5 L 14 13.5 L 12 16.5 L 10 13.5 Z",
        {0, 0, 24, 24}, true
    });

    // Tab 5: Local Saved Projects (Icons.folder_special_outlined)
    registerIcon({
        "ui_tab_projects", "Saved Projects Tab", "UI",
        {"ui", "browser", "projects", "folder", "star"},
        "M 2 5 L 8 5 L 10 7.5 L 22 7.5 L 22 9.5 L 2 9.5 Z "
        "M 2 9.5 L 4 9.5 L 4 20 L 2 20 Z "
        "M 20 9.5 L 22 9.5 L 22 20 L 20 20 Z "
        "M 2 18 L 22 18 L 22 20 L 2 20 Z "
        "M 12 9 L 13.2 12.5 L 17 12.5 L 14 14.8 L 15.2 18.5 L 12 16.2 L 8.8 18.5 L 10 14.8 L 7 12.5 L 10.8 12.5 Z",
        {0, 0, 24, 24}, true
    });

    // Tab 6: History & Time Travel (Icons.history)
    registerIcon({
        "ui_tab_history", "History & Undo Tab", "UI",
        {"ui", "browser", "history", "undo", "redo", "clock"},
        "M 12 4 L 18 4 L 18 6 L 12 6 Z "
        "M 18 6 L 20 6 L 20 18 L 18 18 Z "
        "M 6 18 L 18 18 L 18 20 L 6 20 Z "
        "M 4 11 L 6 11 L 6 18 L 4 18 Z "
        "M 2 8 L 8 8 L 5 4 Z "
        "M 11 8 L 13 8 L 13 13 L 11 13 Z "
        "M 11 11 L 16 11 L 16 13 L 11 13 Z",
        {0, 0, 24, 24}, true
    });

    // Script Filter Chip: All (Icons.apps)
    registerIcon({
        "ui_cat_all", "All Presets Chip", "UI",
        {"ui", "chip", "all", "apps", "grid"},
        "M 3 3 L 7.5 3 L 7.5 7.5 L 3 7.5 Z "
        "M 9.75 3 L 14.25 3 L 14.25 7.5 L 9.75 7.5 Z "
        "M 16.5 3 L 21 3 L 21 7.5 L 16.5 7.5 Z "
        "M 3 9.75 L 7.5 9.75 L 7.5 14.25 L 3 14.25 Z "
        "M 9.75 9.75 L 14.25 9.75 L 14.25 14.25 L 9.75 14.25 Z "
        "M 16.5 9.75 L 21 9.75 L 21 14.25 L 16.5 14.25 Z "
        "M 3 16.5 L 7.5 16.5 L 7.5 21 L 3 21 Z "
        "M 9.75 16.5 L 14.25 16.5 L 14.25 21 L 9.75 21 Z "
        "M 16.5 16.5 L 21 16.5 L 21 21 L 16.5 21 Z",
        {0, 0, 24, 24}, true
    });

    // Script Filter Chip: Instruments (Icons.piano)
    registerIcon({
        "ui_cat_instruments", "Synth Instruments Chip", "UI",
        {"ui", "chip", "instruments", "piano", "synth"},
        "M 2 5 L 22 5 L 22 8 L 2 8 Z "
        "M 2 9.5 L 5.2 9.5 L 5.2 21 L 2 21 Z "
        "M 6.2 9.5 L 9.4 9.5 L 9.4 21 L 6.2 21 Z "
        "M 10.4 9.5 L 13.6 9.5 L 13.6 21 L 10.4 21 Z "
        "M 14.6 9.5 L 17.8 9.5 L 17.8 21 L 14.6 21 Z "
        "M 18.8 9.5 L 22 9.5 L 22 21 L 18.8 21 Z",
        {0, 0, 24, 24}, true
    });

    // Script Filter Chip: Audio FX (Icons.graphic_eq)
    registerIcon({
        "ui_cat_fx", "Audio FX Chip", "UI",
        {"ui", "chip", "fx", "audio", "eq", "bars"},
        "M 1.5 10 L 3.8 10 L 3.8 14 L 1.5 14 Z "
        "M 6 6 L 8.5 6 L 8.5 18 L 6 18 Z "
        "M 10.7 3 L 13.3 3 L 13.3 21 L 10.7 21 Z "
        "M 15.5 6 L 18 6 L 18 18 L 15.5 18 Z "
        "M 20.2 10 L 22.5 10 L 22.5 14 L 20.2 14 Z",
        {0, 0, 24, 24}, true
    });

    // Script Filter Chip: MIDI FX (Icons.music_note)
    registerIcon({
        "ui_cat_midifx", "MIDI FX Chip", "UI",
        {"ui", "chip", "midifx", "note", "music"},
        "M 11 3 L 17 3 L 17 6.5 L 12.5 6.5 Z "
        "M 11 3 L 13 3 L 13 14 L 11 14 Z "
        "M 6.5 12 L 12 12 L 12 16 L 6.5 16 Z",
        {0, 0, 24, 24}, true
    });

    // Script Filter Chip: MIDI Sequences (Icons.view_timeline_outlined)
    registerIcon({
        "ui_cat_seq", "MIDI Sequences Chip", "UI",
        {"ui", "chip", "seq", "timeline", "pattern"},
        "M 3 4 L 21 4 L 21 6 L 3 6 Z "
        "M 3 18 L 21 18 L 21 20 L 3 20 Z "
        "M 3 6 L 5 6 L 5 18 L 3 18 Z "
        "M 19 6 L 21 6 L 21 18 L 19 18 Z "
        "M 6 8 L 12 8 L 12 10 L 6 10 Z "
        "M 10 11.5 L 18 11.5 L 18 13.5 L 10 13.5 Z "
        "M 7 15 L 15 15 L 15 17 L 7 17 Z",
        {0, 0, 24, 24}, true
    });

    // Script Filter Chip: Macros (Icons.auto_awesome)
    registerIcon({
        "ui_cat_macro", "Macros & Automation Chip", "UI",
        {"ui", "chip", "macro", "sparkle", "stars", "auto"},
        "M 10 5 L 11.5 9.5 L 16 11 L 11.5 12.5 L 10 17 L 8.5 12.5 L 4 11 L 8.5 9.5 Z "
        "M 18 2 L 19 4.5 L 21.5 5.5 L 19 6.5 L 18 9 L 17 6.5 L 14.5 5.5 L 17 4.5 Z "
        "M 18 14 L 19 16.5 L 21.5 17.5 L 19 18.5 L 18 21 L 17 18.5 L 14.5 17.5 L 17 16.5 Z",
        {0, 0, 24, 24}, true
    });
}

} // namespace eatsbits::ui
