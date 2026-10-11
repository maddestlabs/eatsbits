#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <vector>
#include <cstdlib>

#include "eatsbits/ui/canvas_renderer.hpp"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/widgets/scrollable_area.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include "eatsbits/ui/theme.hpp"

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::cerr << "Assertion failed: (" #expr ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

using namespace eatsbits;
using namespace eatsbits::ui;
using namespace eatsbits::audio;
using namespace eatsbits::sequencer;

void testCanvasDimensionsAndResize() {
    std::cout << "[Test] Canvas dimensions and sizing..." << std::endl;
    CanvasRenderer renderer(1280.0f, 800.0f);
    REQUIRE(renderer.getWidth() == 1280.0f);
    REQUIRE(renderer.getHeight() == 800.0f);

    // Min clamping test
    renderer.setCanvasSize(300.0f, 200.0f);
    REQUIRE(renderer.getWidth() == 640.0f);
    REQUIRE(renderer.getHeight() == 480.0f);

    renderer.setCanvasSize(1024.0f, 768.0f);
    REQUIRE(renderer.getWidth() == 1024.0f);
    REQUIRE(renderer.getHeight() == 768.0f);
    std::cout << "  [PASS] Canvas dimensions test passed." << std::endl;
}

void testModulePlacementAndRackLayout() {
    std::cout << "[Test] Eurorack modular layout & jack mapping..." << std::endl;
    CanvasRenderer renderer(1024.0f, 768.0f);
    AudioGraph graph;

    auto tb = std::make_shared<Tb303Node>("Tb303");
    auto drums = std::make_shared<DrumKitNode>("Drums");
    auto delay = std::make_shared<DelayNode>("Delay");
    auto gain = std::make_shared<GainNode>("Master");

    NodeId tbId = graph.addNode(tb);
    NodeId drumId = graph.addNode(drums);
    NodeId delayId = graph.addNode(delay);
    NodeId gainId = graph.addNode(gain);

    graph.connect(tbId, 0, delayId, 0);
    graph.connect(delayId, 0, gainId, 0);
    graph.connect(drumId, 0, gainId, 0);

    renderer.updateRackLayout(graph);

    const auto& modules = renderer.getModules();
    REQUIRE(modules.size() == 4);

    for (const auto& m : modules) {
        REQUIRE(m.width == 180.0f);
        REQUIRE(m.height == 240.0f);
        REQUIRE(m.x >= 24.0f);
        REQUIRE(m.y >= 30.0f);
        if (m.name == "Tb303") {
            REQUIRE(m.type == "tb303");
            REQUIRE(m.knobNames.size() == 6);
            REQUIRE(m.outputJacks.size() == 1);
        } else if (m.name == "Drums") {
            REQUIRE(m.type == "drum_kit");
            REQUIRE(m.knobNames.size() == 4);
            REQUIRE(m.outputJacks.size() == 1);
        } else if (m.name == "Delay") {
            REQUIRE(m.type == "delay");
            REQUIRE(m.knobNames.size() == 3);
            REQUIRE(m.inputJacks.size() == 1);
            REQUIRE(m.outputJacks.size() == 1);
        } else if (m.name == "Master") {
            REQUIRE(m.type == "gain");
            REQUIRE(m.inputJacks.size() == 1);
        }
    }

    // Check jack coordinates
    Point2D tbOut = renderer.getJackPosition(tbId, 0, true);
    Point2D delayIn = renderer.getJackPosition(delayId, 0, false);
    REQUIRE(tbOut.x > 0.0f && tbOut.y > 0.0f);
    REQUIRE(delayIn.x > 0.0f && delayIn.y > 0.0f);

    std::cout << "  [PASS] Eurorack layout and jack mapping passed." << std::endl;
}

void testCatenaryCableDroop() {
    std::cout << "[Test] Catenary cable droop and physics..." << std::endl;
    Point2D from{50.0f, 100.0f};
    Point2D to{300.0f, 120.0f};

    CablePath cable = CanvasRenderer::computeCableCurve(from, to, "#39ff14");
    REQUIRE(cable.colorHex == "#39ff14");
    REQUIRE(cable.p0.x == 50.0f && cable.p0.y == 100.0f);
    REQUIRE(cable.p1.x == 300.0f && cable.p1.y == 120.0f);

    // Gravity sag must pull control points downward (higher Y in screen space)
    REQUIRE(cable.cp0.y > from.y);
    REQUIRE(cable.cp1.y > to.y);

    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float dist = std::hypot(dx, dy);
    float expectedSag = std::max(45.0f, 0.28f * dist);
    REQUIRE(std::abs((cable.cp0.y - from.y) - expectedSag) < 0.001f);

    std::cout << "  [PASS] Catenary cable droop passed." << std::endl;
}

void testScopeAndVuMeter() {
    std::cout << "[Test] Oscilloscope downsampling and VU meters..." << std::endl;
    CanvasRenderer renderer(1024.0f, 768.0f);

    std::vector<float> fakeAudio(128);
    for (size_t i = 0; i < 128; ++i) {
        fakeAudio[i] = std::sin(2.0f * 3.14159265f * static_cast<float>(i) / 32.0f);
    }
    renderer.setScopeData(fakeAudio.data(), fakeAudio.size());
    renderer.setVuMeter(0.75f, 0.88f);

    // Extreme/clamped values
    renderer.setVuMeter(-0.5f, 3.5f);
    // Should not crash or overflow

    std::cout << "  [PASS] Scope and VU meter handling passed." << std::endl;
}

void testSvgExport() {
    std::cout << "[Test] Vector SVG export..." << std::endl;
    CanvasRenderer renderer(1024.0f, 768.0f);
    AudioGraph graph;
    auto tb = std::make_shared<Tb303Node>("Tb303");
    auto gain = std::make_shared<GainNode>("Master");
    NodeId tbId = graph.addNode(tb);
    NodeId gId = graph.addNode(gain);
    graph.connect(tbId, 0, gId, 0);

    StepSequencer seq;
    seq.setBpm(130.0);
    renderer.updateRackLayout(graph);

    std::vector<float> scope(64, 0.25f);
    renderer.setScopeData(scope.data(), scope.size());
    renderer.setVuMeter(0.6f, 0.65f);

    std::string svg = renderer.exportToSvg(graph, &seq);

    // Verify SVG structure tags
    REQUIRE(svg.find("<?xml version=\"1.0\"") != std::string::npos);
    REQUIRE(svg.find("<svg") != std::string::npos);
    REQUIRE(svg.find("viewBox=\"0 0 1024 768\"") != std::string::npos);
    REQUIRE(svg.find("<defs>") != std::string::npos);
    REQUIRE(svg.find("id=\"bgGrad\"") != std::string::npos);
    REQUIRE(svg.find("id=\"module_" + std::to_string(tbId) + "\"") != std::string::npos);
    REQUIRE(svg.find("id=\"module_" + std::to_string(gId) + "\"") != std::string::npos);
    REQUIRE(svg.find("id=\"oscilloscope\"") != std::string::npos);
    REQUIRE(svg.find("id=\"vu_meter\"") != std::string::npos);
    REQUIRE(svg.find("id=\"step_sequencer\"") != std::string::npos);
    REQUIRE(svg.find("filter=\"url(#cableShadow)\"") != std::string::npos);
    REQUIRE(svg.find("</svg>") != std::string::npos);

    // Test file save
    bool saved = renderer.saveSvgToFile("test_rack_export.svg", graph, &seq);
    REQUIRE(saved);

    std::ifstream in("test_rack_export.svg");
    REQUIRE(in.is_open());
    std::string fileContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(fileContent.size() == svg.size());

    std::cout << "  [PASS] SVG vector export verified (" << svg.size() << " bytes)." << std::endl;
}

void testAnsiVisualizer() {
    std::cout << "[Test] ANSI terminal visualizer frame..." << std::endl;
    CanvasRenderer renderer(1024.0f, 768.0f);
    AudioGraph graph;
    auto drums = std::make_shared<DrumKitNode>("AnalogDrums");
    NodeId dId = graph.addNode(drums);
    auto gain = std::make_shared<GainNode>("MasterOut");
    NodeId gId = graph.addNode(gain);
    graph.connect(dId, 0, gId, 0);

    renderer.updateRackLayout(graph);

    StepSequencer seq;
    seq.setBpm(135.0);
    seq.setSwing(0.58);

    std::vector<float> scope(128, 0.4f);
    renderer.setScopeData(scope.data(), scope.size());
    renderer.setVuMeter(0.70f, 0.85f);

    std::string ansi = renderer.renderAnsiVisualizer(&seq);
    REQUIRE(!ansi.empty());
    REQUIRE(ansi.find("EATSBITS MODULAR HARDWARE RACK") != std::string::npos);
    REQUIRE(ansi.find("AnalogDrums") != std::string::npos);
    REQUIRE(ansi.find("MasterOut") != std::string::npos);
    REQUIRE(ansi.find("OSCILLOSCOPE") != std::string::npos);
    REQUIRE(ansi.find("VU L:") != std::string::npos);
    REQUIRE(ansi.find("VU R:") != std::string::npos);
    REQUIRE(ansi.find("STEP SEQUENCER") != std::string::npos);
    REQUIRE(ansi.find("135") != std::string::npos); // BPM

    std::cout << "  [PASS] ANSI terminal visualizer verified." << std::endl;
}

void testEngineScopeIntegration() {
    std::cout << "[Test] AudioEngine scope ringbuffer integration..." << std::endl;
    AudioEngine engine;
    AudioEngineConfig cfg{};
    cfg.sampleRate = 48000;
    cfg.bufferFrameSize = 128;

    engine.setupDefaultAcidGraph();
    engine.postNoteOn(36, 0.9f, false, true);

    float outL[128]{0};
    float outR[128]{0};
    engine.renderOfflineBlock(outL, outR, 128);

    float scopeBuf[128]{0};
    size_t count = engine.getScopeSamples(scopeBuf, 128);
    REQUIRE(count > 0);

    CanvasRenderer renderer;
    renderer.setScopeData(scopeBuf, count);

    MeterFeedback fb;
    if (engine.pollMeterFeedback(fb)) {
        renderer.setVuMeter(fb.peakLeft, fb.peakRight);
    }

    std::string svg = renderer.exportToSvg(engine.getGraph());
    REQUIRE(!svg.empty());
    std::cout << "  [PASS] Engine scope integration passed." << std::endl;
}

void testScissorScopeNestedClippingAndRaii() {
    std::cout << "[Test] BatchRenderer2D::ScissorScope nested clipping, RAII, move semantics, and ScrollableArea..." << std::endl;
    BatchRenderer2D renderer;

    REQUIRE(!renderer.hasScissor());
    REQUIRE(renderer.getScissorDepth() == 0);

    // 1. Single ScissorScope test
    {
        BatchRenderer2D::ScissorScope scope1(renderer, 10.0f, 20.0f, 100.0f, 200.0f);
        REQUIRE(scope1.isActive());
        REQUIRE(renderer.hasScissor());
        REQUIRE(renderer.getScissorDepth() == 1);
        Rect2D s1 = renderer.getActiveScissor();
        REQUIRE(s1.x == 10.0f && s1.y == 20.0f && s1.w == 100.0f && s1.h == 200.0f);
    }
    REQUIRE(!renderer.hasScissor());
    REQUIRE(renderer.getScissorDepth() == 0);

    // 2. Nested clipping rectangle calculation (intersection of parent & child)
    {
        // Parent: [50, 50, 200, 200]
        BatchRenderer2D::ScissorScope parent(renderer, 50.0f, 50.0f, 200.0f, 200.0f);
        REQUIRE(renderer.getScissorDepth() == 1);

        {
            // Child: [100, 100, 200, 200] -> intersection with [50, 50, 200, 200] is [100, 100, 150, 150]
            BatchRenderer2D::ScissorScope child(renderer, 100.0f, 100.0f, 200.0f, 200.0f);
            REQUIRE(renderer.getScissorDepth() == 2);
            Rect2D act = renderer.getActiveScissor();
            REQUIRE(act.x == 100.0f);
            REQUIRE(act.y == 100.0f);
            REQUIRE(act.w == 150.0f);
            REQUIRE(act.h == 150.0f);

            {
                // Grandchild: [0, 0, 50, 50] (completely outside intersection [100, 100, 150, 150])
                BatchRenderer2D::ScissorScope grandchild(renderer, 0.0f, 0.0f, 50.0f, 50.0f);
                REQUIRE(renderer.getScissorDepth() == 3);
                Rect2D actGc = renderer.getActiveScissor();
                REQUIRE(actGc.w == 0.0f);
                REQUIRE(actGc.h == 0.0f);
            }
            // After grandchild unrolls, child is restored
            REQUIRE(renderer.getScissorDepth() == 2);
            Rect2D restoredChild = renderer.getActiveScissor();
            REQUIRE(restoredChild.x == 100.0f);
            REQUIRE(restoredChild.y == 100.0f);
            REQUIRE(restoredChild.w == 150.0f);
            REQUIRE(restoredChild.h == 150.0f);
        }
        // After child unrolls, parent is restored
        REQUIRE(renderer.getScissorDepth() == 1);
        Rect2D restoredParent = renderer.getActiveScissor();
        REQUIRE(restoredParent.x == 50.0f);
        REQUIRE(restoredParent.y == 50.0f);
        REQUIRE(restoredParent.w == 200.0f);
        REQUIRE(restoredParent.h == 200.0f);
    }
    REQUIRE(!renderer.hasScissor());
    REQUIRE(renderer.getScissorDepth() == 0);

    // 3. Move semantics & early reset()
    {
        BatchRenderer2D::ScissorScope scopeA(renderer, 10.0f, 10.0f, 50.0f, 50.0f);
        REQUIRE(scopeA.isActive());
        REQUIRE(renderer.getScissorDepth() == 1);

        // Move constructor
        BatchRenderer2D::ScissorScope scopeB(std::move(scopeA));
        REQUIRE(!scopeA.isActive());
        REQUIRE(scopeB.isActive());
        REQUIRE(renderer.getScissorDepth() == 1);

        // Move assignment
        BatchRenderer2D::ScissorScope scopeC;
        scopeC = std::move(scopeB);
        REQUIRE(!scopeB.isActive());
        REQUIRE(scopeC.isActive());
        REQUIRE(renderer.getScissorDepth() == 1);

        // Early reset()
        scopeC.reset();
        REQUIRE(!scopeC.isActive());
        REQUIRE(renderer.getScissorDepth() == 0);
    }
    REQUIRE(renderer.getScissorDepth() == 0);

    // 4. ScrollableArea makeScissorScope integration
    {
        ScrollableArea scrollArea;
        scrollArea.setViewport(30.0f, 40.0f, 300.0f, 400.0f);
        {
            auto scrollScope = scrollArea.makeScissorScope(renderer);
            REQUIRE(scrollScope.isActive());
            REQUIRE(renderer.hasScissor());
            REQUIRE(renderer.getScissorDepth() == 1);
            Rect2D act = renderer.getActiveScissor();
            REQUIRE(act.x == 30.0f);
            REQUIRE(act.y == 40.0f);
            REQUIRE(act.w == 300.0f);
            REQUIRE(act.h == 400.0f);
        }
        REQUIRE(!renderer.hasScissor());
        REQUIRE(renderer.getScissorDepth() == 0);
    }

    // 5. BatchRenderer2D makeScissorScope helper
    {
        auto scope = renderer.makeScissorScope(Rect2D{12.0f, 34.0f, 56.0f, 78.0f});
        REQUIRE(scope.isActive());
        REQUIRE(renderer.getScissorDepth() == 1);
        Rect2D act = renderer.getActiveScissor();
        REQUIRE(act.x == 12.0f && act.y == 34.0f && act.w == 56.0f && act.h == 78.0f);
    }
    REQUIRE(renderer.getScissorDepth() == 0);

    std::cout << "  [PASS] ScissorScope nested clipping and RAII passed." << std::endl;
}

void testTooltipDomainClampingAndAutoFlipping() {
    std::cout << "[Test] Tooltip domain optical separation, top panel element positioning, and auto-flipping..." << std::endl;

    BatchRenderer2D renderer;
    renderer.initialize(nullptr, 1280, 800);
    ThemeTokens theme = Theme::current();

    // 1. Top Transport Bar domain: tooltip must render ABOVE elements and strictly inside the top panel (y in [0, 56])
    {
        renderer.beginFrame(1280.0f, 800.0f);
        size_t vStart = renderer.getVertexCount();

        // Hover Play button: targetY = 10.0f, placeBelow = false
        drawTooltipBadge(renderer, "Play / Pause (Space)", 73.0f, 10.0f, theme, false, 1280.0f, 800.0f, 0.0f, 54.0f);

        size_t vEnd = renderer.getVertexCount();
        REQUIRE(vEnd > vStart);

        const auto& verts = renderer.getVertices();
        float minY = 9999.0f;
        float maxY = -9999.0f;
        for (size_t i = vStart; i < vEnd; ++i) {
            minY = std::min(minY, verts[i].y);
            maxY = std::max(maxY, verts[i].y);
        }

        // Must sit strictly above elements inside top panel: maxY <= 54.0f and minY >= 2.0f
        REQUIRE(minY >= 2.0f);
        REQUIRE(maxY <= 54.0f);
        renderer.endFrame();
    }

    // 2. Central DAW Workspace domain near top seam: must auto-flip downwards so it NEVER bleeds into top transport bar (y < 56)
    {
        renderer.beginFrame(1280.0f, 800.0f);
        size_t vStart = renderer.getVertexCount();

        // Control near top of track/mixer: targetY = 65.0f, placeBelow = false
        // Normally targetY - th - 8 = 37px which would breach the 56px metal faceplate!
        drawTooltipBadge(renderer, "Master Fader: 0.0 dB", 200.0f, 65.0f, theme, false, 1280.0f, 800.0f);

        size_t vEnd = renderer.getVertexCount();
        REQUIRE(vEnd > vStart);

        const auto& verts = renderer.getVertices();
        float minY = 9999.0f;
        float maxY = -9999.0f;
        for (size_t i = vStart; i < vEnd; ++i) {
            minY = std::min(minY, verts[i].y);
            maxY = std::max(maxY, verts[i].y);
        }

        // Must auto-flip below target so minY >= 58.0f (strictly inside CRT workspace cavity)
        REQUIRE(minY >= 58.0f);
        renderer.endFrame();
    }

    // 3. Central DAW Workspace domain near bottom chin: must auto-flip upwards so it NEVER bleeds into bottom chin (y > 752)
    {
        renderer.beginFrame(1280.0f, 800.0f);
        size_t vStart = renderer.getVertexCount();

        // Control near bottom chin: targetY = 740.0f, placeBelow = true
        // Normally targetY + 14 + 20 = 774px which would breach bottom chin (752px)!
        drawTooltipBadge(renderer, "Pan: Center", 300.0f, 740.0f, theme, true, 1280.0f, 800.0f);

        size_t vEnd = renderer.getVertexCount();
        REQUIRE(vEnd > vStart);

        const auto& verts = renderer.getVertices();
        float minY = 9999.0f;
        float maxY = -9999.0f;
        for (size_t i = vStart; i < vEnd; ++i) {
            minY = std::min(minY, verts[i].y);
            maxY = std::max(maxY, verts[i].y);
        }

        // Must auto-flip above target so maxY <= 749.0f (strictly above 752px bottom chin)
        REQUIRE(maxY <= 749.0f);
        REQUIRE(minY >= 58.0f);
        renderer.endFrame();
    }

    std::cout << "  [PASS] Tooltip domain optical separation and auto-flipping passed." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Phase 4 UI Canvas & Visualizer Suite Tests ===" << std::endl;
    testCanvasDimensionsAndResize();
    testModulePlacementAndRackLayout();
    testCatenaryCableDroop();
    testScopeAndVuMeter();
    testSvgExport();
    testAnsiVisualizer();
    testEngineScopeIntegration();
    testScissorScopeNestedClippingAndRaii();
    testTooltipDomainClampingAndAutoFlipping();
    std::cout << "=== All Phase 4 UI Canvas Tests Passed! ===" << std::endl;
    return 0;
}
