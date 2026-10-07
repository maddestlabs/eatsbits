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

// Toroidal / periodic smooth noise wrapping seamlessly across periodW and periodH
float periodicNoise2D(float x, float y, float periodW, float periodH, uint32_t seed) noexcept {
    int pW = std::max(1, static_cast<int>(std::round(periodW)));
    int pH = std::max(1, static_cast<int>(std::round(periodH)));

    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    int ix0 = ((ix % pW) + pW) % pW;
    int iy0 = ((iy % pH) + pH) % pH;
    int ix1 = (ix0 + 1) % pW;
    int iy1 = (iy0 + 1) % pH;

    float ux = fx * fx * (3.0f - 2.0f * fx);
    float uy = fy * fy * (3.0f - 2.0f * fy);

    float n00 = hash2D(ix0, iy0, seed);
    float n10 = hash2D(ix1, iy0, seed);
    float n01 = hash2D(ix0, iy1, seed);
    float n11 = hash2D(ix1, iy1, seed);

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

// Periodic FBM with seamless integer-multiple frequency periods
float periodicFbm2D(float x, float y, float periodW, float periodH,
                    int octaves, float lacunarity, float gain, uint32_t seed) noexcept {
    float sum = 0.0f;
    float amp = 1.0f;
    float maxAmp = 0.0f;
    float cx = x;
    float cy = y;
    float curW = periodW;
    float curH = periodH;

    for (int i = 0; i < octaves; ++i) {
        sum += periodicNoise2D(cx, cy, curW, curH, seed + static_cast<uint32_t>(i * 101)) * amp;
        maxAmp += amp;
        amp *= gain;
        cx *= lacunarity;
        cy *= lacunarity;
        curW *= lacunarity;
        curH *= lacunarity;
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

// Periodic Worley / Cellular Noise (wrapping grid modulo periodW and periodH)
void periodicWorley2D(float x, float y, int periodW, int periodH, uint32_t seed, float& d1, float& d2) noexcept {
    int pW = std::max(1, periodW);
    int pH = std::max(1, periodH);

    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    d1 = 10.0f;
    d2 = 10.0f;

    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            int cx = ((ix + i) % pW + pW) % pW;
            int cy = ((iy + j) % pH + pH) % pH;

            float px = static_cast<float>(i) + hash2D(cx, cy, seed);
            float py = static_cast<float>(j) + hash2D(cx, cy, seed + 997);
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
    // Eliminates repetitive periodic patterning across wide displays while wrapping seamlessly
    for (int my = 0; my < kModHeight; ++my) {
        for (int mx = 0; mx < kModWidth; ++mx) {
            float fx = static_cast<float>(mx);
            float fy = static_cast<float>(my);
            float lowFreq = periodicFbm2D(fx * (4.0f / static_cast<float>(kModWidth)),
                                          fy * (2.0f / static_cast<float>(kModHeight)),
                                          4.0f, 2.0f, 3, 2.0f, 0.5f, 7771);
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
                    // Stretched anisotropic horizontal brush streaks:
                    // Scaled 4x along the main horizontal axis (0.005f & 0.0125f)
                    // Toned down contrast: lines and random dots are close in intensity, eliminating harsh pixelart
                    float yFreq1 = 3.5f;
                    float yFreq2 = 7.0f;
                    float xFreq1 = 0.005f;
                    float xFreq2 = 0.0125f;
                    float pW1 = static_cast<float>(kMasterWidth) * xFreq1;
                    float pH1 = static_cast<float>(kMasterHeight) * yFreq1;
                    float pW2 = static_cast<float>(kMasterWidth) * xFreq2;
                    float pH2 = static_cast<float>(kMasterHeight) * yFreq2;

                    float n1 = periodicNoise2D(fx * xFreq1, fy * yFreq1, pW1, pH1, 101) - 0.5f;
                    float n2 = periodicNoise2D(fx * xFreq2, fy * yFreq2, pW2, pH2, 203) - 0.5f;
                    float brush = n1 * 0.60f + n2 * 0.40f;

                    // Subtle specular sheen band across the plate (close in intensity)
                    float sheen = std::pow(std::cos((fx / static_cast<float>(kMasterWidth)) * 3.14159265f * 2.0f), 4.0f) * 0.05f;
                    s.height = std::clamp(0.5f + brush * 0.10f + sheen, 0.0f, 1.0f);
                    s.detail = std::clamp(brush * 0.4f + 0.5f, 0.0f, 1.0f);

                    // Refined, subtle micro-glints with tight, uniform intensity
                    float randScratch = hash2D(x % kMasterWidth, y % kMasterHeight, 991);
                    if (randScratch > 0.982f) {
                        s.fleck = (randScratch - 0.982f) / 0.018f * 0.18f;
                    }
                    break;
                }

                case ProceduralMaterialType::MattePowderCoat: {
                    // Isotropic tactile micro-stipple (diffuse non-glare) wrapping modulo
                    float s1 = hash2D(x % kMasterWidth, y % kMasterHeight, 101) - 0.5f;
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
                    // Organic fiber coordinate warping - periodic toroidal noise
                    float warpX = (periodicNoise2D(fx * (6.0f / 1024.0f), fy * (2.0f / 128.0f), 6.0f, 2.0f, 881) - 0.5f) * 24.0f;
                    float warpY = (periodicNoise2D(fx * (10.0f / 1024.0f), fy * (2.0f / 128.0f), 10.0f, 2.0f, 991) - 0.5f) * 12.0f;

                    // Concentric growth rings with exact integer harmonic cycles across 1024x128
                    // Walnut: 11 cycles across W, 1 cycle across H
                    // Rosewood: 20 cycles across W, 1 cycle across H
                    float kRingsX = isRosewood ? 20.0f : 11.0f;
                    float ringFreqX = (kRingsX * 2.0f * 3.14159265f) / static_cast<float>(kMasterWidth);
                    float ringFreqY = (1.0f * 2.0f * 3.14159265f) / static_cast<float>(kMasterHeight);
                    float ringVal = std::sin((fx + warpX) * ringFreqX + (fy + warpY) * ringFreqY);
                    s.height = std::clamp(ringVal * 0.5f + 0.5f, 0.0f, 1.0f);

                    // Longitudinal pore vessels (dark elongated wood pores) with periodic wrapping
                    int porePx = static_cast<int>(fx * 0.75f) % static_cast<int>(kMasterWidth * 0.75f);
                    int porePy = static_cast<int>(fy * 0.04f) % static_cast<int>(kMasterHeight * 0.04f);
                    float poreCut = hash2D(porePx, porePy, 773);
                    if (poreCut > 0.92f) {
                        s.detail = (poreCut - 0.92f) / 0.08f; // Pore intensity
                        s.crevice = s.detail * 0.6f;
                    }
                    break;
                }

                case ProceduralMaterialType::Bakelite: {
                    // Vintage 1930s-50s phenolic resin liquid marbling swirls with periodic FBM
                    float qx = (periodicFbm2D(fx * (8.0f / 1024.0f), fy * (4.0f / 128.0f), 8.0f, 4.0f, 3, 2.0f, 0.5f, 411) - 0.5f) * 45.0f;
                    float qy = (periodicFbm2D(fx * (8.0f / 1024.0f), fy * (4.0f / 128.0f), 8.0f, 4.0f, 3, 2.0f, 0.5f, 523) - 0.5f) * 25.0f;
                    float flowSwirl = periodicFbm2D((fx + qx) * (8.0f / 1024.0f), (fy + qy) * (4.0f / 128.0f), 8.0f, 4.0f, 4, 2.0f, 0.5f, 631);

                    s.height = std::clamp(flowSwirl, 0.0f, 1.0f);
                    // Polished resin specular luster
                    float luster = std::pow(std::sin(flowSwirl * 3.14159265f * 1.5f) * 0.5f + 0.5f, 3.0f);
                    s.detail = luster;
                    break;
                }

                case ProceduralMaterialType::CrinklePaint: {
                    // High-resolution vintage wrinkle / crinkle finish (2.5x higher cellular density + micro-crevices)
                    float d1 = 0.0f, d2 = 0.0f;
                    int pW = static_cast<int>(std::round(static_cast<float>(kMasterWidth) * 0.38f));
                    int pH = static_cast<int>(std::round(static_cast<float>(kMasterHeight) * 0.48f));
                    periodicWorley2D(fx * 0.38f, fy * 0.48f, pW, pH, 311, d1, d2);
                    float ridge = 1.0f - std::clamp((d2 - d1) * 3.6f, 0.0f, 1.0f);

                    float d1b = 0.0f, d2b = 0.0f;
                    periodicWorley2D(fx * 0.76f, fy * 0.96f, pW * 2, pH * 2, 709, d1b, d2b);
                    float microRidge = 1.0f - std::clamp((d2b - d1b) * 3.0f, 0.0f, 1.0f);

                    float combined = ridge * 0.75f + microRidge * 0.25f;
                    s.height = std::clamp(combined, 0.0f, 1.0f);
                    s.crevice = std::clamp((1.0f - combined) * 0.85f, 0.0f, 1.0f);
                    s.detail = std::pow(combined, 2.0f);
                    break;
                }

                case ProceduralMaterialType::PcbFiberglass: {
                    // Cross-hatch woven FR4 substrate with exact periodic periods
                    float weaveX = std::sin(fx * (73.0f * 2.0f * 3.14159265f / static_cast<float>(kMasterWidth))) * 0.5f + 0.5f;
                    float weaveY = std::sin(fy * (9.0f * 2.0f * 3.14159265f / static_cast<float>(kMasterHeight))) * 0.5f + 0.5f;
                    s.height = std::clamp(weaveX * 0.5f + weaveY * 0.5f, 0.0f, 1.0f);
                    // Copper trace flecks
                    float trace = hash2D((static_cast<int>(fx * 0.1f)) % 102, (static_cast<int>(fy * 0.2f)) % 25, 819);
                    if (trace > 0.94f) s.fleck = 0.6f;
                    break;
                }

                case ProceduralMaterialType::CarbonFiber: {
                    // 2x2 Twill carbon weave with modulo wrapping
                    int ix = (static_cast<int>(fx * 0.35f)) % static_cast<int>(kMasterWidth * 0.35f);
                    int iy = (static_cast<int>(fy * 0.35f)) % static_cast<int>(kMasterHeight * 0.35f);
                    bool diag = ((ix + iy) & 2) != 0;
                    float weave = diag ? 0.68f : 0.32f;
                    s.height = weave;
                    s.detail = diag ? 1.0f : 0.0f;
                    break;
                }

                case ProceduralMaterialType::CastIronGrungy:
                default: {
                    // Multi-octave stipple with exact power-of-two divisors (dividing 1024 and 128 with zero remainder)
                    float h1 = hash2D(x % kMasterWidth, y % kMasterHeight, 101) - 0.5f;
                    float h2 = hash2D((x / 2) % (kMasterWidth / 2), (y / 2) % (kMasterHeight / 2), 203) - 0.5f;
                    float h3 = hash2D((x / 4) % (kMasterWidth / 4), (y / 4) % (kMasterHeight / 4), 307) - 0.5f;
                    float h4 = hash2D((x / 8) % (kMasterWidth / 8), (y / 8) % (kMasterHeight / 8), 409) - 0.5f;
                    float xWarp = std::sin(fx * (7.0f * 2.0f * 3.14159265f / static_cast<float>(kMasterWidth))) * 0.14f +
                                  std::cos(fx * (14.0f * 2.0f * 3.14159265f / static_cast<float>(kMasterWidth)) +
                                           fy * (2.0f * 2.0f * 3.14159265f / static_cast<float>(kMasterHeight))) * 0.08f;
                    float stipple = 0.5f + (h1 * 0.46f + h2 * 0.28f + h3 * 0.16f + h4 * 0.10f + xWarp * 0.10f) * 0.35f;

                    float fleck = 0.0f;
                    float randSubtle = hash2D(x % kMasterWidth, y % kMasterHeight, 888);
                    if (randSubtle > 0.965f) {
                        fleck = (randSubtle - 0.965f) / 0.035f * 0.22f;
                    }
                    float randFleck = hash2D(x % kMasterWidth, y % kMasterHeight, 777);
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

    // 3. Exact Toroidal Seamless Boundary Smoothing Pass:
    // Crossfades the left (x in [0..M-1]) and right (x in [W-M..W-1]) edges symmetrically
    // so that column 0 exactly matches column W-1 with C1-continuous transition.
    const int blendMarginX = 32;
    for (int y = 0; y < kMasterHeight; ++y) {
        for (int d = 0; d < blendMarginX; ++d) {
            float t = static_cast<float>(d) / static_cast<float>(blendMarginX);
            float w = t * t * (3.0f - 2.0f * t); // smoothstep: 0 at d=0, 1 at d=M
            int leftIdx = y * kMasterWidth + d;
            int rightIdx = y * kMasterWidth + (kMasterWidth - 1 - d);

            auto left = buf.master[leftIdx];
            auto right = buf.master[rightIdx];

            auto blend = [&](float l, float r) {
                float mid = (l + r) * 0.5f;
                return std::make_pair(mid * (1.0f - w) + l * w, mid * (1.0f - w) + r * w);
            };

            auto [hL, hR] = blend(left.height, right.height);
            auto [dL, dR] = blend(left.detail, right.detail);
            auto [cL, cR] = blend(left.crevice, right.crevice);
            auto [fL, fR] = blend(left.fleck, right.fleck);

            buf.master[leftIdx] = ProceduralSample{hL, fL, dL, cL};
            buf.master[rightIdx] = ProceduralSample{hR, fR, dR, cR};
        }
    }

    const int blendMarginY = 16;
    for (int x = 0; x < kMasterWidth; ++x) {
        for (int d = 0; d < blendMarginY; ++d) {
            float t = static_cast<float>(d) / static_cast<float>(blendMarginY);
            float w = t * t * (3.0f - 2.0f * t);
            int topIdx = d * kMasterWidth + x;
            int botIdx = (kMasterHeight - 1 - d) * kMasterWidth + x;

            auto top = buf.master[topIdx];
            auto bot = buf.master[botIdx];

            auto blend = [&](float tp, float bt) {
                float mid = (tp + bt) * 0.5f;
                return std::make_pair(mid * (1.0f - w) + tp * w, mid * (1.0f - w) + bt * w);
            };

            auto [hT, hB] = blend(top.height, bot.height);
            auto [dT, dB] = blend(top.detail, bot.detail);
            auto [cT, cB] = blend(top.crevice, bot.crevice);
            auto [fT, fB] = blend(top.fleck, bot.fleck);

            buf.master[topIdx] = ProceduralSample{hT, fT, dT, cT};
            buf.master[botIdx] = ProceduralSample{hB, fB, dB, cB};
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
            // Anodized / machined brushed metal:
            // Toned down contrast: gentle modulation without harsh pixelart blowout
            float luma = sample.height;
            float contrastCurve = (luma - 0.5f) * 0.18f;
            Color dyed = (contrastCurve >= 0.0f)
                ? tintColor.lighten(contrastCurve)
                : tintColor.darken(-contrastCurve * 1.10f);

            // Subtle specular sheen: anisotropic reflection stays close in intensity
            float spec = std::clamp((sample.height - 0.60f) / 0.40f, 0.0f, 1.0f);
            float wearDiffusion = 1.0f - wearIntensity * 0.40f; // Patina diffuses sharp specular sheen
            Color res = Color::lerp(dyed, Color(0.96f, 0.97f, 1.0f), spec * 0.16f * wearDiffusion);

            // Surface wear & patina: weathered oxidation shifts tone
            if (wearIntensity > 0.01f) {
                res = Color::lerp(res, res.darken(0.14f), wearIntensity * 0.40f);
            }

            // Metallic flecks retain subtle glint with tight, uniform intensity
            if (sample.fleck > 0.01f) {
                res = Color::lerp(res, Color(0.96f, 0.94f, 0.88f), sample.fleck * (0.45f + wearIntensity * 0.45f));
            }
            return res;
        }

        case ProceduralMaterialType::MattePowderCoat: {
            // Powder-coated finish: Even diffuse absorption with micro-stipple occlusion
            float delta = (sample.height - 0.5f) * (0.16f + wearIntensity * 0.10f);
            Color res = (delta >= 0.0f) ? tintColor.lighten(delta) : tintColor.darken(-delta * (1.25f + wearIntensity * 0.5f));
            if (wearIntensity > 0.01f) {
                res = Color::lerp(res, res.darken(0.10f), wearIntensity * 0.30f);
            }
            return res;
        }

        case ProceduralMaterialType::WalnutWood:
        case ProceduralMaterialType::Rosewood: {
            // Wood stain: Two-point harmonic model (earlywood tint vs deep latewood rings)
            float agedHeight = std::clamp(sample.height - wearIntensity * 0.10f, 0.0f, 1.0f);
            Color earlywood = Color::lerp(tintColor, tintColor.darken(0.12f), wearIntensity * 0.35f);
            Color latewood = tintColor.darken(0.38f + wearIntensity * 0.15f).saturate(0.18f);
            Color res = Color::lerp(latewood, earlywood, agedHeight);

            // Longitudinal pore cuts remain dark and deepen with vintage patina
            if (sample.detail > 0.1f) {
                res = res.darken(sample.detail * (0.40f + wearIntensity * 0.30f));
            }
            return res;
        }

        case ProceduralMaterialType::Bakelite: {
            // Phenolic resin: Marbled ribbons between rich amber/oxblood resin and custom tint
            Color resinAmber(0.24f, 0.08f, 0.05f); // Classic dark mahogany phenol
            Color swirl = Color::lerp(resinAmber, tintColor, sample.height * 0.85f);
            // Polished resin specular luster (softens with vintage wear)
            float luster = sample.detail * (1.0f - wearIntensity * 0.45f);
            Color res = swirl.lighten(luster * 0.28f);
            if (wearIntensity > 0.01f) {
                res = Color::lerp(res, res.darken(0.12f), wearIntensity * 0.35f);
            }
            return res;
        }

        case ProceduralMaterialType::CrinklePaint: {
            // High-resolution industrial wrinkle finish: Ridges catch highlight, valleys collect shadow
            Color ridge = tintColor.lighten(0.14f - wearIntensity * 0.06f);
            Color valley = tintColor.darken(0.40f + wearIntensity * 0.20f);
            Color res = Color::lerp(valley, ridge, sample.height);

            if (sample.crevice > 0.01f) {
                res = Color::lerp(res, Color(0.04f, 0.04f, 0.05f), sample.crevice * (0.85f + wearIntensity * 0.15f));
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
            if (wearIntensity > 0.01f) {
                res = Color::lerp(res, res.darken(0.10f), wearIntensity * 0.30f);
            }
            return res;
        }

        case ProceduralMaterialType::CarbonFiber: {
            Color carbon = tintColor;
            float sheen = (sample.detail > 0.5f) ? (0.15f * (1.0f - wearIntensity * 0.4f)) : -0.15f;
            Color res = (sheen >= 0.0f) ? carbon.lighten(sheen) : carbon.darken(-sheen);
            if (wearIntensity > 0.01f) {
                res = Color::lerp(res, res.darken(0.08f), wearIntensity * 0.30f);
            }
            return res;
        }

        case ProceduralMaterialType::CastIronGrungy:
        default: {
            // Dark Chassis anodized / harmonic tinting:
            // When tintColor differs from baseColor (custom tint active), harmonically dye the dark metal plate
            Color effectiveBase = baseColor;
            if (tintColor != baseColor) {
                Color dyed = tintColor.darken(0.35f);
                effectiveBase = Color::lerp(baseColor, dyed, 0.65f);
            }
            // Surface wear & patina mottling
            if (wearIntensity > 0.01f) {
                effectiveBase = Color::lerp(effectiveBase, effectiveBase.darken(0.18f), wearIntensity * 0.45f);
            }
            float stippleDelta = (sample.height - 0.5f) * (0.22f + wearIntensity * 0.12f);
            Color res = (stippleDelta >= 0.0f)
                ? effectiveBase.lighten(stippleDelta)
                : effectiveBase.darken(-stippleDelta * (1.35f + wearIntensity * 0.5f));

            if (sample.fleck > 0.01f) {
                Color brassFleck = Color::lerp(effectiveBase.lighten(0.15f), Color(1.0f, 0.88f, 0.60f), 0.35f);
                res = Color::lerp(res, brassFleck, sample.fleck * (0.8f + wearIntensity * 0.8f));
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
                                         float wearIntensity,
                                         float rotation) {
    uint32_t tintKey = customTint.has_value() ? customTint->toRgba8() : 0u;
    uint32_t themeKey = theme.panelHeaderGradientTop.toRgba8() ^ theme.panelHeaderGradientBottom.toRgba8();

    // Rotation simplified to 2 options: 0 (NORMAL) and 90 (VERTICAL)
    int rotNorm = static_cast<int>(std::round(rotation)) % 360;
    if (rotNorm < 0) rotNorm += 360;
    int rotAngle = (rotNorm >= 45 && rotNorm < 135) ? 90 : 0;

    std::ostringstream ss;
    ss << static_cast<int>(mat) << "_" << theme.name << "_" << tintKey << "_"
       << static_cast<int>(wearIntensity * 100.0f) << "_r" << rotAngle;
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
    int srcW = kMasterWidth;
    int srcH = kMasterHeight;

    if (rotAngle == 90) {
        item.width = srcH;  // 128
        item.height = srcW; // 1024
    } else {
        item.width = srcW;  // 1024
        item.height = srcH; // 128
    }

    item.rgba.resize(static_cast<size_t>(item.width * item.height * 4));
    item.tintKey = tintKey;
    item.themeKey = themeKey;
    item.wear = wearIntensity;
    item.rotation = static_cast<float>(rotAngle);

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

    // Uniform base color for the texture tile:
    // Eliminates per-tile vertical gradients so individual cells tile seamlessly without repetitive gradient lines
    Color uniformBase = customTint.has_value()
        ? *customTint
        : Color::lerp(theme.panelHeaderGradientTop, theme.panelHeaderGradientBottom, 0.5f);

    for (int dstY = 0; dstY < item.height; ++dstY) {
        for (int dstX = 0; dstX < item.width; ++dstX) {
            int srcX = 0;
            int srcY = 0;
            if (rotAngle == 90) {
                srcX = dstY;
                srcY = srcH - 1 - dstX;
            } else {
                srcX = dstX;
                srcY = dstY;
            }

            const auto& s = master[srcY * srcW + srcX];

            // Blend with dual-frequency modulator
            float modDrift = mod[(srcY % kModHeight) * kModWidth + (srcX % kModWidth)];
            ProceduralSample blendedSample = s;
            blendedSample.height = std::clamp(blendedSample.height + modDrift, 0.0f, 1.0f);

            Color c = blendMaterialColor(mat, blendedSample, uniformBase, effectiveTint, wearIntensity);

            int idx = (dstY * item.width + dstX) * 4;
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
                                        bool isTopPanel,
                                        float rotation) {
    if (!r || w <= 0.0f || h <= 0.0f) return;

    ProceduralMaterialType mat = chassisStyleToMaterial(style);
    const auto& tex = getOrCreateTinted(mat, theme, customTint, wearIntensity, rotation);
    if (tex.rgba.empty()) return;

    // 1. Render smooth continuous vertical gradient over the FULL bounds [y, y + h]
    // The gradient spans the entire GUI/panel background rather than repeating per texture cell.
    Color gradTop, gradBot;
    if (customTint.has_value()) {
        gradTop = customTint->lighten(0.08f);
        gradBot = customTint->darken(0.14f);
    } else {
        gradTop = isTopPanel ? theme.panelHeaderGradientTop : theme.panelHeaderGradientTop.darken(0.04f);
        gradBot = isTopPanel ? theme.panelHeaderGradientBottom : theme.panelHeaderGradientBottom.darken(0.08f);
    }

    if (cornerRadius > 0.0f) {
        r->drawRoundedRectGradient(x, y, w, h, cornerRadius,
                                   gradTop.r, gradTop.g, gradTop.b,
                                   gradBot.r, gradBot.g, gradBot.b, 1.0f);
    } else {
        r->drawRectGradient(x, y, w, h,
                            gradTop.r, gradTop.g, gradTop.b,
                            gradBot.r, gradBot.g, gradBot.b, 1.0f);
    }

    // 2. Dual-Frequency 1:1 Pixel-Ratio Tiling:
    // Push hardware scissor so partial tiles at the right/bottom edge are clipped cleanly
    // rather than squished or stretched across GUI dimensions.
    r->pushScissor(x, y, w, h);
    const float texOpacity = 0.65f; // Allows the full GUI vertical gradient to flow smoothly underneath
    for (float curY = y; curY < y + h; curY += static_cast<float>(tex.height)) {
        for (float curX = x; curX < x + w; curX += static_cast<float>(tex.width)) {
            r->drawRgbaBitmap(curX, curY, static_cast<float>(tex.width), static_cast<float>(tex.height),
                              tex.rgba.data(), tex.width, tex.height, texOpacity);
        }
    }
    r->popScissor();

    // 3. World-Space Anomaly & Surface Wear Pass:
    // Sparse deterministic micro-scratches placed at absolute window coordinates with crisp contrast
    if (wearIntensity > 0.05f) {
        float startCellX = std::floor(x / 160.0f) * 160.0f;
        for (float cx = startCellX; cx < x + w; cx += 160.0f) {
            float scratch = evaluateWorldAnomaly(cx + 80.0f, y + h * 0.5f, 911);
            if (scratch > 0.01f) {
                float sx = std::clamp(cx + 40.0f, x + 2.0f, x + w - 40.0f);
                float sy = y + h * 0.35f;
                // Dual scratch: dark crease shadow + crisp metallic specular reflection
                drawLine(*r, sx, sy + 1.0f, sx + 28.0f, sy + 7.0f,
                         0.0f, 0.0f, 0.0f, scratch * 0.35f * wearIntensity, 1.0f);
                drawLine(*r, sx, sy, sx + 28.0f, sy + 6.0f,
                         1.0f, 1.0f, 1.0f, scratch * 0.28f * wearIntensity, 1.0f);
            }
        }

        // Subtle corner & edge micro-scuffing for higher wear levels
        if (wearIntensity > 0.35f) {
            float scuffA = (wearIntensity - 0.35f) * 0.35f;
            drawLine(*r, x + 2.0f, y + 2.0f, x + 24.0f, y + 2.0f, 1.0f, 1.0f, 1.0f, scuffA, 1.0f);
            drawLine(*r, x + w - 24.0f, y + 2.0f, x + w - 2.0f, y + 2.0f, 1.0f, 1.0f, 1.0f, scuffA, 1.0f);
        }
    }

    // 4. Machined Crevice Shadow on rim
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
                                                     float cornerRadius,
                                                     float rotation) {
    if (!r || bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    // 1. Draw base rounded rect backing
    Color baseCol = customTint.has_value() ? *customTint : theme.panelBackground;
    drawRoundedRect(*r, bounds.x, bounds.y, bounds.w, bounds.h, cornerRadius,
                    baseCol.r * 0.85f, baseCol.g * 0.85f, baseCol.b * 0.85f, 1.0f);

    // 2. Overlay procedural textured material with 1:1 ratio
    drawChassis(r, bounds.x, bounds.y, bounds.w, bounds.h, style, theme,
                customTint, wearIntensity, cornerRadius, false, rotation);

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
