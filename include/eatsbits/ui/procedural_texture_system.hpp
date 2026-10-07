#pragma once

#include "eatsbits/ui/geometry.hpp"
#include "eatsbits/ui/theme.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/gui_panel_def.hpp"
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace eatsbits::ui {

/**
 * Procedural Material Types for Hardware UI & Skeuomorphic Panels
 */
enum class ProceduralMaterialType {
    CastIronGrungy,     // Default legacy grungy cast iron / metal plate
    BrushedSteel,       // Heavy machined directional brushed steel
    BrushedAluminum,    // Anodized / satin aluminum with fine directional streaks & specular sheen
    BrushedMetal = BrushedAluminum, // Unified brushed metal material
    MattePowderCoat,    // Tactile fine isotropic micro-stipple, diffuse non-glare finish
    WalnutWood,         // Rich walnut growth rings, grain turbulence, and longitudinal pore cuts
    Rosewood,           // Deep exotic rosewood grain with dense ring fibers
    Bakelite,           // Vintage 1930s-50s marbled phenolic resin with liquid swirling and polish gloss
    CrinklePaint,       // Heavy vintage industrial wrinkle/crinkle paint (Worley ridge network)
    PcbFiberglass,      // Woven glass fiber PCB substrate
    CarbonFiber         // Woven carbon twill
};

/**
 * Map GuiChassisStyle enum to underlying ProceduralMaterialType
 */
ProceduralMaterialType chassisStyleToMaterial(GuiChassisStyle style) noexcept;

/**
 * Procedural surface sample storing multi-channel normalized surface attributes
 */
struct ProceduralSample {
    float height{0.5f};     // 0.0 .. 1.0 (surface height / stipple / primary texture)
    float fleck{0.0f};      // 0.0 .. 1.0 (specular glints / brass / metal flecks)
    float detail{0.0f};     // 0.0 .. 1.0 (anisotropic brush / grain fibers / wrinkle ridges)
    float crevice{0.0f};    // 0.0 .. 1.0 (dirt / ambient occlusion / valley shadow)
};

/**
 * High-performance, memory-efficient procedural texture engine for DAW panels,
 * faceplates, and tactile hardware buttons.
 *
 * Key Architecture Highlights:
 * 1. Dual-Frequency Aperiodic Tiling:
 *    Combines a high-frequency master tile with a prime-period low-frequency
 *    modulator (1024 x 128 and 719 x 97) to eliminate visual periodicity across
 *    ultra-wide displays.
 * 2. World-Space Anomaly Pass:
 *    Injects sparse, deterministic micro-scratches, hairline abrasions, and edge
 *    patina at absolute window coordinates so identical synthesizer modules have
 *    unique individual patina.
 * 3. Physically Aware Color Blending:
 *    Applies material-aware formulas (anodized specular protection, two-point
 *    harmonic wood stains, crinkle valley darkening, bakelite swirl marbling)
 *    when users select a custom chassis tint.
 * 4. Zero-Overhead Caching:
 *    Baked once on theme or tint change, zero CPU generation overhead per frame.
 */
class ProceduralTextureSystem {
public:
    static constexpr int kMasterWidth = 1024;
    static constexpr int kMasterHeight = 128;
    static constexpr int kModWidth = 719;
    static constexpr int kModHeight = 97;
    static constexpr int kNoiseDim = 64;

    static ProceduralTextureSystem& instance();

    ProceduralTextureSystem();
    ~ProceduralTextureSystem() = default;

    /**
     * Draw textured chassis plate for top/bottom DAW bars or background panels.
     */
    void drawChassis(BatchRenderer2D* r,
                     float x, float y, float w, float h,
                     GuiChassisStyle style,
                     const ThemeTokens& theme,
                     const std::optional<Color>& customTint = std::nullopt,
                     float wearIntensity = 0.20f,
                     float cornerRadius = 0.0f,
                     bool isTopPanel = false,
                     float rotation = 0.0f);

    /**
     * Draw faceplate background with rounded corners and skeuomorphic bevels.
     */
    void drawFaceplateBackground(BatchRenderer2D* r,
                                 const Rect2D& bounds,
                                 GuiChassisStyle style,
                                 const ThemeTokens& theme,
                                 const std::optional<Color>& customTint = std::nullopt,
                                 float wearIntensity = 0.20f,
                                 float cornerRadius = 8.0f,
                                 float rotation = 0.0f);

    /**
     * Tactile micro-noise for hardware buttons and push switches.
     */
    void drawButtonNoise(BatchRenderer2D* r,
                         float x, float y, float w, float h,
                         bool isTop = true, float opacity = 0.28f,
                         float cornerRadius = 0.0f,
                         bool roundTL = true, bool roundTR = true,
                         bool roundBL = true, bool roundBR = true,
                         bool pressed = false, float pressShiftY = 3.0f);

    /**
     * Physically aware color blender (exposed for testing and external shader pipelines).
     */
    static Color blendMaterialColor(ProceduralMaterialType mat,
                                    const ProceduralSample& sample,
                                    const Color& baseColor,
                                    const Color& tintColor,
                                    float wearIntensity) noexcept;

    /**
     * Evaluate non-repeating world-space anomaly (hairline scratch or speckle).
     */
    static float evaluateWorldAnomaly(float worldX, float worldY, uint32_t seed) noexcept;

    struct CachedTintedTexture {
        int width{0};
        int height{0};
        std::vector<uint8_t> rgba;
        uint32_t tintKey{0};
        uint32_t themeKey{0};
        float wear{0.0f};
        float rotation{0.0f};
    };

    /**
     * Get or bake cached tinted RGBA texture with rotation and wear.
     */
    const CachedTintedTexture& getOrCreateTinted(ProceduralMaterialType mat,
                                                const ThemeTokens& theme,
                                                const std::optional<Color>& customTint,
                                                float wearIntensity,
                                                float rotation = 0.0f);

    /**
     * Clear cached RGBA textures (e.g. on full theme reset).
     */
    void clearCache();

private:
    struct MaterialMasterBuffer {
        std::vector<ProceduralSample> master; // kMasterWidth * kMasterHeight
        std::vector<float> modulator;         // kModWidth * kModHeight
    };

    void initMasterMaps();
    void generateMaterialMaster(ProceduralMaterialType mat, MaterialMasterBuffer& buf);

    std::unordered_map<int, MaterialMasterBuffer> masterBuffers_;
    std::unordered_map<std::string, CachedTintedTexture> tintedCache_;

    // Button noise master
    std::vector<float> buttonNoiseMaster_;
};

} // namespace eatsbits::ui
