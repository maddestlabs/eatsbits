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
    float scanlineIntensity{0.38f};  // Authentic CRT scanline depth (0.0 = removed, 1.0 = deep)
    uint32_t borderWidthPx{16};      // CRT monitor chassis bezel margin (16px)
    uint32_t cornerRadiusPx{18};     // Phosphor tube rounded corner radius (18px)
    float vignetteStrength{1.0f};    // Analog optical edge & corner falloff multiplier (0.0 to 2.0)
    float spotlightIntensity{1.0f};  // Ambient spotlight sweep intensity (0.0 to 2.0; 0.0 = flat uniform light)
    float spotlightSize{1.35f};      // Beam radius / spread (0.5 to 2.5)
    bool spotlightEnabled{true};     // Enabled for atmospheric studio lighting
    uint32_t topBarHeightPx{56};     // Exact top panel height (56px)
    uint32_t bottomBarHeightPx{48};  // Exact bottom panel height (48px)
    float curvature{0.85f};          // CRT barrel curvature intensity (0.0 = flat screen, 1.5 = deep bulb)
    uint32_t frameWidthPx{20};       // Outer reflective bezel frame curved with CRT (20px)
    uint32_t borderPaddingPx{0};     // Removed black border padding so frame directly hugs curved screen
    bool logoReflectionEnabled{true}; // Subtle brand logo reflection glare
    float reflectionOpacity{0.52f};  // Bezel frame reflection level (0.0 = matte, 1.0 = mirror glaze)
    float scanlinePitchPx{2.5f};     // Scanline spacing/pitch in pixels
    bool reflectionBlurEnabled{true}; // GPU-accelerated frosted reflection blur
    float panelSoftness{0.75f};      // Sub-pixel blur radius for physical panel blending (0.0 to 1.5px)
    float panelSaturation{0.70f};    // Panel color saturation (0.0 to 1.0; 0.70 = realistic hardware desaturation)
    float panelBlackLift{0.025f};    // Soften harsh dark lines by lifting the black floor (0.0 to 0.08)
    float crtReflectionLevel{1.0f};  // Phosphor tube room reflection opacity (0.0 = removed, 1.0 = full reflection)
    float hsyncDistortion{1.0f};     // Horizontal sync wave distortion multiplier (0.0 = rock solid / zero distortion)
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
    bool initializeWeb(void* device, void* surface, uint32_t width, uint32_t height);
    void shutdownNative();
    void resize(uint32_t width, uint32_t height);

    void renderCrtScene(const uint32_t* dawPixelBuffer, uint32_t dawWidth, uint32_t dawHeight, float lampTime, float subBassEnergy = 0.0f, float renderScale = 1.0f);
    [[nodiscard]] bool isGpuAccelerated() const noexcept;

    [[nodiscard]] bool isNativeActive() const noexcept { return nativeActive_; }
    [[nodiscard]] void* getDawTextureView() const noexcept;

    void beginFrame(float width, float height, float pixelRatio) override;
    void submitBatch(const VectorVertex2D* vertices, size_t vCount,
                     const uint32_t* indices, size_t iCount,
                     const VectorDrawCall& call) override;
    void endFrame() override;

    [[nodiscard]] uint32_t getFboWidth() const noexcept { return fboWidth_; }
    [[nodiscard]] uint32_t getFboHeight() const noexcept { return fboHeight_; }
    [[nodiscard]] const CrtMaterialConfig& getMaterialConfig() const noexcept { return matConfig_; }
    void setMaterialConfig(const CrtMaterialConfig& config) noexcept { matConfig_ = config; }

    [[nodiscard]] float getRenderScale() const noexcept { return renderScale_; }
    void setRenderScale(float scale) noexcept { renderScale_ = (scale > 0.001f) ? scale : 1.0f; }

    [[nodiscard]] float getRumbleOffsetX() const noexcept { return currentRumbleOffsetX_; }
    [[nodiscard]] float getRumbleOffsetY() const noexcept { return currentRumbleOffsetY_; }
    [[nodiscard]] float getHWaveStrength() const noexcept { return currentHWaveStrength_; }
    [[nodiscard]] float getLampTime() const noexcept { return currentLampTime_; }

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
    float renderScale_{1.0f};

    float currentRumbleOffsetX_{0.0f};
    float currentRumbleOffsetY_{0.0f};
    float currentHWaveStrength_{0.0f};
    float currentLampTime_{0.0f};

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
