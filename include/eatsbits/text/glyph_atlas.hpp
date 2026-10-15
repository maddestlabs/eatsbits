#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include "font_manager.hpp"

namespace eatsbits::text {

struct GlyphKey {
    FontId fontId{kInvalidFontId};
    uint32_t glyphIndex{0};
    uint16_t pixelSize{0};
    uint8_t subpixelBin{0}; // 0..3 (quarter-pixel offsets: 0.0, 0.25, 0.5, 0.75)

    bool operator==(const GlyphKey& o) const noexcept {
        return fontId == o.fontId &&
               glyphIndex == o.glyphIndex &&
               pixelSize == o.pixelSize &&
               subpixelBin == o.subpixelBin;
    }
};

struct GlyphKeyHash {
    size_t operator()(const GlyphKey& k) const noexcept {
        size_t h = 0x811c9dc5;
        h = (h ^ static_cast<size_t>(k.fontId)) * 0x01000193;
        h = (h ^ static_cast<size_t>(k.glyphIndex)) * 0x01000193;
        h = (h ^ static_cast<size_t>(k.pixelSize)) * 0x01000193;
        h = (h ^ static_cast<size_t>(k.subpixelBin)) * 0x01000193;
        return h;
    }
};

struct AtlasGlyph {
    int atlasX{0};
    int atlasY{0};
    int width{0};
    int height{0};
    float bearingX{0.0f};
    float bearingY{0.0f};
    float advanceX{0.0f};
    float u0{0.0f};
    float v0{0.0f};
    float u1{0.0f};
    float v1{0.0f};
    uint64_t lastUsedFrame{0};
};

class GlyphAtlas {
public:
    explicit GlyphAtlas(int width = 1024, int height = 1024);
    ~GlyphAtlas();

    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;
    GlyphAtlas(GlyphAtlas&&) noexcept;
    GlyphAtlas& operator=(GlyphAtlas&&) noexcept;

    int getWidth() const noexcept { return width_; }
    int getHeight() const noexcept { return height_; }
    const uint8_t* getBuffer() const noexcept { return buffer_.data(); }

    void newFrame() noexcept { ++currentFrame_; }

    const AtlasGlyph* getOrLoadGlyph(FontManager& fontMgr, FontId fontId, uint32_t glyphIndex,
                                     float fontSize, float subpixelX = 0.0f);

    bool isDirty() const noexcept { return isDirty_; }
    void getDirtyBounds(int& outX, int& outY, int& outW, int& outH) const noexcept;
    void clearDirty() noexcept;

    void clear();

    size_t getCachedGlyphCount() const noexcept { return glyphCache_.size(); }
    uint64_t getCacheHits() const noexcept { return cacheHits_; }
    uint64_t getCacheMisses() const noexcept { return cacheMisses_; }

private:
    bool allocateSlot(int glyphW, int glyphH, int& outX, int& outY);
    void markDirty(int x, int y, int w, int h) noexcept;

    int width_{1024};
    int height_{1024};
    std::vector<uint8_t> buffer_;

    // Shelf bin packer state
    int shelfX_{1};
    int shelfY_{1};
    int shelfHeight_{0};

    std::unordered_map<GlyphKey, AtlasGlyph, GlyphKeyHash> glyphCache_;

    bool isDirty_{false};
    int dirtyMinX_{0};
    int dirtyMinY_{0};
    int dirtyMaxX_{0};
    int dirtyMaxY_{0};
    uint64_t currentFrame_{0};
    uint64_t cacheHits_{0};
    uint64_t cacheMisses_{0};
};

} // namespace eatsbits::text
