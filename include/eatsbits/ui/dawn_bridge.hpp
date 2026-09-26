#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace eatsbits::ui {

/**
 * 2D vector vertex compatible with the NanoVG / Vector backend interface.
 */
struct VectorVertex2D {
    float x{0.0f};
    float y{0.0f};
    float u{0.0f};
    float v{0.0f};
    uint32_t color{0xFFFFFFFF};
};

struct VectorDrawCall {
    uint32_t textureId{0};
    uint32_t indexOffset{0};
    uint32_t indexCount{0};
    float scissor[4]{0.0f, 0.0f, 0.0f, 0.0f};
    bool scissorEnabled{false};
};

class INanoVGBackend {
public:
    virtual ~INanoVGBackend() = default;
    virtual bool initialize(uint32_t width, uint32_t height) = 0;
    virtual void beginFrame(float width, float height, float pixelRatio) = 0;
    virtual void submitBatch(const VectorVertex2D* vertices, size_t vCount,
                             const uint32_t* indices, size_t iCount,
                             const VectorDrawCall& call) = 0;
    virtual void endFrame() = 0;
};

struct CrtMaterialConfig {
    float scanlineIntensity{0.38f};  // Authentic CRT scanline depth
    uint32_t borderWidthPx{16};      // CRT monitor chassis bezel margin (16px)
    uint32_t cornerRadiusPx{18};     // Phosphor tube rounded corner radius (18px)
    float vignetteStrength{0.28f};   // Analog optical edge & corner falloff
    float spotlightIntensity{1.0f};  // Ambient spotlight sweep
    bool spotlightEnabled{true};     // Enabled for atmospheric studio lighting
    uint32_t topBarHeightPx{56};     // Exact top panel height (56px)
    uint32_t bottomBarHeightPx{48};  // Exact bottom panel height (48px)
    float curvature{0.85f};          // CRT barrel curvature intensity (0.0 to 1.0)
    uint32_t frameWidthPx{20};       // Outer reflective bezel frame curved with CRT (20px)
    uint32_t borderPaddingPx{0};     // Removed black border padding so frame directly hugs curved screen
    bool logoReflectionEnabled{true}; // Subtle brand logo reflection glare
    float reflectionOpacity{0.45f};  // Diffused frame reflection opacity
    float scanlinePitchPx{2.5f};     // Scanline spacing/pitch in pixels
    bool reflectionBlurEnabled{true}; // GPU-accelerated frosted reflection blur
};

/**
 * Google Dawn & WebGPU Bridge Interface.
 * Manages the native WebGPU rendering pipeline and applies the pure WGSL
 * Apocalypse CRT shader pass directly to the window surface.
 */
class DawnBridge : public INanoVGBackend {
public:
    DawnBridge(uint32_t fboWidth = 1024, uint32_t fboHeight = 768);
    virtual ~DawnBridge() override;

    bool initialize(uint32_t width, uint32_t height) override;
    bool initializeNative(void* windowHandle, uint32_t width, uint32_t height);
    void shutdownNative();
    void resize(uint32_t width, uint32_t height);

    void renderCrtScene(const uint32_t* dawPixelBuffer, uint32_t dawWidth, uint32_t dawHeight, float lampTime, float subBassEnergy = 0.0f);
    [[nodiscard]] bool isGpuAccelerated() const noexcept;

    [[nodiscard]] bool isNativeActive() const noexcept { return nativeActive_; }

    void beginFrame(float width, float height, float pixelRatio) override;
    void submitBatch(const VectorVertex2D* vertices, size_t vCount,
                     const uint32_t* indices, size_t iCount,
                     const VectorDrawCall& call) override;
    void endFrame() override;

    [[nodiscard]] uint32_t getFboWidth() const noexcept { return fboWidth_; }
    [[nodiscard]] uint32_t getFboHeight() const noexcept { return fboHeight_; }
    [[nodiscard]] const CrtMaterialConfig& getMaterialConfig() const noexcept { return matConfig_; }
    void setMaterialConfig(const CrtMaterialConfig& config) noexcept { matConfig_ = config; }

    [[nodiscard]] uint64_t getTotalFramesRendered() const noexcept { return framesRendered_; }
    [[nodiscard]] uint64_t getTotalVerticesSubmitted() const noexcept { return totalVertices_; }
    [[nodiscard]] static const char* getShaderSource() noexcept;

    [[nodiscard]] bool isCrtShaderEnabled() const noexcept { return crtShaderEnabled_; }
    void setCrtShaderEnabled(bool enable) noexcept { crtShaderEnabled_ = enable; }

private:
    uint32_t fboWidth_{1024};
    uint32_t fboHeight_{768};
    CrtMaterialConfig matConfig_{};
    bool crtShaderEnabled_{false};

    uint64_t framesRendered_{0};
    uint64_t totalVertices_{0};
    bool initialized_{false};
    bool nativeActive_{false};
    void* nativeWindowHandle_{nullptr};
    std::vector<uint32_t> uploadBuffer_;

    // Opaque internal implementation pointer to keep headers lightweight
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace eatsbits::ui
