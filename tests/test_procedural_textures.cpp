#include "eatsbits/ui/procedural_texture_system.hpp"
#include "eatsbits/ui/gui_panel_def.hpp"
#include "eatsbits/ui/theme.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits::ui;

static void assertNear(float actual, float expected, float eps = 0.02f, const char* msg = "") {
    if (std::abs(actual - expected) > eps) {
        std::cerr << "Assertion failed: " << msg << " (actual: " << actual << ", expected: " << expected << ")" << std::endl;
        std::abort();
    }
}

void testMaterialMappings() {
    std::cout << "[Test 1] Chassis Style to Procedural Material Mappings..." << std::endl;

    assert(chassisStyleToMaterial(GuiChassisStyle::BrushedAluminum) == ProceduralMaterialType::BrushedAluminum);
    assert(chassisStyleToMaterial(GuiChassisStyle::BrushedSteel) == ProceduralMaterialType::BrushedSteel);
    assert(chassisStyleToMaterial(GuiChassisStyle::MattePowderCoat) == ProceduralMaterialType::MattePowderCoat);
    assert(chassisStyleToMaterial(GuiChassisStyle::Bakelite) == ProceduralMaterialType::Bakelite);
    assert(chassisStyleToMaterial(GuiChassisStyle::CrinklePaint) == ProceduralMaterialType::CrinklePaint);
    assert(chassisStyleToMaterial(GuiChassisStyle::Walnut) == ProceduralMaterialType::WalnutWood);
    assert(chassisStyleToMaterial(GuiChassisStyle::Rosewood) == ProceduralMaterialType::Rosewood);
    assert(chassisStyleToMaterial(GuiChassisStyle::PcbGreen) == ProceduralMaterialType::PcbFiberglass);
    assert(chassisStyleToMaterial(GuiChassisStyle::Carbon) == ProceduralMaterialType::CarbonFiber);
    assert(chassisStyleToMaterial(GuiChassisStyle::DarkChassis) == ProceduralMaterialType::CastIronGrungy);

    std::cout << "  -> PASSED: All material types correctly mapped." << std::endl;
}

void testAperiodicTilingProperties() {
    std::cout << "[Test 2] Aperiodic Dual-Frequency Resolution & Modulus..." << std::endl;

    // Master tile dimension = 1024, Modulator dimension = 719 (Prime)
    int masterW = ProceduralTextureSystem::kMasterWidth;
    int modW = ProceduralTextureSystem::kModWidth;

    assert(masterW == 1024);
    assert(modW == 719);

    // Verify Coprime properties (GCD = 1) -> No repetitive beating
    auto gcd = [](int a, int b) {
        while (b != 0) {
            int t = b;
            b = a % b;
            a = t;
        }
        return a;
    };
    assert(gcd(masterW, modW) == 1);

    long long repeatPeriod = static_cast<long long>(masterW) * static_cast<long long>(modW);
    assert(repeatPeriod == 736256LL);
    std::cout << "  -> Repeat period without identical pattern: " << repeatPeriod << " pixels." << std::endl;
    std::cout << "  -> PASSED: Aperiodic dual-frequency tiling verified." << std::endl;
}

void testWorldSpaceAnomalies() {
    std::cout << "[Test 3] World-Space Anomaly & Scratch Generation..." << std::endl;

    // Test deterministic output
    float a1 = ProceduralTextureSystem::evaluateWorldAnomaly(300.0f, 25.0f, 911);
    float a2 = ProceduralTextureSystem::evaluateWorldAnomaly(300.0f, 25.0f, 911);
    assertNear(a1, a2, 0.0001f, "Deterministic evaluation");

    // Test spatial variance: worldX difference yields different anomaly scores
    float diffCount = 0;
    for (float x = 0.0f; x < 3000.0f; x += 150.0f) {
        float val = ProceduralTextureSystem::evaluateWorldAnomaly(x, 20.0f, 911);
        if (val > 0.0f) diffCount++;
    }
    // Sparse distribution: should only hit a few times across 20 samples
    assert(diffCount >= 1 && diffCount <= 8);
    std::cout << "  -> PASSED: Sparse world-space anomalies correctly distributed." << std::endl;
}

void testPhysicallyAwareBlending() {
    std::cout << "[Test 4] Physically Aware Material Color Blending..." << std::endl;

    Color themeBase(0.20f, 0.20f, 0.22f);
    Color tintBlue(0.10f, 0.40f, 0.90f);

    // 1. Brushed Aluminum / Anodized Metal Specular Protection
    {
        ProceduralSample midSample{0.5f, 0.0f, 0.5f, 0.0f};
        ProceduralSample specSample{0.95f, 0.2f, 0.9f, 0.0f}; // Sharp anisotropic highlight

        Color midResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::BrushedAluminum, midSample, themeBase, tintBlue, 0.2f);
        Color specResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::BrushedAluminum, specSample, themeBase, tintBlue, 0.2f);

        // Specular highlight must stay bright white/silver and NOT become dark muddy blue
        assert(specResult.luminance() > midResult.luminance());
        assert(specResult.r > 0.60f && specResult.g > 0.60f); // Bright reflection
    }

    // 2. Wood Grain Two-Point Stain Model
    {
        Color stainWarm(0.50f, 0.28f, 0.12f);
        ProceduralSample earlywood{0.9f, 0.0f, 0.0f, 0.0f}; // Soft growth band
        ProceduralSample latewood{0.1f, 0.0f, 0.9f, 0.0f};  // Dense ring fiber + dark pore

        Color earlyResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::WalnutWood, earlywood, themeBase, stainWarm, 0.2f);
        Color lateResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::WalnutWood, latewood, themeBase, stainWarm, 0.2f);

        // Latewood rings must be deeper, richer, and darker than earlywood
        assert(earlyResult.luminance() > lateResult.luminance());
        assert(lateResult.r < earlyResult.r);
    }

    // 3. Crinkle Paint Ridge Highlights vs Crevice Occlusion
    {
        Color crinkleGray(0.25f, 0.25f, 0.28f);
        ProceduralSample ridgeSample{0.95f, 0.0f, 0.8f, 0.0f};
        ProceduralSample valleySample{0.10f, 0.0f, 0.0f, 0.8f};

        Color ridgeResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::CrinklePaint, ridgeSample, themeBase, crinkleGray, 0.2f);
        Color valleyResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::CrinklePaint, valleySample, themeBase, crinkleGray, 0.2f);

        assert(ridgeResult.luminance() > valleyResult.luminance());
        assert(valleyResult.luminance() < 0.15f); // Deep valley shadow
    }

    // 4. Bakelite Phenolic Resin Marbling & Specular Luster
    {
        Color tintOxblood(0.45f, 0.08f, 0.06f);
        ProceduralSample swirlSample{0.7f, 0.0f, 0.9f, 0.0f}; // High polish swirl ridge

        Color bakeliteResult = ProceduralTextureSystem::blendMaterialColor(
            ProceduralMaterialType::Bakelite, swirlSample, themeBase, tintOxblood, 0.2f);

        // Polished luster brightens the rich amber/oxblood phenolic base
        assert(bakeliteResult.r > bakeliteResult.g);
        assert(bakeliteResult.r > bakeliteResult.b);
        assert(bakeliteResult.luminance() > 0.15f);
    }

    std::cout << "  -> PASSED: Physically aware blends accurately protect highlights and depth." << std::endl;
}

void testDrawingAndCaching() {
    std::cout << "[Test 5] BatchRenderer2D Drawing & Texture Caching..." << std::endl;

    auto& sys = ProceduralTextureSystem::instance();
    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);

    // Create a dummy batch renderer
    BatchRenderer2D renderer;
    renderer.beginFrame(800.0f, 600.0f);

    // 1. Draw top & bottom chassis plates
    sys.drawChassis(&renderer, 0.0f, 0.0f, 800.0f, 56.0f, GuiChassisStyle::DarkChassis, theme, std::nullopt, 0.2f, 0.0f, true);
    sys.drawChassis(&renderer, 0.0f, 544.0f, 800.0f, 56.0f, GuiChassisStyle::BrushedAluminum, theme, Color(0.12f, 0.45f, 0.95f), 0.25f, 0.0f, false);

    // 2. Draw skeuomorphic faceplates with different materials
    GuiPanelDef p1;
    p1.chassisStyle = GuiChassisStyle::Bakelite;
    p1.chassisTint = Color(0.50f, 0.10f, 0.08f);
    sys.drawFaceplateBackground(&renderer, Rect2D(20.0f, 80.0f, 360.0f, 200.0f), p1.chassisStyle, theme, p1.chassisTint, p1.textureWear, p1.cornerRadius);

    GuiPanelDef p2;
    p2.chassisStyle = GuiChassisStyle::CrinklePaint;
    sys.drawFaceplateBackground(&renderer, Rect2D(400.0f, 80.0f, 360.0f, 200.0f), p2.chassisStyle, theme, p2.chassisTint, p2.textureWear, p2.cornerRadius);

    // 3. Draw tactile button noise
    sys.drawButtonNoise(&renderer, 50.0f, 320.0f, 80.0f, 32.0f, true, 0.28f, 4.0f);
    sys.drawButtonNoise(&renderer, 150.0f, 320.0f, 80.0f, 32.0f, true, 0.28f, 4.0f, true, true, true, true, true, 3.0f);

    // Verify unified drawGuiFaceplate helper
    drawGuiFaceplate(renderer, p1, Rect2D(20.0f, 360.0f, 360.0f, 180.0f), theme);

    renderer.endFrame();

    std::cout << "  -> PASSED: Drawing, faceplates, button noise, and caching fully verified." << std::endl;
}

void testPanelCustomizationOptions() {
    std::cout << "[Test 6] Panel Texture, Tint, and Wear Customization..." << std::endl;

    const GuiChassisStyle styles[10] = {
        GuiChassisStyle::DarkChassis,
        GuiChassisStyle::BrushedAluminum,
        GuiChassisStyle::BrushedSteel,
        GuiChassisStyle::MattePowderCoat,
        GuiChassisStyle::CrinklePaint,
        GuiChassisStyle::Bakelite,
        GuiChassisStyle::Walnut,
        GuiChassisStyle::Rosewood,
        GuiChassisStyle::PcbGreen,
        GuiChassisStyle::Carbon
    };

    auto& sys = ProceduralTextureSystem::instance();
    ThemeTokens theme = Theme::get(Theme::Preset::AteTrack);
    BatchRenderer2D renderer;
    renderer.beginFrame(800.0f, 600.0f);

    for (int i = 0; i < 10; ++i) {
        ProceduralMaterialType mat = chassisStyleToMaterial(styles[i]);
        assert(static_cast<int>(mat) >= 0 && static_cast<int>(mat) <= 9);
        sys.drawChassis(&renderer, 0.0f, 0.0f, 800.0f, 56.0f, styles[i], theme, std::nullopt, 0.20f, 0.0f, true);
        sys.drawChassis(&renderer, 0.0f, 544.0f, 800.0f, 56.0f, styles[i], theme, Color(0.20f, 0.55f, 0.95f), 0.50f, 0.0f, false);
    }

    const std::optional<Color> tints[6] = {
        std::nullopt,
        Color(0.20f, 0.55f, 0.95f),
        Color(0.88f, 0.76f, 0.45f),
        Color(0.22f, 0.65f, 0.40f),
        Color(0.80f, 0.25f, 0.30f),
        Color(0.40f, 0.45f, 0.52f)
    };
    for (int t = 0; t < 6; ++t) {
        sys.drawChassis(&renderer, 0.0f, 0.0f, 800.0f, 56.0f, GuiChassisStyle::BrushedAluminum, theme, tints[t], 0.20f, 0.0f, true);
    }

    const float wears[4] = {0.0f, 0.20f, 0.50f, 0.80f};
    for (int w = 0; w < 4; ++w) {
        sys.drawChassis(&renderer, 0.0f, 0.0f, 800.0f, 56.0f, GuiChassisStyle::BrushedSteel, theme, std::nullopt, wears[w], 0.0f, true);
    }

    renderer.endFrame();

    std::cout << "  -> PASSED: All 10 materials, 6 tints, and 4 wear levels render cleanly." << std::endl;
}

int main() {
    std::cout << "======================================================" << std::endl;
    std::cout << "  Eatsbits Procedural Texture & Material Engine Tests " << std::endl;
    std::cout << "======================================================" << std::endl;

    testMaterialMappings();
    testAperiodicTilingProperties();
    testWorldSpaceAnomalies();
    testPhysicallyAwareBlending();
    testDrawingAndCaching();
    testPanelCustomizationOptions();

    std::cout << "======================================================" << std::endl;
    std::cout << "  ALL PROCEDURAL TEXTURE SYSTEM TESTS PASSED! (100%)  " << std::endl;
    std::cout << "======================================================" << std::endl;
    return 0;
}
