#ifndef EATS_DISPATCH_SCANNER_HPP
#define EATS_DISPATCH_SCANNER_HPP

#include <string>
#include <map>
#include <vector>

namespace eatsbits::eatscript {

enum class NativeDispatchTarget {
    None,
    TB303,
    PolySynth,
    SID,
    DX7,
    SNES,
    YM2612,
    DrumKit808,
    DrumKit909,
    GrandPiano,
    UprightBass,
    SpanishGuitar,
    SteelGuitar,
    Convolver,
    StereoDelay
};

struct ExtractedParam {
    std::string name;
    float minVal{0.0f};
    float maxVal{1.0f};
    float defaultVal{0.5f};
};

/**
 * High-performance, zero-allocation scanner for Eatscript top-level native dispatch flags
 * and parameter declarations in def init().
 */
class NativeDispatchScanner {
public:
    /// Scans script source for top-level acceleration flags (e.g. Eats303 = True).
    [[nodiscard]] static NativeDispatchTarget detectTarget(const std::string& scriptSource) noexcept;

    /// Converts dispatch target enum to display name (e.g. "Roland TB-303 (SIMD)").
    [[nodiscard]] static const char* targetToString(NativeDispatchTarget target) noexcept;

    /// Extracts parameter declarations defined in def init():
    [[nodiscard]] static std::map<std::string, ExtractedParam> extractParameters(const std::string& scriptSource);

    /// Extracts key-value parameter pairs defined in def init():
    [[nodiscard]] static std::map<std::string, float> extractParamsFromInit(const std::string& scriptSource);
};

} // namespace eatsbits::eatscript

#endif // EATS_DISPATCH_SCANNER_HPP
