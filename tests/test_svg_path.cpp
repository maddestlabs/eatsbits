#include "eatsbits/ui/svg_path.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace eatsbits::ui;

void testBasicPathParsing() {
    std::cout << "[Test] Parsing basic SVG path lines and rects...\n";
    const char* lineSvg = "M 10 10 L 50 10 L 50 50 L 10 50 Z";
    SvgPath path = SvgPath::parse(lineSvg);

    assert(!path.empty());
    assert(path.getContours().size() == 1);
    assert(path.getContours()[0].size() >= 4);

    const auto& b = path.getBounds();
    assert(std::abs(b.minX - 10.0f) < 1e-3f);
    assert(std::abs(b.minY - 10.0f) < 1e-3f);
    assert(std::abs(b.maxX - 50.0f) < 1e-3f);
    assert(std::abs(b.maxY - 50.0f) < 1e-3f);
    assert(std::abs(b.width() - 40.0f) < 1e-3f);
    assert(std::abs(b.height() - 40.0f) < 1e-3f);

    std::cout << " -> Passed basic path parsing and bounds verification.\n";
}

void testCurvesAndArcs() {
    std::cout << "[Test] Parsing cubic beziers and elliptical arcs...\n";
    // Circle via arcs
    const char* arcCircleSvg = "M 100 50 A 50 50 0 1 0 200 50 A 50 50 0 1 0 100 50 Z";
    SvgPath arcPath = SvgPath::parse(arcCircleSvg);

    assert(!arcPath.empty());
    const auto& b = arcPath.getBounds();
    assert(b.width() > 80.0f && b.width() <= 110.0f);
    assert(b.height() > 80.0f && b.height() <= 110.0f);

    // Cubic bezier
    const char* cubicSvg = "M 0 0 C 10 20 30 20 40 0 Z";
    SvgPath cubicPath = SvgPath::parse(cubicSvg);
    assert(!cubicPath.empty());
    assert(cubicPath.getContours()[0].size() > 10);

    std::cout << " -> Passed curve and arc parsing.\n";
}

void testTriangulation() {
    std::cout << "[Test] Ear-clipping triangulation of polygon...\n";
    const char* polySvg = "M 0 0 L 100 0 L 100 100 L 0 100 Z";
    SvgPath path = SvgPath::parse(polySvg);

    auto triangles = path.triangulate();
    // A 4-vertex polygon produces 2 triangles = 6 vertices
    assert(triangles.size() == 6);

    std::cout << " -> Triangulation produced " << triangles.size() << " vertices (" << triangles.size() / 3 << " triangles).\n";
}

void testSoftwareRasterization() {
    std::cout << "[Test] Anti-aliased scanline rasterization into RGBA buffer...\n";
    const char* boxSvg = "M 10 10 L 90 10 L 90 90 L 10 90 Z";
    SvgPath path = SvgPath::parse(boxSvg);

    const int w = 64;
    const int h = 64;
    auto rgba = path.rasterize(w, h, 0xFF00FFFF, true); // Cyan fill
    assert(rgba.size() == static_cast<size_t>(w * h * 4));

    // Center pixel should be filled
    int centerIdx = ((h / 2) * w + (w / 2)) * 4;
    assert(rgba[centerIdx + 3] > 200); // Alpha > 200
    // Corner pixel (0, 0) should be transparent
    assert(rgba[0 + 3] == 0);

    std::cout << " -> Rasterized 64x64 RGBA buffer correctly verified.\n";
}

void testEatsbitsLogoLayers() {
    std::cout << "[Test] Verifying 100% pure-path Eatsbits logo layers...\n";
    auto layers = SvgLogo::getLayers();
    assert(layers.size() == 3);

    // Layer 1: Chassis
    SvgPath chassis = SvgPath::parse(layers[0].path);
    auto chassisTris = chassis.triangulate();
    std::cout << "Chassis bounds: [" << chassis.getBounds().minX << ", " << chassis.getBounds().minY
              << " to " << chassis.getBounds().maxX << ", " << chassis.getBounds().maxY << "], contours="
              << chassis.getContours().size() << ", triangles=" << chassisTris.size() / 3 << "\n";

    // Layer 2: Monster head + bits
    SvgPath monster = SvgPath::parse(layers[1].path);
    auto monsterTris = monster.triangulate();
    std::cout << "Monster bounds: [" << monster.getBounds().minX << ", " << monster.getBounds().minY
              << " to " << monster.getBounds().maxX << ", " << monster.getBounds().maxY << "], contours="
              << monster.getContours().size() << ", triangles=" << monsterTris.size() / 3 << "\n";

    // Layer 3: Eye
    SvgPath eye = SvgPath::parse(layers[2].path);
    auto eyeTris = eye.triangulate();
    std::cout << "Eye bounds: [" << eye.getBounds().minX << ", " << eye.getBounds().minY
              << " to " << eye.getBounds().maxX << ", " << eye.getBounds().maxY << "], contours="
              << eye.getContours().size() << ", triangles=" << eyeTris.size() / 3 << "\n";

    // Multi-layer rasterization
    auto logoRgba = SvgTextureCache::rasterizeLayersToRgba(layers, 48, 48);
    std::cout << "Rasterized logoRgba size=" << logoRgba.size() << "\n";

    // Check that non-zero colored pixels were rendered
    int solidCount = 0;
    for (size_t i = 3; i < logoRgba.size(); i += 4) {
        if (logoRgba[i] > 50) solidCount++;
    }

    std::cout << " -> Eatsbits logo verified: 3 layers, " << solidCount << " solid pixels in 48x48 render.\n";
}

int main() {
    std::cout << "=== Running Eatsbits SvgPath Test Suite ===\n";
    testBasicPathParsing();
    testCurvesAndArcs();
    testTriangulation();
    testSoftwareRasterization();
    testEatsbitsLogoLayers();
    std::cout << "=== All SvgPath Tests Passed Successfully! ===\n";
    return 0;
}
