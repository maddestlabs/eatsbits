#ifndef EATS_GRAPH_NODE_HPP
#define EATS_GRAPH_NODE_HPP

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include "../ringbuffer.hpp"

namespace eatsbits::audio {

using NodeId = uint32_t;
constexpr NodeId INVALID_NODE_ID = 0;

enum class PortDirection {
    Input,
    Output
};

struct NodePort {
    uint32_t portIndex{0};
    uint32_t numChannels{2}; // Default stereo (L/R)
    // Pointers to the active audio memory buffers for each channel (up to 2 channels per port)
    std::array<float*, 2> channels{nullptr, nullptr};
    std::array<const float*, 2> inChannels{nullptr, nullptr};
};

/**
 * Base abstract class for modular audio nodes inside AudioGraph.
 * All processBlock() operations guarantee strict real-time safety with ZERO allocations.
 */
class GraphNode {
public:
    explicit GraphNode(std::string name = "GraphNode")
        : name_(std::move(name)) {}

    virtual ~GraphNode() = default;

    [[nodiscard]] NodeId getId() const noexcept { return id_; }
    void setId(NodeId id) noexcept { id_ = id; }

    [[nodiscard]] const std::string& getName() const noexcept { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    // Port counts
    [[nodiscard]] uint32_t numInputPorts() const noexcept { return static_cast<uint32_t>(inputs_.size()); }
    [[nodiscard]] uint32_t numOutputPorts() const noexcept { return static_cast<uint32_t>(outputs_.size()); }

    [[nodiscard]] const NodePort& getInputPort(uint32_t portIdx) const noexcept {
        return inputs_[portIdx];
    }
    [[nodiscard]] NodePort& getInputPort(uint32_t portIdx) noexcept {
        return inputs_[portIdx];
    }

    [[nodiscard]] const NodePort& getOutputPort(uint32_t portIdx) const noexcept {
        return outputs_[portIdx];
    }
    [[nodiscard]] NodePort& getOutputPort(uint32_t portIdx) noexcept {
        return outputs_[portIdx];
    }

    // Buffer access helpers for derived nodes inside processBlock()
    [[nodiscard]] const float* getInputBuffer(uint32_t portIdx, uint32_t channel = 0) const noexcept {
        if (portIdx >= inputs_.size() || channel >= 2) return nullptr;
        return inputs_[portIdx].inChannels[channel];
    }

    [[nodiscard]] float* getOutputBuffer(uint32_t portIdx, uint32_t channel = 0) noexcept {
        if (portIdx >= outputs_.size() || channel >= 2) return nullptr;
        return outputs_[portIdx].channels[channel];
    }

    // Evaluator setup methods
    void setInputBufferPtr(uint32_t portIdx, uint32_t channel, const float* buffer) noexcept {
        if (portIdx < inputs_.size() && channel < 2) {
            inputs_[portIdx].inChannels[channel] = buffer;
        }
    }

    void setOutputBufferPtr(uint32_t portIdx, uint32_t channel, float* buffer) noexcept {
        if (portIdx < outputs_.size() && channel < 2) {
            outputs_[portIdx].channels[channel] = buffer;
        }
    }

    // Lifecycle
    virtual void prepare(double sampleRate, uint32_t maxBlockSize) = 0;
    virtual void reset() noexcept = 0;

    // Real-time audio rendering loop (ZERO dynamic memory allocations permitted)
    virtual void processBlock(uint32_t numFrames) noexcept = 0;

    // Event & modulation handling
    virtual void handleEvent(const AudioEvent& event) noexcept {
        if (event.type == AudioEventType::SetParameter) {
            setParameter(event.paramId, event.paramValue);
        }
    }
    virtual void setParameter(uint32_t /*paramId*/, float /*value*/) noexcept {}

protected:
    void addInputPort(uint32_t numChannels = 2) {
        NodePort port;
        port.portIndex = static_cast<uint32_t>(inputs_.size());
        port.numChannels = std::clamp(numChannels, 1u, 2u);
        inputs_.push_back(port);
    }

    void addOutputPort(uint32_t numChannels = 2) {
        NodePort port;
        port.portIndex = static_cast<uint32_t>(outputs_.size());
        port.numChannels = std::clamp(numChannels, 1u, 2u);
        outputs_.push_back(port);
    }

    NodeId id_{INVALID_NODE_ID};
    std::string name_;
    bool enabled_{true};
    double sampleRate_{48000.0};
    uint32_t maxBlockSize_{128};

    std::vector<NodePort> inputs_;
    std::vector<NodePort> outputs_;
};

} // namespace eatsbits::audio

#endif // EATS_GRAPH_NODE_HPP
