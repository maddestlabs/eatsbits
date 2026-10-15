#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

namespace eatsbits::text {

using FontId = uint32_t;
constexpr FontId kInvalidFontId = 0;

struct FontFaceInfo {
    FontId id{kInvalidFontId};
    std::string name;
    FT_Face ftFace{nullptr};
    hb_font_t* hbFont{nullptr};
    std::vector<uint8_t> memoryBuffer; // Keeps memory alive if copied
    FontId fallbackFontId{kInvalidFontId};
};

class FontManager {
public:
    FontManager();
    ~FontManager();

    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) noexcept;
    FontManager& operator=(FontManager&&) noexcept;

    bool initialize();
    void shutdown();
    bool isInitialized() const noexcept { return ftLibrary_ != nullptr; }

    FT_Library getFtLibrary() const noexcept { return ftLibrary_; }

    FontId loadFontFromFile(const std::string& name, const std::string& filePath);
    FontId loadFontFromMemory(const std::string& name, const uint8_t* data, size_t size, bool copyData = false);
    FontId loadEmbeddedFallback(const std::string& name = "embedded_mono");

    FontId getFontId(std::string_view name) const;
    const FontFaceInfo* getFontInfo(FontId id) const;

    FT_Face getFtFace(FontId id) const;
    hb_font_t* getHbFont(FontId id) const;

    bool setFallbackFont(FontId primaryId, FontId fallbackId);
    FontId getFallbackFont(FontId primaryId) const;

    size_t getLoadedFontCount() const noexcept;

private:
    FT_Library ftLibrary_{nullptr};
    mutable std::mutex mutex_;
    FontId nextFontId_{1};
    std::unordered_map<std::string, FontId> nameToIdMap_;
    std::unordered_map<FontId, std::unique_ptr<FontFaceInfo>> fonts_;
};

} // namespace eatsbits::text
