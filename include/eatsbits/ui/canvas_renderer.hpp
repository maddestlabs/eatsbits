#ifndef EATS_CANVAS_RENDERER_HPP
#define EATS_CANVAS_RENDERER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cmath>
#include "../audio/graph/audio_graph.hpp"
#include "../sequencer/step_sequencer.hpp"

namespace eatsbits::ui {

struct Point2D {
    float x{0.0f};
    float y{0.0f};
};

struct CablePath {
    audio::NodeId srcNode{0};
    uint32_t srcPort{0};
    audio::NodeId dstNode{0};
    uint32_t dstPort{0};
    Point2D p0;
    Point2D cp0;
    Point2D cp1;
    Point2D p1;
    std::string colorHex{"#00e5ff"};
};

struct ModuleLayout {
    audio::NodeId id{0};
    std::string name{"Module"};
    std::string type{"generic"};
    float x{0.0f};
    float y{0.0f};
    float width{180.0f};
    float height{240.0f};
    std::vector<Point2D> inputJacks;
    std::vector<Point2D> outputJacks;
    std::vector<std::string> knobNames;
};

/**
 * High-Precision Vector Canvas & Visualizer Renderer.
 * Manages modular rack placement, catenary cable geometry,
 * oscilloscope trace generation, VU meters, and SVG/ANSI export.
 */
class CanvasRenderer {
public:
    CanvasRenderer(float canvasWidth = 1024.0f, float canvasHeight = 768.0f);

    void setCanvasSize(float width, float height) noexcept;
    [[nodiscard]] float getWidth() const noexcept { return canvasWidth_; }
    [[nodiscard]] float getHeight() const noexcept { return canvasHeight_; }

    // Layout & Geometry
    void updateRackLayout(const audio::AudioGraph& graph);
    [[nodiscard]] const std::vector<ModuleLayout>& getModules() const noexcept { return modules_; }
    [[nodiscard]] const std::vector<CablePath>& getCables() const noexcept { return cables_; }

    Point2D getJackPosition(audio::NodeId nodeId, uint32_t portIdx, bool isOutput) const noexcept;

    // Cable Physics calculation
    static CablePath computeCableCurve(Point2D from, Point2D to, const std::string& color = "#00e5ff") noexcept;

    // Visualizers
    void setScopeData(const float* samples, size_t count);
    void setVuMeter(float peakL, float peakR);

    // SVG Export
    [[nodiscard]] std::string exportToSvg(const audio::AudioGraph& graph,
                                         const sequencer::StepSequencer* sequencer = nullptr) const;
    bool saveSvgToFile(const std::string& filePath,
                       const audio::AudioGraph& graph,
                       const sequencer::StepSequencer* sequencer = nullptr) const;

    // ANSI Terminal Visualizer Frame (for interactive CLI display)
    [[nodiscard]] std::string renderAnsiVisualizer(const sequencer::StepSequencer* sequencer = nullptr) const;

private:
    float canvasWidth_{1024.0f};
    float canvasHeight_{768.0f};

    std::vector<ModuleLayout> modules_;
    std::vector<CablePath> cables_;

    std::vector<float> scopeSamples_;
    float peakL_{0.0f};
    float peakR_{0.0f};
    float peakHoldL_{0.0f};
    float peakHoldR_{0.0f};
};

} // namespace eatsbits::ui

#endif // EATS_CANVAS_RENDERER_HPP
