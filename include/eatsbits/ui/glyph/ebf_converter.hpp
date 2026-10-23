#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "eatsbits/ui/glyph/ebf_font.hpp"

namespace eatsbits::ui::glyph {

/// @brief Configuration options for converting TrueType/OpenType fonts to .ebf.
struct EbfConversionOptions {
    uint32_t firstCodepoint{32};     ///< Starting codepoint (ASCII space = 32)
    uint32_t lastCodepoint{126};     ///< Ending codepoint (ASCII tilde = 126)
    uint16_t targetUnitsPerEm{1000}; ///< Target coordinate grid (default 1000)
    bool includeExtraLatin{true};    ///< Include useful Latin-1 symbols (0x00A0..0x00FF, e.g. °, ±, ©, µ)
    bool includeKerning{true};       ///< Extract horizontal kerning pairs
};

/**
 * @brief High-performance TrueType/OpenType to Eatsbits Font (.ebf) converter.
 * 
 * Uses FreeType to decompose TrueType (.ttf) and OpenType (.otf) vector contours
 * (linear, quadratic conic, and cubic Bézier splines) and typographic metrics
 * directly into compact .ebf bytecode streams.
 * 
 * Can be used:
 * 1. Offline at build-time to produce pre-compiled .ebf files.
 * 2. Directly in application startup to convert bundled TTF fonts on-the-fly.
 */
class EbfConverter {
public:
    /// Converts a TTF/OTF file on disk to an .ebf binary file.
    static bool convertFile(const std::string& ttfPath,
                            const std::string& ebfPath,
                            const EbfConversionOptions& options = {});

    /// Converts a TTF/OTF memory buffer into an .ebf binary byte vector.
    static std::vector<uint8_t> convertMemory(const uint8_t* ttfData,
                                              size_t ttfSize,
                                              const EbfConversionOptions& options = {});

    /// Directly converts a TTF/OTF file and loads it into an EbfFont instance at runtime.
    static bool loadTtfIntoFont(const std::string& ttfPath,
                                EbfFont& outFont,
                                const EbfConversionOptions& options = {});

    /// Directly converts a TTF/OTF memory buffer into an EbfFont instance at runtime.
    static bool loadTtfMemoryIntoFont(const uint8_t* ttfData,
                                      size_t ttfSize,
                                      EbfFont& outFont,
                                      const EbfConversionOptions& options = {});

    /// Exports a converted font to a C++ header containing a static constexpr uint8_t array.
    static bool convertToCppHeader(const std::string& ttfPath,
                                   const std::string& headerPath,
                                   const std::string& fontVarName,
                                   const EbfConversionOptions& options = {});
};

} // namespace eatsbits::ui::glyph
