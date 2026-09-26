#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <vector>
#include <cstdlib>

#include "eatsbits/ui/canvas_renderer.hpp"
#include "eatsbits/audio/audio_engine.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/sequencer/step_sequencer.hpp"

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

int main() {
    std::cout << "=== Running Eatsbits Phase 4 UI Canvas & Visualizer Suite Tests ===" << std::endl;
    testCanvasDimensionsAndResize();
    testModulePlacementAndRackLayout();
    testCatenaryCableDroop();
    testScopeAndVuMeter();
    testSvgExport();
    testAnsiVisualizer();
    testEngineScopeIntegration();
    std::cout << "=== All Phase 4 UI Canvas Tests Passed! ===" << std::endl;
    return 0;
}
