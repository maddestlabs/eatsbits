#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include "geometry.hpp"
#include "theme.hpp"
#include "svg_path.hpp"
#include "batch_renderer_2d.hpp"

namespace eatsbits::ui {

/**
 * Definition of an icon in the Eatsbits vector library.
 */
struct IconDef {
    std::string id;              // Unique identifier, e.g. "inst_synth", "drum_kick"
    std::string displayName;     // Human-readable title, e.g. "Poly Synth", "Kick Drum"
    std::string category;        // "Instruments", "Drums", "FX & Mod", "Hardware", "UI"
    std::vector<std::string> tags; // Search keywords, e.g. {"keys", "lead", "analog"}
    std::string svgPath;         // SVG 'd' path data string
    SvgRect viewBox{0.0f, 0.0f, 24.0f, 24.0f}; // Nominal viewBox coordinate frame
    bool isStockUiOnly{false};   // True if internal UI widget glyph (hidden from track icon picker)
};

/**
 * Centralized, zero-allocation registry of vector icons.
 * Provides:
 * - Built-in categorized icon definitions.
 * - Sub-millisecond search and category filtering.
 * - Safe parsing and validation of user-pasted SVG paths or <svg> elements.
 * - Hardware-accelerated rendering through BatchRenderer2D.
 */
class IconRegistry {
public:
    static IconRegistry& instance();

    // Icon query and lookup
    void registerIcon(IconDef def);
    [[nodiscard]] const IconDef* findIcon(const std::string& id) const;
    [[nodiscard]] std::vector<const IconDef*> getAllIcons() const;
    [[nodiscard]] std::vector<std::string> getCategories() const;
    [[nodiscard]] std::vector<const IconDef*> query(std::string_view searchTerm, 
                                                    std::string_view category = "") const;

    // Helper: parses user clipboard text (supports raw 'd' path or full <svg> element)
    static std::optional<IconDef> parsePastedSvg(std::string_view text, 
                                                 const std::string& customId = "custom",
                                                 const std::string& displayName = "Custom Icon");

    // Renders an icon into the given bounding box with tint color.
    // Supports "preset:<id>", plain "<id>", or "svg:<path_data>".
    void renderIcon(BatchRenderer2D& r, const std::string& iconRef, 
                    const Rect2D& bounds, const Color& tint);

    void renderIcon(BatchRenderer2D& r, const std::string& iconRef,
                    float x, float y, float size, const Color& tint) {
        renderIcon(r, iconRef, Rect2D{x, y, size, size}, tint);
    }

private:
    IconRegistry();
    void initStockLibrary();

    std::unordered_map<std::string, IconDef> icons_;
    std::vector<std::string> order_; // Preservation of insertion order

    // Cached triangulated vector geometry for zero-allocation rendering
    struct CachedMesh {
        std::vector<SvgPoint> triangles;
        SvgRect bounds;
    };
    mutable std::unordered_map<std::string, CachedMesh> meshCache_;
};

} // namespace eatsbits::ui
