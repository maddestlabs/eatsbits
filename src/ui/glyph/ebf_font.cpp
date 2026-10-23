#include "eatsbits/ui/glyph/ebf_font.hpp"
#include "eatsbits/ui/glyph/ebf_converter.hpp"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace eatsbits::ui::glyph {

namespace {

// Helper to read unaligned little-endian integers from a bytecode buffer
inline int16_t readInt16(const uint8_t* ptr) noexcept {
    return static_cast<int16_t>(ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8));
}

inline uint16_t readUint16(const uint8_t* ptr) noexcept {
    return static_cast<uint16_t>(ptr[0] | (static_cast<uint16_t>(ptr[1]) << 8));
}

inline uint32_t readUint32(const uint8_t* ptr) noexcept {
    return static_cast<uint32_t>(ptr[0]) |
           (static_cast<uint32_t>(ptr[1]) << 8) |
           (static_cast<uint32_t>(ptr[2]) << 16) |
           (static_cast<uint32_t>(ptr[3]) << 24);
}

// UTF-8 decoder helper
inline uint32_t decodeUtf8(const char*& ptr, const char* end) noexcept {
    if (ptr >= end) return 0;
    uint8_t c0 = static_cast<uint8_t>(*ptr++);
    if (c0 < 0x80) return c0;
    if ((c0 & 0xE0) == 0xC0) {
        if (ptr >= end) return 0;
        uint8_t c1 = static_cast<uint8_t>(*ptr++);
        return ((c0 & 0x1F) << 6) | (c1 & 0x3F);
    }
    if ((c0 & 0xF0) == 0xE0) {
        if (ptr + 1 >= end) return 0;
        uint8_t c1 = static_cast<uint8_t>(*ptr++);
        uint8_t c2 = static_cast<uint8_t>(*ptr++);
        return ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
    }
    if ((c0 & 0xF8) == 0xF0) {
        if (ptr + 2 >= end) return 0;
        uint8_t c1 = static_cast<uint8_t>(*ptr++);
        uint8_t c2 = static_cast<uint8_t>(*ptr++);
        uint8_t c3 = static_cast<uint8_t>(*ptr++);
        return ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
    }
    return 0;
}

} // anonymous namespace

EbfFont::EbfFont() = default;

EbfFont::EbfFont(EbfFont&&) noexcept = default;
EbfFont& EbfFont::operator=(EbfFont&&) noexcept = default;

bool EbfFont::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size < static_cast<std::streamsize>(sizeof(EbfHeader))) return false;

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return false;

    return loadFromMemory(buffer.data(), buffer.size());
}

bool EbfFont::loadFromMemory(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(EbfHeader)) return false;
    parseFromBuffer(data, size);
    return valid_;
}

bool EbfFont::loadFromTtfFile(const std::string& ttfPath) {
    return EbfConverter::loadTtfIntoFont(ttfPath, *this);
}

bool EbfFont::loadFromTtfMemory(const uint8_t* data, size_t size) {
    return EbfConverter::loadTtfMemoryIntoFont(data, size, *this);
}

void EbfFont::parseFromBuffer(const uint8_t* data, size_t size) {
    valid_ = false;
    glyphs_.clear();
    commandData_.clear();
    kerningPairs_.clear();

    if (size < sizeof(EbfHeader)) return;
    std::memcpy(&header_, data, sizeof(EbfHeader));

    if (header_.magic[0] != 'E' || header_.magic[1] != 'B' ||
        header_.magic[2] != 'F' || header_.magic[3] != '1') {
        return;
    }

    if (header_.unitsPerEm == 0) header_.unitsPerEm = 1000;

    size_t offset = sizeof(EbfHeader);
    size_t glyphTableSize = static_cast<size_t>(header_.glyphCount) * sizeof(EbfGlyphRecord);
    if (offset + glyphTableSize > size) return;

    glyphs_.resize(header_.glyphCount);
    std::memcpy(glyphs_.data(), data + offset, glyphTableSize);
    offset += glyphTableSize;

    // Remaining bytes are bytecode and optional kerning table
    if (offset < size) {
        size_t cmdSize = size - offset;
        commandData_.resize(cmdSize);
        std::memcpy(commandData_.data(), data + offset, cmdSize);
    }

    // Sort glyphs by codepoint for fast binary searching
    std::sort(glyphs_.begin(), glyphs_.end(), [](const EbfGlyphRecord& a, const EbfGlyphRecord& b) {
        return a.codepoint < b.codepoint;
    });

    valid_ = true;
}

bool EbfFont::saveToFile(const std::string& filePath) const {
    if (!valid_) return false;
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(&header_), sizeof(EbfHeader));
    if (!glyphs_.empty()) {
        file.write(reinterpret_cast<const char*>(glyphs_.data()), glyphs_.size() * sizeof(EbfGlyphRecord));
    }
    if (!commandData_.empty()) {
        file.write(reinterpret_cast<const char*>(commandData_.data()), commandData_.size());
    }
    return file.good();
}

float EbfFont::getAscender(float fontSize) const noexcept {
    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    return (static_cast<float>(header_.ascender) / u) * fontSize;
}

float EbfFont::getDescender(float fontSize) const noexcept {
    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    return (static_cast<float>(header_.descender) / u) * fontSize;
}

float EbfFont::getLineHeight(float fontSize) const noexcept {
    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    float h = static_cast<float>(header_.ascender - header_.descender + header_.lineGap);
    return (h / u) * fontSize;
}

const EbfGlyphRecord* EbfFont::findGlyph(uint32_t codepoint) const noexcept {
    if (glyphs_.empty()) return nullptr;
    auto it = std::lower_bound(glyphs_.begin(), glyphs_.end(), codepoint,
                               [](const EbfGlyphRecord& rec, uint32_t cp) {
                                   return rec.codepoint < cp;
                               });
    if (it != glyphs_.end() && it->codepoint == codepoint) {
        return &(*it);
    }
    return nullptr;
}

float EbfFont::getKerning(uint32_t leftCp, uint32_t rightCp, float fontSize) const noexcept {
    if (kerningPairs_.empty()) return 0.0f;
    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    for (const auto& kp : kerningPairs_) {
        if (kp.leftCodepoint == leftCp && kp.rightCodepoint == rightCp) {
            return (static_cast<float>(kp.kerningAmount) / u) * fontSize;
        }
    }
    return 0.0f;
}

float EbfFont::measureText(std::string_view text, float fontSize) const {
    if (text.empty() || !valid_) return 0.0f;

    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    float scale = fontSize / u;

    float maxW = 0.0f;
    float curW = 0.0f;
    const char* ptr = text.data();
    const char* end = ptr + text.size();
    uint32_t prevCp = 0;

    while (ptr < end) {
        uint32_t cp = decodeUtf8(ptr, end);
        if (cp == '\n') {
            if (curW > maxW) maxW = curW;
            curW = 0.0f;
            prevCp = 0;
            continue;
        }
        if (cp == '\r') continue;
        if (cp == '\t') {
            float tabW = 4.0f * (static_cast<float>(header_.maxAdvance) * 0.6f * scale);
            curW = (std::floor(curW / tabW) + 1.0f) * tabW;
            prevCp = 0;
            continue;
        }

        const auto* g = findGlyph(cp);
        if (!g) g = findGlyph('?');

        float adv = g ? static_cast<float>(g->advanceX) * scale : (static_cast<float>(header_.maxAdvance) * scale * 0.5f);
        if (prevCp != 0) {
            adv += getKerning(prevCp, cp, fontSize);
        }
        curW += adv;
        prevCp = cp;
    }

    return std::max(maxW, curW);
}

EbfTextMetrics EbfFont::measureTextDetailed(std::string_view text, float fontSize) const {
    EbfTextMetrics m{};
    if (text.empty() || !valid_) return m;

    m.ascender = getAscender(fontSize);
    m.descender = getDescender(fontSize);
    m.width = measureText(text, fontSize);

    // Compute line count
    size_t lines = 1;
    for (char c : text) {
        if (c == '\n') lines++;
    }
    m.height = static_cast<float>(lines) * getLineHeight(fontSize);
    return m;
}

bool EbfFont::renderGlyph(BatchRenderer2D& r, uint32_t codepoint, float x, float baselineY,
                          float fontSize, float red, float green, float blue, float alpha) const {
    const auto* g = findGlyph(codepoint);
    if (!g) {
        g = findGlyph('?');
        if (!g) return false;
    }

    if (g->commandLength == 0 || g->commandOffset >= commandData_.size()) {
        return true; // Space or non-rendering glyph
    }

    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    float scale = fontSize / u;

    // Optical stroke thickness: calibrated for outline contours vs. skeleton strokes
    bool isOutlineFont = (header_.flags & 2) != 0 || (g->flags & 2) != 0;
    float stemW = isOutlineFont ? std::max(1.0f, std::min(1.5f, fontSize * 0.075f))
                                : std::max(1.15f, std::round(fontSize * 0.115f));
    float barH  = std::max(1.05f, std::round(fontSize * 0.088f));

    const uint8_t* p = commandData_.data() + g->commandOffset;
    const uint8_t* pEnd = p + g->commandLength;

    // Fast check: if glyph contains solid triangle geometry, do not stroke perimeter boundaries
    // to prevent artificial bolding, choked counters, and blurred small text.
    bool hasTriangles = false;
    for (const uint8_t* scan = p; scan < pEnd; ++scan) {
        if (*scan == static_cast<uint8_t>(EbfOpcode::Triangle)) {
            hasTriangles = true;
            break;
        }
    }

    float lastX = x;
    float lastY = baselineY;

    while (p < pEnd) {
        auto op = static_cast<EbfOpcode>(*p++);
        switch (op) {
            case EbfOpcode::End:
                return true;

            case EbfOpcode::MoveTo: {
                if (p + 4 > pEnd) return false;
                int16_t fx = readInt16(p); p += 2;
                int16_t fy = readInt16(p); p += 2;
                lastX = x + static_cast<float>(fx) * scale;
                lastY = baselineY - static_cast<float>(fy) * scale;
                break;
            }

            case EbfOpcode::LineTo: {
                if (p + 4 > pEnd) return false;
                int16_t fx = readInt16(p); p += 2;
                int16_t fy = readInt16(p); p += 2;
                float nx = x + static_cast<float>(fx) * scale;
                float ny = baselineY - static_cast<float>(fy) * scale;
                if (!hasTriangles) {
                    r.drawLine(lastX, lastY, nx, ny, red, green, blue, alpha, stemW);
                }
                lastX = nx;
                lastY = ny;
                break;
            }

            case EbfOpcode::QuadTo: {
                if (p + 8 > pEnd) return false;
                int16_t fcx = readInt16(p); p += 2;
                int16_t fcy = readInt16(p); p += 2;
                int16_t fx  = readInt16(p); p += 2;
                int16_t fy  = readInt16(p); p += 2;
                float cx = x + static_cast<float>(fcx) * scale;
                float cy = baselineY - static_cast<float>(fcy) * scale;
                float nx = x + static_cast<float>(fx) * scale;
                float ny = baselineY - static_cast<float>(fy) * scale;

                if (!hasTriangles) {
                    // 4-step quad curve subdivision
                    float px = lastX, py = lastY;
                    for (int step = 1; step <= 4; ++step) {
                        float t = static_cast<float>(step) * 0.25f;
                        float invT = 1.0f - t;
                        float qx = invT * invT * lastX + 2.0f * invT * t * cx + t * t * nx;
                        float qy = invT * invT * lastY + 2.0f * invT * t * cy + t * t * ny;
                        r.drawLine(px, py, qx, qy, red, green, blue, alpha, stemW);
                        px = qx;
                        py = qy;
                    }
                }
                lastX = nx;
                lastY = ny;
                break;
            }

            case EbfOpcode::CubicTo: {
                if (p + 12 > pEnd) return false;
                int16_t fc1x = readInt16(p); p += 2;
                int16_t fc1y = readInt16(p); p += 2;
                int16_t fc2x = readInt16(p); p += 2;
                int16_t fc2y = readInt16(p); p += 2;
                int16_t fx   = readInt16(p); p += 2;
                int16_t fy   = readInt16(p); p += 2;
                float c1x = x + static_cast<float>(fc1x) * scale;
                float c1y = baselineY - static_cast<float>(fc1y) * scale;
                float c2x = x + static_cast<float>(fc2x) * scale;
                float c2y = baselineY - static_cast<float>(fc2y) * scale;
                float nx  = x + static_cast<float>(fx) * scale;
                float ny  = baselineY - static_cast<float>(fy) * scale;

                if (!hasTriangles) {
                    float px = lastX, py = lastY;
                    for (int step = 1; step <= 6; ++step) {
                        float t = static_cast<float>(step) / 6.0f;
                        float invT = 1.0f - t;
                        float bx = invT * invT * invT * lastX +
                                   3.0f * invT * invT * t * c1x +
                                   3.0f * invT * t * t * c2x +
                                   t * t * t * nx;
                        float by = invT * invT * invT * lastY +
                                   3.0f * invT * invT * t * c1y +
                                   3.0f * invT * t * t * c2y +
                                   t * t * t * ny;
                        r.drawLine(px, py, bx, by, red, green, blue, alpha, stemW);
                        px = bx;
                        py = by;
                    }
                }
                lastX = nx;
                lastY = ny;
                break;
            }

            case EbfOpcode::Close:
                break;

            case EbfOpcode::Rect: {
                if (p + 8 > pEnd) return false;
                int16_t fx = readInt16(p); p += 2;
                int16_t fy = readInt16(p); p += 2;
                int16_t fw = readInt16(p); p += 2;
                int16_t fh = readInt16(p); p += 2;
                float rx = x + static_cast<float>(fx) * scale;
                float ry = baselineY - static_cast<float>(fy + fh) * scale;
                float rw = static_cast<float>(fw) * scale;
                float rh = static_cast<float>(fh) * scale;
                r.drawRect(rx, ry, rw, rh, red, green, blue, alpha);
                break;
            }

            case EbfOpcode::ChamferRect: {
                if (p + 10 > pEnd) return false;
                int16_t fx = readInt16(p); p += 2;
                int16_t fy = readInt16(p); p += 2;
                int16_t fw = readInt16(p); p += 2;
                int16_t fh = readInt16(p); p += 2;
                int16_t fc = readInt16(p); p += 2;

                float x0 = static_cast<float>(fx);
                float x1 = static_cast<float>(fx + fw);
                float y0 = static_cast<float>(fy);
                float y1 = static_cast<float>(fy + fh);
                float c  = static_cast<float>(fc);

                auto toScreenX = [&](float fxVal) { return x + fxVal * scale; };
                auto toScreenY = [&](float fyVal) { return baselineY - fyVal * scale; };

                // Horizontal bars
                float px0 = toScreenX(x0 + c);
                float px1 = toScreenX(x1 - c);
                r.drawRect(px0, toScreenY(y1) - barH * 0.5f, px1 - px0, barH, red, green, blue, alpha);
                r.drawRect(px0, toScreenY(y0) - barH * 0.5f, px1 - px0, barH, red, green, blue, alpha);

                // Vertical stems
                float py0 = toScreenY(y1 - c);
                float py1 = toScreenY(y0 + c);
                r.drawRect(toScreenX(x0) - stemW * 0.5f, py0, stemW, py1 - py0, red, green, blue, alpha);
                r.drawRect(toScreenX(x1) - stemW * 0.5f, py0, stemW, py1 - py0, red, green, blue, alpha);

                // Chamfer diagonals
                r.drawLine(toScreenX(x0), toScreenY(y1 - c), toScreenX(x0 + c), toScreenY(y1), red, green, blue, alpha, stemW);
                r.drawLine(toScreenX(x1 - c), toScreenY(y1), toScreenX(x1), toScreenY(y1 - c), red, green, blue, alpha, stemW);
                r.drawLine(toScreenX(x1), toScreenY(y0 + c), toScreenX(x1 - c), toScreenY(y0), red, green, blue, alpha, stemW);
                r.drawLine(toScreenX(x0 + c), toScreenY(y0), toScreenX(x0), toScreenY(y0 + c), red, green, blue, alpha, stemW);
                break;
            }

            case EbfOpcode::Triangle: {
                if (p + 12 > pEnd) return false;
                int16_t x0 = readInt16(p); p += 2;
                int16_t y0 = readInt16(p); p += 2;
                int16_t x1 = readInt16(p); p += 2;
                int16_t y1 = readInt16(p); p += 2;
                int16_t x2 = readInt16(p); p += 2;
                int16_t y2 = readInt16(p); p += 2;
                r.drawTriangle(x + static_cast<float>(x0) * scale, baselineY - static_cast<float>(y0) * scale,
                               x + static_cast<float>(x1) * scale, baselineY - static_cast<float>(y1) * scale,
                               x + static_cast<float>(x2) * scale, baselineY - static_cast<float>(y2) * scale,
                               red, green, blue, alpha);
                break;
            }

            case EbfOpcode::VStem: {
                if (p + 6 > pEnd) return false;
                int16_t fx  = readInt16(p); p += 2;
                int16_t fy0 = readInt16(p); p += 2;
                int16_t fy1 = readInt16(p); p += 2;
                float sx = x + static_cast<float>(fx) * scale;
                float sy0 = baselineY - static_cast<float>(fy1) * scale;
                float sy1 = baselineY - static_cast<float>(fy0) * scale;
                r.drawRect(sx - stemW * 0.5f, sy0, stemW, std::max(1.0f, sy1 - sy0), red, green, blue, alpha);
                break;
            }

            case EbfOpcode::HBar: {
                if (p + 6 > pEnd) return false;
                int16_t fx0 = readInt16(p); p += 2;
                int16_t fx1 = readInt16(p); p += 2;
                int16_t fy  = readInt16(p); p += 2;
                float sx0 = x + static_cast<float>(fx0) * scale;
                float sx1 = x + static_cast<float>(fx1) * scale;
                float sy = baselineY - static_cast<float>(fy) * scale;
                r.drawRect(sx0, sy - barH * 0.5f, std::max(1.0f, sx1 - sx0), barH, red, green, blue, alpha);
                break;
            }

            case EbfOpcode::Diag: {
                if (p + 8 > pEnd) return false;
                int16_t fx0 = readInt16(p); p += 2;
                int16_t fy0 = readInt16(p); p += 2;
                int16_t fx1 = readInt16(p); p += 2;
                int16_t fy1 = readInt16(p); p += 2;
                float sx0 = x + static_cast<float>(fx0) * scale;
                float sy0 = baselineY - static_cast<float>(fy0) * scale;
                float sx1 = x + static_cast<float>(fx1) * scale;
                float sy1 = baselineY - static_cast<float>(fy1) * scale;
                r.drawLine(sx0, sy0, sx1, sy1, red, green, blue, alpha, stemW);
                break;
            }
        }
    }

    return true;
}

void EbfFont::drawText(BatchRenderer2D& r, float x, float y, float fontSize,
                       std::string_view text,
                       float red, float green, float blue, float alpha) const {
    if (text.empty() || !valid_) return;

    float u = (header_.unitsPerEm > 0) ? static_cast<float>(header_.unitsPerEm) : 1000.0f;
    float scale = fontSize / u;
    float asc = getAscender(fontSize);
    float lineH = getLineHeight(fontSize);

    float curX = x;
    float baselineY = y + asc;

    const char* ptr = text.data();
    const char* end = ptr + text.size();
    uint32_t prevCp = 0;

    while (ptr < end) {
        uint32_t cp = decodeUtf8(ptr, end);
        if (cp == '\n') {
            curX = x;
            baselineY += lineH;
            prevCp = 0;
            continue;
        }
        if (cp == '\r') continue;
        if (cp == '\t') {
            float tabW = 4.0f * (static_cast<float>(header_.maxAdvance) * 0.6f * scale);
            curX = x + (std::floor((curX - x) / tabW) + 1.0f) * tabW;
            prevCp = 0;
            continue;
        }

        const auto* g = findGlyph(cp);
        if (!g) g = findGlyph('?');

        if (prevCp != 0) {
            curX += getKerning(prevCp, cp, fontSize);
        }

        renderGlyph(r, cp, curX, baselineY, fontSize, red, green, blue, alpha);

        float adv = g ? static_cast<float>(g->advanceX) * scale : (static_cast<float>(header_.maxAdvance) * scale * 0.5f);
        curX += adv;
        prevCp = cp;
    }
}

const EbfFont& EbfFont::getEmbeddedOswaldCondensed() {
    static const std::unique_ptr<EbfFont> s_oswald = []() {
        auto f = std::make_unique<EbfFont>();
        const std::vector<std::string> paths = {
            "assets/fonts/Oswald-Regular.ebf",
            "../assets/fonts/Oswald-Regular.ebf",
            "../../assets/fonts/Oswald-Regular.ebf",
            "c:/git/eatsbits/assets/fonts/Oswald-Regular.ebf"
        };
        for (const auto& p : paths) {
            if (f->loadFromFile(p)) break;
        }
        return f;
    }();
    return *s_oswald;
}

const EbfFont& EbfFont::getEmbeddedShareTechMono() {
    static const std::unique_ptr<EbfFont> s_shareTech = []() {
        auto f = std::make_unique<EbfFont>();
        const std::vector<std::string> paths = {
            "assets/fonts/ShareTechMono-Regular.ebf",
            "../assets/fonts/ShareTechMono-Regular.ebf",
            "../../assets/fonts/ShareTechMono-Regular.ebf",
            "c:/git/eatsbits/assets/fonts/ShareTechMono-Regular.ebf"
        };
        for (const auto& p : paths) {
            if (f->loadFromFile(p)) break;
        }
        return f;
    }();
    return *s_shareTech;
}

} // namespace eatsbits::ui::glyph
