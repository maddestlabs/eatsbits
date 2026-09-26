#ifndef EATS_BATCH_RENDERER_2D_HPP
#define EATS_BATCH_RENDERER_2D_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include "eatsbits/ui/canvas_renderer.hpp"
#include "eatsbits/ui/geometry.hpp"

namespace eatsbits::ui {

/**
 * @brief Represents a single 2D vertex for batched UI rendering.
 * Suitable for both desktop Modern OpenGL (VBO) and WebGPU (WGSL pipeline).
 */
struct alignas(16) Vertex2D {
    float x{0.0f};
    float y{0.0f};
    float u{0.0f};
    float v{0.0f};
    uint32_t color{0xFFFFFFFF}; // RGBA8 packed
    uint32_t mode{0};          // 0 = solid/gradient color, 1 = font atlas texture
    float pad[2]{0.0f, 0.0f};  // 32-byte uniform alignment
};

enum class RenderBackendType {
    Filament,
    WebGPU,
    Headless
};

class IBatchRenderBackend {
public:
    virtual ~IBatchRenderBackend() = default;
    virtual bool initialize(void* windowHandle, uint32_t width, uint32_t height) = 0;
    virtual void shutdown() = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual void beginPass(float width, float height) = 0;
    virtual void updateFontTexture(int x, int y, int w, int h, const unsigned char* data, int atlasW, int atlasH) = 0;
    virtual void renderBatch(const std::vector<Vertex2D>& vertices) = 0;
    virtual void endPass() = 0;
    virtual RenderBackendType getBackendType() const noexcept = 0;
    virtual const uint32_t* getFramebuffer() const noexcept { return nullptr; }
    virtual void setDirectPresent(bool /*enable*/) noexcept {}
    virtual void setAntiAliasingMode(int /*mode*/) {}
    virtual void renderRgba(float /*x*/, float /*y*/, float /*w*/, float /*h*/, const uint8_t* /*rgba*/, int /*imgW*/, int /*imgH*/, float /*opacity*/) {}
    virtual void applyBackdropBlur(float /*radius*/ = 3.0f, float /*dimFactor*/ = 0.50f) {}
};

/**
 * @brief High-performance batched 2D vector renderer.
 * Collects 2D quads, circles, lines, and textured font glyphs into a single vertex buffer,
 * flushing them to the GPU in minimal draw passes.
 */
class BatchRenderer2D {
public:
    BatchRenderer2D();
    ~BatchRenderer2D();

    bool initialize(void* windowHandle, uint32_t width, uint32_t height, RenderBackendType backend = RenderBackendType::Filament);
    void shutdown();
    void resize(uint32_t width, uint32_t height);

    void beginFrame(float width, float height);
    void endFrame();
    void flush();
    void applyBackdropBlur(float radius = 3.0f, float dimFactor = 0.50f);

    // --- Vector Primitives ---
    void drawTriangle(float x0, float y0, float x1, float y1, float x2, float y2, float r, float g, float b, float a = 1.0f);
    void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f);
    void drawRectGradient(float x, float y, float w, float h, float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f);
    void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f);
    void drawRoundedRect(float x, float y, float w, float h, float radius, float r, float g, float b, float a = 1.0f, int cornerSegments = 8);
    void drawRoundedRectGradient(float x, float y, float w, float h, float radius, float r0, float g0, float b0, float r1, float g1, float b1, float a = 1.0f, int cornerSegments = 8);
    void drawRoundedRectOutline(float x, float y, float w, float h, float radius, float r, float g, float b, float a = 1.0f, float lineWidth = 1.0f, int cornerSegments = 8);
    void drawCircle(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, int segments = 36);
    void drawCircleOutline(float cx, float cy, float radius, float r, float g, float b, float a = 1.0f, float lineWidth = 1.5f, int segments = 36);
    void drawArc(float cx, float cy, float radius, float startAngle, float endAngle, float r, float g, float b, float a = 1.0f, float lineWidth = 2.0f, int segments = 36);
    void drawLine(float x0, float y0, float x1, float y1, float r, float g, float b, float a = 1.0f, float lineWidth = 1.5f);
    void drawCatenaryBezier(Point2D p0, Point2D cp0, Point2D cp1, Point2D p1, float r, float g, float b, float a, float lineWidth);
    void drawRgbaBitmap(float x, float y, float w, float h, const uint8_t* rgba, int imgW, int imgH, float opacity = 1.0f);

    // --- Render Scale (HiDPI / UI Scaling Transform) ---
    void setRenderScale(float sx, float sy) noexcept {
        renderScaleX_ = (sx > 0.001f) ? sx : 1.0f;
        renderScaleY_ = (sy > 0.001f) ? sy : 1.0f;
    }
    [[nodiscard]] float getRenderScaleX() const noexcept { return renderScaleX_; }
    [[nodiscard]] float getRenderScaleY() const noexcept { return renderScaleY_; }

    // --- 2D Rotation Transform ---
    void setRotation(float angleDegrees, float originX, float originY);
    void resetRotation() noexcept;
    [[nodiscard]] bool isTransformActive() const noexcept { return transformActive_; }

    // --- Contextual Shadows & Lighting ---
    void drawCircleDropShadow(float cx, float cy, float radius, float elevation, const LightSource2D& light, float opacity = 0.45f);
    void drawRectDropShadow(float x, float y, float w, float h, float cornerRadius, float elevation, const LightSource2D& light, float opacity = 0.40f);
    void drawKnurledRing(float cx, float cy, float rInner, float rOuter, int numTeeth, float rotationAngle,
                         const LightSource2D& light, float baseR, float baseG, float baseB,
                         float highlightR = 1.0f, float highlightG = 1.0f, float highlightB = 1.0f,
                         float shadowR = 0.08f, float shadowG = 0.09f, float shadowB = 0.11f);

    // --- Font & Texture Atlas ---
    void updateFontAtlas(int x, int y, int w, int h, const unsigned char* data, int atlasW, int atlasH);
    void drawTexturedTriangles(const float* verts, const float* tcoords, const unsigned int* colors, int nverts);

    [[nodiscard]] size_t getVertexCount() const noexcept { return vertices_.size(); }
    [[nodiscard]] RenderBackendType getBackendType() const noexcept;
    [[nodiscard]] const uint32_t* getFramebuffer() const noexcept { return backend_ ? backend_->getFramebuffer() : nullptr; }
    void setDirectPresent(bool enable) noexcept { if (backend_) backend_->setDirectPresent(enable); }
    void setAntiAliasingMode(int mode) noexcept {
        antiAliasingMode_ = mode;
        if (backend_) backend_->setAntiAliasingMode(mode);
    }
    [[nodiscard]] int getAntiAliasingMode() const noexcept { return antiAliasingMode_; }

    // Helper for packing RGBA floats into uint32_t (ABGR/RGBA order matching shader layout)
    static inline uint32_t packColor(float r, float g, float b, float a) noexcept {
        uint32_t ur = static_cast<uint32_t>(r * 255.0f) & 0xFF;
        uint32_t ug = static_cast<uint32_t>(g * 255.0f) & 0xFF;
        uint32_t ub = static_cast<uint32_t>(b * 255.0f) & 0xFF;
        uint32_t ua = static_cast<uint32_t>(a * 255.0f) & 0xFF;
        return (ua << 24) | (ub << 16) | (ug << 8) | ur;
    }

private:
    std::unique_ptr<IBatchRenderBackend> backend_;
    std::vector<Vertex2D> vertices_;
    float currentWidth_{1280.0f};
    float currentHeight_{800.0f};
    float renderScaleX_{1.0f};
    float renderScaleY_{1.0f};
    int antiAliasingMode_{2};
    bool inFrame_{false};

    // 2D Rotation Transform State
    bool transformActive_{false};
    float transformCos_{1.0f};
    float transformSin_{0.0f};
    float transformOriginX_{0.0f};
    float transformOriginY_{0.0f};

    inline void applyTransform(float& x, float& y) const noexcept {
        if (transformActive_) {
            float dx = x - transformOriginX_;
            float dy = y - transformOriginY_;
            x = transformOriginX_ + transformCos_ * dx - transformSin_ * dy;
            y = transformOriginY_ + transformSin_ * dx + transformCos_ * dy;
        }
    }
};

} // namespace eatsbits::ui

#endif // EATS_BATCH_RENDERER_2D_HPP
