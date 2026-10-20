#include "eatsbits/text/glyph_atlas.hpp"
#include <cstring>
#include <algorithm>

namespace eatsbits::text {

GlyphAtlas::GlyphAtlas(int width, int height)
    : width_(std::max(64, width))
    , height_(std::max(64, height))
    , buffer_(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0)
{
}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlas::GlyphAtlas(GlyphAtlas&& o) noexcept
    : width_(o.width_)
    , height_(o.height_)
    , buffer_(std::move(o.buffer_))
    , shelfX_(o.shelfX_)
    , shelfY_(o.shelfY_)
    , shelfHeight_(o.shelfHeight_)
    , glyphCache_(std::move(o.glyphCache_))
    , isDirty_(o.isDirty_)
    , dirtyMinX_(o.dirtyMinX_)
    , dirtyMinY_(o.dirtyMinY_)
    , dirtyMaxX_(o.dirtyMaxX_)
    , dirtyMaxY_(o.dirtyMaxY_)
    , currentFrame_(o.currentFrame_)
    , cacheHits_(o.cacheHits_)
    , cacheMisses_(o.cacheMisses_)
{
    o.width_ = 0;
    o.height_ = 0;
    o.shelfX_ = 1;
    o.shelfY_ = 1;
    o.shelfHeight_ = 0;
    o.isDirty_ = false;
}

GlyphAtlas& GlyphAtlas::operator=(GlyphAtlas&& o) noexcept {
    if (this != &o) {
        width_ = o.width_;
        height_ = o.height_;
        buffer_ = std::move(o.buffer_);
        shelfX_ = o.shelfX_;
        shelfY_ = o.shelfY_;
        shelfHeight_ = o.shelfHeight_;
        glyphCache_ = std::move(o.glyphCache_);
        isDirty_ = o.isDirty_;
        dirtyMinX_ = o.dirtyMinX_;
        dirtyMinY_ = o.dirtyMinY_;
        dirtyMaxX_ = o.dirtyMaxX_;
        dirtyMaxY_ = o.dirtyMaxY_;
        currentFrame_ = o.currentFrame_;
        cacheHits_ = o.cacheHits_;
        cacheMisses_ = o.cacheMisses_;

        o.width_ = 0;
        o.height_ = 0;
        o.shelfX_ = 1;
        o.shelfY_ = 1;
        o.shelfHeight_ = 0;
        o.isDirty_ = false;
    }
    return *this;
}

void GlyphAtlas::getDirtyBounds(int& outX, int& outY, int& outW, int& outH) const noexcept {
    if (!isDirty_) {
        outX = outY = outW = outH = 0;
        return;
    }
    outX = dirtyMinX_;
    outY = dirtyMinY_;
    outW = dirtyMaxX_ - dirtyMinX_;
    outH = dirtyMaxY_ - dirtyMinY_;
}

void GlyphAtlas::clearDirty() noexcept {
    isDirty_ = false;
    dirtyMinX_ = 0;
    dirtyMinY_ = 0;
    dirtyMaxX_ = 0;
    dirtyMaxY_ = 0;
}

void GlyphAtlas::clear() {
    shelfX_ = 1;
    shelfY_ = 1;
    shelfHeight_ = 0;
    std::fill(buffer_.begin(), buffer_.end(), static_cast<uint8_t>(0));
    glyphCache_.clear();
    markDirty(0, 0, width_, height_);
}

void GlyphAtlas::markDirty(int x, int y, int w, int h) noexcept {
    if (w <= 0 || h <= 0) return;
    ++version_;
    int x1 = std::clamp(x, 0, width_);
    int y1 = std::clamp(y, 0, height_);
    int x2 = std::clamp(x + w, 0, width_);
    int y2 = std::clamp(y + h, 0, height_);

    if (!isDirty_) {
        dirtyMinX_ = x1;
        dirtyMinY_ = y1;
        dirtyMaxX_ = x2;
        dirtyMaxY_ = y2;
        isDirty_ = true;
    } else {
        dirtyMinX_ = std::min(dirtyMinX_, x1);
        dirtyMinY_ = std::min(dirtyMinY_, y1);
        dirtyMaxX_ = std::max(dirtyMaxX_, x2);
        dirtyMaxY_ = std::max(dirtyMaxY_, y2);
    }
}

bool GlyphAtlas::allocateSlot(int glyphW, int glyphH, int& outX, int& outY) {
    const int allocW = glyphW + 2; // 1px border padding
    const int allocH = glyphH + 2;

    if (allocW > width_ || allocH > height_) {
        return false;
    }

    if (shelfX_ + allocW > width_ - 1) {
        // Move to next shelf
        shelfX_ = 1;
        shelfY_ += shelfHeight_ + 1;
        shelfHeight_ = 0;
    }

    if (shelfY_ + allocH > height_ - 1) {
        // Atlas full - clear and start over
        clear();
        shelfX_ = 1;
        shelfY_ = 1;
        shelfHeight_ = 0;
    }

    outX = shelfX_ + 1; // offset by 1 for padding
    outY = shelfY_ + 1;
    shelfX_ += allocW;
    if (allocH > shelfHeight_) {
        shelfHeight_ = allocH;
    }

    return true;
}

const AtlasGlyph* GlyphAtlas::getOrLoadGlyph(FontManager& fontMgr, FontId fontId, uint32_t glyphIndex,
                                             float fontSize, float /*subpixelX*/) {
    if (fontId == kInvalidFontId || fontSize <= 0.0f) {
        return nullptr;
    }

    const uint16_t pSize = static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(fontSize)), 1, 512));
    const GlyphKey key{fontId, glyphIndex, pSize};

    auto it = glyphCache_.find(key);
    if (it != glyphCache_.end()) {
        ++cacheHits_;
        it->second.lastUsedFrame = currentFrame_;
        return &it->second;
    }

    ++cacheMisses_;

    FT_Face face = fontMgr.getFtFace(fontId);
    if (!face) {
        return nullptr;
    }

    FT_Error err = FT_Set_Pixel_Sizes(face, 0, pSize);
    if (err != 0) {
        return nullptr;
    }

    // Ensure face transform is clean identity
    FT_Set_Transform(face, nullptr, nullptr);

    // Render with light hinting to match HarfBuzz fractional positioning
    err = FT_Load_Glyph(face, glyphIndex, FT_LOAD_TARGET_LIGHT | FT_LOAD_RENDER);
    if (err != 0) {
        return nullptr;
    }

    const int bmpW = face->glyph->bitmap.width;
    const int bmpH = face->glyph->bitmap.rows;
    const int bmpPitch = face->glyph->bitmap.pitch;
    const unsigned char* bmpBuf = face->glyph->bitmap.buffer;

    AtlasGlyph ag;
    ag.bearingX = static_cast<float>(face->glyph->bitmap_left);
    ag.bearingY = static_cast<float>(face->glyph->bitmap_top);
    ag.advanceX = static_cast<float>(face->glyph->advance.x) / 64.0f;
    ag.lastUsedFrame = currentFrame_;

    if (bmpW <= 0 || bmpH <= 0 || !bmpBuf) {
        ag.atlasX = 0;
        ag.atlasY = 0;
        ag.width = 0;
        ag.height = 0;
        ag.u0 = ag.v0 = ag.u1 = ag.v1 = 0.0f;
        auto insertRes = glyphCache_.emplace(key, ag);
        return &insertRes.first->second;
    }

    int slotX = 0;
    int slotY = 0;
    if (!allocateSlot(bmpW, bmpH, slotX, slotY)) {
        return nullptr;
    }

    ag.atlasX = slotX;
    ag.atlasY = slotY;
    ag.width = bmpW;
    ag.height = bmpH;

    const float invW = 1.0f / static_cast<float>(width_);
    const float invH = 1.0f / static_cast<float>(height_);
    ag.u0 = static_cast<float>(slotX) * invW;
    ag.v0 = static_cast<float>(slotY) * invH;
    ag.u1 = static_cast<float>(slotX + bmpW) * invW;
    ag.v1 = static_cast<float>(slotY + bmpH) * invH;

    // Copy rendered bitmap into atlas buffer
    for (int r = 0; r < bmpH; ++r) {
        uint8_t* dst = &buffer_[static_cast<size_t>(slotY + r) * static_cast<size_t>(width_) + static_cast<size_t>(slotX)];
        const unsigned char* src = &bmpBuf[r * bmpPitch];
        std::memcpy(dst, src, bmpW);
    }

    markDirty(slotX, slotY, bmpW, bmpH);

    auto insertRes = glyphCache_.emplace(key, ag);
    return &insertRes.first->second;
}

} // namespace eatsbits::text
