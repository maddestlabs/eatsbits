#include "eatsbits/ui/gui_window.hpp"
#include "eatsbits/project/project_file.hpp"
#include "eatsbits/project/eats_serializer.hpp"
#include "eatsbits/audio/export/wav_exporter.hpp"
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"
#include "eatsbits/eatscript/vm.hpp"
#include "eatsbits/eatscript/transpiler.hpp"
#include "eatsbits/eatscript/note_script.hpp"
#include "eatsbits/eatscript/dispatch_scanner.hpp"
#include "eatsbits/eatscript/macro_runtime.hpp"
#include "eatsbits/procgen/procedural_song_engine.hpp"
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/ui/svg_path.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include "eatsbits/ui/app_icon_data.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#endif

namespace {

std::string promptOpenEatsFile() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Eatsbits Project (*.eats)\0*.eats\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

std::string promptOpenAudioFile() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Audio Files (*.wav;*.mp3;*.flac;*.ogg;*.aif)\0*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

std::string promptSaveEatsFile(const std::string& defaultName) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    std::strncpy(filename, defaultName.c_str(), MAX_PATH - 1);
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Eatsbits Project (*.eats)\0*.eats\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "eats";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

std::string promptSaveWavFile(const std::string& defaultName) {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
    char filename[MAX_PATH] = "";
    std::strncpy(filename, defaultName.c_str(), MAX_PATH - 1);
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "WAV Audio File (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "wav";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
#endif
    return "";
}

} // anonymous namespace

#if __has_include(<GLFW/glfw3.h>)
#define EATS_HAS_GLFW 1
#include <GLFW/glfw3.h>
#else
#define EATS_HAS_GLFW 0
#endif

using GLuint = unsigned int;

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
extern "C" {
#include "fontstash.h"
int fonsAddFallbackFont(FONScontext* stash, int base, int fallback);
void fonsResetFallbackFont(FONScontext* stash, int base);
}

namespace eatsbits::ui {

#if defined(__EMSCRIPTEN__)
static GuiWindow* g_activeGuiWindowForWeb = nullptr;

extern "C" EMSCRIPTEN_KEEPALIVE
void eats_on_file_dropped_web(const char* virtualPath, float clientX, float clientY) {
    if (!g_activeGuiWindowForWeb || !virtualPath) return;
    float logX = g_activeGuiWindowForWeb->windowToLogicalX(clientX);
    float logY = g_activeGuiWindowForWeb->windowToLogicalY(clientY);
    if (g_activeGuiWindowForWeb->is3dConsoleEnabled()) {
        g_activeGuiWindowForWeb->transform3dMouseCoords(logX, logY, logX, logY);
    }
    g_activeGuiWindowForWeb->remapCrtMouseCoords(logX, logY, logX, logY);
    g_activeGuiWindowForWeb->onFilesDropped({std::string(virtualPath)}, logX, logY);
}
#endif

static inline std::string toHex2(uint32_t val) {
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%02X", val & 0xFF);
    return std::string(buf);
}

static inline std::string midiNoteToTrackerString(uint8_t note) {
    static const char* noteNames[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
    };
    int semi = note % 12;
    int oct = (note / 12) - 1;
    if (oct < 0) oct = 0;
    if (oct > 9) oct = 9;
    return std::string(noteNames[semi]) + std::to_string(oct);
}

struct TokenSegment {
    std::string text;
    float r{1.0f}, g{1.0f}, b{1.0f};
};

static std::vector<TokenSegment> colorizeEatscriptLine(const std::string& line) {
    std::vector<TokenSegment> segments;
    size_t i = 0;
    while (i < line.size()) {
        if (line[i] == '#') {
            segments.push_back({line.substr(i), 0.38f, 0.49f, 0.55f});
            break;
        }
        if (line[i] == ' ' || line[i] == '\t') {
            size_t start = i;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            segments.push_back({line.substr(start, i - start), 1.0f, 1.0f, 1.0f});
            continue;
        }
        if (line[i] == '\'' || line[i] == '"') {
            char quote = line[i];
            size_t start = i++;
            while (i < line.size() && line[i] != quote) i++;
            if (i < line.size()) i++;
            segments.push_back({line.substr(start, i - start), 1.0f, 0.72f, 0.30f});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(line[i])) || (line[i] == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            size_t start = i;
            while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' || line[i] == 'e' || line[i] == 'E' || line[i] == 'f')) i++;
            segments.push_back({line.substr(start, i - start), 0.0f, 0.90f, 0.45f});
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
            size_t start = i;
            while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) i++;
            std::string ident = line.substr(start, i - start);

            if (ident == "def" || ident == "return" || ident == "if" || ident == "else" ||
                ident == "while" || ident == "for" || ident == "in" || ident == "import" ||
                ident == "pass" || ident == "True" || ident == "False") {
                segments.push_back({ident, 0.0f, 0.95f, 1.0f});
            } else if (ident == "sin" || ident == "cos" || ident == "tanh" || ident == "exp" ||
                       ident == "floor" || ident == "sqrt" || ident == "random" || ident == "pow" ||
                       ident == "abs" || ident == "biquad_lp" || ident == "biquad_hp" || ident == "biquad_bp" ||
                       ident == "init" || ident == "process") {
                segments.push_back({ident, 1.0f, 0.84f, 0.0f});
            } else if (ident == "time" || ident == "freq" || ident == "note" || ident == "in_l" ||
                       ident == "in_r" || ident == "out_l" || ident == "out_r" || ident == "sample_rate") {
                segments.push_back({ident, 0.85f, 0.55f, 1.0f});
            } else {
                segments.push_back({ident, 0.92f, 0.94f, 0.97f});
            }
            continue;
        }
        char c = line[i++];
        float opR = 0.55f, opG = 0.65f, opB = 0.72f;
        if (c == ':' || c == '(' || c == ')' || c == '[' || c == ']') {
            opR = 0.40f; opG = 0.85f; opB = 0.95f;
        } else if (c == '=' || c == '+' || c == '-' || c == '*' || c == '/') {
            opR = 1.0f; opG = 0.45f; opB = 0.65f;
        }
        segments.push_back({std::string(1, c), opR, opG, opB});
    }
    return segments;
}

struct FontRendererState {
    FONScontext* fs{nullptr};
    GLuint tex{1};
    int width{1024};
    int height{1024};
    int fontNormal{FONS_INVALID};
    int fontFallback{FONS_INVALID};
    int fontMono{FONS_INVALID};
    std::string loadedFontPath;
    std::string loadedMonoFontPath;

    ~FontRendererState() {
        if (fs) {
            fonsDeleteInternal(fs);
            fs = nullptr;
        }
        tex = 0;
    }
};

static FontRendererState* g_activeFontRenderer = nullptr;
static BatchRenderer2D* g_activeBatchRenderer = nullptr;

static int glfons__renderCreate(void* uptr, int width, int height) {
    auto* s = static_cast<FontRendererState*>(uptr);
    if (!s) return 0;
    s->width = width;
    s->height = height;
    s->tex = 1;
    return 1;
}

static int glfons__renderResize(void* uptr, int width, int height) {
    return glfons__renderCreate(uptr, width, height);
}

static void glfons__renderUpdate(void* uptr, int* rect, const unsigned char* data) {
    auto* s = static_cast<FontRendererState*>(uptr);
    if (!s) return;
    int w = rect[2] - rect[0];
    int h = rect[3] - rect[1];
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->updateFontAtlas(rect[0], rect[1], w, h, data, s->width, s->height);
    }
}

static void glfons__renderDraw(void* /*uptr*/, const float* verts, const float* tcoords, const unsigned int* colors, int nverts) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawTexturedTriangles(verts, tcoords, colors, nverts);
    }
}

static void glfons__renderDelete(void* uptr) {
    auto* s = static_cast<FontRendererState*>(uptr);
    if (s) s->tex = 0;
}

inline void parseHexColor(const std::string& hex, float& r, float& g, float& b) {
    if (hex.size() >= 7 && hex[0] == '#') {
        try {
            int red = std::stoi(hex.substr(1, 2), nullptr, 16);
            int grn = std::stoi(hex.substr(3, 2), nullptr, 16);
            int blu = std::stoi(hex.substr(5, 2), nullptr, 16);
            r = red / 255.0f;
            g = grn / 255.0f;
            b = blu / 255.0f;
            return;
        } catch (...) {}
    }
    r = 0.0f; g = 0.9f; b = 1.0f; // Default cyan
}

#if EATS_HAS_GLFW
void drawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawTriangle(x0, y0, x1, y1, x2, y2, r, g, b, a);
    }
}

void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRect(x, y, w, h, r, g, b, a);
    }
}

void drawRectGradient(float x, float y, float w, float h, float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRectGradient(x, y, w, h, r0, g0, b0, r1, g1, b1, a);
    }
}

void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRectOutline(x, y, w, h, r, g, b, a, lineWidth);
    }
}

void drawRoundedRect(float x, float y, float w, float h, float radius, float r, float g, float b, float a = 1.0f, int cornerSegments = 8) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRoundedRect(x, y, w, h, radius, r, g, b, a, cornerSegments);
    }
}

void drawRoundedRectGradient(float x, float y, float w, float h, float radius, float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f, int cornerSegments = 8) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRoundedRectGradient(x, y, w, h, radius, r0, g0, b0, r1, g1, b1, a, cornerSegments);
    }
}

void drawRoundedRectOutline(float x, float y, float w, float h, float radius, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f, int cornerSegments = 8) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRoundedRectOutline(x, y, w, h, radius, r, g, b, a, lineWidth, cornerSegments);
    }
}

void drawCircle(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircle(cx, cy, radius, r, g, b, a, segments);
    }
}

void drawCircleOutline(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, float lineWidth = 1.5f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleOutline(cx, cy, radius, r, g, b, a, lineWidth, segments);
    }
}

void drawArc(float cx, float cy, float radius, float startAngle, float endAngle, float r, float g, float b, float a = 1.0f, float lineWidth = 2.0f, int segments = 36) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawArc(cx, cy, radius, startAngle, endAngle, r, g, b, a, lineWidth, segments);
    }
}

void drawLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a = 1.0f, float lineWidth = 1.5f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawLine(x0, y0, x1, y1, r, g, b, a, lineWidth);
    }
}

void drawCatenaryBezier(Point2D p0, Point2D cp0, Point2D cp1, Point2D p1, float r, float g, float b, float a, float lineWidth) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCatenaryBezier(p0, cp0, cp1, p1, r, g, b, a, lineWidth);
    }
}

void drawCircleDropShadow(float cx, float cy, float radius, float elevation, const LightSource2D& light, float opacity = 0.45f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawCircleDropShadow(cx, cy, radius, elevation, light, opacity);
    }
}

void drawRectDropShadow(float x, float y, float w, float h, float cornerRadius, float elevation, const LightSource2D& light, float opacity = 0.40f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawRectDropShadow(x, y, w, h, cornerRadius, elevation, light, opacity);
    }
}

void drawKnurledRing(float cx, float cy, float rInner, float rOuter, int numTeeth, float rotationAngle,
                     const LightSource2D& light, float baseR, float baseG, float baseB,
                     float highlightR = 1.0f, float highlightG = 1.0f, float highlightB = 1.0f,
                     float shadowR = 0.08f, float shadowG = 0.09f, float shadowB = 0.11f) {
    if (g_activeBatchRenderer) {
        g_activeBatchRenderer->drawKnurledRing(cx, cy, rInner, rOuter, numTeeth, rotationAngle, light,
                                              baseR, baseG, baseB, highlightR, highlightG, highlightB,
                                              shadowR, shadowG, shadowB);
    }
}

void drawSvgLayers(const std::vector<SvgLayerDef>& layers, float x, float y, float w, float h, float opacity = 1.0f) {
    if (layers.empty() || w <= 0.0f || h <= 0.0f || !g_activeBatchRenderer) return;

    int texDim = std::clamp(static_cast<int>(std::max(w, h) * 2.0f), 32, 256);
    static std::unordered_map<std::string, std::vector<uint8_t>> s_rgbaCache;

    std::string key = std::to_string(texDim) + "_" + std::to_string(layers.size());
    for (const auto& l : layers) {
        key += "_" + std::string(l.path) + "_" + std::to_string(l.color) + "_" + std::to_string(l.isFill);
    }

    auto it = s_rgbaCache.find(key);
    if (it == s_rgbaCache.end()) {
        auto rgba = SvgTextureCache::rasterizeLayersToRgba(layers, texDim, texDim, 1.0f);
        it = s_rgbaCache.emplace(key, std::move(rgba)).first;
    }

    if (!it->second.empty()) {
        g_activeBatchRenderer->drawRgbaBitmap(x, y, w, h, it->second.data(), texDim, texDim, opacity);
    }
}

// =============================================================================
// PROCEDURAL GRUNGY CAST-METAL CHASSIS TEXTURE SYSTEM (THEME-TINTED)
// Generates a master monochrome micro-pitted cast-iron & chipped-rim surface
// map once at boot, then tints it to the active theme with zero per-frame cost.
// =============================================================================
namespace {

struct ChassisSurfaceSample {
    float stipple;      // 0.0 .. 1.0 (neutral 0.5)
    float fleck;        // 0.0 .. 1.0 (sparse brass flecks)
    float edgeErosion;  // 0.0 .. 1.0 (chipped rim wear)
    float edgeGrime;    // 0.0 .. 1.0 (crevice shadow)
};

class ChassisTextureSystem {
public:
    static constexpr int kWidth = 512;
    static constexpr int kHeight = 64;
    static constexpr int kNoiseDim = 64;

    static ChassisTextureSystem& instance() {
        static ChassisTextureSystem s_sys;
        return s_sys;
    }

    ChassisTextureSystem() {
        initMasterGrayscale();
    }

    void draw(BatchRenderer2D* r, float x, float y, float w, float h, bool isTopPanel, const ThemeTokens& theme) {
        if (!r || w <= 0.0f || h <= 0.0f) return;
        ensureTinted(theme);

        const auto& rgba = isTopPanel ? tintedTop_ : tintedBottom_;
        if (rgba.empty()) return;

        // Tile horizontally across [x, x + w]
        for (float curX = x; curX < x + w; curX += static_cast<float>(kWidth)) {
            float chunkW = std::min(static_cast<float>(kWidth), x + w - curX);
            r->drawRgbaBitmap(curX, y, chunkW, h, rgba.data(), kWidth, kHeight, 1.0f);
        }
    }

    void drawButtonNoise(BatchRenderer2D* r, float x, float y, float w, float h,
                         bool isTop = true, float opacity = 0.28f, float cornerRadius = 0.0f,
                         bool roundTL = true, bool roundTR = true, bool roundBL = true, bool roundBR = true,
                         bool pressed = false, float pressShiftY = 3.0f) {
        if (!r || w <= 0.0f || h <= 0.0f || opacity <= 0.001f) return;

        int iW = static_cast<int>(std::round(w));
        int iH = static_cast<int>(std::round(h));
        if (iW <= 0 || iH <= 0) return;

        static std::vector<uint8_t> s_buttonNoiseBuf;
        s_buttonNoiseBuf.resize(static_cast<size_t>(iW * iH * 4));

        const auto& master = isTop ? masterTop_ : masterBottom_;
        if (master.empty()) return;

        int sampleShiftY = pressed ? static_cast<int>(std::round(pressShiftY)) : 0;

        for (int py = 0; py < iH; ++py) {
            float fy = static_cast<float>(py) + 0.5f;
            for (int px = 0; px < iW; ++px) {
                float fx = static_cast<float>(px) + 0.5f;
                int idx = (py * iW + px) * 4;

                // Precision corner masking
                if (cornerRadius > 0.5f) {
                    bool inCorner = false;
                    float cornerCx = 0.0f, cornerCy = 0.0f;
                    if (roundTL && fx < cornerRadius && fy < cornerRadius) {
                        inCorner = true; cornerCx = cornerRadius; cornerCy = cornerRadius;
                    } else if (roundTR && fx > w - cornerRadius && fy < cornerRadius) {
                        inCorner = true; cornerCx = w - cornerRadius; cornerCy = cornerRadius;
                    } else if (roundBL && fx < cornerRadius && fy > h - cornerRadius) {
                        inCorner = true; cornerCx = cornerRadius; cornerCy = h - cornerRadius;
                    } else if (roundBR && fx > w - cornerRadius && fy > h - cornerRadius) {
                        inCorner = true; cornerCx = w - cornerRadius; cornerCy = h - cornerRadius;
                    }

                    if (inCorner) {
                        float dx = fx - cornerCx;
                        float dy = fy - cornerCy;
                        if (dx * dx + dy * dy > cornerRadius * cornerRadius) {
                            s_buttonNoiseBuf[idx + 0] = 0;
                            s_buttonNoiseBuf[idx + 1] = 0;
                            s_buttonNoiseBuf[idx + 2] = 0;
                            s_buttonNoiseBuf[idx + 3] = 0;
                            continue;
                        }
                    }
                }

                // Sample corresponding coordinate in the panel's procedural master map (with pressed down-stroke shift)
                int srcX = (static_cast<int>(x) + px) % kWidth;
                if (srcX < 0) srcX += kWidth;
                int srcY = (static_cast<int>(y) + py + sampleShiftY) % kHeight;
                if (srcY < 0) srcY += kHeight;

                const auto& s = master[srcY * kWidth + srcX];
                float diff = s.stipple - 0.50f;

                uint8_t rCol = 0, gCol = 0, bCol = 0, aCol = 0;
                if (s.fleck > 0.04f) {
                    // Warm metallic micro-fleck glint matching chassis plate (toned down)
                    rCol = 255;
                    gCol = 225;
                    bCol = 160;
                    aCol = static_cast<uint8_t>(std::clamp(s.fleck * 120.0f + 25.0f, 0.0f, 135.0f));
                } else if (diff < -0.022f) {
                    // Tactile micro-pit shadow (toned down, subtle powder-coat crevice)
                    rCol = 0; gCol = 0; bCol = 0;
                    float pit = (-diff - 0.022f) / 0.40f;
                    aCol = static_cast<uint8_t>(std::clamp(pit * 130.0f, 0.0f, 150.0f));
                } else if (diff > 0.022f) {
                    // Stipple powder-coat micro-highlight (toned down, gentle specular sheen)
                    rCol = 245; gCol = 245; bCol = 250;
                    float peak = (diff - 0.022f) / 0.40f;
                    aCol = static_cast<uint8_t>(std::clamp(peak * 110.0f, 0.0f, 130.0f));
                }

                // Top socket crevice shadow when physically pressed into faceplate well
                if (pressed && py < 4) {
                    float socketShadow = (4.0f - static_cast<float>(py)) / 4.0f * 100.0f;
                    if (socketShadow > static_cast<float>(aCol)) {
                        aCol = static_cast<uint8_t>(socketShadow);
                        rCol = 0; gCol = 0; bCol = 0;
                    }
                }

                s_buttonNoiseBuf[idx + 0] = rCol;
                s_buttonNoiseBuf[idx + 1] = gCol;
                s_buttonNoiseBuf[idx + 2] = bCol;
                s_buttonNoiseBuf[idx + 3] = aCol;
            }
        }

        r->drawRgbaBitmap(x, y, w, h, s_buttonNoiseBuf.data(), iW, iH, opacity);
    }


private:
    void initMasterGrayscale() {
        masterTop_.resize(kWidth * kHeight);
        masterBottom_.resize(kWidth * kHeight);
        buttonNoiseMaster_.resize(kNoiseDim * kNoiseDim);

        auto hash2D = [](int x, int y, uint32_t seed) -> float {
            uint32_t n = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 1442695040888963407u;
            n = (n ^ (n >> 13)) * 1274126177u;
            return static_cast<float>(n & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
        };

        // 1. Button Noise Master Tile (pure tactile stipple noise)
        for (int ny = 0; ny < kNoiseDim; ++ny) {
            for (int nx = 0; nx < kNoiseDim; ++nx) {
                float n1 = hash2D(nx, ny, 401) - 0.5f;
                float n2 = hash2D((nx / 2) % (kNoiseDim / 2), (ny / 2) % (kNoiseDim / 2), 503) - 0.5f;
                buttonNoiseMaster_[ny * kNoiseDim + nx] = 0.5f + (n1 * 0.65f + n2 * 0.35f) * 0.40f;
            }
        }

        // 2. Chassis Master Surface Maps (Top & Bottom panels)
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                float fx = static_cast<float>(x);
                float fy = static_cast<float>(y);

                // Multi-octave cast iron stipple and micro-pitting
                float h1 = hash2D(x, y, 101) - 0.5f;
                float h2 = hash2D((x / 2) % (kWidth / 2), y / 2, 203) - 0.5f;
                float h3 = hash2D((x / 6) % (kWidth / 6), y / 6, 307) - 0.5f;
                float xWarp = std::sin(fx * 0.045f) * 0.18f + std::cos(fx * 0.09f + fy * 0.12f) * 0.12f;
                float stipple = 0.5f + (h1 * 0.52f + h2 * 0.32f + h3 * 0.16f + xWarp * 0.14f) * 0.45f;

                // Subtle organic pixels of slightly lighter intensity (replacing harsh horizontal lines)
                float fleck = 0.0f;
                float randSubtle = hash2D(x, y, 888);
                if (randSubtle > 0.965f) {
                    fleck = (randSubtle - 0.965f) / 0.035f * 0.22f; // Soft intensity 0.0 to 0.22
                }

                // Sparse warm flecks - toned down to gentle metallic micro-highlights
                float randFleck = hash2D(x, y, 777);
                if (randFleck > 0.988f) {
                    float bright = (randFleck - 0.988f) / 0.012f * 0.38f; // Softened peak
                    fleck = std::max(fleck, bright);
                }

                // --- Top Panel Specifics ---
                // Uniform machined chassis crevice along bottom edge (y = kHeight - 1)
                ChassisSurfaceSample sampleTop;
                sampleTop.stipple = stipple;
                sampleTop.fleck = fleck;
                sampleTop.edgeErosion = 0.0f;
                sampleTop.edgeGrime = 0.0f;

                float distToBottom = static_cast<float>(kHeight - 1 - y);
                if (distToBottom < 3.5f) {
                    sampleTop.edgeGrime = std::clamp((3.5f - distToBottom) / 3.5f, 0.0f, 1.0f) * 0.50f;
                }
                masterTop_[y * kWidth + x] = sampleTop;

                // --- Bottom Panel Specifics ---
                // Uniform machined chassis crevice along top edge (y = 0)
                ChassisSurfaceSample sampleBot;
                sampleBot.stipple = stipple;
                sampleBot.fleck = fleck;
                sampleBot.edgeErosion = 0.0f;
                sampleBot.edgeGrime = 0.0f;

                float distToTop = fy;
                if (distToTop < 3.5f) {
                    sampleBot.edgeGrime = std::clamp((3.5f - distToTop) / 3.5f, 0.0f, 1.0f) * 0.50f;
                }
                masterBottom_[y * kWidth + x] = sampleBot;
            }
        }
    }

    void ensureTinted(const ThemeTokens& theme) {
        if (!tintedTop_.empty() && cachedThemeName_ == theme.name &&
            cachedGradTop_ == theme.panelHeaderGradientTop.toRgba8() &&
            cachedGradBot_ == theme.panelHeaderGradientBottom.toRgba8()) {
            return;
        }

        cachedThemeName_ = theme.name;
        cachedGradTop_ = theme.panelHeaderGradientTop.toRgba8();
        cachedGradBot_ = theme.panelHeaderGradientBottom.toRgba8();

        tintedTop_.resize(kWidth * kHeight * 4);
        tintedBottom_.resize(kWidth * kHeight * 4);
        buttonNoiseRgba_.resize(kNoiseDim * kNoiseDim * 4);

        // 1. Bake Button Tactile Noise Tile
        for (int i = 0; i < kNoiseDim * kNoiseDim; ++i) {
            float s = buttonNoiseMaster_[i];
            float delta = (s - 0.5f) * 0.35f;
            Color base = Color(0.18f, 0.18f, 0.19f);
            Color c = (delta >= 0.0f) ? base.lighten(delta) : base.darken(-delta * 1.2f);
            buttonNoiseRgba_[i * 4 + 0] = static_cast<uint8_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
            buttonNoiseRgba_[i * 4 + 1] = static_cast<uint8_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
            buttonNoiseRgba_[i * 4 + 2] = static_cast<uint8_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
            buttonNoiseRgba_[i * 4 + 3] = 255;
        }

        // 2. Bake Top and Bottom Panel Plates
        auto bakePanel = [&](const std::vector<ChassisSurfaceSample>& master, std::vector<uint8_t>& dst, bool /*isTop*/) {
            for (int y = 0; y < kHeight; ++y) {
                float t = static_cast<float>(y) / static_cast<float>(kHeight - 1);
                Color base = Color::lerp(theme.panelHeaderGradientTop, theme.panelHeaderGradientBottom, t);

                for (int x = 0; x < kWidth; ++x) {
                    const auto& s = master[y * kWidth + x];

                    // Modulate base color with stipple pitting
                    float stippleDelta = (s.stipple - 0.5f) * 0.20f;
                    Color c = (stippleDelta >= 0.0f)
                        ? base.lighten(stippleDelta)
                        : base.darken(-stippleDelta * 1.35f);

                    // Grime and dirt shadow in crevices
                    if (s.edgeGrime > 0.005f) {
                        Color grimeCol = theme.backgroundDark.darken(0.35f);
                        c = Color::lerp(c, grimeCol, s.edgeGrime * 0.85f);
                    }

                    // Warm brass/bronze flecks and surface micro-highlights (subtle & toned down)
                    if (s.fleck > 0.01f) {
                        Color brassFleck = Color::lerp(base.lighten(0.10f), theme.primaryAccent.lighten(0.10f), 0.22f);
                        c = Color::lerp(c, brassFleck, s.fleck * 0.40f);
                    }

                    // Exposed raw brass/steel chipped rim wear along edge
                    if (s.edgeErosion > 0.02f) {
                        Color wornMetal = Color::lerp(Color(0.88f, 0.74f, 0.46f), theme.borderSubtle.lighten(0.40f), 0.30f);
                        c = Color::lerp(c, wornMetal, s.edgeErosion * 0.95f);
                    }

                    int idx = (y * kWidth + x) * 4;
                    dst[idx + 0] = static_cast<uint8_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
                    dst[idx + 1] = static_cast<uint8_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
                    dst[idx + 2] = static_cast<uint8_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
                    dst[idx + 3] = 255;
                }
            }
        };

        bakePanel(masterTop_, tintedTop_, true);
        bakePanel(masterBottom_, tintedBottom_, false);
    }

    std::vector<ChassisSurfaceSample> masterTop_;
    std::vector<ChassisSurfaceSample> masterBottom_;
    std::vector<float> buttonNoiseMaster_;
    std::vector<uint8_t> tintedTop_;
    std::vector<uint8_t> tintedBottom_;
    std::vector<uint8_t> buttonNoiseRgba_;
    std::string cachedThemeName_;
    uint32_t cachedGradTop_{0};
    uint32_t cachedGradBot_{0};
};

inline void drawChassisPlate(float x, float y, float w, float h, bool isTopPanel, const ThemeTokens& theme) {
    if (g_activeBatchRenderer) {
        ChassisTextureSystem::instance().draw(g_activeBatchRenderer, x, y, w, h, isTopPanel, theme);
    }
}

inline void drawButtonNoise(float x, float y, float w, float h, bool isTop = true, float opacity = 0.28f,
                            float cornerRadius = 0.0f, bool roundTL = true, bool roundTR = true,
                            bool roundBL = true, bool roundBR = true, bool pressed = false, float pressShiftY = 3.0f) {
    if (g_activeBatchRenderer) {
        ChassisTextureSystem::instance().drawButtonNoise(g_activeBatchRenderer, x, y, w, h, isTop, opacity,
                                                         cornerRadius, roundTL, roundTR, roundBL, roundBR,
                                                         pressed, pressShiftY);
    }
}

inline void setBlendMode(BlendMode mode) {
    if (g_activeBatchRenderer) g_activeBatchRenderer->setBlendMode(mode);
}

struct ScopedBlendMode {
    BatchRenderer2D* r;
    BlendMode prev;
    ScopedBlendMode(BlendMode mode) : r(g_activeBatchRenderer), prev(BlendMode::Normal) {
        if (r) {
            prev = r->getBlendMode();
            r->setBlendMode(mode);
        }
    }
    ~ScopedBlendMode() {
        if (r) r->setBlendMode(prev);
    }
};

} // namespace

void drawEatsbitsLogo(float x, float y, float size, float opacity = 1.0f, bool inverted = false) {
    static const auto s_logoLayers = SvgLogo::getLayers();
    static const auto s_invertedLayers = SvgLogo::getInvertedLayers();
    drawSvgLayers(inverted ? s_invertedLayers : s_logoLayers, x, y, size, size, opacity);
}

inline uint32_t packSvgColor(const Color& c, float extraAlpha = 1.0f) {
    auto ur = static_cast<uint32_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
    auto ug = static_cast<uint32_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
    auto ub = static_cast<uint32_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
    auto ua = static_cast<uint32_t>(std::clamp(c.a * extraAlpha * 255.0f, 0.0f, 255.0f));
    return ur | (ug << 8) | (ub << 16) | (ua << 24);
}

void drawPlainEatsbitsLogo(float x, float y, float w, float h, const Color& color, float opacity = 1.0f,
                           bool inverted = false, const Color& darkColor = Color(0.08f, 0.07f, 0.06f)) {
    if (!inverted) {
        // Subtle drop shadow / stamped depth edge for realistic faceplate silkscreen
        uint32_t shadowCol = packSvgColor(Color(0.0f, 0.0f, 0.0f, 0.50f));
        std::vector<SvgLayerDef> shadowLayers = {
            SvgLayerDef(SvgLogo::kPlainLogoPath, shadowCol, 0.40f * opacity, true)
        };
        drawSvgLayers(shadowLayers, x + 0.65f, y + 0.85f, w, h, opacity);

        // Stamped silkscreen emblem with theme accent color
        uint32_t emblemCol = packSvgColor(color);
        std::vector<SvgLayerDef> emblemLayers = {
            SvgLayerDef(SvgLogo::kPlainLogoPath, emblemCol, opacity, true)
        };
        drawSvgLayers(emblemLayers, x, y, w, h, opacity);
    } else {
        // Inverted state on hover: dark creature body with illuminated accent outline, eye, and eating bits
        uint32_t accentCol = packSvgColor(color);
        uint32_t darkCol = packSvgColor(darkColor);
        auto invertedLayers = SvgLogo::getPlainInvertedLayers(accentCol, darkCol);
        drawSvgLayers(invertedLayers, x, y, w, h, opacity);
    }
}

// Clean vector stroke typography for labels and unicode music/box characters
void drawVectorGlyph(uint32_t c, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
    auto seg = [&](float x0, float y0, float x1, float y1) {
        drawLine(x0, y0, x1, y1, r, g, b, a, 1.5f * scale);
    };

    if (c < 128) {
        switch (std::toupper(static_cast<char>(c))) {
            case 'A':
                seg(x, y + 8 * scale, x + 2.5f * scale, y);
                seg(x + 2.5f * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 1.2f * scale, y + 4.5f * scale, x + 3.8f * scale, y + 4.5f * scale);
                break;
            case 'B':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4 * scale, y + 2 * scale);
                seg(x + 4 * scale, y + 2 * scale, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 4 * scale, y + 6 * scale);
                seg(x + 4 * scale, y + 6 * scale, x, y + 8 * scale);
                break;
            case 'C':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case 'D':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y + 4 * scale);
                seg(x + 4.5f * scale, y + 4 * scale, x, y + 8 * scale);
                break;
            case 'E':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x, y + 4 * scale, x + 3.5f * scale, y + 4 * scale);
                seg(x, y + 8 * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'F':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x, y + 4 * scale, x + 3.5f * scale, y + 4 * scale);
                break;
            case 'G':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x + 2.5f * scale, y + 4 * scale);
                break;
            case 'H':
                seg(x, y, x, y + 8 * scale);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case 'I':
                seg(x + 1 * scale, y, x + 4 * scale, y);
                seg(x + 2.5f * scale, y, x + 2.5f * scale, y + 8 * scale);
                seg(x + 1 * scale, y + 8 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case 'K':
                seg(x, y, x, y + 8 * scale);
                seg(x + 4.5f * scale, y, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'L':
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'M':
                seg(x, y + 8 * scale, x, y);
                seg(x, y, x + 2.5f * scale, y + 5 * scale);
                seg(x + 2.5f * scale, y + 5 * scale, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                break;
            case 'N':
                seg(x, y + 8 * scale, x, y);
                seg(x, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'O':
            case '0':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x, y);
                break;
            case 'P':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x + 4.5f * scale, y, x + 4.5f * scale, y + 4.5f * scale);
                seg(x + 4.5f * scale, y + 4.5f * scale, x, y + 4.5f * scale);
                break;
            case 'R':
                seg(x, y, x, y + 8 * scale);
                seg(x, y, x + 4.5f * scale, y);
                seg(x + 4.5f * scale, y, x + 4.5f * scale, y + 4.5f * scale);
                seg(x + 4.5f * scale, y + 4.5f * scale, x, y + 4.5f * scale);
                seg(x + 1.5f * scale, y + 4.5f * scale, x + 4.5f * scale, y + 8 * scale);
                break;
            case 'S':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                break;
            case 'T':
                seg(x, y, x + 5 * scale, y);
                seg(x + 2.5f * scale, y, x + 2.5f * scale, y + 8 * scale);
                break;
            case 'U':
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'V':
                seg(x, y, x + 2.5f * scale, y + 8 * scale);
                seg(x + 2.5f * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'W':
                seg(x, y, x + 1 * scale, y + 8 * scale);
                seg(x + 1 * scale, y + 8 * scale, x + 2.5f * scale, y + 3 * scale);
                seg(x + 2.5f * scale, y + 3 * scale, x + 4 * scale, y + 8 * scale);
                seg(x + 4 * scale, y + 8 * scale, x + 5 * scale, y);
                break;
            case 'Y':
                seg(x, y, x + 2.5f * scale, y + 4 * scale);
                seg(x + 5 * scale, y, x + 2.5f * scale, y + 4 * scale);
                seg(x + 2.5f * scale, y + 4 * scale, x + 2.5f * scale, y + 8 * scale);
                break;
            case '1':
                seg(x + 1.5f * scale, y + 2 * scale, x + 3 * scale, y);
                seg(x + 3 * scale, y, x + 3 * scale, y + 8 * scale);
                seg(x + 1 * scale, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case '2':
                seg(x, y + 2 * scale, x + 3 * scale, y);
                seg(x + 3 * scale, y, x + 5 * scale, y + 2 * scale);
                seg(x + 5 * scale, y + 2 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case '3':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 2.5f * scale, y + 4 * scale);
                seg(x + 2.5f * scale, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                break;
            case '4':
                seg(x, y, x, y + 4.5f * scale);
                seg(x, y + 4.5f * scale, x + 5 * scale, y + 4.5f * scale);
                seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8 * scale);
                break;
            case '5':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 3.5f * scale);
                seg(x, y + 3.5f * scale, x + 4.5f * scale, y + 3.5f * scale);
                seg(x + 4.5f * scale, y + 3.5f * scale, x + 4.5f * scale, y + 8 * scale);
                seg(x + 4.5f * scale, y + 8 * scale, x, y + 8 * scale);
                break;
            case '6':
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y + 4 * scale);
                seg(x + 5 * scale, y + 4 * scale, x, y + 4 * scale);
                break;
            case '7':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 1.5f * scale, y + 8 * scale);
                break;
            case '8':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x, y);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case '9':
                seg(x + 5 * scale, y + 8 * scale, x + 5 * scale, y);
                seg(x + 5 * scale, y, x, y);
                seg(x, y, x, y + 4 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case '%':
                seg(x + 0.8f * scale, y + 1.2f * scale, x + 2.0f * scale, y + 1.2f * scale);
                seg(x + 3.0f * scale, y + 6.8f * scale, x + 4.2f * scale, y + 6.8f * scale);
                seg(x + 4.5f * scale, y + 0.5f * scale, x + 0.5f * scale, y + 7.5f * scale);
                break;
            case '-':
                seg(x + 1 * scale, y + 4 * scale, x + 4 * scale, y + 4 * scale);
                break;
            case ':':
                seg(x + 2 * scale, y + 2.5f * scale, x + 3 * scale, y + 2.5f * scale);
                seg(x + 2 * scale, y + 5.5f * scale, x + 3 * scale, y + 5.5f * scale);
                break;
            case '.':
                seg(x + 2 * scale, y + 7.5f * scale, x + 3 * scale, y + 7.5f * scale);
                break;
            case 'Q':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y + 8 * scale, x, y + 8 * scale);
                seg(x, y + 8 * scale, x, y);
                seg(x + 3 * scale, y + 5 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case 'X':
                seg(x, y, x + 5 * scale, y + 8 * scale);
                seg(x + 5 * scale, y, x, y + 8 * scale);
                break;
            case 'Z':
                seg(x, y, x + 5 * scale, y);
                seg(x + 5 * scale, y, x, y + 8 * scale);
                seg(x, y + 8 * scale, x + 5 * scale, y + 8 * scale);
                break;
            case '>':
                seg(x + 1 * scale, y, x + 4.5f * scale, y + 4 * scale);
                seg(x + 4.5f * scale, y + 4 * scale, x + 1 * scale, y + 8 * scale);
                break;
            case '<':
                seg(x + 4 * scale, y, x + 0.5f * scale, y + 4 * scale);
                seg(x + 0.5f * scale, y + 4 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case '|':
                seg(x + 2.5f * scale, y, x + 2.5f * scale, y + 8 * scale);
                break;
            case '[':
                seg(x + 4 * scale, y, x + 1.5f * scale, y);
                seg(x + 1.5f * scale, y, x + 1.5f * scale, y + 8 * scale);
                seg(x + 1.5f * scale, y + 8 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case ']':
                seg(x + 1 * scale, y, x + 3.5f * scale, y);
                seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8 * scale);
                seg(x + 3.5f * scale, y + 8 * scale, x + 1 * scale, y + 8 * scale);
                break;
            case '(':
                seg(x + 4 * scale, y, x + 2 * scale, y + 4 * scale);
                seg(x + 2 * scale, y + 4 * scale, x + 4 * scale, y + 8 * scale);
                break;
            case ')':
                seg(x + 1 * scale, y, x + 3 * scale, y + 4 * scale);
                seg(x + 3 * scale, y + 4 * scale, x + 1 * scale, y + 8 * scale);
                break;
            case '#':
                seg(x + 1.5f * scale, y, x + 1.5f * scale, y + 8 * scale);
                seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8 * scale);
                seg(x, y + 2.5f * scale, x + 5 * scale, y + 2.5f * scale);
                seg(x, y + 5.5f * scale, x + 5 * scale, y + 5.5f * scale);
                break;
            case '=':
                seg(x + 0.5f * scale, y + 2.5f * scale, x + 4.5f * scale, y + 2.5f * scale);
                seg(x + 0.5f * scale, y + 5.5f * scale, x + 4.5f * scale, y + 5.5f * scale);
                break;
            case '+':
                seg(x + 2.5f * scale, y + 1 * scale, x + 2.5f * scale, y + 7 * scale);
                seg(x, y + 4 * scale, x + 5 * scale, y + 4 * scale);
                break;
            case '/':
                seg(x + 4.5f * scale, y, x + 0.5f * scale, y + 8 * scale);
                break;
            default:
                break;
        }
        return;
    }

    // Unicode Musical Notes & Box-Drawing Characters
    switch (c) {
        case 0x2669: // ♩ Quarter note
            drawCircle(x + 2.0f * scale, y + 6.5f * scale, 1.8f * scale, r, g, b, a, 10);
            seg(x + 3.5f * scale, y + 6.5f * scale, x + 3.5f * scale, y + 1.0f * scale);
            break;
        case 0x266A: // ♪ Eighth note
            drawCircle(x + 2.0f * scale, y + 6.5f * scale, 1.8f * scale, r, g, b, a, 10);
            seg(x + 3.5f * scale, y + 6.5f * scale, x + 3.5f * scale, y + 1.0f * scale);
            seg(x + 3.5f * scale, y + 1.0f * scale, x + 5.5f * scale, y + 3.0f * scale);
            break;
        case 0x266B: // ♫ Beamed eighth note pair
            drawCircle(x + 1.5f * scale, y + 6.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 2.7f * scale, y + 6.8f * scale, x + 2.7f * scale, y + 1.5f * scale);
            drawCircle(x + 5.0f * scale, y + 5.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 6.2f * scale, y + 5.8f * scale, x + 6.2f * scale, y + 0.5f * scale);
            seg(x + 2.7f * scale, y + 1.5f * scale, x + 6.2f * scale, y + 0.5f * scale);
            seg(x + 2.7f * scale, y + 2.5f * scale, x + 6.2f * scale, y + 1.5f * scale);
            break;
        case 0x266C: // ♬ Beamed sixteenth note pair
            drawCircle(x + 1.5f * scale, y + 6.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 2.7f * scale, y + 6.8f * scale, x + 2.7f * scale, y + 1.5f * scale);
            drawCircle(x + 5.0f * scale, y + 5.8f * scale, 1.5f * scale, r, g, b, a, 8);
            seg(x + 6.2f * scale, y + 5.8f * scale, x + 6.2f * scale, y + 0.5f * scale);
            seg(x + 2.7f * scale, y + 1.2f * scale, x + 6.2f * scale, y + 0.2f * scale);
            seg(x + 2.7f * scale, y + 2.8f * scale, x + 6.2f * scale, y + 1.8f * scale);
            break;
        case 0x25B6: // ▶ Play
            drawTriangle(x + 1.0f * scale, y + 1.0f * scale,
                         x + 6.0f * scale, y + 4.0f * scale,
                         x + 1.0f * scale, y + 7.0f * scale, r, g, b, a);
            break;
        case 0x25A0: // ■ Stop
            drawRect(x + 1.0f * scale, y + 1.5f * scale, 5.0f * scale, 5.0f * scale, r, g, b, a);
            break;
        case 0x25CF: // ● Record
            drawCircle(x + 3.5f * scale, y + 4.0f * scale, 3.0f * scale, r, g, b, a, 12);
            break;
        case 0x2500: // ─ Box horizontal
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            break;
        case 0x2502: // │ Box vertical
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x250C: // ┌ Box top-left
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x2510: // ┐ Box top-right
            seg(x, y + 4.0f * scale, x + 3.5f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x2514: // └ Box bottom-left
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            break;
        case 0x2518: // ┘ Box bottom-right
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 4.0f * scale);
            seg(x, y + 4.0f * scale, x + 3.5f * scale, y + 4.0f * scale);
            break;
        case 0x251C: // ├ Box tee-right
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            break;
        case 0x2524: // ┤ Box tee-left
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            seg(x, y + 4.0f * scale, x + 3.5f * scale, y + 4.0f * scale);
            break;
        case 0x252C: // ┬ Box tee-down
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y + 4.0f * scale, x + 3.5f * scale, y + 8.0f * scale);
            break;
        case 0x2534: // ┴ Box tee-up
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 4.0f * scale);
            break;
        case 0x253C: // ┼ Box cross
            seg(x, y + 4.0f * scale, x + 7.0f * scale, y + 4.0f * scale);
            seg(x + 3.5f * scale, y, x + 3.5f * scale, y + 8.0f * scale);
            break;
        default:
            break;
    }
}

inline uint32_t decodeNextUtf8(const char*& p, const char* end) {
    if (p >= end) return 0;
    unsigned char c = static_cast<unsigned char>(*p++);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && p < end) {
        uint32_t cp = (c & 0x1F) << 6;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && p + 1 < end) {
        uint32_t cp = (c & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && p + 2 < end) {
        uint32_t cp = (c & 0x07) << 18;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F) << 6;
        cp |= (static_cast<unsigned char>(*p++) & 0x3F);
        return cp;
    }
    return c;
}

inline bool isSpecialSymbol(uint32_t cp) {
    return (cp >= 0x2500 && cp <= 0x257F) || // Box drawing
           (cp >= 0x2660 && cp <= 0x266F) || // Music notes
           (cp >= 0x25A0 && cp <= 0x25FF);   // Geometric shapes
}

void drawVectorString(const std::string& str, float x, float y, float scale, float r, float g, float b, float a) {
    bool hasSpecial = false;
    for (unsigned char c : str) {
        if (c >= 0x80) {
            hasSpecial = true;
            break;
        }
    }

    if (!hasSpecial && g_activeFontRenderer && g_activeFontRenderer->fs && g_activeFontRenderer->fontNormal != FONS_INVALID) {
        FONScontext* fs = g_activeFontRenderer->fs;
        fonsClearState(fs);
        float fontSize = std::max(11.0f, scale * 15.5f);
        fonsSetSize(fs, fontSize);
        fonsSetFont(fs, g_activeFontRenderer->fontNormal);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        unsigned int col = static_cast<unsigned int>(cr) |
                          (static_cast<unsigned int>(cg) << 8) |
                          (static_cast<unsigned int>(cb) << 16) |
                          (static_cast<unsigned int>(ca) << 24);
        fonsSetColor(fs, col);
        fonsDrawText(fs, x, y, str.c_str(), nullptr);
        return;
    }

    // Hybrid UTF-8 processor: renders FontStash chunks or vector glyphs
    const char* ptr = str.data();
    const char* end = ptr + str.size();
    float curX = x;
    const float charW = 7.5f * scale;

    while (ptr < end) {
        const char* prevPtr = ptr;
        uint32_t cp = decodeNextUtf8(ptr, end);
        if (cp == ' ') {
            curX += charW;
            continue;
        }
        if (isSpecialSymbol(cp) || !(g_activeFontRenderer && g_activeFontRenderer->fs && g_activeFontRenderer->fontNormal != FONS_INVALID)) {
            drawVectorGlyph(cp, curX, y, scale, r, g, b, a);
            curX += charW;
        } else {
            // Draw regular glyph via FontStash
            std::string sub(prevPtr, ptr);
            FONScontext* fs = g_activeFontRenderer->fs;
            fonsClearState(fs);
            float fontSize = std::max(11.0f, scale * 15.5f);
            fonsSetSize(fs, fontSize);
            fonsSetFont(fs, g_activeFontRenderer->fontNormal);
            fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
            uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
            uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
            uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
            uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
            unsigned int col = static_cast<unsigned int>(cr) |
                              (static_cast<unsigned int>(cg) << 8) |
                              (static_cast<unsigned int>(cb) << 16) |
                              (static_cast<unsigned int>(ca) << 24);
            fonsSetColor(fs, col);
            curX = fonsDrawText(fs, curX, y, sub.c_str(), nullptr);
        }
    }
}

void drawVectorStringCentered(const std::string& str, float cx, float cy, float scale, float r, float g, float b, float a) {
    bool hasSpecial = false;
    for (unsigned char c : str) {
        if (c >= 0x80) {
            hasSpecial = true;
            break;
        }
    }

    if (!hasSpecial && g_activeFontRenderer && g_activeFontRenderer->fs && g_activeFontRenderer->fontNormal != FONS_INVALID) {
        FONScontext* fs = g_activeFontRenderer->fs;
        fonsClearState(fs);
        float fontSize = std::max(11.0f, scale * 15.5f);
        fonsSetSize(fs, fontSize);
        fonsSetFont(fs, g_activeFontRenderer->fontNormal);
        fonsSetAlign(fs, FONS_ALIGN_CENTER | FONS_ALIGN_MIDDLE);

        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        unsigned int col = static_cast<unsigned int>(cr) |
                          (static_cast<unsigned int>(cg) << 8) |
                          (static_cast<unsigned int>(cb) << 16) |
                          (static_cast<unsigned int>(ca) << 24);
        fonsSetColor(fs, col);
        fonsDrawText(fs, cx, cy + 0.5f, str.c_str(), nullptr);
        return;
    }

    const float charW = 7.5f * scale;
    float textW = static_cast<float>(str.size()) * charW;
    float startX = cx - (textW * 0.5f);
    float startY = cy - (4.0f * scale);
    drawVectorString(str, startX, startY, scale, r, g, b, a);
}

float drawSyntaxString(const std::string& str, float x, float y, float scale, float r, float g, float b, float a = 1.0f) {
    if (g_activeFontRenderer && g_activeFontRenderer->fs && (g_activeFontRenderer->fontMono != FONS_INVALID || g_activeFontRenderer->fontNormal != FONS_INVALID)) {
        int fid = (g_activeFontRenderer->fontMono != FONS_INVALID) ? g_activeFontRenderer->fontMono : g_activeFontRenderer->fontNormal;
        FONScontext* fs = g_activeFontRenderer->fs;
        fonsClearState(fs);
        float fontSize = std::max(11.0f, scale * 15.5f);
        fonsSetSize(fs, fontSize);
        fonsSetFont(fs, fid);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        unsigned int col = static_cast<unsigned int>(cr) |
                          (static_cast<unsigned int>(cg) << 8) |
                          (static_cast<unsigned int>(cb) << 16) |
                          (static_cast<unsigned int>(ca) << 24);
        fonsSetColor(fs, col);
        return fonsDrawText(fs, x, y, str.c_str(), nullptr);
    }
    drawVectorString(str, x, y, scale, r, g, b, a);
    return x + static_cast<float>(str.size()) * (7.0f * scale);
}

void drawMonoString(const std::string& str, float x, float y, float scale, float r, float g, float b, float a) {
    if (g_activeFontRenderer && g_activeFontRenderer->fs && (g_activeFontRenderer->fontMono != FONS_INVALID || g_activeFontRenderer->fontNormal != FONS_INVALID)) {
        int fid = (g_activeFontRenderer->fontMono != FONS_INVALID) ? g_activeFontRenderer->fontMono : g_activeFontRenderer->fontNormal;
        FONScontext* fs = g_activeFontRenderer->fs;
        fonsClearState(fs);
        float fontSize = std::max(11.0f, scale * 15.5f);
        fonsSetSize(fs, fontSize);
        fonsSetFont(fs, fid);
        fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

        uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
        uint8_t ca = static_cast<uint8_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f);
        unsigned int col = static_cast<unsigned int>(cr) |
                          (static_cast<unsigned int>(cg) << 8) |
                          (static_cast<unsigned int>(cb) << 16) |
                          (static_cast<unsigned int>(ca) << 24);
        fonsSetColor(fs, col);
        fonsDrawText(fs, x, y, str.c_str(), nullptr);
        return;
    }
    drawVectorString(str, x, y, scale, r, g, b, a);
}
#endif

inline void drawRect(float x, float y, float w, float h, const Color& c) {
    drawRect(x, y, w, h, c.r, c.g, c.b, c.a);
}

inline void drawRectGradient(float x, float y, float w, float h, const Color& c0, const Color& c1) {
    drawRectGradient(x, y, w, h, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a);
}

inline void drawRectOutline(float x, float y, float w, float h, const Color& c, float lineWidth = 1.0f) {
    drawRectOutline(x, y, w, h, c.r, c.g, c.b, c.a, lineWidth);
}

inline void drawRoundedRect(float x, float y, float w, float h, float radius, const Color& c, int cornerSegments = 8) {
    drawRoundedRect(x, y, w, h, radius, c.r, c.g, c.b, c.a, cornerSegments);
}

inline void drawRoundedRectGradient(float x, float y, float w, float h, float radius, const Color& c0, const Color& c1, int cornerSegments = 8) {
    drawRoundedRectGradient(x, y, w, h, radius, c0.r, c0.g, c0.b, c1.r, c1.g, c1.b, c0.a, cornerSegments);
}

inline void drawRoundedRectOutline(float x, float y, float w, float h, float radius, const Color& c, float lineWidth = 1.0f, int cornerSegments = 8) {
    drawRoundedRectOutline(x, y, w, h, radius, c.r, c.g, c.b, c.a, lineWidth, cornerSegments);
}

inline void drawCircle(float cx, float cy, float radius, const Color& c, int segments = 36) {
    drawCircle(cx, cy, radius, c.r, c.g, c.b, c.a, segments);
}

inline void drawCircleOutline(float cx, float cy, float radius, const Color& c, float lineWidth = 1.5f, int segments = 36) {
    drawCircleOutline(cx, cy, radius, c.r, c.g, c.b, c.a, lineWidth, segments);
}

inline void drawLine(float x0, float y0, float x1, float y1, const Color& c, float lineWidth = 1.5f) {
    drawLine(x0, y0, x1, y1, c.r, c.g, c.b, c.a, lineWidth);
}

inline float drawSyntaxString(const std::string& str, float x, float y, float scale, const Color& c) {
    return drawSyntaxString(str, x, y, scale, c.r, c.g, c.b, c.a);
}

inline void drawRect(const Rect& rect, const Color& c) {
    drawRect(rect.x, rect.y, rect.w, rect.h, c);
}

inline void drawRect(const Rect& rect, float r, float g, float b, float a = 1.0f) {
    drawRect(rect.x, rect.y, rect.w, rect.h, r, g, b, a);
}

inline void drawRectGradient(const Rect& rect, const Color& c0, const Color& c1) {
    drawRectGradient(rect.x, rect.y, rect.w, rect.h, c0, c1);
}

inline void drawRectOutline(const Rect& rect, const Color& c, float lineWidth = 1.0f) {
    drawRectOutline(rect.x, rect.y, rect.w, rect.h, c, lineWidth);
}

inline void drawRectOutline(const Rect& rect, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f) {
    drawRectOutline(rect.x, rect.y, rect.w, rect.h, r, g, b, a, lineWidth);
}

// =========================================================================
// SKEUOMORPHIC HARDWARE KNOB ENGINE (TB-303 DIODE LADDER POTENTIOMETER)
// =========================================================================

inline void renderTb303Knob(float cx, float cy, float kRad, float normVal,
                            bool isActive, bool isHovered,
                            const std::string& label, const std::string& readout,
                            const LightSource2D& light,
                            bool isSelector = false) {
    constexpr float minAngle = -2.35619449f; // -135 deg
    constexpr float maxAngle = 2.35619449f;  // +135 deg
    float currentAngle = minAngle + normVal * (maxAngle - minAngle);

    Point lDir = light.getLightDirection(cx, cy);
    float lightAngle = light.getLightAngle(cx, cy);

    // 1. Countersunk / Sunken Chassis Well (Recessed Beveled Collet)
    float wellOuterR = kRad * 1.52f;
    float wellInnerR = kRad * 1.34f;

    // Outer countersink bevel recess
    drawCircle(cx, cy, wellOuterR, 0.10f, 0.11f, 0.13f, 1.0f, 32);

    // Inner rim lighting (3D depth illusion):
    // Top inner lip facing light casts a dark shadow into well;
    // Bottom inner lip facing away catches bright specular reflection
    drawArc(cx, cy, wellInnerR, lightAngle - 1.57f, lightAngle + 1.57f,
            0.02f, 0.03f, 0.04f, 0.95f, 2.2f, 24);
    drawArc(cx, cy, wellInnerR, lightAngle + 1.57f, lightAngle + 4.71f,
            0.72f, 0.76f, 0.84f, 0.88f, 1.6f, 24);

    // Sunken well floor base
    drawCircle(cx, cy, wellInnerR - 1.0f, 0.18f, 0.19f, 0.22f, 1.0f, 32);

    // 2. Screen-Printed Dial Tick Marks (11 hash marks radiating outwards)
    constexpr int kTickCount = 11;
    const float tickStep = (maxAngle - minAngle) / static_cast<float>(kTickCount - 1);
    for (int t = 0; t < kTickCount; ++t) {
        float a = minAngle + static_cast<float>(t) * tickStep;
        float sa = std::sin(a);
        float ca = std::cos(a);

        bool isCenterTick = (t == (kTickCount / 2)); // 12-o'clock index tick
        float rStart = wellOuterR + 1.5f;
        float rEnd = rStart + (isCenterTick ? 7.5f : 4.8f);
        float tickW = isCenterTick ? 2.2f : 1.3f;

        // Ticks are screen-printed dark enamel
        drawLine(cx + sa * rStart, cy - ca * rStart,
                 cx + sa * rEnd, cy - ca * rEnd,
                 0.08f, 0.09f, 0.11f, 0.95f, tickW);
    }

    // 3. Contextual Drop Shadow onto the Well Floor
    drawCircleDropShadow(cx, cy, kRad * 1.08f, 9.5f, light, 0.62f);

    // 4. Knurled Fluted Rotor Grip (Teeth with N·L Shading)
    int numTeeth = isSelector ? 24 : 36;
    float rInnerTeeth = kRad * 0.70f;
    float rOuterTeeth = kRad * 1.08f;

    // Aluminum/Bakelite metallic palette
    drawKnurledRing(cx, cy, rInnerTeeth, rOuterTeeth, numTeeth, currentAngle, light,
                    0.72f, 0.74f, 0.78f,  // Base metal
                    1.0f, 1.0f, 1.0f,     // Specular glint
                    0.13f, 0.14f, 0.17f); // Deep fluted groove shadow

    // 5. Center Rotor Cap
    float capR = kRad * 0.75f;
    // Align top cap gradient with directional light
    float capDiffuse = 0.5f + 0.5f * std::clamp(-lDir.y, -1.0f, 1.0f);
    float cTop = 0.88f + 0.08f * capDiffuse;
    drawCircle(cx, cy, capR, cTop * 0.96f, cTop * 0.98f, cTop * 1.0f, 1.0f, 32);
    // Beveled rim edge of the top face
    drawCircleOutline(cx, cy, capR, 0.40f, 0.44f, 0.52f, 1.0f, 1.2f, 32);
    drawCircleOutline(cx, cy, capR * 0.55f, 0.82f, 0.85f, 0.90f, 0.35f, 0.8f, 24);

    // 6. Inlaid Indicator Needle Cutout (Molded groove extending across cap and knurls)
    float sa = std::sin(currentAngle);
    float ca = std::cos(currentAngle);
    float iStart = capR * 0.20f;
    float iEnd = rOuterTeeth - 0.5f;

    // Inlaid relief groove shadow
    drawLine(cx + sa * iStart + 0.6f, cy - ca * iStart + 0.6f,
             cx + sa * iEnd + 0.6f, cy - ca * iEnd + 0.6f,
             0.14f, 0.15f, 0.18f, 0.85f, 2.6f);

    // Indicator inlay line
    if (isActive) {
        // High-visibility glowing acid neon feedback when turning
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 1.0f, 0.88f, 0.12f, 1.0f, 2.6f);
    } else if (isHovered) {
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 0.30f, 0.95f, 1.0f, 1.0f, 2.2f);
    } else {
        // High-contrast clean white inlay
        drawLine(cx + sa * iStart, cy - ca * iStart,
                 cx + sa * iEnd, cy - ca * iEnd,
                 0.96f, 0.98f, 1.0f, 1.0f, 2.0f);
    }

    // 7. Label and Active Parameter Readout
    if (!label.empty()) {
        float labelScale = std::clamp(kRad * 0.038f, 0.44f, 0.65f);
        float labelW = label.size() * 5.5f * labelScale;
        drawVectorString(label, cx - labelW * 0.5f, cy + wellOuterR + 5.0f,
                         labelScale, 0.70f, 0.75f, 0.85f, 1.0f);
    }

    if (isActive || isHovered) {
        std::string text = readout.empty() ? (std::to_string(static_cast<int>(std::round(normVal * 100.0f))) + "%") : readout;
        float valScale = std::clamp(kRad * 0.036f, 0.42f, 0.60f);
        float valW = text.size() * 5.5f * valScale;
        drawVectorString(text, cx - valW * 0.5f, cy - wellOuterR - 10.0f,
                         valScale, isActive ? 1.0f : 0.4f,
                         isActive ? 0.90f : 1.0f,
                         isActive ? 0.20f : 1.0f, 1.0f);
    }
}

// =========================================================================
// HIGH-PERFORMANCE PROCEDURAL VECTOR UI ICONS (0 KB Bundle Cost)
// =========================================================================

inline void drawIconPlay(float cx, float cy, float size, const Color& fill, const Color& highlight = Color(0.0f, 0.0f, 0.0f, 0.0f)) {
    float halfH = size * 0.55f;
    float halfW = size * 0.50f;
    float x0 = cx - halfW * 0.7f;
    float y0 = cy - halfH;
    float x1 = cx + halfW;
    float y1 = cy;
    float x2 = cx - halfW * 0.7f;
    float y2 = cy + halfH;

    if (highlight.a > 0.0f) {
        drawTriangle(x0 + 0.6f, y0 + 0.8f, x1 + 0.6f, y1 + 0.8f, x2 + 0.6f, y2 + 0.8f, highlight.r, highlight.g, highlight.b, highlight.a);
    }
    drawTriangle(x0, y0, x1, y1, x2, y2, fill.r, fill.g, fill.b, fill.a);
}

inline void drawIconScrewClose(float cx, float cy, float radius, bool hovered, const Color& highlightColor = Color(0.98f, 0.32f, 0.28f)) {
    // 1. Recessed outer countersink well shadow
    drawCircle(cx, cy, radius + 1.2f, Color(0.04f, 0.03f, 0.04f, 0.95f), 16);
    drawCircle(cx, cy, radius + 0.4f, Color(0.08f, 0.08f, 0.09f, 0.85f), 16);

    if (hovered) {
        // Outer aura and halo ring in active theme highlight color
        drawCircle(cx, cy, radius + 2.2f, Color(highlightColor.r, highlightColor.g, highlightColor.b, 0.22f), 16);
        drawCircleOutline(cx, cy, radius + 1.5f, highlightColor.withAlpha(0.70f), 1.2f);
    }

    // 2. Metallic screw head body with highlight tint on hover
    Color screwBody = hovered
        ? Color::lerp(Color(0.28f, 0.30f, 0.36f), highlightColor, 0.22f)
        : Color(0.18f, 0.20f, 0.23f);
    drawCircle(cx, cy, radius, screwBody, 16);

    // 3. Chamfer highlight on top-left edge
    drawCircle(cx - 0.5f, cy - 0.5f, radius * 0.85f, Color(1.0f, 1.0f, 1.0f, hovered ? 0.35f : 0.15f), 16);
    drawCircle(cx + 0.2f, cy + 0.2f, radius * 0.85f, screwBody, 16);

    // 4. Inset Phillips cross slot ("X")
    float arm = radius * 0.55f;
    Color slotColor = hovered ? highlightColor : Color(0.06f, 0.06f, 0.08f);
    float strokeW = std::max(1.3f, radius * (hovered ? 0.32f : 0.28f));

    drawLine(cx - arm, cy - arm, cx + arm, cy + arm, slotColor, strokeW);
    drawLine(cx - arm, cy + arm, cx + arm, cy - arm, slotColor, strokeW);

    if (hovered) {
        drawCircle(cx, cy, std::max(1.5f, radius * 0.25f), highlightColor, 8);
    }
}

inline void drawIconArranger(float x, float y, float w, float h, const Color& c) {
    float laneH = h * 0.26f;
    float gap = h * 0.10f;
    drawRect(x, y, w * 0.65f, laneH, c);
    drawRect(x + w * 0.30f, y + laneH + gap, w * 0.55f, laneH, c);
    drawRect(x + w * 0.10f, y + (laneH + gap) * 2.0f, w * 0.45f, laneH, c);
}

inline void drawIconEdit(float x, float y, float size, const Color& c) {
    float s = size;

    // 1. Stylus Silhouette (45-deg chisel stylus pointing down to baseline)
    float p0x = x + 0.93f * s, p0y = y + 0.27f * s; // top right
    float p1x = x + 0.74f * s, p1y = y + 0.08f * s; // top left (cap)
    float p2x = x + 0.30f * s, p2y = y + 0.48f * s; // shaft left
    float p3x = x + 0.30f * s, p3y = y + 0.70f * s; // chisel tip bottom-left
    float p4x = x + 0.52f * s, p4y = y + 0.70f * s; // chisel tip bottom-right

    // Convex decomposition of 5-gon stylus
    drawTriangle(p0x, p0y, p1x, p1y, p2x, p2y, c.r, c.g, c.b, c.a);
    drawTriangle(p0x, p0y, p2x, p2y, p4x, p4y, c.r, c.g, c.b, c.a);
    drawTriangle(p2x, p2y, p3x, p3y, p4x, p4y, c.r, c.g, c.b, c.a);

    // 2. Diamond cutout near cap
    if (s >= 8.0f) {
        float cx = x + 0.75f * s;
        float cy = y + 0.25f * s;
        float d = std::max(1.0f, 0.065f * s);
        Color bg(0.08f, 0.09f, 0.11f, c.a);
        drawTriangle(cx, cy - d, cx + d, cy, cx, cy + d, bg.r, bg.g, bg.b, bg.a);
        drawTriangle(cx, cy - d, cx - d, cy, cx, cy + d, bg.r, bg.g, bg.b, bg.a);
    }

    // 3. Baseline underline bar (solid filled left section + outlined right box)
    float barH = std::max(1.5f, 0.14f * s);
    float barY = y + 0.78f * s;
    float barX = x + 0.06f * s;
    float barW = 0.88f * s;

    // Solid filled left portion (~68%)
    float filledW = barW * 0.68f;
    drawRect(barX, barY, filledW, barH, c);

    // Outlined right box (~32%)
    float emptyX = barX + filledW;
    float emptyW = barW - filledW;
    float strokeW = std::max(1.0f, 0.06f * s);
    drawLine(emptyX, barY + strokeW * 0.5f, emptyX + emptyW, barY + strokeW * 0.5f, c, strokeW);
    drawLine(emptyX, barY + barH - strokeW * 0.5f, emptyX + emptyW, barY + barH - strokeW * 0.5f, c, strokeW);
    drawLine(emptyX + emptyW - strokeW * 0.5f, barY, emptyX + emptyW - strokeW * 0.5f, barY + barH, c, strokeW);
}

inline void drawIconTrack(float x, float y, float w, float h, const Color& c) {
    float bars[5] = {0.35f, 0.75f, 1.0f, 0.60f, 0.40f};
    float bw = w / 6.0f;
    for (int i = 0; i < 5; ++i) {
        float bh = h * bars[i];
        float by = y + (h - bh) * 0.5f;
        drawRect(x + i * (bw + 1.0f), by, bw, bh, c);
    }
}

inline void drawIconMixer(float x, float y, float w, float h, const Color& c) {
    float step = w / 3.0f;
    float faderH = h * 0.28f;
    float faderW = step * 0.85f;
    for (int i = 0; i < 3; ++i) {
        float fx = x + i * step + step * 0.5f;
        drawLine(fx, y, fx, y + h, Color(c.r, c.g, c.b, 0.45f), 1.2f);
        float knobY = y + (i == 0 ? h * 0.20f : (i == 1 ? h * 0.65f : h * 0.40f));
        drawRect(fx - faderW * 0.5f, knobY - faderH * 0.5f, faderW, faderH, c);
    }
}

inline void drawIconDesign(float x, float y, float w, float h, const Color& c) {
    float midY = y + h * 0.5f;
    drawLine(x + w * 0.35f, y + 1.0f, x + 1.0f, midY, c, 1.5f);
    drawLine(x + 1.0f, midY, x + w * 0.35f, y + h - 1.0f, c, 1.5f);
    drawLine(x + w * 0.65f, y + 1.0f, x + w - 1.0f, midY, c, 1.5f);
    drawLine(x + w - 1.0f, midY, x + w * 0.65f, y + h - 1.0f, c, 1.5f);
}

inline void drawIconPianoRoll(float x, float y, float w, float h, const Color& c) {
    drawRect(x, y, w, h, c);
    float kw = w / 3.0f;
    drawLine(x + kw, y, x + kw, y + h, Color(0.1f, 0.1f, 0.12f, 0.8f), 1.0f);
    drawLine(x + kw * 2.0f, y, x + kw * 2.0f, y + h, Color(0.1f, 0.1f, 0.12f, 0.8f), 1.0f);
    float bkW = kw * 0.55f;
    float bkH = h * 0.55f;
    drawRect(x + kw - bkW * 0.5f, y, bkW, bkH, Color(0.08f, 0.08f, 0.10f, 1.0f));
    drawRect(x + kw * 2.0f - bkW * 0.5f, y, bkW, bkH, Color(0.08f, 0.08f, 0.10f, 1.0f));
}

inline void drawIconTracker(float x, float y, float w, float h, const Color& c) {
    drawRectOutline(x, y, w, h, c, 1.0f);
    drawLine(x + w * 0.35f, y, x + w * 0.35f, y + h, Color(c.r, c.g, c.b, 0.5f), 1.0f);
    drawLine(x + w * 0.70f, y, x + w * 0.70f, y + h, Color(c.r, c.g, c.b, 0.5f), 1.0f);
    for (float ry = y + 3.0f; ry < y + h - 2.0f; ry += 3.5f) {
        drawLine(x + 2.0f, ry, x + w * 0.30f, ry, c, 1.0f);
        drawLine(x + w * 0.40f, ry, x + w * 0.65f, ry, c, 1.0f);
    }
}

inline void drawIconScore(float x, float y, float w, float h, const Color& c) {
    drawCircle(x + w * 0.35f, y + h - 3.5f, 2.5f, c);
    drawLine(x + w * 0.35f + 2.0f, y + h - 3.5f, x + w * 0.35f + 2.0f, y + 2.0f, c, 1.2f);
    drawLine(x + w * 0.35f + 2.0f, y + 2.0f, x + w * 0.35f + 6.0f, y + 5.0f, c, 1.2f);
}

GuiWindow::GuiWindow(uint32_t width, uint32_t height, const std::string& title)
    : width_(width), height_(height),
      windowWidth_(static_cast<int>(width)), windowHeight_(static_cast<int>(height)),
      fbWidth_(static_cast<int>(width)), fbHeight_(static_cast<int>(height)),
      title_(title),
      canvas_(static_cast<float>(width), static_cast<float>(height)),
      dawnBridge_(width, height) {
    setStatusMessage("Eatsbits Modular Workstation Active [Google Dawn]");
    loadPresets();
    initEatscriptIde();
    initNoteScriptEditor();
    initArrangerTracks();
}

GuiWindow::~GuiWindow() {
#if defined(__EMSCRIPTEN__)
    if (g_activeGuiWindowForWeb == this) {
        g_activeGuiWindowForWeb = nullptr;
    }
#endif
    close();
}

void GuiWindow::setStatusMessage(const std::string& msg) noexcept {
    lastStatusMessage_ = msg;
    statusToastText_ = msg;
    statusToastTimer_ = 3.5f;
}

void GuiWindow::setUiScale(float scale) noexcept {
    uiScale_ = std::clamp(scale, 0.50f, 2.50f);
    updateLogicalDimensions();
    setStatusMessage("UI Scale: " + std::to_string(static_cast<int>(std::round(uiScale_ * 100.0f))) + "%");
}

void GuiWindow::zoomIn() noexcept {
    setUiScale(uiScale_ + 0.10f);
}

void GuiWindow::zoomOut() noexcept {
    setUiScale(uiScale_ - 0.10f);
}

void GuiWindow::resetZoom() noexcept {
    setUiScale(1.0f);
}

void GuiWindow::setAntiAliasingMode(int mode) noexcept {
    antiAliasingMode_ = std::clamp(mode, 0, 2);
    if (batchRenderer_) {
        batchRenderer_->setAntiAliasingMode(antiAliasingMode_);
    }
    const char* names[3] = {"Off (1x Point Sampling)", "2x Fast RGSS", "4x High Quality RGSS"};
    setStatusMessage(std::string("Anti-Aliasing: ") + names[antiAliasingMode_]);
}

void GuiWindow::setHiDpiEnabled(bool enable) noexcept {
    if (hiDpiEnabled_ == enable) return;
    hiDpiEnabled_ = enable;
    updateLogicalDimensions();
    if (window_) {
        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
        dawnBridge_.resize(physW, physH);
        if (batchRenderer_) {
            batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
            if (dawnBridge_.isNativeActive()) {
                batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
            }
#endif
        }
    }
    setStatusMessage(hiDpiEnabled_ ? "HiDPI Canvas: Native 1:1 Enabled" : "HiDPI Canvas: Standard 1x (Performance Mode)");
}

void GuiWindow::toggleHiDpi() noexcept {
    setHiDpiEnabled(!hiDpiEnabled_);
}

float GuiWindow::windowToLogicalX(double winX) const noexcept {
    if (uiScale_ <= 0.0001f) return static_cast<float>(winX);
    return static_cast<float>(winX) / uiScale_;
}

float GuiWindow::windowToLogicalY(double winY) const noexcept {
    if (uiScale_ <= 0.0001f) return static_cast<float>(winY);
    return static_cast<float>(winY) / uiScale_;
}

void GuiWindow::remapCrtMouseCoords(float inX, float inY, float& outX, float& outY) const noexcept {
    if (!crtShaderEnabled_ || width_ <= 0 || height_ <= 0) {
        outX = inX;
        outY = inY;
        return;
    }

    const auto& cfg = dawnBridge_.getMaterialConfig();
    const float totalW = static_cast<float>(width_);
    const float totalH = static_cast<float>(height_);
    const float topBarH = static_cast<float>(cfg.topBarHeightPx);
    const float bottomBarH = static_cast<float>(cfg.bottomBarHeightPx);

    const float topCut = topBarH / totalH;
    const float bottomCut = 1.0f - (bottomBarH / totalH);

    // Apply rumble shake offset matching shader globalUV += u.rumbleOffset
    float u = (inX / totalW) + dawnBridge_.getRumbleOffsetX();
    float v = (inY / totalH) + dawnBridge_.getRumbleOffsetY();

    // Top transport bar and bottom navigation chin have zero curvature and 1:1 hit testing
    if (v < topCut || v > bottomCut) {
        outX = std::clamp(u, 0.0f, 1.0f) * totalW;
        outY = std::clamp(v, 0.0f, 1.0f) * totalH;
        return;
    }

    const float span = std::max(0.01f, bottomCut - topCut);
    const float suvX = u;
    const float suvY = (v - topCut) / span;

    // Chassis frame dimensions matching crt_screen.wgsl:
    // let screenRes = vec2<f32>(u.resolution.x, u.resolution.y * span);
    // let frameWidthX_px = 22.0;
    // let frameHeightY_px = 20.0;
    const float fboW = static_cast<float>(dawnBridge_.getFboWidth() > 0 ? dawnBridge_.getFboWidth() : width_);
    const float fboH = static_cast<float>(dawnBridge_.getFboHeight() > 0 ? dawnBridge_.getFboHeight() : height_);
    const float screenResX = fboW;
    const float screenResY = fboH * span;

    constexpr float frameWidthX_px = 22.0f;
    constexpr float frameHeightY_px = 20.0f;

    const float frameFracX = frameWidthX_px / std::max(1.0f, screenResX);
    const float frameFracY = frameHeightY_px / std::max(1.0f, screenResY);

    // Map screen coordinate inside the phosphor tube aperture:
    // tubeUV = (suv - frameFrac) / (1.0 - 2.0 * frameFrac)
    const float denomX = std::max(0.001f, 1.0f - 2.0f * frameFracX);
    const float denomY = std::max(0.001f, 1.0f - 2.0f * frameFracY);
    float tubeUVX = std::clamp((suvX - frameFracX) / denomX, 0.0f, 1.0f);
    float tubeUVY = std::clamp((suvY - frameFracY) / denomY, 0.0f, 1.0f);

    // Apply exact CRT bulb curvature polynomial matching WGSL:
    // curvedTube += dCenter * pow(dist, 2.6) * (u.curvature * 0.08)
    float curvedTubeX = tubeUVX;
    float curvedTubeY = tubeUVY;
    if (cfg.curvature > 0.001f) {
        const float dCenterX = curvedTubeX - 0.5f;
        const float dCenterY = curvedTubeY - 0.5f;
        const float dist = std::sqrt(dCenterX * dCenterX + dCenterY * dCenterY);
        const float factor = std::pow(dist, 2.6f) * (cfg.curvature * 0.08f);
        curvedTubeX += dCenterX * factor;
        curvedTubeY += dCenterY * factor;
    }

    // Horizontal sync scan wave modulation matching shader
    float hWave = 0.0f;
    const float hWaveStrength = dawnBridge_.getHWaveStrength();
    if (hWaveStrength > 0.00001f && cfg.hsyncDistortion > 0.001f) {
        hWave = std::sin(suvY * 10.0f + dawnBridge_.getLampTime() * 5.0f) * (hWaveStrength * cfg.hsyncDistortion);
    }

    const float texUVX = std::clamp(curvedTubeX + hWave, 0.0f, 1.0f);
    const float texUVY = topCut + std::clamp(curvedTubeY, 0.0f, 1.0f) * span;

    outX = texUVX * totalW;
    outY = texUVY * totalH;
}

void GuiWindow::onWindowResize(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return;
    windowWidth_ = width;
    windowHeight_ = height;
    updateLogicalDimensions();
    uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
    uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
    dawnBridge_.resize(physW, physH);
    if (batchRenderer_) {
        batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
        if (dawnBridge_.isNativeActive()) {
            batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
        }
#endif
    }
}

void GuiWindow::onFramebufferResize(int width, int height) noexcept {
    if (width <= 0 || height <= 0) return;
    fbWidth_ = width;
    fbHeight_ = height;
    updateLogicalDimensions();
    uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
    uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
    dawnBridge_.resize(physW, physH);
    if (batchRenderer_) {
        batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
        if (dawnBridge_.isNativeActive()) {
            batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
        }
#endif
    }
    if (!isRendering_ && engine_) {
        renderFrame();
    }
}

void GuiWindow::updateLogicalDimensions() noexcept {
    if (uiScale_ <= 0.0001f) uiScale_ = 1.0f;

    float dpiScaleX = 1.0f;
#if EATS_HAS_GLFW
#if defined(__EMSCRIPTEN__)
    double dpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });
    dpiScaleX = static_cast<float>(dpr);
    if (dpiScaleX <= 0.0f) dpiScaleX = 1.0f;
#else
    if (window_ && windowWidth_ > 0 && fbWidth_ > 0) {
        dpiScaleX = static_cast<float>(fbWidth_) / static_cast<float>(windowWidth_);
    }
#endif
#endif
    if (!hiDpiEnabled_) {
        dpiScale_ = 1.0f;
    } else {
        dpiScale_ = std::max(1.0f, dpiScaleX);
    }
    renderScale_ = dpiScale_ * uiScale_;

    float baseW = hiDpiEnabled_ ? static_cast<float>(fbWidth_) : static_cast<float>(windowWidth_);
    float baseH = hiDpiEnabled_ ? static_cast<float>(fbHeight_) : static_cast<float>(windowHeight_);
    width_ = static_cast<uint32_t>(std::max(1.0f, std::round(baseW / renderScale_)));
    height_ = static_cast<uint32_t>(std::max(1.0f, std::round(baseH / renderScale_)));
    canvas_.setCanvasSize(static_cast<float>(width_), static_cast<float>(height_));

    if (batchRenderer_) {
        batchRenderer_->setRenderScale(renderScale_, renderScale_);
    }
    dawnBridge_.setRenderScale(renderScale_);
}

void GuiWindow::toggleFullscreen() noexcept {
    setFullscreen(!isFullscreen_);
}

void GuiWindow::setFullscreen(bool enable) noexcept {
#if EATS_HAS_GLFW
    if (!window_) return;
    if (isFullscreen_ == enable) return;
    isFullscreen_ = enable;

#if defined(__EMSCRIPTEN__)
    if (enable) {
        EM_ASM({
            var el = document.getElementById('canvas-container') || document.documentElement;
            if (el.requestFullscreen) {
                el.requestFullscreen();
            } else if (el.webkitRequestFullscreen) {
                el.webkitRequestFullscreen();
            }
        });
    } else {
        EM_ASM({
            if (document.exitFullscreen) {
                document.exitFullscreen();
            } else if (document.webkitExitFullscreen) {
                document.webkitExitFullscreen();
            }
        });
    }
    setStatusMessage(isFullscreen_ ? "Display: FULLSCREEN (Esc to exit)" : "Display: WINDOWED");
    pollWebResize();
#elif defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(window_);
    if (hwnd) {
        if (enable) {
            // Save current window style & rect before going fullscreen
            LONG style = GetWindowLong(hwnd, GWL_STYLE);
            if (!hasSavedWindowState_) {
                savedWindowStyle_ = style;
                RECT rect{};
                if (GetWindowRect(hwnd, &rect)) {
                    savedWindowX_ = rect.left;
                    savedWindowY_ = rect.top;
                    savedWindowW_ = rect.right - rect.left;
                    savedWindowH_ = rect.bottom - rect.top;
                }
                hasSavedWindowState_ = true;
            }

            int posX = 0;
            int posY = 0;
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            if (hMonitor) {
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(hMonitor, &mi)) {
                    posX = mi.rcMonitor.left;
                    posY = mi.rcMonitor.top;
                    screenWidth = mi.rcMonitor.right - mi.rcMonitor.left;
                    screenHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
                }
            }

            // Remove window borders & titlebar (borderless fullscreen)
            LONG newStyle = (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP | WS_VISIBLE;
            SetWindowLong(hwnd, GWL_STYLE, newStyle);

            // Position window to cover entire monitor
            SetWindowPos(
                hwnd,
                HWND_TOP,
                posX,
                posY,
                screenWidth,
                screenHeight,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
            );
        } else {
            // Restore regular windowed style
            LONG restoreStyle = (savedWindowStyle_ != 0) ? savedWindowStyle_ : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowLong(hwnd, GWL_STYLE, restoreStyle);

            SetWindowPos(
                hwnd,
                HWND_TOP,
                savedWindowX_,
                savedWindowY_,
                savedWindowW_,
                savedWindowH_,
                SWP_NOZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW
            );
            ShowWindow(hwnd, SW_RESTORE);
        }

        // Update GLFW cached sizes and redraw
        glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
        glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
        updateLogicalDimensions();
        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
        dawnBridge_.resize(physW, physH);
        if (batchRenderer_) {
            batchRenderer_->resize(physW, physH);
#if defined(__EMSCRIPTEN__)
            if (dawnBridge_.isNativeActive()) {
                batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
            }
#endif
        }
        renderFrame();
    }
#else
    if (enable) {
        glfwGetWindowPos(window_, &savedWindowX_, &savedWindowY_);
        glfwGetWindowSize(window_, &savedWindowW_, &savedWindowH_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                glfwSetWindowMonitor(window_, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            }
        }
    } else {
        glfwSetWindowMonitor(window_, nullptr, savedWindowX_, savedWindowY_, savedWindowW_, savedWindowH_, 0);
    }
#endif

    setStatusMessage(isFullscreen_ ? "Display: FULLSCREEN (F11 / Alt+Enter to exit)" : "Display: WINDOWED (F11 / Alt+Enter for fullscreen)");
#endif
}

void GuiWindow::initDefaultKnobValues() {
    const auto& modules = canvas_.getModules();
    for (const auto& m : modules) {
        for (size_t k = 0; k < m.knobNames.size(); ++k) {
            auto key = std::make_pair(m.id, static_cast<uint32_t>(k));
            if (knobValues_.find(key) != knobValues_.end()) {
                continue;
            }
            float defVal = 0.5f;
            if (m.type == "tb303") {
                if (k == 0) defVal = 0.45f;      // Cutoff
                else if (k == 1) defVal = 0.70f; // Res
                else if (k == 2) defVal = 0.65f; // EnvMod
                else if (k == 3) defVal = 0.50f; // Decay
                else if (k == 4) defVal = 0.75f; // Accent
                else if (k == 5) defVal = 0.0f;  // Wave
            } else if (m.type == "drum_kit") {
                if (k == 0) defVal = 0.85f;      // Master
                else if (k == 1) defVal = 0.50f; // Tune
                else if (k == 2) defVal = 0.60f; // Decay
                else if (k == 3) defVal = 0.20f; // Overdrive
            } else if (m.type == "delay") {
                if (k == 0) defVal = 0.35f;      // Time
                else if (k == 1) defVal = 0.45f; // Feedback
                else if (k == 2) defVal = 0.30f; // Mix
            } else if (m.type == "gain") {
                if (k == 0) defVal = 0.80f;      // Volume
                else if (k == 1) defVal = 0.50f; // Pan
            } else if (m.type == "sid") {
                if (k == 0) defVal = 0.0f;       // Pulse
                else if (k == 1) defVal = 0.50f; // 50% PW
                else if (k == 2) defVal = 0.65f; // Cutoff
                else if (k == 3) defVal = 0.60f; // Res
                else if (k == 4) defVal = 0.07f; // Attack
                else if (k == 5) defVal = 0.33f; // Release
            } else if (m.type == "dx7") {
                if (k == 0) defVal = 4.0f / 31.0f; // Algorithm 5
                else if (k == 1) defVal = 6.0f / 7.0f; // Feedback 6
                else if (k == 2) defVal = 0.50f;  // Brightness 1.0
                else if (k == 3) defVal = 0.425f; // TineBell 0.85
                else if (k == 4) defVal = 0.50f;  // BodyWarmth 1.0
                else if (k == 5) defVal = 0.425f; // MasterVolume 0.85
            } else if (m.type == "snes") {
                if (k == 0) defVal = 1.0f / 11.0f; // Square
                else if (k == 1) defVal = 0.05f;   // Attack
                else if (k == 2) defVal = 0.25f;   // Decay
                else if (k == 3) defVal = 0.40f;   // Sustain
                else if (k == 4) defVal = 0.20f;   // Release
                else if (k == 5) defVal = 0.40f;   // Echo Volume
                else if (k == 6) defVal = 0.85f;   // Master Volume
            } else if (m.type == "ym2612") {
                if (k == 0) defVal = 4.0f / 7.0f;  // Algorithm 4
                else if (k == 1) defVal = 5.0f / 7.0f; // Feedback 5
                else if (k == 2) defVal = 24.0f / 127.0f; // Op1TL
                else if (k == 3) defVal = 0.0f;    // Op2TL
                else if (k == 4) defVal = 18.0f / 127.0f; // Op3TL
                else if (k == 5) defVal = 0.80f;   // Master Volume
            } else if (m.type == "convolver") {
                if (k == 0) defVal = 1.0f / 12.0f; // Great Hall (index 1)
                else if (k == 1) defVal = 0.12f;   // PreDelay 12ms
                else if (k == 2) defVal = 0.375f;  // Decay 1.0
                else if (k == 3) defVal = 0.41f;   // HighCut 8500Hz
                else if (k == 4) defVal = 0.03f;   // LowCut 80Hz
                else if (k == 5) defVal = 0.35f;   // Mix 35%
            }
            knobValues_[key] = defVal;
        }
    }
}

void GuiWindow::updateMixerStrips() {
    const auto& modules = canvas_.getModules();
    for (const auto& m : modules) {
        bool exists = false;
        for (const auto& s : mixerStrips_) {
            if (s.nodeId == m.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            MixerStrip strip;
            strip.name = m.name;
            strip.nodeId = m.id;
            strip.volume = (m.type == "gain") ? 0.85f : 0.75f;
            strip.pan = 0.0f;
            strip.mute = false;
            strip.solo = false;
            strip.peakL = 0.0f;
            strip.peakR = 0.0f;
            mixerStrips_.push_back(strip);
        }
    }
}

float GuiWindow::getKnobValue(audio::NodeId nodeId, uint32_t knobIndex) const noexcept {
    auto it = knobValues_.find({nodeId, knobIndex});
    if (it != knobValues_.end()) {
        return it->second;
    }
    return 0.5f;
}

void GuiWindow::setKnobValue(audio::NodeId nodeId, uint32_t knobIndex, float value) noexcept {
    knobValues_[{nodeId, knobIndex}] = std::clamp(value, 0.0f, 1.0f);
}

void GuiWindow::dispatchKnobParameter(audio::NodeId nodeId, uint32_t knobIndex, float normalizedVal) noexcept {
    if (!engine_) return;

    std::string modType = "generic";
    for (const auto& m : canvas_.getModules()) {
        if (m.id == nodeId) {
            modType = m.type;
            break;
        }
    }

    float audioVal = normalizedVal;
    if (modType == "tb303") {
        if (knobIndex == 0) { // Cutoff sweep: 150 Hz to 8000 Hz
            audioVal = 150.0f * std::pow(8000.0f / 150.0f, normalizedVal);
            engine_->setCutoff(audioVal);
        } else if (knobIndex == 1) { // Res: 0.0 to 0.98
            audioVal = normalizedVal * 0.98f;
            engine_->setResonance(audioVal);
        } else if (knobIndex == 2) { // EnvMod
            audioVal = normalizedVal;
        } else if (knobIndex == 3) { // Decay
            audioVal = normalizedVal;
        } else if (knobIndex == 4) { // Accent
            audioVal = normalizedVal;
        } else if (knobIndex == 5) { // Waveform (0=Saw, 1=Square)
            audioVal = (normalizedVal >= 0.5f) ? 1.0f : 0.0f;
        }
    } else if (modType == "delay") {
        if (knobIndex == 0) audioVal = 10.0f + normalizedVal * 990.0f; // 10ms - 1000ms
        else if (knobIndex == 1) audioVal = normalizedVal * 0.95f;     // Feedback
        else if (knobIndex == 2) audioVal = normalizedVal;             // Dry/Wet
    } else if (modType == "gain") {
        if (knobIndex == 0) {
            audioVal = normalizedVal * 1.5f;                           // Volume
            engine_->setMasterVolume(audioVal);
        } else if (knobIndex == 1) {
            audioVal = (normalizedVal - 0.5f) * 2.0f;                  // Pan
        }
    } else if (modType == "drum_kit") {
        if (knobIndex == 0) audioVal = normalizedVal * 1.5f;           // Master
        else audioVal = normalizedVal;
    } else if (modType == "biquad") {
        if (knobIndex == 0) audioVal = 60.0f * std::pow(18000.0f / 60.0f, normalizedVal);
        else if (knobIndex == 1) audioVal = 0.5f + normalizedVal * 9.5f;
    } else if (modType == "poly_synth") {
        if (knobIndex == 0) {
            audioVal = 150.0f * std::pow(12000.0f / 150.0f, normalizedVal);
            engine_->setCutoff(audioVal);
        } else if (knobIndex == 1) {
            audioVal = normalizedVal * 0.98f;
            engine_->setResonance(audioVal);
        } else {
            audioVal = normalizedVal;
        }
    } else if (modType == "sid") {
        uint32_t sidParamId = 0;
        if (knobIndex == 0) { // Waveform (0 to 5)
            audioVal = std::round(normalizedVal * 5.0f);
            sidParamId = 0;
        } else if (knobIndex == 1) { // PW (100 to 4000)
            audioVal = 100.0f + normalizedVal * 3900.0f;
            sidParamId = 1;
        } else if (knobIndex == 2) { // Cutoff (100 to 2047)
            audioVal = 100.0f + normalizedVal * 1947.0f;
            sidParamId = 8;
        } else if (knobIndex == 3) { // Res (0 to 15)
            audioVal = normalizedVal * 15.0f;
            sidParamId = 9;
        } else if (knobIndex == 4) { // Atk (0 to 15)
            audioVal = std::round(normalizedVal * 15.0f);
            sidParamId = 11;
        } else if (knobIndex == 5) { // Rel (0 to 15)
            audioVal = std::round(normalizedVal * 15.0f);
            sidParamId = 14;
        }
        engine_->postNodeParameter(nodeId, sidParamId, audioVal);
        return;
    } else if (modType == "dx7") {
        uint32_t dx7ParamId = 0;
        if (knobIndex == 0) { // Algorithm (1..32)
            audioVal = 1.0f + std::round(normalizedVal * 31.0f);
            dx7ParamId = 0;
        } else if (knobIndex == 1) { // Feedback (0..7)
            audioVal = std::round(normalizedVal * 7.0f);
            dx7ParamId = 1;
        } else if (knobIndex == 2) { // Brightness (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 3;
        } else if (knobIndex == 3) { // TineBell (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 4;
        } else if (knobIndex == 4) { // BodyWarmth (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 5;
        } else if (knobIndex == 5) { // MasterVolume (0..2)
            audioVal = normalizedVal * 2.0f;
            dx7ParamId = 2;
        }
        engine_->postNodeParameter(nodeId, dx7ParamId, audioVal);
        return;
    } else if (modType == "snes") {
        uint32_t snesParamId = 0;
        if (knobIndex == 0) { // Waveform (0..11)
            audioVal = std::round(normalizedVal * 11.0f);
            snesParamId = 0;
        } else if (knobIndex == 1) { // Attack
            audioVal = 0.001f + normalizedVal * 0.999f;
            snesParamId = 2;
        } else if (knobIndex == 2) { // Decay
            audioVal = 0.01f + normalizedVal * 0.99f;
            snesParamId = 3;
        } else if (knobIndex == 3) { // Sustain
            audioVal = normalizedVal;
            snesParamId = 4;
        } else if (knobIndex == 4) { // Release
            audioVal = 0.01f + normalizedVal * 0.99f;
            snesParamId = 5;
        } else if (knobIndex == 5) { // Echo Volume
            audioVal = normalizedVal;
            snesParamId = 9;
        } else if (knobIndex == 6) { // Master Volume
            audioVal = normalizedVal * 1.5f;
            snesParamId = 14;
        }
        engine_->postNodeParameter(nodeId, snesParamId, audioVal);
        return;
    } else if (modType == "ym2612") {
        uint32_t ymParamId = 0;
        if (knobIndex == 0) { // Algorithm (0..7)
            audioVal = std::round(normalizedVal * 7.0f);
            ymParamId = 0;
        } else if (knobIndex == 1) { // Feedback (0..7)
            audioVal = std::round(normalizedVal * 7.0f);
            ymParamId = 1;
        } else if (knobIndex == 2) { // Op1TL (0..127)
            audioVal = normalizedVal * 127.0f;
            ymParamId = 4;
        } else if (knobIndex == 3) { // Op2TL (0..127)
            audioVal = normalizedVal * 127.0f;
            ymParamId = 6;
        } else if (knobIndex == 4) { // Op3TL (0..127)
            audioVal = normalizedVal * 127.0f;
            ymParamId = 8;
        } else if (knobIndex == 5) { // Master Volume (0..1.5)
            audioVal = normalizedVal * 1.5f;
            ymParamId = 2;
        }
        engine_->postNodeParameter(nodeId, ymParamId, audioVal);
        return;
    } else if (modType == "convolver") {
        uint32_t convParamId = 0;
        if (knobIndex == 0) { // Preset index (0..12)
            audioVal = std::round(normalizedVal * 12.0f);
            convParamId = 1;
        } else if (knobIndex == 1) { // Pre-Delay (0..100 ms)
            audioVal = normalizedVal * 100.0f;
            convParamId = 2;
        } else if (knobIndex == 2) { // Decay (0.1..2.5x)
            audioVal = 0.1f + normalizedVal * 2.4f;
            convParamId = 3;
        } else if (knobIndex == 3) { // HighCut (500..20000 Hz)
            audioVal = 500.0f + normalizedVal * 19500.0f;
            convParamId = 4;
        } else if (knobIndex == 4) { // LowCut (20..2000 Hz)
            audioVal = 20.0f + normalizedVal * 1980.0f;
            convParamId = 5;
        } else if (knobIndex == 5) { // Mix (0..1)
            audioVal = normalizedVal;
            convParamId = 0;
        }
        engine_->postNodeParameter(nodeId, convParamId, audioVal);
        return;
    }

    engine_->postNodeParameter(nodeId, knobIndex, audioVal);
}

namespace {
void hitTestNodeRecursive(const project::GuiLayoutNode& node, float x, float y, const project::PresetDefinition& preset, HitTestHardwareKnobResult& out, float faceplateX = 40.0f, float faceplateY = 98.0f, float scale = 1.0f) {
    if (out.hit) return;

    if (node.type == project::GuiNodeType::Knob) {
        float adjNodeY = faceplateY + 54.0f + (node.boundsY - 152.0f) * scale;
        float adjNodeX = faceplateX + (node.boundsX - 40.0f);
        float cx = adjNodeX + node.boundsW * 0.5f;
        float cy = adjNodeY + node.boundsH * 0.35f * (scale < 0.95f ? scale : 1.0f);
        float radius = std::max(node.size * 0.42f + 4.0f, 18.0f);
        if (std::hypot(x - cx, y - cy) <= radius) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {cx, cy};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.5f;
            return;
        }
    }

    for (const auto& child : node.children) {
        hitTestNodeRecursive(child, x, y, preset, out, faceplateX, faceplateY, scale);
        if (out.hit) return;
    }
}

void hitTestCompactNodeRecursive(const project::GuiLayoutNode& node, float localX, float localY, const project::PresetDefinition& preset, HitTestHardwareKnobResult& out) {
    if (out.hit) return;

    if (node.type == project::GuiNodeType::Knob) {
        float cx = node.boundsX + node.boundsW * 0.5f;
        float cy = node.boundsY + node.boundsH * 0.38f;
        float radius = std::max(node.size * 0.42f + 4.0f, 18.0f);
        if (std::hypot(localX - cx, localY - cy) <= radius) {
            out.hit = true;
            out.paramName = node.paramName;
            out.position = {cx, cy};
            const auto* p = preset.findParam(node.paramName);
            out.currentNormVal = p ? p->getNormalized() : 0.5f;
            return;
        }
    }

    for (const auto& child : node.children) {
        hitTestCompactNodeRecursive(child, localX, localY, preset, out);
        if (out.hit) return;
    }
}
} // namespace

void GuiWindow::loadPresets() {
    presets_ = project::PresetLoader::getBuiltinPresets();

    // Scan for external presets in eatsbeats folder if present
    const std::string candidatePaths[] = {
        "c:/git/eatsbeats/presets/instruments/eats_303.eats",
        "c:/git/eatsbeats/presets/drums/analog_808_kick.eats",
        "c:/git/eatsbeats/presets/drums/analog_909_snare.eats",
        "c:/git/eatsbeats/presets/audio_fx/stereo_delay.eats",
        "c:/git/eatsbeats/presets/audio_fx/bitcrusher.eats"
    };

    for (const auto& path : candidatePaths) {
        project::PresetDefinition loaded{};
        if (project::PresetLoader::loadFromFile(path, loaded)) {
            bool replaced = false;
            for (auto& existing : presets_) {
                if (existing.metadata.id == loaded.metadata.id) {
                    existing = loaded;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                presets_.push_back(loaded);
            }
        }
    }

    if (!presets_.empty()) {
        for (auto& pr : presets_) {
            pr.compactGuiRoot = pr.guiRoot;
            project::PresetLoader::computeLayoutBounds(pr.compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
        }
        project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                                   40.0f, 98.0f,
                                                   static_cast<float>(width_) - 80.0f,
                                                   static_cast<float>(height_) - 154.0f);
    }
}

void GuiWindow::loadPresetFile(const std::string& filePath) {
    project::PresetDefinition loaded{};
    if (project::PresetLoader::loadFromFile(filePath, loaded)) {
        loaded.compactGuiRoot = loaded.guiRoot;
        project::PresetLoader::computeLayoutBounds(loaded.compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
        presets_.push_back(loaded);
        activePresetIndex_ = presets_.size() - 1;
        project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                                   40.0f, 98.0f,
                                                   static_cast<float>(width_) - 80.0f,
                                                   static_cast<float>(height_) - 154.0f);
    }
}

void GuiWindow::nextPreset() {
    if (presets_.empty()) return;
    activePresetIndex_ = (activePresetIndex_ + 1) % presets_.size();
    presets_[activePresetIndex_].compactGuiRoot = presets_[activePresetIndex_].guiRoot;
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                               40.0f, 98.0f,
                                               static_cast<float>(width_) - 80.0f,
                                               static_cast<float>(height_) - 154.0f);
    if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
        auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (trk && !presets_[activePresetIndex_].rawScript.empty()) {
            trk->setEatscriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
    if (designSubView_ == DesignSubView::Eatscript && !presets_[activePresetIndex_].rawScript.empty()) {
        setScriptCode(presets_[activePresetIndex_].rawScript);
    }
}

void GuiWindow::prevPreset() {
    if (presets_.empty()) return;
    if (activePresetIndex_ == 0) {
        activePresetIndex_ = presets_.size() - 1;
    } else {
        activePresetIndex_--;
    }
    presets_[activePresetIndex_].compactGuiRoot = presets_[activePresetIndex_].guiRoot;
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                               40.0f, 98.0f,
                                               static_cast<float>(width_) - 80.0f,
                                               static_cast<float>(height_) - 154.0f);
    if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
        auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (trk && !presets_[activePresetIndex_].rawScript.empty()) {
            trk->setEatscriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
    if (designSubView_ == DesignSubView::Eatscript && !presets_[activePresetIndex_].rawScript.empty()) {
        setScriptCode(presets_[activePresetIndex_].rawScript);
    }
}

void GuiWindow::setSelectedTrackIndex(uint32_t idx) noexcept {
    selectedTrackIndex_ = idx;
    if (engine_) {
        engine_->setActiveTrack(idx);
    }
    if (virtualKeyboardDrawerWidget_) {
        virtualKeyboardDrawerWidget_->setActiveTrackIndex(idx);
    }
    if (modularArrangerView_) {
        modularArrangerView_->setActiveTrack(idx);
    }
    if (modularTrackInspectorView_) {
        modularTrackInspectorView_->setActiveTrack(idx);
    }
    syncActiveClipToEditView(idx, -1);
    syncTrackToPreset(idx);
}

void GuiWindow::setTrackMuteState(uint32_t trackIdx, bool mute) {
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].mute = mute;
    }
    if (trackIdx < mixerStrips_.size()) {
        mixerStrips_[trackIdx].mute = mute;
    }
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->getTracks()[trackIdx].mute = mute;
    }
    if (engine_) {
        engine_->setTrackMute(trackIdx, mute);
    }
    recordProjectHistory("Toggle Mute on Track " + std::to_string(trackIdx + 1), "TRACK");
}

void GuiWindow::setTrackSoloState(uint32_t trackIdx, bool solo) {
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].solo = solo;
    }
    if (trackIdx < mixerStrips_.size()) {
        mixerStrips_[trackIdx].solo = solo;
    }
    if (modularArrangerView_ && trackIdx < modularArrangerView_->getTracks().size()) {
        modularArrangerView_->getTracks()[trackIdx].solo = solo;
    }
    if (engine_) {
        engine_->setTrackSolo(trackIdx, solo);
    }
    recordProjectHistory("Toggle Solo on Track " + std::to_string(trackIdx + 1), "TRACK");
}

void GuiWindow::setTrackFreezeState(uint32_t trackIdx, bool freeze) {
    if (trackIdx < arrangerTracks_.size()) {
        arrangerTracks_[trackIdx].freeze = freeze;
    }
    if (trackIdx < mixerStrips_.size()) {
        mixerStrips_[trackIdx].freeze = freeze;
    }
    if (engine_) {
        if (freeze) {
            engine_->freezeTrack(trackIdx);
        } else {
            engine_->unfreezeTrack(trackIdx);
        }
    }
    recordProjectHistory("Toggle Freeze on Track " + std::to_string(trackIdx + 1), "TRACK");
    setStatusMessage(freeze ?
        ("Track " + std::to_string(trackIdx + 1) + ": FROZEN (DSP synth & FX offloaded to PCM buffer)") :
        ("Track " + std::to_string(trackIdx + 1) + ": UNFROZEN (Live real-time synthesis restored)"));
}

void GuiWindow::syncActiveClipToEditView(uint32_t trackIdx, int clipIdx) {
    if (!modularEditView_ || !modularArrangerView_) return;

    auto& tracks = modularArrangerView_->getTracks();
    if (trackIdx >= tracks.size()) return;

    auto& trk = tracks[trackIdx];
    int cIdx = clipIdx;
    if (cIdx < 0 || static_cast<size_t>(cIdx) >= trk.clips.size()) {
        cIdx = modularArrangerView_->getSelectedClipIndex();
        if (cIdx < 0 || static_cast<size_t>(cIdx) >= trk.clips.size()) {
            cIdx = 0;
        }
    }

    if (!trk.clips.empty() && static_cast<size_t>(cIdx) < trk.clips.size()) {
        modularArrangerView_->setSelectedClip(cIdx);
        modularEditView_->loadFromArrangerClip(
            trk.clips[cIdx],
            trk.name,
            Color(trk.r, trk.g, trk.b),
            tracks
        );
    }
}

void GuiWindow::syncArrangerToSequencer() {
    if (!engine_ || !modularArrangerView_) return;

    auto& seq = engine_->getSequencer();
    const auto& tracks = modularArrangerView_->getTracks();
    uint32_t numTracks = static_cast<uint32_t>(tracks.size());

    // Determine arrangement extent in bars / steps
    uint32_t maxBar = 16; // At least 16 bars for standard arrangement
    for (const auto& trk : tracks) {
        for (const auto& clip : trk.clips) {
            uint32_t clipEndBar = clip.startBar + clip.lengthBars - 1;
            if (clipEndBar > maxBar) {
                maxBar = clipEndBar;
            }
        }
    }
    uint32_t totalArrangerSteps = std::min(static_cast<uint32_t>(sequencer::MAX_STEPS_PER_TRACK), maxBar * 16u);
    seq.getTransport().setMaxSteps(totalArrangerSteps);

    for (uint32_t t = 0; t < numTracks; ++t) {
        if (t >= seq.getNumTracks()) break;
        auto* seqTrack = seq.getTrack(t);
        if (!seqTrack) continue;

        seqTrack->setNumSteps(totalArrangerSteps);

        // Clear all steps to inactive
        for (uint32_t s = 0; s < totalArrangerSteps; ++s) {
            sequencer::StepData sd{};
            sd.active = false;
            seqTrack->setStep(s, sd);
        }

        const auto& arrTrack = tracks[t];
        std::map<uint32_t, std::vector<ArrangerClipNote>> stepBuckets;

        for (const auto& clip : arrTrack.clips) {
            if (clip.mute) continue;

            uint32_t clipStartStep = (clip.startBar > 0 ? (clip.startBar - 1) : 0) * 16;
            uint32_t clipLengthSteps = clip.lengthBars * 16;
            uint32_t clipEndStep = clipStartStep + clipLengthSteps;

            uint32_t loopLenBars = clip.isLooped ? (clip.loopLengthBars > 0 ? clip.loopLengthBars : clip.lengthBars) : clip.lengthBars;
            if (loopLenBars == 0) loopLenBars = 1;
            uint32_t loopLenSteps = loopLenBars * 16;

            for (uint32_t cycleStart = clipStartStep; cycleStart < clipEndStep; cycleStart += loopLenSteps) {
                uint32_t cycleEnd = std::min(cycleStart + loopLenSteps, clipEndStep);
                for (const auto& cn : clip.notes) {
                    uint32_t noteStep = cycleStart + static_cast<uint32_t>(std::round(cn.startBeat * 4.0f));
                    if (noteStep >= cycleStart && noteStep < cycleEnd && noteStep < totalArrangerSteps) {
                        stepBuckets[noteStep].push_back(cn);
                    }
                }
            }
        }

        // Apply grouped notes to sequencer track
        for (const auto& [s, noteList] : stepBuckets) {
            if (noteList.empty() || s >= totalArrangerSteps) continue;
            sequencer::StepData sd{};
            sd.active = true;
            sd.note = noteList[0].pitch;
            sd.velocity = std::clamp(noteList[0].velocity, 0.05f, 1.0f);
            sd.gateLength = (noteList[0].lengthBeats <= 0.25f)
                ? std::clamp(noteList[0].lengthBeats * 4.0f, 0.2f, 0.85f)
                : 0.95f;
            for (size_t i = 1; i < noteList.size(); ++i) {
                sd.extraNotes.push_back(noteList[i].pitch);
            }
            seqTrack->setStep(s, sd);
        }
    }
}

void GuiWindow::syncArrangerFromSequencer() {
    if (!engine_ || !modularArrangerView_) return;

    auto& seq = engine_->getSequencer();
    if (seq.getNumPatterns() == 0) {
        initArrangerTracks();
        return;
    }

    const auto& pat = seq.getPattern(0);
    if (pat.tracks.empty()) {
        initArrangerTracks();
        return;
    }

    arrangerTracks_.clear();
    auto& arrangerTracks = modularArrangerView_->getTracks();
    arrangerTracks.clear();

    struct TrackStyleInfo {
        float r, g, b;
        const char* icon;
        const char* instrument;
        const char* engine;
    };
    static const TrackStyleInfo kStyles[] = {
        {1.0f,  0.55f, 0.0f,  "preset:inst_synth",   "Roland TB-303",        "tb303"},       // Amber
        {0.13f, 0.96f, 0.91f, "preset:drum_machine", "Analog 808",           "tr808"},       // Cyan
        {1.0f,  0.16f, 0.43f, "preset:drum_kick",    "Analog 909",           "tr909"},       // Pink/Crimson
        {0.62f, 0.31f, 0.87f, "preset:inst_piano",   "Yamaha DX7 6-Op FM",   "dx7"},         // Purple
        {0.88f, 0.66f, 0.43f, "preset:inst_piano",   "Waveguide Grand Piano","piano"},       // Gold
        {0.20f, 0.85f, 0.35f, "preset:inst_keys",    "Polyphonic Synth",     "poly_synth"},  // Emerald
        {0.25f, 0.65f, 1.0f,  "preset:audio_filter", "Bass Synthesizer",     "synth"},       // Sky Blue
        {1.0f,  0.85f, 0.20f, "preset:fx_reverb",    "EatScript Sound FX",   "eatscript"},   // Yellow
    };
    constexpr size_t numStyles = sizeof(kStyles) / sizeof(kStyles[0]);

    uint32_t maxSteps = 16;

    for (size_t t = 0; t < pat.tracks.size(); ++t) {
        const auto& seqTrk = pat.tracks[t];
        ArrangerTimelineTrack tArr;
        tArr.name = seqTrk.getName().empty() ? ("Track " + std::to_string(t + 1)) : seqTrk.getName();
        tArr.volume = seqTrk.getVolume();
        tArr.pan = seqTrk.getPan();
        tArr.mute = seqTrk.isMuted();
        tArr.solo = seqTrk.isSolo();
        tArr.freeze = seqTrk.isFrozen();

        // Style matching based on name and target node
        std::string lower = tArr.name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        size_t styleIdx = t % numStyles;
        if (lower.find("303") != std::string::npos || lower.find("acid") != std::string::npos || lower.find("bass") != std::string::npos) {
            styleIdx = 0;
        } else if (lower.find("808") != std::string::npos || lower.find("kick") != std::string::npos || lower.find("bd") != std::string::npos) {
            styleIdx = 1;
        } else if (lower.find("909") != std::string::npos || lower.find("drum") != std::string::npos || lower.find("hat") != std::string::npos || lower.find("snare") != std::string::npos || lower.find("clap") != std::string::npos) {
            styleIdx = 2;
        } else if (lower.find("dx7") != std::string::npos || lower.find("rhodes") != std::string::npos || lower.find("ep") != std::string::npos || lower.find("keys") != std::string::npos) {
            styleIdx = 3;
        } else if (lower.find("piano") != std::string::npos || lower.find("grand") != std::string::npos || lower.find("guitar") != std::string::npos || lower.find("string") != std::string::npos) {
            styleIdx = 4;
        }

        const auto& st = kStyles[styleIdx];
        tArr.r = st.r;
        tArr.g = st.g;
        tArr.b = st.b;
        tArr.iconRef = seqTrk.getIconRef().empty() ? st.icon : seqTrk.getIconRef();
        tArr.instrument = st.instrument;
        tArr.instrumentEngine = st.engine;

        // Create timeline clip from steps
        uint32_t trkSteps = std::max(16u, seqTrk.getNumSteps());
        if (trkSteps > maxSteps) maxSteps = trkSteps;
        uint32_t lengthBars = std::max(1u, (trkSteps + 15) / 16);

        ArrangerTimelineClip clip;
        clip.id = "clip_" + std::to_string(t) + "_0";
        clip.name = tArr.name + " Pattern";
        clip.trackIndex = static_cast<uint32_t>(t);
        clip.startBar = 1;
        clip.lengthBars = lengthBars;
        clip.r = tArr.r;
        clip.g = tArr.g;
        clip.b = tArr.b;
        clip.isSelected = (t == 0);
        clip.isAudio = false;
        clip.isLooped = false;
        clip.loopLengthBars = lengthBars;
        clip.volumeScale = 1.0f;
        clip.mute = tArr.mute;

        for (uint32_t s = 0; s < seqTrk.getNumSteps(); ++s) {
            const auto& step = seqTrk.getStep(s);
            if (!step.active) continue;

            ArrangerClipNote note;
            note.pitch = step.note;
            note.startBeat = static_cast<float>(s) * 0.25f; // 16 steps = 4 beats = 1 bar
            note.lengthBeats = std::clamp(step.gateLength * 0.25f, 0.05f, 1.0f);
            note.velocity = std::clamp(step.velocity, 0.05f, 1.0f);
            clip.notes.push_back(note);

            for (uint8_t ex : step.extraNotes) {
                ArrangerClipNote exNote = note;
                exNote.pitch = ex;
                clip.notes.push_back(exNote);
            }
        }

        modularArrangerView_->updateClipDetectedChords(clip);
        tArr.clips.push_back(clip);

        // Populate legacy ArrangerTrackData
        ArrangerTrackData legTrk;
        legTrk.name = tArr.name;
        legTrk.type = (tArr.instrumentEngine == "tr808" || tArr.instrumentEngine == "tr909") ? "SAMPLER" : "SYNTH";
        legTrk.r = tArr.r; legTrk.g = tArr.g; legTrk.b = tArr.b;
        legTrk.volume = tArr.volume;
        legTrk.pan = tArr.pan;
        legTrk.mute = tArr.mute;
        legTrk.solo = tArr.solo;
        legTrk.freeze = tArr.freeze;

        ArrangerClip legClip;
        legClip.name = clip.name;
        legClip.startBar = clip.startBar;
        legClip.barLength = clip.lengthBars;
        legClip.r = clip.r; legClip.g = clip.g; legClip.b = clip.b;
        legClip.isLooped = clip.isLooped;
        legClip.loopLengthBars = clip.loopLengthBars;
        legTrk.clips.push_back(legClip);
        arrangerTracks_.push_back(legTrk);

        arrangerTracks.push_back(tArr);
    }

    selectedTrackIndex_ = 0;
    modularArrangerView_->setActiveTrack(0);
    modularArrangerView_->setSelectedClip(0);
    syncActiveClipToEditView(0, 0);

    seq.getTransport().setMaxSteps(std::max(16u, maxSteps));
    seq.getTransport().setPosition(0);

    mixerStrips_.clear();
    updateMixerStrips();
}

void GuiWindow::syncTrackToPreset(uint32_t trackIndex) {
    selectedTrackIndex_ = trackIndex;
    if (presets_.empty()) return;

    std::string targetKeyword;
    if (engine_ && trackIndex < engine_->getSequencer().getNumTracks()) {
        auto* trk = engine_->getSequencer().getTrack(trackIndex);
        if (trk) {
            const std::string& name = trk->getName();
            if (name.find("303") != std::string::npos || name.find("Acid") != std::string::npos) targetKeyword = "303";
            else if (name.find("808") != std::string::npos || name.find("Drum") != std::string::npos) targetKeyword = "808";
            else if (name.find("Bass") != std::string::npos || name.find("Sub") != std::string::npos) targetKeyword = "bass";
            else if (name.find("Lead") != std::string::npos || name.find("Poly") != std::string::npos || name.find("DX7") != std::string::npos) targetKeyword = "dx7";
            else if (name.find("Master") != std::string::npos || name.find("Delay") != std::string::npos || name.find("Echo") != std::string::npos) targetKeyword = "delay";

            if (targetKeyword.empty()) {
                audio::NodeId nId = trk->getTargetNodeId();
                auto nodePtr = engine_->getGraph().getNode(nId);
                if (nodePtr) {
                    if (dynamic_cast<audio::Tb303Node*>(nodePtr.get())) targetKeyword = "303";
                    else if (dynamic_cast<audio::DrumKitNode*>(nodePtr.get())) targetKeyword = "808";
                    else if (dynamic_cast<audio::DelayNode*>(nodePtr.get())) targetKeyword = "delay";
                    else if (dynamic_cast<audio::PolySynthNode*>(nodePtr.get())) targetKeyword = "dx7";
                }
            }
        }
    }
    if (targetKeyword.empty() && trackIndex < arrangerTracks_.size()) {
        const std::string& name = arrangerTracks_[trackIndex].name;
        if (name.find("303") != std::string::npos || name.find("Acid") != std::string::npos) targetKeyword = "303";
        else if (name.find("808") != std::string::npos || name.find("Drum") != std::string::npos) targetKeyword = "808";
        else if (name.find("Bass") != std::string::npos || name.find("Sub") != std::string::npos) targetKeyword = "bass";
        else if (name.find("Lead") != std::string::npos || name.find("Poly") != std::string::npos) targetKeyword = "dx7";
        else if (name.find("Master") != std::string::npos || name.find("Delay") != std::string::npos) targetKeyword = "delay";
    }
    if (targetKeyword.empty()) {
        if (trackIndex == 0) targetKeyword = "303";
        else if (trackIndex == 1) targetKeyword = "808";
        else if (trackIndex == 2) targetKeyword = "bass";
        else if (trackIndex == 3) targetKeyword = "dx7";
        else if (trackIndex == 4) targetKeyword = "delay";
        else targetKeyword = "303";
    }

    for (size_t i = 0; i < presets_.size(); ++i) {
        const auto& meta = presets_[i].metadata;
        if (meta.id.find(targetKeyword) != std::string::npos ||
            meta.engineId.find(targetKeyword) != std::string::npos ||
            meta.name.find(targetKeyword) != std::string::npos) {
            activePresetIndex_ = i;
            presets_[activePresetIndex_].compactGuiRoot = presets_[activePresetIndex_].guiRoot;
            project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].compactGuiRoot, 0.0f, 0.0f, 520.0f, 210.0f);
            project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                                       40.0f, 98.0f,
                                                       static_cast<float>(width_) - 80.0f,
                                                       static_cast<float>(height_) - 154.0f);
            break;
        }
    }

    if (engine_ && activePresetIndex_ < presets_.size()) {
        auto* trk = engine_->getSequencer().getTrack(trackIndex);
        if (trk && !presets_[activePresetIndex_].rawScript.empty()) {
            trk->setEatscriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
    if (designSubView_ == DesignSubView::Eatscript && activePresetIndex_ < presets_.size() && !presets_[activePresetIndex_].rawScript.empty()) {
        setScriptCode(presets_[activePresetIndex_].rawScript);
    }
}

void GuiWindow::setMasterVolume(float vol) noexcept {
    masterVolume_ = std::clamp(vol, 0.0f, 1.5f);
    if (engine_) {
        engine_->setMasterVolume(masterVolume_);
    }
}

const project::PresetDefinition* GuiWindow::getActivePreset() const noexcept {
    if (presets_.empty() || activePresetIndex_ >= presets_.size()) return nullptr;
    return &presets_[activePresetIndex_];
}

project::PresetDefinition* GuiWindow::getActivePreset() noexcept {
    if (presets_.empty() || activePresetIndex_ >= presets_.size()) return nullptr;
    return &presets_[activePresetIndex_];
}

HitTestHardwareKnobResult GuiWindow::hitTestHardwareKnob(float x, float y, float faceplateX, float faceplateY, float scale) const noexcept {
    HitTestHardwareKnobResult res{};
    const auto* preset = getActivePreset();
    if (!preset) return res;
    hitTestNodeRecursive(preset->guiRoot, x, y, *preset, res, faceplateX, faceplateY, scale);
    return res;
}

void GuiWindow::dispatchHardwareParam(const std::string& paramName, float normVal) noexcept {
    auto* preset = getActivePreset();
    if (!preset) return;

    std::string key = paramName;
    auto* p = preset->findParam(key);
    if (!p) {
        // Try case-insensitive and common alias matching
        std::string lowerParam = paramName;
        for (char& c : lowerParam) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lowerParam == "tuning") lowerParam = "tune";
        else if (lowerParam == "bright") lowerParam = "brightness";
        else if (lowerParam == "snappy" || lowerParam == "punch") lowerParam = "drive";

        for (auto& [k, v] : preset->params) {
            std::string lk = k;
            for (char& c : lk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lk == lowerParam) {
                p = &v;
                key = k;
                break;
            }
        }
    }

    if (p) {
        p->setNormalized(normVal);
    }

    if (!engine_) return;

    std::string normKey = key;
    for (char& c : normKey) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (preset->metadata.id == "eats_303" || preset->metadata.engineId == "tb303") {
        if (normKey == "cutoff" && p) {
            engine_->setCutoff(p->currentVal);
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 0, p->currentVal);
            }
        } else if (normKey == "resonance" && p) {
            float resNorm = std::clamp((p->currentVal - p->minVal) / (p->maxVal - p->minVal), 0.0f, 0.98f);
            engine_->setResonance(resNorm);
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 1, resNorm);
            }
        } else if (normKey == "envmod" && p) {
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 2, normVal);
            }
        } else if (normKey == "decay" && p) {
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 3, normVal);
            }
        } else if (normKey == "accent" && p) {
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 4, normVal);
            }
        } else if (normKey == "waveform" && p) {
            float wave = (normVal >= 0.5f) ? 1.0f : 0.0f;
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 5, wave);
            }
        } else if ((normKey == "drive" || normKey == "overdrive") && p) {
            for (const auto& m : canvas_.getModules()) {
                if (m.type == "tb303") engine_->postNodeParameter(m.id, 6, normVal);
            }
        }
    } else if (preset->metadata.engineId == "drum_808" || preset->metadata.id == "analog_808_kick" || preset->metadata.engineId == "drum_kit") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "drum_kit" || m.name.find("Drum") != std::string::npos || m.name.find("808") != std::string::npos) {
                if (normKey == "tune" || normKey == "pitch" || normKey == "tuning") {
                    engine_->postNodeParameter(m.id, 1, normVal);
                } else if (normKey == "decay") {
                    engine_->postNodeParameter(m.id, 2, normVal);
                } else if (normKey == "overdrive" || normKey == "drive" || normKey == "snappy" || normKey == "punch") {
                    engine_->postNodeParameter(m.id, 3, normVal);
                } else if (normKey == "tone" || normKey == "volume" || normKey == "level" || normKey == "attack") {
                    engine_->postNodeParameter(m.id, 0, normVal * 1.5f);
                }
            }
        }
    } else if (preset->metadata.engineId == "delay") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "delay") {
                if (paramName == "TimeMs" && p) {
                    engine_->postNodeParameter(m.id, 0, p->currentVal);
                } else if (paramName == "Feedback" && p) {
                    engine_->postNodeParameter(m.id, 1, p->currentVal);
                } else if (paramName == "Mix" && p) {
                    engine_->postNodeParameter(m.id, 2, p->currentVal);
                }
            }
        }
    } else if (preset->metadata.engineId == "sid" || preset->metadata.id == "c64_sid_synth") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "sid" || m.name.find("SID") != std::string::npos || m.name.find("Sid") != std::string::npos) {
                uint32_t paramId = 999;
                if (paramName == "Waveform") paramId = 0;
                else if (paramName == "PulseWidth") paramId = 1;
                else if (paramName == "PwmRate") paramId = 2;
                else if (paramName == "PwmDepth") paramId = 3;
                else if (paramName == "ArpMode") paramId = 4;
                else if (paramName == "GlideSpeed") paramId = 5;
                else if (paramName == "ChipModel") paramId = 6;
                else if (paramName == "FilterMode") paramId = 7;
                else if (paramName == "Cutoff") paramId = 8;
                else if (paramName == "Resonance") paramId = 9;
                else if (paramName == "Overdrive") paramId = 10;
                else if (paramName == "Attack") paramId = 11;
                else if (paramName == "Decay") paramId = 12;
                else if (paramName == "Sustain") paramId = 13;
                else if (paramName == "Release") paramId = 14;

                if (paramId != 999 && p) {
                    engine_->postNodeParameter(m.id, paramId, p->currentVal);
                }
            }
        }
    } else if (preset->metadata.engineId == "dx7" || preset->metadata.id == "dx7_epiano") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "dx7" || m.name.find("DX7") != std::string::npos || m.name.find("Dx7") != std::string::npos) {
                uint32_t paramId = 999;
                if (paramName == "Algorithm") paramId = 0;
                else if (paramName == "Feedback") paramId = 1;
                else if (paramName == "Volume" || paramName == "MasterVolume") paramId = 2;
                else if (paramName == "Brightness") paramId = 3;
                else if (paramName == "TineBell") paramId = 4;
                else if (paramName == "BodyWarmth") paramId = 5;

                if (paramId != 999 && p) {
                    engine_->postNodeParameter(m.id, paramId, p->currentVal);
                }
            }
        }
    } else if (preset->metadata.engineId == "snes" || preset->metadata.id == "snes_dsp") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "snes" || m.name.find("SNES") != std::string::npos || m.name.find("Snes") != std::string::npos) {
                uint32_t paramId = 999;
                if (paramName == "Waveform") paramId = 0;
                else if (paramName == "Attack") paramId = 2;
                else if (paramName == "Decay") paramId = 3;
                else if (paramName == "Sustain") paramId = 4;
                else if (paramName == "Release") paramId = 5;
                else if (paramName == "EchoDelay") paramId = 7;
                else if (paramName == "EchoFeedback") paramId = 8;
                else if (paramName == "EchoVolume") paramId = 9;
                else if (paramName == "Volume") paramId = 14;

                if (paramId != 999 && p) {
                    engine_->postNodeParameter(m.id, paramId, p->currentVal);
                }
            }
        }
    } else if (preset->metadata.engineId == "ym2612" || preset->metadata.id == "genesis_ym2612") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "ym2612" || m.name.find("YM2612") != std::string::npos || m.name.find("Ym2612") != std::string::npos || m.name.find("Genesis") != std::string::npos) {
                uint32_t paramId = 999;
                if (paramName == "Algorithm") paramId = 0;
                else if (paramName == "Feedback") paramId = 1;
                else if (paramName == "Volume") paramId = 2;
                else if (paramName == "Op1TL") paramId = 4;
                else if (paramName == "Op2TL") paramId = 6;
                else if (paramName == "Op3TL") paramId = 8;
                else if (paramName == "Op4TL") paramId = 10;
                else if (paramName == "Attack") paramId = 11;
                else if (paramName == "Decay") paramId = 12;
                else if (paramName == "Sustain") paramId = 13;
                else if (paramName == "Release") paramId = 14;

                if (paramId != 999 && p) {
                    engine_->postNodeParameter(m.id, paramId, p->currentVal);
                }
            }
        }
    } else if (preset->metadata.engineId == "convolver" || preset->metadata.id == "convolver_space") {
        for (const auto& m : canvas_.getModules()) {
            if (m.type == "convolver" || m.name.find("Convolver") != std::string::npos || m.name.find("Space") != std::string::npos) {
                uint32_t paramId = 999;
                if (paramName == "Mix") paramId = 0;
                else if (paramName == "Preset") paramId = 1;
                else if (paramName == "PreDelay") paramId = 2;
                else if (paramName == "Decay") paramId = 3;
                else if (paramName == "HighCut") paramId = 4;
                else if (paramName == "LowCut") paramId = 5;

                if (paramId != 999 && p) {
                    engine_->postNodeParameter(m.id, paramId, p->currentVal);
                }
            }
        }
    }
}

bool GuiWindow::initialize(audio::AudioEngine& engine) {
    engine_ = &engine;
    engine_->getSequencer().getTransport().setMaxSteps(18 * 16);
    if (engine_->getSequencer().getNumTracks() == 0) {
        engine_->getSequencer().addTrack("01 TB-303 Acid", 1, 16);
        engine_->getSequencer().addTrack("02 TR-808 Drums", 2, 16);
        engine_->getSequencer().addTrack("03 Sub Bass", 3, 16);
        engine_->getSequencer().addTrack("04 Poly Lead", 4, 16);
        engine_->getSequencer().addTrack("05 Waveguide Piano", 5, 16);
    }
    for (uint32_t t = 0; t < engine_->getSequencer().getNumTracks(); ++t) {
        syncTrackToPreset(t);
    }
    syncTrackToPreset(0);
    canvas_.setCanvasSize(static_cast<float>(width_), static_cast<float>(height_));
    canvas_.updateRackLayout(engine_->getGraph());
    initDefaultKnobValues();
    initHistory();
    updateMixerStrips();
    while (mixerStrips_.size() < 5) {
        MixerStrip strip;
        size_t idx = mixerStrips_.size();
        strip.name = (idx == 0) ? "303 Acid Bass" : ((idx == 1) ? "TR-808 Kit" : ((idx == 2) ? "TR-909 Drive" : ((idx == 3) ? "DX7 Rhodes" : "Concert Grand")));
        strip.nodeId = static_cast<audio::NodeId>(idx + 1);
        strip.volume = 0.8f;
        mixerStrips_.push_back(strip);
    }
    dawnBridge_.initialize(width_, height_);

    // Instantiate Modular Subsystem Architecture (Option B)
    modularArrangerView_ = std::make_unique<ArrangerView>();
    modularEditView_ = std::make_unique<EditView>();
    modularTrackInspectorView_ = std::make_unique<TrackInspectorView>();
    modularMixerView_ = std::make_unique<MixerView>();
    modularDesignView_ = std::make_unique<DesignView>();
    virtualKeyboardDrawerWidget_ = std::make_unique<VirtualKeyboardDrawer>();
    projectBrowserDrawerWidget_ = std::make_unique<ProjectBrowserDrawer>();
    transportHeaderWidget_ = std::make_unique<TransportHeader>();
    bottomNavBarWidget_ = std::make_unique<BottomNavBar>();

    bottomNavBarWidget_->onTabSelected = [this](int tabIdx) {
        if (tabIdx >= 0 && tabIdx <= 4) {
            activeView_ = static_cast<WorkspaceView>(tabIdx);
        }
    };
    transportHeaderWidget_->onToggleProjectHub = [this]() { toggleProjectHub(); };
    transportHeaderWidget_->onToggleBrowser = [this]() { toggleBrowser(); };
    transportHeaderWidget_->onToggleLoop = [this]() { toggleLoop(); };
    transportHeaderWidget_->onToggleMetronome = [this]() { toggleMetronome(); };
    transportHeaderWidget_->onOpenValueEdit = [this](const auto& req) { openValueEditDialog(req); };
    transportHeaderWidget_->onTogglePlay = [this]() {
        if (engine_) {
            if (engine_->getSequencer().isPlaying()) engine_->getSequencer().stop();
            else engine_->getSequencer().start();
        }
    };
    projectBrowserDrawerWidget_->onClose = [this]() { setBrowserOpen(false); };
    projectBrowserDrawerWidget_->onSelectTrack = [this](uint32_t trackIndex) {
        if (trackIndex < arrangerTracks_.size()) {
            selectedTrackIndex_ = trackIndex;
            if (modularArrangerView_) modularArrangerView_->setActiveTrack(trackIndex);
            if (modularMixerView_) modularMixerView_->setSelectedChannel(trackIndex);
            if (engine_) engine_->setActiveTrack(trackIndex);
        }
    };
    projectBrowserDrawerWidget_->onSelectPreset = [this](const std::string& presetId) {
        for (size_t i = 0; i < presets_.size(); ++i) {
            if (presets_[i].metadata.id == presetId || presets_[i].metadata.name.find(presetId) != std::string::npos) {
                loadPresetToSelectedTrack(i);
                return;
            }
        }
        if (!presets_.empty()) loadPresetToSelectedTrack(0);
    };
    projectBrowserDrawerWidget_->onRunMacro = [this](const std::string& macroId) {
        if (macroId == "macro_song") {
            runMacro(4);
        } else if (macroId == "macro_909") {
            runMacro(1);
        } else if (macroId == "macro_humanize") {
            runMacro(2);
        } else {
            runMacro(0);
        }
    };
    projectBrowserDrawerWidget_->onRunScript = [this](const std::string& scriptId) {
        runMacro(0);
    };
    projectBrowserDrawerWidget_->onAddPresetTrack = [this](const std::string& presetId) {
        lastStatusMessage_ = "ADDED TRACK: " + presetId;
    };
    projectBrowserDrawerWidget_->onLoadProject = [this](const std::string& filePath) {
        loadProjectFromFile(filePath);
    };
    projectBrowserDrawerWidget_->onSaveProject = [this](const std::string& filePath) {
        saveProjectToFile(filePath);
        if (projectBrowserDrawerWidget_) projectBrowserDrawerWidget_->scanSavedProjects();
    };
    projectBrowserDrawerWidget_->onSaveProjectAs = [this]() {
        saveProjectAs();
        if (projectBrowserDrawerWidget_) projectBrowserDrawerWidget_->scanSavedProjects();
    };
    projectBrowserDrawerWidget_->onOpenProjectsFolder = [this]() {
#if defined(_WIN32)
        system("start Projects");
#endif
    };
    projectBrowserDrawerWidget_->onDeleteProject = [this](const std::string& filePath) {
        std::error_code ec;
        std::filesystem::remove(filePath, ec);
        if (projectBrowserDrawerWidget_) projectBrowserDrawerWidget_->scanSavedProjects();
    };
    projectBrowserDrawerWidget_->onUndo = [this]() { undoHistory(); };
    projectBrowserDrawerWidget_->onRedo = [this]() { redoHistory(); };
    projectBrowserDrawerWidget_->onJumpToHistory = [this](size_t historyIndex) { jumpToHistoryIndex(historyIndex); };
    projectBrowserDrawerWidget_->onCreateCheckpoint = [this](const std::string& name) {
        createHistoryMilestone(name + " " + std::to_string(diffHistory_.getTimelineCount()));
    };
    projectBrowserDrawerWidget_->onClearHistory = [this]() { clearHistory(); };
    projectBrowserDrawerWidget_->onLaunchAudioToMidi = [this]() {
        openAudioToMidiConverter();
    };

    // Configure Audio-to-MIDI Transcription Modal Dialog
    audioToMidiDialog_.onBrowseAudioFile = [this]() {
        return promptOpenAudioFile();
    };

    audioToMidiDialog_.onTranscriptionComplete = [this](const audio::TranscribedMidiTrack& track,
                                                        bool createNewTrack,
                                                        const std::string& trackName,
                                                        bool extractChords) {
        if (track.notes.empty()) {
            lastStatusMessage_ = "Audio to MIDI: No notes detected in audio file";
            return;
        }

        uint32_t targetTrackIdx = selectedTrackIndex_;
        if (createNewTrack && modularArrangerView_) {
            ArrangerTimelineTrack newTrack;
            newTrack.name = trackName.empty() ? "Transcribed MIDI" : trackName;
            newTrack.instrument = "Polyphonic Synth";
            newTrack.instrumentEngine = "poly_synth";
            newTrack.iconRef = "preset:inst_keys";
            newTrack.r = 0.0f; newTrack.g = 0.85f; newTrack.b = 1.0f; // Cyan
            modularArrangerView_->getTracks().push_back(newTrack);
            targetTrackIdx = static_cast<uint32_t>(modularArrangerView_->getTracks().size() - 1);
            setSelectedTrackIndex(targetTrackIdx);
        }

        if (modularArrangerView_ && targetTrackIdx < modularArrangerView_->getTracks().size()) {
            auto& t = modularArrangerView_->getTracks()[targetTrackIdx];
            ArrangerTimelineClip clip;
            clip.id = "transcribed_clip_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            clip.name = trackName.empty() ? "Transcribed Clip" : trackName;
            clip.trackIndex = targetTrackIdx;
            clip.startBar = 1;
            float maxStep = 16.0f;
            for (const auto& tn : track.notes) {
                ArrangerClipNote cn;
                cn.pitch = tn.pitch;
                cn.startBeat = tn.getStartBeat();
                cn.lengthBeats = std::max(0.25f, tn.getDurationBeats());
                cn.velocity = tn.velocity;
                clip.notes.push_back(cn);
                maxStep = std::max(maxStep, tn.startStep + tn.durationSteps);
            }
            clip.lengthBars = std::max(1u, static_cast<uint32_t>(std::ceil(maxStep / 16.0f)));
            clip.r = 0.0f; clip.g = 0.85f; clip.b = 1.0f;
            if (extractChords && !track.detectedChords.empty()) {
                clip.detectedChords = track.detectedChords;
            }
            t.clips.push_back(clip);
        }

        if (engine_ && targetTrackIdx < engine_->getSequencer().getNumTracks()) {
            auto* seqTrack = engine_->getSequencer().getTrack(targetTrackIdx);
            if (seqTrack) {
                for (const auto& tn : track.notes) {
                    uint32_t sIdx = static_cast<uint32_t>(std::round(tn.startStep));
                    if (sIdx < seqTrack->getNumSteps()) {
                        auto& step = seqTrack->getStep(sIdx);
                        step.active = true;
                        step.note = tn.pitch;
                        step.velocity = tn.velocity;
                        step.gateLength = std::clamp(tn.durationSteps / 2.0f, 0.2f, 1.0f);
                    }
                }
            }
        }

        lastStatusMessage_ = "Transcribed " + std::to_string(track.notes.size()) +
                             " MIDI notes into " + trackName +
                             (extractChords && !track.detectedChords.empty()
                                 ? " (" + std::to_string(track.detectedChords.size()) + " chords extracted)"
                                 : "");
    };

    audioToMidiDialog_.onAuditionStart = [this](const std::vector<float>& /*samples*/, uint32_t /*sampleRate*/) {
        if (engine_) engine_->postNoteOn(60, 0.85f);
    };

    audioToMidiDialog_.onAuditionStop = [this]() {
        if (engine_) engine_->postNoteOff(60);
    };

    // Register Default Universal Quick Commands into CommandPaletteDialog
    commandPaletteDialog_.clearCommands();
    commandPaletteDialog_.registerCommand({
        "view.arranger", "Arranger View", "Full multi-track timeline, clips and automation",
        CommandCategory::View, "1", [this]() { setActiveView(WorkspaceView::Arranger); }
    });
    commandPaletteDialog_.registerCommand({
        "view.edit", "Edit View (Piano Roll / Tracker)", "Note editing, step sequencer and chords",
        CommandCategory::View, "2", [this]() { setActiveView(WorkspaceView::Edit); }
    });
    commandPaletteDialog_.registerCommand({
        "view.inspector", "Track Inspector", "Channel parameters, inserts and routing",
        CommandCategory::View, "3", [this]() { setActiveView(WorkspaceView::Track); }
    });
    commandPaletteDialog_.registerCommand({
        "view.mixer", "Modular Mixer", "High-density 8-channel console with VU ballistics",
        CommandCategory::View, "4", [this]() { setActiveView(WorkspaceView::Mixer); }
    });
    commandPaletteDialog_.registerCommand({
        "view.design", "Modular Rack & Design", "Synthesizer node graph and modular routing",
        CommandCategory::View, "5", [this]() { setActiveView(WorkspaceView::Design); }
    });
    commandPaletteDialog_.registerCommand({
        "view.drawer.keyboard", "Toggle Piano / Drum Pad Drawer", "Slide-up interactive audition drawer",
        CommandCategory::View, "K", [this]() { toggleVirtualKeyboardDrawer(); }
    });
    commandPaletteDialog_.registerCommand({
        "view.drawer.browser", "Toggle Project Browser Drawer", "Projects, presets, macros, and history timeline",
        CommandCategory::View, "B", [this]() { toggleBrowser(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.play_pause", "Play / Pause Transport", "Toggle global transport playback",
        CommandCategory::Action, "Space", [this]() {
            if (engine_) {
                if (engine_->getSequencer().isPlaying()) engine_->getSequencer().stop();
                else engine_->getSequencer().start();
            }
        }
    });
    commandPaletteDialog_.registerCommand({
        "action.stop_panic", "Panic / All Notes Off", "Halt transport and kill all sounding voices",
        CommandCategory::Action, "Esc", [this]() {
            if (engine_) engine_->panic();
        }
    });
    commandPaletteDialog_.registerCommand({
        "action.loop", "Toggle Loop Cycle", "Enable or disable global arrangement cycle loop",
        CommandCategory::Action, "L", [this]() { toggleLoop(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.metronome", "Toggle Metronome", "Audible click track on beats",
        CommandCategory::Action, "M", [this]() { toggleMetronome(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.undo", "Undo Last Action", "Revert last project or parameter state",
        CommandCategory::Action, "Ctrl+Z", [this]() { undoHistory(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.redo", "Redo Reverted Action", "Re-apply reverted state modification",
        CommandCategory::Action, "Ctrl+Y", [this]() { redoHistory(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.save", "Save Project", "Write project to current active file",
        CommandCategory::Action, "Ctrl+S", [this]() { saveProjectToFile(projectFilePath_); }
    });
    commandPaletteDialog_.registerCommand({
        "action.save_as", "Save Project As...", "Export project with new name or path",
        CommandCategory::Action, "Ctrl+Shift+S", [this]() { saveProjectAs(); }
    });
    commandPaletteDialog_.registerCommand({
        "action.bounce_wav", "Bounce Master to WAV", "Render arrangement master audio to WAV file",
        CommandCategory::Action, "Ctrl+E", [this]() { bounceMasterToWav(projectName_ + ".wav"); }
    });
    commandPaletteDialog_.registerCommand({
        "action.audio_to_midi", "Audio to MIDI Converter", "Transcribe audio recording or sample into MIDI clip",
        CommandCategory::Action, "Ctrl+M", [this]() { openAudioToMidiConverter(); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.303_acid", "TB-303 Acid Bass", "Resonant acid squelch synth preset",
        CommandCategory::Preset, "", [this]() { loadPresetToSelectedTrack(0); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.808_kit", "TR-808 Rhythm Kit", "Classic analog drum machine kit",
        CommandCategory::Preset, "", [this]() { if (presets_.size() > 1) loadPresetToSelectedTrack(1); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.909_drive", "TR-909 Punch Kit", "Club kick and snappy snares",
        CommandCategory::Preset, "", [this]() { if (presets_.size() > 2) loadPresetToSelectedTrack(2); }
    });
    commandPaletteDialog_.registerCommand({
        "preset.dx7_rhodes", "DX7 Electric Piano", "Lush FM synth tine keys and bells",
        CommandCategory::Preset, "", [this]() { if (presets_.size() > 3) loadPresetToSelectedTrack(3); }
    });
    commandPaletteDialog_.registerCommand({
        "theme.neon", "Theme: Cyberpunk Neon", "Electric purple, hot pink, and cyan glow",
        CommandCategory::Theme, "", [this]() { setActiveThemePreset(0); }
    });
    commandPaletteDialog_.registerCommand({
        "theme.midnight", "Theme: Midnight Blue", "Deep midnight blue with vibrant accents",
        CommandCategory::Theme, "", [this]() { setActiveThemePreset(1); }
    });
    commandPaletteDialog_.registerCommand({
        "theme.charcoal", "Theme: Charcoal Studio", "Minimalist sleek dark mode professional studio",
        CommandCategory::Theme, "", [this]() { setActiveThemePreset(2); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.procedural_acid", "Macro: Procedural Acid Bassline", "Generate algorithmic 16-step 303 pattern",
        CommandCategory::Macro, "", [this]() { runMacro(0); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.techno_groove", "Macro: 909 Techno Groove", "Populate kick, hi-hat offbeat, and claps",
        CommandCategory::Macro, "", [this]() { runMacro(1); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.humanize", "Macro: Humanize Velocities & Timing", "Apply subtle organic jitter to selected notes",
        CommandCategory::Macro, "", [this]() { runMacro(2); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.arp_track", "Macro: Arpeggiate Active Track", "Transform active track notes into 16th-note arpeggio",
        CommandCategory::Macro, "", [this]() { runMacro(3); }
    });
    commandPaletteDialog_.registerCommand({
        "macro.procedural_song", "Macro: Procedural Song Architect", "Generate complete multi-track song arrangement",
        CommandCategory::Macro, "Ctrl+Shift+G", [this]() { runMacro(4); }
    });

    if (modularArrangerView_) {
        modularArrangerView_->onVolumeChanged = [this](uint32_t idx, float vol) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].volume = vol;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].volume = vol;
            }
            if (engine_) {
                engine_->setTrackVolume(idx, vol);
            }
        };
        modularArrangerView_->onPanChanged = [this](uint32_t idx, float pan) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].pan = pan;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].pan = pan;
            }
            if (engine_) {
                engine_->setTrackPan(idx, pan);
            }
        };
        modularArrangerView_->onMuteToggled = [this](uint32_t idx, bool mute) {
            setTrackMuteState(idx, mute);
        };
        modularArrangerView_->onSoloToggled = [this](uint32_t idx, bool solo) {
            setTrackSoloState(idx, solo);
        };
        modularArrangerView_->onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
            (void)idx;
            dispatchHardwareParam(paramName, normVal);
        };
        modularArrangerView_->onClipsChanged = [this]() {
            syncArrangerToSequencer();
        };
        modularArrangerView_->onTrackSelected = [this](uint32_t idx) {
            setSelectedTrackIndex(idx);
        };
        modularArrangerView_->onTrackRename = [this](uint32_t idx, const std::string& newName) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].name = newName;
            }
            if (idx < mixerStrips_.size()) {
                mixerStrips_[idx].name = newName;
            }
            setStatusMessage("Track renamed: " + newName);
        };
        modularArrangerView_->onTrackIconChanged = [this](uint32_t idx, const std::string& iconRef) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].iconRef = iconRef;
            }
            if (idx < mixerStrips_.size()) {
                mixerStrips_[idx].iconRef = iconRef;
            }
            if (valueEditDialog_.isOpen()) {
                valueEditDialog_.setIconRef(iconRef);
            }
            setStatusMessage("Track icon updated");
        };
        modularArrangerView_->getIconSearchDialog().setClipboardProvider([this]() -> std::string {
#if EATS_HAS_GLFW
            if (window_) {
                const char* str = glfwGetClipboardString(window_);
                return str ? std::string(str) : std::string();
            }
#endif
            return {};
        });
    }

    valueEditDialog_.onCopyToClipboard = [this](const std::string& text) {
#if EATS_HAS_GLFW
        if (window_) {
            glfwSetClipboardString(window_, text.c_str());
        }
#endif
    };
    valueEditDialog_.onPasteFromClipboard = [this]() -> std::string {
#if EATS_HAS_GLFW
        if (window_) {
            const char* str = glfwGetClipboardString(window_);
            return str ? std::string(str) : std::string();
        }
#endif
        return {};
    };

    if (modularTrackInspectorView_) {
        modularTrackInspectorView_->onTrackSelected = [this](uint32_t idx) {
            setSelectedTrackIndex(idx);
        };
        modularTrackInspectorView_->onVolumeChanged = [this](uint32_t idx, float vol) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].volume = vol;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].volume = vol;
            }
            if (engine_) {
                engine_->setTrackVolume(idx, vol);
            }
        };
        modularTrackInspectorView_->onPanChanged = [this](uint32_t idx, float pan) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].pan = pan;
                if (idx < mixerStrips_.size()) mixerStrips_[idx].pan = pan;
            }
            if (engine_) {
                engine_->setTrackPan(idx, pan);
            }
        };
        modularTrackInspectorView_->onMuteToggled = [this](uint32_t idx, bool mute) {
            setTrackMuteState(idx, mute);
        };
        modularTrackInspectorView_->onSoloToggled = [this](uint32_t idx, bool solo) {
            setTrackSoloState(idx, solo);
        };
        modularTrackInspectorView_->onFreezeToggled = [this](uint32_t idx, bool freeze) {
            setTrackFreezeState(idx, freeze);
        };
        modularTrackInspectorView_->onColorChanged = [this](uint32_t idx, float r, float g, float b) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].r = r;
                arrangerTracks_[idx].g = g;
                arrangerTracks_[idx].b = b;
                for (auto& clp : arrangerTracks_[idx].clips) {
                    clp.r = r; clp.g = g; clp.b = b;
                }
                recordProjectHistory("Change Accent Color on Track " + std::to_string(idx + 1), "TRACK");
            }
        };
        modularTrackInspectorView_->onOpenCodeEditor = [this](uint32_t idx) {
            (void)idx;
            setActiveView(WorkspaceView::Design);
            setDesignSubView(DesignSubView::Eatscript);
        };
        modularTrackInspectorView_->onPrevPreset = [this]() {
            prevPreset();
        };
        modularTrackInspectorView_->onNextPreset = [this]() {
            nextPreset();
        };
        modularTrackInspectorView_->onChordFollowChanged = [this](uint32_t idx, ChordFollowMode mode) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].chordFollowMode = mode;
                recordProjectHistory("Set Chord Follow on Track " + std::to_string(idx + 1), "CHORD");
            }
        };
        modularTrackInspectorView_->onBakeChords = [this](uint32_t idx) {
            recordProjectHistory("Bake Chords on Track " + std::to_string(idx + 1), "CHORD");
            setStatusMessage("Harmonic chords baked into track " + std::to_string(idx + 1));
        };
        modularTrackInspectorView_->onParamChanged = [this](uint32_t idx, const std::string& paramName, float normVal) {
            (void)idx;
            dispatchHardwareParam(paramName, normVal);
        };
        modularTrackInspectorView_->onScrollChanged = [this](float sY) {
            trackInspectorScrollY_ = sY;
        };
    }

    if (modularMixerView_) {
        modularMixerView_->onMuteToggled = [this](uint32_t idx, bool mute) {
            setTrackMuteState(idx, mute);
        };
        modularMixerView_->onSoloToggled = [this](uint32_t idx, bool solo) {
            setTrackSoloState(idx, solo);
        };
        modularMixerView_->onTrackSelected = [this](uint32_t idx) {
            setSelectedTrackIndex(idx);
        };
        modularMixerView_->onTrackRename = [this](uint32_t idx, const std::string& newName) {
            if (idx < arrangerTracks_.size()) {
                arrangerTracks_[idx].name = newName;
            }
            if (idx < mixerStrips_.size()) {
                mixerStrips_[idx].name = newName;
            }
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                modularArrangerView_->getTracks()[idx].name = newName;
            }
            setStatusMessage("Track renamed: " + newName);
        };
        modularMixerView_->onChooseTrackIcon = [this](uint32_t idx) {
            if (modularArrangerView_ && idx < modularArrangerView_->getTracks().size()) {
                const auto& trk = modularArrangerView_->getTracks()[idx];
                modularArrangerView_->getIconSearchDialog().open(trk.name, idx, trk.iconRef);
            }
        };
    }

    if (modularEditView_) {
        modularEditView_->onNotesChanged = [this](uint32_t tIdx, int cIdx) {
            if (!modularArrangerView_) return;
            auto& tracks = modularArrangerView_->getTracks();
            if (tIdx < tracks.size()) {
                int clipToUpdate = cIdx;
                if (clipToUpdate < 0 || static_cast<size_t>(clipToUpdate) >= tracks[tIdx].clips.size()) {
                    clipToUpdate = modularArrangerView_->getSelectedClipIndex();
                    if (clipToUpdate < 0 || static_cast<size_t>(clipToUpdate) >= tracks[tIdx].clips.size()) {
                        clipToUpdate = 0;
                    }
                }
                if (!tracks[tIdx].clips.empty() && static_cast<size_t>(clipToUpdate) < tracks[tIdx].clips.size()) {
                    modularEditView_->writeBackToArrangerClip(tracks[tIdx].clips[clipToUpdate]);
                    modularArrangerView_->updateClipDetectedChords(tracks[tIdx].clips[clipToUpdate]);
                }
            }
            if (engine_) {
                syncArrangerToSequencer();
            }
        };
    }

    syncArrangerToSequencer();
    syncActiveClipToEditView(0, 0);

    if (modularDesignView_) {
        modularDesignView_->onCompileScript = [this](const std::string& targetId, const std::string& code) {
            setScriptCode(code);
            compileActiveScript();
        };
        modularDesignView_->onCopyToClipboard = [this](const std::string& text) {
#if EATS_HAS_GLFW
            if (window_) {
                glfwSetClipboardString(window_, text.c_str());
            }
#endif
        };
    }

    valueEditDialog_.onCopyToClipboard = [this](const std::string& text) {
#if EATS_HAS_GLFW
        if (window_) {
            glfwSetClipboardString(window_, text.c_str());
        }
#endif
    };

#if EATS_HAS_GLFW
#if defined(_WIN32)
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using SetProcessDpiAwarenessContextFunc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setDpiFunc = reinterpret_cast<SetProcessDpiAwarenessContextFunc>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setDpiFunc) {
            setDpiFunc(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        } else {
            using SetProcessDPIAwareFunc = BOOL(WINAPI*)();
            auto fallbackFunc = reinterpret_cast<SetProcessDPIAwareFunc>(
                GetProcAddress(user32, "SetProcessDPIAware"));
            if (fallbackFunc) fallbackFunc();
        }
    }
#endif
    if (!glfwInit()) {
        std::cerr << "[GuiWindow] Warning: Could not initialize GLFW (running in headless mode)." << std::endl;
        return true;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

#if defined(__EMSCRIPTEN__)
    double initCssW = EM_ASM_DOUBLE({
        var c = document.getElementById('canvas-container');
        return (c && c.clientWidth > 0) ? c.clientWidth : (window.innerWidth || 1280);
    });
    double initCssH = EM_ASM_DOUBLE({
        var c = document.getElementById('canvas-container');
        return (c && c.clientHeight > 0) ? c.clientHeight : (window.innerHeight || 800);
    });
    if (initCssW > 200.0) windowWidth_ = static_cast<int>(std::round(initCssW));
    if (initCssH > 200.0) windowHeight_ = static_cast<int>(std::round(initCssH));
#endif

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "[GuiWindow] Warning: GLFW window creation failed (running in headless mode)." << std::endl;
        return true;
    }

    GLFWimage icons[2];
    icons[0].width = kAppIcon48Width;
    icons[0].height = kAppIcon48Height;
    icons[0].pixels = const_cast<unsigned char*>(kAppIcon48Pixels);
    icons[1].width = kAppIcon32Width;
    icons[1].height = kAppIcon32Height;
    icons[1].pixels = const_cast<unsigned char*>(kAppIcon32Pixels);
    glfwSetWindowIcon(window_, 2, icons);

    glfwShowWindow(window_);

    glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
    glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
#if defined(__EMSCRIPTEN__)
    g_activeGuiWindowForWeb = this;
    double initDpr = EM_ASM_DOUBLE({ return window.devicePixelRatio || 1.0; });
    fbWidth_ = static_cast<int>(std::round(static_cast<double>(windowWidth_) * (hiDpiEnabled_ ? initDpr : 1.0)));
    fbHeight_ = static_cast<int>(std::round(static_cast<double>(windowHeight_) * (hiDpiEnabled_ ? initDpr : 1.0)));
    EM_ASM({
        var canvas = document.getElementById('canvas');
        if (canvas) {
            canvas.width = $0;
            canvas.height = $1;
        }
        if (!canvas) canvas = document.body;
        if (canvas && !canvas._eatsDropInitialized) {
            canvas._eatsDropInitialized = true;
            canvas.addEventListener('dragover', function(e) {
                e.preventDefault();
                e.stopPropagation();
                if (e.dataTransfer) {
                    e.dataTransfer.dropEffect = 'copy';
                }
            });
            canvas.addEventListener('drop', function(e) {
                e.preventDefault();
                e.stopPropagation();
                var files = e.dataTransfer ? e.dataTransfer.files : null;
                if (!files || files.length === 0) return;
                var rect = canvas.getBoundingClientRect ? canvas.getBoundingClientRect() : {left: 0, top: 0};
                var clientX = e.clientX - rect.left;
                var clientY = e.clientY - rect.top;

                for (var i = 0; i < files.length; i++) {
                    (function(file) {
                        var reader = new FileReader();
                        reader.onload = function(evt) {
                            var arrayBuffer = evt.target.result;
                            var uint8 = new Uint8Array(arrayBuffer);
                            var safeName = file.name.replace(/[^a-zA-Z0-9._-]/g, '_');
                            var vpath = '/tmp/' + safeName;
                            try {
                                FS.writeFile(vpath, uint8);
                                Module.ccall('eats_on_file_dropped_web', null,
                                    ['string', 'number', 'number'],
                                    [vpath, clientX, clientY]
                                );
                            } catch (err) {
                                console.error('Eatsbits Web Drop Error:', err);
                            }
                        };
                        reader.readAsArrayBuffer(file);
                    })(files[i]);
                }
            });
        }
    }, fbWidth_, fbHeight_);
#endif
    updateLogicalDimensions();
    initFontRenderer();

    uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
    uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));

    batchRenderer_ = std::make_unique<BatchRenderer2D>();
    batchRenderer_->setRenderScale(renderScale_, renderScale_);
    RenderBackendType backend = RenderBackendType::WebGPU;
    batchRenderer_->initialize(window_, physW, physH, backend);
    batchRenderer_->setAntiAliasingMode(antiAliasingMode_);
    g_activeBatchRenderer = batchRenderer_.get();

#if defined(__EMSCRIPTEN__)
    if (dawnBridge_.initializeWeb(batchRenderer_->getNativeDevice(), batchRenderer_->getNativeSurface(), physW, physH)) {
        if (dawnBridge_.isNativeActive()) {
            std::cout << "[GuiWindow] Connected Google Dawn / WebGPU pipeline on Web." << std::endl;
            batchRenderer_->setDirectPresent(false);
            batchRenderer_->setCustomRenderTargetView(dawnBridge_.getDawTextureView());
        }
    }
#else
    if (dawnBridge_.initializeNative(window_, physW, physH)) {
        if (dawnBridge_.isNativeActive()) {
            std::cout << "[GuiWindow] Connected Google Dawn / WebGPU pipeline." << std::endl;
            batchRenderer_->setDirectPresent(false);
        }
    }
#endif

    // Store pointer for GLFW callbacks
    glfwSetWindowUserPointer(window_, this);

    glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) app->onFramebufferResize(width, height);
    });

    glfwSetWindowSizeCallback(window_, [](GLFWwindow* w, int width, int height) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) app->onWindowResize(width, height);
    });

    glfwSetWindowRefreshCallback(window_, [](GLFWwindow* w) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app && !app->isRendering_) {
            app->renderFrame();
        }
    });

    glfwSetCursorPosCallback(window_, [](GLFWwindow* w, double xpos, double ypos) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            float logX = app->windowToLogicalX(xpos);
            float logY = app->windowToLogicalY(ypos);
            if (app->is3dConsoleEnabled()) {
                app->transform3dMouseCoords(logX, logY, logX, logY);
            }
            app->remapCrtMouseCoords(logX, logY, logX, logY);
            app->onMouseMove(logX, logY);
        }
    });

    glfwSetMouseButtonCallback(window_, [](GLFWwindow* w, int button, int action, int /*mods*/) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (!app) return;
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        float logX = app->windowToLogicalX(x);
        float logY = app->windowToLogicalY(y);
        if (app->is3dConsoleEnabled()) {
            app->transform3dMouseCoords(logX, logY, logX, logY);
        }
        app->remapCrtMouseCoords(logX, logY, logX, logY);
        if (action == GLFW_PRESS) {
            app->onMouseDown(button, logX, logY);
        } else if (action == GLFW_RELEASE) {
            app->onMouseUp(button, logX, logY);
        }
    });

    glfwSetKeyCallback(window_, [](GLFWwindow* w, int key, int /*scancode*/, int action, int mods) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
            app->onKeyDown(key, mods);
        }
    });

    glfwSetCharCallback(window_, [](GLFWwindow* w, unsigned int codepoint) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->onChar(codepoint);
        }
    });

    glfwSetScrollCallback(window_, [](GLFWwindow* w, double xoffset, double yoffset) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (app) {
            app->onMouseScroll(xoffset, yoffset);
        }
    });

    glfwSetDropCallback(window_, [](GLFWwindow* w, int count, const char** paths) {
        auto* app = static_cast<GuiWindow*>(glfwGetWindowUserPointer(w));
        if (!app || count <= 0 || !paths) return;
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        float logX = app->windowToLogicalX(x);
        float logY = app->windowToLogicalY(y);
        if (app->is3dConsoleEnabled()) {
            app->transform3dMouseCoords(logX, logY, logX, logY);
        }
        app->remapCrtMouseCoords(logX, logY, logX, logY);
        std::vector<std::string> fileList;
        fileList.reserve(count);
        for (int i = 0; i < count; ++i) {
            if (paths[i]) fileList.emplace_back(paths[i]);
        }
        app->onFilesDropped(fileList, logX, logY);
    });
#endif

    return true;
}

ViewContext GuiWindow::createViewContext() noexcept {
    ViewContext ctx;
    ctx.renderer = batchRenderer_.get();
    ctx.theme = &getTheme();
    ctx.audioEngine = engine_;
    ctx.screenWidth = static_cast<float>(width_);
    ctx.screenHeight = static_cast<float>(height_);
    ctx.logicalWidth = static_cast<float>(width_);
    ctx.logicalHeight = static_cast<float>(height_);
    ctx.uiScale = renderScale_;
    ctx.mouseX = mouseX_;
    ctx.mouseY = mouseY_;
    ctx.onNavigateTab = [this](WorkspaceView v) {
        if (v == WorkspaceView::Edit && modularEditView_) {
            modularEditView_->autoCenterOnNotesOrDefault();
        }
        setActiveView(v);
    };
    ctx.onJumpToClipEdit = [this](uint32_t tIdx, int cIdx) {
        setSelectedTrackIndex(tIdx);
        syncActiveClipToEditView(tIdx, cIdx);
        if (modularEditView_) {
            modularEditView_->autoCenterOnNotesOrDefault();
        }
        setActiveView(WorkspaceView::Edit);
    };
    ctx.onToggleBrowser = [this](bool open) { setBrowserOpen(open); };
    ctx.onShowNotification = [this](const std::string& msg) { setStatusMessage(msg); };
    ctx.onOpenValueEdit = [this](const ValueEditRequest& req) { openValueEditDialog(req); };
    return ctx;
}

void GuiWindow::pollWebResize() noexcept {
#if defined(__EMSCRIPTEN__)
    if (!window_) return;

    double cssW = EM_ASM_DOUBLE({
        var container = document.getElementById('canvas-container');
        return (container && container.clientWidth > 0) ? container.clientWidth : (window.innerWidth || 1280);
    });
    double cssH = EM_ASM_DOUBLE({
        var container = document.getElementById('canvas-container');
        return (container && container.clientHeight > 0) ? container.clientHeight : (window.innerHeight || 800);
    });
    double dpr = EM_ASM_DOUBLE({
        return window.devicePixelRatio || 1.0;
    });

    int targetW = static_cast<int>(std::round(cssW));
    int targetH = static_cast<int>(std::round(cssH));

    if (targetW < 320) targetW = 320;
    if (targetH < 240) targetH = 240;

    float expectedDpi = hiDpiEnabled_ ? static_cast<float>(dpr) : 1.0f;
    int targetFbW = static_cast<int>(std::round(static_cast<float>(targetW) * expectedDpi));
    int targetFbH = static_cast<int>(std::round(static_cast<float>(targetH) * expectedDpi));

    if (targetW != windowWidth_ || targetH != windowHeight_ || targetFbW != fbWidth_ || targetFbH != fbHeight_) {
        glfwSetWindowSize(window_, targetW, targetH);
        windowWidth_ = targetW;
        windowHeight_ = targetH;
        fbWidth_ = targetFbW;
        fbHeight_ = targetFbH;

        EM_ASM({
            var canvas = document.getElementById('canvas');
            if (canvas) {
                canvas.width = $0;
                canvas.height = $1;
            }
        }, fbWidth_, fbHeight_);

        onWindowResize(targetW, targetH);
        onFramebufferResize(fbWidth_, fbHeight_);
    }
#endif
}

void GuiWindow::runEventLoop() {
#if defined(__EMSCRIPTEN__)
    if (!window_) return;
    std::cout << "[GUI] Registering requestAnimationFrame main loop with responsive auto-resize..." << std::endl;
    pollWebResize();
    emscripten_set_main_loop_arg([](void* arg) {
        auto* self = static_cast<GuiWindow*>(arg);
        if (self && self->isOpen()) {
            self->pollWebResize();
            glfwPollEvents();
            self->renderFrame();
        }
    }, this, 0, 0);
#elif EATS_HAS_GLFW
    if (!window_) return;

    using clock = std::chrono::steady_clock;
    constexpr auto targetFrameDuration = std::chrono::microseconds(16666); // ~60 FPS smooth frame pacing

    while (!glfwWindowShouldClose(window_)) {
        auto frameStart = clock::now();

        glfwPollEvents();
        renderFrame();

        auto frameEnd = clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);
        if (elapsed < targetFrameDuration) {
            std::this_thread::sleep_for(targetFrameDuration - elapsed);
        }
    }
#endif
}

void GuiWindow::close() noexcept {
#if EATS_HAS_GLFW
    dawnBridge_.shutdownNative();
    if (g_activeBatchRenderer == batchRenderer_.get()) {
        g_activeBatchRenderer = nullptr;
    }
    batchRenderer_.reset();
    fontRenderer_.reset();
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
#endif
}

bool GuiWindow::isOpen() const noexcept {
#if EATS_HAS_GLFW
    if (window_) {
        return !glfwWindowShouldClose(window_);
    }
#endif
    return false;
}

void GuiWindow::drawTopTransportBar() {
#if EATS_HAS_GLFW
    const auto& theme = getTheme();
    const float r = static_cast<float>(width_);

    // =========================================================================
    // =========================================================================
    // TOP TRANSPORT HEADER BAR (Height = 56px) - Heavy Machined Metal Faceplate
    // =========================================================================
    // Procedural Theme-Tinted Grungy Metal Chassis Faceplate (Shader-Off & Base)
    drawChassisPlate(0.0f, 0.0f, r, 56.0f, true, theme);

    // Top specular rim highlight & uniform bottom machined bevel
    drawLine(0.0f, 0.0f, r, 0.0f, theme.borderSubtle.lighten(0.15f), 1.5f);
    drawLine(0.0f, 1.5f, r, 1.5f, theme.borderSubtle, 1.0f);
    drawLine(0.0f, 54.5f, r, 54.5f, theme.borderSubtle.darken(0.10f), 1.0f);
    drawLine(0.0f, 56.0f, r, 56.0f, theme.backgroundDark, 1.5f);

    // Industrial Hex Corner Fasteners
    auto drawRivet = [&](float rx, float ry) {
        drawCircle(rx, ry, 5.0f, theme.backgroundDark.darken(0.15f));
        drawCircle(rx, ry, 3.5f, theme.borderSubtle);
        drawLine(rx - 2.0f, ry, rx + 2.0f, ry, theme.backgroundDark, 1.2f);
        drawLine(rx, ry - 2.0f, rx, ry + 2.0f, theme.backgroundDark, 1.2f);
    };
    drawRivet(r - 8.0f, 8.0f);

    const bool isPlaying = engine_ ? engine_->getSequencer().isPlaying() : false;

    // 1. EATSBITS BRAND EMBLEM LOGO (Unboxed plain silkscreen logo directly on metal faceplate)
    const float logoX = 14.0f;
    const float logoY = 16.0f;
    const float logoW = 27.0f;
    const float logoH = 24.0f;
    const bool logoHovered = (mouseX_ >= 8.0f && mouseX_ <= 50.0f &&
                              mouseY_ >= 8.0f && mouseY_ <= 48.0f);
    Color logoColor = logoHovered ? theme.primaryAccent.lighten(0.18f) : theme.primaryAccent;
    drawPlainEatsbitsLogo(logoX, logoY, logoW, logoH, logoColor, 1.0f, logoHovered, theme.backgroundDark);

    // =========================================================================
    // 2. CLUSTERED TACTILE TRANSPORT KEYBANK (Play, Stop, Record)
    // =========================================================================
    const float tX = 54.0f;
    const float tY = 10.0f;
    const float btnW = 38.0f;
    const float btnH = 36.0f;
    const float clusterW = btnW * 3.0f; // 114px

    // Deep recessed perimeter trench housing the clustered transport keys
    drawRoundedRect(tX - 2.5f, tY - 2.5f, clusterW + 5.0f, btnH + 5.0f, 4.0f, theme.controlWell.darken(0.20f));
    drawRoundedRectOutline(tX - 2.5f, tY - 2.5f, clusterW + 5.0f, btnH + 5.0f, 4.0f, theme.borderSubtle.darken(0.20f), 1.0f);

    // Keycap base colors derived dynamically from theme primary accent
    Color baseCapTop = theme.isLight ? theme.primaryAccent : theme.primaryAccent.darken(0.16f);
    Color baseCapBot = theme.isLight ? baseCapTop.darken(0.15f) : baseCapTop.darken(0.24f);

    // Helper for rendering realistic tactile keycaps with smooth corner radii and bevels
    auto drawTransportKeyBase = [&](float bx, float by, float bw, float bh, bool pressed, int keyPos) {
        float po = pressed ? 2.0f : 0.0f;
        Color capTop = pressed ? baseCapTop.darken(0.14f) : baseCapTop;
        Color capBot = pressed ? baseCapBot.darken(0.14f) : baseCapBot;

        float cornerRadius = 3.5f;
        if (keyPos == 0) {
            // Left key: rounded left corners, square right
            drawRoundedRectGradient(bx, by + po, bw, bh - po, cornerRadius, capTop, capBot);
            drawRectGradient(bx + bw * 0.5f, by + po, bw * 0.5f, bh - po, capTop, capBot);
        } else if (keyPos == 2) {
            // Right key: square left corners, rounded right
            drawRoundedRectGradient(bx, by + po, bw, bh - po, cornerRadius, capTop, capBot);
            drawRectGradient(bx, by + po, bw * 0.5f, bh - po, capTop, capBot);
        } else {
            // Center key: square
            drawRectGradient(bx, by + po, bw, bh - po, capTop, capBot);
        }
    };

    auto drawTransportKeyOverlay = [&](float bx, float by, float bw, float bh, bool pressed, int keyPos) {
        float po = pressed ? 2.0f : 0.0f;
        Color capTop = pressed ? baseCapTop.darken(0.14f) : baseCapTop;
        Color capBot = pressed ? baseCapBot.darken(0.14f) : baseCapBot;

        float cornerRadius = 3.5f;
        float cr = (keyPos == 1) ? 0.0f : cornerRadius;
        bool rTL = (keyPos == 0);
        bool rBL = (keyPos == 0);
        bool rTR = (keyPos == 2);
        bool rBR = (keyPos == 2);

        // Tactile micro-noise overlay applied across keycap AND printed icon content!
        drawButtonNoise(bx, by + po, bw, bh - po, true, 0.28f, cr, rTL, rTR, rBL, rBR, pressed, 3.0f);

        // Multi-layer 3D Chamfer Bevels
        Color topBevel = capTop.lighten(0.22f);
        Color botShadow = capBot.darken(0.22f);
        drawLine(bx + 1.0f, by + po + 1.0f, bx + bw - 1.0f, by + po + 1.0f, topBevel, 1.5f);
        drawLine(bx + 1.0f, by + bh - 1.0f, bx + bw - 1.0f, by + bh - 1.0f, botShadow, 1.5f);

        // Vertical hairline shadow outline
        drawRectOutline(bx, by + po, bw, bh - po, capBot.darken(0.15f), 1.0f);
    };

    // Dark screen-printed icon color providing clean tactile contrast against keycaps
    Color darkIcon = theme.isLight ? theme.textPrimary : Color(0.14f, 0.12f, 0.10f);

    // PLAY KEY (Key 0)
    drawTransportKeyBase(tX, tY, btnW, btnH, isPlaying, 0);
    // Dark Charcoal / Graphite Screen-Printed Triangle Icon
    float plX = tX + 13.5f;
    float plY = tY + 11.0f + (isPlaying ? 2.0f : 0.0f);
    drawTriangle(plX, plY, plX + 13.0f, plY + 7.0f, plX, plY + 14.0f, darkIcon.r, darkIcon.g, darkIcon.b, 0.92f);
    drawTransportKeyOverlay(tX, tY, btnW, btnH, isPlaying, 0);

    // Tight Division Crevice between Play & Stop
    drawLine(tX + btnW, tY, tX + btnW, tY + btnH, theme.controlWell.darken(0.35f), 1.5f);

    // STOP KEY (Key 1)
    drawTransportKeyBase(tX + btnW, tY, btnW, btnH, !isPlaying, 1);
    // Dark Charcoal / Graphite Screen-Printed Square Icon
    float stX = tX + btnW + 12.5f;
    float stY = tY + 11.5f + (!isPlaying ? 2.0f : 0.0f);
    drawRect(stX, stY, 13.0f, 13.0f, darkIcon.r, darkIcon.g, darkIcon.b, 0.92f);
    drawTransportKeyOverlay(tX + btnW, tY, btnW, btnH, !isPlaying, 1);

    // Tight Division Crevice between Stop & Record
    drawLine(tX + btnW * 2.0f, tY, tX + btnW * 2.0f, tY + btnH, theme.controlWell.darken(0.35f), 1.5f);

    // RECORD KEY (Key 2)
    drawTransportKeyBase(tX + btnW * 2.0f, tY, btnW, btnH, false, 2);
    // Screen-Printed Deep Vermilion / Crimson Circle Dot (Themed)
    float recX = tX + btnW * 2.0f + 19.0f;
    float recY = tY + 18.0f;
    drawCircleOutline(recX, recY, 6.5f, theme.recordActive.darken(0.30f), 1.2f);
    drawCircle(recX, recY, 6.5f, theme.recordActive.withAlpha(0.92f));
    drawTransportKeyOverlay(tX + btnW * 2.0f, tY, btnW, btnH, false, 2);

    // =========================================================================
    // 3. MINIMAL RECESSED READOUT PODS (BPM & TIMECODE)
    // =========================================================================
    // Helper 1: Recessed dark trench and smoked substrate (behind the content)
    auto drawReadoutPodBase = [&](float podX, float podY, float podW, float podH) {
        drawRoundedRect(podX - 2.5f, podY - 2.5f, podW + 5.0f, podH + 5.0f, 6.0f, theme.controlWell.darken(0.20f));
        drawRoundedRectOutline(podX - 2.5f, podY - 2.5f, podW + 5.0f, podH + 5.0f, 6.0f, theme.borderSubtle.darken(0.20f), 1.0f);
        drawRoundedRect(podX, podY, podW, podH, 4.5f, theme.lcdBackground);
    };

    // Helper 2: Subtle lens reflection gradient rendered IN FRONT of the content
    // Matching CSS: linear-gradient(to bottom, 0% rgba(226,226,226), 50% rgba(219,219,219), 51% rgba(209,209,209), 100% rgba(254,254,254))
    // tinted with theme.lcdBackground
    auto drawReadoutPodGlassOverlay = [&](float podX, float podY, float podW, float podH) {
        const Color& B = theme.lcdBackground;
        float halfH = podH * 0.5f;

        // Top half specular reflection: 0% to 50%
        Color gradTop0 = Color(std::clamp(B.r * 1.20f + 0.16f, 0.0f, 1.0f),
                               std::clamp(B.g * 1.20f + 0.16f, 0.0f, 1.0f),
                               std::clamp(B.b * 1.20f + 0.16f, 0.0f, 1.0f), 0.18f);
        Color gradTop1 = Color(std::clamp(B.r * 1.05f + 0.06f, 0.0f, 1.0f),
                               std::clamp(B.g * 1.05f + 0.06f, 0.0f, 1.0f),
                               std::clamp(B.b * 1.05f + 0.06f, 0.0f, 1.0f), 0.07f);
        drawRoundedRectGradient(podX, podY, podW, halfH + 1.0f, 4.5f, gradTop0, gradTop1);
        drawRectGradient(podX, podY + 4.5f, podW, halfH - 3.5f, gradTop0, gradTop1);

        // Sharp specular split horizon crease at 50%/51%
        drawLine(podX + 2.0f, podY + halfH, podX + podW - 2.0f, podY + halfH, Color(0.0f, 0.0f, 0.0f, 0.30f), 1.0f);

        // Bottom half specular refraction: 51% to 100%
        Color gradBot0 = Color(B.r * 0.85f, B.g * 0.85f, B.b * 0.85f, 0.04f);
        Color gradBot1 = Color(std::clamp(B.r * 1.35f + 0.22f, 0.0f, 1.0f),
                               std::clamp(B.g * 1.35f + 0.22f, 0.0f, 1.0f),
                               std::clamp(B.b * 1.35f + 0.22f, 0.0f, 1.0f), 0.24f);
        drawRoundedRectGradient(podX, podY + halfH, podW, halfH, 4.5f, gradBot0, gradBot1);
        drawRectGradient(podX, podY + halfH, podW, halfH - 4.5f, gradBot0, gradBot1);

        // Smoked acrylic glass outer lens bezel rim
        drawRoundedRectOutline(podX, podY, podW, podH, 4.5f, theme.lcdBorder.withAlpha(0.65f), 1.0f);

        // Top subtle inner shadow line
        drawLine(podX + 2.0f, podY + 1.0f, podX + podW - 2.0f, podY + 1.0f, Color(0.0f, 0.0f, 0.0f, 0.55f), 1.5f);

        // Bottom specular flare reflection line
        drawLine(podX + 2.0f, podY + podH - 1.0f, podX + podW - 2.0f, podY + podH - 1.0f,
                 Color(1.0f, 1.0f, 1.0f, 0.20f), 1.0f);
    };

    // Shared readout palette (both readouts in theme.lcdText with subtle, delicate phosphor bloom)
    Color textColor = theme.lcdText;
    Color glowColor = theme.tempoGlow;

    // --- POD 1: BPM READOUT CAPSULE (Center-Left) ---
    const float bpmPodX = tX + clusterW + 14.0f; // ~182px
    const float bpmPodY = 10.0f;
    const float bpmPodW = 112.0f;
    const float bpmPodH = 36.0f;

    // 1. Base substrate
    drawReadoutPodBase(bpmPodX, bpmPodY, bpmPodW, bpmPodH);

    int bpmVal = engine_ ? static_cast<int>(std::round(engine_->getSequencer().getTransport().getBpm())) : 120;
    std::string bpmStr = std::to_string(bpmVal) + " BPM";

    float bpmTextX = bpmPodX + (bpmPodW - static_cast<float>(bpmStr.size()) * 11.5f) * 0.5f;
    float bpmTextY = bpmPodY + 8.5f;

    // 2. Soft, delicate ambient bloom behind digits
    float bpmCx = bpmPodX + bpmPodW * 0.5f;
    float bpmCy = bpmPodY + bpmPodH * 0.5f;
    drawRoundedRect(bpmCx - 30.0f, bpmCy - 9.0f, 60.0f, 18.0f, 5.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.045f));
    drawRoundedRect(bpmCx - 18.0f, bpmCy - 6.0f, 36.0f, 12.0f, 4.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.065f));

    // 3. Subtle character optical bloom halo (lower intensity, soft & natural)
    // Faint aura (1.2px)
    drawMonoString(bpmStr, bpmTextX - 1.2f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(bpmStr, bpmTextX + 1.2f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(bpmStr, bpmTextX, bpmTextY - 1.0f, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(bpmStr, bpmTextX, bpmTextY + 1.0f, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    // Proximity halo (0.6px)
    drawMonoString(bpmStr, bpmTextX - 0.6f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    drawMonoString(bpmStr, bpmTextX + 0.6f, bpmTextY, 1.15f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    // Active illuminated core text
    drawMonoString(bpmStr, bpmTextX, bpmTextY, 1.15f, textColor.r, textColor.g, textColor.b, 0.98f);

    // 4. Glass reflection gradient rendered IN FRONT of content
    drawReadoutPodGlassOverlay(bpmPodX, bpmPodY, bpmPodW, bpmPodH);

    // --- POD 2: SECONDARY TIMECODE / BAR:BEAT:DIV READOUT (Right side) ---
    int curStep = engine_ ? static_cast<int>(engine_->getSequencer().getTransport().getCurrentStep()) : 0;
    int barNum = (curStep / 16) + 1;
    int beatNum = ((curStep % 16) / 4) + 1;
    int subBeat = (curStep % 4) + 1;

    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%d:%d", barNum, beatNum, subBeat);

    const float timePodW = 96.0f;
    const float timePodH = 36.0f;
    const float timePodX = r - 228.0f;
    const float timePodY = 10.0f;

    // 1. Base substrate
    drawReadoutPodBase(timePodX, timePodY, timePodW, timePodH);

    float timeTextX = timePodX + (timePodW - 6.0f * 10.5f) * 0.5f;
    float timeTextY = timePodY + 8.5f;

    // 2. Soft, delicate ambient bloom behind digits
    float timeCx = timePodX + timePodW * 0.5f;
    float timeCy = timePodY + timePodH * 0.5f;
    drawRoundedRect(timeCx - 26.0f, timeCy - 9.0f, 52.0f, 18.0f, 5.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.045f));
    drawRoundedRect(timeCx - 15.0f, timeCy - 6.0f, 30.0f, 12.0f, 4.0f,
                    Color(glowColor.r, glowColor.g, glowColor.b, 0.065f));

    // 3. Unlit ghost segment mask
    Color ghostCol = textColor.withAlpha(0.12f);
    drawMonoString("88:8:8", timeTextX, timeTextY, 1.05f, ghostCol.r, ghostCol.g, ghostCol.b, ghostCol.a);

    // 4. Active illuminated digits with subtle, softened bloom
    drawMonoString(timeBuf, timeTextX - 1.2f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX + 1.2f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX, timeTextY - 1.0f, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX, timeTextY + 1.0f, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.07f);
    drawMonoString(timeBuf, timeTextX - 0.6f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    drawMonoString(timeBuf, timeTextX + 0.6f, timeTextY, 1.05f, glowColor.r, glowColor.g, glowColor.b, 0.16f);
    drawMonoString(timeBuf, timeTextX, timeTextY, 1.05f, textColor.r, textColor.g, textColor.b, 0.98f);

    // 5. Glass reflection gradient rendered IN FRONT of content
    drawReadoutPodGlassOverlay(timePodX, timePodY, timePodW, timePodH);

    // =========================================================================
    // 5. RIGHT TACTILE HARDWARE BUTTONS (Lock, Search, Folder)
    // =========================================================================
    const float toolY = 11.0f;
    const float sqBtnSize = 34.0f;
    Color buttonEtch = theme.secondaryAccent;

    auto drawTactileSquareBtnBase = [&](float bx, float by, bool active) {
        float po = active ? 2.0f : 0.0f;
        // Recessed shadow well (Themed)
        drawRoundedRect(bx - 2.0f, by - 2.0f, sqBtnSize + 4.0f, sqBtnSize + 4.0f, 4.0f, theme.controlWell.darken(0.20f));
        drawRoundedRectOutline(bx - 2.0f, by - 2.0f, sqBtnSize + 4.0f, sqBtnSize + 4.0f, 4.0f, theme.borderSubtle.darken(0.20f), 1.0f);
        // Keycap gradient
        Color capTop = active ? theme.controlWell : theme.controlBackground;
        Color capBot = active ? theme.controlWell.darken(0.12f) : theme.controlWell;
        drawRoundedRectGradient(bx, by + po, sqBtnSize, sqBtnSize - po, 3.5f, capTop, capBot);
    };

    auto drawTactileSquareBtnOverlay = [&](float bx, float by, bool active) {
        float po = active ? 2.0f : 0.0f;
        Color capTop = active ? theme.controlWell : theme.controlBackground;

        // Tactile micro-noise overlay applied across keycap AND icon glyph!
        drawButtonNoise(bx, by + po, sqBtnSize, sqBtnSize - po, true, 0.28f, 3.5f, true, true, true, true, active, 3.0f);

        // Top specular bevel highlight
        drawLine(bx + 2.0f, by + po + 1.0f, bx + sqBtnSize - 2.0f, by + po + 1.0f, capTop.lighten(0.16f), 1.5f);
        drawRoundedRectOutline(bx, by + po, sqBtnSize, sqBtnSize - po, 3.5f,
                               active ? theme.primaryAccent : theme.borderSubtle.darken(0.15f), active ? 1.5f : 1.0f);
    };

    // BUTTON 1: LOCK (Toggle Workspace Edit Lock)
    const float lockX = r - 120.0f;
    drawTactileSquareBtnBase(lockX, toolY, projectLocked_);
    // Etched Lock Glyph (Themed with clear contrast)
    Color lockCol = projectLocked_ ? theme.primaryAccent : buttonEtch.lighten(0.10f);
    float lBodyX = lockX + 10.5f;
    float lBodyY = toolY + 16.0f + (projectLocked_ ? 2.0f : 0.0f);
    drawRoundedRect(lBodyX, lBodyY, 13.0f, 10.0f, 2.0f, lockCol);
    drawCircleOutline(lBodyX + 6.5f, lBodyY - 1.0f, 4.0f, lockCol, 1.5f);
    drawCircle(lBodyX + 6.5f, lBodyY + 4.5f, 1.5f, darkIcon);
    drawTactileSquareBtnOverlay(lockX, toolY, projectLocked_);

    // BUTTON 2: SEARCH / INSPECT (Toggle Fullscreen / Plugin Finder)
    const float searchX = r - 80.0f;
    drawTactileSquareBtnBase(searchX, toolY, isFullscreen_);
    // Etched Magnifying Glass Glyph (Themed with clear contrast)
    Color searchCol = isFullscreen_ ? theme.primaryAccent : buttonEtch.lighten(0.10f);
    float sLensX = searchX + 15.0f;
    float sLensY = toolY + 16.0f + (isFullscreen_ ? 2.0f : 0.0f);
    drawCircleOutline(sLensX, sLensY, 5.0f, searchCol, 1.6f);
    drawLine(sLensX + 3.5f, sLensY + 3.5f, sLensX + 7.5f, sLensY + 7.5f, searchCol, 2.0f);
    drawTactileSquareBtnOverlay(searchX, toolY, isFullscreen_);

    // BUTTON 3: FOLDER / PRESET LIBRARY (Toggle Sliding Drawer)
    const float folderX = r - 40.0f;
    drawTactileSquareBtnBase(folderX, toolY, browserOpen_);
    // Etched Folder Glyph (Themed with clear contrast)
    Color fldCol = browserOpen_ ? theme.primaryAccent : buttonEtch.lighten(0.10f);
    float fX = folderX + 8.5f;
    float fY = toolY + 12.0f + (browserOpen_ ? 2.0f : 0.0f);
    drawRect(fX, fY, 6.0f, 3.5f, fldCol);
    drawRoundedRect(fX, fY + 3.0f, 17.0f, 11.5f, 1.5f, fldCol);
    drawLine(fX, fY + 5.0f, fX + 17.0f, fY + 5.0f, darkIcon, 1.0f);
    drawTactileSquareBtnOverlay(folderX, toolY, browserOpen_);
#endif
}

void GuiWindow::renderFrame() {
    if (!engine_ || isRendering_) return;
    isRendering_ = true;
    struct RenderGuard {
        bool& flag;
        ~RenderGuard() { flag = false; }
    } guard{isRendering_};

    static int s_frameLog = 0;
    if (s_frameLog++ < 3) {
        std::cout << "[GUI] Frame #" << s_frameLog << " rendering (size " << width_ << "x" << height_
                  << ", fb " << fbWidth_ << "x" << fbHeight_ << ")" << std::endl;
    }

    static auto s_lastFrameTime = std::chrono::steady_clock::now();
    const auto currentFrameTime = std::chrono::steady_clock::now();
    float frameDt = std::chrono::duration<float>(currentFrameTime - s_lastFrameTime).count();
    s_lastFrameTime = currentFrameTime;
    if (frameDt <= 0.0001f || frameDt > 0.1f) frameDt = 0.016f;

    // 1. Pull real-time audio scope data & meters from lock-free queues
    size_t count = engine_->getScopeSamples(scopeBuffer_, 128);
    if (count > 0) {
        canvas_.setScopeData(scopeBuffer_, count);
    }

    MeterFeedback fb{};
    if (engine_ && engine_->pollMeterFeedback(fb)) {
        canvas_.setVuMeter(fb.peakLeft, fb.peakRight);
        masterPeakL_ = std::max(fb.peakLeft, masterPeakL_ * 0.91f);
        masterPeakR_ = std::max(fb.peakRight, masterPeakR_ * 0.91f);
    } else {
        masterPeakL_ *= 0.91f;
        masterPeakR_ *= 0.91f;
    }

    if (engine_) {
        for (size_t i = 0; i < mixerStrips_.size() && i < chPeakL_.size(); ++i) {
            MeterFeedback trFb{};
            if (engine_->getTrackMeterFeedback(static_cast<uint32_t>(i), trFb)) {
                chPeakL_[i] = std::max(trFb.peakLeft, chPeakL_[i] * 0.91f);
                chPeakR_[i] = std::max(trFb.peakRight, chPeakR_[i] * 0.91f);
            } else {
                float est = (masterPeakL_ + masterPeakR_) * 0.5f * (mixerStrips_[i].mute ? 0.0f : mixerStrips_[i].volume);
                chPeakL_[i] = std::max(est, chPeakL_[i] * 0.91f);
                chPeakR_[i] = std::max(est, chPeakR_[i] * 0.91f);
            }
            mixerStrips_[i].peakL = chPeakL_[i];
            mixerStrips_[i].peakR = chPeakR_[i];
        }
    }

#if EATS_HAS_GLFW
    if (window_) {
        g_activeFontRenderer = fontRenderer_.get();
        g_activeBatchRenderer = batchRenderer_.get();
        ScoreVectorGlyphs::setBatchRenderer(batchRenderer_.get());
        if (windowWidth_ == 0 || windowHeight_ == 0 || fbWidth_ == 0 || fbHeight_ == 0) {
            glfwGetFramebufferSize(window_, &fbWidth_, &fbHeight_);
            glfwGetWindowSize(window_, &windowWidth_, &windowHeight_);
        }
        updateLogicalDimensions();

        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));

        if (batchRenderer_) {
            batchRenderer_->setRenderScale(renderScale_, renderScale_);
            batchRenderer_->resize(physW, physH);
            batchRenderer_->beginFrame(static_cast<float>(width_), static_cast<float>(height_));
        }

        const auto& theme = getTheme();
        const bool is3d = is3dConsoleEnabled();
        const float bPanelY = static_cast<float>(height_) - 48.0f;
        const float bPanelH = 48.0f;

        // Background dark gradient
        drawRectGradient(0, 0, static_cast<float>(width_), static_cast<float>(height_),
                         theme.backgroundDark, theme.backgroundDark.darken(0.04f));

        lampTime_ += (1.0f / 60.0f);

        // =========================================================================
        // 2. ACTIVE VIEW DISPATCH
        // =========================================================================
        if (activeView_ == WorkspaceView::Arranger) {
            if (modularArrangerView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = static_cast<float>(height_) - 48.0f;
                modularArrangerView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularArrangerView_->render(ctx);
            }
        } else if (activeView_ == WorkspaceView::ModularRack || activeView_ == WorkspaceView::Design) {
            if (modularDesignView_) {
                ViewContext ctx = createViewContext();
                float topY = 56.0f;
                float bottomY = static_cast<float>(height_) - 48.0f;
                modularDesignView_->setAudioScopeBuffer(scopeBuffer_, 256);
                modularDesignView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
                modularDesignView_->render(ctx);
            }
        } else if (activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) {
        if (modularEditView_) {
            ViewContext ctx = createViewContext();
            float topY = 56.0f;
            float bottomY = static_cast<float>(height_) - 48.0f;
            if (modularEditView_->getActiveTrackIndex() != selectedTrackIndex_) {
                syncActiveClipToEditView(selectedTrackIndex_, -1);
            }
            modularEditView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
            modularEditView_->render(ctx);
        }
    } else if (activeView_ == WorkspaceView::Mixer) {
        if (modularMixerView_) {
            ViewContext ctx = createViewContext();
            float topY = 56.0f;
            float bottomY = bPanelY - 6.0f;

            std::vector<std::string> trackNames;
            std::vector<float> trackVols;
            std::vector<float> trackPans;
            std::vector<bool> trackMutes;
            std::vector<bool> trackSolos;
            std::vector<bool> trackFreezes;
            std::vector<Color> trackColors;

            size_t trkCount = std::max(mixerStrips_.size(), arrangerTracks_.size());
            for (size_t i = 0; i < trkCount; ++i) {
                std::string name = (i < mixerStrips_.size()) ? mixerStrips_[i].name : ((i < arrangerTracks_.size()) ? arrangerTracks_[i].name : ("Track " + std::to_string(i + 1)));
                float vol = (i < mixerStrips_.size()) ? mixerStrips_[i].volume : ((i < arrangerTracks_.size()) ? arrangerTracks_[i].volume : 0.8f);
                float pan = (i < mixerStrips_.size()) ? mixerStrips_[i].pan : ((i < arrangerTracks_.size()) ? arrangerTracks_[i].pan : 0.0f);
                bool mute = (i < mixerStrips_.size()) ? mixerStrips_[i].mute : ((i < arrangerTracks_.size()) ? arrangerTracks_[i].mute : false);
                bool solo = (i < mixerStrips_.size()) ? mixerStrips_[i].solo : ((i < arrangerTracks_.size()) ? arrangerTracks_[i].solo : false);
                bool freeze = (i < arrangerTracks_.size()) ? arrangerTracks_[i].freeze : ((i < mixerStrips_.size()) ? mixerStrips_[i].freeze : false);
                Color col = (i < arrangerTracks_.size()) ? Color(arrangerTracks_[i].r, arrangerTracks_[i].g, arrangerTracks_[i].b) : Color(0.0f, 0.90f, 1.0f);

                trackNames.push_back(name);
                trackVols.push_back(vol);
                trackPans.push_back(pan);
                trackMutes.push_back(mute);
                trackSolos.push_back(solo);
                trackFreezes.push_back(freeze);
                trackColors.push_back(col);
            }

            modularMixerView_->syncFromWindow(
                trackNames, trackVols, trackPans, trackMutes, trackSolos, trackFreezes, trackColors,
                selectedTrackIndex_, masterVolume_, masterPan_, masterMute_,
                masterPeakL_, masterPeakR_, chPeakL_.data(), chPeakR_.data(), 5,
                mixerPropertiesExpanded_, mixerPropertiesWidth_,
                showMixerRouting_, showMixerPan_, showMixerButtons_,
                showMixerMeters_, showMixerAutomation_, showMixerReadouts_,
                browserOpen_, trackInspectorScrollY_
            );
            modularMixerView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
            modularMixerView_->render(ctx);
        }
    } else if (activeView_ == WorkspaceView::HardwarePanel || activeView_ == WorkspaceView::Track) {
        if (modularTrackInspectorView_) {
            ViewContext ctx = createViewContext();
            float topY = 56.0f;
            float bottomY = bPanelY - 6.0f;

            std::vector<std::string> trackNames;
            std::vector<float> trackVols;
            std::vector<float> trackPans;
            std::vector<bool> trackMutes;
            std::vector<bool> trackSolos;
            std::vector<bool> trackFreezes;
            std::vector<Color> trackColors;

            for (const auto& trk : arrangerTracks_) {
                trackNames.push_back(trk.name);
                trackVols.push_back(trk.volume);
                trackPans.push_back(trk.pan);
                trackMutes.push_back(trk.mute);
                trackSolos.push_back(trk.solo);
                trackFreezes.push_back(trk.freeze);
                trackColors.push_back(Color(trk.r, trk.g, trk.b));
            }

            const auto* preset = getActivePreset();
            std::string pTitle = preset ? preset->guiRoot.title : "TB-303 Acid Bassline";
            std::string pSub = preset ? preset->guiRoot.subtitle : "Diode Ladder Synthesizer";

            modularTrackInspectorView_->syncFromWindow(
                trackNames, trackVols, trackPans, trackMutes, trackSolos, trackFreezes, trackColors,
                selectedTrackIndex_, pTitle, pSub, activePresetIndex_, presets_.size(), trackInspectorScrollY_
            );
            modularTrackInspectorView_->setAudioScopeBuffer(scopeBuffer_, 256);
            modularTrackInspectorView_->layout(Rect2D{0.0f, topY, static_cast<float>(width_), bottomY - topY}, ctx);
            modularTrackInspectorView_->render(ctx);
        }
    }

        // Virtual Piano Keyboard Drawer (Horizontal collapsible drawer docking above bottom nav)
        drawVirtualKeyboardDrawer();

        // =========================================================================
        // 3. BOTTOM HARDWARE NAVIGATION CONTROL STRIP
        // =========================================================================
        const float chinTopY = is3d ? (bPanelY - 14.0f) : bPanelY;
        const float chinTotalH = is3d ? (bPanelH + 14.0f) : bPanelH;
        // Procedural Theme-Tinted Grungy Metal Chin Chassis (matching top panel)
        drawChassisPlate(0.0f, chinTopY, static_cast<float>(width_), chinTotalH, false, theme);

        // Top lip specular highlight line & crevice
        drawLine(0.0f, chinTopY, static_cast<float>(width_), chinTopY, theme.borderSubtle.lighten(0.15f), 1.5f);
        drawLine(0.0f, chinTopY + 1.5f, static_cast<float>(width_), chinTopY + 1.5f, theme.backgroundDark, 1.0f);

        // 5 Primary Navigation Buttons: ARRANGER, EDIT, TRACK, MIXER, DESIGN
        const float totalNavW = static_cast<float>(width_) - 24.0f;
        const float navBtnW = totalNavW / 5.0f;
        struct BottomNavBtn {
            const char* label;
            WorkspaceView view;
            bool isSecondary;
        };
        const BottomNavBtn navButtons[5] = {
            {"ARRANGER", WorkspaceView::Arranger, false},
            {"EDIT",     WorkspaceView::Edit,     true},
            {"TRACK",    WorkspaceView::Track,    false},
            {"MIXER",    WorkspaceView::Mixer,    false},
            {"DESIGN",   WorkspaceView::Design,   true}
        };
        for (int i = 0; i < 5; ++i) {
            float bx = 12.0f + i * navBtnW;
            float bw = navBtnW - 6.0f;
            float bh = 32.0f;
            float by = bPanelY + (bPanelH - bh) * 0.5f;
            bool isAct = (activeView_ == navButtons[i].view);
            const Color& btnAccent = navButtons[i].isSecondary ? theme.secondaryAccent : theme.primaryAccent;

            // Tactile 3D Extruded Mechanical Keycaps with subtle rounded corners (r = 2.5px)
            float pressOff = isAct ? 2.0f : 0.0f;

            // 1. Deep Recessed Shadow Trench (Themed)
            drawRoundedRect(bx - 2.0f, by - 2.0f, bw + 4.0f, bh + 4.0f, 3.5f, theme.controlWell.darken(0.20f));
            drawRoundedRectOutline(bx - 2.0f, by - 2.0f, bw + 4.0f, bh + 4.0f, 3.5f, theme.borderSubtle.darken(0.20f), 1.0f);

            // 2. Extruded Keycap Body Plate (Themed)
            Color capTop = isAct ? theme.controlWell : theme.controlBackground;
            Color capBot = isAct ? theme.controlWell.darken(0.12f) : theme.controlWell;
            drawRoundedRectGradient(bx, by + pressOff, bw, bh - pressOff, 2.5f, capTop, capBot);

            // 3. Consistent Linear Notch / LED Tally Strip along top center edge of keycap
            if (isAct) {
                drawRoundedRect(bx + bw * 0.5f - 16.0f, by + pressOff + 1.5f, 32.0f, 2.5f, 1.0f, btnAccent);
                drawRoundedRect(bx + bw * 0.5f - 18.0f, by + pressOff + 1.0f, 36.0f, 3.5f, 1.5f, Color(btnAccent.r, btnAccent.g, btnAccent.b, 0.30f));
            } else {
                drawRoundedRect(bx + bw * 0.5f - 16.0f, by + 1.5f, 32.0f, 2.5f, 1.0f, theme.controlWell.darken(0.35f));
                drawRoundedRectOutline(bx + bw * 0.5f - 16.0f, by + 1.5f, 32.0f, 2.5f, 1.0f, theme.borderSubtle.darken(0.20f), 0.8f);
            }

            // 4. Engraved Backlit Icon and Text with theme accent radiance
            Color glowCol = isAct ? btnAccent : theme.textSecondary.lighten(0.12f);

            float iconW = 14.0f;
            float gap = 8.0f;
            float textLen = static_cast<float>(std::strlen(navButtons[i].label)) * 7.0f;
            float totalW = iconW + gap + textLen;
            float startX = bx + (bw - totalW) * 0.5f;
            float textY = by + pressOff + 9.0f;

            // Procedural Navigation Icon
            if (navButtons[i].view == WorkspaceView::Arranger) {
                drawIconArranger(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Edit) {
                drawIconEdit(startX, textY - 1.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Track) {
                drawIconTrack(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Mixer) {
                drawIconMixer(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            } else if (navButtons[i].view == WorkspaceView::Design) {
                drawIconDesign(startX, textY - 1.0f, 13.0f, 11.0f, glowCol);
            }

            drawVectorString(navButtons[i].label, startX + iconW + gap, textY, 0.90f, glowCol);

            // 5. Tactile micro-noise overlay applied across keycap, icon, AND text label (shifts on active)
            drawButtonNoise(bx, by + pressOff, bw, bh - pressOff, false, 0.28f, 2.5f, true, true, true, true, isAct, 3.0f);

            // 6. Multi-layer 3D Chamfer Bevels and border outline
            drawLine(bx + 2.0f, by + pressOff + 1.0f, bx + bw - 2.0f, by + pressOff + 1.0f,
                     isAct ? capTop.lighten(0.12f) : capTop.lighten(0.18f), 1.6f);
            drawLine(bx + 1.0f, by + pressOff + 2.0f, bx + 1.0f, by + bh - 2.0f, capTop.lighten(0.08f), 1.0f);
            drawRoundedRectOutline(bx, by + pressOff, bw, bh - pressOff, 2.5f,
                                   isAct ? btnAccent : theme.borderSubtle.darken(0.15f),
                                   isAct ? 1.6f : 1.0f);
        }

        // =========================================================================
        // TOP TRANSPORT HEADER BAR (Rendered after workspace contents so scrolled
        // tracks and containers never draw over top panel chrome)
        // =========================================================================
        drawTopTransportBar();

        // =========================================================================
        // PRESET BROWSER / PATCH LIBRARIAN SLIDING DRAWER OVERLAY
        // =========================================================================
        if (projectBrowserDrawerWidget_ && (browserOpen_ || projectBrowserDrawerWidget_->getAnimOffset() < projectBrowserDrawerWidget_->getDrawerWidth())) {
            if (browserOpen_) {
                // Synchronize live tracks
                std::vector<BrowserTrackAssetItem> bTracks;
                if (engine_) {
                    for (size_t i = 0; i < engine_->getSequencer().getNumTracks(); ++i) {
                        const auto* t = engine_->getSequencer().getTrack(static_cast<uint32_t>(i));
                        if (!t) continue;
                        BrowserTrackAssetItem item;
                        item.index = static_cast<uint32_t>(i);
                        item.name = t->getName();
                        item.type = (i == 1 || i == 2) ? "DRUMS" : "SYNTH";
                        item.clipCount = (i < arrangerTracks_.size()) ? arrangerTracks_[i].clips.size() : 1;
                        item.isMuted = t->isMuted();
                        item.isSolo = t->isSolo();
                        if (i < arrangerTracks_.size()) {
                            item.color = Color(arrangerTracks_[i].r, arrangerTracks_[i].g, arrangerTracks_[i].b, 1.0f);
                        }
                        bTracks.push_back(item);
                    }
                }
                projectBrowserDrawerWidget_->setTracks(bTracks);

                // Synchronize live diff history
                std::vector<BrowserHistoryMilestoneItem> bHist;
                auto timeline = diffHistory_.getTimeline();
                size_t curIdx = diffHistory_.getCurrentTimelineIndex();
                for (size_t i = 0; i < timeline.size(); ++i) {
                    BrowserHistoryMilestoneItem m;
                    m.stepIndex = i;
                    m.description = timeline[i].description;
                    m.category = timeline[i].category;
                    m.isMilestone = timeline[i].isMilestone;
                    m.isCurrent = (i == curIdx);
                    bHist.push_back(m);
                }
                projectBrowserDrawerWidget_->setHistory(bHist, diffHistory_.canUndo(), diffHistory_.canRedo());
                projectBrowserDrawerWidget_->open();
            } else {
                projectBrowserDrawerWidget_->close();
            }

            projectBrowserDrawerWidget_->layout(static_cast<float>(width_), static_cast<float>(height_), 56.0f, 48.0f);
            projectBrowserDrawerWidget_->update(frameDt);
            if (batchRenderer_) {
                projectBrowserDrawerWidget_->render(*batchRenderer_, getTheme());
            }
        }

        // =========================================================================
        // 5. PROJECT HUB & SYSTEM SETTINGS MODAL DIALOG
        // =========================================================================
        if (projectHubOpen_) {
            DialogFrameConfig cfg;
            cfg.width = 540.0f;
            cfg.height = 580.0f;
            cfg.title = "EATSBITS SETTINGS";
            cfg.showLogo = true;
            cfg.showCloseButton = true;
            cfg.showBottomClose = true;
            cfg.cornerRadius = 14.0f;

            DialogLayout dl = drawModalDialogFrame(cfg);
            const float hubX = dl.x;
            const float hubY = dl.y;
            const float hubW = dl.w;
            const float hubH = dl.h;

            // Accordion and Drawer Dimensions
            const float topContentY = hubY + 50.0f;
            const float footerY = hubY + hubH - 36.0f;
            const float headerH = 30.0f;
            const float headerGap = 6.0f;
            const float headerStep = headerH + headerGap; // 36.0f
            const int numSections = 6;
            // Maximum height available for any expanded drawer inside the dialog
            const float availableDrawerH = (footerY - topContentY) - (numSections * headerStep) - headerGap; // 272.0f

            float curDrawerH = 0.0f;
            if (projectHubSection_ >= 0) {
                if (projectHubSection_ == 0) curDrawerH = 190.0f;
                else if (projectHubSection_ == 1) curDrawerH = 114.0f;
                else if (projectHubSection_ == 2) curDrawerH = 244.0f;
                else if (projectHubSection_ == 3) curDrawerH = availableDrawerH;
                else if (projectHubSection_ == 4) curDrawerH = 100.0f;
                else if (projectHubSection_ == 5) curDrawerH = 100.0f;
            }

            const float drawerX = hubX + 16.0f;
            const float drawerW = hubW - 32.0f;
            const float drawerY = topContentY + (projectHubSection_ + 1) * headerStep;
            const float drawerH = curDrawerH;

            // Configure scrollable area constrained to the CRT Shader drawer
            if (projectHubSection_ == 3) {
                projectHubScrollArea_.setViewport(drawerX, drawerY, drawerW, drawerH);
                projectHubScrollArea_.setContentHeight(586.0f);
                projectHubScrollArea_.setScrollY(projectHubScrollY_);
                projectHubMaxScroll_ = projectHubScrollArea_.getMaxScroll();
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            } else {
                projectHubScrollArea_.setViewport(0.0f, 0.0f, 0.0f, 0.0f);
                projectHubScrollArea_.setContentHeight(0.0f);
                projectHubScrollY_ = 0.0f;
            }

            const char* secTitles[6] = {
                "PROJECT HUB",
                "SESSION PERSISTENCE & AUTO-RESTORE",
                "DISPLAY & WORKSPACE",
                "CRT SHADER",
                "AUDIO ENGINE CONFIG",
                "CREDITS & ACKNOWLEDGMENTS"
            };

            for (int s = 0; s < 6; ++s) {
                bool isExp = (projectHubSection_ == s);
                float curY = 0.0f;
                if (projectHubSection_ == -1 || s <= projectHubSection_) {
                    curY = topContentY + s * headerStep;
                } else {
                    curY = drawerY + drawerH + headerGap + (s - (projectHubSection_ + 1)) * headerStep;
                }

                // Section Header Bar [hubX + 12, curY, hubW - 24, 30]
                if (isExp) {
                    drawRoundedRectGradient(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f,
                                            theme.controlWell.darken(0.04f), theme.controlWell.darken(0.12f));
                    drawRoundedRectOutline(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f,
                                           theme.primaryAccent.darken(0.35f), 1.0f);
                    // Small closed down arrow ▼
                    float ax = hubX + hubW - 28.0f;
                    float ay = curY + 15.0f;
                    drawTriangle(ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f,
                                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 1.0f);
                    drawVectorString(secTitles[s], hubX + 44.0f, curY + 8.0f, 0.85f, theme.primaryAccent);
                } else {
                    drawRoundedRect(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f, theme.panelBackground.darken(0.04f));
                    drawRoundedRectOutline(hubX + 12.0f, curY, hubW - 24.0f, 30.0f, 5.0f, theme.borderSubtle.darken(0.20f), 1.0f);
                    // Small closed right arrow ▶
                    float ax = hubX + hubW - 28.0f;
                    float ay = curY + 15.0f;
                    drawTriangle(ax - 3.0f, ay - 4.5f, ax + 3.5f, ay, ax - 3.0f, ay + 4.5f,
                                 theme.textMuted.r, theme.textMuted.g, theme.textMuted.b, 1.0f);
                    drawVectorString(secTitles[s], hubX + 44.0f, curY + 8.0f, 0.85f, theme.textSecondary);
                }

                // Section Icons
                if (s == 0) {
                    drawRect(hubX + 22.0f, curY + 11.0f, 13.0f, 9.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                    drawRect(hubX + 22.0f, curY + 9.0f, 6.0f, 3.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                } else if (s == 1) {
                    drawCircleOutline(hubX + 28.0f, curY + 15.0f, 5.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 28.0f, curY + 12.0f, hubX + 28.0f, curY + 15.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.0f);
                    drawLine(hubX + 28.0f, curY + 15.0f, hubX + 31.0f, curY + 15.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.0f);
                } else if (s == 2) {
                    drawIconEdit(hubX + 22.0f, curY + 10.0f, 10.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                } else if (s == 3) {
                    drawRoundedRectOutline(hubX + 22.0f, curY + 10.0f, 12.0f, 9.0f, 2.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 25.0f, curY + 19.0f, hubX + 31.0f, curY + 19.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                } else if (s == 4) {
                    drawLine(hubX + 22.0f, curY + 15.0f, hubX + 32.0f, curY + 15.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 25.0f, curY + 12.0f, hubX + 25.0f, curY + 18.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 28.0f, curY + 10.0f, hubX + 28.0f, curY + 20.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                } else if (s == 5) {
                    drawCircleOutline(hubX + 28.0f, curY + 15.0f, 5.0f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawLine(hubX + 28.0f, curY + 14.0f, hubX + 28.0f, curY + 17.5f, isExp ? theme.primaryAccent : theme.textSecondary, 1.2f);
                    drawCircle(hubX + 28.0f, curY + 12.0f, 1.0f, isExp ? theme.primaryAccent : theme.textSecondary);
                }

                if (isExp) {
                    // Drawer Container Well
                    drawRoundedRect(drawerX, drawerY, drawerW, drawerH, 6.0f, theme.backgroundDark);
                    drawRoundedRectOutline(drawerX, drawerY, drawerW, drawerH, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);

                    float contentY = drawerY;
                    if (s == 0) {
                        // SECTION 0: PROJECT HUB
                        // Composition Details Outer Card
                        drawRoundedRect(hubX + 20.0f, contentY + 2.0f, hubW - 40.0f, 96.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, contentY + 2.0f, hubW - 40.0f, 96.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);
                        drawVectorString("COMPOSITION DETAILS", hubX + 32.0f, contentY + 10.0f, 0.58f, theme.textMuted);

                        // Title Box
                        drawRoundedRect(hubX + 28.0f, contentY + 22.0f, hubW - 56.0f, 30.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 28.0f, contentY + 22.0f, hubW - 56.0f, 30.0f, 4.0f,
                                               isEditingTitle_ ? theme.primaryAccent : theme.borderSubtle.darken(0.10f),
                                               isEditingTitle_ ? 1.5f : 1.0f);
                        drawVectorString("Title / Song Name", hubX + 36.0f, contentY + 17.0f, 0.50f, theme.textMuted);
                        drawVectorString(projectName_ + (isEditingTitle_ ? "_" : ""), hubX + 36.0f, contentY + 29.0f, 0.90f, theme.textPrimary);

                        // Author Box
                        drawRoundedRect(hubX + 28.0f, contentY + 58.0f, hubW - 56.0f, 30.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 28.0f, contentY + 58.0f, hubW - 56.0f, 30.0f, 4.0f,
                                               isEditingAuthor_ ? theme.primaryAccent : theme.borderSubtle.darken(0.10f),
                                               isEditingAuthor_ ? 1.5f : 1.0f);
                        drawVectorString("Author / Creator", hubX + 36.0f, contentY + 53.0f, 0.50f, theme.textMuted);
                        drawVectorString(authorName_ + (isEditingAuthor_ ? "_" : ""), hubX + 36.0f, contentY + 65.0f, 0.90f, theme.textPrimary);

                        // Action Buttons 2x3 Grid
                        const float btnW = (hubW - 48.0f - 16.0f) / 3.0f;
                        const float row1Y = contentY + 104.0f;
                        const float row2Y = contentY + 144.0f;

                        // [ SAVE (.eats) ]
                        drawRoundedRectGradient(hubX + 24.0f, row1Y, btnW, 34.0f, 4.0f,
                                                theme.primaryAccent.darken(0.65f), theme.primaryAccent.darken(0.80f));
                        drawRoundedRectOutline(hubX + 24.0f, row1Y, btnW, 34.0f, 4.0f, theme.primaryAccent, 1.2f);
                        drawVectorString("SAVE (.eats)", hubX + 44.0f, row1Y + 10.0f, 0.85f, theme.primaryAccent.lighten(0.15f));

                        // [ SAVE AS... ]
                        drawRoundedRectGradient(hubX + 32.0f + btnW, row1Y, btnW, 34.0f, 4.0f,
                                                theme.primaryAccent.darken(0.70f), theme.primaryAccent.darken(0.82f));
                        drawRoundedRectOutline(hubX + 32.0f + btnW, row1Y, btnW, 34.0f, 4.0f, theme.primaryAccent.darken(0.25f), 1.0f);
                        drawVectorString("SAVE AS...", hubX + 54.0f + btnW, row1Y + 10.0f, 0.85f, theme.primaryAccent.lighten(0.15f));

                        // [ LOAD (.eats) ]
                        drawRoundedRectGradient(hubX + 40.0f + 2.0f * btnW, row1Y, btnW, 34.0f, 4.0f,
                                                theme.secondaryAccent.darken(0.65f), theme.secondaryAccent.darken(0.80f));
                        drawRoundedRectOutline(hubX + 40.0f + 2.0f * btnW, row1Y, btnW, 34.0f, 4.0f, theme.secondaryAccent, 1.2f);
                        drawVectorString("LOAD (.eats)", hubX + 62.0f + 2.0f * btnW, row1Y + 10.0f, 0.85f, theme.secondaryAccent);

                        // [ NEW / RESET ]
                        drawRoundedRectGradient(hubX + 24.0f, row2Y, btnW, 34.0f, 4.0f,
                                                Color(0.24f, 0.08f, 0.12f), Color(0.14f, 0.05f, 0.07f));
                        drawRoundedRectOutline(hubX + 24.0f, row2Y, btnW, 34.0f, 4.0f, Color(0.85f, 0.30f, 0.40f), 1.2f);
                        drawVectorString("NEW / RESET", hubX + 44.0f, row2Y + 10.0f, 0.85f, Color(1.0f, 0.45f, 0.55f));

                        // [ BOUNCE WAV ]
                        drawRoundedRectGradient(hubX + 32.0f + btnW, row2Y, btnW, 34.0f, 4.0f,
                                                Color(0.06f, 0.20f, 0.10f), Color(0.03f, 0.11f, 0.05f));
                        drawRoundedRectOutline(hubX + 32.0f + btnW, row2Y, btnW, 34.0f, 4.0f, Color(0.0f, 0.85f, 0.40f), 1.2f);
                        drawVectorString("EXPORT WAV", hubX + 50.0f + btnW, row2Y + 10.0f, 0.85f, Color(0.0f, 0.95f, 0.45f));

                        // [ SCRIPT VIEW ]
                        drawRoundedRectGradient(hubX + 40.0f + 2.0f * btnW, row2Y, btnW, 34.0f, 4.0f,
                                                Color(0.20f, 0.08f, 0.22f), Color(0.11f, 0.04f, 0.12f));
                        drawRoundedRectOutline(hubX + 40.0f + 2.0f * btnW, row2Y, btnW, 34.0f, 4.0f, Color(0.80f, 0.35f, 0.90f), 1.2f);
                        drawVectorString("SCRIPT VIEW", hubX + 58.0f + 2.0f * btnW, row2Y + 10.0f, 0.85f, Color(0.95f, 0.45f, 1.0f));
                    } else if (s == 1) {
                        // SECTION 1: SESSION PERSISTENCE & AUTO-RESTORE
                        drawVectorString("Restore last project on startup", hubX + 28.0f, contentY + 14.0f, 0.85f, theme.textPrimary);
                        drawVectorString("Automatically resumes previous workspace", hubX + 28.0f, contentY + 28.0f, 0.65f, theme.textMuted);
                        // Switch [hubX + hubW - 70, contentY + 12, 44, 20]
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 12.0f, 44.0f, 20.0f, 10.0f,
                                       autoRestoreSession_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 12.0f, 44.0f, 20.0f, 10.0f,
                                              autoRestoreSession_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(autoRestoreSession_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 22.0f, 7.0f,
                                   autoRestoreSession_ ? theme.primaryAccent : theme.textMuted);

                        // Row 2: Auto-save project state
                        drawLine(hubX + 24.0f, contentY + 36.0f, hubX + hubW - 24.0f, contentY + 36.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("Auto-save project state snapshot", hubX + 28.0f, contentY + 46.0f, 0.85f, theme.textPrimary);
                        drawVectorString("Silently writes .eats state to disk every 60s", hubX + 28.0f, contentY + 60.0f, 0.65f, theme.textMuted);
                        // Switch
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 44.0f, 44.0f, 20.0f, 10.0f,
                                       autoSaveEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 44.0f, 44.0f, 20.0f, 10.0f,
                                              autoSaveEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(autoSaveEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 54.0f, 7.0f,
                                   autoSaveEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // Row 3: Reset to clean slate button
                        drawRoundedRect(hubX + 24.0f, contentY + 74.0f, hubW - 48.0f, 30.0f, 4.0f, theme.controlWell);
                        drawRoundedRectOutline(hubX + 24.0f, contentY + 74.0f, hubW - 48.0f, 30.0f, 4.0f, theme.borderSubtle, 1.0f);
                        drawVectorString("RESET TO DEFAULT TEMPLATE (CLEAN SLATE)", hubX + 110.0f, contentY + 82.0f, 0.80f, theme.textSecondary);
                    } else if (s == 2) {
                        // SECTION 2: DISPLAY & WORKSPACE
                        // UI Magnification / Scale Chips
                        drawVectorString("UI MAGNIFICATION / SCALE FACTOR", hubX + 28.0f, contentY + 10.0f, 0.65f, theme.textMuted);
                        const float scales[5] = {1.0f, 1.10f, 1.25f, 1.50f, 2.0f};
                        const char* scaleLabels[5] = {"100%", "110%", "125%", "150%", "200%"};
                        for (int sc = 0; sc < 5; ++sc) {
                            float cx = hubX + 24.0f + sc * 64.0f;
                            bool isAct = (std::abs(uiScale_ - scales[sc]) < 0.02f);
                            drawRoundedRect(cx, contentY + 24.0f, 58.0f, 24.0f, 4.0f,
                                           isAct ? theme.controlWell : theme.controlBackground);
                            drawRoundedRectOutline(cx, contentY + 24.0f, 58.0f, 24.0f, 4.0f,
                                                  isAct ? theme.primaryAccent : theme.borderSubtle, isAct ? 1.5f : 1.0f);
                            drawVectorString(scaleLabels[sc], cx + 12.0f, contentY + 29.0f, 0.8f,
                                             isAct ? theme.primaryAccent : theme.textSecondary);
                        }

                        // Theme Engine Chips (5 Curated Eatsbeats Presets)
                        drawVectorString("UI THEME ENGINE PALETTE (EATSBEATS CONSOLES)", hubX + 28.0f, contentY + 54.0f, 0.65f, theme.textMuted);
                        const char* themeLabels[5] = {"ATE TRACK", "MIDNIGHT", "LT SNACK", "BREAKFAST", "DINNER"};
                        for (int th = 0; th < 5; ++th) {
                            float tx = hubX + 24.0f + th * 95.0f;
                            bool isAct = (activeThemePreset_ == th);
                            const auto& thToken = Theme::get(static_cast<Theme::Preset>(th));
                            drawRoundedRect(tx, contentY + 68.0f, 88.0f, 24.0f, 4.0f,
                                           isAct ? thToken.primaryAccent.darken(0.45f) : theme.controlBackground);
                            drawRoundedRectOutline(tx, contentY + 68.0f, 88.0f, 24.0f, 4.0f,
                                                  isAct ? thToken.primaryAccent : theme.borderSubtle, isAct ? 1.5f : 1.0f);
                            drawCircle(tx + 10.0f, contentY + 80.0f, 4.0f, thToken.primaryAccent);
                            drawVectorString(themeLabels[th], tx + 18.0f, contentY + 73.0f, 0.68f,
                                             isAct ? Color(1.0f, 1.0f, 1.0f) : theme.textSecondary);
                        }

                        // Anti-Aliasing Mode Chips (No AA 1x, 2x Fast, 4x RGSS)
                        drawLine(hubX + 24.0f, contentY + 98.0f, hubX + hubW - 24.0f, contentY + 98.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("ANTI-ALIASING (SUBPIXEL EDGE QUALITY)", hubX + 28.0f, contentY + 105.0f, 0.65f, theme.textMuted);
                        const char* aaLabels[3] = {"NO AA (1X)", "2X FAST", "4X RGSS"};
                        const float aaWidths[3] = {100.0f, 100.0f, 110.0f};
                        const float aaOffsets[3] = {0.0f, 108.0f, 216.0f};
                        for (int aa = 0; aa < 3; ++aa) {
                            float ax = hubX + 24.0f + aaOffsets[aa];
                            bool isAct = (antiAliasingMode_ == aa);
                            drawRoundedRect(ax, contentY + 118.0f, aaWidths[aa], 24.0f, 4.0f,
                                           isAct ? theme.controlWell : theme.controlBackground);
                            drawRoundedRectOutline(ax, contentY + 118.0f, aaWidths[aa], 24.0f, 4.0f,
                                                  isAct ? theme.primaryAccent : theme.borderSubtle, isAct ? 1.5f : 1.0f);
                            drawVectorString(aaLabels[aa], ax + 14.0f, contentY + 123.0f, 0.78f,
                                             isAct ? theme.primaryAccent : theme.textSecondary);
                        }

                        // HiDPI Canvas Resolution Switch
                        drawLine(hubX + 24.0f, contentY + 150.0f, hubX + hubW - 24.0f, contentY + 150.0f, theme.borderSubtle.darken(0.15f), 1.0f);
                        drawVectorString("Native 1:1 HiDPI Resolution (Retina/4K)", hubX + 28.0f, contentY + 160.0f, 0.85f, theme.textPrimary);
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 156.0f, 44.0f, 20.0f, 10.0f,
                                       hiDpiEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 156.0f, 44.0f, 20.0f, 10.0f,
                                              hiDpiEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(hiDpiEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 166.0f, 7.0f,
                                   hiDpiEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // CRT Curved Shaders & Tweaker HUD Button
                        drawVectorString("Screen Shaders & CRT Glass Curvature", hubX + 28.0f, contentY + 186.0f, 0.85f, theme.textPrimary);
                        // [TWEAK HUD] button
                        drawRoundedRect(hubX + hubW - 165.0f, contentY + 180.0f, 82.0f, 22.0f, 4.0f, theme.controlBackground);
                        drawRoundedRectOutline(hubX + hubW - 165.0f, contentY + 180.0f, 82.0f, 22.0f, 4.0f, theme.primaryAccent, 1.0f);
                        drawVectorString("TWEAK HUD", hubX + hubW - 158.0f, contentY + 185.0f, 0.65f, theme.primaryAccent);
                        // CRT toggle pill
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 182.0f, 44.0f, 20.0f, 10.0f,
                                       crtShaderEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 182.0f, 44.0f, 20.0f, 10.0f,
                                              crtShaderEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(crtShaderEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 192.0f, 7.0f,
                                   crtShaderEnabled_ ? theme.primaryAccent : theme.textMuted);

                        // GUI Animations toggle
                        drawVectorString("Real-time Oscilloscopes & Ticker Animations", hubX + 28.0f, contentY + 212.0f, 0.85f, theme.textPrimary);
                        drawRoundedRect(hubX + hubW - 70.0f, contentY + 208.0f, 44.0f, 20.0f, 10.0f,
                                       guiAnimationsEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                        drawRoundedRectOutline(hubX + hubW - 70.0f, contentY + 208.0f, 44.0f, 20.0f, 10.0f,
                                              guiAnimationsEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                        drawCircle(guiAnimationsEnabled_ ? (hubX + hubW - 38.0f) : (hubX + hubW - 58.0f), contentY + 218.0f, 7.0f,
                                   guiAnimationsEnabled_ ? theme.primaryAccent : theme.textMuted);
                    } else if (s == 3) {
                        // SECTION 3: CRT SHADER (Scrollable within constrained drawer space)
                        const float contentBaseY = drawerY - projectHubScrollY_;

                        // Master Toggle Row
                        float rowMasterY = contentBaseY + 6.0f;
                        if (projectHubScrollArea_.isVisible(rowMasterY, 26.0f) && rowMasterY >= drawerY - 4.0f && rowMasterY + 26.0f <= drawerY + drawerH + 4.0f) {
                            drawVectorString("CRT Screen Shader & Curvature Engine", drawerX + 12.0f, rowMasterY + 6.0f, 0.85f, theme.textPrimary);

                            // [LIVE HUD (F10)] Button
                            drawRoundedRect(drawerX + drawerW - 180.0f, rowMasterY, 108.0f, 22.0f, 4.0f, theme.controlBackground);
                            drawRoundedRectOutline(drawerX + drawerW - 180.0f, rowMasterY, 108.0f, 22.0f, 4.0f, theme.primaryAccent, 1.0f);
                            drawVectorString("LIVE HUD (F10)", drawerX + drawerW - 173.0f, rowMasterY + 5.0f, 0.65f, theme.primaryAccent);

                            // CRT Toggle Switch
                            drawRoundedRect(drawerX + drawerW - 60.0f, rowMasterY + 1.0f, 44.0f, 20.0f, 10.0f,
                                           crtShaderEnabled_ ? theme.primaryAccent.darken(0.3f) : theme.controlWell);
                            drawRoundedRectOutline(drawerX + drawerW - 60.0f, rowMasterY + 1.0f, 44.0f, 20.0f, 10.0f,
                                                  crtShaderEnabled_ ? theme.primaryAccent : theme.borderSubtle, 1.0f);
                            drawCircle(crtShaderEnabled_ ? (drawerX + drawerW - 28.0f) : (drawerX + drawerW - 48.0f), rowMasterY + 11.0f, 7.0f,
                                       crtShaderEnabled_ ? theme.primaryAccent : theme.textMuted);
                        }

                        // Presets Row
                        float prY = contentBaseY + 36.0f;
                        if (projectHubScrollArea_.isVisible(prY, 32.0f) && prY >= drawerY - 4.0f && prY + 32.0f <= drawerY + drawerH + 4.0f) {
                            drawLine(drawerX + 12.0f, prY, drawerX + drawerW - 12.0f, prY, theme.borderSubtle.darken(0.15f), 1.0f);
                            drawVectorString("PRESETS:", drawerX + 12.0f, prY + 9.0f, 0.65f, theme.primaryAccent);

                            const char* prLabels[4] = {"STUDIO REF", "MAX CLARITY", "WARM VINTAGE", "RESET"};
                            const float prWidths[4] = {92.0f, 98.0f, 102.0f, 68.0f};
                            float prOffsets[4] = {74.0f, 172.0f, 276.0f, 384.0f};
                            for (int p = 0; p < 4; ++p) {
                                float px = drawerX + prOffsets[p];
                                drawRoundedRect(px, prY + 5.0f, prWidths[p], 20.0f, 4.0f, theme.controlBackground);
                                drawRoundedRectOutline(px, prY + 5.0f, prWidths[p], 20.0f, 4.0f, theme.borderSubtle, 1.0f);
                                drawVectorString(prLabels[p], px + 8.0f, prY + 9.0f, 0.65f, theme.textSecondary);
                            }
                        }

                        // Divider
                        float divY = contentBaseY + 68.0f;
                        if (divY >= drawerY && divY <= drawerY + drawerH) {
                            drawLine(drawerX + 12.0f, divY, drawerX + drawerW - 12.0f, divY, theme.borderSubtle.darken(0.15f), 1.0f);
                        }

                        const auto& crtCfg = dawnBridge_.getMaterialConfig();
                        char bufSoftness[32];
                        snprintf(bufSoftness, sizeof(bufSoftness), (crtCfg.panelSoftness <= 0.01f) ? "0.00 px (SHARP)" : "%.2f px", crtCfg.panelSoftness);
                        char bufBlackLift[32];
                        snprintf(bufBlackLift, sizeof(bufBlackLift), (crtCfg.panelBlackLift <= 0.001f) ? "0.00 (PITCH BLACK)" : "+%.3f", crtCfg.panelBlackLift);

                        struct HubSlider {
                            std::string label;
                            float minVal;
                            float maxVal;
                            float curVal;
                            std::string valStr;
                            bool isGreen;
                        };

                        HubSlider hSliders[11] = {
                            {"SCANLINE INTENSITY (0 = NONE)", 0.0f, 1.0f, crtCfg.scanlineIntensity,
                             (crtCfg.scanlineIntensity <= 0.005f) ? "0.00 (OFF / CRISP)" : (std::to_string(static_cast<int>(std::round(crtCfg.scanlineIntensity * 100.0f))) + "%"),
                             (crtCfg.scanlineIntensity <= 0.005f)},
                            {"CRT BULB CURVATURE", 0.0f, 1.50f, crtCfg.curvature,
                             (crtCfg.curvature <= 0.01f) ? "0.00 (FLAT GLASS)" : (std::to_string(static_cast<int>(std::round(crtCfg.curvature * 100.0f))) + "%"), false},
                            {"CRT TUBE ROOM REFLECTION", 0.0f, 1.0f, crtCfg.crtReflectionLevel,
                             (crtCfg.crtReflectionLevel <= 0.005f) ? "0.00 (OFF / NONE)" : (std::to_string(static_cast<int>(std::round(crtCfg.crtReflectionLevel * 100.0f))) + "%"),
                             (crtCfg.crtReflectionLevel <= 0.005f)},
                            {"H-SYNC WAVE DISTORTION", 0.0f, 2.0f, crtCfg.hsyncDistortion,
                             (crtCfg.hsyncDistortion <= 0.005f) ? "0.00 (ZERO / STATIC)" : (std::to_string(static_cast<int>(std::round(crtCfg.hsyncDistortion * 100.0f))) + "%"),
                             (crtCfg.hsyncDistortion <= 0.005f)},
                            {"SPOTLIGHT INTENSITY", 0.0f, 2.0f, crtCfg.spotlightIntensity,
                             (crtCfg.spotlightIntensity <= 0.01f) ? "0.00 (FLAT LIGHT)" : (std::to_string(static_cast<int>(std::round(crtCfg.spotlightIntensity * 100.0f))) + "%"), false},
                            {"SPOTLIGHT BEAM SIZE", 0.5f, 2.5f, crtCfg.spotlightSize,
                             std::to_string(static_cast<int>(std::round(crtCfg.spotlightSize * 100.0f))) + "%", false},
                            {"VIGNETTE CORNER FALLOFF", 0.0f, 2.0f, crtCfg.vignetteStrength,
                             (crtCfg.vignetteStrength <= 0.01f) ? "0.00 (NONE)" : (std::to_string(static_cast<int>(std::round(crtCfg.vignetteStrength * 100.0f))) + "%"), false},
                            {"BEZEL FRAME REFLECTION", 0.0f, 1.0f, crtCfg.reflectionOpacity,
                             (crtCfg.reflectionOpacity <= 0.01f) ? "0.00 (MATTE)" : (std::to_string(static_cast<int>(std::round(crtCfg.reflectionOpacity * 100.0f))) + "%"), false},
                            {"PANEL SOFTNESS (SUBPIXEL BLUR)", 0.0f, 1.50f, crtCfg.panelSoftness, bufSoftness, false},
                            {"PANEL HARDWARE SATURATION", 0.0f, 1.0f, crtCfg.panelSaturation,
                             std::to_string(static_cast<int>(std::round(crtCfg.panelSaturation * 100.0f))) + "%", false},
                            {"PANEL BLACK FLOOR LIFT", 0.0f, 0.08f, crtCfg.panelBlackLift, bufBlackLift, false}
                        };

                        const float sTrackX = drawerX + 12.0f;
                        const float sTrackW = drawerW - 32.0f;
                        const float sTrackH = 5.0f;

                        for (int i = 0; i < 11; ++i) {
                            float rowY = contentBaseY + 76.0f + i * 46.0f;
                            if (!projectHubScrollArea_.isVisible(rowY, 46.0f) || rowY < drawerY - 4.0f || rowY + 36.0f > drawerY + drawerH + 4.0f) continue;
                            float trackY = rowY + 18.0f;

                            // Label
                            drawVectorString(hSliders[i].label, sTrackX, rowY + 2.0f, 0.65f, theme.textPrimary);

                            // Value readout
                            float valW = static_cast<float>(hSliders[i].valStr.size()) * 7.0f;
                            drawVectorString(hSliders[i].valStr, sTrackX + sTrackW - valW, rowY + 2.0f, 0.68f,
                                             hSliders[i].isGreen ? theme.playActive : theme.primaryAccent);

                            // Track Background
                            drawRoundedRect(sTrackX, trackY, sTrackW, sTrackH, 2.5f, theme.controlWell);
                            drawRoundedRectOutline(sTrackX, trackY, sTrackW, sTrackH, 2.5f, theme.borderSubtle.darken(0.20f), 1.0f);

                            // Fill bar
                            float t = std::clamp((hSliders[i].curVal - hSliders[i].minVal) / (hSliders[i].maxVal - hSliders[i].minVal), 0.0f, 1.0f);
                            float fillW = t * sTrackW;
                            if (fillW > 2.0f) {
                                drawRoundedRectGradient(sTrackX, trackY, fillW, sTrackH, 2.5f,
                                                        theme.primaryAccent.darken(0.20f), theme.primaryAccent);
                            }

                            // Thumb
                            float thumbX = sTrackX + fillW;
                            float thumbY = trackY + 2.5f;
                            drawCircle(thumbX, thumbY, 7.0f, theme.backgroundDark);
                            drawCircle(thumbX, thumbY, 5.5f, theme.primaryAccent);
                            drawCircleOutline(thumbX, thumbY, 5.5f, theme.textPrimary, 1.2f);
                        }

                        // Render scrollbar constrained inside the drawer
                        if (projectHubScrollArea_.canScroll() && batchRenderer_) {
                            projectHubScrollArea_.renderScrollbar(*batchRenderer_, theme);
                        }
                    } else if (s == 4) {
                        // SECTION 4: AUDIO ENGINE CONFIG
                        drawRoundedRect(hubX + 20.0f, contentY + 4.0f, hubW - 40.0f, 92.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, contentY + 4.0f, hubW - 40.0f, 92.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);
#if defined(__EMSCRIPTEN__)
                        drawVectorString("• Hardware Driver: WebAssembly AudioWorklet / WebGPU Native", hubX + 32.0f, contentY + 14.0f, 0.75f, theme.textPrimary);
#elif defined(_WIN32)
                        drawVectorString("• Hardware Driver: WASAPI Exclusive Low-Latency (miniaudio)", hubX + 32.0f, contentY + 14.0f, 0.75f, theme.textPrimary);
#else
                        drawVectorString("• Hardware Driver: ALSA / CoreAudio Low-Latency (miniaudio)", hubX + 32.0f, contentY + 14.0f, 0.75f, theme.textPrimary);
#endif
                        drawVectorString("• Sample Rate: 48,000 Hz Hardware Native / Stereo Floating-Point", hubX + 32.0f, contentY + 30.0f, 0.75f, theme.textSecondary);
                        drawVectorString("• Buffer Latency: 128 frames (~2.67 ms) / Strict Zero-Allocation Audio Loop", hubX + 32.0f, contentY + 46.0f, 0.75f, theme.textSecondary);
                        drawVectorString("• Script Engine: Dual-Mode Eatscript VM & SIMD AOT Transpiler (Pure C++20)", hubX + 32.0f, contentY + 62.0f, 0.75f, theme.textSecondary);
                        drawVectorString("• Lock-Free Concurrency: SPSC Wait-Free Event & Meter Ringbuffers", hubX + 32.0f, contentY + 78.0f, 0.75f, theme.textSecondary);
                    } else if (s == 5) {
                        // SECTION 5: CREDITS & ACKNOWLEDGMENTS
                        drawRoundedRect(hubX + 20.0f, contentY + 4.0f, hubW - 40.0f, 92.0f, 6.0f, theme.backgroundDark);
                        drawRoundedRectOutline(hubX + 20.0f, contentY + 4.0f, hubW - 40.0f, 92.0f, 6.0f, theme.borderSubtle.darken(0.18f), 1.0f);
                        drawVectorString("• Stanford CCRMA / Bank-Bensa commuted waveguide piano, bass & guitar models.", hubX + 32.0f, contentY + 14.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Roland TB-303 diode ladder acid core & analog 60ms slide modeling.", hubX + 32.0f, contentY + 30.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Vintage Soundchips: Commodore 64 MOS 6581 SID, SPC700, Yamaha YM2612 & DX7.", hubX + 32.0f, contentY + 46.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Studio FX: Convolution Reverb procedural IR simulator & 5-band parametric EQ.", hubX + 32.0f, contentY + 62.0f, 0.70f, theme.textPrimary);
                        drawVectorString("• Built with pure C++20, miniaudio, Google Filament, and NanoVG vector graphics.", hubX + 32.0f, contentY + 78.0f, 0.70f, theme.textPrimary);
                    }
                }
            }

            // Clean caps above and below drawer to prevent any subpixel bleed
            if (projectHubSection_ >= 0) {
                drawRect(hubX + 12.0f, drawerY - headerGap, hubW - 24.0f, headerGap, theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 1.0f);
                drawRect(hubX + 12.0f, drawerY + drawerH, hubW - 24.0f, headerGap, theme.panelBackground.r, theme.panelBackground.g, theme.panelBackground.b, 1.0f);
            }

            // Status message at bottom of modal
            if (!lastStatusMessage_.empty()) {
                drawVectorString(lastStatusMessage_, hubX + 24.0f, hubY + hubH - 18.0f, 0.75f, theme.playActive);
            }
        }

        // 6. REUSABLE VALUE EDIT MODAL DIALOG
        if (valueEditDialog_.isOpen()) {
            valueEditDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                valueEditDialog_.render(*batchRenderer_, theme);
            }
        }

        // 6a. STACKED ICON SEARCH MODAL DIALOG (stacked above value edit dialog)
        if (valueEditDialog_.isOpen() && modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            modularArrangerView_->getIconSearchDialog().layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                modularArrangerView_->getIconSearchDialog().render(*batchRenderer_, theme);
            }
        }

        // 6b. AUDIO TO MIDI TRANSCRIPTION MODAL DIALOG
        if (audioToMidiDialog_.isOpen()) {
            audioToMidiDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            audioToMidiDialog_.update(frameDt);
            if (batchRenderer_) {
                audioToMidiDialog_.render(*batchRenderer_, theme);
            }
        }

        // 7. UNIVERSAL QUICK COMMAND PALETTE DIALOG (Ctrl+P / Ctrl+K / Search)
        if (commandPaletteDialog_.isOpen()) {
            commandPaletteDialog_.layout(static_cast<float>(width_), static_cast<float>(height_));
            commandPaletteDialog_.update(frameDt);
            if (batchRenderer_) {
                commandPaletteDialog_.render(*batchRenderer_, theme);
            }
        }

        // 7b. REUSABLE MODAL PLUGIN / FX SEARCH DIALOG (Full-Screen Backdrop)
        PluginSearchDialog* activePluginDialog = nullptr;
        if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
            if (modularArrangerView_->getPluginSearchDialog().isOpen()) {
                activePluginDialog = &modularArrangerView_->getPluginSearchDialog();
            } else if (modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
                activePluginDialog = &modularArrangerView_->getPropertiesDrawer().getPluginSearchDialog();
            }
        } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
            if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
                activePluginDialog = &modularMixerView_->getPropertiesDrawer().getPluginSearchDialog();
            }
        }

        if (activePluginDialog && activePluginDialog->isOpen()) {
            activePluginDialog->layout(static_cast<float>(width_), static_cast<float>(height_));
            if (batchRenderer_) {
                activePluginDialog->render(*batchRenderer_, theme);
            }
        }

        // 8. CRT SHADER & CHASSIS TWEAKER HUD MODAL
        if (crtTweakerOpen_) {
            renderCrtTweakerModal();
        }

        // Persistent Floating Status Toast Overlay (F10 / F11 / Action Feedback)
        if (statusToastTimer_ > 0.0f && !statusToastText_.empty()) {
            statusToastTimer_ -= (1.0f / 60.0f);
            float alpha = std::clamp(statusToastTimer_ / 0.4f, 0.0f, 1.0f);
            float pillW = std::max(280.0f, static_cast<float>(statusToastText_.size()) * 8.5f + 48.0f);
            float pillH = 34.0f;
            float pillX = (static_cast<float>(width_) - pillW) * 0.5f;
            float pillY = 16.0f;

            drawRect(pillX - 2.0f, pillY - 2.0f, pillW + 4.0f, pillH + 4.0f, 0.0f, 0.0f, 0.0f, 0.50f * alpha);
            drawRectGradient(pillX, pillY, pillW, pillH, 0.14f, 0.16f, 0.20f, 0.08f, 0.09f, 0.12f, 0.96f * alpha);
            drawRectOutline(pillX, pillY, pillW, pillH, 1.0f, 0.65f, 0.10f, 0.90f * alpha, 1.5f);
            drawCircle(pillX + 16.0f, pillY + 17.0f, 4.0f, 1.0f, 0.65f, 0.10f, alpha);
            drawVectorString(statusToastText_, pillX + 28.0f, pillY + 11.0f, 0.88f, 0.95f, 0.96f, 1.0f, alpha);
        }

        if (batchRenderer_) {
            batchRenderer_->endFrame();
        }
        g_activeBatchRenderer = nullptr;
        g_activeFontRenderer = nullptr;
        ScoreVectorGlyphs::setBatchRenderer(nullptr);
    }
#endif

    // Submit frame to Google Dawn / WebGPU Bridge
    if (dawnBridge_.isNativeActive()) {
        const uint32_t* dawPixels = batchRenderer_ ? batchRenderer_->getFramebuffer() : nullptr;
        dawnBridge_.setCrtShaderEnabled(crtShaderEnabled_);
        uint32_t physW = static_cast<uint32_t>(std::round(static_cast<float>(width_) * renderScale_));
        uint32_t physH = static_cast<uint32_t>(std::round(static_cast<float>(height_) * renderScale_));
        float subBass = engine_ ? engine_->getSubBassEnergy() : 0.0f;
        dawnBridge_.renderCrtScene(dawPixels, physW, physH, lampTime_, subBass, renderScale_);
    } else {
        dawnBridge_.beginFrame(static_cast<float>(width_), static_cast<float>(height_), 1.0f);
        dawnBridge_.endFrame();
    }
}

DialogLayout GuiWindow::computeDialogLayout(float w, float h) const noexcept {
    DialogLayout dl{};
    dl.w = std::min(w, static_cast<float>(width_) - 24.0f);
    dl.h = std::min(h, static_cast<float>(height_) - 24.0f);
    dl.x = std::max(12.0f, (static_cast<float>(width_) - dl.w) * 0.5f);
    dl.y = std::max(12.0f, (static_cast<float>(height_) - dl.h) * 0.5f);
    dl.contentX = dl.x + 16.0f;
    dl.contentY = dl.y + 48.0f;
    dl.contentW = dl.w - 32.0f;
    dl.contentH = std::max(80.0f, dl.h - 86.0f);
    dl.closeBtnW = 24.0f;
    dl.closeBtnH = 24.0f;
    dl.closeBtnX = dl.x + dl.w - 32.0f;
    dl.closeBtnY = dl.y + 12.0f;
    dl.bottomCloseW = 60.0f;
    dl.bottomCloseH = 22.0f;
    dl.bottomCloseX = dl.x + dl.w - dl.bottomCloseW - 20.0f;
    dl.bottomCloseY = dl.y + dl.h - dl.bottomCloseH - 12.0f;
    return dl;
}

DialogLayout GuiWindow::drawModalDialogFrame(const DialogFrameConfig& config) {
    const auto& theme = getTheme();
    DialogLayout dl = computeDialogLayout(config.width, config.height);

    // Soft dimming backdrop (GPU blur disabled by default for maximum 60+ FPS performance)
    if (config.enableBlur && batchRenderer_) {
        batchRenderer_->applyBackdropBlur(4.0f, 0.48f);
    }

    // 1. Semi-transparent backdrop shadow
    drawRect(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 0.0f, 0.0f, 0.45f);

    // 2. Dialog Chassis with softly rounded corners
    drawRoundedRect(dl.x - 2.0f, dl.y - 2.0f, dl.w + 4.0f, dl.h + 4.0f, config.cornerRadius + 1.0f, Color(0.0f, 0.0f, 0.0f, 0.40f));
    drawRoundedRectGradient(dl.x, dl.y, dl.w, dl.h, config.cornerRadius,
                            theme.panelBackground, theme.panelBackground.darken(0.06f));
    drawRoundedRectOutline(dl.x, dl.y, dl.w, dl.h, config.cornerRadius,
                           theme.borderSubtle.darken(0.12f), 1.2f);

    // 3. Top Header: Eatsbits Logo + Title in theme.primaryAccent
    float textStartX = dl.x + 22.0f;
    if (config.showLogo) {
        drawEatsbitsLogo(dl.x + 22.0f, dl.y + 16.0f, 22.0f, 1.0f, false);
        textStartX += 32.0f;
    }
    drawVectorString(config.title, textStartX, dl.y + 20.0f, 1.0f, theme.primaryAccent);

    // 4. Close Button [Top-Right]
    if (config.showCloseButton) {
        float hClX = dl.closeBtnX + dl.closeBtnW * 0.5f;
        float hClY = dl.closeBtnY + dl.closeBtnH * 0.5f;
        bool hClHov = (mouseX_ >= dl.closeBtnX - 4.0f && mouseX_ <= dl.closeBtnX + dl.closeBtnW + 4.0f &&
                       mouseY_ >= dl.closeBtnY - 4.0f && mouseY_ <= dl.closeBtnY + dl.closeBtnH + 4.0f) ||
                      (std::hypot(mouseX_ - hClX, mouseY_ - hClY) <= 13.0f);
        drawIconScrewClose(hClX, hClY, 9.0f, hClHov, theme.primaryAccent);
    }

    // 5. Close Button [Bottom-Right]
    if (config.showBottomClose) {
        bool bClHov = (mouseX_ >= dl.bottomCloseX && mouseX_ <= dl.bottomCloseX + dl.bottomCloseW &&
                       mouseY_ >= dl.bottomCloseY && mouseY_ <= dl.bottomCloseY + dl.bottomCloseH);
        drawVectorString("CLOSE", dl.bottomCloseX + 6.0f, dl.bottomCloseY + 5.0f, 0.85f,
                         bClHov ? theme.primaryAccent.lighten(0.2f) : theme.primaryAccent);
    }

    return dl;
}

void GuiWindow::initFontRenderer() {
#if EATS_HAS_GLFW
    fontRenderer_ = std::make_unique<FontRendererState>();

    FONSparams params;
    std::memset(&params, 0, sizeof(params));
    params.width = 1024;
    params.height = 1024;
    params.flags = FONS_ZERO_TOPLEFT;
    params.renderCreate = glfons__renderCreate;
    params.renderResize = glfons__renderResize;
    params.renderUpdate = glfons__renderUpdate;
    params.renderDraw = glfons__renderDraw;
    params.renderDelete = glfons__renderDelete;
    params.userPtr = fontRenderer_.get();

    fontRenderer_->fs = fonsCreateInternal(&params);
    if (!fontRenderer_->fs) {
        std::cerr << "[GuiWindow] Warning: Could not initialize FontStash context" << std::endl;
        fontRenderer_.reset();
        return;
    }

    // Prioritize Oswald for primary UI typography
    std::vector<std::string> searchPaths;
    if (!pendingFontPath_.empty()) {
        searchPaths.push_back(pendingFontPath_);
    }
    searchPaths.push_back("assets/fonts/Oswald-Medium.ttf");
    searchPaths.push_back("assets/fonts/Oswald-Regular.ttf");
    searchPaths.push_back("assets/fonts/Oswald-Bold.ttf");
    searchPaths.push_back("../assets/fonts/Oswald-Medium.ttf");
    searchPaths.push_back("../../assets/fonts/Oswald-Medium.ttf");
    searchPaths.push_back("c:/git/eatsbits/assets/fonts/Oswald-Medium.ttf");

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            std::string fName = pendingFontName_.empty() ? "ui" : pendingFontName_;
            if (loadFont(path, fName)) {
                break;
            }
        }
    }

    // Fallback UI font (e.g. Oswald-Regular if Oswald-Medium is primary)
    if (fontRenderer_->fontNormal != FONS_INVALID) {
        const std::vector<std::string> fallbackPaths = {
            "assets/fonts/Oswald-Regular.ttf",
            "../assets/fonts/Oswald-Regular.ttf",
            "../../assets/fonts/Oswald-Regular.ttf",
            "c:/git/eatsbits/assets/fonts/Oswald-Regular.ttf"
        };
        for (const auto& fbPath : fallbackPaths) {
            if (std::filesystem::exists(fbPath) && fbPath != fontRenderer_->loadedFontPath) {
                int fbId = fonsAddFont(fontRenderer_->fs, "fallback", fbPath.c_str(), 0);
                if (fbId != FONS_INVALID) {
                    fonsAddFallbackFont(fontRenderer_->fs, fontRenderer_->fontNormal, fbId);
                    fontRenderer_->fontFallback = fbId;
                    std::cout << "[GuiWindow] Loaded fallback font from " << fbPath << std::endl;
                    break;
                }
            }
        }
    }

    // Load Share Tech Mono for code editor and digital metrics readout
    std::vector<std::string> monoSearchPaths;
    if (!pendingMonoFontPath_.empty()) {
        monoSearchPaths.push_back(pendingMonoFontPath_);
    }
    monoSearchPaths.push_back("assets/fonts/ShareTechMono-Regular.ttf");
    monoSearchPaths.push_back("../assets/fonts/ShareTechMono-Regular.ttf");
    monoSearchPaths.push_back("../../assets/fonts/ShareTechMono-Regular.ttf");
    monoSearchPaths.push_back("c:/git/eatsbits/assets/fonts/ShareTechMono-Regular.ttf");

    for (const auto& mPath : monoSearchPaths) {
        if (std::filesystem::exists(mPath)) {
            std::string mfName = pendingMonoFontName_.empty() ? "mono" : pendingMonoFontName_;
            if (loadMonoFont(mPath, mfName)) {
                break;
            }
        }
    }
#endif
}

bool GuiWindow::loadFont(const std::string& fontPath, const std::string& fontName) {
    activeFontName_ = fontName;
    activeFontPath_ = fontPath;

    if (!fontRenderer_ || !fontRenderer_->fs) {
        pendingFontPath_ = fontPath;
        pendingFontName_ = fontName;
        return false;
    }

    if (!std::filesystem::exists(fontPath)) {
        std::cerr << "[GuiWindow] Error: Font file not found at " << fontPath << std::endl;
        return false;
    }

    int fid = fonsAddFont(fontRenderer_->fs, fontName.c_str(), fontPath.c_str(), 0);
    if (fid == FONS_INVALID) {
        std::cerr << "[GuiWindow] Error: Failed to parse font file " << fontPath << std::endl;
        return false;
    }

    fontRenderer_->fontNormal = fid;
    fontRenderer_->loadedFontPath = fontPath;
    std::cout << "[GuiWindow] Successfully loaded font '" << fontName << "' (" << fontPath << ")" << std::endl;
    return true;
}

bool GuiWindow::loadMonoFont(const std::string& fontPath, const std::string& fontName) {
    activeMonoFontName_ = fontName;
    activeMonoFontPath_ = fontPath;

    if (!fontRenderer_ || !fontRenderer_->fs) {
        pendingMonoFontPath_ = fontPath;
        pendingMonoFontName_ = fontName;
        return false;
    }

    if (!std::filesystem::exists(fontPath)) {
        std::cerr << "[GuiWindow] Error: Mono font file not found at " << fontPath << std::endl;
        return false;
    }

    int fid = fonsAddFont(fontRenderer_->fs, fontName.c_str(), fontPath.c_str(), 0);
    if (fid == FONS_INVALID) {
        std::cerr << "[GuiWindow] Error: Failed to parse mono font file " << fontPath << std::endl;
        return false;
    }

    fontRenderer_->fontMono = fid;
    fontRenderer_->loadedMonoFontPath = fontPath;
    std::cout << "[GuiWindow] Successfully loaded mono font '" << fontName << "' (" << fontPath << ")" << std::endl;
    return true;
}

bool GuiWindow::isFontLoaded() const noexcept {
    return fontRenderer_ && fontRenderer_->fontNormal != FONS_INVALID;
}

bool GuiWindow::isMonoFontLoaded() const noexcept {
    return fontRenderer_ && fontRenderer_->fontMono != FONS_INVALID;
}

HitTestTransportResult GuiWindow::hitTestTransport(float x, float y) const noexcept {
    HitTestTransportResult res{};
    if (y < 0.0f || y > 56.0f) {
        return res;
    }

    const float r = static_cast<float>(width_);

    // 0. Top-Left Eatsbits Brand Logo (opens Project Hub / Settings dialog) [10 to 46]
    if (x >= 10.0f && x <= 46.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::ProjectHubToggle;
        return res;
    }

    // 1. Tactile Clustered Transport Keys: Play [54..92], Stop [92..130], Record [130..168]
    if (x >= 54.0f && x < 92.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::PlayPause;
        return res;
    }
    if (x >= 92.0f && x < 130.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::Stop;
        return res;
    }
    if (x >= 130.0f && x < 168.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::Record;
        return res;
    }

    // 2. Minimal Recessed BPM Display Capsule [178 to 294]
    if (x >= 176.0f && x <= 296.0f && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::Bpm;
        return res;
    }

    // 3. Right-Side Tactile Pushbuttons:
    // Lock Button [r - 134 to r - 96]
    if (x >= (r - 134.0f) && x < (r - 96.0f) && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::LockToggle;
        return res;
    }

    // Search / Fullscreen Button [r - 96 to r - 66]
    if (x >= (r - 96.0f) && x < (r - 66.0f) && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::FullscreenToggle;
        return res;
    }

    // Folder / Preset Library Button [r - 66 to r - 4]
    if (x >= (r - 66.0f) && x <= (r - 4.0f) && y >= 8.0f && y <= 48.0f) {
        res.hit = true;
        res.action = TransportAction::BrowserToggle;
        return res;
    }

    // Snap Selector Box [695, 11, 65, 34]
    if (x >= 695.0f && x <= 760.0f && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::SnapToggle;
        return res;
    }

    // Scale Selector Box [r - 345, 11, 72, 34]
    if (x >= (r - 345.0f) && x <= (r - 273.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::ScaleToggle;
        return res;
    }

    // Metronome Toggle [r - 265, 11, 75, 34]
    if (x >= (r - 265.0f) && x <= (r - 190.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::MetronomeToggle;
        return res;
    }

    // Loop Toggle [r - 180, 11, 65, 34]
    if (x >= (r - 180.0f) && x <= (r - 115.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::LoopToggle;
        return res;
    }

    // Project Browser Button [r - 105, 11, 85, 34]
    if (x >= (r - 105.0f) && x <= (r - 20.0f) && y >= 11.0f && y <= 45.0f) {
        res.hit = true;
        res.action = TransportAction::BrowserToggle;
        return res;
    }

    // Legacy tab positions (for backwards compatibility if invoked directly):
    // Tab Rack [r - 430, 12, 68, 32]
    if (x >= (r - 430.0f) && x <= (r - 362.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabRack;
        return res;
    }
    // Tab Hardware Panel [r - 355, 12, 75, 32]
    if (x >= (r - 355.0f) && x <= (r - 280.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabHardware;
        return res;
    }
    // Tab Tracker [r - 274, 12, 80, 32]
    if (x >= (r - 274.0f) && x <= (r - 194.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabTracker;
        return res;
    }
    // Tab Mixer [r - 188, 12, 75, 32]
    if (x >= (r - 188.0f) && x <= (r - 113.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabMixer;
        return res;
    }
    // Tab Script [r - 106, 12, 75, 32]
    if (x >= (r - 106.0f) && x <= (r - 31.0f) && y >= 12.0f && y <= 44.0f) {
        res.hit = true;
        res.action = TransportAction::TabEatscript;
        return res;
    }

    // Preset switcher buttons (when activeView_ == HardwarePanel / Track):
    if ((activeView_ == WorkspaceView::HardwarePanel || activeView_ == WorkspaceView::Track) && y >= 85.0f && y <= 120.0f) {
        if (x >= 52.0f && x <= 134.0f) {
            res.hit = true;
            res.action = TransportAction::PresetPrev;
            return res;
        }
        if (x >= 135.0f && x <= 218.0f) {
            res.hit = true;
            res.action = TransportAction::PresetNext;
            return res;
        }
    }

    return res;
}

HitTestBottomNavResult GuiWindow::hitTestBottomNav(float x, float y) const noexcept {
    HitTestBottomNavResult res{};
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    if (y < bPanelY || y > static_cast<float>(height_)) {
        return res;
    }

    const float totalNavW = static_cast<float>(width_) - 24.0f;
    const float navBtnW = totalNavW / 5.0f;

    if (x < 12.0f || x > (12.0f + totalNavW)) {
        return res;
    }

    int idx = static_cast<int>((x - 12.0f) / navBtnW);
    if (idx >= 0 && idx < 5) {
        res.hit = true;
        switch (idx) {
            case 0: res.action = BottomNavAction::Arranger; break;
            case 1: res.action = BottomNavAction::Edit; break;
            case 2: res.action = BottomNavAction::Track; break;
            case 3: res.action = BottomNavAction::Mixer; break;
            case 4: res.action = BottomNavAction::Design; break;
            default: break;
        }
    }
    return res;
}

HitTestMixerResult GuiWindow::hitTestMixer(float x, float y) const noexcept {
    HitTestMixerResult res{};
    if (activeView_ != WorkspaceView::Mixer) return res;

    // 0. Check Top Modular Section Toolbar (Y: 58..90)
    if (y >= 58.0f && y <= 90.0f) {
        if (x >= 390.0f && x <= 444.0f) {
            res.hit = true;
            res.isPresetFull = true;
            return res;
        }
        if (x >= 452.0f && x <= 560.0f) {
            res.hit = true;
            res.isPresetFaders = true;
            return res;
        }
        if (x >= 568.0f && x <= 700.0f) {
            res.hit = true;
            res.isPresetMeters = true;
            return res;
        }
        if (x >= 710.0f && x <= 786.0f) {
            res.hit = true;
            res.isToggleRouting = true;
            return res;
        }
        if (x >= 794.0f && x <= 842.0f) {
            res.hit = true;
            res.isTogglePan = true;
            return res;
        }
        if (x >= 850.0f && x <= 930.0f) {
            res.hit = true;
            res.isToggleButtons = true;
            return res;
        }
        if (x >= 938.0f && x <= 1012.0f) {
            res.hit = true;
            res.isToggleMeters = true;
            return res;
        }
    }

    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float startY = 96.0f;
    const float bottomY = bPanelY - 6.0f;
    const float topY = 56.0f;

    const float w = static_cast<float>(width_);
    const float propW = mixerPropertiesExpanded_ ? mixerPropertiesWidth_ : 0.0f;
    const float pullTabW = kMixerPullTabW;
    const float drawerTotalW = mixerPropertiesExpanded_ ? (pullTabW + propW) : pullTabW;
    const float browserOffset = browserOpen_ ? ProjectBrowserDrawer::getDrawerWidth() : 0.0f;
    const float rightBoundary = w - drawerTotalW - browserOffset;
    const float pullTabX = rightBoundary;
    const float propX = pullTabX + pullTabW;

    // 0.1 Check Right-Side Pull-Tab Strip Hit
    if (x >= pullTabX && x <= pullTabX + pullTabW && y >= topY && y <= bottomY) {
        res.hit = true;
        res.isPullTab = true;
        return res;
    }

    // 0.2 Check Expanded Track Properties Sidebar Body Hit
    if (mixerPropertiesExpanded_ && x > pullTabX + pullTabW && x <= propX + propW && y >= topY && y <= bottomY) {
        res.hit = true;
        // Close Button [propX + propW - 28, topY + 7, 22, 22]
        float clBtnX = propX + propW - 28.0f;
        float clBtnY = topY + 7.0f;
        if (x >= clBtnX && x <= clBtnX + 22.0f && y >= clBtnY && y <= clBtnY + 22.0f) {
            res.isPropertiesClose = true;
            return res;
        }

        float cardX = propX + 10.0f;
        float cardW = propW - 20.0f;
        float cardY = topY + 44.0f;
        float cardH = bottomY - cardY - 8.0f;
        auto inspHit = hitTestTrackPropertiesContainer(x, y, cardX, cardY, cardW, cardH, selectedTrackIndex_, trackInspectorScrollY_, true);
        if (inspHit.hit) {
            res.trackInspectorHit = inspHit;
            return res;
        }
        return res;
    }

    // 1. Check Master Strip [40, startY, 135, stripH] (Also accepts legacy Y: 110..640)
    if (x >= 40.0f && x <= 175.0f && y >= std::min(startY, 110.0f) && y <= std::max(bottomY, 640.0f)) {
        res.hit = true;
        res.isMaster = true;
        res.channelIndex = 999;

        // Master Backlit LCD Screen: [48, 116..150]
        if (x >= 48.0f && x <= 167.0f && y >= 116.0f && y <= 150.0f) {
            res.isLcdScreen = true;
            return res;
        }

        // Master Mute button: [54, 152, 50, 24]
        if (x >= 54.0f && x <= 104.0f && y >= 152.0f && y <= 176.0f) {
            res.isMute = true;
            return res;
        }
        // Master Pan pot: around (107, 215), radius ~ 22
        if (std::hypot(x - 107.0f, y - 215.0f) <= 22.0f) {
            res.isPan = true;
            return res;
        }
        // Master Fader slot: [65, 260, 50, (bottomY - 260)]
        float mSlotBottom = std::max(555.0f, bottomY - 36.0f);
        if (x >= 60.0f && x <= 115.0f && y >= 260.0f && y <= mSlotBottom) {
            res.isFader = true;
            float totalTravel = std::max(120.0f, mSlotBottom - 265.0f);
            float norm = 1.0f - std::clamp((y - 265.0f) / totalTravel, 0.0f, 1.0f);
            res.normVal = norm * 1.5f;
            return res;
        }
        return res;
    }

    // 2. Check Channel Strips [195 + i*(135+16), startY, 135, stripH]
    const float startX = 195.0f;
    const float stripW = 135.0f;
    const float gap = 16.0f;

    for (size_t i = 0; i < mixerStrips_.size() && i < 5; ++i) {
        float cx = startX + i * (stripW + gap);
        if (x >= cx && x <= cx + stripW && y >= std::min(startY, 110.0f) && y <= std::max(bottomY, 640.0f)) {
            res.hit = true;
            res.channelIndex = static_cast<uint32_t>(i);

            // Backlit LCD Screen: [cx + 8, 116..150]
            if (x >= cx + 8.0f && x <= cx + stripW - 8.0f && y >= 116.0f && y <= 150.0f) {
                res.isLcdScreen = true;
                return res;
            }

            // Mute button: [cx + 12, 152, 50, 24]
            if (x >= cx + 12.0f && x <= cx + 62.0f && y >= 152.0f && y <= 176.0f) {
                res.isMute = true;
                return res;
            }
            // Solo button: [cx + 68, 152, 50, 24]
            if (x >= cx + 68.0f && x <= cx + 118.0f && y >= 152.0f && y <= 176.0f) {
                res.isSolo = true;
                return res;
            }
            // Automation Mode button: [cx + 68, 184, 50, 18]
            if (x >= cx + 68.0f && x <= cx + 118.0f && y >= 184.0f && y <= 202.0f) {
                res.isAutomation = true;
                return res;
            }
            // Pan pot: center (cx + 65, 215)
            if (std::hypot(x - (cx + 65.0f), y - 215.0f) <= 22.0f) {
                res.isPan = true;
                return res;
            }
            // Phase Invert button: [cx + 80, 269, 26, 20]
            if (x >= cx + 80.0f && x <= cx + 106.0f && y >= 269.0f && y <= 289.0f) {
                res.isPhase = true;
                return res;
            }
            // FX In button: [cx + 80, 295, 26, 20]
            if (x >= cx + 80.0f && x <= cx + 106.0f && y >= 295.0f && y <= 315.0f) {
                res.isFxIn = true;
                return res;
            }
            // Freeze button: [cx + 80, 321, 26, 20]
            if (x >= cx + 80.0f && x <= cx + 106.0f && y >= 321.0f && y <= 341.0f) {
                res.isFreeze = true;
                return res;
            }
            // Edit button: [cx + 20, 602..626] or [cx + 20, bottomY - 34..bottomY]
            float editBtnY = (bottomY >= 635.0f) ? (bottomY - 26.0f) : 602.0f;
            if (x >= cx + 20.0f && x <= cx + 115.0f &&
                ((y >= 600.0f && y <= 628.0f) || (y >= editBtnY && y <= editBtnY + 26.0f))) {
                res.isEditButton = true;
                return res;
            }

            // Fader slot: [cx + 10, 260, 65, (editBtnY - 8)]
            float chSlotBottom = editBtnY - 8.0f;
            if (x >= cx + 10.0f && x <= cx + 75.0f && y >= 260.0f && y <= chSlotBottom) {
                res.isFader = true;
                float totalTravel = std::max(120.0f, chSlotBottom - 265.0f);
                float norm = 1.0f - std::clamp((y - 265.0f) / totalTravel, 0.0f, 1.0f);
                res.normVal = norm * 1.5f;
                return res;
            }

            return res;
        }
    }

    return res;
}

HitTestTrackTabResult GuiWindow::hitTestTrackTab(float x, float y) const noexcept {
    HitTestTrackTabResult res{};
    if (activeView_ != WorkspaceView::Track && activeView_ != WorkspaceView::HardwarePanel) return res;
    if (y < 60.0f || y > 95.0f) return res;

    const float startX = 40.0f;
    const float tabW = 120.0f;
    const float tabGap = 8.0f;
    for (uint32_t t = 0; t < 5; ++t) {
        float tx = startX + t * (tabW + tabGap);
        if (x >= tx && x <= tx + tabW) {
            res.hit = true;
            res.trackIndex = t;
            return res;
        }
    }
    return res;
}

void GuiWindow::drawTrackPropertiesContainer(float x, float y, float width, float height, uint32_t trackIndex, float scrollY, bool isCompact) {
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : 0;
    auto& trk = (tIdx < arrangerTracks_.size()) ? arrangerTracks_[tIdx] : arrangerTracks_[0];
    if (selectedTrackIndex_ != tIdx) {
        syncTrackToPreset(tIdx);
    }
    (void)getTheme();

    static const float swatches[8][3] = {
        {0.0f, 0.90f, 1.0f},   // Neon Cyan
        {1.0f, 0.55f, 0.0f},   // Neon Amber
        {0.0f, 1.00f, 0.40f},  // Acid Green
        {1.0f, 0.00f, 0.48f},  // Hot Pink
        {0.74f, 0.00f, 1.0f},  // Electric Purple
        {1.0f, 0.20f, 0.20f},  // Crimson Red
        {1.0f, 0.85f, 0.0f},   // Gold Yellow
        {0.20f, 0.60f, 1.0f}   // Sky Blue
    };

    float card1H = isCompact ? 52.0f : 60.0f;
    float gap1 = isCompact ? 8.0f : 8.0f;
    float cardColorH = isCompact ? 50.0f : 0.0f;
    float gapColor = isCompact ? 8.0f : 0.0f;
    float card2H = isCompact ? 86.0f : 68.0f;
    float gap2 = isCompact ? 6.0f : 8.0f;
    const float designW = 520.0f;
    float card3H = isCompact ? (30.0f + 210.0f * (std::max(200.0f, width - 16.0f) / designW)) : 280.0f;
    float gap3 = isCompact ? 8.0f : 12.0f;
    float card4H = isCompact ? 84.0f : 80.0f;
    float gap4 = isCompact ? 6.0f : 10.0f;
    float card5H = isCompact ? 180.0f : 120.0f;
    float gap5 = isCompact ? 6.0f : 10.0f;
    float card6H = isCompact ? 280.0f : 135.0f;
    float gap6 = isCompact ? 16.0f : 20.0f;

    float totalContentH = card1H + gap1 + (isCompact ? (cardColorH + gapColor) : 0.0f) + card2H + gap2 + card3H + gap3 + card4H + gap4 + card5H + gap5 + card6H + gap6;
    float maxScroll = std::max(10.0f, totalContentH - height + 40.0f);

    bool needScrollbar = (totalContentH > height);
    float contentW = needScrollbar ? (width - 16.0f) : width;

    // Helper lambda for mini dials
    auto drawMiniDial = [&](float cx, float cy, float radius, float normVal, const char* label) {
        constexpr float minA = -2.35619449f;
        constexpr float maxA = 2.35619449f;
        float curA = minA + std::clamp(normVal, 0.0f, 1.0f) * (maxA - minA);
        drawCircle(cx, cy, radius + 3.0f, 0.06f, 0.07f, 0.09f);
        drawArc(cx, cy, radius + 1.5f, minA, maxA, 0.20f, 0.24f, 0.30f, 1.0f, 1.8f);
        if (normVal > 0.01f) {
            drawArc(cx, cy, radius + 1.5f, minA, curA, 0.0f, 0.85f, 1.0f, 1.0f, 2.0f);
        }
        drawCircle(cx, cy, radius, 0.14f, 0.16f, 0.22f);
        drawCircleOutline(cx, cy, radius, 0.28f, 0.32f, 0.42f, 1.0f, 1.0f);
        float indX = cx + std::sin(curA) * (radius - 2.0f);
        float indY = cy - std::cos(curA) * (radius - 2.0f);
        drawLine(cx, cy, indX, indY, Color(0.0f, 0.95f, 1.0f), 1.8f);
        if (label && label[0] != '\0') {
            drawVectorString(label, cx - 12.0f, cy + radius + 3.0f, 0.50f, Color(0.65f, 0.70f, 0.80f));
        }
    };

    float curY = y - scrollY;

    // SECTION 1: Track Header Card
    if (curY + card1H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card1H, 6.0f, Color(0.13f, 0.15f, 0.20f), Color(0.08f, 0.09f, 0.12f));
        drawRoundedRectOutline(x, curY, contentW, card1H, 6.0f, Color(0.24f, 0.28f, 0.38f), 1.2f);
        drawRoundedRect(x + 10.0f, curY + 10.0f, 5.0f, card1H - 20.0f, 2.5f, Color(trk.r, trk.g, trk.b));

        // Clean Track Name (no numeric prefix)
        drawVectorString(trk.name, x + 24.0f, curY + 12.0f, isCompact ? 0.90f : 1.05f, Color(0.95f, 0.95f, 1.0f));
        drawVectorString("TRACK CHANNEL " + std::to_string(tIdx + 1) + " • EATSCRIPT DSP",
                         x + 24.0f, curY + (isCompact ? 32.0f : 36.0f), isCompact ? 0.56f : 0.62f, Color(0.55f, 0.60f, 0.70f));

        if (!isCompact) {
            // Fullscreen Color Swatches in Header
            float swStartX = x + 310.0f;
            float swY = curY + 18.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = swStartX + s * 26.0f;
                drawCircle(sx + 10.0f, swY + 12.0f, 8.0f, swatches[s][0], swatches[s][1], swatches[s][2]);
                if (std::abs(trk.r - swatches[s][0]) < 0.05f && std::abs(trk.g - swatches[s][1]) < 0.05f) {
                    drawCircleOutline(sx + 10.0f, swY + 12.0f, 10.5f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f);
                }
            }

            // [ CODE ] button
            float btnCodeX = x + contentW - 245.0f;
            drawRoundedRectGradient(btnCodeX, curY + 15.0f, 62.0f, 30.0f, 4.0f, Color(0.14f, 0.18f, 0.26f), Color(0.08f, 0.10f, 0.16f));
            drawRoundedRectOutline(btnCodeX, curY + 15.0f, 62.0f, 30.0f, 4.0f, Color(0.0f, 0.85f, 1.0f), 1.2f);
            drawVectorString("[ CODE ]", btnCodeX + 8.0f, curY + 22.0f, 0.70f, Color(0.0f, 0.95f, 1.0f));

            // MUTE button
            float btnMuteX = x + contentW - 175.0f;
            if (trk.mute) {
                drawRoundedRect(btnMuteX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.85f, 0.25f, 0.15f));
                drawVectorString("MUTE", btnMuteX + 10.0f, curY + 22.0f, 0.72f, Color(1.0f, 1.0f, 1.0f));
            } else {
                drawRoundedRect(btnMuteX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(btnMuteX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString("MUTE", btnMuteX + 10.0f, curY + 22.0f, 0.72f, Color(0.60f, 0.65f, 0.72f));
            }

            // SOLO button
            float btnSoloX = x + contentW - 117.0f;
            if (trk.solo) {
                drawRoundedRect(btnSoloX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.95f, 0.75f, 0.10f));
                drawVectorString("SOLO", btnSoloX + 10.0f, curY + 22.0f, 0.72f, Color(0.05f, 0.08f, 0.12f));
            } else {
                drawRoundedRect(btnSoloX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(btnSoloX, curY + 15.0f, 52.0f, 30.0f, 4.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString("SOLO", btnSoloX + 10.0f, curY + 22.0f, 0.72f, Color(0.60f, 0.65f, 0.72f));
            }

            // FREEZE button
            float btnFzX = x + contentW - 59.0f;
            if (trk.freeze) {
                drawRoundedRect(btnFzX, curY + 15.0f, 54.0f, 30.0f, 4.0f, Color(0.0f, 0.85f, 1.0f));
                drawVectorString("FREEZE", btnFzX + 6.0f, curY + 22.0f, 0.65f, Color(0.05f, 0.08f, 0.12f));
            } else {
                drawRoundedRect(btnFzX, curY + 15.0f, 54.0f, 30.0f, 4.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(btnFzX, curY + 15.0f, 54.0f, 30.0f, 4.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString("FREEZE", btnFzX + 6.0f, curY + 22.0f, 0.65f, Color(0.60f, 0.65f, 0.72f));
            }
        } else {
            // Compact MUTE and SOLO
            float btnMuteX = x + contentW - 110.0f;
            drawRoundedRect(btnMuteX, curY + 12.0f, 50.0f, 30.0f, 4.0f, trk.mute ? Color(0.85f, 0.25f, 0.15f) : Color(0.14f, 0.16f, 0.20f));
            drawVectorString("MUTE", btnMuteX + 8.0f, curY + 20.0f, 0.68f, trk.mute ? Color(1.0f, 1.0f, 1.0f) : Color(0.60f, 0.65f, 0.72f));

            float btnSoloX = x + contentW - 54.0f;
            drawRoundedRect(btnSoloX, curY + 12.0f, 50.0f, 30.0f, 4.0f, trk.solo ? Color(0.95f, 0.75f, 0.10f) : Color(0.14f, 0.16f, 0.20f));
            drawVectorString("SOLO", btnSoloX + 8.0f, curY + 20.0f, 0.68f, trk.solo ? Color(0.05f, 0.08f, 0.12f) : Color(0.60f, 0.65f, 0.72f));
        }
    }
    curY += card1H + gap1;

    // SECTION 1B: Track Accent Color (Compact Sidebar Mode)
    if (isCompact) {
        if (curY + cardColorH >= y && curY <= y + height) {
            drawRoundedRectGradient(x, curY, contentW, cardColorH, 6.0f, Color(0.12f, 0.14f, 0.18f), Color(0.08f, 0.09f, 0.12f));
            drawRoundedRectOutline(x, curY, contentW, cardColorH, 6.0f, Color(0.20f, 0.24f, 0.32f), 1.0f);
            drawVectorString("TRACK ACCENT COLOR", x + 10.0f, curY + 6.0f, 0.65f, Color(0.60f, 0.65f, 0.75f));

            float swatchStep = (contentW - 4.0f) / 8.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = x + 12.0f + s * swatchStep;
                float sy = curY + 20.0f;
                float sw = swatchStep - 4.0f;
                float sh = 24.0f;
                drawRoundedRect(sx, sy, sw, sh, 4.0f, Color(swatches[s][0], swatches[s][1], swatches[s][2]));
                if (std::abs(trk.r - swatches[s][0]) < 0.05f && std::abs(trk.g - swatches[s][1]) < 0.05f) {
                    drawRoundedRectOutline(sx - 1.0f, sy - 1.0f, sw + 2.0f, sh + 2.0f, 4.0f, Color(1.0f, 1.0f, 1.0f), 2.0f);
                }
            }
        }
        curY += cardColorH + gapColor;
    }

    // SECTION 2: Channel Mixer Quick Controls
    if (curY + card2H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card2H, 6.0f, Color(0.11f, 0.12f, 0.16f), Color(0.07f, 0.08f, 0.11f));
        drawRoundedRectOutline(x, curY, contentW, card2H, 6.0f, Color(0.22f, 0.25f, 0.34f), 1.0f);
        drawVectorString("CHANNEL MIXER QUICK CONTROLS", x + 16.0f, curY + 10.0f, 0.62f, Color(0.50f, 0.55f, 0.65f));

        float normVol = std::clamp(trk.volume / 1.5f, 0.0f, 1.0f);
        float normPan = std::clamp((trk.pan + 1.0f) * 0.5f, 0.0f, 1.0f);
        int volPct = static_cast<int>(std::round(trk.volume * 100.0f));
        std::string panStr = (std::abs(trk.pan) < 0.04f) ? "C" : ((trk.pan < 0.0f) ? "L" + std::to_string(static_cast<int>(std::round(-trk.pan * 100.0f))) : "R" + std::to_string(static_cast<int>(std::round(trk.pan * 100.0f))));

        if (!isCompact) {
            // Fullscreen Volume & Pan Side-by-Side
            drawVectorString("VOL", x + 24.0f, curY + 38.0f, 0.72f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 80.0f, curY + 36.0f, 220.0f, 18.0f, 4.0f, Color(0.06f, 0.07f, 0.09f));
            drawRoundedRectOutline(x + 80.0f, curY + 36.0f, 220.0f, 18.0f, 4.0f, Color(0.20f, 0.22f, 0.28f), 1.0f);
            if (normVol > 0.01f) {
                drawRoundedRect(x + 82.0f, curY + 38.0f, (220.0f - 4.0f) * normVol, 14.0f, 3.0f, Color(0.0f, 0.85f, 1.0f));
            }
            float volThumbX = x + 80.0f + normVol * 220.0f;
            drawRoundedRectGradient(volThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.35f, 0.40f, 0.50f), Color(0.18f, 0.20f, 0.28f));
            drawRoundedRectOutline(volThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.60f, 0.65f, 0.80f), 1.0f);
            drawVectorString(std::to_string(volPct) + "%", x + 315.0f, curY + 38.0f, 0.72f, Color(0.0f, 0.95f, 1.0f));

            drawVectorString("PAN", x + 375.0f, curY + 38.0f, 0.72f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 420.0f, curY + 36.0f, 180.0f, 18.0f, 4.0f, Color(0.06f, 0.07f, 0.09f));
            drawRoundedRectOutline(x + 420.0f, curY + 36.0f, 180.0f, 18.0f, 4.0f, Color(0.20f, 0.22f, 0.28f), 1.0f);
            drawLine(x + 510.0f, curY + 37.0f, x + 510.0f, curY + 53.0f, Color(0.35f, 0.40f, 0.50f), 1.5f);
            float panThumbX = x + 420.0f + normPan * 180.0f;
            drawRoundedRectGradient(panThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.35f, 0.40f, 0.50f), Color(0.18f, 0.20f, 0.28f));
            drawRoundedRectOutline(panThumbX - 6.0f, curY + 33.0f, 12.0f, 24.0f, 3.0f, Color(0.60f, 0.65f, 0.80f), 1.0f);
            drawVectorString(panStr, x + 615.0f, curY + 38.0f, 0.72f, Color(0.0f, 0.95f, 1.0f));
        } else {
            // Compact Vertical Stack
            float sliderW = contentW - 120.0f;
            drawVectorString("VOL", x + 16.0f, curY + 30.0f, 0.65f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 55.0f, curY + 28.0f, sliderW, 16.0f, 3.0f, Color(0.06f, 0.07f, 0.09f));
            if (normVol > 0.01f) drawRoundedRect(x + 56.0f, curY + 29.0f, (sliderW - 2.0f) * normVol, 14.0f, 2.5f, Color(0.0f, 0.85f, 1.0f));
            drawVectorString(std::to_string(volPct) + "%", x + 65.0f + sliderW, curY + 30.0f, 0.65f, Color(0.0f, 0.95f, 1.0f));

            drawVectorString("PAN", x + 16.0f, curY + 58.0f, 0.65f, Color(0.70f, 0.75f, 0.85f));
            drawRoundedRect(x + 55.0f, curY + 56.0f, sliderW, 16.0f, 3.0f, Color(0.06f, 0.07f, 0.09f));
            float panX = x + 55.0f + normPan * sliderW;
            drawRoundedRect(panX - 4.0f, curY + 54.0f, 8.0f, 20.0f, 2.0f, Color(0.0f, 0.95f, 1.0f));
            drawVectorString(panStr, x + 65.0f + sliderW, curY + 58.0f, 0.65f, Color(0.0f, 0.95f, 1.0f));
        }
    }
    curY += card2H + gap2;

    // SECTION 3: Dynamic Instrument Faceplate
    const auto* preset = getActivePreset();
    if (preset && (curY + card3H >= y && curY <= y + height)) {
        const float pX = x;
        const float pY = curY;
        const float pW = contentW;
        const float pH = card3H;

        if (isCompact) {
            float scale = contentW / designW;

            const std::string& bg = preset->compactGuiRoot.background.empty() ? preset->guiRoot.background : preset->compactGuiRoot.background;
            if (bg == "minimal_white") {
                drawRectGradient(pX, pY, pW, pH, 0.92f, 0.94f, 0.96f, 0.82f, 0.84f, 0.88f);
                drawRectOutline(pX, pY, pW, pH, 0.45f, 0.48f, 0.55f, 1.0f, 1.5f);
            } else if (bg == "c64_breadbin") {
                drawRectGradient(pX, pY, pW, pH, 0.24f, 0.21f, 0.17f, 0.16f, 0.14f, 0.11f);
                drawRectOutline(pX, pY, pW, pH, 0.42f, 0.37f, 0.71f, 1.0f, 1.5f);
            } else if (bg == "dx7_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.12f, 0.11f, 0.09f, 0.07f, 0.06f, 0.06f);
                drawRectOutline(pX, pY, pW, pH, 0.0f, 0.66f, 0.53f, 1.0f, 1.5f);
            } else if (bg == "snes_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.76f, 0.76f, 0.80f, 0.62f, 0.62f, 0.66f);
                drawRectOutline(pX, pY, pW, pH, 0.48f, 0.42f, 0.72f, 1.0f, 1.5f);
            } else if (bg == "ym2612_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.08f, 0.08f, 0.09f, 0.04f, 0.04f, 0.05f);
                drawRectOutline(pX, pY, pW, pH, 0.90f, 0.66f, 0.14f, 1.0f, 1.5f);
            } else if (bg == "convolver_faceplate") {
                drawRectGradient(pX, pY, pW, pH, 0.13f, 0.15f, 0.18f, 0.07f, 0.08f, 0.10f);
                drawRectOutline(pX, pY, pW, pH, 0.0f, 0.90f, 1.0f, 1.0f, 1.5f);
            } else if (bg == "silver") {
                drawRectGradient(pX, pY, pW, pH, 0.26f, 0.28f, 0.35f, 0.16f, 0.18f, 0.23f);
                drawRectOutline(pX, pY, pW, pH, 0.45f, 0.50f, 0.62f, 1.0f, 1.5f);
            } else {
                drawRectGradient(pX, pY, pW, pH, 0.11f, 0.12f, 0.16f, 0.06f, 0.07f, 0.10f);
                drawRectOutline(pX, pY, pW, pH, 0.22f, 0.26f, 0.35f, 1.0f, 1.5f);
            }

            // Top Compact Banner Bar (Height = 28px)
            drawRect(pX, pY, pW, 28.0f, 0.07f, 0.08f, 0.11f);
            drawLine(pX, pY + 28.0f, pX + pW, pY + 28.0f, 0.25f, 0.30f, 0.38f, 1.0f, 1.2f);

            // Preset Navigation: < and >
            drawRectGradient(pX + 6.0f, pY + 4.0f, 32.0f, 20.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
            drawRectOutline(pX + 6.0f, pY + 4.0f, 32.0f, 20.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.0f);
            drawVectorString("<", pX + 18.0f, pY + 9.0f, 0.70f, 0.0f, 1.0f, 1.0f);

            drawRectGradient(pX + 42.0f, pY + 4.0f, 32.0f, 20.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
            drawRectOutline(pX + 42.0f, pY + 4.0f, 32.0f, 20.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.0f);
            drawVectorString(">", pX + 54.0f, pY + 9.0f, 0.70f, 0.0f, 1.0f, 1.0f);

            // Title
            const std::string& pTitle = preset->compactGuiRoot.title.empty() ? preset->guiRoot.title : preset->compactGuiRoot.title;
            drawVectorString(pTitle, pX + 80.0f, pY + 9.0f, 0.72f, 0.0f, 0.95f, 1.0f);

            // [ FULL ] button on right
            float fullBtnW = 44.0f;
            float fullBtnX = pX + pW - fullBtnW - 6.0f;
            drawRectGradient(fullBtnX, pY + 4.0f, fullBtnW, 20.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
            drawRectOutline(fullBtnX, pY + 4.0f, fullBtnW, 20.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.0f);
            drawVectorString("FULL", fullBtnX + 8.0f, pY + 9.0f, 0.65f, 0.0f, 0.95f, 1.0f);

            // Recursive Compact Node rendering using designWidth 520.0f
            std::function<void(const project::GuiLayoutNode&)> renderCompactNode = [&](const project::GuiLayoutNode& node) {
                float adjNodeX = pX + node.boundsX * scale;
                float adjNodeY = pY + 28.0f + (node.boundsY - 28.0f) * scale;
                float adjNodeW = node.boundsW * scale;
                float adjNodeH = node.boundsH * scale;

                if (node.type == project::GuiNodeType::Row || node.type == project::GuiNodeType::Column || node.type == project::GuiNodeType::Panel) {
                    for (const auto& c : node.children) renderCompactNode(c);
                } else if (node.type == project::GuiNodeType::Group) {
                    drawRoundedRect(adjNodeX, adjNodeY, adjNodeW, adjNodeH, 3.0f, Color(0.08f, 0.09f, 0.12f));
                    drawRoundedRectOutline(adjNodeX, adjNodeY, adjNodeW, adjNodeH, 3.0f, Color(0.20f, 0.24f, 0.30f), 1.0f);
                    if (!node.label.empty()) {
                        drawVectorString(node.label, adjNodeX + 4.0f, adjNodeY + 3.0f, 0.50f * scale + 0.15f, Color(0.60f, 0.65f, 0.75f));
                    }
                    for (const auto& c : node.children) renderCompactNode(c);
                } else if (node.type == project::GuiNodeType::Knob) {
                    float cx = adjNodeX + adjNodeW * 0.5f;
                    float cy = adjNodeY + adjNodeH * 0.38f;
                    float kRad = node.size * 0.42f * scale;
                    const auto* p = preset->findParam(node.paramName);
                    float normVal = p ? p->getNormalized() : 0.5f;
                    bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                    bool isHovered = (std::hypot(mouseX_ - cx, mouseY_ - cy) <= kRad * 1.5f);
                    const auto& light = Theme::current().globalLight;

                    if (node.hardwareStyle == "tb303_potentiometer" || node.hardwareStyle == "tb303_selector" ||
                        preset->metadata.id == "eats_303" || preset->metadata.engineId == "tb303") {
                        std::string readout = p ? p->getFormatted() : "";
                        renderTb303Knob(cx, cy, kRad, normVal, isActive, isHovered, node.label, readout, light, node.hardwareStyle == "tb303_selector");
                    } else {
                        drawCircleDropShadow(cx, cy, kRad + 2.0f, 6.0f, light, 0.45f);
                        constexpr float minAngle = -2.35619449f;
                        constexpr float maxAngle = 2.35619449f;
                        float currentAngle = minAngle + normVal * (maxAngle - minAngle);

                        drawCircle(cx, cy, kRad + 3.0f, 0.06f, 0.07f, 0.09f);
                        drawArc(cx, cy, kRad + 2.0f, minAngle, maxAngle, 0.20f, 0.22f, 0.28f, 1.0f, 1.5f);
                        if (normVal > 0.01f) {
                            drawArc(cx, cy, kRad + 2.0f, minAngle, currentAngle, isActive ? 1.0f : 0.0f,
                                                                                  isActive ? 0.85f : 0.85f,
                                                                                  isActive ? 0.1f : 1.0f, 1.0f, 1.8f);
                        }
                        drawCircle(cx, cy, kRad, 0.16f, 0.18f, 0.24f);
                        drawCircleOutline(cx, cy, kRad, 0.28f, 0.32f, 0.42f, 1.0f, 1.0f);
                        float indX = cx + std::sin(currentAngle) * (kRad - 1.5f);
                        float indY = cy - std::cos(currentAngle) * (kRad - 1.5f);
                        drawLine(cx, cy, indX, indY, Color(0.0f, 0.95f, 1.0f), 1.5f);
                        drawVectorString(node.label, cx - node.label.size() * 2.5f, cy + kRad + 5.0f, 0.48f, Color(0.70f, 0.75f, 0.85f));
                    }
                } else if (node.type == project::GuiNodeType::Nixie) {
                    float cx = adjNodeX + adjNodeW * 0.5f;
                    float cy = adjNodeY + adjNodeH * 0.4f;
                    drawRoundedRect(cx - 20.0f, cy - 8.0f, 40.0f, 16.0f, 2.0f, Color(0.05f, 0.02f, 0.0f));
                    drawRoundedRectOutline(cx - 20.0f, cy - 8.0f, 40.0f, 16.0f, 2.0f, Color(0.85f, 0.40f, 0.0f), 1.0f);
                    drawVectorString(node.label, cx - 12.0f, cy - 3.0f, 0.60f, Color(1.0f, 0.60f, 0.10f));
                } else if (node.type == project::GuiNodeType::Divider) {
                    drawLine(adjNodeX + adjNodeW * 0.5f, adjNodeY + 4.0f,
                             adjNodeX + adjNodeW * 0.5f, adjNodeY + adjNodeH - 4.0f,
                             Color(0.4f, 0.45f, 0.55f, 0.5f), 1.0f);
                }
            };

            for (const auto& child : preset->compactGuiRoot.children) {
                renderCompactNode(child);
            }
        } else {
            float scale = 0.70f;

        const auto& light = Theme::current().globalLight;
        drawRectDropShadow(pX, pY, pW, pH, 6.0f, 14.0f, light, 0.40f);

        const std::string& bg = preset->guiRoot.background;
        if (bg == "minimal_white") {
            drawRectGradient(pX, pY, pW, pH, 0.92f, 0.94f, 0.96f, 0.82f, 0.84f, 0.88f);
            drawRectOutline(pX, pY, pW, pH, 0.45f, 0.48f, 0.55f, 1.0f, 2.0f);
        } else if (bg == "c64_breadbin") {
            drawRectGradient(pX, pY, pW, pH, 0.24f, 0.21f, 0.17f, 0.16f, 0.14f, 0.11f);
            drawRectOutline(pX, pY, pW, pH, 0.42f, 0.37f, 0.71f, 1.0f, 2.5f);
        } else if (bg == "dx7_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.12f, 0.11f, 0.09f, 0.07f, 0.06f, 0.06f);
            drawRectOutline(pX, pY, pW, pH, 0.0f, 0.66f, 0.53f, 1.0f, 2.5f);
        } else if (bg == "snes_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.76f, 0.76f, 0.80f, 0.62f, 0.62f, 0.66f);
            drawRectOutline(pX, pY, pW, pH, 0.48f, 0.42f, 0.72f, 1.0f, 2.5f);
        } else if (bg == "ym2612_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.08f, 0.08f, 0.09f, 0.04f, 0.04f, 0.05f);
            drawRectOutline(pX, pY, pW, pH, 0.90f, 0.66f, 0.14f, 1.0f, 2.5f);
        } else if (bg == "convolver_faceplate") {
            drawRectGradient(pX, pY, pW, pH, 0.13f, 0.15f, 0.18f, 0.07f, 0.08f, 0.10f);
            drawRectOutline(pX, pY, pW, pH, 0.0f, 0.90f, 1.0f, 1.0f, 2.5f);
        } else if (bg == "silver") {
            drawRectGradient(pX, pY, pW, pH, 0.26f, 0.28f, 0.35f, 0.16f, 0.18f, 0.23f);
            drawRectOutline(pX, pY, pW, pH, 0.45f, 0.50f, 0.62f, 1.0f, 2.0f);
        } else {
            drawRectGradient(pX, pY, pW, pH, 0.11f, 0.12f, 0.16f, 0.06f, 0.07f, 0.10f);
            drawRectOutline(pX, pY, pW, pH, 0.22f, 0.26f, 0.35f, 1.0f, 2.0f);
        }

        // Corner hex mounting bolts
        drawCircle(pX + 12.0f, pY + 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);
        drawCircle(pX + pW - 12.0f, pY + 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);
        drawCircle(pX + 12.0f, pY + pH - 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);
        drawCircle(pX + pW - 12.0f, pY + pH - 12.0f, 3.5f, 0.4f, 0.44f, 0.52f);

        // Top Banner Plate
        drawRect(pX, pY, pW, 52.0f, (bg == "minimal_white") ? 0.85f : 0.07f,
                                    (bg == "minimal_white") ? 0.87f : 0.08f,
                                    (bg == "minimal_white") ? 0.90f : 0.11f);
        drawLine(pX, pY + 52.0f, pX + pW, pY + 52.0f, 0.25f, 0.30f, 0.38f, 1.0f, 1.5f);

        // Preset Navigation: < PREV and NEXT >
        drawRectGradient(pX + 16.0f, pY + 12.0f, 75.0f, 28.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
        drawRectOutline(pX + 16.0f, pY + 12.0f, 75.0f, 28.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.2f);
        drawVectorString("< PREV", pX + 24.0f, pY + 20.0f, 0.85f, 0.0f, 1.0f, 1.0f);

        drawRectGradient(pX + 98.0f, pY + 12.0f, 75.0f, 28.0f, 0.18f, 0.20f, 0.26f, 0.10f, 0.11f, 0.15f);
        drawRectOutline(pX + 98.0f, pY + 12.0f, 75.0f, 28.0f, 0.0f, 0.85f, 1.0f, 1.0f, 1.2f);
        drawVectorString("NEXT >", pX + 106.0f, pY + 20.0f, 0.85f, 0.0f, 1.0f, 1.0f);

        std::string pCounter = std::to_string(activePresetIndex_ + 1) + "/" + std::to_string(presets_.size());
        drawVectorString(pCounter, pX + 184.0f, pY + 20.0f, 0.85f, (bg == "minimal_white") ? 0.2f : 0.6f,
                                                                     (bg == "minimal_white") ? 0.2f : 0.65f,
                                                                     (bg == "minimal_white") ? 0.2f : 0.75f);

        drawVectorString(preset->guiRoot.title, pX + 245.0f, pY + 12.0f, 1.15f, 0.0f, 0.95f, 1.0f);
        drawVectorString(preset->guiRoot.subtitle, pX + 245.0f, pY + 32.0f, 0.72f, 0.55f, 0.60f, 0.70f);

        // Recursive node rendering with layout scaling and offset
        std::function<void(const project::GuiLayoutNode&)> renderNode = [&](const project::GuiLayoutNode& node) {
            float adjNodeY = pY + 54.0f + (node.boundsY - 152.0f) * scale;
            float adjNodeX = pX + (node.boundsX - 40.0f);
            if (node.type == project::GuiNodeType::Row || node.type == project::GuiNodeType::Column || node.type == project::GuiNodeType::Panel) {
                for (const auto& c : node.children) renderNode(c);
            } else if (node.type == project::GuiNodeType::Group) {
                drawRoundedRect(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, 4.0f, Color(0.08f, 0.09f, 0.12f));
                drawRoundedRectOutline(adjNodeX, adjNodeY, node.boundsW, node.boundsH * scale, 4.0f, Color(0.20f, 0.24f, 0.30f), 1.0f);
                if (!node.label.empty()) {
                    drawVectorString(node.label, adjNodeX + 8.0f, adjNodeY + 4.0f, 0.60f, Color(0.60f, 0.65f, 0.75f));
                }
                for (const auto& c : node.children) renderNode(c);
            } else if (node.type == project::GuiNodeType::Knob) {
                float cx = adjNodeX + node.boundsW * 0.5f;
                float cy = adjNodeY + node.boundsH * 0.35f * (scale < 0.95f ? scale : 1.0f);
                float kRad = node.size * 0.42f;
                const auto* p = preset->findParam(node.paramName);
                float normVal = p ? p->getNormalized() : 0.5f;
                bool isActive = (dragMode_ == DragMode::HardwareKnob && activeHardwareParam_ == node.paramName);
                bool isHovered = (std::hypot(mouseX_ - cx, mouseY_ - cy) <= kRad * 1.5f);

                if (node.hardwareStyle == "tb303_potentiometer" || node.hardwareStyle == "tb303_selector" ||
                    preset->metadata.id == "eats_303" || preset->metadata.engineId == "tb303") {
                    std::string readout = p ? p->getFormatted() : "";
                    renderTb303Knob(cx, cy, kRad, normVal, isActive, isHovered, node.label, readout, light, node.hardwareStyle == "tb303_selector");
                } else {
                    drawCircleDropShadow(cx, cy, kRad + 3.0f, 8.0f, light, 0.48f);
                    constexpr float minAngle = -2.35619449f;
                    constexpr float maxAngle = 2.35619449f;
                    float currentAngle = minAngle + normVal * (maxAngle - minAngle);

                    drawCircle(cx, cy, kRad + 5.0f, 0.06f, 0.07f, 0.09f);
                    drawArc(cx, cy, kRad + 3.0f, minAngle, maxAngle, 0.20f, 0.22f, 0.28f, 1.0f, 1.8f);
                    if (normVal > 0.01f) {
                        drawArc(cx, cy, kRad + 3.0f, minAngle, currentAngle, isActive ? 1.0f : 0.0f,
                                                                              isActive ? 0.85f : 0.85f,
                                                                              isActive ? 0.1f : 1.0f, 1.0f, 2.2f);
                    }
                    drawCircle(cx, cy, kRad, 0.16f, 0.18f, 0.24f);
                    drawCircleOutline(cx, cy, kRad, 0.28f, 0.32f, 0.42f, 1.0f, 1.0f);
                    float indX = cx + std::sin(currentAngle) * (kRad - 2.5f);
                    float indY = cy - std::cos(currentAngle) * (kRad - 2.5f);
                    drawLine(cx, cy, indX, indY, Color(0.0f, 0.95f, 1.0f), 1.8f);
                    drawVectorString(node.label, cx - node.label.size() * 3.0f, cy + kRad + 8.0f, 0.55f, Color(0.70f, 0.75f, 0.85f));
                }
            } else if (node.type == project::GuiNodeType::Nixie) {
                float cx = adjNodeX + node.boundsW * 0.5f;
                float cy = adjNodeY + node.boundsH * 0.4f;
                drawRoundedRect(cx - 28.0f, cy - 12.0f, 56.0f, 24.0f, 3.0f, Color(0.05f, 0.02f, 0.0f));
                drawRoundedRectOutline(cx - 28.0f, cy - 12.0f, 56.0f, 24.0f, 3.0f, Color(0.85f, 0.40f, 0.0f), 1.2f);
                drawVectorString(node.label, cx - 18.0f, cy - 4.0f, 0.80f, Color(1.0f, 0.60f, 0.10f));
            } else if (node.type == project::GuiNodeType::Divider) {
                drawLine(adjNodeX + node.boundsW * 0.5f, adjNodeY + 6.0f,
                         adjNodeX + node.boundsW * 0.5f, adjNodeY + node.boundsH * scale - 6.0f,
                         Color(0.4f, 0.45f, 0.55f, 0.5f), 1.0f);
            }
        };

        for (const auto& child : preset->guiRoot.children) {
            renderNode(child);
        }

        // Mini oscilloscope on bottom right of faceplate (fullscreen only)
        if (!isCompact && pW > 600.0f) {
            float scX = pX + pW - 195.0f;
            float scY = pY + pH - 90.0f;
            drawRect(scX, scY, 180.0f, 78.0f, 0.02f, 0.05f, 0.03f);
            drawRectOutline(scX, scY, 180.0f, 78.0f, 0.10f, 0.75f, 0.25f, 0.8f, 1.2f);
            drawVectorString("OSCILLOSCOPE", scX + 8.0f, scY + 6.0f, 0.60f, 0.15f, 0.85f, 0.30f);
            constexpr int PTS = 48;
            float prevX = scX + 6.0f;
            float prevY = scY + 45.0f - scopeBuffer_[0] * 28.0f;
            for (int i = 1; i < PTS; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(PTS - 1);
                float sx = scX + 6.0f + t * 168.0f;
                float wave = scopeBuffer_[(i * 4) % 256];
                float sy = scY + 45.0f - wave * 28.0f;
                drawLine(prevX, prevY, sx, sy, Color(0.15f, 1.0f, 0.35f, 0.95f), 1.6f);
                prevX = sx;
                prevY = sy;
            }
        }
        }
    }
    curY += card3H + gap3;

    // SECTION 4: HARMONIC CHORD TRACK FOLLOW
    if (curY + card4H >= y && curY <= y + height) {
        bool isFollowing = (trk.chordFollowMode != ChordFollowMode::Off);
        drawRoundedRectGradient(x, curY, contentW, card4H, 6.0f, Color(0.12f, 0.13f, 0.16f), Color(0.08f, 0.09f, 0.11f));
        drawRoundedRectOutline(x, curY, contentW, card4H, 6.0f,
                               isFollowing ? Color(0.95f, 0.75f, 0.15f) : Color(0.24f, 0.28f, 0.36f),
                               isFollowing ? 1.6f : 1.0f);

        drawVectorString("HARMONIC CHORD TRACK FOLLOW", x + 16.0f, curY + 14.0f, 0.72f, Color(0.95f, 0.75f, 0.15f));
        if (!isCompact) {
            drawVectorString("Conform notes on this track non-destructively to active chords",
                             x + 16.0f, curY + 28.0f, 0.58f, Color(0.60f, 0.65f, 0.72f));
            float bakeX = x + contentW - 130.0f;
            drawRoundedRect(bakeX, curY + 12.0f, 115.0f, 26.0f, 4.0f, Color(0.18f, 0.15f, 0.10f));
            drawRoundedRectOutline(bakeX, curY + 12.0f, 115.0f, 26.0f, 4.0f, Color(0.95f, 0.75f, 0.15f), 1.2f);
            drawVectorString("BAKE TO MIDI", bakeX + 16.0f, curY + 19.0f, 0.65f, Color(0.95f, 0.80f, 0.20f));
        }

        static const char* chipNames[5] = {"OFF", "CHORD", "BASS", "SCALE", "COLOR LEAD"};
        static const ChordFollowMode modes[5] = {
            ChordFollowMode::Off,
            ChordFollowMode::Chord,
            ChordFollowMode::Bass,
            ChordFollowMode::Scale,
            ChordFollowMode::ColorLead
        };
        float chipX = x + 16.0f;
        float chipY = curY + (isCompact ? 40.0f : 44.0f);
        for (int c = 0; c < 5; ++c) {
            float chipW = (c == 4) ? (isCompact ? 80.0f : 100.0f) : (isCompact ? 60.0f : 78.0f);
            bool isSel = (trk.chordFollowMode == modes[c]);
            if (isSel) {
                drawRoundedRect(chipX, chipY, chipW, 26.0f, 13.0f, Color(0.0f, 0.85f, 1.0f));
                drawVectorString(chipNames[c], chipX + 10.0f, chipY + 7.0f, isCompact ? 0.58f : 0.68f, Color(0.05f, 0.08f, 0.12f));
            } else {
                drawRoundedRect(chipX, chipY, chipW, 26.0f, 13.0f, Color(0.14f, 0.16f, 0.20f));
                drawRoundedRectOutline(chipX, chipY, chipW, 26.0f, 13.0f, Color(0.28f, 0.32f, 0.40f), 1.0f);
                drawVectorString(chipNames[c], chipX + 10.0f, chipY + 7.0f, isCompact ? 0.58f : 0.65f, Color(0.70f, 0.75f, 0.82f));
            }
            chipX += chipW + 8.0f;
        }
    }
    curY += card4H + gap4;

    // SECTION 5: SCRIPTABLE MIDI FX RACK
    if (curY + card5H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card5H, 6.0f, Color(0.12f, 0.13f, 0.18f), Color(0.08f, 0.09f, 0.12f));
        drawRoundedRectOutline(x, curY, contentW, card5H, 6.0f, Color(0.25f, 0.28f, 0.38f), 1.0f);
        drawVectorString("MIDI FX INSERT RACK", x + 16.0f, curY + 14.0f, 0.72f, Color(0.92f, 0.35f, 0.85f));

        float modW = (contentW - 48.0f) / 3.0f;

        // Mod 1: ARPEGGIATOR
        float m1X = x + 16.0f;
        drawRoundedRect(m1X, curY + 28.0f, modW, 82.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
        drawRoundedRectOutline(m1X, curY + 28.0f, modW, 82.0f, 4.0f,
                               trk.midiFx.arpEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
        drawVectorString("ARPEGGIATOR", m1X + 8.0f, curY + 36.0f, 0.65f, Color(0.95f, 0.95f, 1.0f));
        drawRoundedRect(m1X + modW - 32.0f, curY + 34.0f, 24.0f, 14.0f, 7.0f,
                        trk.midiFx.arpEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.28f));
        static const char* arpPatterns[5] = {"UP", "DOWN", "UP/DN", "RAND", "CHORD"};
        std::string patStr = (trk.midiFx.arpPattern >= 0 && trk.midiFx.arpPattern < 5) ? arpPatterns[trk.midiFx.arpPattern] : "UP";
        drawRoundedRect(m1X + 8.0f, curY + 54.0f, 60.0f, 20.0f, 3.0f, Color(0.16f, 0.18f, 0.24f));
        drawVectorString(patStr, m1X + 14.0f, curY + 60.0f, 0.60f, Color(0.90f, 0.90f, 1.0f));
        drawVectorString("RATE: 1/16", m1X + 75.0f, curY + 60.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));
        drawVectorString("OCT: 2", m1X + 145.0f, curY + 60.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));
        drawVectorString("GATE: 85%", m1X + 75.0f, curY + 76.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));
        drawVectorString("SWING: 0%", m1X + 145.0f, curY + 76.0f, 0.60f, Color(0.65f, 0.70f, 0.80f));

        // Mod 2: SCALE SNAP
        float m2X = x + 24.0f + modW;
        drawRoundedRect(m2X, curY + 28.0f, modW, 82.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
        drawRoundedRectOutline(m2X, curY + 28.0f, modW, 82.0f, 4.0f,
                               trk.midiFx.scaleSnapEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
        drawVectorString("SCALE SNAP", m2X + 8.0f, curY + 36.0f, 0.65f, Color(0.95f, 0.95f, 1.0f));
        drawRoundedRect(m2X + modW - 32.0f, curY + 34.0f, 24.0f, 14.0f, 7.0f,
                        trk.midiFx.scaleSnapEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.28f));
        drawRoundedRect(m2X + 8.0f, curY + 54.0f, 70.0f, 20.0f, 3.0f, Color(0.16f, 0.18f, 0.24f));
        drawVectorString("KEY: C", m2X + 14.0f, curY + 60.0f, 0.60f, Color(0.90f, 0.90f, 1.0f));
        drawRoundedRect(m2X + 85.0f, curY + 54.0f, 80.0f, 20.0f, 3.0f, Color(0.16f, 0.18f, 0.24f));
        drawVectorString(trk.midiFx.scaleMinor ? "NAT MINOR" : "MAJOR", m2X + 92.0f, curY + 60.0f, 0.60f, Color(0.90f, 0.90f, 1.0f));

        // Mod 3: HUMANIZE
        float m3X = x + 32.0f + 2 * modW;
        drawRoundedRect(m3X, curY + 28.0f, modW, 82.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
        drawRoundedRectOutline(m3X, curY + 28.0f, modW, 82.0f, 4.0f,
                               trk.midiFx.humanizeEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
        drawVectorString("HUMANIZE", m3X + 8.0f, curY + 36.0f, 0.65f, Color(0.95f, 0.95f, 1.0f));
        drawRoundedRect(m3X + modW - 32.0f, curY + 34.0f, 24.0f, 14.0f, 7.0f,
                        trk.midiFx.humanizeEnabled ? Color(0.92f, 0.35f, 0.85f) : Color(0.20f, 0.24f, 0.28f));
        drawVectorString("TIMING: ±15ms", m3X + 12.0f, curY + 60.0f, 0.60f, Color(0.70f, 0.75f, 0.85f));
        drawVectorString("VELOCITY: ±20%", m3X + 105.0f, curY + 60.0f, 0.60f, Color(0.70f, 0.75f, 0.85f));
    }
    curY += card5H + gap5;

    // SECTION 6: AUDIO FX INSERT RACK
    if (curY + card6H >= y && curY <= y + height) {
        drawRoundedRectGradient(x, curY, contentW, card6H, 6.0f, Color(0.12f, 0.14f, 0.18f), Color(0.08f, 0.09f, 0.12f));
        drawRoundedRectOutline(x, curY, contentW, card6H, 6.0f, Color(0.25f, 0.28f, 0.38f), 1.0f);
        drawVectorString("AUDIO FX INSERT RACK", x + 16.0f, curY + 14.0f, 0.72f, Color(0.0f, 0.90f, 1.0f));

        float fxW = (contentW - 64.0f) / 5.0f;
        const char* fxTitles[5] = {"DELAY", "CHORUS", "5-BAND EQ", "COMPRESSOR", "CONVOLVER"};
        bool fxEnabled[5] = {
            trk.audioFx.delayEnabled,
            trk.audioFx.chorusEnabled,
            trk.audioFx.eqEnabled,
            trk.audioFx.compEnabled,
            trk.audioFx.convolverEnabled
        };

        for (int u = 0; u < 5; ++u) {
            float ux = x + 16.0f + u * (fxW + 8.0f);
            drawRoundedRect(ux, curY + 28.0f, fxW, 95.0f, 4.0f, Color(0.09f, 0.10f, 0.14f));
            drawRoundedRectOutline(ux, curY + 28.0f, fxW, 95.0f, 4.0f,
                                   fxEnabled[u] ? Color(0.0f, 0.85f, 1.0f) : Color(0.20f, 0.24f, 0.30f), 1.2f);
            drawVectorString(fxTitles[u], ux + 8.0f, curY + 36.0f, 0.62f, Color(0.95f, 0.95f, 1.0f));
            drawRoundedRect(ux + fxW - 28.0f, curY + 34.0f, 20.0f, 12.0f, 6.0f,
                            fxEnabled[u] ? Color(0.0f, 0.90f, 1.0f) : Color(0.20f, 0.24f, 0.28f));

            float kw = fxW / 3.0f;
            float ky = curY + 68.0f;
            if (u == 0) { // Delay: Time, Fdbk, Mix
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.delayTime, "TIME");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.delayFeedback, "FDBK");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.delayMix, "MIX");
            } else if (u == 1) { // Chorus: Rate, Depth, Mix
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.chorusRate, "RATE");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.chorusDepth, "DPTH");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.chorusMix, "MIX");
            } else if (u == 2) { // EQ: Low, Mid, High
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.eqLow, "LOW");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.eqMid, "MID");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.eqHigh, "HIGH");
            } else if (u == 3) { // Comp: Thresh, Ratio, Gain
                drawMiniDial(ux + kw * 0.5f, ky, 12.0f, trk.audioFx.compThreshold, "THRS");
                drawMiniDial(ux + kw * 1.5f, ky, 12.0f, trk.audioFx.compRatio, "RATIO");
                drawMiniDial(ux + kw * 2.5f, ky, 12.0f, trk.audioFx.compGain, "GAIN");
            } else if (u == 4) { // Convolver: Space, Mix
                drawMiniDial(ux + kw * 0.8f, ky, 12.0f, static_cast<float>(trk.audioFx.convolverPreset) / 12.0f, "SPACE");
                drawMiniDial(ux + kw * 2.2f, ky, 12.0f, trk.audioFx.convolverMix, "MIX");
            }
        }
    }

    // Scrollbar Rendering on the right edge
    if (needScrollbar) {
        float sbX = x + width - 8.0f;
        float sbY = y;
        float sbW = 6.0f;
        float sbH = height;
        drawRoundedRect(sbX, sbY, sbW, sbH, 3.0f, Color(0.06f, 0.07f, 0.09f));
        drawRoundedRectOutline(sbX, sbY, sbW, sbH, 3.0f, Color(0.18f, 0.20f, 0.26f), 1.0f);
        float thumbH = std::clamp(height * (height / totalContentH), 32.0f, height * 0.85f);
        float thumbNorm = std::clamp(scrollY / maxScroll, 0.0f, 1.0f);
        float thumbY = sbY + thumbNorm * (sbH - thumbH);
        bool isDraggingSb = (dragMode_ == DragMode::TrackInspectorScrollbar);
        drawRoundedRectGradient(sbX, thumbY, sbW, thumbH, 3.0f,
                                isDraggingSb ? Color(0.0f, 0.90f, 1.0f) : Color(0.35f, 0.40f, 0.50f),
                                Color(0.18f, 0.22f, 0.30f));
        drawRoundedRectOutline(sbX, thumbY, sbW, thumbH, 3.0f,
                               isDraggingSb ? Color(1.0f, 1.0f, 1.0f) : Color(0.0f, 0.85f, 1.0f, 0.7f), 1.0f);
    }
}

HitTestTrackInspectorResult GuiWindow::hitTestTrackPropertiesContainer(float mx, float my, float x, float y, float width, float height, uint32_t trackIndex, float scrollY, bool isCompact) const noexcept {
    HitTestTrackInspectorResult res{};
    uint32_t tIdx = (trackIndex < arrangerTracks_.size()) ? trackIndex : 0;
    const auto& trk = (tIdx < arrangerTracks_.size()) ? arrangerTracks_[tIdx] : arrangerTracks_[0];
    res.trackIndex = tIdx;

    float card1H = isCompact ? 52.0f : 60.0f;
    float gap1 = isCompact ? 8.0f : 8.0f;
    float cardColorH = isCompact ? 50.0f : 0.0f;
    float gapColor = isCompact ? 8.0f : 0.0f;
    float card2H = isCompact ? 86.0f : 68.0f;
    float gap2 = isCompact ? 6.0f : 8.0f;
    const float designW = 520.0f;
    float card3H = isCompact ? (30.0f + 210.0f * (std::max(200.0f, width - 16.0f) / designW)) : 260.0f;
    float gap3 = isCompact ? 8.0f : 12.0f;
    float card4H = isCompact ? 84.0f : 80.0f;
    float gap4 = isCompact ? 6.0f : 10.0f;
    float card5H = isCompact ? 180.0f : 120.0f;
    float gap5 = isCompact ? 6.0f : 10.0f;
    float card6H = isCompact ? 280.0f : 135.0f;
    float gap6 = isCompact ? 16.0f : 20.0f;

    float totalContentH = card1H + gap1 + (isCompact ? (cardColorH + gapColor) : 0.0f) + card2H + gap2 + card3H + gap3 + card4H + gap4 + card5H + gap5 + card6H + gap6;
    float maxScroll = std::max(10.0f, totalContentH - height + 40.0f);

    bool needScrollbar = (totalContentH > height);
    float contentW = needScrollbar ? (width - 16.0f) : width;

    // Check Scrollbar hit first
    if (needScrollbar) {
        float sbX = x + width - 8.0f;
        float sbY = y;
        float sbH = height;
        if (mx >= sbX - 6.0f && mx <= sbX + 10.0f && my >= sbY && my <= sbY + sbH) {
            float thumbH = std::clamp(height * (height / totalContentH), 32.0f, height * 0.85f);
            float thumbNorm = std::clamp(scrollY / maxScroll, 0.0f, 1.0f);
            float thumbY = sbY + thumbNorm * (sbH - thumbH);
            res.hit = true;
            if (my >= thumbY && my <= thumbY + thumbH) {
                res.area = TrackInspectorHitArea::ScrollbarThumb;
            } else {
                res.area = TrackInspectorHitArea::ScrollbarTrack;
            }
            return res;
        }
    }

    if (mx < x || mx > x + contentW || my < y || my > y + height) {
        return res;
    }

    float curY = y - scrollY;

    // SECTION 1: Track Header Card
    if (my >= curY && my <= curY + card1H) {
        if (!isCompact) {
            float swStartX = x + 310.0f;
            float swY = curY + 18.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = swStartX + s * 26.0f;
                if (mx >= sx && mx <= sx + 20.0f && my >= swY && my <= swY + 24.0f) {
                    res.hit = true;
                    res.area = TrackInspectorHitArea::ColorSwatch;
                    res.colorSwatchIndex = s;
                    return res;
                }
            }

            float btnCodeX = x + contentW - 245.0f;
            if (mx >= btnCodeX && mx <= btnCodeX + 62.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::CodeButton;
                return res;
            }
            float btnMuteX = x + contentW - 175.0f;
            if (mx >= btnMuteX && mx <= btnMuteX + 52.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::MuteButton;
                return res;
            }
            float btnSoloX = x + contentW - 117.0f;
            if (mx >= btnSoloX && mx <= btnSoloX + 52.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::SoloButton;
                return res;
            }
            float btnFzX = x + contentW - 59.0f;
            if (mx >= btnFzX && mx <= btnFzX + 54.0f && my >= curY + 15.0f && my <= curY + 45.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::FreezeButton;
                return res;
            }
        } else {
            float btnMuteX = x + contentW - 110.0f;
            if (mx >= btnMuteX && mx <= btnMuteX + 50.0f && my >= curY + 12.0f && my <= curY + 42.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::MuteButton;
                return res;
            }
            float btnSoloX = x + contentW - 54.0f;
            if (mx >= btnSoloX && mx <= btnSoloX + 50.0f && my >= curY + 12.0f && my <= curY + 42.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::SoloButton;
                return res;
            }
        }
    }
    curY += card1H + gap1;

    // SECTION 1B: Track Accent Color (Compact Sidebar Mode)
    if (isCompact) {
        if (my >= curY && my <= curY + cardColorH) {
            float swatchStep = (contentW - 4.0f) / 8.0f;
            for (uint32_t s = 0; s < 8; ++s) {
                float sx = x + 12.0f + s * swatchStep;
                float sy = curY + 20.0f;
                float sw = swatchStep - 4.0f;
                float sh = 24.0f;
                if (mx >= sx && mx <= sx + sw && my >= sy && my <= sy + sh) {
                    res.hit = true;
                    res.area = TrackInspectorHitArea::ColorSwatch;
                    res.colorSwatchIndex = s;
                    return res;
                }
            }
        }
        curY += cardColorH + gapColor;
    }

    // SECTION 2: Channel Mixer
    if (my >= curY && my <= curY + card2H) {
        if (!isCompact) {
            if (mx >= x + 70.0f && mx <= x + 310.0f && my >= curY + 30.0f && my <= curY + 60.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::VolumeSlider;
                res.normVal = trk.volume;
                return res;
            }
            if (mx >= x + 410.0f && mx <= x + 610.0f && my >= curY + 30.0f && my <= curY + 60.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PanKnob;
                res.normVal = trk.pan;
                return res;
            }
        } else {
            float sliderW = contentW - 120.0f;
            if (mx >= x + 50.0f && mx <= x + 60.0f + sliderW && my >= curY + 22.0f && my <= curY + 48.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::VolumeSlider;
                res.normVal = trk.volume;
                return res;
            }
            if (mx >= x + 50.0f && mx <= x + 60.0f + sliderW && my >= curY + 50.0f && my <= curY + 76.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PanKnob;
                res.normVal = trk.pan;
                return res;
            }
        }
    }
    curY += card2H + gap2;

    // SECTION 3: Dynamic Instrument Faceplate
    if (my >= curY && my <= curY + card3H) {
        if (isCompact) {
            float pW = contentW;
            // < PREV: [x + 6, curY + 4, 32, 20]
            if (mx >= x + 6.0f && mx <= x + 38.0f && my >= curY + 4.0f && my <= curY + 24.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetPrev;
                return res;
            }
            // NEXT >: [x + 42, curY + 4, 32, 20]
            if (mx >= x + 42.0f && mx <= x + 74.0f && my >= curY + 4.0f && my <= curY + 24.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetNext;
                return res;
            }
            // FULL: [x + pW - 50, curY + 4, 44, 20]
            float fullBtnW = 44.0f;
            float fullBtnX = x + pW - fullBtnW - 6.0f;
            if (mx >= fullBtnX && mx <= fullBtnX + fullBtnW && my >= curY + 4.0f && my <= curY + 24.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::TrackTab;
                return res;
            }
            if (my >= curY + 28.0f) {
                const auto* preset = getActivePreset();
                if (preset) {
                    float scale = contentW / designW;
                    float localX = (mx - x) / scale;
                    float localY = (my - (curY + 28.0f)) / scale + 28.0f;
                    HitTestHardwareKnobResult knobHit{};
                    hitTestCompactNodeRecursive(preset->compactGuiRoot, localX, localY, *preset, knobHit);
                    if (knobHit.hit) {
                        res.hit = true;
                        res.area = TrackInspectorHitArea::HardwareKnob;
                        res.paramName = knobHit.paramName;
                        res.normVal = knobHit.currentNormVal;
                        res.position = { x + knobHit.position.x * scale, (curY + 28.0f) + (knobHit.position.y - 28.0f) * scale };
                        return res;
                    }
                }
            }
        } else {
            // Preset Prev: [x + 16, curY + 12, 75, 28]
            if (mx >= x + 16.0f && mx <= x + 91.0f && my >= curY + 12.0f && my <= curY + 40.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetPrev;
                return res;
            }
            // Preset Next: [x + 98, curY + 12, 75, 28]
            if (mx >= x + 98.0f && mx <= x + 173.0f && my >= curY + 12.0f && my <= curY + 40.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::PresetNext;
                return res;
            }

            float scale = 0.70f;
            auto knobHit = hitTestHardwareKnob(mx, my, x, curY, scale);
            if (knobHit.hit) {
                res.hit = true;
                res.area = TrackInspectorHitArea::HardwareKnob;
                res.paramName = knobHit.paramName;
                res.normVal = knobHit.currentNormVal;
                res.position = knobHit.position;
                return res;
            }
        }
    }
    curY += card3H + gap3;

    // SECTION 4: Harmonic Chord Follow
    if (my >= curY && my <= curY + card4H) {
        if (!isCompact) {
            float bakeX = x + contentW - 130.0f;
            if (mx >= bakeX && mx <= bakeX + 115.0f && my >= curY + 12.0f && my <= curY + 38.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::BakeChordsButton;
                return res;
            }
        }
        static const ChordFollowMode modes[5] = {
            ChordFollowMode::Off,
            ChordFollowMode::Chord,
            ChordFollowMode::Bass,
            ChordFollowMode::Scale,
            ChordFollowMode::ColorLead
        };
        float chipX = x + 16.0f;
        float chipY = curY + (isCompact ? 40.0f : 44.0f);
        for (int c = 0; c < 5; ++c) {
            float chipW = (c == 4) ? (isCompact ? 80.0f : 100.0f) : (isCompact ? 60.0f : 78.0f);
            if (mx >= chipX && mx <= chipX + chipW && my >= chipY && my <= chipY + 26.0f) {
                res.hit = true;
                res.area = TrackInspectorHitArea::ChordFollowChip;
                res.chordMode = modes[c];
                return res;
            }
            chipX += chipW + 8.0f;
        }
    }
    curY += card4H + gap4;

    // SECTION 5: MIDI FX
    if (my >= curY && my <= curY + card5H) {
        float modW = (contentW - 48.0f) / 3.0f;
        float m1X = x + 16.0f;
        if (mx >= m1X + modW - 36.0f && mx <= m1X + modW - 8.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpToggle;
            return res;
        }
        if (mx >= m1X + 8.0f && mx <= m1X + 70.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpPattern";
            return res;
        }
        if (mx >= m1X + 75.0f && mx <= m1X + 140.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpRate";
            res.normVal = trk.midiFx.arpRate;
            return res;
        }
        if (mx >= m1X + 145.0f && mx <= m1X + 210.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpOctaves";
            res.normVal = static_cast<float>(trk.midiFx.arpOctaves);
            return res;
        }
        if (mx >= m1X + 75.0f && mx <= m1X + 140.0f && my >= curY + 74.0f && my <= curY + 96.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpGate";
            res.normVal = trk.midiFx.arpGate;
            return res;
        }
        if (mx >= m1X + 145.0f && mx <= m1X + 210.0f && my >= curY + 74.0f && my <= curY + 96.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxArpKnob;
            res.paramName = "arpSwing";
            res.normVal = trk.midiFx.arpSwing;
            return res;
        }

        float m2X = x + 24.0f + modW;
        if (mx >= m2X + modW - 36.0f && mx <= m2X + modW - 8.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxScaleToggle;
            return res;
        }
        if (mx >= m2X + 8.0f && mx <= m2X + 80.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxScaleKnob;
            res.paramName = "scaleRoot";
            return res;
        }
        if (mx >= m2X + 85.0f && mx <= m2X + 170.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxScaleKnob;
            res.paramName = "scaleMinor";
            return res;
        }

        float m3X = x + 32.0f + 2 * modW;
        if (mx >= m3X + modW - 36.0f && mx <= m3X + modW - 8.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxHumanizeToggle;
            return res;
        }
        if (mx >= m3X + 8.0f && mx <= m3X + 100.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxHumanizeKnob;
            res.paramName = "humanizeTiming";
            res.normVal = trk.midiFx.humanizeTiming;
            return res;
        }
        if (mx >= m3X + 105.0f && mx <= m3X + 210.0f && my >= curY + 54.0f && my <= curY + 76.0f) {
            res.hit = true;
            res.area = TrackInspectorHitArea::MidiFxHumanizeKnob;
            res.paramName = "humanizeVelocity";
            res.normVal = trk.midiFx.humanizeVelocity;
            return res;
        }
    }
    curY += card5H + gap5;

    // SECTION 6: AUDIO FX
    if (my >= curY && my <= curY + card6H) {
        float fxW = (contentW - 64.0f) / 5.0f;
        for (int u = 0; u < 5; ++u) {
            float ux = x + 16.0f + u * (fxW + 8.0f);
            if (mx >= ux + fxW - 32.0f && mx <= ux + fxW - 6.0f && my >= curY + 28.0f && my <= curY + 50.0f) {
                res.hit = true;
                if (u == 0) res.area = TrackInspectorHitArea::AudioFxDelayToggle;
                else if (u == 1) res.area = TrackInspectorHitArea::AudioFxChorusToggle;
                else if (u == 2) res.area = TrackInspectorHitArea::AudioFxEqToggle;
                else if (u == 3) res.area = TrackInspectorHitArea::AudioFxCompToggle;
                else if (u == 4) res.area = TrackInspectorHitArea::AudioFxConvolverToggle;
                return res;
            }
            float kw = fxW / 3.0f;
            float ky = curY + 68.0f;
            for (int k = 0; k < 3; ++k) {
                float kx = ux + (k + 0.5f) * kw;
                if (std::hypot(mx - kx, my - ky) <= 16.0f) {
                    res.hit = true;
                    if (u == 0) {
                        res.area = TrackInspectorHitArea::AudioFxDelayKnob;
                        res.paramName = (k == 0) ? "delayTime" : ((k == 1) ? "delayFeedback" : "delayMix");
                        res.normVal = (k == 0) ? trk.audioFx.delayTime : ((k == 1) ? trk.audioFx.delayFeedback : trk.audioFx.delayMix);
                    } else if (u == 1) {
                        res.area = TrackInspectorHitArea::AudioFxChorusKnob;
                        res.paramName = (k == 0) ? "chorusRate" : ((k == 1) ? "chorusDepth" : "chorusMix");
                        res.normVal = (k == 0) ? trk.audioFx.chorusRate : ((k == 1) ? trk.audioFx.chorusDepth : trk.audioFx.chorusMix);
                    } else if (u == 2) {
                        res.area = TrackInspectorHitArea::AudioFxEqKnob;
                        res.paramName = (k == 0) ? "eqLow" : ((k == 1) ? "eqMid" : "eqHigh");
                        res.normVal = (k == 0) ? trk.audioFx.eqLow : ((k == 1) ? trk.audioFx.eqMid : trk.audioFx.eqHigh);
                    } else if (u == 3) {
                        res.area = TrackInspectorHitArea::AudioFxCompKnob;
                        res.paramName = (k == 0) ? "compThreshold" : ((k == 1) ? "compRatio" : "compGain");
                        res.normVal = (k == 0) ? trk.audioFx.compThreshold : ((k == 1) ? trk.audioFx.compRatio : trk.audioFx.compGain);
                    } else if (u == 4) {
                        res.area = TrackInspectorHitArea::AudioFxConvolverKnob;
                        res.paramName = (k == 0) ? "convolverPreset" : "convolverMix";
                        res.normVal = (k == 0) ? static_cast<float>(trk.audioFx.convolverPreset) / 12.0f : trk.audioFx.convolverMix;
                    }
                    return res;
                }
            }
        }
    }

    return res;
}

HitTestTrackInspectorResult GuiWindow::hitTestTrackInspector(float x, float y) const noexcept {
    HitTestTrackInspectorResult res{};
    if (activeView_ != WorkspaceView::Track && activeView_ != WorkspaceView::HardwarePanel) return res;

    const float inspX = 40.0f;
    const float inspY = 66.0f;
    const float inspW = static_cast<float>(width_) - 80.0f;
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float inspH = bPanelY - inspY - 8.0f;

    return hitTestTrackPropertiesContainer(x, y, inspX, inspY, inspW, inspH, selectedTrackIndex_, trackInspectorScrollY_, false);
}

HitTestBrowserResult GuiWindow::hitTestBrowser(float x, float y) const noexcept {
    HitTestBrowserResult res{};
    if (!browserOpen_) return res;

    const float drW = 440.0f;
    const float drX = static_cast<float>(width_) - drW - 16.0f;
    const float drY = 60.0f;
    const float drH = static_cast<float>(height_) - 116.0f;

    if (x < drX || x > drX + drW || y < drY || y > drY + drH) {
        return res;
    }

    res.hit = true;

    // 1. Close button [drX + drW - 32, drY + 8, 24, 24]
    if (x >= (drX + drW - 32.0f) && x <= (drX + drW - 8.0f) && y >= (drY + 8.0f) && y <= (drY + 32.0f)) {
        res.action = BrowserHitAction::Close;
        return res;
    }

    // 2. Tab Selection pills: [drY + 48..drY + 72]
    if (y >= (drY + 48.0f) && y <= (drY + 72.0f)) {
        const float tabW = 130.0f;
        const float tabGap = 8.0f;
        float tab1X = drX + 16.0f;
        float tab2X = drX + 16.0f + tabW + tabGap;
        float tab3X = drX + 16.0f + 2.0f * (tabW + tabGap);

        if (x >= tab1X && x <= tab1X + tabW) {
            res.action = BrowserHitAction::TabPresets;
            return res;
        }
        if (x >= tab2X && x <= tab2X + tabW) {
            res.action = BrowserHitAction::TabMacros;
            return res;
        }
        if (x >= tab3X && x <= tab3X + tabW) {
            res.action = BrowserHitAction::TabHistory;
            return res;
        }
    }

    // 3. Project Action buttons at bottom: [drY + drH - 46..drY + drH - 14]
    float actY = drY + drH - 46.0f;
    if (y >= actY && y <= actY + 32.0f) {
        // [ SAVE ] at [drX + 16, actY, 124, 32]
        if (x >= drX + 16.0f && x <= drX + 140.0f) {
            res.action = BrowserHitAction::SaveProject;
            return res;
        }
        // [ LOAD ] at [drX + 150, actY, 124, 32]
        if (x >= drX + 150.0f && x <= drX + 274.0f) {
            res.action = BrowserHitAction::LoadProject;
            return res;
        }
        // [ BOUNCE ] at [drX + 284, actY, 140, 32]
        if (x >= drX + 284.0f && x <= drX + 424.0f) {
            res.action = BrowserHitAction::BounceMaster;
            return res;
        }
    }

    // 4. Content rows based on active tab:
    if (browserTab_ == BrowserTab::Presets) {
        // Category selection pills [drY + 76..drY + 102]
        if (y >= (drY + 76.0f) && y <= (drY + 102.0f)) {
            const char* cats[6] = {"ALL", "BASS", "LEAD", "DRUMS", "KEYS", "FX"};
            const float catW = 58.0f;
            const float catGap = 8.0f;
            for (int c = 0; c < 6; ++c) {
                float cx = drX + 16.0f + c * (catW + catGap);
                if (x >= cx && x <= cx + catW) {
                    res.action = BrowserHitAction::CategorySelect;
                    res.category = cats[c];
                    return res;
                }
            }
        }

        // Preset list rows [drY + 110..actY - 8]
        float rowStartY = drY + 110.0f;
        float rowH = 34.0f;
        float rowGap = 4.0f;
        size_t visibleIdx = 0;
        for (size_t i = 0; i < presets_.size(); ++i) {
            const auto& p = presets_[i];
            if (browserCategory_ != "ALL") {
                std::string catUpper = p.metadata.category;
                std::transform(catUpper.begin(), catUpper.end(), catUpper.begin(), ::toupper);
                std::string idUpper = p.metadata.id;
                std::transform(idUpper.begin(), idUpper.end(), idUpper.begin(), ::toupper);
                std::string engUpper = p.metadata.engineId;
                std::transform(engUpper.begin(), engUpper.end(), engUpper.begin(), ::toupper);

                if (catUpper.find(browserCategory_) == std::string::npos &&
                    idUpper.find(browserCategory_) == std::string::npos &&
                    engUpper.find(browserCategory_) == std::string::npos) {
                    continue;
                }
            }

            float ry = rowStartY + visibleIdx * (rowH + rowGap);
            if (ry + rowH > actY - 8.0f) break;

            if (y >= ry && y <= ry + rowH && x >= drX + 16.0f && x <= drX + drW - 16.0f) {
                if (x >= (drX + drW - 74.0f) && x <= (drX + drW - 16.0f)) {
                    res.action = BrowserHitAction::PresetLoad;
                } else {
                    res.action = BrowserHitAction::PresetSelect;
                }
                res.presetIndex = i;
                return res;
            }
            visibleIdx++;
        }
    } else if (browserTab_ == BrowserTab::Macros) {
        // Macro list rows [drY + 84..actY - 8]
        const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
        float rowStartY = drY + 84.0f;
        float rowH = 48.0f;
        float rowGap = 6.0f;

        for (size_t i = 0; i < macros.size(); ++i) {
            float ry = rowStartY + i * (rowH + rowGap);
            if (ry + rowH > actY - 8.0f) break;

            if (y >= ry && y <= ry + rowH && x >= drX + 16.0f && x <= drX + drW - 16.0f) {
                if (x >= (drX + drW - 74.0f) && x <= (drX + drW - 16.0f)) {
                    res.action = BrowserHitAction::MacroRun;
                    res.macroIndex = i;
                    return res;
                }
            }
        }
    } else if (browserTab_ == BrowserTab::History) {
        float tbY = drY + 76.0f;
        if (y >= tbY && y <= tbY + 26.0f) {
            // [ UNDO ]
            if (x >= drX + 16.0f && x <= drX + 106.0f) {
                res.action = BrowserHitAction::HistoryUndo;
                return res;
            }
            // [ REDO ]
            if (x >= drX + 112.0f && x <= drX + 202.0f) {
                res.action = BrowserHitAction::HistoryRedo;
                return res;
            }
            // [ + CHECKPOINT ]
            if (x >= drX + 208.0f && x <= drX + 324.0f) {
                res.action = BrowserHitAction::HistoryMilestone;
                return res;
            }
            // [ CLEAR ]
            if (x >= drX + 330.0f && x <= drX + 424.0f) {
                res.action = BrowserHitAction::HistoryClear;
                return res;
            }
        }

        // Timeline item clicks
        float listY = drY + 124.0f;
        float itemH = 34.0f;
        float itemGap = 4.0f;
        float maxListH = (actY - 146.0f) - listY;

        auto timeline = diffHistory_.getTimeline();
        size_t curIdx = diffHistory_.getCurrentTimelineIndex();
        size_t maxItems = static_cast<size_t>(std::max(1, static_cast<int>(maxListH / (itemH + itemGap))));
        size_t startItem = 0;
        if (curIdx >= maxItems) {
            startItem = curIdx - maxItems + 1;
        }

        for (size_t i = startItem; i < timeline.size() && (i - startItem) < maxItems; ++i) {
            float iy = listY + (i - startItem) * (itemH + itemGap);
            if (y >= iy && y <= iy + itemH && x >= drX + 16.0f && x <= drX + drW - 16.0f) {
                res.action = BrowserHitAction::HistoryStepSelect;
                res.historyStepIndex = i;
                return res;
            }
        }
    }

    return res;
}

HitTestProjectHubResult GuiWindow::hitTestProjectHub(float x, float y) const noexcept {
    HitTestProjectHubResult res{};
    if (!projectHubOpen_) return res;

    DialogLayout dl = computeDialogLayout(540.0f, 580.0f);
    const float hubX = dl.x;
    const float hubY = dl.y;
    const float hubW = dl.w;
    const float hubH = dl.h;

    if (x < hubX || x > hubX + hubW || y < hubY || y > hubY + hubH) {
        return res;
    }

    res.hit = true;

    // 1. Close button (Top-Right screw button)
    if (x >= (dl.closeBtnX - 4.0f) && x <= (dl.closeBtnX + dl.closeBtnW + 4.0f) &&
        y >= (dl.closeBtnY - 4.0f) && y <= (dl.closeBtnY + dl.closeBtnH + 4.0f)) {
        res.action = ProjectHubAction::Close;
        return res;
    }

    // 2. Close button (Bottom-Right CLOSE button)
    if (x >= (dl.bottomCloseX - 6.0f) && x <= (dl.bottomCloseX + dl.bottomCloseW + 6.0f) &&
        y >= (dl.bottomCloseY - 4.0f) && y <= (dl.bottomCloseY + dl.bottomCloseH + 6.0f)) {
        res.action = ProjectHubAction::Close;
        return res;
    }

    const float topContentY = hubY + 50.0f;
    const float footerY = hubY + hubH - 36.0f;
    const float headerH = 30.0f;
    const float headerGap = 6.0f;
    const float headerStep = headerH + headerGap; // 36.0f
    const int numSections = 6;
    const float availableDrawerH = (footerY - topContentY) - (numSections * headerStep) - headerGap;

    float curDrawerH = 0.0f;
    if (projectHubSection_ >= 0) {
        if (projectHubSection_ == 0) curDrawerH = 190.0f;
        else if (projectHubSection_ == 1) curDrawerH = 114.0f;
        else if (projectHubSection_ == 2) curDrawerH = 244.0f;
        else if (projectHubSection_ == 3) curDrawerH = availableDrawerH;
        else if (projectHubSection_ == 4) curDrawerH = 100.0f;
        else if (projectHubSection_ == 5) curDrawerH = 100.0f;
    }

    const float drawerX = hubX + 16.0f;
    const float drawerW = hubW - 32.0f;
    const float drawerY = topContentY + (projectHubSection_ + 1) * headerStep;
    const float drawerH = curDrawerH;

    // 1. Check Section Headers hit FIRST (headers always receive clicks over underlying drawer bleed)
    for (int s = 0; s < 6; ++s) {
        float hY = 0.0f;
        if (projectHubSection_ == -1 || s <= projectHubSection_) {
            hY = topContentY + s * headerStep;
        } else {
            hY = drawerY + drawerH + headerGap + (s - (projectHubSection_ + 1)) * headerStep;
        }

        if (y >= hY && y <= hY + 30.0f && x >= hubX + 12.0f && x <= hubX + hubW - 12.0f) {
            res.action = ProjectHubAction::SectionHeader;
            res.sectionIndex = s;
            return res;
        }
    }

    // 2. If a section is open, check hit within drawer bounds
    if (projectHubSection_ >= 0 && x >= drawerX && x <= drawerX + drawerW && y >= drawerY && y <= drawerY + drawerH) {
        if (projectHubSection_ == 0) {
            float contentStartY = drawerY;
            if (y >= contentStartY + 6.0f && y <= contentStartY + 48.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::TitleClick;
                return res;
            }
            if (y >= contentStartY + 54.0f && y <= contentStartY + 96.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::AuthorClick;
                return res;
            }
            const float btnW = (hubW - 48.0f - 16.0f) / 3.0f;
            const float row1Y = contentStartY + 104.0f;
            const float row2Y = contentStartY + 144.0f;

            if (y >= row1Y && y <= row1Y + 34.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 24.0f + btnW) { res.action = ProjectHubAction::SaveProject; return res; }
                if (x >= hubX + 32.0f + btnW && x <= hubX + 32.0f + 2.0f * btnW) { res.action = ProjectHubAction::SaveAsProject; return res; }
                if (x >= hubX + 40.0f + 2.0f * btnW && x <= hubX + 40.0f + 3.0f * btnW) { res.action = ProjectHubAction::LoadProject; return res; }
            } else if (y >= row2Y && y <= row2Y + 34.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 24.0f + btnW) { res.action = ProjectHubAction::NewProject; return res; }
                if (x >= hubX + 32.0f + btnW && x <= hubX + 32.0f + 2.0f * btnW) { res.action = ProjectHubAction::BounceWav; return res; }
                if (x >= hubX + 40.0f + 2.0f * btnW && x <= hubX + 40.0f + 3.0f * btnW) { res.action = ProjectHubAction::OpenScriptView; return res; }
            }
        } else if (projectHubSection_ == 1) {
            float contentStartY = drawerY;
            if (y >= contentStartY + 6.0f && y <= contentStartY + 34.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleRestoreSession;
                return res;
            }
            if (y >= contentStartY + 38.0f && y <= contentStartY + 66.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleAutosave;
                return res;
            }
            if (y >= contentStartY + 74.0f && y <= contentStartY + 104.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ResetCleanSlate;
                return res;
            }
        } else if (projectHubSection_ == 2) {
            float contentStartY = drawerY;
            if (y >= contentStartY + 24.0f && y <= contentStartY + 50.0f) {
                const float scales[5] = {1.0f, 1.10f, 1.25f, 1.50f, 2.0f};
                for (int sc = 0; sc < 5; ++sc) {
                    float cx = hubX + 24.0f + sc * 64.0f;
                    if (x >= cx && x <= cx + 58.0f) {
                        res.action = ProjectHubAction::SetUiScale;
                        res.scaleValue = scales[sc];
                        return res;
                    }
                }
            }
            if (y >= contentStartY + 68.0f && y <= contentStartY + 94.0f) {
                for (int th = 0; th < 5; ++th) {
                    float tx = hubX + 24.0f + th * 95.0f;
                    if (x >= tx && x <= tx + 88.0f) {
                        res.action = ProjectHubAction::SelectTheme;
                        res.themeIndex = th;
                        return res;
                    }
                }
            }
            if (y >= contentStartY + 116.0f && y <= contentStartY + 144.0f) {
                if (x >= hubX + 24.0f && x <= hubX + 124.0f) { res.action = ProjectHubAction::SetAntiAliasing; res.aaMode = 0; return res; }
                if (x >= hubX + 132.0f && x <= hubX + 232.0f) { res.action = ProjectHubAction::SetAntiAliasing; res.aaMode = 1; return res; }
                if (x >= hubX + 240.0f && x <= hubX + 350.0f) { res.action = ProjectHubAction::SetAntiAliasing; res.aaMode = 2; return res; }
            }
            if (y >= contentStartY + 154.0f && y <= contentStartY + 178.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleHiDpi;
                return res;
            }
            if (y >= contentStartY + 180.0f && y <= contentStartY + 204.0f) {
                if (x >= hubX + hubW - 170.0f && x <= hubX + hubW - 80.0f) { res.action = ProjectHubAction::OpenCrtTweaker; return res; }
                if (x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) { res.action = ProjectHubAction::ToggleCrtShader; return res; }
            }
            if (y >= contentStartY + 206.0f && y <= contentStartY + 230.0f &&
                x >= hubX + 24.0f && x <= hubX + hubW - 24.0f) {
                res.action = ProjectHubAction::ToggleAnimations;
                return res;
            }
        } else if (projectHubSection_ == 3) {
            // Check scrollbar hit within drawer
            if (projectHubScrollArea_.canScroll() && projectHubScrollArea_.getScrollbarTrackBounds().contains(x, y)) {
                res.action = ProjectHubAction::Scrollbar;
                return res;
            }

            float contentStartY = drawerY - projectHubScrollY_;

            // CRT Switch & Live HUD Button
            if (y >= contentStartY + 6.0f && y <= contentStartY + 30.0f) {
                if (x >= drawerX + drawerW - 180.0f && x <= drawerX + drawerW - 72.0f) {
                    res.action = ProjectHubAction::OpenCrtTweaker;
                    return res;
                }
                if (x >= drawerX + drawerW - 60.0f && x <= drawerX + drawerW - 16.0f) {
                    res.action = ProjectHubAction::ToggleCrtShader;
                    return res;
                }
            }

            // Presets
            if (y >= contentStartY + 38.0f && y <= contentStartY + 64.0f) {
                if (x >= drawerX + 74.0f && x <= drawerX + 166.0f) { res.action = ProjectHubAction::CrtPresetStudioRef; return res; }
                if (x >= drawerX + 172.0f && x <= drawerX + 270.0f) { res.action = ProjectHubAction::CrtPresetMaxClarity; return res; }
                if (x >= drawerX + 276.0f && x <= drawerX + 378.0f) { res.action = ProjectHubAction::CrtPresetWarmVintage; return res; }
                if (x >= drawerX + 384.0f && x <= drawerX + 452.0f) { res.action = ProjectHubAction::CrtPresetReset; return res; }
            }

            // Sliders 0..10
            const float sTrackX = drawerX + 12.0f;
            const float sTrackW = drawerW - 32.0f;
            for (int i = 0; i < 11; ++i) {
                float rowY = contentStartY + 76.0f + i * 46.0f;
                if (y >= rowY + 6.0f && y <= rowY + 36.0f && x >= sTrackX - 4.0f && x <= sTrackX + sTrackW + 4.0f) {
                    res.action = ProjectHubAction::CrtSlider;
                    res.crtSliderIndex = i;
                    return res;
                }
            }

            res.action = ProjectHubAction::ContentDrag;
            return res;
        }
    }
    return res;
}

HitTestEatscriptResult GuiWindow::hitTestEatscript(float x, float y) const noexcept {
    HitTestEatscriptResult res{};
    if (activeView_ != WorkspaceView::Design) return res;

    // 1. Top Sub-Navigation Tabs: [Y = 60..95]
    if (y >= 60.0f && y <= 95.0f) {
        // [ MODULAR RACK ] at [30, 64, 125, 28]
        if (x >= 30.0f && x <= 155.0f) {
            res.hit = true;
            res.action = HitTestEatscriptResult::Action::SubNavModular;
            return res;
        }
        // [ EATSCRIPT IDE ] at [165, 64, 125, 28]
        if (x >= 165.0f && x <= 290.0f) {
            res.hit = true;
            res.action = HitTestEatscriptResult::Action::SubNavEatscript;
            return res;
        }
        // [ GUI DESIGNER ] at [300, 64, 125, 28]
        if (x >= 300.0f && x <= 425.0f) {
            res.hit = true;
            res.action = HitTestEatscriptResult::Action::SubNavGuiDesigner;
            return res;
        }

        // When in Eatscript sub-view, check action toolbar and template buttons
        if (designSubView_ == DesignSubView::Eatscript) {
            // [ COMPILE (F5) ] at [445, 64, 115, 28]
            if (x >= 445.0f && x <= 560.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::Compile;
                return res;
            }
            // [ HOT-RELOAD ] at [570, 64, 135, 28]
            if (x >= 570.0f && x <= 705.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::HotReload;
                return res;
            }
            // [ AOT C++ ] at [715, 64, 95, 28]
            if (x >= 715.0f && x <= 810.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::ToggleAotView;
                return res;
            }

            // Template buttons at Y = 64..92
            // Template 0 [ ACID 303 ] at [825, 64, 80, 28]
            if (x >= 825.0f && x <= 905.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 0;
                return res;
            }
            // Template 1 [ DUAL SAW ] at [912, 64, 80, 28]
            if (x >= 912.0f && x <= 992.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 1;
                return res;
            }
            // Template 2 [ DELAY ] at [999, 64, 75, 28]
            if (x >= 999.0f && x <= 1074.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 2;
                return res;
            }
            // Template 3 [ FM BELL ] at [1081, 64, 78, 28]
            if (x >= 1081.0f && x <= 1159.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 3;
                return res;
            }
            // Template 4 [ DISTORT ] at [1166, 64, 78, 28]
            if (x >= 1166.0f && x <= 1244.0f) {
                res.hit = true;
                res.action = HitTestEatscriptResult::Action::LoadTemplate;
                res.templateIndex = 4;
                return res;
            }
        }
    }

    if (designSubView_ != DesignSubView::Eatscript) return res;

    // 2. Code Editor Canvas area: [X = 30..820, Y = 134..700]
    if (x >= 30.0f && x <= 820.0f && y >= 134.0f && y <= 700.0f) {
        res.hit = true;
        res.action = HitTestEatscriptResult::Action::EditorClick;
        int line = static_cast<int>((y - 138.0f) / 22.0f);
        int col = static_cast<int>((x - 90.0f) / 8.5f);
        if (col < 0) col = 0;
        res.line = line;
        res.col = col;
        return res;
    }

    // 3. Right Panel Bind Track Button: [850, 260, 220, 32]
    if (x >= 850.0f && x <= 1070.0f && y >= 260.0f && y <= 292.0f) {
        res.hit = true;
        res.action = HitTestEatscriptResult::Action::BindTrack;
        return res;
    }

    return res;
}

void GuiWindow::setDesignSubView(DesignSubView subView) {
    designSubView_ = subView;
    if (modularDesignView_) {
        if (subView == DesignSubView::ModularRack) {
            modularDesignView_->setSubMode(DesignSubMode::ModularRack);
        } else if (subView == DesignSubView::Eatscript) {
            modularDesignView_->setSubMode(DesignSubMode::Code);
        } else if (subView == DesignSubView::GuiDesigner) {
            modularDesignView_->setSubMode(DesignSubMode::GuiDesigner);
        }
    }
    if (subView == DesignSubView::Eatscript) {
        if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (trk && !trk->getEatscriptCode().empty()) {
                setScriptCode(trk->getEatscriptCode());
            } else if (activePresetIndex_ < presets_.size() && !presets_[activePresetIndex_].rawScript.empty()) {
                setScriptCode(presets_[activePresetIndex_].rawScript);
            }
        } else if (activePresetIndex_ < presets_.size() && !presets_[activePresetIndex_].rawScript.empty()) {
            setScriptCode(presets_[activePresetIndex_].rawScript);
        }
    }
}

void GuiWindow::initEatscriptIde() {
    scriptTemplates_.clear();

    scriptTemplates_.push_back({"ACID 303",
        "# @engine: tb303\n"
        "# ACID 303 Voice\n"
        "def init():\n"
        "    return {\n"
        "        \"Cutoff\": 1500.0,\n"
        "        \"Resonance\": 0.75\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Roland TB-303 Acid Bassline Oscillator\n"
        "    val = math.sin(2.0 * math.pi * freq * time)\n"
        "    return math.tanh(val * 2.5) * 0.75\n"
    });

    scriptTemplates_.push_back({"DUAL SAW",
        "# DUAL SAW Voice with detune\n"
        "def init():\n"
        "    return {\n"
        "        \"Detune\": 1.008,\n"
        "        \"Gain\": 0.70\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Stereo Detuned Saw Voice\n"
        "    s1 = math.sin(2.0 * math.pi * freq * time)\n"
        "    s2 = math.sin(2.0 * math.pi * freq * 1.008 * time)\n"
        "    return (s1 + s2) * 0.4\n"
    });

    scriptTemplates_.push_back({"DELAY FX",
        "# DELAY FX with feedback\n"
        "def init():\n"
        "    return {\n"
        "        \"Feedback\": 0.45,\n"
        "        \"Mix\": 0.35\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Modulated Echo Effect\n"
        "    carrier = math.sin(2.0 * math.pi * freq * time)\n"
        "    echo = math.sin(2.0 * math.pi * freq * (time - 0.05))\n"
        "    return carrier * 0.6 + echo * 0.25\n"
    });

    scriptTemplates_.push_back({"FM BELL",
        "# FM BELL Voice with mod\n"
        "def init():\n"
        "    return {\n"
        "        \"Ratio\": 2.76,\n"
        "        \"Depth\": 3.50\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # 2-Operator FM Synthesis\n"
        "    mod = math.sin(2.0 * math.pi * freq * 2.76 * time) * 3.5\n"
        "    car = math.sin(2.0 * math.pi * freq * time + mod)\n"
        "    return car * 0.7\n"
    });

    scriptTemplates_.push_back({"DISTORT",
        "# DISTORT Waveshaping Saturation with tanh\n"
        "def init():\n"
        "    return {\n"
        "        \"Drive\": 4.0,\n"
        "        \"Ceiling\": 0.8\n"
        "    }\n"
        "\n"
        "def process(time, freq, note, params):\n"
        "    # Non-linear Waveshaping Saturation\n"
        "    raw = math.sin(2.0 * math.pi * freq * time)\n"
        "    return math.tanh(raw * 4.0) * 0.8\n"
    });

    loadScriptTemplate(0);
}

std::string GuiWindow::getScriptCode() const {
    std::string code;
    for (size_t i = 0; i < scriptBuffer_.size(); ++i) {
        code += scriptBuffer_[i];
        if (i + 1 < scriptBuffer_.size()) {
            code += "\n";
        }
    }
    return code;
}

void GuiWindow::setScriptCode(const std::string& code) {
    scriptBuffer_.clear();
    std::stringstream ss(code);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        scriptBuffer_.push_back(line);
    }
    if (scriptBuffer_.empty()) {
        scriptBuffer_.push_back("");
    }
    scriptCursorLine_ = 0;
    scriptCursorCol_ = 0;
    compileActiveScript();
}

void GuiWindow::setScriptCursor(int line, int col) noexcept {
    if (scriptBuffer_.empty()) {
        scriptBuffer_.push_back("");
    }
    if (line < 0) line = 0;
    if (line >= static_cast<int>(scriptBuffer_.size())) {
        line = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    scriptCursorLine_ = line;
    int lineLen = static_cast<int>(scriptBuffer_[line].size());
    if (col < 0) col = 0;
    if (col > lineLen) col = lineLen;
    scriptCursorCol_ = col;
}

void GuiWindow::insertScriptChar(char c) {
    if (scriptBuffer_.empty()) scriptBuffer_.push_back("");
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& line = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ > static_cast<int>(line.size())) {
        scriptCursorCol_ = static_cast<int>(line.size());
    }
    line.insert(line.begin() + scriptCursorCol_, c);
    scriptCursorCol_++;
}

void GuiWindow::insertScriptText(const std::string& text) {
    for (char c : text) {
        if (c == '\n') {
            insertScriptNewLine();
        } else if (c != '\r') {
            insertScriptChar(c);
        }
    }
}

void GuiWindow::deleteScriptCharBackwards() {
    if (scriptBuffer_.empty()) return;
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& line = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ > 0) {
        if (scriptCursorCol_ <= static_cast<int>(line.size())) {
            line.erase(line.begin() + scriptCursorCol_ - 1);
            scriptCursorCol_--;
        }
    } else if (scriptCursorLine_ > 0) {
        int prevLineIdx = scriptCursorLine_ - 1;
        int prevLineLen = static_cast<int>(scriptBuffer_[prevLineIdx].size());
        scriptBuffer_[prevLineIdx] += line;
        scriptBuffer_.erase(scriptBuffer_.begin() + scriptCursorLine_);
        scriptCursorLine_ = prevLineIdx;
        scriptCursorCol_ = prevLineLen;
    }
}

void GuiWindow::deleteScriptCharForwards() {
    if (scriptBuffer_.empty()) return;
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& line = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ < static_cast<int>(line.size())) {
        line.erase(line.begin() + scriptCursorCol_);
    } else if (scriptCursorLine_ + 1 < static_cast<int>(scriptBuffer_.size())) {
        line += scriptBuffer_[scriptCursorLine_ + 1];
        scriptBuffer_.erase(scriptBuffer_.begin() + scriptCursorLine_ + 1);
    }
}

void GuiWindow::insertScriptNewLine() {
    if (scriptBuffer_.empty()) scriptBuffer_.push_back("");
    if (scriptCursorLine_ >= static_cast<int>(scriptBuffer_.size())) {
        scriptCursorLine_ = static_cast<int>(scriptBuffer_.size()) - 1;
    }
    auto& curLine = scriptBuffer_[scriptCursorLine_];
    if (scriptCursorCol_ > static_cast<int>(curLine.size())) {
        scriptCursorCol_ = static_cast<int>(curLine.size());
    }

    std::string leftPart = curLine.substr(0, scriptCursorCol_);
    std::string remaining = curLine.substr(scriptCursorCol_);

    std::string indent;
    for (char ch : leftPart) {
        if (ch == ' ' || ch == '\t') indent += ch;
        else break;
    }
    std::string trimmed = leftPart;
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) trimmed.pop_back();
    if (!trimmed.empty() && trimmed.back() == ':') {
        indent += "    ";
    }

    curLine = leftPart;
    scriptBuffer_.insert(scriptBuffer_.begin() + scriptCursorLine_ + 1, indent + remaining);
    scriptCursorLine_++;
    scriptCursorCol_ = static_cast<int>(indent.size());
}

void GuiWindow::loadScriptTemplate(size_t index) {
    if (index < scriptTemplates_.size()) {
        setScriptCode(scriptTemplates_[index].second);
        setStatusMessage("LOADED TEMPLATE: " + scriptTemplates_[index].first);
    }
}
std::string GuiWindow::getScriptTemplateName(size_t index) const {
    if (index < scriptTemplates_.size()) {
        return scriptTemplates_[index].first;
    }
    return "";
}

bool GuiWindow::compileActiveScript() {
    std::cout << "[compileActiveScript] Starting..." << std::endl;
    std::string src = getScriptCode();
    scriptDisassembly_.clear();
    scriptError_.clear();
    scriptErrorLine_ = -1;

    auto nativeTarget = eatscript::NativeDispatchScanner::detectTarget(src);
    if (nativeTarget != eatscript::NativeDispatchTarget::None) {
        scriptCompiled_ = true;
        scriptDisassembly_.push_back(std::string("0000: NATIVE_SIMD_DISPATCH ") + eatscript::NativeDispatchScanner::targetToString(nativeTarget));
        auto extracted = eatscript::NativeDispatchScanner::extractParamsFromInit(src);
        for (const auto& [k, v] : extracted) {
            scriptDisassembly_.push_back(std::string("PARAM: ") + k + " = " + std::to_string(v));
        }
        eatscript::Transpiler transpiler;
        scriptTranspiledCode_ = transpiler.transpileSource(src, "eatscript_dsp", "Live Eatscript Voice");
        scriptStatusMsg_ = std::string("Native DSP Dispatched (") + eatscript::NativeDispatchScanner::targetToString(nativeTarget) + ")";
        return true;
    }

    try {
        std::cout << "[compileActiveScript] Parsing..." << std::endl;
        eatscript::Lexer lexer(src);
        auto tokens = lexer.tokenize();
        eatscript::Parser parser(std::move(tokens));
        activeProgram_ = parser.parse();
        if (!activeProgram_) {
            scriptCompiled_ = false;
            scriptError_ = "Parser returned empty program";
            scriptErrorLine_ = 1;
            scriptStatusMsg_ = "Parse Error";
            return false;
        }

        std::cout << "[compileActiveScript] Compiling VM..." << std::endl;
        eatscript::VM vm;
        if (!vm.compileProgram(*activeProgram_)) {
            scriptCompiled_ = false;
            scriptError_ = "Failed to compile bytecode chunk";
            scriptErrorLine_ = 1;
            scriptStatusMsg_ = "Bytecode Error";
            return false;
        }

        std::cout << "[compileActiveScript] Disassembling bytecode..." << std::endl;
        const auto& chunk = vm.getProcessChunk();
        size_t ip = 0;
        int instNum = 0;
        while (ip < chunk.code.size()) {
            uint8_t opByte = chunk.code[ip++];
            eatscript::OpCode op = static_cast<eatscript::OpCode>(opByte);
            char lineBuf[96];
            std::string opName;
            std::string detail;

            switch (op) {
                case eatscript::OpCode::Constant: {
                    uint8_t cIdx = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    double val = (cIdx < chunk.constants.size()) ? chunk.constants[cIdx] : 0.0;
                    opName = "OP_CONSTANT";
                    detail = std::to_string(val);
                    break;
                }
                case eatscript::OpCode::GetLocal: {
                    uint8_t slot = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_GETLOCAL";
                    detail = "slot " + std::to_string(slot);
                    break;
                }
                case eatscript::OpCode::SetLocal: {
                    uint8_t slot = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_SETLOCAL";
                    detail = "slot " + std::to_string(slot);
                    break;
                }
                case eatscript::OpCode::GetParam: {
                    uint8_t p = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_GETPARAM";
                    detail = "param " + std::to_string(p);
                    break;
                }
                case eatscript::OpCode::Add: opName = "OP_ADD"; break;
                case eatscript::OpCode::Subtract: opName = "OP_SUBTRACT"; break;
                case eatscript::OpCode::Multiply: opName = "OP_MULTIPLY"; break;
                case eatscript::OpCode::Divide: opName = "OP_DIVIDE"; break;
                case eatscript::OpCode::Modulo: opName = "OP_MODULO"; break;
                case eatscript::OpCode::Power: opName = "OP_POWER"; break;
                case eatscript::OpCode::Negate: opName = "OP_NEGATE"; break;
                case eatscript::OpCode::Equal: opName = "OP_EQUAL"; break;
                case eatscript::OpCode::NotEqual: opName = "OP_NOTEQUAL"; break;
                case eatscript::OpCode::Less: opName = "OP_LESS"; break;
                case eatscript::OpCode::LessEqual: opName = "OP_LESSEQUAL"; break;
                case eatscript::OpCode::Greater: opName = "OP_GREATER"; break;
                case eatscript::OpCode::GreaterEqual: opName = "OP_GREATEREQUAL"; break;
                case eatscript::OpCode::MathSin: opName = "OP_MATHSIN"; break;
                case eatscript::OpCode::MathCos: opName = "OP_MATHCOS"; break;
                case eatscript::OpCode::MathTanh: opName = "OP_MATHTANH"; break;
                case eatscript::OpCode::MathExp: opName = "OP_MATHEXP"; break;
                case eatscript::OpCode::MathFloor: opName = "OP_MATHFLOOR"; break;
                case eatscript::OpCode::MathSqrt: opName = "OP_MATHSQRT"; break;
                case eatscript::OpCode::MathRandom: opName = "OP_MATHRANDOM"; break;
                case eatscript::OpCode::Jump: {
                    uint8_t hi = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    uint8_t lo = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_JUMP";
                    detail = "offset " + std::to_string((hi << 8) | lo);
                    break;
                }
                case eatscript::OpCode::JumpIfFalse: {
                    uint8_t hi = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    uint8_t lo = (ip < chunk.code.size()) ? chunk.code[ip++] : 0;
                    opName = "OP_JUMPIFFALSE";
                    detail = "offset " + std::to_string((hi << 8) | lo);
                    break;
                }
                case eatscript::OpCode::Return: opName = "OP_RETURN"; break;
                default: opName = "OP_UNKNOWN (" + std::to_string(opByte) + ")"; break;
            }

            std::snprintf(lineBuf, sizeof(lineBuf), "%04d: %-15s %s", instNum++, opName.c_str(), detail.c_str());
            scriptDisassembly_.push_back(lineBuf);
        }

        std::cout << "[compileActiveScript] Transpiling..." << std::endl;
        eatscript::Transpiler transpiler;
        scriptTranspiledCode_ = transpiler.transpile(*activeProgram_, "eatscript_dsp", "Live Eatscript Voice");
        std::cout << "[compileActiveScript] Transpiled successfully!" << std::endl;

        scriptCompiled_ = true;
        scriptStatusMsg_ = "Compiled Successfully (" + std::to_string(scriptDisassembly_.size()) + " instructions)";
        std::cout << "[compileActiveScript] Done!" << std::endl;
        return true;
    } catch (const std::exception& e) {
        scriptCompiled_ = false;
        scriptError_ = e.what();
        scriptErrorLine_ = 1;
        scriptStatusMsg_ = "Error: " + scriptError_;
        return false;
    } catch (...) {
        scriptCompiled_ = false;
        scriptError_ = "Unknown compilation error";
        scriptErrorLine_ = 1;
        scriptStatusMsg_ = "Compilation Error";
        return false;
    }
}

bool GuiWindow::hotReloadScriptToTrack(uint32_t trackIndex) {
    if (!compileActiveScript()) {
        return false;
    }
    if (!engine_) return false;

    std::string code = getScriptCode();
    auto& seq = engine_->getSequencer();
    auto* trk = (trackIndex < seq.getNumTracks()) ? seq.getTrack(trackIndex) : nullptr;
    if (trk) {
        trk->setEatscriptCode(code);
    }

    auto dispatchTarget = eatscript::NativeDispatchScanner::detectTarget(code);
    if (dispatchTarget != eatscript::NativeDispatchTarget::None) {
        auto extracted = eatscript::NativeDispatchScanner::extractParamsFromInit(code);
        if (activePresetIndex_ < presets_.size()) {
            auto& p = presets_[activePresetIndex_];
            for (const auto& [k, v] : extracted) {
                if (auto* param = p.findParam(k)) {
                    param->currentVal = v;
                }
            }
        }
        scriptTargetTrack_ = trackIndex;
        if (trk) {
            trk->setName("EATSCRIPT");
        }
        setStatusMessage(std::string("HOT-RELOADED NATIVE DSP (") + eatscript::NativeDispatchScanner::targetToString(dispatchTarget) + ") TO TRACK 0" + std::to_string(trackIndex + 1));
        return true;
    }

    auto& graph = engine_->getGraph();

    std::shared_ptr<audio::EatscriptNode> targetNode;
    const auto& nodes = graph.getNodes();
    for (const auto& [id, node] : nodes) {
        if (auto es = std::dynamic_pointer_cast<audio::EatscriptNode>(node)) {
            targetNode = es;
            break;
        }
    }

    if (!targetNode) {
        targetNode = std::make_shared<audio::EatscriptNode>("LiveEatscript");
        audio::NodeId esId = graph.addNode(targetNode);
        audio::NodeId outId = graph.getOutputNodeId();
        if (outId != 0 && outId != esId) {
            graph.connect(esId, 0, outId, 0);
        }
        graph.compile();
    }

    targetNode->setScript(code);

    if (trk) {
        trk->setName("EATSCRIPT");
    }

    scriptTargetTrack_ = trackIndex;
    setStatusMessage("HOT-RELOADED EATSCRIPT INTO TRACK 0" + std::to_string(trackIndex + 1));
    return true;
}

void GuiWindow::initNoteScriptEditor() {
    noteScriptBuffer_.clear();
    noteScriptBuffer_.push_back("# Clip Notes (Eatscript Declarative Event Notepad)");
    noteScriptBuffer_.push_back("# Syntax: { pitch = \"C3\", step = 0, dur = 0.75, vel = 0.80, slide = false, accent = false }");
    noteScriptBuffer_.push_back("");
    noteScriptBuffer_.push_back("{ pitch = \"C3\", step = 0, dur = 0.75, vel = 0.85 }");
    noteScriptBuffer_.push_back("{ pitch = \"D#3\", step = 3, dur = 0.75, vel = 0.90, slide = true, accent = true }");
    noteScriptBuffer_.push_back("{ pitch = \"G3\", step = 7, dur = 0.75, vel = 0.80 }");
    noteScriptBuffer_.push_back("{ pitch = \"A#3\", step = 10, dur = 0.75, vel = 0.95, slide = true }");
    noteScriptCursorLine_ = 3;
    noteScriptCursorCol_ = 0;
    noteScriptScrollY_ = 0.0f;
    noteScriptDirty_ = false;
    noteScriptStatusMsg_ = "Ready";
    noteScriptError_ = "";
    noteScriptErrorLine_ = -1;
    noteScriptLastTrack_ = 9999;
}

void GuiWindow::initArrangerTracks() {
    arrangerTracks_.clear();

    // Track 0: 303 Acid Bass (Synth)
    {
        ArrangerTrackData t0;
        t0.name = "303 Acid Bass";
        t0.type = "SYNTH";
        t0.r = 1.0f; t0.g = 0.55f; t0.b = 0.0f;
        t0.volume = 0.8f;
        t0.eqLow = 0.5f; t0.eqMid = 0.65f; t0.eqHigh = 0.55f;

        ArrangerClip c1;
        c1.name = "Acid Pattern A";
        c1.startBar = 1;
        c1.barLength = 4;
        c1.isLooped = false;
        c1.loopLengthBars = 4;
        c1.r = 1.0f; c1.g = 0.55f; c1.b = 0.0f;
        t0.clips.push_back(c1);

        ArrangerClip c2;
        c2.name = "Acid Pattern B";
        c2.startBar = 5;
        c2.barLength = 4;
        c2.isLooped = false;
        c2.loopLengthBars = 4;
        c2.r = 1.0f; c2.g = 0.55f; c2.b = 0.0f;
        t0.clips.push_back(c2);

        arrangerTracks_.push_back(t0);
    }

    // Track 1: TR-808 Kit (Sampler)
    {
        ArrangerTrackData t1;
        t1.name = "TR-808 Kit";
        t1.type = "SAMPLER";
        t1.r = 0.13f; t1.g = 0.96f; t1.b = 0.91f;
        t1.volume = 0.85f;
        t1.eqLow = 0.70f; t1.eqMid = 0.50f; t1.eqHigh = 0.60f;

        ArrangerClip c1;
        c1.name = "808 Beat 01";
        c1.startBar = 1;
        c1.barLength = 8;
        c1.isLooped = true;
        c1.loopLengthBars = 4;
        c1.r = 0.13f; c1.g = 0.96f; c1.b = 0.91f;
        t1.clips.push_back(c1);

        arrangerTracks_.push_back(t1);
    }

    // Track 2: TR-909 Drive (Drums)
    {
        ArrangerTrackData t2;
        t2.name = "TR-909 Drive";
        t2.type = "SAMPLER";
        t2.r = 1.0f; t2.g = 0.16f; t2.b = 0.43f;
        t2.volume = 0.80f;
        t2.eqLow = 0.80f; t2.eqMid = 0.50f; t2.eqHigh = 0.60f;

        ArrangerClip c1;
        c1.name = "909 Groove";
        c1.startBar = 5;
        c1.barLength = 4;
        c1.isLooped = false;
        c1.loopLengthBars = 4;
        c1.r = 1.0f; c1.g = 0.16f; c1.b = 0.43f;
        t2.clips.push_back(c1);

        arrangerTracks_.push_back(t2);
    }

    // Track 3: DX7 Rhodes (FM Electric Piano)
    {
        ArrangerTrackData t3;
        t3.name = "DX7 Rhodes";
        t3.type = "SYNTH";
        t3.r = 0.62f; t3.g = 0.31f; t3.b = 0.87f;
        t3.volume = 0.75f;
        t3.eqLow = 0.40f; t3.eqMid = 0.60f; t3.eqHigh = 0.70f;

        ArrangerClip c1;
        c1.name = "Chords A";
        c1.startBar = 1;
        c1.barLength = 8;
        c1.isLooped = false;
        c1.loopLengthBars = 8;
        c1.r = 0.62f; c1.g = 0.31f; c1.b = 0.87f;
        t3.clips.push_back(c1);

        arrangerTracks_.push_back(t3);
    }

    // Track 4: Concert Grand (Physical Modeling Piano)
    {
        ArrangerTrackData t4;
        t4.name = "Concert Grand";
        t4.type = "PHYSICAL";
        t4.r = 0.88f; t4.g = 0.66f; t4.b = 0.43f;
        t4.volume = 0.75f;
        t4.eqLow = 0.50f; t4.eqMid = 0.50f; t4.eqHigh = 0.50f;

        ArrangerClip c1;
        c1.name = "Piano Solo";
        c1.startBar = 9;
        c1.barLength = 8;
        c1.isLooped = false;
        c1.loopLengthBars = 8;
        c1.r = 0.88f; c1.g = 0.66f; c1.b = 0.43f;
        t4.clips.push_back(c1);

        arrangerTracks_.push_back(t4);
    }
}

void GuiWindow::setArrangerPropertiesWidth(float width) noexcept {
    arrangerPropertiesWidth_ = std::clamp(width, kArrangerPropertiesMinW, kArrangerPropertiesMaxW);
}

void GuiWindow::setMixerPropertiesWidth(float width) noexcept {
    mixerPropertiesWidth_ = std::clamp(width, kMixerPropertiesMinW, kMixerPropertiesMaxW);
}

void GuiWindow::handleTrackInspectorInteraction(const HitTestTrackInspectorResult& inspHit, float x, float y) {
    uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
    auto& trk = arrangerTracks_[tIdx];
    switch (inspHit.area) {
        case TrackInspectorHitArea::ScrollbarThumb:
        case TrackInspectorHitArea::ScrollbarTrack:
            dragMode_ = DragMode::TrackInspectorScrollbar;
            dragStartY_ = y;
            dragStartScrollY_ = trackInspectorScrollY_;
            break;
        case TrackInspectorHitArea::PresetPrev:
            prevPreset();
            break;
        case TrackInspectorHitArea::PresetNext:
            nextPreset();
            break;
        case TrackInspectorHitArea::TrackTab:
            syncTrackToPreset(inspHit.trackIndex);
            activeView_ = WorkspaceView::Track;
            break;
        case TrackInspectorHitArea::CodeButton:
            activeView_ = WorkspaceView::Design;
            designSubView_ = DesignSubView::Eatscript;
            break;
        case TrackInspectorHitArea::MuteButton:
            trk.mute = !trk.mute;
            if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].mute = trk.mute;
            if (engine_ && tIdx < engine_->getSequencer().getNumTracks()) {
                auto* tr = engine_->getSequencer().getTrack(tIdx);
                if (tr) tr->setMuted(trk.mute);
            }
            recordProjectHistory("Toggle Mute on Track " + std::to_string(tIdx + 1), "TRACK");
            break;
        case TrackInspectorHitArea::SoloButton:
            trk.solo = !trk.solo;
            if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].solo = trk.solo;
            if (engine_ && tIdx < engine_->getSequencer().getNumTracks()) {
                auto* tr = engine_->getSequencer().getTrack(tIdx);
                if (tr) tr->setSolo(trk.solo);
            }
            recordProjectHistory("Toggle Solo on Track " + std::to_string(tIdx + 1), "TRACK");
            break;
        case TrackInspectorHitArea::FreezeButton:
            setTrackFreezeState(tIdx, !trk.freeze);
            break;
        case TrackInspectorHitArea::VolumeSlider:
            dragMode_ = DragMode::TrackInspectorVolume;
            dragStartX_ = x;
            dragStartValGeneric_ = trk.volume;
            break;
        case TrackInspectorHitArea::PanKnob:
            dragMode_ = DragMode::TrackInspectorPan;
            dragStartX_ = x;
            dragStartPan_ = trk.pan;
            break;
        case TrackInspectorHitArea::ChordFollowChip:
            trk.chordFollowMode = inspHit.chordMode;
            recordProjectHistory("Set Chord Follow on Track " + std::to_string(tIdx + 1), "CHORD");
            break;
        case TrackInspectorHitArea::BakeChordsButton:
            recordProjectHistory("Bake Chords on Track " + std::to_string(tIdx + 1), "CHORD");
            scriptStatusMsg_ = "Harmonic chords baked into track " + std::to_string(tIdx + 1);
            break;
        case TrackInspectorHitArea::MidiFxArpToggle:
            trk.midiFx.arpEnabled = !trk.midiFx.arpEnabled;
            recordProjectHistory("Toggle Arp on Track " + std::to_string(tIdx + 1), "MIDI_FX");
            break;
        case TrackInspectorHitArea::MidiFxScaleToggle:
            trk.midiFx.scaleSnapEnabled = !trk.midiFx.scaleSnapEnabled;
            recordProjectHistory("Toggle Scale Snap on Track " + std::to_string(tIdx + 1), "MIDI_FX");
            break;
        case TrackInspectorHitArea::MidiFxHumanizeToggle:
            trk.midiFx.humanizeEnabled = !trk.midiFx.humanizeEnabled;
            recordProjectHistory("Toggle Humanize on Track " + std::to_string(tIdx + 1), "MIDI_FX");
            break;
        case TrackInspectorHitArea::AudioFxDelayToggle:
            trk.audioFx.delayEnabled = !trk.audioFx.delayEnabled;
            recordProjectHistory("Toggle Delay on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxChorusToggle:
            trk.audioFx.chorusEnabled = !trk.audioFx.chorusEnabled;
            recordProjectHistory("Toggle Chorus on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxEqToggle:
            trk.audioFx.eqEnabled = !trk.audioFx.eqEnabled;
            recordProjectHistory("Toggle EQ on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxCompToggle:
            trk.audioFx.compEnabled = !trk.audioFx.compEnabled;
            recordProjectHistory("Toggle Compressor on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::AudioFxConvolverToggle:
            trk.audioFx.convolverEnabled = !trk.audioFx.convolverEnabled;
            recordProjectHistory("Toggle Convolver on Track " + std::to_string(tIdx + 1), "AUDIO_FX");
            break;
        case TrackInspectorHitArea::MidiFxArpKnob:
        case TrackInspectorHitArea::MidiFxScaleKnob:
        case TrackInspectorHitArea::MidiFxHumanizeKnob:
        case TrackInspectorHitArea::AudioFxDelayKnob:
        case TrackInspectorHitArea::AudioFxChorusKnob:
        case TrackInspectorHitArea::AudioFxEqKnob:
        case TrackInspectorHitArea::AudioFxCompKnob:
        case TrackInspectorHitArea::AudioFxConvolverKnob:
            dragMode_ = DragMode::TrackInspectorFxKnob;
            activeTrackInspectorFxParam_ = inspHit.paramName;
            dragStartY_ = y;
            dragStartTrackInspectorVal_ = inspHit.normVal;
            break;
        case TrackInspectorHitArea::HardwareKnob:
            dragMode_ = DragMode::HardwareKnob;
            activeHardwareParam_ = inspHit.paramName;
            dragStartY_ = y;
            dragStartHardwareVal_ = inspHit.normVal;
            break;
        case TrackInspectorHitArea::ColorSwatch: {
            static const float swatches[8][3] = {
                {0.0f, 0.90f, 1.0f},   // Neon Cyan
                {1.0f, 0.55f, 0.0f},   // Neon Amber
                {0.0f, 1.00f, 0.40f},  // Acid Green
                {1.0f, 0.00f, 0.48f},  // Hot Pink
                {0.74f, 0.00f, 1.0f},  // Electric Purple
                {1.0f, 0.20f, 0.20f},  // Crimson Red
                {1.0f, 0.85f, 0.0f},   // Gold Yellow
                {0.20f, 0.60f, 1.0f}   // Sky Blue
            };
            if (inspHit.colorSwatchIndex < 8) {
                trk.r = swatches[inspHit.colorSwatchIndex][0];
                trk.g = swatches[inspHit.colorSwatchIndex][1];
                trk.b = swatches[inspHit.colorSwatchIndex][2];
                for (auto& clp : trk.clips) {
                    clp.r = trk.r;
                    clp.g = trk.g;
                    clp.b = trk.b;
                }
                recordProjectHistory("Change Accent Color on Track " + std::to_string(tIdx + 1), "TRACK");
            }
            break;
        }
        default:
            break;
    }
}

void GuiWindow::selectArrangerClip(int trackIdx, int clipIdx) noexcept {
    selectedArrangerClipTrack_ = trackIdx;
    selectedArrangerClipIndex_ = clipIdx;
    if (trackIdx >= 0 && static_cast<size_t>(trackIdx) < arrangerTracks_.size()) {
        selectedTrackIndex_ = static_cast<uint32_t>(trackIdx);
        arrangerInspectorTab_ = ArrangerInspectorTab::Clip;
    }
}

std::string GuiWindow::getNoteScriptText() const {
    std::ostringstream ss;
    for (size_t i = 0; i < noteScriptBuffer_.size(); ++i) {
        ss << noteScriptBuffer_[i];
        if (i + 1 < noteScriptBuffer_.size()) ss << "\n";
    }
    return ss.str();
}

void GuiWindow::setNoteScriptText(const std::string& text) {
    noteScriptBuffer_.clear();
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        noteScriptBuffer_.push_back(line);
    }
    if (noteScriptBuffer_.empty()) {
        noteScriptBuffer_.push_back("");
    }
    noteScriptCursorLine_ = 0;
    noteScriptCursorCol_ = 0;
    noteScriptDirty_ = true;
}

void GuiWindow::setNoteScriptCursor(int line, int col) noexcept {
    if (noteScriptBuffer_.empty()) {
        noteScriptBuffer_.push_back("");
    }
    if (line < 0) line = 0;
    if (line >= static_cast<int>(noteScriptBuffer_.size())) {
        line = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    noteScriptCursorLine_ = line;
    int lineLen = static_cast<int>(noteScriptBuffer_[line].size());
    if (col < 0) col = 0;
    if (col > lineLen) col = lineLen;
    noteScriptCursorCol_ = col;
}

void GuiWindow::insertNoteScriptChar(char c) {
    if (noteScriptBuffer_.empty()) noteScriptBuffer_.push_back("");
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& line = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ > static_cast<int>(line.size())) {
        noteScriptCursorCol_ = static_cast<int>(line.size());
    }
    line.insert(line.begin() + noteScriptCursorCol_, c);
    noteScriptCursorCol_++;
    noteScriptDirty_ = true;
}

void GuiWindow::insertNoteScriptText(const std::string& text) {
    for (char c : text) {
        if (c == '\n') {
            insertNoteScriptNewLine();
        } else if (c != '\r') {
            insertNoteScriptChar(c);
        }
    }
}

void GuiWindow::deleteNoteScriptCharBackwards() {
    if (noteScriptBuffer_.empty()) return;
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& line = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ > 0) {
        if (noteScriptCursorCol_ <= static_cast<int>(line.size())) {
            line.erase(line.begin() + noteScriptCursorCol_ - 1);
            noteScriptCursorCol_--;
            noteScriptDirty_ = true;
        }
    } else if (noteScriptCursorLine_ > 0) {
        int prevLineIdx = noteScriptCursorLine_ - 1;
        int prevLineLen = static_cast<int>(noteScriptBuffer_[prevLineIdx].size());
        noteScriptBuffer_[prevLineIdx] += line;
        noteScriptBuffer_.erase(noteScriptBuffer_.begin() + noteScriptCursorLine_);
        noteScriptCursorLine_ = prevLineIdx;
        noteScriptCursorCol_ = prevLineLen;
        noteScriptDirty_ = true;
    }
}

void GuiWindow::deleteNoteScriptCharForwards() {
    if (noteScriptBuffer_.empty()) return;
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& line = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ < static_cast<int>(line.size())) {
        line.erase(line.begin() + noteScriptCursorCol_);
        noteScriptDirty_ = true;
    } else if (noteScriptCursorLine_ + 1 < static_cast<int>(noteScriptBuffer_.size())) {
        line += noteScriptBuffer_[noteScriptCursorLine_ + 1];
        noteScriptBuffer_.erase(noteScriptBuffer_.begin() + noteScriptCursorLine_ + 1);
        noteScriptDirty_ = true;
    }
}

void GuiWindow::insertNoteScriptNewLine() {
    if (noteScriptBuffer_.empty()) noteScriptBuffer_.push_back("");
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = static_cast<int>(noteScriptBuffer_.size()) - 1;
    }
    auto& curLine = noteScriptBuffer_[noteScriptCursorLine_];
    if (noteScriptCursorCol_ > static_cast<int>(curLine.size())) {
        noteScriptCursorCol_ = static_cast<int>(curLine.size());
    }

    std::string leftPart = curLine.substr(0, noteScriptCursorCol_);
    std::string remaining = curLine.substr(noteScriptCursorCol_);

    curLine = leftPart;
    noteScriptBuffer_.insert(noteScriptBuffer_.begin() + noteScriptCursorLine_ + 1, remaining);
    noteScriptCursorLine_++;
    noteScriptCursorCol_ = 0;
    noteScriptDirty_ = true;
}

bool GuiWindow::syncNoteScriptToTrack() {
    if (!engine_) return false;
    auto& seq = engine_->getSequencer();
    if (selectedTrackIndex_ >= seq.getNumTracks()) return false;
    auto* tr = seq.getTrack(selectedTrackIndex_);
    if (!tr) return false;

    std::string err;
    int errLine = -1;
    bool ok = eatscript::NoteScriptEngine::parseTrackNotesFromLines(noteScriptBuffer_, *tr, &err, &errLine);
    if (ok) {
        noteScriptDirty_ = false;
        noteScriptError_.clear();
        noteScriptErrorLine_ = -1;
        noteScriptStatusMsg_ = "Applied to " + tr->getName();
        setStatusMessage("APPLIED SCRIPT TO " + tr->getName());
        return true;
    } else {
        noteScriptError_ = err;
        noteScriptErrorLine_ = errLine;
        noteScriptStatusMsg_ = "Error (Line " + std::to_string(errLine) + "): " + err;
        setStatusMessage("SCRIPT SYNTAX ERROR: " + err);
        return false;
    }
}

void GuiWindow::syncTrackToNoteScript() {
    if (!engine_) return;
    auto& seq = engine_->getSequencer();
    if (selectedTrackIndex_ >= seq.getNumTracks()) return;
    auto* tr = seq.getTrack(selectedTrackIndex_);
    if (!tr) return;

    noteScriptBuffer_ = eatscript::NoteScriptEngine::serializeTrackNotesToLines(*tr);
    noteScriptDirty_ = false;
    noteScriptError_.clear();
    noteScriptErrorLine_ = -1;
    noteScriptStatusMsg_ = "Synced from " + tr->getName();
    noteScriptLastTrack_ = selectedTrackIndex_;
    if (noteScriptCursorLine_ >= static_cast<int>(noteScriptBuffer_.size())) {
        noteScriptCursorLine_ = std::max(0, static_cast<int>(noteScriptBuffer_.size()) - 1);
    }
}

void GuiWindow::insertNoteScriptTemplate() {
    noteScriptBuffer_.push_back("{ pitch = \"C3\", step = 0, dur = 0.75, vel = 0.85 }");
    noteScriptBuffer_.push_back("{ pitch = \"D#3\", step = 4, dur = 0.75, vel = 0.80, slide = true }");
    noteScriptBuffer_.push_back("{ pitch = \"F3\", step = 8, dur = 0.75, vel = 0.90 }");
    noteScriptBuffer_.push_back("{ pitch = \"G3\", step = 12, dur = 0.75, vel = 0.95, accent = true }");
    noteScriptDirty_ = true;
    syncNoteScriptToTrack();
}

HitTestNoteScriptResult GuiWindow::hitTestNoteScript(float x, float y) const noexcept {
    HitTestNoteScriptResult res{};
    if (activeView_ != WorkspaceView::Edit && activeView_ != WorkspaceView::Tracker) return res;
    if (editSubView_ != EditSubView::Script) return res;

    // Toolbar buttons: [Y = 98..126]
    if (y >= 98.0f && y <= 126.0f) {
        // [ APPLY TO TRACK ] at [40..230]
        if (x >= 40.0f && x <= 230.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::ApplySync;
            return res;
        }
        // [ REVERT FROM TRACK ] at [240..390]
        if (x >= 240.0f && x <= 390.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::RevertFromTrack;
            return res;
        }
        // [ + 4-BAR ARP ] at [400..520]
        if (x >= 400.0f && x <= 520.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::InsertTemplate;
            return res;
        }
        // [ CLEAR ] at [530..610]
        if (x >= 530.0f && x <= 610.0f) {
            res.hit = true;
            res.action = HitTestNoteScriptResult::Action::Clear;
            return res;
        }
    }

    // Code Editor Canvas: [X = 30..width-30, Y = 134..680]
    float rightBound = static_cast<float>(width_) - 30.0f;
    if (x >= 30.0f && x <= rightBound && y >= 134.0f && y <= 680.0f) {
        res.hit = true;
        res.action = HitTestNoteScriptResult::Action::EditorClick;
        int line = static_cast<int>((y - 138.0f) / 20.0f);
        int col = static_cast<int>((x - 85.0f) / 8.5f);
        if (col < 0) col = 0;
        res.line = line;
        res.col = col;
        return res;
    }

    return res;
}

void GuiWindow::loadPresetToSelectedTrack(size_t presetIndex) {
    if (presetIndex >= presets_.size()) return;
    activePresetIndex_ = presetIndex;
    selectedBrowserPresetIndex_ = presetIndex;
    const auto& p = presets_[presetIndex];

    project::PresetLoader::computeLayoutBounds(presets_[activePresetIndex_].guiRoot,
                                               40.0f, 98.0f,
                                               static_cast<float>(width_) - 80.0f,
                                               static_cast<float>(height_) - 154.0f);

    if (selectedTrackIndex_ < mixerStrips_.size()) {
        mixerStrips_[selectedTrackIndex_].name = p.metadata.name;
    }
    if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
        auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (tr) tr->setName(p.metadata.name);
    }

    for (const auto& [paramName, param] : p.params) {
        dispatchHardwareParam(paramName, param.getNormalized());
    }

    lastStatusMessage_ = "LOADED: " + p.metadata.name;
    recordProjectHistory("Loaded Preset: " + p.metadata.name, "PRESET");
}

void GuiWindow::runMacro(size_t macroIndex) {
    if (!engine_) return;
    const auto& macros = eatscript::MacroRuntime::getBuiltinMacros();
    if (macroIndex >= macros.size()) return;

    eatscript::MacroRuntime runtime;
    auto res = runtime.execute(macros[macroIndex].scriptCode, *engine_, engine_->getSequencer());
    if (res.success) {
        lastStatusMessage_ = "MACRO: " + macros[macroIndex].name + " EXECUTED";
        recordProjectHistory("Ran Macro: " + macros[macroIndex].name, "MACRO");

        if (macroIndex == 4) {
            // Procedural Song: sync arranger timeline tracks & clips
            procgen::SongGenerationParams p;
            p.style = "Lo-Fi Hip Hop";
            p.bars = 16;
            p.seed = 42;
            auto songRes = procgen::ProceduralSongEngine::generateSong(p);
            if (songRes.success && !songRes.generatedTracks.empty()) {
                arrangerTracks_.clear();
                for (const auto& gt : songRes.generatedTracks) {
                    ArrangerTrackData atd;
                    atd.name = gt.name;
                    atd.type = gt.instrumentEngine;
                    atd.volume = gt.volume;
                    atd.pan = gt.pan;
                    atd.r = gt.r;
                    atd.g = gt.g;
                    atd.b = gt.b;
                    for (const auto& c : gt.clips) {
                        ArrangerClip ac;
                        ac.name = c.name;
                        ac.startBar = c.startBar;
                        ac.barLength = c.lengthBars;
                        ac.r = c.r;
                        ac.g = c.g;
                        ac.b = c.b;
                        atd.clips.push_back(ac);
                    }
                    arrangerTracks_.push_back(std::move(atd));
                }
                projectName_ = "Lo-Fi Hip Hop (Procedural)";
                if (engine_) engine_->getSequencer().setBpm(songRes.bpm);
            }
        }

        // If on note script view, synchronize script editor with new track data
        if (selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* trk = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (trk) {
                noteScriptBuffer_ = eatscript::NoteScriptEngine::serializeTrackNotesToLines(*trk);
            }
        }
    } else {
        lastStatusMessage_ = "MACRO ERROR: " + res.message;
    }
}

bool GuiWindow::saveProjectToFile(const std::string& filePath) {
    if (!engine_) return false;
    double bpm = engine_->getSequencer().getBpm();
    double swing = engine_->getSequencer().getSwing();
    bool ok = project::ProjectFile::saveToFile(filePath, engine_->getGraph(), engine_->getSequencer(), projectName_, bpm, swing);
    if (ok) {
        projectFilePath_ = filePath;
        lastStatusMessage_ = "PROJECT SAVED TO " + filePath;
    } else {
        lastStatusMessage_ = "ERROR: FAILED TO SAVE PROJECT";
    }
    return ok;
}

bool GuiWindow::saveProjectAs() {
    std::string suggested = projectName_.empty() ? "my_song.eats" : (projectName_ + ".eats");
    std::string path = promptSaveEatsFile(suggested);
    if (path.empty()) {
        path = suggested;
    }
    return saveProjectToFile(path);
}

bool GuiWindow::loadProjectFromFile(const std::string& filePath) {
    if (!engine_) return false;
    std::string title;
    double bpm = 135.0, swing = 0.50;
    bool ok = project::ProjectFile::loadFromFile(filePath, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing);
    if (ok) {
        if (!title.empty()) {
            projectName_ = title;
        }
        projectFilePath_ = filePath;
        engine_->setEngineMode(audio::SynthEngineMode::ModularGraph);
        engine_->getGraph().compile();
        canvas_.updateRackLayout(engine_->getGraph());
        updateMixerStrips();
        syncArrangerFromSequencer();
        initDefaultKnobValues();
        initHistory();
        lastStatusMessage_ = "LOADED PROJECT: " + (title.empty() ? filePath : title);
    } else {
        lastStatusMessage_ = "ERROR: FAILED TO LOAD " + filePath;
    }
    return ok;
}

bool GuiWindow::loadProjectPrompt() {
    std::string path = promptOpenEatsFile();
    if (path.empty()) {
        path = "project.eats";
    }
    return loadProjectFromFile(path);
}

void GuiWindow::resetToDefaultProject() {
    if (!engine_) return;
    engine_->setupDefaultAcidGraph();
    canvas_.updateRackLayout(engine_->getGraph());
    updateMixerStrips();
    initDefaultKnobValues();
    initArrangerTracks();
    projectName_ = "Untitled Song";
    authorName_ = "Anonymous Producer";
    projectFilePath_ = "project.eats";
    initHistory();
    lastStatusMessage_ = "RESET WORKSPACE TO DEFAULT TEMPLATE";
}

bool GuiWindow::bounceMasterToWav(const std::string& filePath) {
    if (!engine_) return false;
    auto stats = engine_->bounceProject(filePath, 8.0, audio::exporting::WavFormat::Pcm24);
    if (stats.totalFrames > 0) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << stats.speedMultiplier << "x FAST-BOUNCE: " << filePath;
        lastStatusMessage_ = ss.str();
        return true;
    }
    lastStatusMessage_ = "ERROR: BOUNCE FAILED";
    return false;
}

void GuiWindow::initHistory() {
    if (!engine_) return;
    double bpm = engine_->getSequencer().getBpm();
    double swing = engine_->getSequencer().getSwing();
    std::string currentEats = project::EatsProjectSerializer::serialize(
        engine_->getGraph(), engine_->getSequencer(), projectName_, bpm, swing);
    diffHistory_.init(currentEats, "Project Initialized: " + projectName_);
    selectedHistoryIndex_ = 0;
}

void GuiWindow::recordProjectHistory(const std::string& description, const std::string& category, bool isMilestone, const std::string& milestoneName) {
    if (!engine_) return;
    double bpm = engine_->getSequencer().getBpm();
    double swing = engine_->getSequencer().getSwing();
    std::string newEats = project::EatsProjectSerializer::serialize(
        engine_->getGraph(), engine_->getSequencer(), projectName_, bpm, swing);
    bool recorded = diffHistory_.record(newEats, description, category, isMilestone, milestoneName);
    if (recorded) {
        selectedHistoryIndex_ = diffHistory_.getCurrentTimelineIndex();
        historyDirty_ = true;
        setStatusMessage(description);
    }
}

bool GuiWindow::undoHistory() {
    std::string restored;
    if (diffHistory_.undo(restored)) {
        if (engine_) {
            std::string title;
            double bpm = 135.0, swing = 0.50;
            if (project::EatsProjectSerializer::deserialize(restored, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing)) {
                if (!title.empty()) projectName_ = title;
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                updateMixerStrips();
                initDefaultKnobValues();
            }
        }
        selectedHistoryIndex_ = diffHistory_.getCurrentTimelineIndex();
        const auto* cur = diffHistory_.getCurrentEntry();
        std::string desc = cur ? cur->description : "Undo Action";
        setStatusMessage("UNDO: " + desc);
        return true;
    }
    setStatusMessage("Undo: At oldest project state");
    return false;
}

bool GuiWindow::redoHistory() {
    std::string restored;
    if (diffHistory_.redo(restored)) {
        if (engine_) {
            std::string title;
            double bpm = 135.0, swing = 0.50;
            if (project::EatsProjectSerializer::deserialize(restored, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing)) {
                if (!title.empty()) projectName_ = title;
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                updateMixerStrips();
                initDefaultKnobValues();
            }
        }
        selectedHistoryIndex_ = diffHistory_.getCurrentTimelineIndex();
        const auto* cur = diffHistory_.getCurrentEntry();
        std::string desc = cur ? cur->description : "Redo Action";
        setStatusMessage("REDO: " + desc);
        return true;
    }
    setStatusMessage("Redo: At newest project state");
    return false;
}

bool GuiWindow::jumpToHistoryIndex(size_t index) {
    std::string restored;
    if (diffHistory_.jumpToTimelineIndex(index, restored)) {
        if (engine_) {
            std::string title;
            double bpm = 135.0, swing = 0.50;
            if (project::EatsProjectSerializer::deserialize(restored, engine_->getGraph(), engine_->getSequencer(), title, bpm, swing)) {
                if (!title.empty()) projectName_ = title;
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                updateMixerStrips();
                initDefaultKnobValues();
            }
        }
        selectedHistoryIndex_ = index;
        const auto* cur = diffHistory_.getCurrentEntry();
        std::string desc = cur ? cur->description : "Timeline State";
        setStatusMessage("TIME-TRAVEL: #" + std::to_string(index) + " - " + desc);
        return true;
    }
    return false;
}

void GuiWindow::createHistoryMilestone(const std::string& name) {
    diffHistory_.createMilestone(name);
    setStatusMessage("MILESTONE SAVED: " + name);
}

void GuiWindow::clearHistory() {
    diffHistory_.clear();
    selectedHistoryIndex_ = 0;
    setStatusMessage("HISTORY CLEARED (Origin Preserved)");
}

HitTestJackResult GuiWindow::hitTestJack(float x, float y) const noexcept {
    HitTestJackResult res{};
    const auto& modules = canvas_.getModules();

    for (const auto& m : modules) {
        // Test Input Jacks
        for (uint32_t i = 0; i < m.inputJacks.size(); ++i) {
            const auto& pt = m.inputJacks[i];
            const float dist = std::hypot(x - pt.x, y - pt.y);
            if (dist <= 14.0f) {
                res.hit = true;
                res.nodeId = m.id;
                res.portIndex = i;
                res.isOutput = false;
                res.position = pt;
                return res;
            }
        }

        // Test Output Jacks
        for (uint32_t i = 0; i < m.outputJacks.size(); ++i) {
            const auto& pt = m.outputJacks[i];
            const float dist = std::hypot(x - pt.x, y - pt.y);
            if (dist <= 14.0f) {
                res.hit = true;
                res.nodeId = m.id;
                res.portIndex = i;
                res.isOutput = true;
                res.position = pt;
                return res;
            }
        }
    }

    return res;
}

HitTestKnobResult GuiWindow::hitTestKnob(float x, float y) const noexcept {
    HitTestKnobResult res{};
    const auto& modules = canvas_.getModules();

    for (const auto& m : modules) {
        for (size_t k = 0; k < m.knobNames.size(); ++k) {
            const float kx = m.x + 36.0f + ((k % 2) * 90.0f);
            const float ky = m.y + 56.0f + ((k / 2) * 52.0f);
            const float dist = std::hypot(x - kx, y - ky);
            if (dist <= 18.0f) {
                res.hit = true;
                res.nodeId = m.id;
                res.knobIndex = static_cast<uint32_t>(k);
                res.knobName = m.knobNames[k];
                res.position = Point2D{kx, ky};
                return res;
            }
        }
    }

    return res;
}

HitTestStepResult GuiWindow::hitTestStep(float x, float y) const noexcept {
    HitTestStepResult res{};

    if (activeView_ == WorkspaceView::Tracker && engine_) {
        const float startY = 120.0f;
        const float trackH = 88.0f;
        const float gap = 20.0f;
        const float stepW = (static_cast<float>(width_) - 180.0f) / 16.0f;
        const size_t numTracks = engine_->getSequencer().getNumTracks();

        for (size_t t = 0; t < numTracks && t < 4; ++t) {
            float ty = startY + t * (trackH + gap);
            for (uint32_t s = 0; s < 16; ++s) {
                float sx = 140.0f + s * stepW;
                float sy = ty;
                if (x >= sx && x <= (sx + stepW - 4.0f) && y >= sy && y <= (sy + trackH)) {
                    res.hit = true;
                    res.trackIndex = static_cast<uint32_t>(t);
                    res.stepIndex = s;
                    return res;
                }
            }
        }
        return res;
    }

    const float scopeX = 30.0f;
    const float scopeW = 280.0f;
    const float vuX = scopeX + scopeW + 20.0f;
    const float vuW = 50.0f;
    const float seqX = vuX + vuW + 20.0f;
    const float seqY = static_cast<float>(height_) - 180.0f;
    const float seqW = static_cast<float>(width_) - seqX - 30.0f;

    const float stepW = (seqW - 24.0f) / 16.0f;

    for (uint32_t s = 0; s < 16; ++s) {
        const float sx = seqX + 12.0f + (s * stepW);
        const float sy = seqY + 38.0f;
        if (x >= sx && x <= (sx + stepW - 4.0f) && y >= sy && y <= (sy + 60.0f)) {
            res.hit = true;
            res.trackIndex = 0;
            res.stepIndex = s;
            return res;
        }
    }

    return res;
}

HitTestEditSubNavResult GuiWindow::hitTestEditSubNav(float x, float y) const noexcept {
    HitTestEditSubNavResult res{};
    if (y < 60.0f || y > 95.0f) return res;

    // Subnav buttons:
    // [ PIANO ROLL ] : 830..928
    // [ TRACKER ]    : 940..1038
    // [ SCORE ]      : 1050..1148
    // [ SCRIPT ]     : 1160..1258
    if (x >= 830.0f && x <= 928.0f) {
        res.hit = true;
        res.subView = EditSubView::PianoRoll;
    } else if (x >= 940.0f && x <= 1038.0f) {
        res.hit = true;
        res.subView = EditSubView::Tracker;
    } else if (x >= 1050.0f && x <= 1148.0f) {
        res.hit = true;
        res.subView = EditSubView::Score;
    } else if (x >= 1160.0f && x <= 1258.0f) {
        res.hit = true;
        res.subView = EditSubView::Script;
    }
    return res;
}

HitTestPianoKeyResult GuiWindow::hitTestPianoKey(float x, float y) const noexcept {
    HitTestPianoKeyResult res{};
    const float kbX = 20.0f;
    const float kbW = pianoRollKeyboard_.getKeyWidth();
    const float topY = 100.0f;
    const float bottomY = 650.0f;
    const float kbH = bottomY - topY;

    auto hit = pianoRollKeyboard_.hitTest(x, y, kbX, topY, kbW, kbH, pianoRollScrollY_);
    if (hit.hit) {
        res.hit = true;
        res.pitch = static_cast<uint8_t>(hit.pitch);
        res.isBlackKey = hit.isBlack;
        res.velocity = hit.velocity;
    }
    return res;
}

HitTestPianoRollGridResult GuiWindow::hitTestPianoRollGrid(float x, float y) const noexcept {
    HitTestPianoRollGridResult res{};
    const float gridX = 110.0f;
    const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
    const float scrollbarW = 12.0f;
    const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
    const float topY = 100.0f;
    const float bottomY = 650.0f;

    if (x < gridX || x > (gridX + gridW) || y < topY || y > bottomY) {
        return res;
    }

    uint32_t step = static_cast<uint32_t>((x - gridX + pianoRollScrollX_) / pianoRollStepW_);
    int pitch = pianoRollKeyboard_.verticalYToPitch(y - topY, pianoRollScrollY_);

    res.hit = true;
    res.step = step;
    res.pitch = static_cast<uint8_t>(pitch);

    if (engine_) {
        auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
        if (tr && step < tr->getNumSteps()) {
            const auto& st = tr->getStep(step);
            if (st.active && st.note == pitch) {
                res.hasExistingNote = true;
            }
        }
    }
    return res;
}

HitTestPianoRollScrollbarResult GuiWindow::hitTestPianoRollScrollbar(float mx, float my) const noexcept {
    HitTestPianoRollScrollbarResult res{};
    const float gridX = 110.0f;
    const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
    const float scrollbarW = 12.0f;
    const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
    const float topY = 100.0f;
    const float bottomY = 650.0f;
    const float sbH = bottomY - topY;

    // 1. Vertical Scrollbar (on right of grid)
    const float vsbX = gridX + gridW + 2.0f;
    const float vsbY = topY;
    if (mx >= vsbX && mx <= vsbX + scrollbarW && my >= vsbY && my <= vsbY + sbH) {
        res.hit = true;
        res.isVertical = true;
        float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
        float thumbH = std::clamp((sbH / std::max(totalH, sbH)) * sbH, 24.0f, sbH);
        float maxScrollY = std::max(0.0f, totalH - sbH);
        float thumbY = vsbY + (maxScrollY > 0.0f ? (pianoRollScrollY_ / maxScrollY) * (sbH - thumbH) : 0.0f);
        if (my >= thumbY && my <= thumbY + thumbH) {
            res.isThumb = true;
        }
        res.normOffset = std::clamp((my - vsbY) / sbH, 0.0f, 1.0f);
        return res;
    }

    // 2. Horizontal Scrollbar (at bottom of grid)
    const float hsbX = gridX;
    const float hsbY = bottomY + 2.0f;
    const float hsbH = 10.0f;
    if (mx >= hsbX && mx <= hsbX + gridW && my >= hsbY && my <= hsbY + hsbH) {
        res.hit = true;
        res.isVertical = false;
        const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                        ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                        : 64u;
        float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
        float thumbW = std::clamp((gridW / std::max(totalW, gridW)) * gridW, 24.0f, gridW);
        float maxScrollX = std::max(0.0f, totalW - gridW);
        float thumbX = hsbX + (maxScrollX > 0.0f ? (pianoRollScrollX_ / maxScrollX) * (gridW - thumbW) : 0.0f);
        if (mx >= thumbX && mx <= thumbX + thumbW) {
            res.isThumb = true;
        }
        res.normOffset = std::clamp((mx - hsbX) / gridW, 0.0f, 1.0f);
        return res;
    }

    return res;
}

void GuiWindow::setPianoRollScrollX(float scrollX) noexcept {
    const float gridX = 110.0f;
    const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                             engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
    const float scrollbarW = 12.0f;
    const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
    const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                    ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                    : 64u;
    float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
    float maxScrollX = std::max(0.0f, totalW - gridW);
    pianoRollScrollX_ = std::clamp(scrollX, 0.0f, maxScrollX);
}

void GuiWindow::setPianoRollScrollY(float scrollY) noexcept {
    const float topY = 100.0f;
    const float bottomY = 650.0f;
    const float sbH = bottomY - topY;
    float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
    float maxScrollY = std::max(0.0f, totalH - sbH);
    pianoRollScrollY_ = std::clamp(scrollY, 0.0f, maxScrollY);
}

void GuiWindow::setPianoRollStepWidth(float stepW) noexcept {
    pianoRollStepW_ = std::clamp(stepW, 16.0f, 160.0f);
}

void GuiWindow::setPianoRollRowHeight(float rowH) noexcept {
    pianoRollRowH_ = std::clamp(rowH, 12.0f, 48.0f);
    pianoRollKeyboard_.setKeyHeight(pianoRollRowH_);
}

void GuiWindow::centerPianoRollOnNotes() noexcept {
    if (!engine_ || selectedTrackIndex_ >= engine_->getSequencer().getNumTracks()) return;
    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
    if (!tr) return;

    int minNote = 127;
    int maxNote = 0;
    int noteCount = 0;
    for (uint32_t s = 0; s < tr->getNumSteps(); ++s) {
        const auto& st = tr->getStep(s);
        if (st.active) {
            minNote = std::min(minNote, static_cast<int>(st.note));
            maxNote = std::max(maxNote, static_cast<int>(st.note));
            noteCount++;
        }
    }

    int avgNote = (noteCount > 0) ? ((minNote + maxNote) / 2) : 60;
    const float viewportH = 550.0f;
    float targetY = static_cast<float>(pianoRollKeyboard_.getMaxPitch() - avgNote) * pianoRollRowH_ - viewportH * 0.5f;
    setPianoRollScrollY(targetY);
}

void GuiWindow::selectPianoRollNotesByPitch(int pitch, bool additive) {
    if (!engine_ || selectedTrackIndex_ >= engine_->getSequencer().getNumTracks()) return;
    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
    if (!tr) return;

    std::vector<uint32_t> matching;
    for (uint32_t s = 0; s < tr->getNumSteps(); ++s) {
        const auto& st = tr->getStep(s);
        if (st.active && st.note == pitch) {
            matching.push_back(s);
        }
    }

    if (!matching.empty()) {
        tr->selectSteps(matching, !additive);
        if (engine_) {
            engine_->postNoteOn(static_cast<uint8_t>(pitch), 0.85f);
        }
        previewingPitch_ = pitch;
        previewingVelocity_ = 0.85f;
    }
}

bool GuiWindow::isShiftPressed() const noexcept {
    if (mockShiftPressed_) return true;
    if (!window_) return false;
    return (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
}

void GuiWindow::setVirtualKeyboardBaseOctave(int octave) noexcept {
    virtualKeyboardBaseOctave_ = std::clamp(octave, 1, 6);
    virtualPianoDrawerKeyboard_.setBaseOctave(virtualKeyboardBaseOctave_);
}

HitTestVirtualKeyboardDrawerResult GuiWindow::hitTestVirtualKeyboardDrawer(float mx, float my) const noexcept {
    HitTestVirtualKeyboardDrawerResult res{};
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float tabW = 140.0f;
    const float tabH = 20.0f;
    const float tabX = (static_cast<float>(width_) - tabW) * 0.5f;

    float animProg = guiAnimationsEnabled_ ? virtualKeyboardAnimProgress_ : (virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f);
    float currentH = virtualKeyboardDrawerHeight_ * animProg;
    float drawerY = bPanelY - currentH;
    float tabY = drawerY - tabH;

    // Pull-tab hit test (active in both open and closed animated states)
    if (mx >= tabX && mx <= tabX + tabW && my >= tabY && my <= (currentH > 0.0f ? drawerY + 4.0f : bPanelY)) {
        res.hit = true;
        res.isPullTab = true;
        return res;
    }

    if (animProg > 0.2f && currentH > 30.0f) {
        // Inside Drawer Body
        if (my >= drawerY && my <= bPanelY && mx >= 0.0f && mx <= static_cast<float>(width_)) {
            res.hit = true;

            // Octave controls in toolbar [y: drawerY+3..drawerY+25]
            if (my >= drawerY + 3.0f && my <= drawerY + 25.0f) {
                // [< OCT] button
                if (mx >= 240.0f && mx <= 300.0f) {
                    res.isOctaveDown = true;
                    return res;
                }
                // [OCT >] button
                if (mx >= 420.0f && mx <= 480.0f) {
                    res.isOctaveUp = true;
                    return res;
                }
            }

            // Keyboard area
            float kx = 16.0f;
            float ky = drawerY + 28.0f;
            float kw = static_cast<float>(width_) - 32.0f;
            float kh = currentH - 34.0f;

            if (kh > 20.0f && my >= ky && my <= ky + kh && mx >= kx && mx <= kx + kw) {
                auto keyHit = virtualPianoDrawerKeyboard_.hitTest(mx, my, kx, ky, kw, kh, 0.0f);
                if (keyHit.hit) {
                    res.isKey = true;
                    res.pitch = keyHit.pitch;
                    res.velocity = keyHit.velocity;
                    return res;
                }
            }
        }
    }

    return res;
}

void GuiWindow::drawVirtualKeyboardDrawer() {
    const float bPanelY = static_cast<float>(height_) - 48.0f;
    const float tabW = 140.0f;
    const float tabH = 20.0f;
    const float tabX = (static_cast<float>(width_) - tabW) * 0.5f;
    const auto& theme = getTheme();

    // Smooth ease-in animation step (speed matching ProjectBrowserDrawer)
    float target = virtualKeyboardDrawerOpen_ ? 1.0f : 0.0f;
    constexpr float speed = 28.0f;
    virtualKeyboardAnimProgress_ += (target - virtualKeyboardAnimProgress_) * std::clamp(0.016f * speed, 0.0f, 1.0f);
    if (std::abs(virtualKeyboardAnimProgress_ - target) < 0.005f) {
        virtualKeyboardAnimProgress_ = target;
    }

    float currentH = virtualKeyboardDrawerHeight_ * virtualKeyboardAnimProgress_;
    float drawerY = bPanelY - currentH;
    float tabY = drawerY - tabH;

    if (virtualKeyboardAnimProgress_ <= 0.001f) {
        // Closed: draw pull-tab docked above bottom bar
        float closedTabY = bPanelY - tabH;
        drawRoundedRectGradient(tabX, closedTabY, tabW, tabH + 4.0f, 4.0f,
                                theme.controlBackground, theme.controlWell);
        drawRoundedRectOutline(tabX, closedTabY, tabW, tabH + 4.0f, 4.0f,
                               theme.borderSubtle, 1.0f);
        if (batchRenderer_) {
            drawPianoIcon(*batchRenderer_, tabX + 16.0f, closedTabY + 4.5f, 16.0f, 11.0f, theme.primaryAccent);
            drawCenteredText(*batchRenderer_, "VIRTUAL PIANO", tabX + 36.0f, closedTabY, tabW - 44.0f, tabH, 10.5f, theme.primaryAccent);
        } else {
            drawVectorString("VIRTUAL PIANO", tabX + 38.0f, closedTabY + 5.0f, 0.72f, theme.primaryAccent);
        }
        return;
    }

    // Pull Tab above drawer sliding smoothly
    drawRoundedRectGradient(tabX, tabY, tabW, tabH + 4.0f, 4.0f,
                            theme.controlBackground, theme.controlWell);
    drawRoundedRectOutline(tabX, tabY, tabW, tabH + 4.0f, 4.0f,
                           theme.borderSubtle, 1.0f);
    if (batchRenderer_) {
        drawPianoIcon(*batchRenderer_, tabX + 16.0f, tabY + 4.5f, 16.0f, 11.0f, theme.secondaryAccent);
        drawCenteredText(*batchRenderer_, "VIRTUAL PIANO", tabX + 36.0f, tabY, tabW - 44.0f, tabH, 10.5f, theme.secondaryAccent);
    } else {
        drawVectorString("VIRTUAL PIANO", tabX + 38.0f, tabY + 5.0f, 0.72f, theme.secondaryAccent);
    }

    // Chassis background with drop shadow and sleek bevel following theme
    drawRect(0.0f, drawerY - 4.0f, static_cast<float>(width_), 4.0f, 0.0f, 0.0f, 0.0f, 0.45f * virtualKeyboardAnimProgress_);
    drawRectGradient(0.0f, drawerY, static_cast<float>(width_), currentH,
                     theme.panelBackground.lighten(0.04f).r, theme.panelBackground.lighten(0.04f).g, theme.panelBackground.lighten(0.04f).b,
                     theme.panelBackground.darken(0.12f).r, theme.panelBackground.darken(0.12f).g, theme.panelBackground.darken(0.12f).b);
    drawLine(0.0f, drawerY, static_cast<float>(width_), drawerY,
             theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.9f * virtualKeyboardAnimProgress_, 1.5f);

    if (virtualKeyboardAnimProgress_ <= 0.15f || currentH <= 40.0f) {
        return;
    }

    // Top Header / Toolbar inside Drawer
    drawVectorString("VIRTUAL PIANO KEYBOARD", 20.0f, drawerY + 6.0f, 0.85f, theme.primaryAccent);
    drawVectorString("| TOUCH / DRAG GLISSANDO & VELOCITY", 210.0f, drawerY + 6.0f, 0.70f, theme.textMuted);

    // Octave controls
    // [< OCT] button
    drawRoundedRect(240.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                     theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 1.0f);
    drawRoundedRectOutline(240.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 1.0f, 1.0f);
    drawVectorStringCentered("< OCT", 270.0f, drawerY + 14.0f, 0.70f, theme.textPrimary);

    // Current Octave Label
    std::string octText = "OCTAVE: C" + std::to_string(virtualKeyboardBaseOctave_);
    drawVectorString(octText, 320.0f, drawerY + 6.5f, 0.75f, theme.secondaryAccent);

    // [OCT >] button
    drawRoundedRect(420.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                     theme.controlBackground.r, theme.controlBackground.g, theme.controlBackground.b, 1.0f);
    drawRoundedRectOutline(420.0f, drawerY + 3.0f, 60.0f, 22.0f, 3.0f,
                           theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 1.0f, 1.0f);
    drawVectorStringCentered("OCT >", 450.0f, drawerY + 14.0f, 0.70f, theme.textPrimary);

    // Keyboard geometry
    const float kx = 16.0f;
    const float ky = drawerY + 28.0f;
    const float kw = static_cast<float>(width_) - 32.0f;
    const float kh = currentH - 34.0f;
    if (kh <= 10.0f) return;

    int startPitch = (virtualKeyboardBaseOctave_ + 1) * 12;
    int octavesCount = virtualPianoDrawerKeyboard_.getOctavesCount();
    int totalNotes = octavesCount * 12;
    int numWhiteKeys = octavesCount * 7;
    float whiteKeyW = kw / static_cast<float>(numWhiteKeys);
    float blackKeyW = whiteKeyW * 0.65f;
    float blackKeyH = kh * 0.60f;

    // Pass 1: Render White Keys
    int currWhite = 0;
    for (int p = startPitch; p < startPitch + totalNotes; ++p) {
        if (!PianoKeyboard::isBlackKey(p)) {
            float wx = kx + static_cast<float>(currWhite) * whiteKeyW;
            bool isPressed = (previewingPitch_ == p || virtualKeyboardActivePitch_ == p);

            if (isPressed) {
                drawRectGradient(wx, ky, whiteKeyW - 1.0f, kh,
                                 theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b,
                                 theme.primaryAccent.darken(0.35f).r, theme.primaryAccent.darken(0.35f).g, theme.primaryAccent.darken(0.35f).b);
                // Dynamic Velocity Fill Bar (extending from bottom up, matching top-is-higher velocity)
                float fillH = std::clamp(kh * previewingVelocity_, 4.0f, kh);
                drawRect(wx, ky + kh - fillH, whiteKeyW - 1.0f, fillH,
                         theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b, 0.90f);
            } else {
                drawRectGradient(wx, ky, whiteKeyW - 1.0f, kh, 0.92f, 0.94f, 0.97f, 0.76f, 0.78f, 0.82f);
            }
            drawRectOutline(wx, ky, whiteKeyW - 1.0f, kh, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.85f, 1.0f);

            // Note label on white key bottom lip
            if (p % 12 == 0) {
                std::string cLabel = "C" + std::to_string(p / 12 - 1);
                Color lblCol = isPressed ? Color(1.0f, 1.0f, 1.0f, 1.0f) : theme.textMuted;
                drawVectorString(cLabel, wx + 4.0f, ky + kh - 18.0f, 0.75f, lblCol);
            }
            currWhite++;
        }
    }

    // Pass 2: Render Black Keys
    currWhite = 0;
    for (int p = startPitch; p < startPitch + totalNotes; ++p) {
        if (PianoKeyboard::isBlackKey(p)) {
            float cx = kx + static_cast<float>(currWhite) * whiteKeyW;
            float bx = cx - blackKeyW * 0.5f;
            bool isPressed = (previewingPitch_ == p || virtualKeyboardActivePitch_ == p);

            if (isPressed) {
                drawRectGradient(bx, ky, blackKeyW, blackKeyH,
                                 theme.secondaryAccent.r, theme.secondaryAccent.g, theme.secondaryAccent.b,
                                 theme.secondaryAccent.darken(0.40f).r, theme.secondaryAccent.darken(0.40f).g, theme.secondaryAccent.darken(0.40f).b);
                // Dynamic Velocity Fill Bar (extending from bottom up, matching top-is-higher velocity)
                float fillH = std::clamp(blackKeyH * previewingVelocity_, 4.0f, blackKeyH);
                drawRect(bx, ky + blackKeyH - fillH, blackKeyW, fillH,
                         theme.primaryAccent.r, theme.primaryAccent.g, theme.primaryAccent.b, 0.95f);
            } else {
                drawRectGradient(bx, ky, blackKeyW, blackKeyH, 0.22f, 0.24f, 0.30f, 0.08f, 0.09f, 0.12f);
            }
            drawRectOutline(bx, ky, blackKeyW, blackKeyH, theme.borderSubtle.r, theme.borderSubtle.g, theme.borderSubtle.b, 0.90f, 1.0f);
        } else {
            currWhite++;
        }
    }
}

Rect GuiWindow::getMarqueeRect() const noexcept {
    float mx = std::min(marqueeStartX_, marqueeCurX_);
    float my = std::min(marqueeStartY_, marqueeCurY_);
    float mw = std::abs(marqueeCurX_ - marqueeStartX_);
    float mh = std::abs(marqueeCurY_ - marqueeStartY_);
    return {mx, my, mw, mh};
}

void GuiWindow::updatePianoRollMarqueeSelection(float startX, float startY, float curX, float curY, bool isShift) {
    if (!engine_ || selectedTrackIndex_ >= engine_->getSequencer().getNumTracks()) return;
    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
    if (!tr) return;

    const int minPitch = pianoRollKeyboard_.getMinPitch();
    const int maxPitch = pianoRollKeyboard_.getMaxPitch();
    const float topY = 100.0f;
    const float rowH = pianoRollRowH_;
    const float gridX = 110.0f;
    const float stepW = pianoRollStepW_;

    float rLeft = std::min(startX, curX);
    float rRight = std::max(startX, curX);
    float rTop = std::min(startY, curY);
    float rBottom = std::max(startY, curY);

    std::vector<uint32_t> matchedSteps;
    for (uint32_t s = 0; s < tr->getNumSteps(); ++s) {
        const auto& st = tr->getStep(s);
        if (st.active && st.note >= minPitch && st.note <= maxPitch) {
            float nx = gridX + static_cast<float>(s) * stepW - pianoRollScrollX_ + 2.0f;
            float ny = topY + static_cast<float>(maxPitch - st.note) * rowH - pianoRollScrollY_ + 2.0f;
            float nw = std::max(14.0f, (stepW * st.gateLength) - 4.0f);
            float nh = rowH - 4.0f;

            if (nx < rRight && (nx + nw) > rLeft && ny < rBottom && (ny + nh) > rTop) {
                matchedSteps.push_back(s);
            }
        }
    }

    if (isShift) {
        for (uint32_t s : matchedSteps) {
            tr->selectStep(s, false);
        }
    } else {
        tr->selectSteps(matchedSteps, true);
    }
}

HitTestSelectionSidebarResult GuiWindow::hitTestSelectionSidebar(float mx, float my, float x, float y, float width, float height, const sequencer::SequencerTrack& track) const noexcept {
    HitTestSelectionSidebarResult res{};
    (void)track;
    if (mx < x || mx > x + width || my < y || my > y + height) return res;

    res.hit = true;

    // Header buttons
    float delX = x + width - 68.0f;
    float delY = y + 5.0f;
    if (mx >= delX && mx <= delX + 32.0f && my >= delY && my <= delY + 22.0f) {
        res.action = SelectionSidebarAction::Delete;
        return res;
    }

    float closeX = x + width - 30.0f;
    float closeY = y + 5.0f;
    if (mx >= closeX && mx <= closeX + 22.0f && my >= closeY && my <= closeY + 22.0f) {
        res.action = SelectionSidebarAction::Close;
        return res;
    }

    // Transpose buttons (y + 100)
    float curY = y + 100.0f;
    if (my >= curY && my <= curY + 26.0f) {
        if (mx >= x + 10.0f && mx <= x + 50.0f) {
            res.action = SelectionSidebarAction::TransposeOctDown;
            res.paramValue = -12;
            return res;
        }
        if (mx >= x + 54.0f && mx <= x + 90.0f) {
            res.action = SelectionSidebarAction::TransposeSemiDown;
            res.paramValue = -1;
            return res;
        }
        if (mx >= x + width - 90.0f && mx <= x + width - 54.0f) {
            res.action = SelectionSidebarAction::TransposeSemiUp;
            res.paramValue = 1;
            return res;
        }
        if (mx >= x + width - 50.0f && mx <= x + width - 10.0f) {
            res.action = SelectionSidebarAction::TransposeOctUp;
            res.paramValue = 12;
            return res;
        }
    }

    // Step nudge buttons (y + 144)
    curY = y + 144.0f;
    if (my >= curY && my <= curY + 30.0f) {
        if (mx >= x + width - 128.0f && mx <= x + width - 72.0f) {
            res.action = SelectionSidebarAction::NudgeLeft;
            res.paramValue = -1;
            return res;
        }
        if (mx >= x + width - 68.0f && mx <= x + width - 12.0f) {
            res.action = SelectionSidebarAction::NudgeRight;
            res.paramValue = 1;
            return res;
        }
    }

    // Duration buttons (y + 174..220)
    curY = y + 174.0f;
    if (my >= curY - 4.0f && my <= curY + 18.0f) {
        if (mx >= x + width - 128.0f && mx <= x + width - 72.0f) {
            res.action = SelectionSidebarAction::ShortenDuration;
            res.floatValue = -0.25f;
            return res;
        }
        if (mx >= x + width - 68.0f && mx <= x + width - 12.0f) {
            res.action = SelectionSidebarAction::LengthenDuration;
            res.floatValue = 0.25f;
            return res;
        }
    }

    // Quick duration chips (y + 196)
    float chipY = curY + 22.0f;
    if (my >= chipY && my <= chipY + 22.0f) {
        if (mx >= x + 10.0f && mx <= x + 64.0f) {
            res.action = SelectionSidebarAction::DurPreset25;
            res.floatValue = 0.25f;
            return res;
        }
        if (mx >= x + 68.0f && mx <= x + 122.0f) {
            res.action = SelectionSidebarAction::DurPreset50;
            res.floatValue = 0.50f;
            return res;
        }
        if (mx >= x + 126.0f && mx <= x + 180.0f) {
            res.action = SelectionSidebarAction::DurPreset75;
            res.floatValue = 0.75f;
            return res;
        }
        if (mx >= x + 184.0f && mx <= x + 238.0f) {
            res.action = SelectionSidebarAction::DurPreset100;
            res.floatValue = 1.00f;
            return res;
        }
    }

    // Velocity buttons (y + 244)
    curY = y + 244.0f;
    if (my >= curY && my <= curY + 22.0f) {
        if (mx >= x + 10.0f && mx <= x + 52.0f) {
            res.action = SelectionSidebarAction::Vel25;
            res.floatValue = 0.25f;
            return res;
        }
        if (mx >= x + 56.0f && mx <= x + 98.0f) {
            res.action = SelectionSidebarAction::Vel50;
            res.floatValue = 0.50f;
            return res;
        }
        if (mx >= x + 102.0f && mx <= x + 144.0f) {
            res.action = SelectionSidebarAction::Vel75;
            res.floatValue = 0.75f;
            return res;
        }
        if (mx >= x + 148.0f && mx <= x + 190.0f) {
            res.action = SelectionSidebarAction::Vel100;
            res.floatValue = 1.00f;
            return res;
        }
        if (mx >= x + 194.0f && mx <= x + 254.0f) {
            res.action = SelectionSidebarAction::Humanize;
            return res;
        }
    }

    // Expression (y + 294)
    curY = y + 294.0f;
    if (my >= curY && my <= curY + 26.0f) {
        if (mx >= x + 10.0f && mx <= x + 125.0f) {
            res.action = SelectionSidebarAction::ToggleSlide;
            return res;
        }
        if (mx >= x + 135.0f && mx <= x + 250.0f) {
            res.action = SelectionSidebarAction::ToggleAccent;
            return res;
        }
    }

    // Selection utilities (y + 346..400)
    curY = y + 346.0f;
    if (my >= curY && my <= curY + 24.0f) {
        if (mx >= x + 10.0f && mx <= x + 125.0f) {
            res.action = SelectionSidebarAction::SelectAll;
            return res;
        }
        if (mx >= x + 135.0f && mx <= x + 250.0f) {
            res.action = SelectionSidebarAction::Invert;
            return res;
        }
    }

    curY += 28.0f;
    if (my >= curY && my <= curY + 24.0f) {
        if (mx >= x + 10.0f && mx <= x + 125.0f) {
            res.action = SelectionSidebarAction::Quantize;
            return res;
        }
        if (mx >= x + 135.0f && mx <= x + 250.0f) {
            res.action = SelectionSidebarAction::Clear;
            return res;
        }
    }

    return res;
}

void GuiWindow::drawNoteSelectionSidebar(float x, float y, float width, float height, sequencer::SequencerTrack& track) {
    const auto& selected = track.getSelectedSteps();
    if (selected.empty()) return;

    const bool isSingle = (selected.size() == 1);
    uint32_t firstStep = *selected.begin();
    const auto& firstNote = track.getStep(firstStep);

    // 1. Chassis Background & Drop Shadow
    drawRect(x - 3.0f, y, 3.0f, height, 0.0f, 0.0f, 0.0f, 0.45f);
    drawRectGradient(x, y, width, height, 0.09f, 0.10f, 0.13f, 0.06f, 0.07f, 0.09f);
    drawRectOutline(x, y, width, height, 0.0f, 0.80f, 0.95f, 0.85f, 1.5f);

    // 2. Header Bar (Y: y .. y + 32)
    drawRectGradient(x, y, width, 32.0f, 0.14f, 0.16f, 0.22f, 0.08f, 0.09f, 0.13f);
    drawLine(x, y + 32.0f, x + width, y + 32.0f, 0.24f, 0.28f, 0.38f, 1.0f, 1.0f);

    // Track color indicator dot
    drawCircle(x + 14.0f, y + 16.0f, 4.0f, 0.0f, 0.95f, 1.0f);

    // Header Title
    std::string title = isSingle ? "NOTE INSPECTOR" : "MULTI-NOTE INSPECTOR";
    drawVectorString(title, x + 24.0f, y + 10.0f, 0.78f, 0.90f, 0.95f, 1.0f);

    // Delete Button [x + width - 68, y + 5, 32, 22]
    float delX = x + width - 68.0f;
    float delY = y + 5.0f;
    drawRectGradient(delX, delY, 32.0f, 22.0f, 0.35f, 0.08f, 0.10f, 0.18f, 0.04f, 0.05f);
    drawRectOutline(delX, delY, 32.0f, 22.0f, 0.85f, 0.20f, 0.20f, 1.0f, 1.0f);
    drawVectorString("DEL", delX + 6.0f, delY + 5.0f, 0.70f, 1.0f, 0.40f, 0.40f);

    // Close Button [x + width - 30, y + 5, 22, 22]
    float closeX = x + width - 30.0f;
    float closeY = y + 5.0f;
    bool closeHov = (mouseX_ >= closeX - 2.0f && mouseX_ <= closeX + 24.0f && mouseY_ >= closeY - 2.0f && mouseY_ <= closeY + 24.0f) ||
                    (std::hypot(mouseX_ - (closeX + 11.0f), mouseY_ - (closeY + 11.0f)) <= 12.0f);
    drawIconScrewClose(closeX + 11.0f, closeY + 11.0f, 8.5f, closeHov, getTheme().primaryAccent);

    // 3. Summary Box (Y: y + 38 .. y + 78)
    float sumY = y + 38.0f;
    drawRectGradient(x + 8.0f, sumY, width - 16.0f, 40.0f, 0.06f, 0.18f, 0.26f, 0.03f, 0.09f, 0.14f);
    drawRectOutline(x + 8.0f, sumY, width - 16.0f, 40.0f, 0.0f, 0.85f, 1.0f, 0.7f, 1.0f);

    static const char* kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (isSingle) {
        std::string nName = std::string(kNoteNames[firstNote.note % 12]) + std::to_string(firstNote.note / 12 - 1);
        drawVectorString(nName, x + 16.0f, sumY + 8.0f, 1.15f, 0.0f, 1.0f, 0.95f);

        std::string stepInfo = "STEP " + std::to_string(firstStep) + " (" + std::to_string(static_cast<int>(firstNote.gateLength * 100)) + "% dur)";
        drawVectorString(stepInfo, x + 16.0f, sumY + 24.0f, 0.70f, 0.70f, 0.78f, 0.90f);

        std::string velInfo = "VEL: " + std::to_string(static_cast<int>(firstNote.velocity * 100)) + "%";
        drawVectorString(velInfo, x + width - 85.0f, sumY + 12.0f, 0.80f, 1.0f, 0.85f, 0.20f);
    } else {
        std::string countStr = std::to_string(selected.size()) + " NOTES SELECTED";
        drawVectorString(countStr, x + 16.0f, sumY + 12.0f, 0.90f, 0.0f, 1.0f, 0.95f);

        // [BATCH MODE] Badge
        float badgeX = x + width - 105.0f;
        drawRectGradient(badgeX, sumY + 9.0f, 90.0f, 22.0f, 0.30f, 0.22f, 0.05f, 0.15f, 0.11f, 0.02f);
        drawRectOutline(badgeX, sumY + 9.0f, 90.0f, 22.0f, 1.0f, 0.80f, 0.10f, 1.0f, 1.0f);
        drawVectorString("BATCH MODE", badgeX + 8.0f, sumY + 14.0f, 0.70f, 1.0f, 0.85f, 0.20f);
    }

    // 4. Section: PITCH TRANSPOSE (Y: y + 84)
    float curY = y + 84.0f;
    drawVectorString("PITCH TRANSPOSE", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    auto drawBtn = [&](float bx, float by, float bw, float bh, const std::string& label, float tr, float tg, float tb) {
        drawRectGradient(bx, by, bw, bh, 0.12f, 0.14f, 0.18f, 0.07f, 0.08f, 0.11f);
        drawRectOutline(bx, by, bw, bh, 0.22f, 0.26f, 0.35f, 1.0f, 1.0f);
        drawVectorString(label, bx + (bw - label.size() * 7.5f) * 0.5f, by + (bh - 10.0f) * 0.5f, 0.75f, tr, tg, tb);
    };

    drawBtn(x + 10.0f, curY, 40.0f, 26.0f, "-12", 0.0f, 0.95f, 1.0f);
    drawBtn(x + 54.0f, curY, 36.0f, 26.0f, "-1", 0.0f, 0.85f, 1.0f);

    // Center badge
    float badgeW = width - 188.0f;
    float badgePosX = x + 94.0f;
    drawRect(badgePosX, curY, badgeW, 26.0f, 0.05f, 0.07f, 0.10f);
    drawRectOutline(badgePosX, curY, badgeW, 26.0f, 0.0f, 0.85f, 1.0f, 0.6f, 1.0f);
    std::string pitchBadge = isSingle ? (std::string(kNoteNames[firstNote.note % 12]) + std::to_string(firstNote.note / 12 - 1)) : "+-PITCH";
    drawVectorString(pitchBadge, badgePosX + (badgeW - pitchBadge.size() * 7.5f) * 0.5f, curY + 7.0f, 0.78f, 0.0f, 1.0f, 0.95f);

    drawBtn(x + width - 90.0f, curY, 36.0f, 26.0f, "+1", 0.0f, 0.85f, 1.0f);
    drawBtn(x + width - 50.0f, curY, 40.0f, 26.0f, "+12", 0.0f, 0.95f, 1.0f);
    curY += 34.0f;

    // 5. Section: POSITION (STEP NUDGE)
    drawVectorString("POSITION (STEP NUDGE)", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    std::string nudgeLabel = isSingle ? ("Step " + std::to_string(firstStep)) : "Shift in Time";
    drawVectorString(nudgeLabel, x + 10.0f, curY + 16.0f, 0.75f, 1.0f, 0.85f, 0.20f);
    drawBtn(x + width - 128.0f, curY + 10.0f, 56.0f, 24.0f, "-STEP", 1.0f, 0.85f, 0.20f);
    drawBtn(x + width - 68.0f, curY + 10.0f, 56.0f, 24.0f, "+STEP", 1.0f, 0.85f, 0.20f);
    curY += 40.0f;

    // 6. Section: LENGTH / DURATION
    drawVectorString("LENGTH / DURATION", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    drawBtn(x + width - 128.0f, curY - 4.0f, 56.0f, 22.0f, "-LEN", 0.0f, 0.90f, 1.0f);
    drawBtn(x + width - 68.0f, curY - 4.0f, 56.0f, 22.0f, "+LEN", 0.0f, 0.90f, 1.0f);
    curY += 22.0f;

    // Quick duration chips: [0.25] [0.50] [0.75] [1.00]
    drawBtn(x + 10.0f, curY, 54.0f, 22.0f, "0.25", 0.75f, 0.80f, 0.90f);
    drawBtn(x + 68.0f, curY, 54.0f, 22.0f, "0.50", 0.75f, 0.80f, 0.90f);
    drawBtn(x + 126.0f, curY, 54.0f, 22.0f, "0.75", 0.75f, 0.80f, 0.90f);
    drawBtn(x + 184.0f, curY, 54.0f, 22.0f, "1.00", 0.75f, 0.80f, 0.90f);
    curY += 32.0f;

    // 7. Section: VELOCITY
    float avgVel = 0.0f;
    for (uint32_t s : selected) avgVel += track.getStep(s).velocity;
    avgVel /= static_cast<float>(selected.size());
    int avgVelPct = static_cast<int>(avgVel * 100.0f);
    drawVectorString("VELOCITY (" + std::to_string(avgVelPct) + "% AVG)", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    drawBtn(x + 10.0f, curY, 42.0f, 22.0f, "25%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 56.0f, curY, 42.0f, 22.0f, "50%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 102.0f, curY, 42.0f, 22.0f, "75%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 148.0f, curY, 42.0f, 22.0f, "100%", 0.0f, 0.85f, 1.0f);
    drawBtn(x + 194.0f, curY, 60.0f, 22.0f, "HUMAN", 1.0f, 0.85f, 0.20f);
    curY += 34.0f;

    // 8. Section: EXPRESSION / TB-303
    drawVectorString("EXPRESSION / TB-303", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    bool isSlide = firstNote.slide;
    bool isAccent = firstNote.accent;

    if (isSlide) {
        drawRectGradient(x + 10.0f, curY, 115.0f, 26.0f, 0.0f, 0.35f, 0.45f, 0.0f, 0.18f, 0.25f);
        drawRectOutline(x + 10.0f, curY, 115.0f, 26.0f, 0.0f, 0.95f, 1.0f, 1.0f, 1.4f);
        drawVectorString("SLIDE: ON", x + 24.0f, curY + 7.0f, 0.75f, 0.0f, 1.0f, 1.0f);
    } else {
        drawBtn(x + 10.0f, curY, 115.0f, 26.0f, "SLIDE: OFF", 0.50f, 0.55f, 0.65f);
    }

    if (isAccent) {
        drawRectGradient(x + 135.0f, curY, 115.0f, 26.0f, 0.35f, 0.25f, 0.05f, 0.18f, 0.12f, 0.02f);
        drawRectOutline(x + 135.0f, curY, 115.0f, 26.0f, 1.0f, 0.85f, 0.10f, 1.0f, 1.4f);
        drawVectorString("ACCENT: ON", x + 144.0f, curY + 7.0f, 0.75f, 1.0f, 0.88f, 0.20f);
    } else {
        drawBtn(x + 135.0f, curY, 115.0f, 26.0f, "ACCENT: OFF", 0.50f, 0.55f, 0.65f);
    }
    curY += 36.0f;

    // 9. Section: SELECTION UTILITIES
    drawVectorString("SELECTION UTILITIES", x + 10.0f, curY, 0.70f, 0.45f, 0.50f, 0.62f);
    curY += 16.0f;

    drawBtn(x + 10.0f, curY, 115.0f, 24.0f, "SELECT ALL", 0.75f, 0.85f, 0.95f);
    drawBtn(x + 135.0f, curY, 115.0f, 24.0f, "INVERT", 0.75f, 0.85f, 0.95f);
    curY += 28.0f;

    drawBtn(x + 10.0f, curY, 115.0f, 24.0f, "QUANTIZE", 0.0f, 0.90f, 1.0f);
    drawBtn(x + 135.0f, curY, 115.0f, 24.0f, "CLEAR", 0.80f, 0.45f, 0.45f);
}

HitTestTrackerResult GuiWindow::hitTestTracker(float x, float y) const noexcept {
    HitTestTrackerResult res{};
    if (activeView_ != WorkspaceView::Edit && activeView_ != WorkspaceView::Tracker) return res;
    if (editSubView_ != EditSubView::Tracker) return res;

    const size_t numTracks = engine_ ? engine_->getSequencer().getNumTracks() : 5;
    const float startX = 95.0f;
    const float endX = static_cast<float>(width_) - 25.0f;
    const float colW = (numTracks > 0) ? std::max(180.0f, (endX - startX) / static_cast<float>(numTracks)) : 220.0f;

    // 1. Column Headers (Y = 135..165)
    if (y >= 135.0f && y <= 165.0f) {
        if (x >= startX && x <= (startX + colW * numTracks)) {
            uint32_t t = static_cast<uint32_t>((x - startX) / colW);
            if (t < numTracks) {
                res.hit = true;
                res.isHeader = true;
                res.trackIndex = t;
                float hx = startX + t * colW;
                if (x >= (hx + colW - 54.0f) && x <= (hx + colW - 32.0f)) {
                    res.isMute = true;
                } else if (x >= (hx + colW - 28.0f) && x <= (hx + colW - 6.0f)) {
                    res.isSolo = true;
                }
                return res;
            }
        }
    }

    // 2. Tracker Rows (Y = 170..bPanelY-6)
    const float topY = 170.0f;
    const float bottomY = (static_cast<float>(height_) - 48.0f) - 6.0f;
    const float rowH = 24.0f;
    const int visibleRows = static_cast<int>((bottomY - topY) / rowH); // 23 rows

    if (y >= topY && y <= bottomY && x >= 30.0f) {
        int startRow = 0;
        if (trackerFollowPlayback_ && engine_ && engine_->getSequencer().isPlaying()) {
            int playRow = static_cast<int>(engine_->getSequencer().getTransport().getCurrentStep() % 64);
            startRow = std::clamp(playRow - 11, 0, 64 - visibleRows);
        } else {
            startRow = std::clamp(static_cast<int>(trackerSelectedRow_) - 11, 0, 64 - visibleRows);
        }

        int rowOffset = static_cast<int>((y - topY) / rowH);
        uint32_t r = static_cast<uint32_t>(startRow + rowOffset);
        if (r < 64) {
            res.hit = true;
            res.row = r;
            if (x >= startX && x <= (startX + colW * numTracks)) {
                uint32_t t = static_cast<uint32_t>((x - startX) / colW);
                if (t < numTracks) {
                    res.trackIndex = t;
                }
            } else {
                res.trackIndex = trackerSelectedTrack_;
            }
            return res;
        }
    }

    return res;
}

int GuiWindow::qwertyKeyToMidiPitch(int key, int baseOctave) noexcept {
    int baseMidi = baseOctave * 12 + 12; // Base Octave 4: 60 (Middle C)

    switch (key) {
        // Lower Octave (Z S X D C V G B H N J M -> C to B)
        case 90: return baseMidi + 0;  // Z -> C
        case 83: return baseMidi + 1;  // S -> C#
        case 88: return baseMidi + 2;  // X -> D
        case 68: return baseMidi + 3;  // D -> D#
        case 67: return baseMidi + 4;  // C -> E
        case 86: return baseMidi + 5;  // V -> F
        case 71: return baseMidi + 6;  // G -> F#
        case 66: return baseMidi + 7;  // B -> G
        case 72: return baseMidi + 8;  // H -> G#
        case 78: return baseMidi + 9;  // N -> A
        case 74: return baseMidi + 10; // J -> A#
        case 77: return baseMidi + 11; // M -> B

        // Upper Octave (baseOctave + 1): Q 2 W 3 E R 5 T 6 Y 7 U I -> C5 to C6
        case 81: return baseMidi + 12; // Q -> C5
        case 50: return baseMidi + 13; // 2 -> C#5
        case 87: return baseMidi + 14; // W -> D5
        case 51: return baseMidi + 15; // 3 -> D#5
        case 69: return baseMidi + 16; // E -> E5
        case 82: return baseMidi + 17; // R -> F5
        case 53: return baseMidi + 18; // 5 -> F#5
        case 84: return baseMidi + 19; // T -> G5
        case 54: return baseMidi + 20; // 6 -> G#5
        case 89: return baseMidi + 21; // Y -> A5
        case 55: return baseMidi + 22; // 7 -> A#5
        case 85: return baseMidi + 23; // U -> B5
        case 73: return baseMidi + 24; // I -> C6

        default: return -1;
    }
}

static float scoreDurationToSteps(ScoreNoteType dur) {
    switch (dur) {
        case ScoreNoteType::Whole: return 16.0f;
        case ScoreNoteType::Half: return 8.0f;
        case ScoreNoteType::Quarter: return 4.0f;
        case ScoreNoteType::Eighth: return 2.0f;
        case ScoreNoteType::Sixteenth: return 1.0f;
    }
    return 4.0f;
}

HitTestScoreResult GuiWindow::hitTestScore(float x, float y) const noexcept {
    HitTestScoreResult res{};
    if (activeView_ != WorkspaceView::Edit && activeView_ != WorkspaceView::Tracker) return res;
    if (editSubView_ != EditSubView::Score) return res;

    // 1. Toolbar button hit testing: Y in [100..132]
    if (y >= 100.0f && y <= 132.0f) {
        // Clefs:
        // [ GRAND ] : [80..144]
        if (x >= 80.0f && x <= 144.0f) {
            res.hit = true;
            res.action = ScoreHitAction::ClefGrandStaff;
            return res;
        }
        // [ TREBLE ] : [148..212]
        if (x >= 148.0f && x <= 212.0f) {
            res.hit = true;
            res.action = ScoreHitAction::ClefTreble;
            return res;
        }
        // [ BASS ] : [216..270]
        if (x >= 216.0f && x <= 270.0f) {
            res.hit = true;
            res.action = ScoreHitAction::ClefBass;
            return res;
        }

        // Note durations:
        // [ 1 ] : [330..364]
        if (x >= 330.0f && x <= 364.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationWhole;
            return res;
        }
        // [ 1/2 ] : [368..408]
        if (x >= 368.0f && x <= 408.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationHalf;
            return res;
        }
        // [ 1/4 ] : [412..452]
        if (x >= 412.0f && x <= 452.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationQuarter;
            return res;
        }
        // [ 1/8 ] : [456..496]
        if (x >= 456.0f && x <= 496.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationEighth;
            return res;
        }
        // [ 1/16 ] : [500..546]
        if (x >= 500.0f && x <= 546.0f) {
            res.hit = true;
            res.action = ScoreHitAction::DurationSixteenth;
            return res;
        }

        // Accidentals:
        // [ NAT ] : [600..642]
        if (x >= 600.0f && x <= 642.0f) {
            res.hit = true;
            res.action = ScoreHitAction::AccidentalNone;
            return res;
        }
        // [ # ] : [646..678]
        if (x >= 646.0f && x <= 678.0f) {
            res.hit = true;
            res.action = ScoreHitAction::AccidentalSharp;
            return res;
        }
        // [ b ] : [682..714]
        if (x >= 682.0f && x <= 714.0f) {
            res.hit = true;
            res.action = ScoreHitAction::AccidentalFlat;
            return res;
        }
    }

    // 2. Grand Staff interactive note placement:
    // X in [140 .. width - 60], Y in [160 .. 380]
    const float staffLeftX = 140.0f;
    const float staffRightX = static_cast<float>(width_) - 60.0f;
    if (x >= staffLeftX && x <= staffRightX && y >= 160.0f && y <= 380.0f) {
        float stepW = (staffRightX - staffLeftX) / 16.0f;
        uint32_t step = std::clamp(static_cast<uint32_t>((x - staffLeftX) / stepW), 0u, 15u);

        const float sp = scoreStaffSpace_; // 14.0f
        const float trebleLine1Y = 256.0f;
        const float bassLine1Y = 340.0f;

        bool isTreble = (scoreClef_ == ScoreClef::Treble) ? true :
                       ((scoreClef_ == ScoreClef::Bass) ? false : (y <= 270.0f));

        float l1Y = isTreble ? trebleLine1Y : bassLine1Y;
        int midi = ScoreLayoutEngine::yToMidiPitch(y, l1Y, sp, isTreble, scoreAccidental_);

        res.hit = true;
        res.action = ScoreHitAction::StaffClick;
        res.step = step;
        res.pitch = static_cast<uint8_t>(midi);
        res.isTrebleStaff = isTreble;

        if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
            auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
            if (tr) {
                auto st = tr->getStep(step);
                if (st.active && st.note == res.pitch) {
                    res.hasExistingNote = true;
                }
            }
        }
        return res;
    }

    return res;
}

HitTestArrangerResult GuiWindow::hitTestArranger(float x, float y) const noexcept {
    HitTestArrangerResult res{};
    const float w = static_cast<float>(width_);
    const float h = static_cast<float>(height_);
    const float topY = 56.0f;
    const float rulerH = 22.0f;
    const float trackHeaderW = 210.0f;
    const float barW = 60.0f;
    const float trackH = 76.0f;
    const float bottomY = h - 48.0f;
    const float miniY = bottomY - 32.0f;
    const float miniH = 22.0f;

    const float propW = arrangerPropertiesExpanded_ ? arrangerPropertiesWidth_ : 0.0f;
    const float pullTabW = kArrangerPullTabW;
    const float drawerTotalW = arrangerPropertiesExpanded_ ? (pullTabW + propW) : pullTabW;
    const float browserOffset = browserOpen_ ? ProjectBrowserDrawer::getDrawerWidth() : 0.0f;
    const float rightBoundary = w - drawerTotalW - browserOffset;
    const float pullTabX = rightBoundary;
    const float propX = pullTabX + pullTabW;

    // 0. Pull-Tab Strip Hit (X in [pullTabX, pullTabX + pullTabW])
    if (x >= pullTabX && x <= pullTabX + pullTabW && y >= topY && y <= bottomY) {
        res.hit = true;
        res.area = ArrangerHitArea::SidebarPullTab;
        return res;
    }

    // 0.1 Expanded Properties Sidebar Body Hit
    if (arrangerPropertiesExpanded_ && x > pullTabX + pullTabW && x <= propX + propW && y >= topY && y <= bottomY) {
        res.hit = true;
        // Close Button [propX + propW - 28, topY + 7, 22, 22]
        float clBtnX = propX + propW - 28.0f;
        float clBtnY = topY + 7.0f;
        if (x >= clBtnX && x <= clBtnX + 22.0f && y >= clBtnY && y <= clBtnY + 22.0f) {
            res.area = ArrangerHitArea::SidebarCloseButton;
            return res;
        }

        // Segmented Tab Switcher [Y = topY + 44..topY + 72]
        float tabY = topY + 44.0f;
        float halfW = (propW - 26.0f) * 0.5f;
        if (y >= tabY && y <= tabY + 28.0f) {
            if (x >= propX + 10.0f && x <= propX + 10.0f + halfW) {
                res.area = ArrangerHitArea::SidebarTabTrack;
                return res;
            } else if (x >= propX + 14.0f + halfW && x <= propX + 14.0f + 2.0f * halfW) {
                res.area = ArrangerHitArea::SidebarTabClip;
                return res;
            }
        }

        float cardX = propX + 10.0f;
        float cardW = propW - 20.0f;
        float cardY = topY + 80.0f;

        if (arrangerInspectorTab_ == ArrangerInspectorTab::Track) {
            float cardH = bottomY - cardY - 8.0f;
            auto inspHit = hitTestTrackPropertiesContainer(x, y, cardX, cardY, cardW, cardH, selectedTrackIndex_, trackInspectorScrollY_, true);
            if (inspHit.hit) {
                res.hit = true;
                res.trackInspectorHit = inspHit;
                if (inspHit.area == TrackInspectorHitArea::VolumeSlider) {
                    res.area = ArrangerHitArea::TrackVolume;
                    res.trackIndex = selectedTrackIndex_;
                    res.normVal = inspHit.normVal;
                } else if (inspHit.area == TrackInspectorHitArea::PanKnob) {
                    res.area = ArrangerHitArea::TrackPan;
                    res.trackIndex = selectedTrackIndex_;
                    res.normVal = inspHit.normVal;
                } else if (inspHit.area == TrackInspectorHitArea::MuteButton) {
                    res.area = ArrangerHitArea::TrackMute;
                    res.trackIndex = selectedTrackIndex_;
                } else if (inspHit.area == TrackInspectorHitArea::SoloButton) {
                    res.area = ArrangerHitArea::TrackSolo;
                    res.trackIndex = selectedTrackIndex_;
                } else if (inspHit.area == TrackInspectorHitArea::ColorSwatch) {
                    res.area = ArrangerHitArea::SidebarColorSwatch;
                    res.trackIndex = selectedTrackIndex_;
                    res.colorSwatchIndex = inspHit.colorSwatchIndex;
                } else {
                    res.area = ArrangerHitArea::SidebarTrackInspector;
                }
                return res;
            }
        } else {
            // Clip Tab
            if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0) {
                // Loop toggle button
                float loopCardY = cardY + 60.0f;
                float loopBtnX = cardX + cardW - 74.0f;
                float loopBtnY = loopCardY + 26.0f;
                if (x >= loopBtnX && x <= loopBtnX + 64.0f && y >= loopBtnY && y <= loopBtnY + 26.0f) {
                    res.area = ArrangerHitArea::SidebarClipLoopToggle;
                    return res;
                }

                // Pitch & Transpose Card
                float transCardY = cardY + 142.0f;
                if (y >= transCardY + 28.0f && y <= transCardY + 56.0f) {
                    float transBtnW = (cardW - 32.0f) / 5.0f;
                    for (int tb = 0; tb < 5; ++tb) {
                        float tbx = cardX + 10.0f + tb * (transBtnW + 3.0f);
                        if (x >= tbx && x <= tbx + transBtnW) {
                            res.area = ArrangerHitArea::SidebarClipTranspose;
                            if (tb == 0) res.transposeDelta = -12;
                            else if (tb == 1) res.transposeDelta = -1;
                            else if (tb == 2) res.transposeDelta = 1;
                            else if (tb == 3) res.transposeDelta = 12;
                            else res.transposeDelta = 0;
                            return res;
                        }
                    }
                }

                // Clip Actions Card
                float clipActY = cardY + 220.0f;
                if (y >= clipActY + 24.0f && y <= clipActY + 42.0f && x >= cardX + 10.0f && x <= cardX + cardW - 10.0f) {
                    res.area = ArrangerHitArea::SidebarAction;
                    res.actionName = "QuantizeClip";
                    return res;
                }
                if (y >= clipActY + 46.0f && y <= clipActY + 64.0f && x >= cardX + 10.0f && x <= cardX + cardW - 10.0f) {
                    res.area = ArrangerHitArea::SidebarAction;
                    res.actionName = "DuplicateClip";
                    return res;
                }
                if (y >= clipActY + 68.0f && y <= clipActY + 86.0f && x >= cardX + 10.0f && x <= cardX + cardW - 10.0f) {
                    res.area = ArrangerHitArea::SidebarAction;
                    res.actionName = "DeleteClip";
                    return res;
                }
            }
        }
        return res;
    }

    // 1. Time Ruler scrubbing: Y in [topY, topY + rulerH]
    if (y >= topY && y <= topY + rulerH) {
        if (x >= trackHeaderW && x <= rightBoundary) {
            res.hit = true;
            res.area = ArrangerHitArea::Ruler;
            float barProgress = (x - trackHeaderW) / barW;
            res.bar = static_cast<uint32_t>(barProgress) + 1;
            res.step = static_cast<uint32_t>(std::clamp(barProgress * 16.0f, 0.0f, 18.0f * 16.0f));
            res.normVal = std::clamp((x - trackHeaderW) / (rightBoundary - trackHeaderW), 0.0f, 1.0f);
            return res;
        }
    }

    // 2. Track Lanes and Headers
    for (uint32_t t = 0; t < 5; ++t) {
        float curY = topY + rulerH + t * trackH;
        if (curY + trackH > bottomY - 30.0f) break;

        if (y >= curY && y < curY + trackH) {
            res.trackIndex = t;

            if (x < trackHeaderW) {
                // Row 1: Mute, Solo, Freeze buttons
                // Mute button: X in [128, 153], Y in [curY + 6, curY + 30]
                if (x >= 128.0f && x <= 153.0f && y >= curY + 6.0f && y <= curY + 30.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackMute;
                    return res;
                }
                // Solo button: X in [154, 179], Y in [curY + 6, curY + 30]
                if (x >= 154.0f && x <= 179.0f && y >= curY + 6.0f && y <= curY + 30.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackSolo;
                    return res;
                }
                // Freeze button: X in [180, 206], Y in [curY + 6, curY + 30]
                if (x >= 180.0f && x <= 206.0f && y >= curY + 6.0f && y <= curY + 30.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackFreeze;
                    return res;
                }

                // Row 2: Volume slider & meter, Track Pan knob
                // Volume slider: X in [36, 156], Y in [curY + 38, curY + 64]
                if (x >= 36.0f && x <= 156.0f && y >= curY + 38.0f && y <= curY + 64.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackVolume;
                    res.normVal = std::clamp((x - 38.0f) / 114.0f, 0.0f, 1.0f);
                    return res;
                }
                // Track pan knob: X in [168, 202], Y in [curY + 38, curY + 68]
                if (x >= 168.0f && x <= 202.0f && y >= curY + 38.0f && y <= curY + 68.0f) {
                    res.hit = true;
                    res.area = ArrangerHitArea::TrackPan;
                    return res;
                }
                // Entire track header card
                res.hit = true;
                res.area = ArrangerHitArea::TrackHeader;
                return res;
            } else if (x >= trackHeaderW && x <= rightBoundary) {
                // Timeline lane
                float barProgress = (x - trackHeaderW) / barW;
                res.bar = static_cast<uint32_t>(barProgress) + 1;
                res.step = static_cast<uint32_t>(std::clamp(barProgress * 16.0f, 0.0f, 18.0f * 16.0f));

                // Check clip bounds for tracks
                if (t < arrangerTracks_.size() && !arrangerTracks_[t].clips.empty()) {
                    for (size_t c = 0; c < arrangerTracks_[t].clips.size(); ++c) {
                        const auto& clp = arrangerTracks_[t].clips[c];
                        float cX = trackHeaderW + (clp.startBar - 1) * barW;
                        float cW = clp.barLength * barW;
                        if (x >= cX && x <= cX + cW) {
                            res.hit = true;
                            res.area = ArrangerHitArea::Clip;
                            res.clipIndex = static_cast<int>(c);
                            return res;
                        }
                    }
                } else {
                    bool inClip = false;
                    if (t == 0) {
                        if (x >= trackHeaderW && x <= trackHeaderW + 8.0f * barW) inClip = true;
                    } else if (t == 1 || t == 2) {
                        if (x >= trackHeaderW && x <= trackHeaderW + 8.0f * barW) inClip = true;
                    } else if (t == 3) {
                        if (x >= trackHeaderW + 4.0f * barW && x <= trackHeaderW + 12.0f * barW) inClip = true;
                    }

                    if (inClip) {
                        res.hit = true;
                        res.area = ArrangerHitArea::Clip;
                        res.clipIndex = 0;
                        return res;
                    }
                }
            }
        }
    }

    // 3. Minimap Overview Scrollbar
    if (y >= miniY && y <= miniY + miniH) {
        float miniTrackX = trackHeaderW;
        float miniTrackW = rightBoundary - trackHeaderW - 20.0f;
        if (x >= miniTrackX && x <= miniTrackX + miniTrackW) {
            res.hit = true;
            res.area = ArrangerHitArea::OverviewScrollbar;
            res.normVal = std::clamp((x - miniTrackX) / miniTrackW, 0.0f, 1.0f);
            return res;
        }
    }

    return res;
}

void GuiWindow::onMouseMove(float x, float y) {
    // Intercept if Plugin Search Modal Dialog is open
    PluginSearchDialog* activePluginDialog = nullptr;
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPluginSearchDialog().isOpen()) {
            activePluginDialog = &modularArrangerView_->getPluginSearchDialog();
        } else if (modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            activePluginDialog = &modularArrangerView_->getPropertiesDrawer().getPluginSearchDialog();
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            activePluginDialog = &modularMixerView_->getPropertiesDrawer().getPluginSearchDialog();
        }
    }

    if (activePluginDialog && activePluginDialog->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (activePluginDialog->handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
        mouseX_ = x;
        mouseY_ = y;
        return; // Absorb events behind modal
    }

    if (commandPaletteDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (commandPaletteDialog_.handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (audioToMidiDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (audioToMidiDialog_.handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Move;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            pev.dx = x - mouseX_;
            pev.dy = y - mouseY_;
            if (modularArrangerView_->getIconSearchDialog().handlePointer(pev)) {
                mouseX_ = x;
                mouseY_ = y;
                return;
            }
        }

        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (valueEditDialog_.handlePointer(pev)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    if (browserOpen_ && projectBrowserDrawerWidget_) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        projectBrowserDrawerWidget_->handlePointer(pev);
    }

    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularArrangerView_->handlePointer(pev, ctx)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularEditView_->handlePointer(pev, ctx)) {
            if (dragMode_ == DragMode::PianoRollMiddlePan) {
                float dx = x - panStartMouseX_;
                float dy = y - panStartMouseY_;
                setPianoRollScrollX(panStartScrollX_ - dx);
                setPianoRollScrollY(panStartScrollY_ - dy);
            }
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularTrackInspectorView_->handlePointer(pev, ctx)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }
    if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        modularMixerView_->handlePointer(pev, ctx);
    }
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Move;
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        pev.dx = x - mouseX_;
        pev.dy = y - mouseY_;
        if (modularDesignView_->handlePointer(pev, ctx)) {
            mouseX_ = x;
            mouseY_ = y;
            return;
        }
    }

    mouseX_ = x;
    mouseY_ = y;

    if (dragMode_ == DragMode::PianoRollMiddlePan) {
        float dx = x - panStartMouseX_;
        float dy = y - panStartMouseY_;
        setPianoRollScrollX(panStartScrollX_ - dx);
        setPianoRollScrollY(panStartScrollY_ - dy);
        return;
    } else if (dragMode_ == DragMode::PianoRollScrollbarV) {
        float dy = y - panStartMouseY_;
        const float sbH = 550.0f;
        float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
        float thumbH = std::clamp((sbH / std::max(totalH, sbH)) * sbH, 24.0f, sbH);
        float trackH = std::max(1.0f, sbH - thumbH);
        float maxScrollY = std::max(0.0f, totalH - sbH);
        setPianoRollScrollY(panStartScrollY_ + (dy / trackH) * maxScrollY);
        return;
    } else if (dragMode_ == DragMode::PianoRollScrollbarH) {
        float dx = x - panStartMouseX_;
        const float gridX = 110.0f;
        const bool showSidebar = engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() &&
                                 engine_->getSequencer().getTrack(selectedTrackIndex_) &&
                                 engine_->getSequencer().getTrack(selectedTrackIndex_)->hasSelectedNotes();
        const float scrollbarW = 12.0f;
        const float gridW = static_cast<float>(width_) - gridX - (showSidebar ? 280.0f : 20.0f) - scrollbarW;
        const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                        ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                        : 64u;
        float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
        float thumbW = std::clamp((gridW / std::max(totalW, gridW)) * gridW, 24.0f, gridW);
        float trackW = std::max(1.0f, gridW - thumbW);
        float maxScrollX = std::max(0.0f, totalW - gridW);
        setPianoRollScrollX(panStartScrollX_ + (dx / trackW) * maxScrollX);
        return;
    } else if (dragMode_ == DragMode::VirtualKeyboardGlissando) {
        if (virtualKeyboardDrawerOpen_) {
            auto dHit = hitTestVirtualKeyboardDrawer(x, y);
            if (dHit.hit && dHit.isKey && dHit.pitch != virtualKeyboardActivePitch_) {
                if (engine_) {
                    if (virtualKeyboardActivePitch_ != -1) {
                        engine_->postNoteOff(static_cast<uint8_t>(virtualKeyboardActivePitch_));
                    }
                    if (previewingPitch_ != -1 && previewingPitch_ != virtualKeyboardActivePitch_) {
                        engine_->postNoteOff(static_cast<uint8_t>(previewingPitch_));
                    }
                    engine_->postNoteOn(static_cast<uint8_t>(dHit.pitch), dHit.velocity);
                }
                virtualKeyboardActivePitch_ = dHit.pitch;
                previewingPitch_ = dHit.pitch;
                previewingVelocity_ = dHit.velocity;
                return;
            }
        }
        if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll) {
            auto kHit = hitTestPianoKey(x, y);
            if (kHit.hit && kHit.pitch != previewingPitch_) {
                if (engine_) {
                    if (previewingPitch_ != -1) {
                        engine_->postNoteOff(static_cast<uint8_t>(previewingPitch_));
                    }
                    if (virtualKeyboardActivePitch_ != -1 && virtualKeyboardActivePitch_ != previewingPitch_) {
                        engine_->postNoteOff(static_cast<uint8_t>(virtualKeyboardActivePitch_));
                    }
                    engine_->postNoteOn(static_cast<uint8_t>(kHit.pitch), kHit.velocity);
                }
                previewingPitch_ = kHit.pitch;
                virtualKeyboardActivePitch_ = kHit.pitch;
                previewingVelocity_ = kHit.velocity;
                return;
            }
        }
        return;
    } else if (dragMode_ == DragMode::PianoRollMarquee) {
        marqueeCurX_ = x;
        marqueeCurY_ = y;
        float dist = std::hypot(marqueeCurX_ - marqueeStartX_, marqueeCurY_ - marqueeStartY_);
        if (dist > 4.0f) {
            isMarqueeSelecting_ = true;
            updatePianoRollMarqueeSelection(marqueeStartX_, marqueeStartY_, x, y, isShiftPressed());
        }
    } else if (dragMode_ == DragMode::PianoRollNoteMove && engine_) {
        auto* activeTrk = (selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                              ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                              : nullptr;
        if (activeTrk) {
            const int minPitch = pianoRollKeyboard_.getMinPitch();
            const int maxPitch = pianoRollKeyboard_.getMaxPitch();
            const float rowH = pianoRollRowH_;
            int deltaPitches = -static_cast<int>((y - dragStartY_) / rowH);

            if (deltaPitches != 0) {
                bool canMove = true;
                for (const auto& kv : batchMoveStartPitches_) {
                    int newP = static_cast<int>(kv.second) + deltaPitches;
                    if (newP < minPitch || newP > maxPitch) {
                        canMove = false;
                        break;
                    }
                }
                if (canMove) {
                    for (const auto& kv : batchMoveStartPitches_) {
                        if (kv.first < activeTrk->getNumSteps()) {
                            activeTrk->getStep(kv.first).note = static_cast<uint8_t>(kv.second + deltaPitches);
                        }
                    }
                    if (activeMoveStep_ >= 0 && static_cast<uint32_t>(activeMoveStep_) < activeTrk->getNumSteps()) {
                        previewingPitch_ = activeTrk->getStep(activeMoveStep_).note;
                    }
                }
            }
        }
    } else if (dragMode_ == DragMode::PianoRollNoteResize && engine_) {
        auto* activeTrk = (selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                              ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                              : nullptr;
        if (activeTrk) {
            const float stepW = pianoRollStepW_;
            float deltaDur = (x - dragStartX_) / stepW;

            for (const auto& kv : batchResizeStartDurations_) {
                if (kv.first < activeTrk->getNumSteps()) {
                    activeTrk->getStep(kv.first).gateLength = std::clamp(kv.second + deltaDur, 0.10f, 4.0f);
                }
            }
        }
    } else if (dragMode_ == DragMode::HardwareKnob) {
        const float deltaY = dragStartY_ - y;
        constexpr float sensitivity = 1.0f / 160.0f;
        float newVal = std::clamp(dragStartHardwareVal_ + deltaY * sensitivity, 0.0f, 1.0f);
        dispatchHardwareParam(activeHardwareParam_, newVal);
    } else if (dragMode_ == DragMode::Knob && engine_) {
        const float deltaY = dragStartY_ - y; // Dragging up increases parameter
        constexpr float sensitivity = 1.0f / 160.0f; // 160px for full 0..1 range
        float newVal = std::clamp(dragStartValue_ + deltaY * sensitivity, 0.0f, 1.0f);
        setKnobValue(activeKnobNode_, activeKnobIndex_, newVal);
        dispatchKnobParameter(activeKnobNode_, activeKnobIndex_, newVal);
    } else if (dragMode_ == DragMode::BpmScrubber && engine_) {
        const float deltaY = dragStartY_ - y;
        float newBpm = std::clamp(dragStartValGeneric_ + deltaY * 0.5f, 40.0f, 300.0f);
        engine_->getSequencer().setBpm(newBpm);
    } else if (dragMode_ == DragMode::SwingScrubber && engine_) {
        const float deltaY = dragStartY_ - y;
        float newSwing = std::clamp(dragStartValGeneric_ + deltaY * 0.005f, 0.0f, 0.75f);
        engine_->getSequencer().setSwing(newSwing);
    } else if (dragMode_ == DragMode::MasterFader) {
        const float deltaY = dragStartY_ - y;
        float newVol = std::clamp(dragStartValGeneric_ + deltaY * (1.5f / 280.0f), 0.0f, 1.5f);
        setMasterVolume(newVol);
    } else if (dragMode_ == DragMode::MasterPan) {
        const float deltaY = dragStartY_ - y;
        float newPan = std::clamp(dragStartValGeneric_ + deltaY * (2.0f / 100.0f), -1.0f, 1.0f);
        setMasterPan(newPan);
    } else if (dragMode_ == DragMode::MixerFader) {
        if (activeMixerChannel_ < mixerStrips_.size()) {
            const float deltaY = dragStartY_ - y;
            float newVol = std::clamp(dragStartValGeneric_ + deltaY * (1.5f / 280.0f), 0.0f, 1.5f);
            mixerStrips_[activeMixerChannel_].volume = newVol;
            if (activeMixerChannel_ < arrangerTracks_.size()) {
                arrangerTracks_[activeMixerChannel_].volume = newVol;
            }
            if (engine_) {
                if (activeMixerChannel_ < 4) {
                    engine_->setTrackVolume(activeMixerChannel_, newVol);
                } else {
                    setMasterVolume(newVol);
                }
            }
        }
    } else if (dragMode_ == DragMode::MixerPan) {
        if (activeMixerChannel_ < mixerStrips_.size()) {
            const float deltaY = dragStartY_ - y;
            float newPan = std::clamp(dragStartValGeneric_ + deltaY * (2.0f / 100.0f), -1.0f, 1.0f);
            mixerStrips_[activeMixerChannel_].pan = newPan;
            if (activeMixerChannel_ < arrangerTracks_.size()) {
                arrangerTracks_[activeMixerChannel_].pan = newPan;
            }
            if (engine_) {
                if (activeMixerChannel_ < 4) {
                    engine_->setTrackPan(activeMixerChannel_, newPan);
                } else {
                    setMasterPan(newPan);
                }
            }
        }
    } else if (dragMode_ == DragMode::ArrangerRulerScrub && engine_) {
        const float trackHeaderW = 210.0f;
        const float barW = 60.0f;
        if (x >= trackHeaderW) {
            float barProgress = (x - trackHeaderW) / barW;
            uint32_t targetStep = static_cast<uint32_t>(std::clamp(barProgress * 16.0f, 0.0f, 18.0f * 16.0f));
            engine_->getSequencer().getTransport().setPosition(targetStep);
        }
    } else if (dragMode_ == DragMode::ArrangerTrackVolume) {
        float normVal = std::clamp((x - 38.0f) / 114.0f, 0.0f, 1.0f);
        float newVol = normVal * 1.5f;
        if (activeArrangerTrack_ < mixerStrips_.size()) {
            mixerStrips_[activeArrangerTrack_].volume = newVol;
        }
        if (activeArrangerTrack_ < arrangerTracks_.size()) {
            arrangerTracks_[activeArrangerTrack_].volume = newVol;
        }
        if (engine_) {
            engine_->setTrackVolume(activeArrangerTrack_, newVol);
        }
    } else if (dragMode_ == DragMode::ArrangerTrackPan) {
        float delta = (x - dragStartX_) * 0.015f - (y - dragStartY_) * 0.015f;
        float newPan = std::clamp(dragStartPan_ + delta, -1.0f, 1.0f);
        if (activeArrangerTrack_ < arrangerTracks_.size()) {
            arrangerTracks_[activeArrangerTrack_].pan = newPan;
        }
        if (activeArrangerTrack_ < mixerStrips_.size()) {
            mixerStrips_[activeArrangerTrack_].pan = newPan;
        }
        if (engine_) {
            engine_->setTrackPan(activeArrangerTrack_, newPan);
        }
    } else if (dragMode_ == DragMode::ArrangerOverviewScroll && engine_) {
        const float trackHeaderW = 210.0f;
        const float miniTrackW = static_cast<float>(width_) - trackHeaderW - 20.0f;
        if (miniTrackW > 0.0f) {
            float norm = std::clamp((x - trackHeaderW) / miniTrackW, 0.0f, 1.0f);
            uint32_t targetStep = static_cast<uint32_t>(norm * 18.0f * 16.0f);
            engine_->getSequencer().getTransport().setPosition(targetStep);
        }
    } else if (dragMode_ == DragMode::ArrangerPropertiesResize) {
        if (!arrangerPropertiesExpanded_) {
            if (x - dragStartX_ < -4.0f) {
                arrangerPropertiesExpanded_ = true;
                dragStartX_ = x;
                dragStartValGeneric_ = arrangerPropertiesWidth_;
            }
        } else {
            float newWidth = dragStartValGeneric_ + (dragStartX_ - x);
            if (newWidth < kArrangerPropertiesMinW - 25.0f) {
                arrangerPropertiesExpanded_ = false;
                arrangerPropertiesWidth_ = 300.0f;
                dragMode_ = DragMode::None;
            } else {
                arrangerPropertiesWidth_ = std::clamp(newWidth, kArrangerPropertiesMinW, kArrangerPropertiesMaxW);
            }
        }
    } else if (dragMode_ == DragMode::MixerPropertiesResize) {
        if (!mixerPropertiesExpanded_) {
            if (x - dragStartX_ < -4.0f) {
                mixerPropertiesExpanded_ = true;
                dragStartX_ = x;
                dragStartValGeneric_ = mixerPropertiesWidth_;
            }
        } else {
            float newWidth = dragStartValGeneric_ + (dragStartX_ - x);
            if (newWidth < kMixerPropertiesMinW - 25.0f) {
                mixerPropertiesExpanded_ = false;
                mixerPropertiesWidth_ = 360.0f;
                dragMode_ = DragMode::None;
            } else {
                mixerPropertiesWidth_ = std::clamp(newWidth, kMixerPropertiesMinW, kMixerPropertiesMaxW);
            }
        }
    } else if (dragMode_ == DragMode::TrackInspectorVolume) {
        const float inspX = 40.0f;
        float norm = std::clamp((x - (inspX + 80.0f)) / 220.0f, 0.0f, 1.0f);
        float newVol = norm * 1.5f;
        uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
        arrangerTracks_[tIdx].volume = newVol;
        if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].volume = newVol;
        if (engine_) {
            engine_->setTrackVolume(tIdx, newVol);
        }
    } else if (dragMode_ == DragMode::TrackInspectorPan) {
        const float inspX = 40.0f;
        float norm = std::clamp((x - (inspX + 420.0f)) / 180.0f, 0.0f, 1.0f);
        float newPan = norm * 2.0f - 1.0f;
        uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
        arrangerTracks_[tIdx].pan = newPan;
        if (tIdx < mixerStrips_.size()) mixerStrips_[tIdx].pan = newPan;
        if (engine_) {
            engine_->setTrackPan(tIdx, newPan);
        }
    } else if (dragMode_ == DragMode::TrackInspectorFxKnob) {
        const float deltaY = dragStartY_ - y;
        constexpr float sensitivity = 1.0f / 140.0f;
        float newVal = std::clamp(dragStartTrackInspectorVal_ + deltaY * sensitivity, 0.0f, 1.0f);
        uint32_t tIdx = (selectedTrackIndex_ < arrangerTracks_.size()) ? selectedTrackIndex_ : 0;
        auto& trk = arrangerTracks_[tIdx];
        if (activeTrackInspectorFxParam_ == "delayTime") trk.audioFx.delayTime = newVal;
        else if (activeTrackInspectorFxParam_ == "delayFeedback") trk.audioFx.delayFeedback = newVal;
        else if (activeTrackInspectorFxParam_ == "delayMix") trk.audioFx.delayMix = newVal;
        else if (activeTrackInspectorFxParam_ == "chorusRate") trk.audioFx.chorusRate = newVal;
        else if (activeTrackInspectorFxParam_ == "chorusDepth") trk.audioFx.chorusDepth = newVal;
        else if (activeTrackInspectorFxParam_ == "chorusMix") trk.audioFx.chorusMix = newVal;
        else if (activeTrackInspectorFxParam_ == "eqLow") trk.audioFx.eqLow = newVal;
        else if (activeTrackInspectorFxParam_ == "eqMid") trk.audioFx.eqMid = newVal;
        else if (activeTrackInspectorFxParam_ == "eqHigh") trk.audioFx.eqHigh = newVal;
        else if (activeTrackInspectorFxParam_ == "compThreshold") trk.audioFx.compThreshold = newVal;
        else if (activeTrackInspectorFxParam_ == "compRatio") trk.audioFx.compRatio = newVal;
        else if (activeTrackInspectorFxParam_ == "compGain") trk.audioFx.compGain = newVal;
        else if (activeTrackInspectorFxParam_ == "convolverPreset") trk.audioFx.convolverPreset = static_cast<int>(std::round(newVal * 12.0f));
        else if (activeTrackInspectorFxParam_ == "convolverMix") trk.audioFx.convolverMix = newVal;
        else if (activeTrackInspectorFxParam_ == "arpPattern") trk.midiFx.arpPattern = static_cast<int>(std::round(newVal * 4.0f));
        else if (activeTrackInspectorFxParam_ == "arpRate") trk.midiFx.arpRate = newVal;
        else if (activeTrackInspectorFxParam_ == "arpOctaves") trk.midiFx.arpOctaves = static_cast<int>(std::round(newVal));
        else if (activeTrackInspectorFxParam_ == "arpGate") trk.midiFx.arpGate = newVal;
        else if (activeTrackInspectorFxParam_ == "arpSwing") trk.midiFx.arpSwing = newVal;
        else if (activeTrackInspectorFxParam_ == "scaleMinor") trk.midiFx.scaleMinor = (newVal >= 0.5f);
        else if (activeTrackInspectorFxParam_ == "humanizeTiming") trk.midiFx.humanizeTiming = newVal;
        else if (activeTrackInspectorFxParam_ == "humanizeVelocity") trk.midiFx.humanizeVelocity = newVal;
    } else if (dragMode_ == DragMode::TrackInspectorScrollbar) {
        constexpr float maxScroll = 450.0f;
        const float deltaY = y - dragStartY_;
        const float scrollRange = static_cast<float>(height_) - 120.0f;
        if (scrollRange > 0.0f) {
            float scrollDelta = (deltaY / scrollRange) * (maxScroll + scrollRange);
            trackInspectorScrollY_ = std::clamp(dragStartScrollY_ + scrollDelta, 0.0f, maxScroll);
        }
    } else if (dragMode_ == DragMode::ProjectHubScroll) {
        if (projectHubScrollArea_.canScroll()) {
            const float deltaY = y - dragStartY_;
            if (projectHubDraggingThumb_) {
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Move;
                pev.x = x;
                pev.y = y;
                projectHubScrollArea_.handlePointer(pev);
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            } else {
                projectHubScrollArea_.setScrollY(dragStartScrollY_ - deltaY);
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            }
        }
    } else if (dragMode_ == DragMode::CrtTweakerSlider) {
        handleCrtTweakerDrag(x, y);
    }
}

void GuiWindow::onMouseDown(int button, float x, float y) {
    mouseX_ = x;
    mouseY_ = y;

    // Intercept if CRT Shader & Chassis Tweaker Modal is open
    if (crtTweakerOpen_) {
        if (handleCrtTweakerPointer(x, y, true, false)) {
            return;
        }
    }

    // 0. Intercept if Command Palette or Value Edit Modal Dialog is open
    if (commandPaletteDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (commandPaletteDialog_.handlePointer(pev)) return;
    }

    if (audioToMidiDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (audioToMidiDialog_.handlePointer(pev)) return;
    }

    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->getIconSearchDialog().handlePointer(pev)) return;
        }

        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Down;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (valueEditDialog_.handlePointer(pev)) return;
    }

    if (button == 2) { // Middle click: 2D panning in Arranger or Piano Roll
        if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Middle;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->handlePointer(pev, ctx)) return;
        } else if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Middle;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularEditView_->handlePointer(pev, ctx)) {
                if (editSubView_ == EditSubView::PianoRoll) {
                    dragMode_ = DragMode::PianoRollMiddlePan;
                    panStartMouseX_ = x;
                    panStartMouseY_ = y;
                    panStartScrollX_ = pianoRollScrollX_;
                    panStartScrollY_ = pianoRollScrollY_;
                }
                return;
            }
        }
        if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll) {
            dragMode_ = DragMode::PianoRollMiddlePan;
            panStartMouseX_ = x;
            panStartMouseY_ = y;
            panStartScrollX_ = pianoRollScrollX_;
            panStartScrollY_ = pianoRollScrollY_;
            return;
        }
    }

    // Modal Plugin Search Dialog intercepts clicks in Arranger and Mixer Views
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPluginSearchDialog().isOpen() ||
            modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->handlePointer(pev, ctx)) return;
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularMixerView_->handlePointer(pev, ctx)) return;
        }
    }

    if (button == 0) { // Left click
        // 0A. Check Project Hub & Settings Modal Hit (if open)
        if (projectHubOpen_) {
            auto hubHit = hitTestProjectHub(x, y);
            if (hubHit.hit) {
                switch (hubHit.action) {
                    case ProjectHubAction::Close:
                        projectHubOpen_ = false;
                        isEditingTitle_ = false;
                        isEditingAuthor_ = false;
                        break;
                    case ProjectHubAction::SectionHeader:
                        projectHubSection_ = (projectHubSection_ == hubHit.sectionIndex) ? -1 : hubHit.sectionIndex;
                        projectHubScrollY_ = 0.0f;
                        projectHubScrollArea_.setScrollY(0.0f);
                        isEditingTitle_ = false;
                        isEditingAuthor_ = false;
                        break;
                    case ProjectHubAction::TitleClick:
                        isEditingTitle_ = true;
                        isEditingAuthor_ = false;
                        break;
                    case ProjectHubAction::AuthorClick:
                        isEditingAuthor_ = true;
                        isEditingTitle_ = false;
                        break;
                    case ProjectHubAction::SaveProject:
                        saveProjectToFile(projectFilePath_);
                        break;
                    case ProjectHubAction::SaveAsProject:
                        saveProjectAs();
                        break;
                    case ProjectHubAction::LoadProject:
                        loadProjectPrompt();
                        break;
                    case ProjectHubAction::NewProject:
                        resetToDefaultProject();
                        break;
                    case ProjectHubAction::BounceWav:
                        bounceMasterToWav(projectName_ + ".wav");
                        break;
                    case ProjectHubAction::OpenScriptView:
                        setActiveView(WorkspaceView::Edit);
                        setEditSubView(EditSubView::Script);
                        projectHubOpen_ = false;
                        break;
                    case ProjectHubAction::ToggleRestoreSession:
                        autoRestoreSession_ = !autoRestoreSession_;
                        break;
                    case ProjectHubAction::ToggleAutosave:
                        autoSaveEnabled_ = !autoSaveEnabled_;
                        break;
                    case ProjectHubAction::ResetCleanSlate:
                        resetToDefaultProject();
                        break;
                    case ProjectHubAction::SetUiScale:
                        setUiScale(hubHit.scaleValue);
                        break;
                    case ProjectHubAction::SelectTheme:
                        setActiveThemePreset(hubHit.themeIndex);
                        setStatusMessage("Theme: " + getTheme().name);
                        break;
                    case ProjectHubAction::ToggleCrtShader:
                        crtShaderEnabled_ = !crtShaderEnabled_;
                        break;
                    case ProjectHubAction::OpenCrtTweaker:
                        crtTweakerOpen_ = true;
                        break;
                    case ProjectHubAction::ToggleAnimations:
                        guiAnimationsEnabled_ = !guiAnimationsEnabled_;
                        break;
                    case ProjectHubAction::SetAntiAliasing:
                        setAntiAliasingMode(hubHit.aaMode);
                        break;
                    case ProjectHubAction::ToggleHiDpi:
                        toggleHiDpi();
                        break;
                    case ProjectHubAction::CrtPresetStudioRef: {
                        auto cfg = dawnBridge_.getMaterialConfig();
                        cfg.scanlineIntensity = 0.38f;
                        cfg.curvature = 0.85f;
                        cfg.crtReflectionLevel = 1.0f;
                        cfg.hsyncDistortion = 1.0f;
                        cfg.spotlightIntensity = 1.0f;
                        cfg.spotlightSize = 1.0f;
                        cfg.vignetteStrength = 1.0f;
                        cfg.reflectionOpacity = 0.45f;
                        cfg.panelSoftness = 0.50f;
                        cfg.panelSaturation = 0.70f;
                        cfg.panelBlackLift = 0.025f;
                        crtShaderEnabled_ = true;
                        dawnBridge_.setMaterialConfig(cfg);
                        dawnBridge_.setCrtShaderEnabled(true);
                        setStatusMessage("CRT Preset: STUDIO REF (Calibrated Hardware)");
                        break;
                    }
                    case ProjectHubAction::CrtPresetMaxClarity: {
                        auto cfg = dawnBridge_.getMaterialConfig();
                        cfg.scanlineIntensity = 0.0f;
                        cfg.curvature = 0.0f;
                        cfg.crtReflectionLevel = 0.0f;
                        cfg.hsyncDistortion = 0.0f;
                        cfg.spotlightIntensity = 0.0f;
                        cfg.spotlightSize = 1.0f;
                        cfg.vignetteStrength = 0.0f;
                        cfg.reflectionOpacity = 0.0f;
                        cfg.panelSoftness = 0.0f;
                        cfg.panelSaturation = 1.0f;
                        cfg.panelBlackLift = 0.0f;
                        dawnBridge_.setMaterialConfig(cfg);
                        setStatusMessage("CRT Preset: MAX CLARITY (Flat Screen, 0 Scanlines, 0 Glare)");
                        break;
                    }
                    case ProjectHubAction::CrtPresetWarmVintage: {
                        auto cfg = dawnBridge_.getMaterialConfig();
                        cfg.scanlineIntensity = 0.55f;
                        cfg.curvature = 1.15f;
                        cfg.crtReflectionLevel = 1.25f;
                        cfg.hsyncDistortion = 1.35f;
                        cfg.spotlightIntensity = 1.25f;
                        cfg.spotlightSize = 1.20f;
                        cfg.vignetteStrength = 1.35f;
                        cfg.reflectionOpacity = 0.65f;
                        cfg.panelSoftness = 1.0f;
                        cfg.panelSaturation = 0.65f;
                        cfg.panelBlackLift = 0.035f;
                        crtShaderEnabled_ = true;
                        dawnBridge_.setMaterialConfig(cfg);
                        dawnBridge_.setCrtShaderEnabled(true);
                        setStatusMessage("CRT Preset: WARM VINTAGE (Deep Bulb & Scanlines)");
                        break;
                    }
                    case ProjectHubAction::CrtPresetReset: {
                        CrtMaterialConfig cfg{};
                        dawnBridge_.setMaterialConfig(cfg);
                        setStatusMessage("CRT Preset: RESET (Factory Defaults)");
                        break;
                    }
                    case ProjectHubAction::CrtSlider: {
                        DialogLayout dl = computeDialogLayout(540.0f, 580.0f);
                        dragMode_ = DragMode::CrtTweakerSlider;
                        crtTweakerSliderIndex_ = hubHit.crtSliderIndex;
                        crtTweakerTrackX_ = dl.x + 28.0f;
                        crtTweakerTrackW_ = dl.w - 64.0f;
                        handleCrtTweakerDrag(x, y);
                        break;
                    }
                    case ProjectHubAction::Scrollbar: {
                        PointerEvent pev;
                        pev.type = PointerType::Mouse;
                        pev.action = PointerAction::Down;
                        pev.button = PointerButton::Left;
                        pev.x = x;
                        pev.y = y;
                        if (projectHubScrollArea_.handlePointer(pev)) {
                            projectHubScrollY_ = projectHubScrollArea_.getScrollY();
                        }
                        dragMode_ = DragMode::ProjectHubScroll;
                        dragStartY_ = y;
                        dragStartScrollY_ = projectHubScrollY_;
                        projectHubDraggingThumb_ = true;
                        break;
                    }
                    case ProjectHubAction::ContentDrag:
                        dragMode_ = DragMode::ProjectHubScroll;
                        dragStartY_ = y;
                        dragStartScrollY_ = projectHubScrollY_;
                        projectHubDraggingThumb_ = false;
                        break;
                    default:
                        break;
                }
                return;
            } else if (y >= 56.0f) {
                // Clicked outside modal in workspace area -> close modal
                projectHubOpen_ = false;
                isEditingTitle_ = false;
                isEditingAuthor_ = false;
                return;
            }
        }

        // 0. Check Preset Browser Drawer Hit (if open)
        if (browserOpen_) {
            auto bHit = hitTestBrowser(x, y);
            if (bHit.hit) {
                if (bHit.action == BrowserHitAction::Close) {
                    setBrowserOpen(false);
                    return;
                } else if (bHit.action == BrowserHitAction::CategorySelect) {
                    browserCategory_ = bHit.category;
                    return;
                } else if (bHit.action == BrowserHitAction::PresetSelect) {
                    if (bHit.presetIndex < presets_.size()) {
                        selectedBrowserPresetIndex_ = bHit.presetIndex;
                    }
                    return;
                } else if (bHit.action == BrowserHitAction::PresetLoad) {
                    if (bHit.presetIndex < presets_.size()) {
                        selectedBrowserPresetIndex_ = bHit.presetIndex;
                        loadPresetToSelectedTrack(bHit.presetIndex);
                    }
                    return;
                } else if (bHit.action == BrowserHitAction::TabPresets) {
                    browserTab_ = BrowserTab::Presets;
                    return;
                } else if (bHit.action == BrowserHitAction::TabMacros) {
                    browserTab_ = BrowserTab::Macros;
                    return;
                } else if (bHit.action == BrowserHitAction::TabHistory) {
                    browserTab_ = BrowserTab::History;
                    return;
                } else if (bHit.action == BrowserHitAction::MacroRun) {
                    runMacro(bHit.macroIndex);
                    return;
                } else if (bHit.action == BrowserHitAction::SaveProject) {
                    saveProjectToFile(projectFilePath_.empty() ? "project.eats" : projectFilePath_);
                    return;
                } else if (bHit.action == BrowserHitAction::LoadProject) {
                    loadProjectFromFile(projectFilePath_.empty() ? "project.eats" : projectFilePath_);
                    return;
                } else if (bHit.action == BrowserHitAction::BounceMaster) {
                    bounceMasterToWav("bounce.wav");
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryUndo) {
                    undoHistory();
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryRedo) {
                    redoHistory();
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryMilestone) {
                    createHistoryMilestone("Checkpoint " + std::to_string(diffHistory_.getTimelineCount()));
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryClear) {
                    clearHistory();
                    return;
                } else if (bHit.action == BrowserHitAction::HistoryStepSelect) {
                    jumpToHistoryIndex(bHit.historyStepIndex);
                    return;
                }
            }

            if (projectBrowserDrawerWidget_) {
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = PointerButton::Left;
                pev.x = x;
                pev.y = y;
                if (projectBrowserDrawerWidget_->handlePointer(pev)) {
                    return;
                } else if (y >= 56.0f && x < projectBrowserDrawerWidget_->getDrawerBounds().x) {
                    // Clicked outside drawer in workspace area -> close drawer
                    setBrowserOpen(false);
                    return;
                }
            }
        }

        // 1. Check Top Transport Header Hit
        auto transportHit = hitTestTransport(x, y);
        if (transportHit.hit) {
            switch (transportHit.action) {
                case TransportAction::ProjectHubToggle:
                    projectHubOpen_ = !projectHubOpen_;
                    if (projectHubOpen_) browserOpen_ = false;
                    break;
                case TransportAction::PlayPause:
                    if (engine_) {
                        if (engine_->getSequencer().isPlaying()) {
                            engine_->getSequencer().stop();
                        } else {
                            engine_->getSequencer().start();
                        }
                    }
                    break;
                case TransportAction::Stop:
                    if (engine_) {
                        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                        if ((nowMs - lastStopClickTimeMs_) < 400) {
                            engine_->panic();
                            setStatusMessage("[PANIC] Full Audio Stop: All buffers flushed & voices killed");
                        } else {
                            engine_->getSequencer().stop();
                        }
                        lastStopClickTimeMs_ = nowMs;
                    }
                    break;
                case TransportAction::Record:
                    break;
                case TransportAction::Bpm:
                    if (engine_) {
                        dragMode_ = DragMode::BpmScrubber;
                        dragStartY_ = y;
                        dragStartValGeneric_ = static_cast<float>(engine_->getSequencer().getBpm());
                    }
                    break;
                case TransportAction::Swing:
                    if (engine_) {
                        dragMode_ = DragMode::SwingScrubber;
                        dragStartY_ = y;
                        dragStartValGeneric_ = static_cast<float>(engine_->getSequencer().getSwing());
                    }
                    break;
                case TransportAction::MasterVol:
                    dragMode_ = DragMode::MasterVolume;
                    dragStartY_ = y;
                    dragStartValGeneric_ = 1.0f;
                    break;
                case TransportAction::LoopToggle:
                    toggleLoop();
                    break;
                case TransportAction::MetronomeToggle:
                    toggleMetronome();
                    break;
                case TransportAction::BrowserToggle:
                    toggleBrowser();
                    if (browserOpen_) projectHubOpen_ = false;
                    break;
                case TransportAction::FullscreenToggle:
                    toggleFullscreen();
                    break;
                case TransportAction::SearchToggle:
                    toggleCommandPalette();
                    break;
                case TransportAction::LockToggle:
                    projectLocked_ = !projectLocked_;
                    setStatusMessage(projectLocked_ ? "[LOCK] Workspace Edit Protection Engaged" : "[LOCK] Workspace Edit Protection Released");
                    break;
                case TransportAction::SnapToggle:
                    break;
                case TransportAction::ScaleToggle:
                    if (uiScale_ < 0.80f) setUiScale(0.85f);
                    else if (uiScale_ < 0.95f) setUiScale(1.0f);
                    else if (uiScale_ < 1.15f) setUiScale(1.25f);
                    else if (uiScale_ < 1.40f) setUiScale(1.50f);
                    else setUiScale(0.75f);
                    break;
                case TransportAction::TabRack:
                    setActiveView(WorkspaceView::ModularRack);
                    break;
                case TransportAction::TabHardware:
                    syncTrackToPreset(selectedTrackIndex_);
                    setActiveView(WorkspaceView::HardwarePanel);
                    break;
                case TransportAction::PresetPrev:
                    prevPreset();
                    break;
                case TransportAction::PresetNext:
                    nextPreset();
                    break;
                case TransportAction::Toggle3dConsole:
                case TransportAction::ToggleCameraFocus:
                    break;
                default:
                    break;
            }
            return;
        }

        // 2. Check Bottom Navigation Control Strip Hit
        auto navHit = hitTestBottomNav(x, y);
        if (navHit.hit) {
            switch (navHit.action) {
                case BottomNavAction::Arranger:
                    setActiveView(WorkspaceView::Arranger);
                    break;
                case BottomNavAction::Edit:
                    setActiveView(WorkspaceView::Edit);
                    break;
                case BottomNavAction::Track:
                    syncTrackToPreset(selectedTrackIndex_);
                    setActiveView(WorkspaceView::Track);
                    break;
                case BottomNavAction::Mixer:
                    setActiveView(WorkspaceView::Mixer);
                    break;
                case BottomNavAction::Design:
                    setActiveView(WorkspaceView::Design);
                    break;
                default:
                    break;
            }
            return;
        }

        // 2b. Check Virtual Keyboard Drawer Hit (Global across views)
        auto drawerHit = hitTestVirtualKeyboardDrawer(x, y);
        if (drawerHit.hit) {
            if (drawerHit.isPullTab) {
                toggleVirtualKeyboardDrawer();
                return;
            }
            if (drawerHit.isOctaveDown) {
                setVirtualKeyboardBaseOctave(virtualKeyboardBaseOctave_ - 1);
                return;
            }
            if (drawerHit.isOctaveUp) {
                setVirtualKeyboardBaseOctave(virtualKeyboardBaseOctave_ + 1);
                return;
            }
            if (drawerHit.isKey) {
                virtualKeyboardActivePitch_ = drawerHit.pitch;
                previewingPitch_ = drawerHit.pitch;
                previewingVelocity_ = drawerHit.velocity;
                if (engine_) {
                    engine_->postNoteOn(static_cast<uint8_t>(drawerHit.pitch), drawerHit.velocity);
                }
                dragMode_ = DragMode::VirtualKeyboardGlissando;
                return;
            }
        }

        // 3. Sub-bar Switchers (y: 56 .. 95)
        if (activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) {
            auto sn = hitTestEditSubNav(x, y);
            if (sn.hit) {
                setEditSubView(sn.subView);
                return;
            }
        }

        // 4. Dispatch clicks according to Active View
        if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            modularEditView_->setActiveTrackIndex(selectedTrackIndex_);
            if (modularEditView_->handlePointer(pev, ctx)) {
                return;
            }
        }
        if (activeView_ == WorkspaceView::Arranger) {
            if (modularArrangerView_) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularArrangerView_->handlePointer(pev, ctx)) {
                    setSelectedTrackIndex(modularArrangerView_->getActiveTrackIndex());
                    return;
                }
            }
            auto arrHit = hitTestArranger(x, y);
            if (arrHit.hit) {
                if (arrHit.area == ArrangerHitArea::Ruler) {
                    dragMode_ = DragMode::ArrangerRulerScrub;
                    if (engine_) {
                        engine_->getSequencer().getTransport().setPosition(arrHit.step);
                    }
                } else if (arrHit.area == ArrangerHitArea::TrackMute) {
                    selectedTrackIndex_ = arrHit.trackIndex;
                    bool currentMute = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].mute : false;
                    setTrackMuteState(arrHit.trackIndex, !currentMute);
                } else if (arrHit.area == ArrangerHitArea::TrackSolo) {
                    selectedTrackIndex_ = arrHit.trackIndex;
                    bool currentSolo = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].solo : false;
                    setTrackSoloState(arrHit.trackIndex, !currentSolo);
                } else if (arrHit.area == ArrangerHitArea::TrackFreeze) {
                    selectedTrackIndex_ = arrHit.trackIndex;
                    bool currentFz = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].freeze : false;
                    setTrackFreezeState(arrHit.trackIndex, !currentFz);
                } else if (arrHit.area == ArrangerHitArea::TrackVolume) {
                    selectedTrackIndex_ = arrHit.trackIndex;
                    activeArrangerTrack_ = arrHit.trackIndex;
                    dragMode_ = DragMode::ArrangerTrackVolume;
                    dragStartX_ = x;
                    if (static_cast<size_t>(arrHit.trackIndex) < mixerStrips_.size()) {
                        mixerStrips_[arrHit.trackIndex].volume = arrHit.normVal;
                    }
                    if (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) {
                        arrangerTracks_[arrHit.trackIndex].volume = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::TrackPan) {
                    selectedTrackIndex_ = arrHit.trackIndex;
                    activeArrangerTrack_ = arrHit.trackIndex;
                    dragMode_ = DragMode::ArrangerTrackPan;
                    dragStartX_ = x;
                    dragStartY_ = y;
                    dragStartPan_ = (static_cast<size_t>(arrHit.trackIndex) < arrangerTracks_.size()) ? arrangerTracks_[arrHit.trackIndex].pan : 0.0f;
                } else if (arrHit.area == ArrangerHitArea::TrackEditButton) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    setActiveView(WorkspaceView::Edit);
                } else if (arrHit.area == ArrangerHitArea::TrackHeader) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (arrHit.trackIndex == lastClickedTrack_ && (nowMs - static_cast<int64_t>(lastTrackClickTime_)) < 400) {
                        setActiveView(WorkspaceView::Edit);
                    }
                    lastClickedTrack_ = arrHit.trackIndex;
                    lastTrackClickTime_ = static_cast<double>(nowMs);
                } else if (arrHit.area == ArrangerHitArea::Clip) {
                    setSelectedTrackIndex(arrHit.trackIndex);
                    selectArrangerClip(arrHit.trackIndex, arrHit.clipIndex);
                    arrangerPropertiesExpanded_ = true;
                    arrangerInspectorTab_ = ArrangerInspectorTab::Clip;
                    if (engine_) {
                        engine_->getSequencer().getTransport().setPosition(arrHit.step);
                    }
                    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                    if (lastClickedClipTrack_ == static_cast<int>(arrHit.trackIndex) &&
                        lastClickedClipIdx_ == arrHit.clipIndex &&
                        (nowMs - static_cast<int64_t>(lastClipClickTime_)) < 400) {
                        setActiveView(WorkspaceView::Edit);
                    }
                    lastClickedClipTrack_ = static_cast<int>(arrHit.trackIndex);
                    lastClickedClipIdx_ = arrHit.clipIndex;
                    lastClipClickTime_ = static_cast<double>(nowMs);
                } else if (arrHit.area == ArrangerHitArea::SidebarPullTab) {
                    dragMode_ = DragMode::ArrangerPropertiesResize;
                    dragStartX_ = x;
                    dragStartValGeneric_ = arrangerPropertiesWidth_;
                } else if (arrHit.area == ArrangerHitArea::SidebarCloseButton) {
                    arrangerPropertiesExpanded_ = false;
                } else if (arrHit.area == ArrangerHitArea::SidebarTabTrack) {
                    arrangerInspectorTab_ = ArrangerInspectorTab::Track;
                } else if (arrHit.area == ArrangerHitArea::SidebarTabClip) {
                    arrangerInspectorTab_ = ArrangerInspectorTab::Clip;
                } else if (arrHit.area == ArrangerHitArea::SidebarColorSwatch) {
                    static const float swatches[8][3] = {
                        {0.0f, 0.90f, 1.0f},   // Neon Cyan
                        {1.0f, 0.55f, 0.0f},   // Neon Amber
                        {0.0f, 1.00f, 0.40f},  // Acid Green
                        {1.0f, 0.00f, 0.48f},  // Hot Pink
                        {0.74f, 0.00f, 1.0f},  // Electric Purple
                        {1.0f, 0.20f, 0.20f},  // Crimson Red
                        {1.0f, 0.85f, 0.0f},   // Gold Yellow
                        {0.20f, 0.60f, 1.0f}   // Sky Blue
                    };
                    if (arrHit.colorSwatchIndex < 8 && selectedTrackIndex_ < arrangerTracks_.size()) {
                        auto& trk = arrangerTracks_[selectedTrackIndex_];
                        trk.r = swatches[arrHit.colorSwatchIndex][0];
                        trk.g = swatches[arrHit.colorSwatchIndex][1];
                        trk.b = swatches[arrHit.colorSwatchIndex][2];
                        for (auto& clp : trk.clips) {
                            clp.r = trk.r;
                            clp.g = trk.g;
                            clp.b = trk.b;
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarClipLoopToggle) {
                    if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                        static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                        auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                        if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                            trk.clips[selectedArrangerClipIndex_].isLooped = !trk.clips[selectedArrangerClipIndex_].isLooped;
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarClipTranspose) {
                    if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                        static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                        auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                        if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                            if (arrHit.transposeDelta == 0) {
                                trk.clips[selectedArrangerClipIndex_].transposeSemitones = 0;
                            } else {
                                trk.clips[selectedArrangerClipIndex_].transposeSemitones += arrHit.transposeDelta;
                                trk.clips[selectedArrangerClipIndex_].transposeSemitones = std::clamp(trk.clips[selectedArrangerClipIndex_].transposeSemitones, -48, 48);
                            }
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarEqLow) {
                    if (selectedTrackIndex_ < arrangerTracks_.size()) {
                        arrangerTracks_[selectedTrackIndex_].eqLow = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarEqMid) {
                    if (selectedTrackIndex_ < arrangerTracks_.size()) {
                        arrangerTracks_[selectedTrackIndex_].eqMid = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarEqHigh) {
                    if (selectedTrackIndex_ < arrangerTracks_.size()) {
                        arrangerTracks_[selectedTrackIndex_].eqHigh = arrHit.normVal;
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarAction) {
                    if (arrHit.actionName == "EditInPianoRoll") {
                        setActiveView(WorkspaceView::Edit);
                    } else if (arrHit.actionName == "DuplicateTrack") {
                        if (selectedTrackIndex_ < arrangerTracks_.size()) {
                            ArrangerTrackData dup = arrangerTracks_[selectedTrackIndex_];
                            dup.name += " (Copy)";
                            arrangerTracks_.push_back(dup);
                        }
                    } else if (arrHit.actionName == "DuplicateClip") {
                        if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                            static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                            auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                            if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                                ArrangerClip dup = trk.clips[selectedArrangerClipIndex_];
                                dup.startBar += dup.barLength;
                                trk.clips.push_back(dup);
                                selectedArrangerClipIndex_ = static_cast<int>(trk.clips.size() - 1);
                            }
                        }
                    } else if (arrHit.actionName == "DeleteClip") {
                        if (selectedArrangerClipTrack_ >= 0 && selectedArrangerClipIndex_ >= 0 &&
                            static_cast<size_t>(selectedArrangerClipTrack_) < arrangerTracks_.size()) {
                            auto& trk = arrangerTracks_[selectedArrangerClipTrack_];
                            if (static_cast<size_t>(selectedArrangerClipIndex_) < trk.clips.size()) {
                                trk.clips.erase(trk.clips.begin() + selectedArrangerClipIndex_);
                                selectedArrangerClipIndex_ = -1;
                                selectedArrangerClipTrack_ = -1;
                            }
                        }
                    }
                } else if (arrHit.area == ArrangerHitArea::OverviewScrollbar) {
                    dragMode_ = DragMode::ArrangerOverviewScroll;
                    if (engine_) {
                        uint32_t targetStep = static_cast<uint32_t>(arrHit.normVal * 18.0f * 16.0f);
                        engine_->getSequencer().getTransport().setPosition(targetStep);
                    }
                } else if (arrHit.area == ArrangerHitArea::SidebarTrackInspector || arrHit.trackInspectorHit.hit) {
                    handleTrackInspectorInteraction(arrHit.trackInspectorHit, x, y);
                }
            }
            return;
        }

        if (activeView_ == WorkspaceView::HardwarePanel || activeView_ == WorkspaceView::Track) {
            auto inspHit = hitTestTrackInspector(x, y);
            if (inspHit.hit) {
                handleTrackInspectorInteraction(inspHit, x, y);
                return;
            }
            if (modularTrackInspectorView_) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularTrackInspectorView_->handlePointer(pev, ctx)) return;
            }
            auto tabHit = hitTestTrackTab(x, y);
            if (tabHit.hit) {
                setSelectedTrackIndex(tabHit.trackIndex);
                if (modularTrackInspectorView_) {
                    modularTrackInspectorView_->setActiveTrack(tabHit.trackIndex);
                }
                return;
            }
            return;
        }

        if (activeView_ == WorkspaceView::Mixer) {
            auto mixerHit = hitTestMixer(x, y);
            if (mixerHit.hit) {
                // Toolbar Presets & Toggles
                if (mixerHit.isPresetFull) {
                    setMixerSectionPreset(MixerSectionPreset::Full);
                    setStatusMessage("[MIXER] Preset: FULL STUDIO CONSOLE");
                    return;
                } else if (mixerHit.isPresetFaders) {
                    setMixerSectionPreset(MixerSectionPreset::FadersOnly);
                    setStatusMessage("[MIXER] Preset: COMPACT FADERS");
                    return;
                } else if (mixerHit.isPresetMeters) {
                    setMixerSectionPreset(MixerSectionPreset::FadersAndMeters);
                    setStatusMessage("[MIXER] Preset: FADERS & METERS");
                    return;
                } else if (mixerHit.isToggleRouting) {
                    showMixerRouting_ = !showMixerRouting_;
                    showMixerAutomation_ = showMixerRouting_;
                    setStatusMessage(std::string("[MIXER] Routing Section: ") + (showMixerRouting_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isTogglePan) {
                    showMixerPan_ = !showMixerPan_;
                    setStatusMessage(std::string("[MIXER] Panorama Section: ") + (showMixerPan_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isToggleButtons) {
                    showMixerButtons_ = !showMixerButtons_;
                    setStatusMessage(std::string("[MIXER] Console Buttons: ") + (showMixerButtons_ ? "ENABLED" : "HIDDEN"));
                    return;
                } else if (mixerHit.isToggleMeters) {
                    showMixerMeters_ = !showMixerMeters_;
                    setStatusMessage(std::string("[MIXER] LED Meters: ") + (showMixerMeters_ ? "ENABLED" : "HIDDEN"));
                    return;
                }

                // Pull-Tab and Track Properties Sidebar Interactions
                if (mixerHit.isPullTab) {
                    dragMode_ = DragMode::MixerPropertiesResize;
                    dragStartX_ = x;
                    dragStartValGeneric_ = mixerPropertiesWidth_;
                    return;
                }
                if (mixerHit.isPropertiesClose) {
                    mixerPropertiesExpanded_ = false;
                    return;
                }
                if (mixerHit.trackInspectorHit.hit) {
                    handleTrackInspectorInteraction(mixerHit.trackInspectorHit, x, y);
                    return;
                }

                // LCD Screen Clicks (Selects track & opens/ensures sidebar expanded)
                if (mixerHit.isLcdScreen) {
                    if (!mixerHit.isMaster && mixerHit.channelIndex < mixerStrips_.size()) {
                        setSelectedTrackIndex(mixerHit.channelIndex);
                    }
                    mixerPropertiesExpanded_ = true;
                    return;
                }

                if (mixerHit.isMaster) {
                    if (mixerHit.isMute) {
                        setMasterMuted(!isMasterMuted());
                    } else if (mixerHit.isFader) {
                        dragMode_ = DragMode::MasterFader;
                        dragStartY_ = y;
                        dragStartValGeneric_ = masterVolume_;
                    } else if (mixerHit.isPan) {
                        dragMode_ = DragMode::MasterPan;
                        dragStartY_ = y;
                        dragStartValGeneric_ = masterPan_;
                    }
                } else if (mixerHit.channelIndex < mixerStrips_.size()) {
                    setSelectedTrackIndex(mixerHit.channelIndex);
                    if (mixerHit.isEditButton) {
                        // User constraint: The mixer shouldn't link to EDIT view at all.
                        // Clicking only selects the track and updates/expands track properties.
                        mixerPropertiesExpanded_ = true;
                    } else if (mixerHit.isMute) {
                        bool currentMute = (mixerHit.channelIndex < mixerStrips_.size()) ? mixerStrips_[mixerHit.channelIndex].mute : false;
                        setTrackMuteState(mixerHit.channelIndex, !currentMute);
                    } else if (mixerHit.isSolo) {
                        bool currentSolo = (mixerHit.channelIndex < mixerStrips_.size()) ? mixerStrips_[mixerHit.channelIndex].solo : false;
                        setTrackSoloState(mixerHit.channelIndex, !currentSolo);
                    } else if (mixerHit.isFreeze) {
                        bool currentFz = (mixerHit.channelIndex < mixerStrips_.size()) ? mixerStrips_[mixerHit.channelIndex].freeze : false;
                        setTrackFreezeState(mixerHit.channelIndex, !currentFz);
                    } else if (mixerHit.isPhase) {
                        mixerStrips_[mixerHit.channelIndex].phaseInvert = !mixerStrips_[mixerHit.channelIndex].phaseInvert;
                        setStatusMessage("Track " + std::to_string(mixerHit.channelIndex + 1) + ": Phase " + (mixerStrips_[mixerHit.channelIndex].phaseInvert ? "INVERTED (180deg)" : "NORMAL"));
                    } else if (mixerHit.isFxIn) {
                        mixerStrips_[mixerHit.channelIndex].fxIn = !mixerStrips_[mixerHit.channelIndex].fxIn;
                        setStatusMessage("Track " + std::to_string(mixerHit.channelIndex + 1) + ": Inserts " + (mixerStrips_[mixerHit.channelIndex].fxIn ? "ACTIVE" : "BYPASSED"));
                    } else if (mixerHit.isAutomation) {
                        mixerStrips_[mixerHit.channelIndex].automationMode = (mixerStrips_[mixerHit.channelIndex].automationMode + 1) % 4;
                        const char* aModes[] = {"TRIM", "READ", "TOUCH", "LATCH"};
                        setStatusMessage("Track " + std::to_string(mixerHit.channelIndex + 1) + " Automation: " + aModes[mixerStrips_[mixerHit.channelIndex].automationMode]);
                    } else if (mixerHit.isFader) {
                        dragMode_ = DragMode::MixerFader;
                        activeMixerChannel_ = mixerHit.channelIndex;
                        dragStartY_ = y;
                        dragStartValGeneric_ = mixerStrips_[mixerHit.channelIndex].volume;
                    } else if (mixerHit.isPan) {
                        dragMode_ = DragMode::MixerPan;
                        activeMixerChannel_ = mixerHit.channelIndex;
                        dragStartY_ = y;
                        dragStartValGeneric_ = mixerStrips_[mixerHit.channelIndex].pan;
                    }
                }
            }
            return;
        }

        if (activeView_ == WorkspaceView::Tracker || activeView_ == WorkspaceView::Edit) {
            auto* activeTrk = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                                  ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                                  : nullptr;

            // 0. Check Note Selection Sidebar Hit first
            if (activeTrk && activeTrk->hasSelectedNotes()) {
                const float bPanelY = static_cast<float>(height_) - 48.0f;
                const float sbW = 265.0f;
                const float sbX = static_cast<float>(width_) - sbW - 10.0f;
                const float sbY = 98.0f;
                const float sbH = bPanelY - sbY - 6.0f;
                auto sbHit = hitTestSelectionSidebar(x, y, sbX, sbY, sbW, sbH, *activeTrk);
                if (sbHit.hit) {
                    switch (sbHit.action) {
                        case SelectionSidebarAction::Close:
                        case SelectionSidebarAction::Clear:
                            activeTrk->clearSelection();
                            break;
                        case SelectionSidebarAction::Delete:
                            activeTrk->deleteSelectedNotes();
                            recordProjectHistory("Delete Selected Notes on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeOctDown:
                            activeTrk->transposeSelectedNotes(-12);
                            recordProjectHistory("Transpose -1 Octave", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeSemiDown:
                            activeTrk->transposeSelectedNotes(-1);
                            recordProjectHistory("Transpose -1 Semitone", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeSemiUp:
                            activeTrk->transposeSelectedNotes(1);
                            recordProjectHistory("Transpose +1 Semitone", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::TransposeOctUp:
                            activeTrk->transposeSelectedNotes(12);
                            recordProjectHistory("Transpose +1 Octave", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::NudgeLeft:
                            activeTrk->nudgeSelectedNotes(-1);
                            recordProjectHistory("Nudge Notes Left", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::NudgeRight:
                            activeTrk->nudgeSelectedNotes(1);
                            recordProjectHistory("Nudge Notes Right", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::ShortenDuration:
                            activeTrk->changeSelectedNotesDuration(-0.25f);
                            recordProjectHistory("Shorten Duration", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::LengthenDuration:
                            activeTrk->changeSelectedNotesDuration(0.25f);
                            recordProjectHistory("Lengthen Duration", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset25:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 0.25f;
                            }
                            recordProjectHistory("Set Duration 0.25", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset50:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 0.50f;
                            }
                            recordProjectHistory("Set Duration 0.50", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset75:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 0.75f;
                            }
                            recordProjectHistory("Set Duration 0.75", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::DurPreset100:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                activeTrk->getStep(s).gateLength = 1.00f;
                            }
                            recordProjectHistory("Set Duration 1.00", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel25:
                            activeTrk->setSelectedNotesVelocity(0.25f);
                            recordProjectHistory("Set Velocity 25%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel50:
                            activeTrk->setSelectedNotesVelocity(0.50f);
                            recordProjectHistory("Set Velocity 50%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel75:
                            activeTrk->setSelectedNotesVelocity(0.75f);
                            recordProjectHistory("Set Velocity 75%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Vel100:
                            activeTrk->setSelectedNotesVelocity(1.00f);
                            recordProjectHistory("Set Velocity 100%", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::Humanize:
                            activeTrk->humanizeSelectedNotes(0.15f);
                            recordProjectHistory("Humanize Velocity", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::ToggleSlide:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                auto& st = activeTrk->getStep(s);
                                st.slide = !st.slide;
                            }
                            recordProjectHistory("Toggle Slide", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::ToggleAccent:
                            for (uint32_t s : activeTrk->getSelectedSteps()) {
                                auto& st = activeTrk->getStep(s);
                                st.accent = !st.accent;
                            }
                            recordProjectHistory("Toggle Accent", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        case SelectionSidebarAction::SelectAll:
                            activeTrk->selectAllNotes();
                            break;
                        case SelectionSidebarAction::Invert:
                            activeTrk->invertNoteSelection();
                            break;
                        case SelectionSidebarAction::Quantize:
                            activeTrk->quantizeSelectedNotes(1);
                            recordProjectHistory("Quantize Notes", "NOTE");
                            syncTrackToNoteScript();
                            break;
                        default:
                            break;
                    }
                    return;
                }
            }

            if (editSubView_ == EditSubView::PianoRoll) {
                // 0. Piano Roll Scrollbar Hit
                auto sbHit = hitTestPianoRollScrollbar(x, y);
                if (sbHit.hit) {
                    if (sbHit.isVertical) {
                        if (sbHit.isThumb) {
                            dragMode_ = DragMode::PianoRollScrollbarV;
                            panStartMouseY_ = y;
                            panStartScrollY_ = pianoRollScrollY_;
                        } else {
                            float totalH = pianoRollKeyboard_.getTotalVerticalContentHeight();
                            float maxScrollY = std::max(0.0f, totalH - 550.0f);
                            setPianoRollScrollY(sbHit.normOffset * maxScrollY);
                        }
                        return;
                    } else {
                        if (sbHit.isThumb) {
                            dragMode_ = DragMode::PianoRollScrollbarH;
                            panStartMouseX_ = x;
                            panStartScrollX_ = pianoRollScrollX_;
                        } else {
                            const uint32_t totalSteps = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks() && engine_->getSequencer().getTrack(selectedTrackIndex_))
                                                            ? std::max(64u, engine_->getSequencer().getTrack(selectedTrackIndex_)->getNumSteps())
                                                            : 64u;
                            float totalW = static_cast<float>(totalSteps) * pianoRollStepW_;
                            float maxScrollX = std::max(0.0f, totalW - 1000.0f);
                            setPianoRollScrollX(sbHit.normOffset * maxScrollX);
                        }
                        return;
                    }
                }

                // 1. Piano Key Hit (Auditory & Visual Note Preview with Velocity & Glissando Drag)
                auto keyHit = hitTestPianoKey(x, y);
                if (keyHit.hit) {
                    previewingPitch_ = keyHit.pitch;
                    previewingVelocity_ = keyHit.velocity;
                    virtualKeyboardActivePitch_ = keyHit.pitch;
                    dragMode_ = DragMode::VirtualKeyboardGlissando;
                    if (engine_) {
                        engine_->postNoteOn(keyHit.pitch, keyHit.velocity);
                    }
                    return;
                }

                // 2. Check Hit on Existing Note Blocks or their Resize Handles
                if (activeTrk) {
                    const int minPitch = pianoRollKeyboard_.getMinPitch();
                    const int maxPitch = pianoRollKeyboard_.getMaxPitch();
                    const float topY = 100.0f;
                    const float rowH = pianoRollRowH_;
                    const float gridX = 110.0f;
                    const float stepW = pianoRollStepW_;

                    for (uint32_t s = 0; s < activeTrk->getNumSteps(); ++s) {
                        const auto& st = activeTrk->getStep(s);
                        if (st.active && st.note >= minPitch && st.note <= maxPitch) {
                            float nx = gridX + static_cast<float>(s) * stepW - pianoRollScrollX_ + 2.0f;
                            float ny = topY + static_cast<float>(maxPitch - st.note) * rowH - pianoRollScrollY_ + 2.0f;
                            float nw = std::max(14.0f, (stepW * st.gateLength) - 4.0f);
                            float nh = rowH - 4.0f;

                            // Check resize handle on selected note
                            if (activeTrk->isStepSelected(s) && nw >= 24.0f) {
                                float handleW = 12.0f;
                                float handleX = nx + nw - handleW;
                                if (x >= handleX && x <= nx + nw && y >= ny && y <= ny + nh) {
                                    dragMode_ = DragMode::PianoRollNoteResize;
                                    activeResizeStep_ = static_cast<int>(s);
                                    dragStartX_ = x;
                                    batchResizeStartDurations_.clear();
                                    for (uint32_t selS : activeTrk->getSelectedSteps()) {
                                        batchResizeStartDurations_[selS] = activeTrk->getStep(selS).gateLength;
                                    }
                                    return;
                                }
                            }

                            // Check note block body
                            if (x >= nx && x <= (nx + nw) && y >= ny && y <= (ny + nh)) {
                                bool isDouble = (lastClickedStep_ == static_cast<int>(s));
                                lastClickedStep_ = static_cast<int>(s);

                                if (isDouble) {
                                    // Double click -> delete note (from Eatsbeats)
                                    if (activeTrk->isStepSelected(s) && activeTrk->getSelectedNoteCount() > 1) {
                                        activeTrk->deleteSelectedNotes();
                                    } else {
                                        activeTrk->getStep(s) = sequencer::StepData{};
                                        activeTrk->clearSelection();
                                    }
                                    lastClickedStep_ = -1;
                                    recordProjectHistory("Delete Note", "NOTE");
                                    syncTrackToNoteScript();
                                    return;
                                }

                                // Single click -> select note (supporting Shift multi-selection toggle from Eatsbeats)
                                if (isShiftPressed()) {
                                    activeTrk->toggleStepSelection(s);
                                } else if (!activeTrk->isStepSelected(s)) {
                                    activeTrk->selectStep(s, true);
                                }
                                if (engine_) {
                                    engine_->postNoteOn(st.note, st.velocity);
                                    previewingPitch_ = st.note;
                                    previewingVelocity_ = st.velocity;
                                }

                                dragMode_ = DragMode::PianoRollNoteMove;
                                activeMoveStep_ = static_cast<int>(s);
                                dragStartX_ = x;
                                dragStartY_ = y;
                                batchMoveStartPitches_.clear();
                                batchMoveStartSteps_.clear();
                                for (uint32_t selS : activeTrk->getSelectedSteps()) {
                                    batchMoveStartPitches_[selS] = activeTrk->getStep(selS).note;
                                    batchMoveStartSteps_[selS] = selS;
                                }
                                return;
                            }
                        }
                    }
                }

                // 3. Piano Roll Grid Hit (Empty space -> Marquee Drag or Double-Click Insert)
                auto gridHit = hitTestPianoRollGrid(x, y);
                if (gridHit.hit && activeTrk) {
                    bool isDouble = (lastClickedStep_ == -2);
                    lastClickedStep_ = -2;

                    if (isDouble) {
                        // Double click on empty grid -> place note and select it (from Eatsbeats)
                        auto st = activeTrk->getStep(gridHit.step);
                        st.active = true;
                        st.note = gridHit.pitch;
                        st.velocity = 0.85f;
                        st.gateLength = 0.75f;
                        activeTrk->setStep(gridHit.step, st);
                        activeTrk->selectStep(gridHit.step, true);
                        if (engine_) {
                            engine_->postNoteOn(st.note, st.velocity);
                            previewingPitch_ = st.note;
                            previewingVelocity_ = st.velocity;
                        }
                        syncTrackToNoteScript();
                        recordProjectHistory("Add Note on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
                        lastClickedStep_ = -1;
                        return;
                    }

                    // Single click on empty grid -> marquee selection drag (clears selection if released without drag)
                    dragMode_ = DragMode::PianoRollMarquee;
                    isMarqueeSelecting_ = true;
                    marqueeStartX_ = x;
                    marqueeStartY_ = y;
                    marqueeCurX_ = x;
                    marqueeCurY_ = y;
                    return;
                }
            } else if (editSubView_ == EditSubView::Tracker) {
                // Tracker Grid Hit (Header Mute/Solo or Cell Selection/Audition)
                // Also check top Follow button at [width - 250, 103, 105, 24]
                float folX = static_cast<float>(width_) - 250.0f;
                if (x >= folX && x <= (folX + 105.0f) && y >= 103.0f && y <= 127.0f) {
                    trackerFollowPlayback_ = !trackerFollowPlayback_;
                    return;
                }

                auto trkHit = hitTestTracker(x, y);
                if (trkHit.hit && engine_) {
                    auto& seq = engine_->getSequencer();
                    if (trkHit.trackIndex < seq.getNumTracks()) {
                        auto* tr = seq.getTrack(trkHit.trackIndex);
                        if (tr) {
                            if (trkHit.isHeader) {
                                if (trkHit.isMute) {
                                    tr->setMuted(!tr->isMuted());
                                    if (trkHit.trackIndex < mixerStrips_.size()) {
                                        mixerStrips_[trkHit.trackIndex].mute = tr->isMuted();
                                    }
                                } else if (trkHit.isSolo) {
                                    tr->setSolo(!tr->isSolo());
                                    if (trkHit.trackIndex < mixerStrips_.size()) {
                                        mixerStrips_[trkHit.trackIndex].solo = tr->isSolo();
                                    }
                                } else {
                                    trackerSelectedTrack_ = trkHit.trackIndex;
                                    selectedTrackIndex_ = trkHit.trackIndex;
                                }
                            } else {
                                trackerSelectedRow_ = trkHit.row;
                                trackerSelectedTrack_ = trkHit.trackIndex;
                                selectedTrackIndex_ = trkHit.trackIndex;
                                auto st = tr->getStep(trkHit.row);
                                if (st.active) {
                                    tr->selectStep(trkHit.row, true);
                                    engine_->postNoteOn(st.note, 0.85f);
                                    previewingPitch_ = st.note;
                                } else {
                                    tr->clearSelection();
                                }
                            }
                        }
                    }
                    return;
                }
            } else if (editSubView_ == EditSubView::Score) {
                auto scoreHit = hitTestScore(x, y);
                if (scoreHit.hit) {
                    switch (scoreHit.action) {
                        case ScoreHitAction::ClefGrandStaff:
                            setScoreClef(ScoreClef::GrandStaff);
                            break;
                        case ScoreHitAction::ClefTreble:
                            setScoreClef(ScoreClef::Treble);
                            break;
                        case ScoreHitAction::ClefBass:
                            setScoreClef(ScoreClef::Bass);
                            break;
                        case ScoreHitAction::DurationWhole:
                            setScoreDuration(ScoreNoteType::Whole);
                            break;
                        case ScoreHitAction::DurationHalf:
                            setScoreDuration(ScoreNoteType::Half);
                            break;
                        case ScoreHitAction::DurationQuarter:
                            setScoreDuration(ScoreNoteType::Quarter);
                            break;
                        case ScoreHitAction::DurationEighth:
                            setScoreDuration(ScoreNoteType::Eighth);
                            break;
                        case ScoreHitAction::DurationSixteenth:
                            setScoreDuration(ScoreNoteType::Sixteenth);
                            break;
                        case ScoreHitAction::AccidentalNone:
                            setScoreAccidental(ScoreAccidental::None);
                            break;
                        case ScoreHitAction::AccidentalSharp:
                            setScoreAccidental(ScoreAccidental::Sharp);
                            break;
                        case ScoreHitAction::AccidentalFlat:
                            setScoreAccidental(ScoreAccidental::Flat);
                            break;
                        case ScoreHitAction::StaffClick:
                            if (engine_) {
                                auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                                if (tr) {
                                    auto st = tr->getStep(scoreHit.step);
                                    if (st.active && st.note == scoreHit.pitch) {
                                        st.active = false;
                                        tr->setStep(scoreHit.step, st);
                                        tr->clearSelection();
                                        syncTrackToNoteScript();
                                    } else {
                                        st.active = true;
                                        st.note = scoreHit.pitch;
                                        st.velocity = 0.85f;
                                        float durSteps = scoreDurationToSteps(scoreDuration_);
                                        st.gateLength = std::clamp(durSteps / 4.0f, 0.25f, 4.0f);
                                        engine_->postNoteOn(st.note, st.velocity);
                                        previewingPitch_ = st.note;
                                        tr->setStep(scoreHit.step, st);
                                        tr->selectStep(scoreHit.step, true);
                                        syncTrackToNoteScript();
                                    }
                                }
                            }
                            break;
                        default:
                            break;
                    }
                    return;
                }
            } else if (editSubView_ == EditSubView::Script) {
                auto nsHit = hitTestNoteScript(x, y);
                if (nsHit.hit) {
                    if (nsHit.action == HitTestNoteScriptResult::Action::ApplySync) {
                        syncNoteScriptToTrack();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::RevertFromTrack) {
                        syncTrackToNoteScript();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::InsertTemplate) {
                        insertNoteScriptTemplate();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::Clear) {
                        if (engine_) {
                            auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                            if (tr) tr->clear();
                        }
                        syncTrackToNoteScript();
                        return;
                    } else if (nsHit.action == HitTestNoteScriptResult::Action::EditorClick) {
                        setNoteScriptCursor(nsHit.line, nsHit.col);
                        return;
                    }
                }
                return;
            } else {
                auto stepHit = hitTestStep(x, y);
                if (stepHit.hit && engine_) {
                    auto* tr = engine_->getSequencer().getTrack(stepHit.trackIndex);
                    if (tr) {
                        auto st = tr->getStep(stepHit.stepIndex);
                        st.active = !st.active;
                        st.velocity = 0.85f;
                        tr->setStep(stepHit.stepIndex, st);
                        syncTrackToNoteScript();
                        recordProjectHistory("Toggle Step " + std::to_string(stepHit.stepIndex + 1) + " on Track " + std::to_string(stepHit.trackIndex + 1), "STEP");
                    }
                }
                return;
            }
        }

        // Design interactions (Modular Rack, Eatscript, Split, or GUI Designer)
        if (activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) {
            if (modularDesignView_) {
                ViewContext ctx = createViewContext();
                PointerEvent pev;
                pev.type = PointerType::Mouse;
                pev.action = PointerAction::Down;
                pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
                pev.x = x;
                pev.y = y;
                pev.rawX = x;
                pev.rawY = y;
                if (modularDesignView_->handlePointer(pev, ctx)) {
                    switch (modularDesignView_->getSubMode()) {
                        case DesignSubMode::ModularRack:
                            designSubView_ = DesignSubView::ModularRack;
                            break;
                        case DesignSubMode::Code:
                            designSubView_ = DesignSubView::Eatscript;
                            break;
                        case DesignSubMode::GuiDesigner:
                            designSubView_ = DesignSubView::GuiDesigner;
                            break;
                        case DesignSubMode::Split:
                            break;
                    }
                    return;
                }
            }

            auto esHit = hitTestEatscript(x, y);
            if (esHit.hit) {
                if (esHit.action == HitTestEatscriptResult::Action::SubNavModular) {
                    setDesignSubView(DesignSubView::ModularRack);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::SubNavEatscript) {
                    setDesignSubView(DesignSubView::Eatscript);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::SubNavGuiDesigner) {
                    setDesignSubView(DesignSubView::GuiDesigner);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::Compile) {
                    compileActiveScript();
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::HotReload) {
                    hotReloadScriptToTrack(selectedTrackIndex_);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::ToggleAotView) {
                    scriptShowAotView_ = !scriptShowAotView_;
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::LoadTemplate) {
                    loadScriptTemplate(esHit.templateIndex);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::EditorClick) {
                    setScriptCursor(esHit.line, esHit.col);
                    return;
                } else if (esHit.action == HitTestEatscriptResult::Action::BindTrack) {
                    hotReloadScriptToTrack(selectedTrackIndex_);
                    return;
                }
            }

            if (designSubView_ == DesignSubView::Eatscript) {
                return;
            }

            // ModularRack interactions
            // Check Jack hit
            auto jack = hitTestJack(x, y);
            if (jack.hit && jack.isOutput) {
                dragMode_ = DragMode::PatchCable;
                dragCableSrcNode_ = jack.nodeId;
                dragCableSrcPort_ = jack.portIndex;
                dragCableSrcPos_ = jack.position;
                return;
            }

            // Check Knob hit
            auto knob = hitTestKnob(x, y);
            if (knob.hit) {
                dragMode_ = DragMode::Knob;
                activeKnobNode_ = knob.nodeId;
                activeKnobIndex_ = knob.knobIndex;
                dragStartY_ = y;
                dragStartValue_ = getKnobValue(knob.nodeId, knob.knobIndex);
                return;
            }

            // Check Step sequencer hit
            auto step = hitTestStep(x, y);
            if (step.hit && engine_) {
                auto* tr = engine_->getSequencer().getTrack(step.trackIndex);
                if (tr) {
                    auto st = tr->getStep(step.stepIndex);
                    st.active = !st.active;
                    st.velocity = 0.85f;
                    tr->setStep(step.stepIndex, st);
                }
                return;
            }
        }
    } else if (button == 1) { // Right click: manual value edit dialog, select notes by pitch on Piano Roll, or disconnect jack in Modular Rack
        // Check right-click on tempo / BPM area to open manual value edit dialog
        bool tempoHit = false;
        if (transportHeaderWidget_ && transportHeaderWidget_->getBpmBounds().contains(x, y)) {
            tempoHit = true;
        } else if (x >= 170.0f && x <= 300.0f && y >= 6.0f && y <= 48.0f) {
            tempoHit = true;
        }
        if (tempoHit) {
            float bVal = engine_ ? static_cast<float>(engine_->getSequencer().getTransport().getBpm()) : 120.0f;
            ValueEditRequest req;
            req.title = "PROJECT TEMPO";
            req.paramName = "Tempo";
            req.currentValue = bVal;
            req.minValue = 20.0f;
            req.maxValue = 300.0f;
            req.defaultValue = 120.0f;
            req.hasDefault = true;
            req.allowPercentage = false;
            req.unit = "BPM";
            const auto& theme = getTheme();
            req.accentColor = theme.tempoGlow;
            req.onCommit = [this](float val) {
                if (engine_) {
                    engine_->getSequencer().getTransport().setBpm(val);
                }
            };
            openValueEditDialog(req);
            return;
        }

        // Forward right click to active view for contextual edit dialogs (Mixer volume sliders, Arranger track headers, etc.)
        if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularMixerView_->handlePointer(pev, ctx)) return;
        }
        if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->handlePointer(pev, ctx)) return;
        }
        if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularTrackInspectorView_->handlePointer(pev, ctx)) return;
        }
        if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
            ViewContext ctx = createViewContext();
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Down;
            pev.button = PointerButton::Right;
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularDesignView_->handlePointer(pev, ctx)) return;
        }

        if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll) {
            auto keyHit = hitTestPianoKey(x, y);
            if (keyHit.hit) {
                // Right-click on vertical piano key: select all notes on active track matching this pitch (from Eatsbeats)
                selectPianoRollNotesByPitch(keyHit.pitch, isShiftPressed());
                return;
            }
        }
        if (activeView_ == WorkspaceView::ModularRack || activeView_ == WorkspaceView::Design) {
            auto jack = hitTestJack(x, y);
            if (jack.hit && engine_) {
                const auto conns = engine_->getGraph().getConnections();
                for (const auto& c : conns) {
                    if ((jack.isOutput && c.srcNode == jack.nodeId && c.srcPort == jack.portIndex) ||
                        (!jack.isOutput && c.dstNode == jack.nodeId && c.dstPort == jack.portIndex)) {
                        engine_->getGraph().disconnect(c.srcNode, c.srcPort, c.dstNode, c.dstPort);
                    }
                }
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                initDefaultKnobValues();
            }
        }
    }
}

void GuiWindow::onMouseUp(int button, float x, float y) {
    if (projectHubOpen_) {
        projectHubScrollArea_.stopDragging();
    }

    PluginSearchDialog* activePluginDialog = nullptr;
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPluginSearchDialog().isOpen()) {
            activePluginDialog = &modularArrangerView_->getPluginSearchDialog();
        } else if (modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            activePluginDialog = &modularArrangerView_->getPropertiesDrawer().getPluginSearchDialog();
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            activePluginDialog = &modularMixerView_->getPropertiesDrawer().getPluginSearchDialog();
        }
    }

    if (activePluginDialog && activePluginDialog->isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (activePluginDialog->handlePointer(pev)) return;
    }

    if (commandPaletteDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (commandPaletteDialog_.handlePointer(pev)) return;
    }

    if (audioToMidiDialog_.isOpen()) {
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (audioToMidiDialog_.handlePointer(pev)) return;
    }

    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            PointerEvent pev;
            pev.type = PointerType::Mouse;
            pev.action = PointerAction::Up;
            pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
            pev.x = x;
            pev.y = y;
            pev.rawX = x;
            pev.rawY = y;
            if (modularArrangerView_->getIconSearchDialog().handlePointer(pev)) return;
        }

        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        if (valueEditDialog_.handlePointer(pev)) return;
    }

    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularArrangerView_->handlePointer(pev, ctx);
    }
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularEditView_->handlePointer(pev, ctx);
    }
    if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularMixerView_->handlePointer(pev, ctx);
    }
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) && modularDesignView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularDesignView_->handlePointer(pev, ctx);
    }
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        PointerEvent pev;
        pev.type = PointerType::Mouse;
        pev.action = PointerAction::Up;
        pev.button = (button == 0) ? PointerButton::Left : ((button == 1) ? PointerButton::Right : PointerButton::Middle);
        pev.x = x;
        pev.y = y;
        pev.rawX = x;
        pev.rawY = y;
        modularTrackInspectorView_->handlePointer(pev, ctx);
    }

    if (button == 0 || button == 2) {
        if (previewingPitch_ != -1) {
            if (engine_) {
                engine_->postNoteOff(static_cast<uint8_t>(previewingPitch_));
            }
            previewingPitch_ = -1;
        }

        if (virtualKeyboardActivePitch_ != -1) {
            if (engine_) {
                engine_->postNoteOff(static_cast<uint8_t>(virtualKeyboardActivePitch_));
            }
            virtualKeyboardActivePitch_ = -1;
        }

        if (dragMode_ == DragMode::PianoRollMarquee) {
            if (std::abs(marqueeCurX_ - marqueeStartX_) < 4.0f && std::abs(marqueeCurY_ - marqueeStartY_) < 4.0f) {
                if (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks()) {
                    auto* tr = engine_->getSequencer().getTrack(selectedTrackIndex_);
                    if (tr) tr->clearSelection();
                }
            }
            isMarqueeSelecting_ = false;
        } else if (dragMode_ == DragMode::PianoRollNoteMove || dragMode_ == DragMode::PianoRollNoteResize) {
            syncTrackToNoteScript();
            recordProjectHistory("Modify Notes on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
        }

        if (dragMode_ == DragMode::PatchCable && engine_) {
            // Drop cable: Check if mouse is over an input jack
            auto destJack = hitTestJack(x, y);
            if (destJack.hit && !destJack.isOutput) {
                engine_->getGraph().connect(dragCableSrcNode_, dragCableSrcPort_, destJack.nodeId, destJack.portIndex);
                engine_->getGraph().compile();
                canvas_.updateRackLayout(engine_->getGraph());
                initDefaultKnobValues();
            }
        }
        if (dragMode_ == DragMode::ArrangerPropertiesResize) {
            if (std::abs(x - dragStartX_) < 4.0f) {
                arrangerPropertiesExpanded_ = !arrangerPropertiesExpanded_;
            }
        }
        if (dragMode_ == DragMode::MixerPropertiesResize) {
            if (std::abs(x - dragStartX_) < 4.0f) {
                mixerPropertiesExpanded_ = !mixerPropertiesExpanded_;
            }
        }
    }

    if (crtTweakerOpen_) {
        handleCrtTweakerPointer(x, y, false, true);
    }
    if (dragMode_ == DragMode::CrtTweakerSlider) {
        dragMode_ = DragMode::None;
        crtTweakerSliderIndex_ = -1;
    }

    dragMode_ = DragMode::None;
}

void GuiWindow::onMouseScroll(double xoffset, double yoffset) {
    PointerEvent pev;
    pev.type = PointerType::Mouse;
    pev.action = PointerAction::Scroll;
    pev.x = mouseX_;
    pev.y = mouseY_;
    pev.scrollX = static_cast<float>(xoffset);
    pev.scrollY = static_cast<float>(yoffset);

    // 1. Modals & Dialogs (ensure context-based scroll and absorb scroll so it never leaks to DAW)
    if (commandPaletteDialog_.isOpen()) {
        commandPaletteDialog_.handlePointer(pev);
        return;
    }
    if (valueEditDialog_.isOpen()) {
        if (modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
            modularArrangerView_->getIconSearchDialog().handleScroll(static_cast<float>(yoffset));
        }
        return;
    }
    if (audioToMidiDialog_.isOpen()) {
        return;
    }
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPluginSearchDialog().isOpen() ||
            modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            ViewContext ctx = createViewContext();
            modularArrangerView_->handlePointer(pev, ctx);
            return;
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            ViewContext ctx = createViewContext();
            modularMixerView_->handlePointer(pev, ctx);
            return;
        }
    }

    // 2. Eatsbits Settings / Project Hub (context-based scroll via reusable ScrollableArea)
    if (projectHubOpen_) {
        DialogLayout dl = computeDialogLayout(540.0f, 580.0f);
        if (mouseX_ >= dl.x && mouseX_ <= dl.x + dl.w && mouseY_ >= dl.y && mouseY_ <= dl.y + dl.h) {
            if (projectHubSection_ == 3 && projectHubScrollArea_.canScroll()) {
                projectHubScrollArea_.scrollBy(-static_cast<float>(yoffset) * 32.0f);
                projectHubScrollY_ = projectHubScrollArea_.getScrollY();
            }
        }
        return; // Absorb all scroll events while Settings is open so it never scrolls the DAW underneath
    }

    // 3. Sidebars & Drawers
    if (browserOpen_ && projectBrowserDrawerWidget_) {
        if (projectBrowserDrawerWidget_->handlePointer(pev)) return;
    }
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        if (modularArrangerView_->handlePointer(pev, ctx)) return;
    }
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        modularEditView_->handlePointer(pev, ctx);
    }
    if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        ViewContext ctx = createViewContext();
        if (modularMixerView_->handlePointer(pev, ctx)) return;
    }
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        if (modularTrackInspectorView_->handlePointer(pev, ctx)) return;
    }
    (void)xoffset;
    if (activeView_ == WorkspaceView::Track ||
        (activeView_ == WorkspaceView::Arranger && arrangerPropertiesExpanded_ && arrangerInspectorTab_ == ArrangerInspectorTab::Track) ||
        (activeView_ == WorkspaceView::Mixer && mixerPropertiesExpanded_)) {
        constexpr float maxScroll = 450.0f;
        trackInspectorScrollY_ = std::clamp(trackInspectorScrollY_ - static_cast<float>(yoffset) * 35.0f, 0.0f, maxScroll);
    } else if (activeView_ == WorkspaceView::Edit) {
        if (editSubView_ == EditSubView::Tracker) {
            trackerScrollY_ = std::max(0.0f, trackerScrollY_ - static_cast<float>(yoffset) * 20.0f);
        } else if (editSubView_ == EditSubView::Score) {
            scoreScrollX_ = std::max(0.0f, scoreScrollX_ - static_cast<float>(yoffset) * 25.0f);
        } else if (editSubView_ == EditSubView::PianoRoll) {
            bool isCtrl = false;
            bool isShift = false;
#ifndef __EMSCRIPTEN__
            if (window_) {
                isCtrl = (glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
                isShift = (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
            }
#endif
            if (isCtrl && isShift) {
                setPianoRollRowHeight(pianoRollRowH_ + static_cast<float>(yoffset) * 2.0f);
            } else if (isCtrl) {
                setPianoRollStepWidth(pianoRollStepW_ + static_cast<float>(yoffset) * 4.0f);
            } else if (isShift) {
                setPianoRollScrollX(pianoRollScrollX_ - static_cast<float>(yoffset) * 40.0f);
            } else {
                setPianoRollScrollY(pianoRollScrollY_ - static_cast<float>(yoffset) * 30.0f);
            }
        }
    }
}

void GuiWindow::onFilesDropped(const std::vector<std::string>& filePaths, float x, float y) {
    if (filePaths.empty()) return;

    if (onExternalFilesDropped) {
        onExternalFilesDropped(filePaths, x, y);
    }

    // 1. Audio-to-MIDI Modal Dialog routing
    if (audioToMidiDialog_.isOpen()) {
        for (const auto& path : filePaths) {
            std::filesystem::path fp(path);
            std::string ext = fp.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aif" || ext == ".aiff") {
                if (audioToMidiDialog_.loadAudioFile(path)) {
                    setStatusMessage("Loaded audio into Audio-to-MIDI Converter: " + fp.filename().string());
                    return;
                }
            }
        }
    }

    // 2. Delegate to active workspace view (e.g. ArrangerView timeline)
    ViewContext ctx = createViewContext();
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->handleFileDrop(filePaths, x, y, ctx)) {
            return;
        }
    }

    // 3. Global Fallback Dispatch
    for (const auto& path : filePaths) {
        std::filesystem::path fp(path);
        std::string ext = fp.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".eats" || ext == ".json") {
            if (loadProjectFromFile(path)) {
                setStatusMessage("Loaded project: " + fp.filename().string());
                return;
            }
        } else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac" || ext == ".aif" || ext == ".aiff" || ext == ".sf2") {
            if (modularArrangerView_) {
                if (modularArrangerView_->handleFileDrop({path}, x, y, ctx)) {
                    setStatusMessage("Imported sample into Arranger: " + fp.filename().string());
                    return;
                }
            }
        } else if (ext == ".mid" || ext == ".midi") {
            if (modularArrangerView_) {
                if (modularArrangerView_->handleFileDrop({path}, x, y, ctx)) {
                    setStatusMessage("Imported MIDI into Arranger: " + fp.filename().string());
                    return;
                }
            }
        } else if (ext == ".eatscript") {
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                setActiveView(WorkspaceView::Design);
                setStatusMessage("Loaded Eatscript: " + fp.filename().string());
                return;
            }
        }
    }
}

void GuiWindow::onKeyDown(int key, int mods) {
    // Intercept keyboard input if CRT Shader Tweaker is open (Escape dismisses)
    if (crtTweakerOpen_) {
        if (key == 256) { // GLFW_KEY_ESCAPE
            crtTweakerOpen_ = false;
            return;
        }
    }

    // Intercept keyboard input if Command Palette is open
    if (commandPaletteDialog_.isOpen()) {
        if (commandPaletteDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Stacked Icon Search Modal Dialog is open
    if (valueEditDialog_.isOpen() && modularArrangerView_ && modularArrangerView_->getIconSearchDialog().isOpen()) {
        if (modularArrangerView_->getIconSearchDialog().handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Reusable Value Edit Modal Dialog is open
    if (valueEditDialog_.isOpen()) {
        if (valueEditDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Audio-to-MIDI Modal Dialog is open
    if (audioToMidiDialog_.isOpen()) {
        if (audioToMidiDialog_.handleKey(key, 0, 1 /* GLFW_PRESS */, mods)) {
            return;
        }
    }

    // Intercept keyboard input if Plugin Search Modal Dialog is open
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        if (modularArrangerView_->getPluginSearchDialog().isOpen() ||
            modularArrangerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            ViewContext ctx = createViewContext();
            if (modularArrangerView_->handleKey(key, 0, 1, mods, ctx)) return;
        }
    } else if (activeView_ == WorkspaceView::Mixer && modularMixerView_) {
        if (modularMixerView_->getPropertiesDrawer().isPluginDialogOpen()) {
            ViewContext ctx = createViewContext();
            if (modularMixerView_->handleKey(key, 0, 1, mods, ctx)) return;
        }
    }

    bool isCtrl = (mods & 2) != 0;
    bool isShift = (mods & 1) != 0;
    bool isAlt = (mods & 4) != 0;

    // Ctrl+M: Open Audio to MIDI Converter Modal Dialog
    if (isCtrl && (key == 77 || key == 109)) { // 'M'
        openAudioToMidiConverter();
        return;
    }

    // Ctrl+P or Ctrl+K: Toggle Universal Quick Command Palette
    if (isCtrl && (key == 80 || key == 112 || key == 75 || key == 107)) { // 'P' or 'K'
        toggleCommandPalette();
        return;
    }

    if (!engine_) return;

    // Piano Roll & Editor Note Selection Keyboard Shortcuts
    auto* activeTrk = (engine_ && selectedTrackIndex_ < engine_->getSequencer().getNumTracks())
                          ? engine_->getSequencer().getTrack(selectedTrackIndex_)
                          : nullptr;
    if (activeView_ == WorkspaceView::Edit && editSubView_ == EditSubView::PianoRoll && activeTrk && activeTrk->hasSelectedNotes()) {
        if (key == 261 || key == 259) { // GLFW_KEY_DELETE or GLFW_KEY_BACKSPACE
            activeTrk->deleteSelectedNotes();
            recordProjectHistory("Delete Selected Notes on Track " + std::to_string(selectedTrackIndex_ + 1), "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 256) { // GLFW_KEY_ESCAPE
            activeTrk->clearSelection();
            return;
        }
        if (isCtrl && (key == 65 || key == 97)) { // Ctrl+A
            activeTrk->selectAllNotes();
            return;
        }
        if (key == 265) { // GLFW_KEY_UP
            activeTrk->transposeSelectedNotes(isShift ? 12 : 1);
            recordProjectHistory("Transpose Up", "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 264) { // GLFW_KEY_DOWN
            activeTrk->transposeSelectedNotes(isShift ? -12 : -1);
            recordProjectHistory("Transpose Down", "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 263) { // GLFW_KEY_LEFT
            activeTrk->nudgeSelectedNotes(-1);
            recordProjectHistory("Nudge Left", "NOTE");
            syncTrackToNoteScript();
            return;
        }
        if (key == 262) { // GLFW_KEY_RIGHT
            activeTrk->nudgeSelectedNotes(1);
            recordProjectHistory("Nudge Right", "NOTE");
            syncTrackToNoteScript();
            return;
        }
    } else if (activeTrk && isCtrl && (key == 65 || key == 97) &&
               (activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker)) {
        activeTrk->selectAllNotes();
        return;
    }

    // Alt+F or F1: Toggle Project Hub & Settings Menu
    if ((isAlt && (key == 70 || key == 102)) || key == 290) { // 'F' or F1
        toggleProjectHub();
        return;
    }

    // F11 or Alt+Enter: Toggle Fullscreen (matching Eatsbeats)
    if (key == 300 || (isAlt && (key == 257 || key == 335))) {
        toggleFullscreen();
        return;
    }

    // F9: Toggle Apocalypse CRT Shader (Shift+F9 toggles CRT Tweaker HUD)
    if (key == 298) { // F9
        if (isShift) {
            toggleCrtTweaker();
            return;
        }
        toggleCrtShader();
        setStatusMessage(crtShaderEnabled_ ? "CRT Shader: ENABLED (Swaying Spotlight + Scanlines + Rumble)" : "CRT Shader: BYPASSED (Raw UI)");
        return;
    }

    // F10: Toggle CRT Shader & Chassis Tweaker HUD Modal
    if (key == 299) { // F10
        toggleCrtTweaker();
        return;
    }

    // Ctrl+S / Ctrl+Shift+S: Save / Save As
    if (isCtrl && (key == 83 || key == 115)) {
        if (isShift) {
            saveProjectAs();
        } else {
            saveProjectToFile(projectFilePath_);
        }
        return;
    }

    // Ctrl+O: Open / Load Project
    if (isCtrl && (key == 79 || key == 111)) {
        loadProjectPrompt();
        return;
    }

    // Ctrl+N: New Project
    if (isCtrl && (key == 78 || key == 110)) {
        resetToDefaultProject();
        return;
    }

    // Ctrl+E: Export Master WAV
    if (isCtrl && (key == 69 || key == 101)) {
        bounceMasterToWav(projectName_ + ".wav");
        return;
    }

    // Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y: Pure Diff Undo / Redo
    if (isCtrl && (key == 90 || key == 122)) { // 'Z'
        if (isShift) {
            redoHistory();
        } else {
            undoHistory();
        }
        return;
    }
    if (isCtrl && (key == 89 || key == 121)) { // 'Y'
        redoHistory();
        return;
    }

    // Escape (256): Close modal/drawer if open
    if (key == 256) {
        if (projectHubOpen_) {
            projectHubOpen_ = false;
            isEditingTitle_ = false;
            isEditingAuthor_ = false;
            return;
        }
        if (browserOpen_) {
            browserOpen_ = false;
            return;
        }
    }

    // If editing Title or Author in Project Hub:
    if (projectHubOpen_ && (isEditingTitle_ || isEditingAuthor_)) {
        std::string& targetStr = isEditingTitle_ ? projectName_ : authorName_;
        if (key == 257 || key == 256) { // Enter or Esc
            isEditingTitle_ = false;
            isEditingAuthor_ = false;
            return;
        }
        if (key == 259) { // Backspace
            if (!targetStr.empty()) targetStr.pop_back();
            return;
        }
        if (key == 32) { // Space
            targetStr += ' ';
            return;
        }
        if (key >= 32 && key <= 126) {
            char c = static_cast<char>(key);
            if (!isShift && c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            targetStr += c;
            return;
        }
        return;
    }

    // Global Zoom Hotkeys (Ctrl + '=', Ctrl + '-', Ctrl + '0')
    if (isCtrl) {
        if (key == 61 || key == 334) { // '=' / '+'
            zoomIn();
            return;
        }
        if (key == 45 || key == 333) { // '-'
            zoomOut();
            return;
        }
        if (key == 48 || key == 320) { // '0'
            resetZoom();
            return;
        }
    }

    // Arranger View Hotkeys & Plugin Search Dialog Text Input
    if (activeView_ == WorkspaceView::Arranger && modularArrangerView_) {
        ViewContext ctx = createViewContext();
        if (modularArrangerView_->handleKey(key, 0, 1, mods, ctx)) return;
    }
    // Edit View Hotkeys (Piano Roll, Tracker, Score, Script)
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) && modularEditView_) {
        ViewContext ctx = createViewContext();
        if (modularEditView_->handleKey(key, 0, 1, mods, ctx)) return;
    }
    // Track Inspector View Hotkeys & Plugin Search Dialog Input
    if ((activeView_ == WorkspaceView::Track || activeView_ == WorkspaceView::HardwarePanel) && modularTrackInspectorView_) {
        ViewContext ctx = createViewContext();
        if (modularTrackInspectorView_->handleKey(key, 0, 1, mods, ctx)) return;
    }

    // Note Script Editor Keyboard Navigation & Text Editing (EDIT > SCRIPT)
    if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) &&
        editSubView_ == EditSubView::Script && !browserOpen_) {
        // Apply/Sync to track: Ctrl+Enter (257) or F5 (294)
        if ((key == 257 && isCtrl) || key == 294) {
            syncNoteScriptToTrack();
            return;
        }

        // F1..F5: View navigation shortcuts
        if (key == 290) { setActiveView(WorkspaceView::Arranger); return; }
        if (key == 291) { setActiveView(WorkspaceView::Edit); return; }
        if (key == 292) { setActiveView(WorkspaceView::Track); return; }
        if (key == 293) { setActiveView(WorkspaceView::Mixer); return; }
        if (key == 294) { setActiveView(WorkspaceView::Design); return; }

        // Escape (256): Panic stop
        if (key == 256) {
            engine_->getSequencer().stop();
            return;
        }

        // Navigation
        if (key == 265) { setNoteScriptCursor(noteScriptCursorLine_ - 1, noteScriptCursorCol_); return; }
        if (key == 264) { setNoteScriptCursor(noteScriptCursorLine_ + 1, noteScriptCursorCol_); return; }
        if (key == 263) { setNoteScriptCursor(noteScriptCursorLine_, noteScriptCursorCol_ - 1); return; }
        if (key == 262) { setNoteScriptCursor(noteScriptCursorLine_, noteScriptCursorCol_ + 1); return; }
        if (key == 268) { setNoteScriptCursor(noteScriptCursorLine_, 0); return; }
        if (key == 269) {
            int len = (noteScriptCursorLine_ < static_cast<int>(noteScriptBuffer_.size())) ? static_cast<int>(noteScriptBuffer_[noteScriptCursorLine_].size()) : 0;
            setNoteScriptCursor(noteScriptCursorLine_, len);
            return;
        }
        if (key == 266) { setNoteScriptCursor(noteScriptCursorLine_ - 5, noteScriptCursorCol_); return; }
        if (key == 267) { setNoteScriptCursor(noteScriptCursorLine_ + 5, noteScriptCursorCol_); return; }

        // Editing
        if (key == 259) { deleteNoteScriptCharBackwards(); return; }
        if (key == 261) { deleteNoteScriptCharForwards(); return; }
        if (key == 257) { insertNoteScriptNewLine(); return; }
        if (key == 258) { insertNoteScriptText("    "); return; }
        if (key == 32)  { insertNoteScriptChar(' '); return; }

        // Character typing with shift awareness
        if (isShift) {
            if (key >= 65 && key <= 90) { insertNoteScriptChar(static_cast<char>(key)); return; }
            if (key == '3') { insertNoteScriptChar('#'); return; }
            if (key == '9') { insertNoteScriptChar('('); return; }
            if (key == '0') { insertNoteScriptChar(')'); return; }
            if (key == 39)  { insertNoteScriptChar('"'); return; }
            if (key == 91)  { insertNoteScriptChar('{'); return; }
            if (key == 93)  { insertNoteScriptChar('}'); return; }
            if (key == 59)  { insertNoteScriptChar(':'); return; }
            if (key == 61)  { insertNoteScriptChar('+'); return; }
            if (key == 45)  { insertNoteScriptChar('_'); return; }
        } else {
            if (key >= 65 && key <= 90) { insertNoteScriptChar(static_cast<char>(std::tolower(key))); return; }
            if (key == 39) { insertNoteScriptChar('\''); return; }
            if (key == 91) { insertNoteScriptChar('['); return; }
            if (key == 93) { insertNoteScriptChar(']'); return; }
            if (key == 59) { insertNoteScriptChar(';'); return; }
        }

        // Numbers 0..9
        if (key >= 48 && key <= 57) {
            insertNoteScriptChar(static_cast<char>(key));
            return;
        }

        // Direct punctuation
        if (key == 44) { insertNoteScriptChar(','); return; }
        if (key == 46) { insertNoteScriptChar('.'); return; }
        if (key == 45) { insertNoteScriptChar('-'); return; }
        if (key == 61) { insertNoteScriptChar('='); return; }
        if (key == 47) { insertNoteScriptChar('/'); return; }

        return;
    }

    // Eatscript IDE Keyboard Navigation & Text Editing
    if ((activeView_ == WorkspaceView::Design || activeView_ == WorkspaceView::ModularRack) &&
        designSubView_ == DesignSubView::Eatscript && !browserOpen_) {
        // F5 (294): Compile Script
        if (key == 294) {
            compileActiveScript();
            return;
        }
        // F1..F4: View navigation shortcuts
        if (key == 290 || key == 49) { // F1 or '1'
            setActiveView(WorkspaceView::Arranger);
            return;
        } else if (key == 291 || key == 50) { // F2 or '2'
            setActiveView(WorkspaceView::Edit);
            return;
        } else if (key == 292 || key == 51) { // F3 or '3'
            setActiveView(WorkspaceView::Track);
            return;
        } else if (key == 293 || key == 52) { // F4 or '4'
            setActiveView(WorkspaceView::Mixer);
            return;
        }
        // Escape (256): Panic stop
        if (key == 256) {
            engine_->getSequencer().stop();
            return;
        }
        // Up Arrow (265)
        if (key == 265) {
            setScriptCursor(scriptCursorLine_ - 1, scriptCursorCol_);
            return;
        }
        // Down Arrow (264)
        if (key == 264) {
            setScriptCursor(scriptCursorLine_ + 1, scriptCursorCol_);
            return;
        }
        // Left Arrow (263)
        if (key == 263) {
            setScriptCursor(scriptCursorLine_, scriptCursorCol_ - 1);
            return;
        }
        // Right Arrow (262)
        if (key == 262) {
            setScriptCursor(scriptCursorLine_, scriptCursorCol_ + 1);
            return;
        }
        // Home (268)
        if (key == 268) {
            setScriptCursor(scriptCursorLine_, 0);
            return;
        }
        // End (269)
        if (key == 269) {
            int len = (scriptCursorLine_ < static_cast<int>(scriptBuffer_.size())) ? static_cast<int>(scriptBuffer_[scriptCursorLine_].size()) : 0;
            setScriptCursor(scriptCursorLine_, len);
            return;
        }
        // Page Up (266)
        if (key == 266) {
            setScriptCursor(scriptCursorLine_ - 5, scriptCursorCol_);
            return;
        }
        // Page Down (267)
        if (key == 267) {
            setScriptCursor(scriptCursorLine_ + 5, scriptCursorCol_);
            return;
        }
        // Backspace (259)
        if (key == 259) {
            deleteScriptCharBackwards();
            return;
        }
        // Delete (261)
        if (key == 261) {
            deleteScriptCharForwards();
            return;
        }
        // Enter / Return (257)
        if (key == 257) {
            insertScriptNewLine();
            return;
        }
        // Tab (258)
        if (key == 258) {
            insertScriptText("    ");
            return;
        }
        // Space (32)
        if (key == 32) {
            insertScriptChar(' ');
            return;
        }
        // Characters: Letters (A-Z -> a-z)
        if (key >= 65 && key <= 90) {
            insertScriptChar(static_cast<char>(std::tolower(key)));
            return;
        }
        // Numbers (0-9)
        if (key >= 48 && key <= 57) {
            insertScriptChar(static_cast<char>(key));
            return;
        }
        // Punctuation & Operators
        if (key == 44) { insertScriptChar(','); return; }
        if (key == 46) { insertScriptChar('.'); return; }
        if (key == 45) { insertScriptChar('-'); return; }
        if (key == 61) { insertScriptChar('='); return; }
        if (key == 47) { insertScriptChar('/'); return; }
        if (key == 59) { insertScriptChar(':'); return; }
        if (key == 39) { insertScriptChar('\''); return; }
        if (key == 91) { insertScriptChar('['); return; }
        if (key == 93) { insertScriptChar(']'); return; }
        if (key == 92) { insertScriptChar('\\'); return; }
        return;
    }

    // Spacebar (32): Toggle Sequencer Play / Stop
    if (key == 32) {
        if (engine_->getSequencer().isPlaying()) {
            engine_->getSequencer().stop();
        } else {
            engine_->getSequencer().start();
        }
    }
    // 'B' (66): Toggle Preset Browser / Patch Librarian Drawer
    else if (key == 66) {
        toggleBrowser();
    }
    // Escape (256): Close Browser if open, otherwise Panic Stop
    else if (key == 256) {
        if (browserOpen_) {
            browserOpen_ = false;
        } else {
            engine_->getSequencer().stop();
        }
    }
    // Tracker View Navigation & QWERTY Musical Note Entry
    else if ((activeView_ == WorkspaceView::Edit || activeView_ == WorkspaceView::Tracker) &&
             editSubView_ == EditSubView::Tracker && !browserOpen_) {
        auto& seq = engine_->getSequencer();
        const size_t numTracks = seq.getNumTracks();

        // Up Arrow (265): Move cursor up 1 row
        if (key == 265) {
            trackerSelectedRow_ = (trackerSelectedRow_ == 0) ? 63 : trackerSelectedRow_ - 1;
        }
        // Down Arrow (264): Move cursor down 1 row
        else if (key == 264) {
            trackerSelectedRow_ = (trackerSelectedRow_ + 1) % 64;
        }
        // Left Arrow (263): Move cursor left 1 track
        else if (key == 263) {
            if (trackerSelectedTrack_ > 0) {
                trackerSelectedTrack_--;
                selectedTrackIndex_ = trackerSelectedTrack_;
            }
        }
        // Right Arrow (262): Move cursor right 1 track
        else if (key == 262) {
            if (numTracks > 0 && trackerSelectedTrack_ + 1 < numTracks) {
                trackerSelectedTrack_++;
                selectedTrackIndex_ = trackerSelectedTrack_;
            }
        }
        // Page Up (266): Jump 16 rows up (1 bar)
        else if (key == 266) {
            trackerSelectedRow_ = (trackerSelectedRow_ < 16) ? 0 : trackerSelectedRow_ - 16;
        }
        // Page Down (267): Jump 16 rows down (1 bar)
        else if (key == 267) {
            trackerSelectedRow_ = std::min(63u, trackerSelectedRow_ + 16);
        }
        // Home (268): Jump to row 0
        else if (key == 268) {
            trackerSelectedRow_ = 0;
        }
        // End (269): Jump to row 63
        else if (key == 269) {
            trackerSelectedRow_ = 63;
        }
        // Delete (261) or Backspace (259): Clear step note
        else if (key == 261 || key == 259) {
            if (numTracks > 0 && trackerSelectedTrack_ < numTracks) {
                auto* tr = seq.getTrack(trackerSelectedTrack_);
                if (tr) {
                    auto st = tr->getStep(trackerSelectedRow_);
                    st.active = false;
                    tr->setStep(trackerSelectedRow_, st);
                }
            }
            trackerSelectedRow_ = (trackerSelectedRow_ + 1) % 64;
        }
        // QWERTY Musical Note Entry
        else {
            int pitch = qwertyKeyToMidiPitch(key, 4);
            if (pitch >= 0) {
                if (numTracks > 0 && trackerSelectedTrack_ < numTracks) {
                    auto* tr = seq.getTrack(trackerSelectedTrack_);
                    if (tr) {
                        auto st = tr->getStep(trackerSelectedRow_);
                        st.active = true;
                        st.note = static_cast<uint8_t>(pitch);
                        st.velocity = 0.85f;
                        st.gateLength = 0.75f;
                        tr->setStep(trackerSelectedRow_, st);
                        engine_->postNoteOn(pitch, 0.85f);
                        previewingPitch_ = pitch;
                    }
                }
                trackerSelectedRow_ = (trackerSelectedRow_ + 1) % 64;
            }
        }
    }
    // 'R' (82): Panic Stop (outside tracker mode)
    else if (key == 82) {
        engine_->getSequencer().stop();
    }
    // Tab / Function Keys for rapid view switching
    else if (key == 290 || key == 49) { // F1 or '1'
        setActiveView(WorkspaceView::Arranger);
    } else if (key == 291 || key == 50) { // F2 or '2'
        setActiveView(WorkspaceView::Edit);
    } else if (key == 292 || key == 51) { // F3 or '3'
        setActiveView(WorkspaceView::Track);
    } else if (key == 293 || key == 52) { // F4 or '4'
        setActiveView(WorkspaceView::Mixer);
    } else if (key == 294 || key == 53) { // F5 or '5'
        setActiveView(WorkspaceView::Design);
    } else if (key == 258) { // Tab key cycles views
        int next = (static_cast<int>(activeView_) + 1) % 5;
        setActiveView(static_cast<WorkspaceView>(next));
    }
}

void GuiWindow::openCommandPalette() noexcept {
    commandPaletteDialog_.open();
}

void GuiWindow::closeCommandPalette() noexcept {
    commandPaletteDialog_.close();
}

void GuiWindow::toggleCommandPalette() noexcept {
    commandPaletteDialog_.toggle();
}

bool GuiWindow::isCommandPaletteOpen() const noexcept {
    return commandPaletteDialog_.isOpen();
}

void GuiWindow::onChar(unsigned int codepoint) {
    if (commandPaletteDialog_.isOpen()) {
        commandPaletteDialog_.handleChar(codepoint);
        return;
    }
    if (audioToMidiDialog_.isOpen()) {
        audioToMidiDialog_.handleChar(codepoint);
        return;
    }
}

void GuiWindow::openAudioToMidiConverter(const std::optional<audio::DecodedAudioBuffer>& initialBuffer,
                                        const std::string& name) {
    audioToMidiDialog_.open(initialBuffer, name);
}

void GuiWindow::closeAudioToMidiConverter() noexcept {
    audioToMidiDialog_.close();
}

bool GuiWindow::isAudioToMidiDialogOpen() const noexcept {
    return audioToMidiDialog_.isOpen();
}

void GuiWindow::renderCrtTweakerModal() {
    // Backdrop darkener
    drawRect(0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 0.0f, 0.0f, 0.40f);

    const float mW = 490.0f;
    const float mH = 612.0f;
    const float mX = (static_cast<float>(width_) - mW) * 0.5f;
    const float mY = (static_cast<float>(height_) - mH) * 0.5f;

    // Modal Outer Shadow & Glassmorphic Body
    drawRect(mX - 4.0f, mY - 4.0f, mW + 8.0f, mH + 8.0f, 0.0f, 0.0f, 0.0f, 0.50f);
    drawRoundedRectGradient(mX, mY, mW, mH, 8.0f, 0.11f, 0.13f, 0.16f, 0.06f, 0.07f, 0.09f, 0.96f);
    drawRoundedRectOutline(mX, mY, mW, mH, 8.0f, 0.0f, 0.85f, 0.95f, 0.80f, 1.5f);

    // Header Bar
    drawRoundedRectGradient(mX, mY, mW, 42.0f, 8.0f, 0.14f, 0.16f, 0.20f, 0.09f, 0.10f, 0.13f, 1.0f);
    drawLine(mX, mY + 42.0f, mX + mW, mY + 42.0f, 0.0f, 0.85f, 0.95f, 0.45f, 1.0f);

    // [CRT HUD] pill badge
    drawRoundedRect(mX + 14.0f, mY + 11.0f, 62.0f, 20.0f, 4.0f, 0.0f, 0.70f, 0.85f, 0.25f);
    drawRoundedRectOutline(mX + 14.0f, mY + 11.0f, 62.0f, 20.0f, 4.0f, 0.0f, 0.85f, 1.0f, 0.70f, 1.0f);
    drawVectorString("CRT HUD", mX + 20.0f, mY + 15.0f, 0.65f, Color(0.0f, 0.90f, 1.0f));

    // Title
    drawVectorString("CHASSIS & SHADER TWEAKER", mX + 84.0f, mY + 14.0f, 0.80f, Color(0.95f, 0.97f, 1.0f));

    // Master Power Toggle Button
    const float pBtnX = mX + mW - 138.0f;
    const float pBtnY = mY + 9.0f;
    drawRoundedRect(pBtnX, pBtnY, 92.0f, 24.0f, 4.0f, crtShaderEnabled_ ? 0.05f : 0.20f, crtShaderEnabled_ ? 0.35f : 0.08f, crtShaderEnabled_ ? 0.12f : 0.08f, 0.85f);
    drawRoundedRectOutline(pBtnX, pBtnY, 92.0f, 24.0f, 4.0f, crtShaderEnabled_ ? 0.20f : 0.60f, crtShaderEnabled_ ? 0.95f : 0.20f, crtShaderEnabled_ ? 0.30f : 0.20f, 0.90f, 1.2f);
    drawCircle(pBtnX + 12.0f, pBtnY + 12.0f, 4.0f, crtShaderEnabled_ ? 0.20f : 0.60f, crtShaderEnabled_ ? 0.95f : 0.20f, crtShaderEnabled_ ? 0.30f : 0.20f, 1.0f);
    drawVectorString(crtShaderEnabled_ ? "CRT: ON" : "CRT: OFF", pBtnX + 22.0f, pBtnY + 14.0f, 0.70f, Color(1.0f, 1.0f, 1.0f));

    // Close button [X]
    const float cBtnX = mX + mW - 36.0f;
    const float cBtnY = mY + 9.0f;
    drawRoundedRect(cBtnX, cBtnY, 26.0f, 24.0f, 4.0f, 0.25f, 0.08f, 0.08f, 0.70f);
    drawRoundedRectOutline(cBtnX, cBtnY, 26.0f, 24.0f, 4.0f, 0.90f, 0.30f, 0.30f, 0.85f, 1.0f);
    drawVectorString("X", cBtnX + 8.5f, cBtnY + 14.0f, 0.80f, Color(1.0f, 0.6f, 0.6f));

    // Subtitle & Hints
    drawVectorString("Real-time WebGPU Shader Parameters * Live Viewport Feedback", mX + 16.0f, mY + 49.0f, 0.60f, Color(0.60f, 0.65f, 0.72f));

    // Presets Row
    drawVectorString("PRESETS:", mX + 16.0f, mY + 74.0f, 0.62f, Color(0.0f, 0.85f, 0.95f));

    struct PresetBtn {
        const char* label;
        float x;
        float w;
    };
    PresetBtn presets[4] = {
        {"STUDIO REF", mX + 80.0f, 96.0f},
        {"MAX CLARITY", mX + 182.0f, 102.0f},
        {"WARM VINTAGE", mX + 290.0f, 106.0f},
        {"RESET", mX + 402.0f, 72.0f}
    };
    for (int p = 0; p < 4; ++p) {
        drawRoundedRect(presets[p].x, mY + 68.0f, presets[p].w, 22.0f, 4.0f, 0.12f, 0.15f, 0.19f, 0.90f);
        drawRoundedRectOutline(presets[p].x, mY + 68.0f, presets[p].w, 22.0f, 4.0f, 0.25f, 0.30f, 0.38f, 0.85f, 1.0f);
        drawVectorString(presets[p].label, presets[p].x + 10.0f, mY + 73.0f, 0.65f, Color(0.85f, 0.90f, 0.96f));
    }

    // Sliders
    const auto& cfg = dawnBridge_.getMaterialConfig();

    struct SliderItem {
        std::string label;
        float minVal;
        float maxVal;
        float curVal;
        std::string valStr;
        bool isGreen{false};
    };

    char buf6[32];
    snprintf(buf6, sizeof(buf6), (cfg.panelSoftness <= 0.01f) ? "0.00 px (SHARP)" : "%.2f px", cfg.panelSoftness);

    char buf8[32];
    snprintf(buf8, sizeof(buf8), (cfg.panelBlackLift <= 0.001f) ? "0.00 (PITCH BLACK)" : "+%.3f", cfg.panelBlackLift);

    SliderItem items[11] = {
        {"SCANLINE INTENSITY (0 = NONE)", 0.0f, 1.0f, cfg.scanlineIntensity,
         (cfg.scanlineIntensity <= 0.005f) ? "0.00 (OFF / CRISP)" : (std::to_string(static_cast<int>(std::round(cfg.scanlineIntensity * 100.0f))) + "%"),
         (cfg.scanlineIntensity <= 0.005f)},
        {"CRT BULB CURVATURE", 0.0f, 1.50f, cfg.curvature,
         (cfg.curvature <= 0.01f) ? "0.00 (FLAT GLASS)" : (std::to_string(static_cast<int>(std::round(cfg.curvature * 100.0f))) + "%"), false},
        {"CRT TUBE ROOM REFLECTION", 0.0f, 1.0f, cfg.crtReflectionLevel,
         (cfg.crtReflectionLevel <= 0.005f) ? "0.00 (OFF / NONE)" : (std::to_string(static_cast<int>(std::round(cfg.crtReflectionLevel * 100.0f))) + "%"),
         (cfg.crtReflectionLevel <= 0.005f)},
        {"H-SYNC WAVE DISTORTION", 0.0f, 2.0f, cfg.hsyncDistortion,
         (cfg.hsyncDistortion <= 0.005f) ? "0.00 (ZERO / STATIC)" : (std::to_string(static_cast<int>(std::round(cfg.hsyncDistortion * 100.0f))) + "%"),
         (cfg.hsyncDistortion <= 0.005f)},
        {"SPOTLIGHT INTENSITY", 0.0f, 2.0f, cfg.spotlightIntensity,
         (cfg.spotlightIntensity <= 0.01f) ? "0.00 (FLAT LIGHT)" : (std::to_string(static_cast<int>(std::round(cfg.spotlightIntensity * 100.0f))) + "%"), false},
        {"SPOTLIGHT BEAM SIZE", 0.5f, 2.5f, cfg.spotlightSize,
         std::to_string(static_cast<int>(std::round(cfg.spotlightSize * 100.0f))) + "%", false},
        {"VIGNETTE CORNER FALLOFF", 0.0f, 2.0f, cfg.vignetteStrength,
         (cfg.vignetteStrength <= 0.01f) ? "0.00 (NONE)" : (std::to_string(static_cast<int>(std::round(cfg.vignetteStrength * 100.0f))) + "%"), false},
        {"BEZEL FRAME REFLECTION", 0.0f, 1.0f, cfg.reflectionOpacity,
         (cfg.reflectionOpacity <= 0.01f) ? "0.00 (MATTE)" : (std::to_string(static_cast<int>(std::round(cfg.reflectionOpacity * 100.0f))) + "%"), false},
        {"PANEL SOFTNESS (SUBPIXEL BLUR)", 0.0f, 1.50f, cfg.panelSoftness, buf6, false},
        {"PANEL HARDWARE SATURATION", 0.0f, 1.0f, cfg.panelSaturation,
         std::to_string(static_cast<int>(std::round(cfg.panelSaturation * 100.0f))) + "%", false},
        {"PANEL BLACK FLOOR LIFT", 0.0f, 0.08f, cfg.panelBlackLift, buf8, false}
    };

    const float trackX = mX + 18.0f;
    const float trackW = mW - 36.0f;
    const float trackH = 6.0f;

    for (int i = 0; i < 11; ++i) {
        float rowY = mY + 98.0f + i * 45.0f;
        float trackY = rowY + 20.0f;

        // Label
        drawVectorString(items[i].label, trackX, rowY + 2.0f, 0.68f, Color(0.85f, 0.88f, 0.94f));

        // Value readout
        float valW = static_cast<float>(items[i].valStr.size()) * 7.2f;
        drawVectorString(items[i].valStr, trackX + trackW - valW, rowY + 2.0f, 0.70f,
                         items[i].isGreen ? Color(0.20f, 1.0f, 0.40f) : Color(0.0f, 0.90f, 1.0f));

        // Track Background
        drawRoundedRect(trackX, trackY, trackW, trackH, 3.0f, 0.04f, 0.05f, 0.07f, 1.0f);
        drawRoundedRectOutline(trackX, trackY, trackW, trackH, 3.0f, 0.18f, 0.20f, 0.26f, 0.8f, 1.0f);

        // Fill bar
        float t = std::clamp((items[i].curVal - items[i].minVal) / (items[i].maxVal - items[i].minVal), 0.0f, 1.0f);
        float fillW = t * trackW;
        if (fillW > 2.0f) {
            drawRoundedRectGradient(trackX, trackY, fillW, trackH, 3.0f, 0.0f, 0.65f, 0.85f, 0.0f, 0.90f, 0.95f, 0.95f);
        }

        // Thumb
        float thumbX = trackX + fillW;
        float thumbY = trackY + 3.0f;
        drawCircle(thumbX, thumbY, 8.0f, 0.0f, 0.0f, 0.0f, 0.40f);
        drawCircle(thumbX, thumbY, 6.5f, 0.90f, 0.96f, 1.0f, 1.0f);
        drawCircleOutline(thumbX, thumbY, 6.5f, 0.0f, 0.75f, 0.95f, 1.0f, 1.5f);
    }
}

bool GuiWindow::handleCrtTweakerPointer(float x, float y, bool isDown, bool isUp) {
    (void)isUp;
    if (!crtTweakerOpen_) return false;

    const float mW = 490.0f;
    const float mH = 612.0f;
    const float mX = (static_cast<float>(width_) - mW) * 0.5f;
    const float mY = (static_cast<float>(height_) - mH) * 0.5f;

    // Check click outside modal
    if (x < mX || x > mX + mW || y < mY || y > mY + mH) {
        if (isDown) {
            crtTweakerOpen_ = false;
            return true;
        }
        return false;
    }

    if (!isDown) {
        return true; // Consume event inside modal bounds
    }

    // 1. Close button [X]
    const float cBtnX = mX + mW - 36.0f;
    const float cBtnY = mY + 9.0f;
    if (x >= cBtnX && x <= cBtnX + 26.0f && y >= cBtnY && y <= cBtnY + 24.0f) {
        crtTweakerOpen_ = false;
        return true;
    }

    // 2. Power toggle button
    const float pBtnX = mX + mW - 138.0f;
    const float pBtnY = mY + 9.0f;
    if (x >= pBtnX && x <= pBtnX + 92.0f && y >= pBtnY && y <= pBtnY + 24.0f) {
        toggleCrtShader();
        dawnBridge_.setCrtShaderEnabled(crtShaderEnabled_);
        setStatusMessage(crtShaderEnabled_ ? "CRT Shader: ENABLED" : "CRT Shader: BYPASSED");
        return true;
    }

    // 3. Preset Buttons
    auto cfg = dawnBridge_.getMaterialConfig();

    // Preset 1: Studio Ref
    if (x >= mX + 80.0f && x <= mX + 176.0f && y >= mY + 68.0f && y <= mY + 90.0f) {
        cfg.scanlineIntensity = 0.38f;
        cfg.curvature = 0.85f;
        cfg.crtReflectionLevel = 1.0f;
        cfg.hsyncDistortion = 1.0f;
        cfg.spotlightIntensity = 1.0f;
        cfg.spotlightSize = 1.35f;
        cfg.vignetteStrength = 1.0f;
        cfg.reflectionOpacity = 0.52f;
        cfg.panelSoftness = 0.75f;
        cfg.panelSaturation = 0.70f;
        cfg.panelBlackLift = 0.025f;
        crtShaderEnabled_ = true;
        dawnBridge_.setMaterialConfig(cfg);
        dawnBridge_.setCrtShaderEnabled(true);
        setStatusMessage("CRT Preset: STUDIO REF (Calibrated Hardware)");
        return true;
    }

    // Preset 2: Max Clarity
    if (x >= mX + 182.0f && x <= mX + 284.0f && y >= mY + 68.0f && y <= mY + 90.0f) {
        cfg.scanlineIntensity = 0.0f;  // Scanlines completely disabled
        cfg.curvature = 0.0f;          // Flat screen
        cfg.crtReflectionLevel = 0.0f; // Zero tube reflection
        cfg.hsyncDistortion = 0.0f;    // Zero h-sync distortion / static raster
        cfg.spotlightIntensity = 0.0f; // Flat uniform lighting
        cfg.spotlightSize = 1.35f;
        cfg.vignetteStrength = 0.0f;   // No corner vignette
        cfg.reflectionOpacity = 0.0f;  // No frame glare
        cfg.panelSoftness = 0.0f;      // Crisp pixels
        cfg.panelSaturation = 1.0f;    // 100% saturation
        cfg.panelBlackLift = 0.0f;
        dawnBridge_.setMaterialConfig(cfg);
        setStatusMessage("CRT Preset: MAX CLARITY (Flat Screen, 0 Scanlines, 0 Glare)");
        return true;
    }

    // Preset 3: Warm Vintage
    if (x >= mX + 290.0f && x <= mX + 396.0f && y >= mY + 68.0f && y <= mY + 90.0f) {
        cfg.scanlineIntensity = 0.55f;
        cfg.curvature = 1.15f;
        cfg.crtReflectionLevel = 1.25f;
        cfg.hsyncDistortion = 1.35f;
        cfg.spotlightIntensity = 1.25f;
        cfg.spotlightSize = 1.20f;
        cfg.vignetteStrength = 1.35f;
        cfg.reflectionOpacity = 0.65f;
        cfg.panelSoftness = 1.0f;
        cfg.panelSaturation = 0.65f;
        cfg.panelBlackLift = 0.035f;
        crtShaderEnabled_ = true;
        dawnBridge_.setMaterialConfig(cfg);
        dawnBridge_.setCrtShaderEnabled(true);
        setStatusMessage("CRT Preset: WARM VINTAGE (Deep Bulb & Scanlines)");
        return true;
    }

    // Preset 4: Reset
    if (x >= mX + 402.0f && x <= mX + 474.0f && y >= mY + 68.0f && y <= mY + 90.0f) {
        cfg = CrtMaterialConfig{};
        dawnBridge_.setMaterialConfig(cfg);
        setStatusMessage("CRT Preset: RESET (Factory Defaults)");
        return true;
    }

    // 4. Slider Hit Tests
    const float trackX = mX + 18.0f;
    const float trackW = mW - 36.0f;
    for (int i = 0; i < 11; ++i) {
        float rowY = mY + 98.0f + i * 45.0f;
        if (x >= trackX - 8.0f && x <= trackX + trackW + 8.0f && y >= rowY + 8.0f && y <= rowY + 36.0f) {
            dragMode_ = DragMode::CrtTweakerSlider;
            crtTweakerSliderIndex_ = i;
            crtTweakerTrackX_ = trackX;
            crtTweakerTrackW_ = trackW;
            handleCrtTweakerDrag(x, y);
            return true;
        }
    }

    return true;
}

void GuiWindow::handleCrtTweakerDrag(float x, float y) {
    (void)y;
    if (crtTweakerSliderIndex_ < 0 || crtTweakerSliderIndex_ > 10 || crtTweakerTrackW_ <= 0.0f) return;

    float t = std::clamp((x - crtTweakerTrackX_) / crtTweakerTrackW_, 0.0f, 1.0f);
    auto cfg = dawnBridge_.getMaterialConfig();

    switch (crtTweakerSliderIndex_) {
        case 0:
            cfg.scanlineIntensity = t * 1.0f;
            setStatusMessage("Scanlines: " + std::to_string(static_cast<int>(std::round(cfg.scanlineIntensity * 100.0f))) + "%" + (cfg.scanlineIntensity <= 0.005f ? " (OFF)" : ""));
            break;
        case 1:
            cfg.curvature = t * 1.50f;
            setStatusMessage("CRT Curvature: " + std::to_string(static_cast<int>(std::round(cfg.curvature * 100.0f))) + "%");
            break;
        case 2:
            cfg.crtReflectionLevel = t * 1.0f;
            setStatusMessage("CRT Tube Reflection: " + std::to_string(static_cast<int>(std::round(cfg.crtReflectionLevel * 100.0f))) + "%" + (cfg.crtReflectionLevel <= 0.005f ? " (OFF)" : ""));
            break;
        case 3:
            cfg.hsyncDistortion = t * 2.0f;
            setStatusMessage("H-Sync Distortion: " + std::to_string(static_cast<int>(std::round(cfg.hsyncDistortion * 100.0f))) + "%" + (cfg.hsyncDistortion <= 0.005f ? " (ZERO / STATIC)" : ""));
            break;
        case 4:
            cfg.spotlightIntensity = t * 2.0f;
            setStatusMessage("Spotlight Intensity: " + std::to_string(static_cast<int>(std::round(cfg.spotlightIntensity * 100.0f))) + "%");
            break;
        case 5:
            cfg.spotlightSize = 0.5f + t * 2.0f;
            setStatusMessage("Spotlight Beam Size: " + std::to_string(static_cast<int>(std::round(cfg.spotlightSize * 100.0f))) + "%");
            break;
        case 6:
            cfg.vignetteStrength = t * 2.0f;
            setStatusMessage("Vignette Falloff: " + std::to_string(static_cast<int>(std::round(cfg.vignetteStrength * 100.0f))) + "%");
            break;
        case 7:
            cfg.reflectionOpacity = t * 1.0f;
            setStatusMessage("Frame Reflection: " + std::to_string(static_cast<int>(std::round(cfg.reflectionOpacity * 100.0f))) + "%");
            break;
        case 8: {
            cfg.panelSoftness = t * 1.50f;
            char buf[32];
            snprintf(buf, sizeof(buf), "Panel Softness: %.2f px", cfg.panelSoftness);
            setStatusMessage(buf);
            break;
        }
        case 9:
            cfg.panelSaturation = t * 1.0f;
            setStatusMessage("Panel Saturation: " + std::to_string(static_cast<int>(std::round(cfg.panelSaturation * 100.0f))) + "%");
            break;
        case 10: {
            cfg.panelBlackLift = t * 0.08f;
            char buf[32];
            snprintf(buf, sizeof(buf), "Panel Black Floor Lift: +%.3f", cfg.panelBlackLift);
            setStatusMessage(buf);
            break;
        }
        default:
            break;
    }

    dawnBridge_.setMaterialConfig(cfg);
}

} // namespace eatsbits::ui

