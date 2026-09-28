#ifndef EATS_SVG_PATH_HPP
#define EATS_SVG_PATH_HPP

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace eatsbits::ui {

/**
 * 2D vector coordinate primitive.
 */
struct SvgPoint {
    float x{0.0f};
    float y{0.0f};

    constexpr SvgPoint() = default;
    constexpr SvgPoint(float px, float py) : x(px), y(py) {}

    bool operator==(const SvgPoint& o) const noexcept {
        return std::abs(x - o.x) < 1e-5f && std::abs(y - o.y) < 1e-5f;
    }
};

/**
 * 2D Axis-Aligned Bounding Box.
 */
struct SvgRect {
    float minX{1e9f};
    float minY{1e9f};
    float maxX{-1e9f};
    float maxY{-1e9f};

    [[nodiscard]] constexpr float width() const noexcept { return isEmpty() ? 0.0f : maxX - minX; }
    [[nodiscard]] constexpr float height() const noexcept { return isEmpty() ? 0.0f : maxY - minY; }
    [[nodiscard]] constexpr bool isEmpty() const noexcept { return minX > maxX || minY > maxY; }
    [[nodiscard]] constexpr SvgPoint center() const noexcept {
        return {(minX + maxX) * 0.5f, (minY + maxY) * 0.5f};
    }

    void expandToInclude(const SvgPoint& p) noexcept {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
};

/**
 * Declarative vector layer definition matching Eatsbeats' SvgLayerDef.
 * Used for custom instrument/FX panel watermarks, vector skins, and logos.
 */
struct SvgLayerDef {
    std::string path;
    uint32_t color{0xFFFFFFFF}; // RGBA8 packed
    float opacity{1.0f};
    bool isFill{true};
    float strokeWidth{1.0f};

    SvgLayerDef() = default;
    SvgLayerDef(std::string p, uint32_t c, float op = 1.0f, bool fill = true, float stroke = 1.0f)
        : path(std::move(p)), color(c), opacity(op), isFill(fill), strokeWidth(stroke) {}
};

/**
 * High-performance, zero-allocation SVG path parser, polygon tessellator,
 * and software rasterizer.
 * Supports standard SVG path syntax: M, m, L, l, H, h, V, v, C, c, S, s, Q, q, T, t, A, a, Z, z.
 */
class SvgPath {
public:
    SvgPath() = default;

    /**
     * Parses an SVG path data string (`d="..."`) into native discretized contours.
     * Caches parsed results in-memory so identical path definitions incur zero overhead.
     */
    static SvgPath parse(std::string_view d);

    /**
     * Clears the in-memory path cache.
     */
    static void clearCache();

    [[nodiscard]] const std::vector<std::vector<SvgPoint>>& getContours() const noexcept { return contours_; }
    [[nodiscard]] const SvgRect& getBounds() const noexcept { return bounds_; }
    [[nodiscard]] bool empty() const noexcept { return contours_.empty(); }

    /**
     * Tessellates all closed contours into triangles using ear-clipping.
     * Output is a flat vector of vertices where every 3 points form a triangle.
     */
    [[nodiscard]] std::vector<SvgPoint> triangulate() const;

    /**
     * Rasterizes the path into a 32-bit RGBA pixel buffer at the specified dimensions.
     * Scaled and centered to fit within (width, height) with optional padding.
     */
    [[nodiscard]] std::vector<uint8_t> rasterize(int width, int height,
                                                 uint32_t color = 0xFFFFFFFF,
                                                 bool isFill = true,
                                                 float strokeWidth = 1.0f,
                                                 float padding = 0.0f,
                                                 const SvgRect* customBounds = nullptr) const;

    /**
     * Adds an explicit contour of points.
     */
    void addContour(std::vector<SvgPoint> contour);

private:
    std::vector<std::vector<SvgPoint>> contours_;
    SvgRect bounds_{0.0f, 0.0f, 0.0f, 0.0f};

    void computeBounds();
};

/**
 * Texture cache for GPU-accelerated rasterized SVG paths and multi-layer watermarks.
 * Caches texture IDs across frames for high-performance zero-allocation UI rendering.
 */
class SvgTextureCache {
public:
    static SvgTextureCache& instance();

    ~SvgTextureCache();

    /**
     * Retrieves or rasterizes a multi-layer SVG definition into a GPU texture ID.
     * Returns 0 if rasterization fails or OpenGL is not initialized.
     */
    uint32_t getOrCreateTexture(const std::string& cacheKey,
                                const std::vector<SvgLayerDef>& layers,
                                int width, int height,
                                float scale = 1.0f);

    /**
     * Rasterizes an SVG layer into an RGBA pixel buffer.
     */
    static std::vector<uint8_t> rasterizeLayersToRgba(const std::vector<SvgLayerDef>& layers,
                                                      int width, int height,
                                                      float scale = 1.0f);

    /**
     * Frees all GPU textures and clears cache.
     */
    void shutdown();

private:
    SvgTextureCache() = default;

    struct CachedTexture {
        uint32_t textureId{0};
        int width{0};
        int height{0};
    };

    std::unordered_map<std::string, CachedTexture> cache_;
};

/**
 * Canonical Eatsbits Logo geometry represented in 100% pure SVG paths.
 * No <rect> or <circle> elements used.
 */
struct SvgLogo {
    // 1. Amber Chassis Square with rx=80 rounded corners
    static constexpr const char* kAmberChassisPath =
        "M 112 32 L 400 32 A 80 80 0 0 1 480 112 L 480 400 A 80 80 0 0 1 400 480 "
        "L 112 480 A 80 80 0 0 1 32 400 L 32 112 A 80 80 0 0 1 112 32 Z";

    // 2. Weathered Dark Monster Head, Open Mouth & Two Eating Bits
    static constexpr const char* kMonsterHeadAndBitsPath =
        "M 100 100 L 380 100 L 380 220 L 220 220 L 220 340 L 380 340 L 380 410 L 100 410 Z "
        "M 408 230 L 440 230 A 8 8 0 0 1 448 238 L 448 270 A 8 8 0 0 1 440 278 L 408 278 A 8 8 0 0 1 400 270 L 400 238 A 8 8 0 0 1 408 230 Z "
        "M 406 320 L 430 320 A 6 6 0 0 1 436 326 L 436 350 A 6 6 0 0 1 430 356 L 406 356 A 6 6 0 0 1 400 350 L 400 326 A 6 6 0 0 1 406 320 Z";

    // 3. Amber Monster Eye circular arc
    static constexpr const char* kMonsterEyePath =
        "M 228 160 A 32 32 0 1 0 292 160 A 32 32 0 1 0 228 160 Z";

    // 4. Authentic Unboxed Silk-Screened Faceplate Logo (Existing SVG inner logo without outer square)
    static constexpr const char* kPlainLogoPath =
        "M 100 100 L 380 100 L 380 220 L 220 220 L 220 340 L 380 340 L 380 410 L 100 410 Z "
        "M 228 160 A 32 32 0 1 0 292 160 A 32 32 0 1 0 228 160 Z "
        "M 408 230 L 440 230 A 8 8 0 0 1 448 238 L 448 270 A 8 8 0 0 1 440 278 L 408 278 A 8 8 0 0 1 400 270 L 400 238 A 8 8 0 0 1 408 230 Z "
        "M 406 320 L 430 320 A 6 6 0 0 1 436 326 L 436 350 A 6 6 0 0 1 430 356 L 406 356 A 6 6 0 0 1 400 350 L 400 326 A 6 6 0 0 1 406 320 Z";

    // 5. Two Eating Bits
    static constexpr const char* kMonsterBitsPath =
        "M 408 230 L 440 230 A 8 8 0 0 1 448 238 L 448 270 A 8 8 0 0 1 440 278 L 408 278 A 8 8 0 0 1 400 270 L 400 238 A 8 8 0 0 1 408 230 Z "
        "M 406 320 L 430 320 A 6 6 0 0 1 436 326 L 436 350 A 6 6 0 0 1 430 356 L 406 356 A 6 6 0 0 1 400 350 L 400 326 A 6 6 0 0 1 406 320 Z";

    /**
     * Color definitions:
     * Warm Amber: #FF8C00 (R=255, G=140, B=0, A=255)
     * Weathered Dark: #141210 (R=20, G=18, B=16, A=255)
     */
    static constexpr uint32_t kColorAmber = 0xFF008CFF; // ABGR/RGBA packed (R=0xFF, G=0x8C, B=0x00, A=0xFF)
    static constexpr uint32_t kColorDark  = 0xFF101214; // ABGR/RGBA packed (R=0x14, G=0x12, B=0x10, A=0xFF)

    /**
     * Returns the 3-layer vector stack for the Eatsbits brand logo.
     */
    static std::vector<SvgLayerDef> getLayers();

    /**
     * Returns the inverted vector stack for hover state.
     */
    static std::vector<SvgLayerDef> getInvertedLayers();

    /**
     * Returns the unboxed plain vector stack matching the vintage hardware faceplate silk-screen.
     */
    static std::vector<SvgLayerDef> getPlainLayers(uint32_t color = kColorAmber);

    /**
     * Returns the inverted unboxed plain vector stack for hover state.
     */
    static std::vector<SvgLayerDef> getPlainInvertedLayers(uint32_t accentColor = kColorAmber, uint32_t darkColor = kColorDark);
};

} // namespace eatsbits::ui

#endif // EATS_SVG_PATH_HPP
