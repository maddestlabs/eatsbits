#include "eatsbits/ui/procedural_texture_system.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace eatsbits::ui {

namespace {

// Fast integer hash for procedural noise
inline float hash2D(int x, int y, uint32_t seed) noexcept {
    uint32_t n = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u +
                 seed * 1442695040888963407u;
    n = (n ^ (n >> 13)) * 1274126177u;
    return static_cast<float>(n & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF);
}

// Smooth 2D value noise with cubic Hermite interpolation
float smoothNoise2D(float x, float y, uint32_t seed) noexcept {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    // Smoothstep curve
    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);

    float n00 = hash2D(ix, iy, seed);
    float n10 = hash2D(ix + 1, iy, seed);
    float n01 = hash2D(ix, iy + 1, seed);
    float n11 = hash2D(ix + 1, iy + 1, seed);

    float nx0 = n00 + ux * (n10 - n00);
    float nx1 = n01 + ux * (n11 - n01);
    return nx0 + uy * (nx1 - nx0);
}

// Fractal Brownian Motion (FBM)
float fbm2D(float x, float y, int octaves, float lacunarity, float gain, uint32_t seed) noexcept {
    float sum = 0.0f;
    float amp = 1.0f;
    float maxAmp = 0.0f;
    float cx = x;
    float cy = y;

    for (int i = 0; i < octaves; ++i) {
        sum += smoothNoise2D(cx, cy, seed + static_cast<uint32_t>(i * 101)) * amp;
        maxAmp += amp;
        amp *= gain;
        cx *= lacunarity;
        cy *= lacunarity;
    }
    return maxAmp > 0.0f ? (sum / maxAmp) : 0.5f;
}

// Worley / Cellular Noise (distance to nearest 2 points)
void worley2D(float x, float y, uint32_t seed, float& d1, float& d2) noexcept {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    d1 = 10.0f;
    d2 = 10.0f;

    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            float px = static_cast<float>(i) + hash2D(ix + i, iy + j, seed);
            float py = static_cast<float>(j) + hash2D(ix + i, iy + j, seed + 997);
            float dx = px - fx;
            float dy = py - fy;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < d1) {
                d2 = d1;
                d1 = dist;
            } else if (dist < d2) {
                d2 = dist;
            }
        }
    }
}

} // namespace

ProceduralMaterialType chassisStyleToMaterial(GuiChassisStyle style) noexcept {
    switch (style) {
        case GuiChassisStyle::BrushedSteel:     return ProceduralMaterialType::BrushedSteel;
        case GuiChassisStyle::BrushedAluminum:  return ProceduralMaterialType::BrushedAluminum;
        case GuiChassisStyle::MattePowderCoat:  return ProceduralMaterialType::MattePowderCoat;
        case GuiChassisStyle::Walnut:           return ProceduralMaterialType::WalnutWood;
        case GuiChassisStyle::Rosewood:         return ProceduralMaterialType::Rosewood;
        case GuiChassisStyle::Bakelite:         return ProceduralMaterialType::Bakelite;
        case GuiChassisStyle::CrinklePaint:     return ProceduralMaterialType::CrinklePaint;
        case GuiChassisStyle::PcbGreen:         return ProceduralMaterialType::PcbFiberglass;
        case GuiChassisStyle::Carbon:           return ProceduralMaterialType::CarbonFiber;
        case GuiChassisStyle::DarkChassis:
        case GuiChassisStyle::Silver:
        case GuiChassisStyle::Snes:
        case GuiChassisStyle::Grunge:
        case GuiChassisStyle::MinimalWhite:
        default:                                return ProceduralMaterialType::CastIronGrungy;
    }
}

ProceduralTextureSystem& ProceduralTextureSystem::instance() {
    static ProceduralTextureSystem s_sys;
    return s_sys;
}

ProceduralTextureSystem::ProceduralTextureSystem() {
    initMasterMaps();
}

void ProceduralTextureSystem::clearCache() {
    tintedCache_.clear();
}

void ProceduralTextureSystem::initMasterMaps() {
    // Pre-allocate button tactile noise
    buttonNoiseMaster_.resize(kNoiseDim * kNoiseDim);
    for (int ny = 0; ny < kNoiseDim; ++ny) {
        for (int nx = 0; nx < kNoiseDim; ++nx) {
            float n1 = hash2D(nx, ny, 401) - 0.5f;
            float n2 = hash2D((nx / 2) % (kNoiseDim / 2), (ny / 2) % (kNoiseDim / 2), 503) - 0.5f;
            buttonNoiseMaster_[ny * kNoiseDim + nx] = 0.5f + (n1 * 0.65f + n2 * 0.35f) * 0.40f;
        }
    }

    // Pre-initialize standard procedural materials
    static const ProceduralMaterialType mats[] = {
        ProceduralMaterialType::CastIronGrungy,
        ProceduralMaterialType::BrushedSteel,
        ProceduralMaterialType::BrushedAluminum,
        ProceduralMaterialType::MattePowderCoat,
        ProceduralMaterialType::WalnutWood,
        ProceduralMaterialType::Rosewood,
        ProceduralMaterialType::Bakelite,
        ProceduralMaterialType::CrinklePaint,
        ProceduralMaterialType::PcbFiberglass,
        ProceduralMaterialType::CarbonFiber
    };

    for (auto m : mats) {
        MaterialMasterBuffer buf;
        generateMaterialMaster(m, buf);
        masterBuffers_[static_cast<int>(m)] = std::move(buf);
    }
}

void ProceduralTextureSystem::generateMaterialMaster(ProceduralMaterialType mat, MaterialMasterBuffer& buf) {
    buf.master.resize(kMasterWidth * kMasterHeight);
    buf.modulator.resize(kModWidth * kModHeight);

    // 1. Generate Secondary Low-Frequency Prime Modulator (719 x 97)
    // Eliminates repetitive periodic patterning across wide displays
    for (int my = 0; my < kModHeight; ++my) {
        for (int mx = 0; mx < kModWidth; ++mx) {
            float fx = static_cast<float>(mx);
            float fy = static_cast<float>(my);
            float lowFreq = fbm2D(fx * 0.008f, fy * 0.02f, 3, 2.0f, 0.5f, 7771);
            buf.modulator[my * kModWidth + mx] = (lowFreq - 0.5f) * 0.08f; // +/- 4% gentle luma drift
        }
    }

    // 2. Generate Material-Specific Master Surface Map (1024 x 128)
    for (int y = 0; y < kMasterHeight; ++y) {
        for (int x = 0; x < kMasterWidth; ++x) {
            float fx = static_cast<float>(x);
            float fy = static_cast<float>(y);
            ProceduralSample s{};

            switch (mat) {
                case ProceduralMaterialType::BrushedSteel:
                case ProceduralMaterialType::BrushedAluminum: {
                    // Stretched anisotropic horizontal brush streaks
                    bool isAluminum = (mat == ProceduralMaterialType::BrushedAluminum);
                    float yFreq1 = isAluminum ? 4.5f : 3.0f;
                    float yFreq2 = isAluminum ? 9.0f : 6.0f;
                    float n1 = hash2D(static_cast<int>(fx * 0.02f), static_cast<int>(fy * yFreq1), 101) - 0.5f;
                    float n2 = hash2D(static_cast<int>(fx * 0.05f), static_cast<int>(fy * yFreq2), 203) - 0.5f;
                    float brush = n1 * 0.65f + n2 * 0.35f;

                    // Specular highlight band across the plate
                    float sheen = std::pow(std::cos((fx / static_cast<float>(kMasterWidth)) * 3.14159f * 2.0f), 4.0f) * 0.12f;
                    s.height = std::clamp(0.5f + brush * (isAluminum ? 0.22f : 0.32f) + sheen, 0.0f, 1.0f);
                    s.detail = std::clamp(brush * 0.5f + 0.5f, 0.0f, 1.0f);

                    // Sparse fine hairline micro-scratches
                    float randScratch = hash2D(x, y, 991);
                    if (randScratch > 0.985f) {
                        s.fleck = (randScratch - 0.985f) / 0.015f * 0.40f;
                    }
                    break;
                }

                case ProceduralMaterialType::MattePowderCoat: {
                    // Isotropic tactile micro-stipple (diffuse non-glare)
                    float s1 = hash2D(x, y, 101) - 0.5f;
                    float s2 = hash2D((x / 2) % (kMasterWidth / 2), (y / 2) % (kMasterHeight / 2), 203) - 0.5f;
                    float s3 = hash2D((x / 4) % (kMasterWidth / 4), (y / 4) % (kMasterHeight / 4), 307) - 0.5f;
                    float stipple = 0.5f + (s1 * 0.55f + s2 * 0.30f + s3 * 0.15f) * 0.18f;
                    s.height = std::clamp(stipple, 0.0f, 1.0f);
                    s.detail = 0.0f;
                    break;
                }

                case ProceduralMaterialType::WalnutWood:
                case ProceduralMaterialType::Rosewood: {
                    bool isRosewood = (mat == ProceduralMaterialType::Rosewood);
                    // Organic fiber coordinate warping
                    float warpX = smoothNoise2D(fx * 0.006f, fy * 0.035f, 881) * 24.0f;
                    float warpY = smoothNoise2D(fx * 0.012f, fy * 0.015f, 991) * 12.0f;

                    // Concentric growth rings
                    float ringFreq = isRosewood ? 0.12f : 0.065f;
                    float ringVal = std::sin((fx + warpX) * ringFreq + (fy + warpY) * 0.03f);
                    s.height = std::clamp(ringVal * 0.5f + 0.5f, 0.0f, 1.0f);

                    // Longitudinal pore vessels (dark elongated wood pores)
                    float poreCut = hash2D(static_cast<int>(fx * 0.75f), static_cast<int>(fy * 0.04f), 773);
                    if (poreCut > 0.92f) {
                        s.detail = (poreCut - 0.92f) / 0.08f; // Pore intensity
                        s.crevice = s.detail * 0.6f;
                    }
                    break;
                }

                case ProceduralMaterialType::Bakelite: {
                    // Vintage 1930s-50s phenolic resin liquid marbling swirls
                    float qx = fbm2D(fx * 0.008f, fy * 0.02f, 3, 2.0f, 0.5f, 411);
                    float qy = fbm2D(fx * 0.012f, fy * 0.016f, 3, 2.0f, 0.5f, 523);
                    float flowSwirl = fbm2D((fx + qx * 45.0f) * 0.007f, (fy + qy * 25.0f) * 0.018f, 4, 2.0f, 0.5f, 631);

                    s.height = std::clamp(flowSwirl, 0.0f, 1.0f);
                    // Polished resin specular luster
                    float luster = std::pow(std::sin(flowSwirl * 3.14159f * 1.5f) * 0.5f + 0.5f, 3.0f);
                    s.detail = luster;
                    break;
                }

                case ProceduralMaterialType::CrinklePaint: {
                    // Heavy wrinkle / crinkle finish (Worley cellular ridges)
                    float d1 = 0.0f, d2 = 0.0f;
                    worley2D(fx * 0.14f, fy * 0.18f, 311, d1, d2);
                    float ridge = 1.0f - std::clamp((d2 - d1) * 3.8f, 0.0f, 1.0f);
                    // Valleys are dark crevices, ridges catch micro-specular
                    s.height = std::clamp(ridge, 0.0f, 1.0f);
                    s.crevice = std::clamp((1.0f - ridge) * 0.8f, 0.0f, 1.0f);
                    s.detail = std::pow(ridge, 2.5f);
                    break;
                }

                case ProceduralMaterialType::PcbFiberglass: {
                    // Cross-hatch woven FR4 substrate
                    float weaveX = std::sin(fx * 0.45f) * 0.5f + 0.5f;
                    float weaveY = std::sin(fy * 0.45f) * 0.5f + 0.5f;
                    s.height = std::clamp(weaveX * 0.5f + weaveY * 0.5f, 0.0f, 1.0f);
                    // Copper trace flecks
                    float trace = hash2D(static_cast<int>(fx * 0.1f), static_cast<int>(fy * 0.2f), 819);
                    if (trace > 0.94f) s.fleck = 0.6f;
                    break;
                }

                case ProceduralMaterialType::CarbonFiber: {
                    // 2x2 Twill carbon weave
                    int ix = static_cast<int>(fx * 0.35f);
                    int iy = static_cast<int>(fy * 0.35f);
                    bool diag = ((ix + iy) & 2) != 0;
                    float weave = diag ? 0.68f : 0.32f;
                    s.height = weave;
                    s.detail = diag ? 1.0f : 0.0f;
                    break;
                }

                case ProceduralMaterialType::CastIronGrungy:
                default: {
                    // Multi-octave stipple + micro-pitting + sparse brass flecks
                    float h1 = hash2D(x, y, 101) - 0.5f;
                    float h2 = hash2D((x / 2) % (kMasterWidth / 2), y / 2, 203) - 0.5f;
                    float h3 = hash2D((x / 6) % (kMasterWidth / 6), y / 6, 307) - 0.5f;
                    float xWarp = std::sin(fx * 0.045f) * 0.18f + std::cos(fx * 0.09f + fy * 0.12f) * 0.12f;
                    float stipple = 0.5f + (h1 * 0.52f + h2 * 0.32f + h3 * 0.16f + xWarp * 0.14f) * 0.45f;

                    float fleck = 0.0f;
                    float randSubtle = hash2D(x, y, 888);
                    if (randSubtle > 0.965f) {
                        fleck = (randSubtle - 0.965f) / 0.035f * 0.22f;
                    }
                    float randFleck = hash2D(x, y, 777);
                    if (randFleck > 0.988f) {
                        fleck = std::max(fleck, (randFleck - 0.988f) / 0.012f * 0.38f);
                    }

                    s.height = std::clamp(stipple, 0.0f, 1.0f);
                    s.fleck = fleck;
                    break;
                }
            }

            buf.master[y * kMasterWidth + x] = s;
        }
    }
}

Color ProceduralTextureSystem::blendMaterialColor(ProceduralMaterialType mat,
                                                  const ProceduralSample& sample,
                                                  const Color& baseColor,
                                                  const Color& tintColor,
                                                  float wearIntensity) noexcept {
    switch (mat) {
        case ProceduralMaterialType::BrushedSteel:
        case ProceduralMaterialType::BrushedAluminum: {
            // Anodized metal: Soft-light diffuse modulation with specular sheen protection
            float luma = sample.height;
            // Soft-light formula preserving metal contrast
            Color dyed = (luma < 0.5f)
                ? (tintColor * (2.0f * luma + (luma * luma) * (1.0f - 2.0f * luma)))
                : (tintColor * (1.0f - (1.0f - luma) * (1.0f - (2.0f * luma - 1.0f))));

            // Specular protection: anisotropic highlights stay bright white/silver
            float spec = std::clamp((sample.height - 0.65f) / 0.35f, 0.0f, 1.0f);
            Color res = Color::lerp(dyed, Color(0.96f, 0.97f, 1.0f), spec * 0.45f);

            // Metallic flecks retain bright glint
            if (sample.fleck > 0.01f) {
                res = Color::lerp(res, Color(1.0f, 0.94f, 0.85f), sample.fleck * (0.6f + wearIntensity * 0.4f));
            }
            return res;
        }

        case ProceduralMaterialType::MattePowderCoat: {
            // Powder-coated plastic: Even diffuse absorption with micro-stipple occlusion
            float delta = (sample.height - 0.5f) * 0.16f;
            return (delta >= 0.0f) ? tintColor.lighten(delta) : tintColor.darken(-delta * 1.25f);
        }

        case ProceduralMaterialType::WalnutWood:
        case ProceduralMaterialType::Rosewood: {
            // Wood stain: Two-point harmonic model (earlywood tint vs deep latewood rings)
            Color earlywood = tintColor;
            Color latewood = tintColor.darken(0.38f).saturate(0.18f);
            Color res = Color::lerp(latewood, earlywood, sample.height);

            // Longitudinal pore cuts remain dark
            if (sample.detail > 0.1f) {
                res = res.darken(sample.detail * 0.40f);
            }
            return res;
        }

        case ProceduralMaterialType::Bakelite: {
            // Phenolic resin: Marbled ribbons between rich amber/oxblood resin and custom tint
            Color resinAmber(0.24f, 0.08f, 0.05f); // Classic dark mahogany phenol
            Color swirl = Color::lerp(resinAmber, tintColor, sample.height * 0.85f);
            // High-polish specular sheen along swirl ridges
            Color res = swirl.lighten(sample.detail * 0.30f);
            return res;
        }

        case ProceduralMaterialType::CrinklePaint: {
            // Industrial wrinkle finish: Ridges catch highlight, valleys collect shadow
            Color ridge = tintColor.lighten(0.14f);
            Color valley = tintColor.darken(0.40f);
            Color res = Color::lerp(valley, ridge, sample.height);

            if (sample.crevice > 0.01f) {
                res = Color::lerp(res, Color(0.04f, 0.04f, 0.05f), sample.crevice * 0.85f);
            }
            return res;
        }

        case ProceduralMaterialType::PcbFiberglass: {
            Color pcbGreen = tintColor;
            float stippleDelta = (sample.height - 0.5f) * 0.22f;
            Color res = (stippleDelta >= 0.0f) ? pcbGreen.lighten(stippleDelta) : pcbGreen.darken(-stippleDelta * 1.2f);
            if (sample.fleck > 0.01f) {
                res = Color::lerp(res, Color(0.85f, 0.70f, 0.25f), sample.fleck * 0.8f); // Copper trace
            }
            return res;
        }

        case ProceduralMaterialType::CarbonFiber: {
            Color carbon = tintColor;
            float sheen = (sample.detail > 0.5f) ? 0.15f : -0.15f;
            return (sheen >= 0.0f) ? carbon.lighten(sheen) : carbon.darken(-sheen);
        }

        case ProceduralMaterialType::CastIronGrungy:
        default: {
            // Legacy Grungy Cast Iron modulation
            float stippleDelta = (sample.height - 0.5f) * 0.22f;
            Color res = (stippleDelta >= 0.0f)
                ? baseColor.lighten(stippleDelta)
                : baseColor.darken(-stippleDelta * 1.35f);

            if (sample.fleck > 0.01f) {
                Color brassFleck = Color::lerp(baseColor.lighten(0.10f), Color(1.0f, 0.88f, 0.60f), 0.35f);
                res = Color::lerp(res, brassFleck, sample.fleck * (0.8f + wearIntensity * 0.4f));
            }
            return res;
        }
    }
}

float ProceduralTextureSystem::evaluateWorldAnomaly(float worldX, float worldY, uint32_t seed) noexcept {
    int cellX = static_cast<int>(std::floor(worldX / 160.0f));
    int cellY = static_cast<int>(std::floor(worldY / 32.0f));
    float h = hash2D(cellX, cellY, seed);
    if (h > 0.88f) {
        // Compute distance from random diagonal scratch line within cell
        float localX = worldX - static_cast<float>(cellX * 160);
        float localY = worldY - static_cast<float>(cellY * 32);
        float angle = hash2D(cellX, cellY, seed + 101) * 3.14159f;
        float scratchLen = 15.0f + hash2D(cellX, cellY, seed + 202) * 35.0f;

        float distLine = std::abs((localX - 80.0f) * std::sin(angle) - (localY - 16.0f) * std::cos(angle));
        float alongLine = std::abs((localX - 80.0f) * std::cos(angle) + (localY - 16.0f) * std::sin(angle));

        if (alongLine < scratchLen && distLine < 1.2f) {
            return (1.2f - distLine) / 1.2f;
        }
    }
    return 0.0f;
}

const ProceduralTextureSystem::CachedTintedTexture&
ProceduralTextureSystem::getOrCreateTinted(ProceduralMaterialType mat,
                                         const ThemeTokens& theme,
                                         const std::optional<Color>& customTint,
                                         float wearIntensity) {
    uint32_t tintKey = customTint.has_value() ? customTint->toRgba8() : 0u;
    uint32_t themeKey = theme.panelHeaderGradientTop.toRgba8() ^ theme.panelHeaderGradientBottom.toRgba8();

    std::ostringstream ss;
    ss << static_cast<int>(mat) << "_" << theme.name << "_" << tintKey << "_" << static_cast<int>(wearIntensity * 100.0f);
    std::string key = ss.str();

    auto it = tintedCache_.find(key);
    if (it != tintedCache_.end()) {
        return it->second;
    }

    // Bake tinted texture from master map
    auto mbIt = masterBuffers_.find(static_cast<int>(mat));
    if (mbIt == masterBuffers_.end()) {
        static CachedTintedTexture empty{};
        return empty;
    }

    const auto& master = mbIt->second.master;
    const auto& mod = mbIt->second.modulator;

    CachedTintedTexture item{};
    item.width = kMasterWidth;
    item.height = kMasterHeight;
    item.rgba.resize(static_cast<size_t>(kMasterWidth * kMasterHeight * 4));
    item.tintKey = tintKey;
    item.themeKey = themeKey;
    item.wear = wearIntensity;

    // Determine base tint color
    Color effectiveTint = customTint.has_value() ? *customTint : Color(0.18f, 0.19f, 0.22f);
    if (!customTint.has_value()) {
        switch (mat) {
            case ProceduralMaterialType::BrushedSteel:    effectiveTint = Color(0.28f, 0.30f, 0.34f); break;
            case ProceduralMaterialType::BrushedAluminum: effectiveTint = Color(0.78f, 0.80f, 0.84f); break;
            case ProceduralMaterialType::MattePowderCoat: effectiveTint = Color(0.14f, 0.15f, 0.18f); break;
            case ProceduralMaterialType::WalnutWood:      effectiveTint = Color(0.32f, 0.18f, 0.10f); break;
            case ProceduralMaterialType::Rosewood:        effectiveTint = Color(0.24f, 0.08f, 0.06f); break;
            case ProceduralMaterialType::Bakelite:        effectiveTint = Color(0.34f, 0.12f, 0.06f); break;
            case ProceduralMaterialType::CrinklePaint:    effectiveTint = Color(0.16f, 0.16f, 0.18f); break;
            case ProceduralMaterialType::PcbFiberglass:   effectiveTint = Color(0.06f, 0.24f, 0.12f); break;
            case ProceduralMaterialType::CarbonFiber:     effectiveTint = Color(0.08f, 0.09f, 0.11f); break;
            default:                                      effectiveTint = theme.panelHeaderGradientTop; break;
        }
    }

    for (int y = 0; y < kMasterHeight; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(kMasterHeight - 1);
        Color base = Color::lerp(theme.panelHeaderGradientTop, theme.panelHeaderGradientBottom, t);

        for (int x = 0; x < kMasterWidth; ++x) {
            const auto& s = master[y * kMasterWidth + x];

            // Blend with dual-frequency modulator
            float modDrift = mod[(y % kModHeight) * kModWidth + (x % kModWidth)];
            ProceduralSample blendedSample = s;
            blendedSample.height = std::clamp(blendedSample.height + modDrift, 0.0f, 1.0f);

            Color c = blendMaterialColor(mat, blendedSample, base, effectiveTint, wearIntensity);

            int idx = (y * kMasterWidth + x) * 4;
            item.rgba[idx + 0] = static_cast<uint8_t>(std::clamp(c.r * 255.0f, 0.0f, 255.0f));
            item.rgba[idx + 1] = static_cast<uint8_t>(std::clamp(c.g * 255.0f, 0.0f, 255.0f));
            item.rgba[idx + 2] = static_cast<uint8_t>(std::clamp(c.b * 255.0f, 0.0f, 255.0f));
            item.rgba[idx + 3] = 255;
        }
    }

    auto insertRes = tintedCache_.emplace(key, std::move(item));
    return insertRes.first->second;
}

void ProceduralTextureSystem::drawChassis(BatchRenderer2D* r,
                                        float x, float y, float w, float h,
                                        GuiChassisStyle style,
                                        const ThemeTokens& theme,
                                        const std::optional<Color>& customTint,
                                        float wearIntensity,
                                        float cornerRadius,
                                        bool isTopPanel) {
    if (!r || w <= 0.0f || h <= 0.0f) return;

    ProceduralMaterialType mat = chassisStyleToMaterial(style);
    const auto& tex = getOrCreateTinted(mat, theme, customTint, wearIntensity);
    if (tex.rgba.empty()) return;

    // 1. Dual-Frequency Tiling: Tile horizontally across [x, x + w]
    for (float curX = x; curX < x + w; curX += static_cast<float>(kMasterWidth)) {
        float chunkW = std::min(static_cast<float>(kMasterWidth), x + w - curX);
        r->drawRgbaBitmap(curX, y, chunkW, h, tex.rgba.data(), kMasterWidth, kMasterHeight, 1.0f);
    }

    // 2. World-Space Anomaly Pass: Sparse deterministic micro-scratches placed at absolute window coordinates
    if (wearIntensity > 0.05f) {
        float startCellX = std::floor(x / 160.0f) * 160.0f;
        for (float cx = startCellX; cx < x + w; cx += 160.0f) {
            float scratch = evaluateWorldAnomaly(cx + 80.0f, y + h * 0.5f, 911);
            if (scratch > 0.01f) {
                float sx = std::clamp(cx + 40.0f, x + 2.0f, x + w - 40.0f);
                float sy = y + h * 0.35f;
                drawLine(*r, sx, sy, sx + 28.0f, sy + 6.0f,
                         1.0f, 1.0f, 1.0f, scratch * 0.06f * wearIntensity, 1.0f);
            }
        }
    }

    // 3. Machined Crevice Shadow on rim
    if (isTopPanel) {
        drawLine(*r, x, y + h - 1.0f, x + w, y + h - 1.0f, 0.0f, 0.0f, 0.0f, 0.45f, 1.0f);
    } else {
        drawLine(*r, x, y, x + w, y, 0.0f, 0.0f, 0.0f, 0.45f, 1.0f);
    }
}

void ProceduralTextureSystem::drawFaceplateBackground(BatchRenderer2D* r,
                                                     const Rect2D& bounds,
                                                     GuiChassisStyle style,
                                                     const ThemeTokens& theme,
                                                     const std::optional<Color>& customTint,
                                                     float wearIntensity,
                                                     float cornerRadius) {
    if (!r || bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    // 1. Draw base rounded rect backing
    Color baseCol = customTint.has_value() ? *customTint : theme.panelBackground;
    drawRoundedRect(*r, bounds.x, bounds.y, bounds.w, bounds.h, cornerRadius,
                    baseCol.r * 0.85f, baseCol.g * 0.85f, baseCol.b * 0.85f, 1.0f);

    // 2. Overlay procedural textured material
    drawChassis(r, bounds.x, bounds.y, bounds.w, bounds.h, style, theme,
                customTint, wearIntensity, cornerRadius, false);

    // 3. Chamfered 3D Bevel Outline (Inner highlight & outer rim)
    drawRoundedRectOutline(*r, bounds.x, bounds.y, bounds.w, bounds.h, cornerRadius,
                           baseCol.lighten(0.20f).r, baseCol.lighten(0.20f).g, baseCol.lighten(0.20f).b, 0.65f, 1.2f);
    drawRoundedRectOutline(*r, bounds.x + 1.0f, bounds.y + 1.0f, bounds.w - 2.0f, bounds.h - 2.0f,
                           std::max(0.0f, cornerRadius - 1.0f),
                           0.0f, 0.0f, 0.0f, 0.35f, 1.0f);
}

void ProceduralTextureSystem::drawButtonNoise(BatchRenderer2D* r,
                                             float x, float y, float w, float h,
                                             bool /*isTop*/, float opacity, float cornerRadius,
                                             bool roundTL, bool roundTR, bool roundBL, bool roundBR,
                                             bool pressed, float pressShiftY) {
    if (!r || w <= 0.0f || h <= 0.0f || opacity <= 0.001f) return;

    int iW = static_cast<int>(std::round(w));
    int iH = static_cast<int>(std::round(h));
    if (iW <= 0 || iH <= 0) return;

    static std::vector<uint8_t> s_buttonNoiseBuf;
    s_buttonNoiseBuf.resize(static_cast<size_t>(iW * iH * 4));

    if (buttonNoiseMaster_.empty()) return;

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

            int srcX = (static_cast<int>(x) + px) % kNoiseDim;
            if (srcX < 0) srcX += kNoiseDim;
            int srcY = (static_cast<int>(y) + py + sampleShiftY) % kNoiseDim;
            if (srcY < 0) srcY += kNoiseDim;

            float val = buttonNoiseMaster_[srcY * kNoiseDim + srcX];
            float diff = val - 0.50f;

            uint8_t rCol = 0, gCol = 0, bCol = 0, aCol = 0;
            if (diff < -0.022f) {
                rCol = 0; gCol = 0; bCol = 0;
                float pit = (-diff - 0.022f) / 0.40f;
                aCol = static_cast<uint8_t>(std::clamp(pit * 130.0f, 0.0f, 150.0f));
            } else if (diff > 0.022f) {
                rCol = 245; gCol = 245; bCol = 250;
                float peak = (diff - 0.022f) / 0.40f;
                aCol = static_cast<uint8_t>(std::clamp(peak * 110.0f, 0.0f, 130.0f));
            }

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

} // namespace eatsbits::ui
