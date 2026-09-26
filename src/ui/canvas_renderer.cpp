#include "eatsbits/ui/canvas_renderer.hpp"
#include "eatsbits/audio/graph/nodes/tb303_node.hpp"
#include "eatsbits/audio/graph/nodes/drum_kit_node.hpp"
#include "eatsbits/audio/graph/nodes/poly_synth_node.hpp"
#include "eatsbits/audio/graph/nodes/biquad_node.hpp"
#include "eatsbits/audio/graph/nodes/delay_node.hpp"
#include "eatsbits/audio/graph/nodes/gain_node.hpp"
#include "eatsbits/audio/graph/nodes/eatscript_node.hpp"
#include "eatsbits/audio/graph/nodes/sid_node.hpp"
#include "eatsbits/audio/graph/nodes/dx7_node.hpp"
#include "eatsbits/audio/graph/nodes/snes_node.hpp"
#include "eatsbits/audio/graph/nodes/ym2612_node.hpp"
#include "eatsbits/audio/graph/nodes/convolver_node.hpp"

#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace eatsbits::ui {

CanvasRenderer::CanvasRenderer(float canvasWidth, float canvasHeight)
    : canvasWidth_(canvasWidth), canvasHeight_(canvasHeight) {
    scopeSamples_.assign(128, 0.0f);
}

void CanvasRenderer::setCanvasSize(float width, float height) noexcept {
    canvasWidth_ = std::max(640.0f, width);
    canvasHeight_ = std::max(480.0f, height);
}

void CanvasRenderer::updateRackLayout(const audio::AudioGraph& graph) {
    modules_.clear();
    cables_.clear();

    const float modWidth = 180.0f;
    const float modHeight = 240.0f;
    const float paddingX = 24.0f;
    const float paddingY = 24.0f;
    const float startX = 24.0f;
    const float startY = 76.0f;

    const auto& nodes = graph.getNodes();
    int col = 0;
    int row = 0;
    const int maxCols = std::max(1, static_cast<int>((canvasWidth_ - startX) / (modWidth + paddingX)));

    // 1. Position Modules
    for (const auto& [id, node] : nodes) {
        if (!node) continue;

        ModuleLayout ml;
        ml.id = id;
        ml.name = node->getName();
        ml.width = modWidth;
        ml.height = modHeight;
        ml.x = startX + static_cast<float>(col) * (modWidth + paddingX);
        ml.y = startY + static_cast<float>(row) * (modHeight + paddingY);

        // Classify module & knobs
        if (dynamic_cast<audio::Tb303Node*>(node.get())) {
            ml.type = "tb303";
            ml.knobNames = {"Cutoff", "Res", "EnvMod", "Decay", "Accent", "Wave"};
        } else if (dynamic_cast<audio::DrumKitNode*>(node.get())) {
            ml.type = "drum_kit";
            ml.knobNames = {"Master", "Tune", "Decay", "Overdrive"};
        } else if (dynamic_cast<audio::PolySynthNode*>(node.get())) {
            ml.type = "poly_synth";
            ml.knobNames = {"Cutoff", "Res", "Attack", "Release"};
        } else if (dynamic_cast<audio::BiquadNode*>(node.get())) {
            ml.type = "biquad";
            ml.knobNames = {"Freq", "Q"};
        } else if (dynamic_cast<audio::DelayNode*>(node.get())) {
            ml.type = "delay";
            ml.knobNames = {"Time", "Feedback", "Mix"};
        } else if (dynamic_cast<audio::GainNode*>(node.get())) {
            ml.type = "gain";
            ml.knobNames = {"Volume", "Pan"};
        } else if (dynamic_cast<audio::EatscriptNode*>(node.get())) {
            ml.type = "eatscript";
            ml.knobNames = {"P0", "P1", "P2", "P3"};
        } else if (dynamic_cast<audio::SidNode*>(node.get())) {
            ml.type = "sid";
            ml.knobNames = {"Wave", "PW", "Cutoff", "Res", "Atk", "Rel"};
        } else if (dynamic_cast<audio::Dx7Node*>(node.get())) {
            ml.type = "dx7";
            ml.knobNames = {"Algo", "Fdbk", "Bright", "Tine", "Warmth", "Vol"};
        } else if (dynamic_cast<audio::SnesNode*>(node.get())) {
            ml.type = "snes";
            ml.knobNames = {"Wave", "Atk", "Dec", "Sus", "Rel", "Echo", "Vol"};
        } else if (dynamic_cast<audio::Ym2612Node*>(node.get())) {
            ml.type = "ym2612";
            ml.knobNames = {"Algo", "Fdbk", "Op1", "Op2", "Op3", "Vol"};
        } else if (dynamic_cast<audio::ConvolverNode*>(node.get())) {
            ml.type = "convolver";
            ml.knobNames = {"Space", "PreDly", "Decay", "HiCut", "LoCut", "Mix"};
        } else {
            ml.type = "generic";
            ml.knobNames = {"Param1", "Param2"};
        }

        // Place Jacks
        const uint32_t numIn = node->numInputPorts();
        for (uint32_t i = 0; i < numIn; ++i) {
            ml.inputJacks.push_back(Point2D{ml.x + 30.0f + (i * 35.0f), ml.y + modHeight - 32.0f});
        }

        const uint32_t numOut = node->numOutputPorts();
        for (uint32_t i = 0; i < numOut; ++i) {
            ml.outputJacks.push_back(Point2D{ml.x + modWidth - 30.0f - (i * 35.0f), ml.y + modHeight - 32.0f});
        }

        modules_.push_back(std::move(ml));

        if (++col >= maxCols) {
            col = 0;
            row++;
        }
    }

    // 2. Position Cables
    for (const auto& conn : graph.getConnections()) {
        Point2D p0 = getJackPosition(conn.srcNode, conn.srcPort, true);
        Point2D p1 = getJackPosition(conn.dstNode, conn.dstPort, false);

        std::string color = "#00e5ff"; // Default cyan
        for (const auto& m : modules_) {
            if (m.id == conn.srcNode) {
                if (m.type == "tb303") color = "#39ff14";       // Acid Green
                else if (m.type == "drum_kit") color = "#ff5500"; // 808 Orange
                else if (m.type == "delay") color = "#ff007f";    // Hot Pink
                else if (m.type == "eatscript") color = "#b400ff";// Electric Purple
                else if (m.type == "gain") color = "#00e5ff";     // Master Cyan
                break;
            }
        }

        CablePath cp = computeCableCurve(p0, p1, color);
        cp.srcNode = conn.srcNode;
        cp.srcPort = conn.srcPort;
        cp.dstNode = conn.dstNode;
        cp.dstPort = conn.dstPort;
        cables_.push_back(std::move(cp));
    }
}

Point2D CanvasRenderer::getJackPosition(audio::NodeId nodeId, uint32_t portIdx, bool isOutput) const noexcept {
    for (const auto& m : modules_) {
        if (m.id == nodeId) {
            if (isOutput) {
                if (portIdx < m.outputJacks.size()) return m.outputJacks[portIdx];
                return Point2D{m.x + m.width - 30.0f, m.y + m.height - 32.0f};
            } else {
                if (portIdx < m.inputJacks.size()) return m.inputJacks[portIdx];
                return Point2D{m.x + 30.0f, m.y + m.height - 32.0f};
            }
        }
    }
    return Point2D{0.0f, 0.0f};
}

CablePath CanvasRenderer::computeCableCurve(Point2D from, Point2D to, const std::string& color) noexcept {
    CablePath cp;
    cp.p0 = from;
    cp.p1 = to;
    cp.colorHex = color;

    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dist = std::hypot(dx, dy);

    // Catenary gravity droop calculation
    const float sag = std::max(45.0f, 0.28f * dist);

    cp.cp0.x = from.x + (dx * 0.15f);
    cp.cp0.y = from.y + sag;

    cp.cp1.x = to.x - (dx * 0.15f);
    cp.cp1.y = to.y + sag;

    return cp;
}

void CanvasRenderer::setScopeData(const float* samples, size_t count) {
    if (!samples || count == 0) return;
    scopeSamples_.resize(count);
    std::copy_n(samples, count, scopeSamples_.begin());
}

void CanvasRenderer::setVuMeter(float peakL, float peakR) {
    peakL_ = std::clamp(peakL, 0.0f, 2.0f);
    peakR_ = std::clamp(peakR, 0.0f, 2.0f);

    peakHoldL_ = std::max(peakL_, peakHoldL_ * 0.95f);
    peakHoldR_ = std::max(peakR_, peakHoldR_ * 0.95f);
}

std::string CanvasRenderer::exportToSvg(const audio::AudioGraph& /*graph*/,
                                       const sequencer::StepSequencer* sequencer) const {
    std::ostringstream ss;

    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "
       << canvasWidth_ << " " << canvasHeight_ << "\" width=\"" << canvasWidth_
       << "\" height=\"" << canvasHeight_ << "\">\n";

    // Styles & Filters
    ss << "<defs>\n";
    ss << "  <linearGradient id=\"bgGrad\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">\n";
    ss << "    <stop offset=\"0%\" stop-color=\"#0a0c12\"/>\n";
    ss << "    <stop offset=\"100%\" stop-color=\"#131722\"/>\n";
    ss << "  </linearGradient>\n";
    ss << "  <linearGradient id=\"moduleGrad\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">\n";
    ss << "    <stop offset=\"0%\" stop-color=\"#222736\"/>\n";
    ss << "    <stop offset=\"100%\" stop-color=\"#181b24\"/>\n";
    ss << "  </linearGradient>\n";
    ss << "  <filter id=\"glow\" x=\"-20%\" y=\"-20%\" width=\"140%\" height=\"140%\">\n";
    ss << "    <feGaussianBlur stdDeviation=\"4\" result=\"blur\"/>\n";
    ss << "    <feMerge><feMergeNode in=\"blur\"/><feMergeNode in=\"SourceGraphic\"/></feMerge>\n";
    ss << "  </filter>\n";
    ss << "  <filter id=\"cableShadow\" x=\"-20%\" y=\"-20%\" width=\"140%\" height=\"140%\">\n";
    ss << "    <feDropShadow dx=\"0\" dy=\"12\" stdDeviation=\"6\" flood-color=\"#000000\" flood-opacity=\"0.75\"/>\n";
    ss << "  </filter>\n";
    ss << "</defs>\n\n";

    // Background Canvas
    ss << "<rect width=\"100%\" height=\"100%\" fill=\"url(#bgGrad)\"/>\n";

    // Rack Rails
    ss << "<rect x=\"0\" y=\"16\" width=\"100%\" height=\"6\" fill=\"#2d3345\"/>\n";
    ss << "<rect x=\"0\" y=\"" << (canvasHeight_ - 22) << "\" width=\"100%\" height=\"6\" fill=\"#2d3345\"/>\n";

    // Modules
    for (const auto& m : modules_) {
        // Module Chassis
        ss << "<g id=\"module_" << m.id << "\">\n";
        ss << "  <rect x=\"" << m.x << "\" y=\"" << m.y << "\" width=\"" << m.width
           << "\" height=\"" << m.height << "\" rx=\"6\" fill=\"url(#moduleGrad)\" stroke=\"#3a4258\" stroke-width=\"1.5\"/>\n";

        // Faceplate Screws
        ss << "  <circle cx=\"" << (m.x + 8) << "\" cy=\"" << (m.y + 8) << "\" r=\"2.5\" fill=\"#4a546e\"/>\n";
        ss << "  <circle cx=\"" << (m.x + m.width - 8) << "\" cy=\"" << (m.y + 8) << "\" r=\"2.5\" fill=\"#4a546e\"/>\n";
        ss << "  <circle cx=\"" << (m.x + 8) << "\" cy=\"" << (m.y + m.height - 8) << "\" r=\"2.5\" fill=\"#4a546e\"/>\n";
        ss << "  <circle cx=\"" << (m.x + m.width - 8) << "\" cy=\"" << (m.y + m.height - 8) << "\" r=\"2.5\" fill=\"#4a546e\"/>\n";

        // Header Bar
        ss << "  <rect x=\"" << m.x << "\" y=\"" << m.y << "\" width=\"" << m.width << "\" height=\"28\" rx=\"6\" fill=\"#1b1f2b\"/>\n";
        ss << "  <text x=\"" << (m.x + 12) << "\" y=\"" << (m.y + 19) << "\" font-family=\"monospace\" font-size=\"12\" font-weight=\"bold\" fill=\"#00e5ff\">"
           << m.name << "</text>\n";

        // Knobs Grid
        for (size_t k = 0; k < m.knobNames.size(); ++k) {
            float kx = m.x + 36.0f + ((k % 2) * 90.0f);
            float ky = m.y + 56.0f + ((k / 2) * 52.0f);

            ss << "  <circle cx=\"" << kx << "\" cy=\"" << ky << "\" r=\"15\" fill=\"#12141c\" stroke=\"#4a546e\" stroke-width=\"1.5\"/>\n";
            ss << "  <line x1=\"" << kx << "\" y1=\"" << (ky - 6) << "\" x2=\"" << kx << "\" y2=\"" << (ky - 14) << "\" stroke=\"#00e5ff\" stroke-width=\"2\" stroke-linecap=\"round\"/>\n";
            ss << "  <text x=\"" << kx << "\" y=\"" << (ky + 23) << "\" font-family=\"sans-serif\" font-size=\"9\" fill=\"#8b9bb4\" text-anchor=\"middle\">"
               << m.knobNames[k] << "</text>\n";
        }

        // Input Jacks
        for (size_t i = 0; i < m.inputJacks.size(); ++i) {
            const auto& pt = m.inputJacks[i];
            ss << "  <circle cx=\"" << pt.x << "\" cy=\"" << pt.y << "\" r=\"9\" fill=\"#12141c\" stroke=\"#00e5ff\" stroke-width=\"2\"/>\n";
            ss << "  <circle cx=\"" << pt.x << "\" cy=\"" << pt.y << "\" r=\"3.5\" fill=\"#000000\"/>\n";
            ss << "  <text x=\"" << pt.x << "\" y=\"" << (pt.y - 12) << "\" font-family=\"sans-serif\" font-size=\"8\" fill=\"#00e5ff\" text-anchor=\"middle\">IN</text>\n";
        }

        // Output Jacks
        for (size_t i = 0; i < m.outputJacks.size(); ++i) {
            const auto& pt = m.outputJacks[i];
            ss << "  <circle cx=\"" << pt.x << "\" cy=\"" << pt.y << "\" r=\"9\" fill=\"#12141c\" stroke=\"#ff5500\" stroke-width=\"2\"/>\n";
            ss << "  <circle cx=\"" << pt.x << "\" cy=\"" << pt.y << "\" r=\"3.5\" fill=\"#000000\"/>\n";
            ss << "  <text x=\"" << pt.x << "\" y=\"" << (pt.y - 12) << "\" font-family=\"sans-serif\" font-size=\"8\" fill=\"#ff5500\" text-anchor=\"middle\">OUT</text>\n";
        }

        ss << "</g>\n";
    }

    // Oscilloscope Screen (Bottom Panel)
    const float scopeX = 30.0f;
    const float scopeY = canvasHeight_ - 180.0f;
    const float scopeW = 280.0f;
    const float scopeH = 130.0f;

    ss << "<g id=\"oscilloscope\">\n";
    ss << "  <rect x=\"" << scopeX << "\" y=\"" << scopeY << "\" width=\"" << scopeW << "\" height=\"" << scopeH << "\" rx=\"4\" fill=\"#05080c\" stroke=\"#223240\" stroke-width=\"2\"/>\n";
    // Scope Grid Lines
    for (int gx = 1; gx < 6; ++gx) {
        float x = scopeX + (gx * (scopeW / 6.0f));
        ss << "  <line x1=\"" << x << "\" y1=\"" << scopeY << "\" x2=\"" << x << "\" y2=\"" << (scopeY + scopeH) << "\" stroke=\"#0f2228\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n";
    }
    ss << "  <line x1=\"" << scopeX << "\" y1=\"" << (scopeY + scopeH * 0.5f) << "\" x2=\"" << (scopeX + scopeW) << "\" y2=\"" << (scopeY + scopeH * 0.5f) << "\" stroke=\"#0f2228\" stroke-width=\"1\"/>\n";

    // Trace line
    if (!scopeSamples_.empty()) {
        ss << "  <path d=\"M " << scopeX << " " << (scopeY + scopeH * 0.5f);
        for (size_t s = 0; s < scopeSamples_.size(); ++s) {
            float px = scopeX + (static_cast<float>(s) / (scopeSamples_.size() - 1)) * scopeW;
            float py = (scopeY + scopeH * 0.5f) - (scopeSamples_[s] * (scopeH * 0.42f));
            ss << " L " << px << " " << py;
        }
        ss << "\" fill=\"none\" stroke=\"#00ffc8\" stroke-width=\"2\" filter=\"url(#glow)\"/>\n";
    }
    ss << "  <text x=\"" << (scopeX + 8) << "\" y=\"" << (scopeY + 16) << "\" font-family=\"monospace\" font-size=\"10\" fill=\"#00ffc8\">OSCILLOSCOPE 48kHz</text>\n";
    ss << "</g>\n";

    // VU Meters (Right of Scope)
    const float vuX = scopeX + scopeW + 20.0f;
    const float vuY = scopeY;
    const float vuW = 50.0f;
    const float vuH = scopeH;

    ss << "<g id=\"vu_meter\">\n";
    ss << "  <rect x=\"" << vuX << "\" y=\"" << vuY << "\" width=\"" << vuW << "\" height=\"" << vuH << "\" rx=\"4\" fill=\"#0a0c10\" stroke=\"#222838\" stroke-width=\"1\"/>\n";
    // Meter bars L/R
    float barHL = std::clamp(peakL_ * (vuH - 24.0f), 0.0f, vuH - 24.0f);
    float barHR = std::clamp(peakR_ * (vuH - 24.0f), 0.0f, vuH - 24.0f);

    ss << "  <rect x=\"" << (vuX + 10) << "\" y=\"" << (vuY + vuH - 12 - barHL) << "\" width=\"12\" height=\"" << barHL << "\" fill=\"#39ff14\"/>\n";
    ss << "  <rect x=\"" << (vuX + 28) << "\" y=\"" << (vuY + vuH - 12 - barHR) << "\" width=\"12\" height=\"" << barHR << "\" fill=\"#39ff14\"/>\n";
    ss << "  <text x=\"" << (vuX + 16) << "\" y=\"" << (vuY + 14) << "\" font-family=\"sans-serif\" font-size=\"8\" fill=\"#7a889b\" text-anchor=\"middle\">L</text>\n";
    ss << "  <text x=\"" << (vuX + 34) << "\" y=\"" << (vuY + 14) << "\" font-family=\"sans-serif\" font-size=\"8\" fill=\"#7a889b\" text-anchor=\"middle\">R</text>\n";
    ss << "</g>\n";

    // Sequencer 16-Step Grid (Right of VU)
    if (sequencer) {
        const float seqX = vuX + vuW + 20.0f;
        const float seqY = scopeY;
        const float seqW = canvasWidth_ - seqX - 30.0f;
        const float seqH = scopeH;

        ss << "<g id=\"step_sequencer\">\n";
        ss << "  <rect x=\"" << seqX << "\" y=\"" << seqY << "\" width=\"" << seqW << "\" height=\"" << seqH << "\" rx=\"4\" fill=\"#0d111a\" stroke=\"#242c3d\" stroke-width=\"1\"/>\n";
        ss << "  <text x=\"" << (seqX + 12) << "\" y=\"" << (seqY + 18) << "\" font-family=\"monospace\" font-size=\"11\" fill=\"#ffd700\" font-weight=\"bold\">TRACKER STEP SEQUENCER (16 STEPS)</text>\n";

        uint32_t currentStep = sequencer->getTransport().getCurrentStep();
        float stepW = (seqW - 24.0f) / 16.0f;

        for (uint32_t s = 0; s < 16; ++s) {
            float sx = seqX + 12.0f + (s * stepW);
            float sy = seqY + 40.0f;
            bool isCurrent = (s == currentStep);

            // Step Pad Background
            ss << "  <rect x=\"" << sx << "\" y=\"" << sy << "\" width=\"" << (stepW - 4.0f) << "\" height=\"60\" rx=\"3\" fill=\""
               << (isCurrent ? "#2a4263" : ((s % 4 == 0) ? "#1f2638" : "#141926")) << "\" stroke=\""
               << (isCurrent ? "#00f0ff" : "#323c52") << "\" stroke-width=\"1.5\"/>\n";

            // LED Indicator
            ss << "  <circle cx=\"" << (sx + (stepW - 4.0f) * 0.5f) << "\" cy=\"" << (sy + 10.0f) << "\" r=\"3.5\" fill=\""
               << (isCurrent ? "#00ffff" : "#44526b") << "\""
               << (isCurrent ? " filter=\"url(#glow)\"" : "") << "/>\n";

            ss << "  <text x=\"" << (sx + (stepW - 4.0f) * 0.5f) << "\" y=\"" << (sy + 45.0f) << "\" font-family=\"monospace\" font-size=\"9\" fill=\""
               << (isCurrent ? "#ffffff" : "#6c7d99") << "\" text-anchor=\"middle\">" << (s + 1) << "</text>\n";
        }
        ss << "</g>\n";
    }

    // Physical Patch Cables (Rendered on top with drop shadow and glow)
    for (const auto& c : cables_) {
        ss << "<g filter=\"url(#cableShadow)\">\n";
        // Cable Base Shadow
        ss << "  <path d=\"M " << c.p0.x << " " << c.p0.y << " C "
           << c.cp0.x << " " << c.cp0.y << ", " << c.cp1.x << " " << c.cp1.y << ", "
           << c.p1.x << " " << c.p1.y << "\" fill=\"none\" stroke=\"#000000\" stroke-width=\"7\" opacity=\"0.6\"/>\n";

        // Cable Jacket Core
        ss << "  <path d=\"M " << c.p0.x << " " << c.p0.y << " C "
           << c.cp0.x << " " << c.cp0.y << ", " << c.cp1.x << " " << c.cp1.y << ", "
           << c.p1.x << " " << c.p1.y << "\" fill=\"none\" stroke=\"" << c.colorHex << "\" stroke-width=\"4.5\" stroke-linecap=\"round\" filter=\"url(#glow)\"/>\n";

        // Cable Highlight Strip
        ss << "  <path d=\"M " << c.p0.x << " " << c.p0.y << " C "
           << c.cp0.x << " " << c.cp0.y << ", " << c.cp1.x << " " << c.cp1.y << ", "
           << c.p1.x << " " << c.p1.y << "\" fill=\"none\" stroke=\"#ffffff\" stroke-width=\"1.5\" opacity=\"0.65\"/>\n";

        // Jack plugs on both ends
        ss << "  <circle cx=\"" << c.p0.x << "\" cy=\"" << c.p0.y << "\" r=\"6\" fill=\"#1c212d\" stroke=\"" << c.colorHex << "\" stroke-width=\"2\"/>\n";
        ss << "  <circle cx=\"" << c.p1.x << "\" cy=\"" << c.p1.y << "\" r=\"6\" fill=\"#1c212d\" stroke=\"" << c.colorHex << "\" stroke-width=\"2\"/>\n";
        ss << "</g>\n";
    }

    ss << "</svg>\n";
    return ss.str();
}

bool CanvasRenderer::saveSvgToFile(const std::string& filePath,
                                  const audio::AudioGraph& graph,
                                  const sequencer::StepSequencer* sequencer) const {
    std::string svg = exportToSvg(graph, sequencer);
    std::ofstream out(filePath);
    if (!out.is_open()) return false;
    out << svg;
    return true;
}

std::string CanvasRenderer::renderAnsiVisualizer(const sequencer::StepSequencer* sequencer) const {
    std::ostringstream ss;

    // ANSI Box Drawing visualizer
    ss << "\033[1;36m+=============================================================================+\033[0m\n";
    ss << "\033[1;36m|                     EATSBITS MODULAR HARDWARE RACK                          |\033[0m\n";
    ss << "\033[1;36m+=============================================================================+\033[0m\n";

    // Modules summary
    ss << "| Active Modules (" << modules_.size() << " units patched):\n";
    for (const auto& m : modules_) {
        ss << "|   * [" << std::left << std::setw(14) << m.name << "] type: "
           << std::setw(10) << m.type << " | In: " << m.inputJacks.size() << "  Out: " << m.outputJacks.size() << "\n";
    }

    // Cables summary
    ss << "| Patch Cables (" << cables_.size() << " connected):\n";
    for (const auto& c : cables_) {
        ss << "|   * Node " << c.srcNode << ":" << c.srcPort << " ===(" << c.colorHex << ")===> Node "
           << c.dstNode << ":" << c.dstPort << "\n";
    }

    // Oscilloscope in ASCII
    ss << "+-----------------------------------------------------------------------------+\n";
    ss << "| OSCILLOSCOPE (Stereo Audio Bus):\n";
    const int scopeWidth = 50;
    std::string scopeLine(scopeWidth, ' ');
    if (!scopeSamples_.empty()) {
        for (int x = 0; x < scopeWidth; ++x) {
            size_t idx = (x * scopeSamples_.size()) / scopeWidth;
            float val = scopeSamples_[idx];
            if (std::abs(val) > 0.05f) {
                scopeLine[x] = (val > 0.5f) ? '^' : (val < -0.5f ? 'v' : '~');
            } else {
                scopeLine[x] = '-';
            }
        }
    }
    ss << "|   [" << "\033[1;32m" << scopeLine << "\033[0m" << "]\n";

    // VU Meters
    int barL = static_cast<int>(std::clamp(peakL_ * 20.0f, 0.0f, 20.0f));
    int barR = static_cast<int>(std::clamp(peakR_ * 20.0f, 0.0f, 20.0f));
    std::string strL(barL, '#'); strL.resize(20, '.');
    std::string strR(barR, '#'); strR.resize(20, '.');

    ss << "| VU L: [" << "\033[1;33m" << strL << "\033[0m" << "] " << std::fixed << std::setprecision(2) << peakL_ << " | "
       << "VU R: [" << "\033[1;33m" << strR << "\033[0m" << "] " << std::fixed << std::setprecision(2) << peakR_ << "\n";

    // Step Sequencer status
    if (sequencer) {
        ss << "+-----------------------------------------------------------------------------+\n";
        uint32_t step = sequencer->getTransport().getCurrentStep();
        ss << "| STEP SEQUENCER [Step " << std::setw(2) << (step + 1) << "/16 | "
           << sequencer->getBpm() << " BPM | " << static_cast<int>(sequencer->getSwing() * 100.0) << "% Swing]:\n";
        ss << "|   ";
        for (uint32_t s = 0; s < 16; ++s) {
            if (s == step) {
                ss << "\033[1;37;44m[" << std::setw(2) << (s + 1) << "]\033[0m ";
            } else if (s % 4 == 0) {
                ss << "\033[1;33m(" << std::setw(2) << (s + 1) << ")\033[0m ";
            } else {
                ss << std::setw(2) << (s + 1) << "  ";
            }
        }
        ss << "\n";
    }

    ss << "\033[1;36m+=============================================================================+\033[0m\n";
    return ss.str();
}

} // namespace eatsbits::ui
