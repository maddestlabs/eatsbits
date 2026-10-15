#include "eatsbits/text/font_manager.hpp"
#include "eatsbits/text/embedded_fonts.hpp"
#include <iostream>

namespace eatsbits::text {

FontManager::FontManager() {
    initialize();
}

FontManager::~FontManager() {
    shutdown();
}

FontManager::FontManager(FontManager&& o) noexcept {
    std::lock_guard<std::mutex> lock(o.mutex_);
    ftLibrary_ = o.ftLibrary_;
    o.ftLibrary_ = nullptr;
    nextFontId_ = o.nextFontId_;
    nameToIdMap_ = std::move(o.nameToIdMap_);
    fonts_ = std::move(o.fonts_);
}

FontManager& FontManager::operator=(FontManager&& o) noexcept {
    if (this != &o) {
        shutdown();
        std::scoped_lock lock(mutex_, o.mutex_);
        ftLibrary_ = o.ftLibrary_;
        o.ftLibrary_ = nullptr;
        nextFontId_ = o.nextFontId_;
        nameToIdMap_ = std::move(o.nameToIdMap_);
        fonts_ = std::move(o.fonts_);
    }
    return *this;
}

bool FontManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ftLibrary_) return true;

    FT_Error err = FT_Init_FreeType(&ftLibrary_);
    if (err) {
        std::cerr << "[FontManager] FT_Init_FreeType failed with error: " << err << std::endl;
        ftLibrary_ = nullptr;
        return false;
    }
    return true;
}

void FontManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, info] : fonts_) {
        if (info) {
            if (info->hbFont) {
                hb_font_destroy(info->hbFont);
                info->hbFont = nullptr;
            }
            if (info->ftFace) {
                FT_Done_Face(info->ftFace);
                info->ftFace = nullptr;
            }
        }
    }
    fonts_.clear();
    nameToIdMap_.clear();

    if (ftLibrary_) {
        FT_Done_FreeType(ftLibrary_);
        ftLibrary_ = nullptr;
    }
}

FontId FontManager::loadFontFromFile(const std::string& name, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ftLibrary_) return kInvalidFontId;

    auto it = nameToIdMap_.find(name);
    if (it != nameToIdMap_.end()) {
        return it->second;
    }

    FT_Face face = nullptr;
    FT_Error err = FT_New_Face(ftLibrary_, filePath.c_str(), 0, &face);
    if (err || !face) {
        std::cerr << "[FontManager] Failed to load font file: " << filePath << " (error: " << err << ")" << std::endl;
        return kInvalidFontId;
    }

    hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
    if (!hbFont) {
        FT_Done_Face(face);
        return kInvalidFontId;
    }

    FontId newId = nextFontId_++;
    auto info = std::make_unique<FontFaceInfo>();
    info->id = newId;
    info->name = name;
    info->ftFace = face;
    info->hbFont = hbFont;

    nameToIdMap_[name] = newId;
    fonts_[newId] = std::move(info);
    return newId;
}

FontId FontManager::loadFontFromMemory(const std::string& name, const uint8_t* data, size_t size, bool copyData) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ftLibrary_ || !data || size == 0) return kInvalidFontId;

    auto it = nameToIdMap_.find(name);
    if (it != nameToIdMap_.end()) {
        return it->second;
    }

    auto info = std::make_unique<FontFaceInfo>();
    const uint8_t* fontBytes = data;
    if (copyData) {
        info->memoryBuffer.assign(data, data + size);
        fontBytes = info->memoryBuffer.data();
    }

    FT_Face face = nullptr;
    FT_Error err = FT_New_Memory_Face(ftLibrary_, fontBytes, static_cast<FT_Long>(size), 0, &face);
    if (err || !face) {
        std::cerr << "[FontManager] Failed to load font from memory: " << name << " (error: " << err << ")" << std::endl;
        return kInvalidFontId;
    }

    hb_font_t* hbFont = hb_ft_font_create(face, nullptr);
    if (!hbFont) {
        FT_Done_Face(face);
        return kInvalidFontId;
    }

    FontId newId = nextFontId_++;
    info->id = newId;
    info->name = name;
    info->ftFace = face;
    info->hbFont = hbFont;

    nameToIdMap_[name] = newId;
    fonts_[newId] = std::move(info);
    return newId;
}

FontId FontManager::loadEmbeddedFallback(const std::string& name) {
    return loadFontFromMemory(name, kShareTechMonoTtf, kShareTechMonoTtfSize, false);
}

FontId FontManager::getFontId(std::string_view name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [n, id] : nameToIdMap_) {
        if (n == name) return id;
    }
    return kInvalidFontId;
}

const FontFaceInfo* FontManager::getFontInfo(FontId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fonts_.find(id);
    if (it != fonts_.end()) {
        return it->second.get();
    }
    return nullptr;
}

FT_Face FontManager::getFtFace(FontId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fonts_.find(id);
    if (it != fonts_.end() && it->second) {
        return it->second->ftFace;
    }
    return nullptr;
}

hb_font_t* FontManager::getHbFont(FontId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fonts_.find(id);
    if (it != fonts_.end() && it->second) {
        return it->second->hbFont;
    }
    return nullptr;
}

bool FontManager::setFallbackFont(FontId primaryId, FontId fallbackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fonts_.find(primaryId);
    if (it != fonts_.end() && it->second) {
        it->second->fallbackFontId = fallbackId;
        return true;
    }
    return false;
}

FontId FontManager::getFallbackFont(FontId primaryId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = fonts_.find(primaryId);
    if (it != fonts_.end() && it->second) {
        return it->second->fallbackFontId;
    }
    return kInvalidFontId;
}

size_t FontManager::getLoadedFontCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return fonts_.size();
}

} // namespace eatsbits::text
