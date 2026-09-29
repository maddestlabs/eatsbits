#ifndef EATS_AUDIO_GRAPH_HPP
#define EATS_AUDIO_GRAPH_HPP

#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <array>
#include "graph_node.hpp"
#include "../../abi/eats_plugin_abi.h"

namespace eatsbits::audio {

struct GraphConnection {
    NodeId srcNode{INVALID_NODE_ID};
    uint32_t srcPort{0};
    NodeId dstNode{INVALID_NODE_ID};
    uint32_t dstPort{0};

    bool operator==(const GraphConnection& other) const noexcept {
        return srcNode == other.srcNode && srcPort == other.srcPort &&
               dstNode == other.dstNode && dstPort == other.dstPort;
    }
};

/**
 * Pre-allocated memory pool for audio graph buffer routing.
 * Ensures strict ZERO allocations on the real-time audio thread.
 */
class BufferArena {
public:
    static constexpr size_t MAX_BUFFERS = 128;
    static constexpr size_t MAX_BLOCK_SIZE = 2048;

    BufferArena() {
        memory_.assign(MAX_BUFFERS * MAX_BLOCK_SIZE, 0.0f);
    }

    void reset() noexcept {
        allocatedCount_ = 0;
    }

    float* allocateBuffer() noexcept {
        if (allocatedCount_ >= MAX_BUFFERS) return nullptr;
        float* ptr = memory_.data() + (allocatedCount_ * MAX_BLOCK_SIZE);
        allocatedCount_++;
        return ptr;
    }

    [[nodiscard]] size_t getAllocatedCount() const noexcept { return allocatedCount_; }

private:
    std::vector<float> memory_;
    size_t allocatedCount_{0};
};

/**
 * Atomic double-buffered execution plan evaluated by the audio thread.
 */
struct SumStep {
    const float* src;
    float* dst;
    uint32_t frames;
};

struct ExecutionStep {
    GraphNode* node{nullptr};
    std::vector<float*> sumBuffersToClear;
    std::vector<SumStep> preSumSteps;
};

struct ExecutionPlan {
    std::vector<ExecutionStep> steps;
    GraphNode* outputNode{nullptr};
    uint32_t outputPort{0};
    bool valid{false};
};

/**
 * Composable, Zero-Allocation Modular DSP Audio Graph & Evaluator.
 * Supports dynamic cable patching, topological sorting, cycle detection,
 * and lock-free execution swaps.
 */
class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    void prepare(double sampleRate, uint32_t maxBlockSize);
    void reset() noexcept;

    // Node Lifecycle Management
    NodeId addNode(std::shared_ptr<GraphNode> node);
    bool removeNode(NodeId id);
    [[nodiscard]] std::shared_ptr<GraphNode> getNode(NodeId id) const;
    [[nodiscard]] size_t getNodeCount() const noexcept;
    [[nodiscard]] std::vector<NodeId> getNodeIds() const;
    [[nodiscard]] std::vector<std::pair<NodeId, std::shared_ptr<GraphNode>>> getNodeSnapshot() const;
    [[nodiscard]] NodeId findNodeByName(const std::string& name) const;
    [[nodiscard]] const std::unordered_map<NodeId, std::shared_ptr<GraphNode>>& getNodes() const noexcept { return nodes_; }

    // Cable Patching
    bool connect(NodeId srcNode, uint32_t srcPort, NodeId dstNode, uint32_t dstPort);
    bool disconnect(NodeId srcNode, uint32_t srcPort, NodeId dstNode, uint32_t dstPort);
    void disconnectAll(NodeId id);
    void clear();

    [[nodiscard]] const std::vector<GraphConnection>& getConnections() const noexcept {
        return connections_;
    }

    // Designate master output node
    void setOutputNode(NodeId id, uint32_t port = 0);
    [[nodiscard]] NodeId getOutputNodeId() const noexcept { return outputNodeId_; }

    // Rebuild execution schedule (called automatically on topology change)
    bool compile();

    // Real-Time Audio Rendering Loop (Wait-free, zero allocation)
    void process(float* masterOutL, float* masterOutR, uint32_t numFrames) noexcept;

    // Real-Time Event Dispatch to all nodes
    void broadcastEvent(const AudioEvent& event) noexcept;
    void sendNodeEvent(NodeId targetId, const AudioEvent& event) noexcept;

private:
    bool buildTopologicalOrder(std::vector<NodeId>& order) const;
    bool rebuildExecutionPlan(ExecutionPlan& plan, BufferArena& arena);

    double sampleRate_{48000.0};
    uint32_t maxBlockSize_{128};
    NodeId nextNodeId_{1};
    NodeId outputNodeId_{INVALID_NODE_ID};
    uint32_t outputNodePort_{0};

    mutable std::mutex graphMutex_; // Protects off-audio topology mutations
    std::unordered_map<NodeId, std::shared_ptr<GraphNode>> nodes_;
    std::vector<GraphConnection> connections_;

    // Double-buffered real-time execution plan and buffer arenas
    ExecutionPlan plans_[2];
    BufferArena arenas_[2];
    std::atomic<int> activePlanIndex_{0};

    // Pre-allocated silent zero buffer for unconnected inputs
    alignas(64) std::array<float, BufferArena::MAX_BLOCK_SIZE> zeroBuffer_{};
};

} // namespace eatsbits::audio

#endif // EATS_AUDIO_GRAPH_HPP
