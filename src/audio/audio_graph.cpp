#include "eatsbits/audio/graph/audio_graph.hpp"
#include <queue>
#include <cstring>
#include <iostream>

namespace eatsbits::audio {

AudioGraph::AudioGraph() {
    zeroBuffer_.fill(0.0f);
}

AudioGraph::~AudioGraph() = default;

void AudioGraph::prepare(double sampleRate, uint32_t maxBlockSize) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    sampleRate_ = sampleRate;
    maxBlockSize_ = maxBlockSize;

    for (auto& [id, node] : nodes_) {
        if (node) {
            node->prepare(sampleRate, maxBlockSize);
        }
    }

    compile();
}

void AudioGraph::reset() noexcept {
    std::lock_guard<std::mutex> lock(graphMutex_);
    for (auto& [id, node] : nodes_) {
        if (node) {
            node->reset();
        }
    }
}

NodeId AudioGraph::addNode(std::shared_ptr<GraphNode> node) {
    if (!node) return INVALID_NODE_ID;

    std::lock_guard<std::mutex> lock(graphMutex_);
    NodeId id = nextNodeId_++;
    node->setId(id);
    node->prepare(sampleRate_, maxBlockSize_);
    nodes_[id] = node;

    // Default output node if none selected
    if (outputNodeId_ == INVALID_NODE_ID && node->numOutputPorts() > 0) {
        outputNodeId_ = id;
        outputNodePort_ = 0;
    }

    compile();
    return id;
}

bool AudioGraph::removeNode(NodeId id) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = nodes_.find(id);
    if (it == nodes_.end()) return false;

    disconnectAll(id);

    if (outputNodeId_ == id) {
        outputNodeId_ = INVALID_NODE_ID;
        outputNodePort_ = 0;
    }

    nodes_.erase(it);
    compile();
    return true;
}

std::shared_ptr<GraphNode> AudioGraph::getNode(NodeId id) const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = nodes_.find(id);
    if (it != nodes_.end()) return it->second;
    return nullptr;
}

size_t AudioGraph::getNodeCount() const noexcept {
    std::lock_guard<std::mutex> lock(graphMutex_);
    return nodes_.size();
}

std::vector<NodeId> AudioGraph::getNodeIds() const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    std::vector<NodeId> ids;
    ids.reserve(nodes_.size());
    for (const auto& [id, _] : nodes_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<std::pair<NodeId, std::shared_ptr<GraphNode>>> AudioGraph::getNodeSnapshot() const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    std::vector<std::pair<NodeId, std::shared_ptr<GraphNode>>> snapshot;
    snapshot.reserve(nodes_.size());
    for (const auto& pair : nodes_) {
        snapshot.push_back(pair);
    }
    return snapshot;
}

NodeId AudioGraph::findNodeByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    for (const auto& [id, node] : nodes_) {
        if (node && node->getName() == name) return id;
    }
    return INVALID_NODE_ID;
}

bool AudioGraph::connect(NodeId srcNode, uint32_t srcPort, NodeId dstNode, uint32_t dstPort) {
    std::lock_guard<std::mutex> lock(graphMutex_);

    if (srcNode == dstNode) {
        return false; // Direct self-loop not permitted without explicit delay
    }

    auto srcIt = nodes_.find(srcNode);
    auto dstIt = nodes_.find(dstNode);
    if (srcIt == nodes_.end() || dstIt == nodes_.end()) {
        return false;
    }

    if (srcPort >= srcIt->second->numOutputPorts() || dstPort >= dstIt->second->numInputPorts()) {
        return false;
    }

    GraphConnection newConn{srcNode, srcPort, dstNode, dstPort};
    for (const auto& conn : connections_) {
        if (conn == newConn) return true; // Already connected
    }

    // Speculatively add connection and test for cycles
    connections_.push_back(newConn);
    std::vector<NodeId> testOrder;
    if (!buildTopologicalOrder(testOrder)) {
        // Cycle detected, revert!
        connections_.pop_back();
        return false;
    }

    compile();
    return true;
}

bool AudioGraph::disconnect(NodeId srcNode, uint32_t srcPort, NodeId dstNode, uint32_t dstPort) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    GraphConnection target{srcNode, srcPort, dstNode, dstPort};

    auto it = std::remove(connections_.begin(), connections_.end(), target);
    if (it != connections_.end()) {
        connections_.erase(it, connections_.end());
        compile();
        return true;
    }
    return false;
}

void AudioGraph::disconnectAll(NodeId id) {
    auto it = std::remove_if(connections_.begin(), connections_.end(), [id](const GraphConnection& c) {
        return c.srcNode == id || c.dstNode == id;
    });
    connections_.erase(it, connections_.end());
}

void AudioGraph::clear() {
    std::lock_guard<std::mutex> lock(graphMutex_);
    connections_.clear();
    nodes_.clear();
    outputNodeId_ = INVALID_NODE_ID;
    outputNodePort_ = 0;
    nextNodeId_ = 1;
    compile();
}

void AudioGraph::setOutputNode(NodeId id, uint32_t port) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    outputNodeId_ = id;
    outputNodePort_ = port;
    compile();
}

bool AudioGraph::buildTopologicalOrder(std::vector<NodeId>& order) const {
    order.clear();
    if (nodes_.empty()) return true;

    std::unordered_map<NodeId, uint32_t> inDegree;
    std::unordered_map<NodeId, std::vector<NodeId>> adjList;

    for (const auto& [id, _] : nodes_) {
        inDegree[id] = 0;
    }

    for (const auto& conn : connections_) {
        adjList[conn.srcNode].push_back(conn.dstNode);
        inDegree[conn.dstNode]++;
    }

    std::queue<NodeId> q;
    for (const auto& [id, deg] : inDegree) {
        if (deg == 0) {
            q.push(id);
        }
    }

    while (!q.empty()) {
        NodeId curr = q.front();
        q.pop();
        order.push_back(curr);

        auto it = adjList.find(curr);
        if (it != adjList.end()) {
            for (NodeId neighbor : it->second) {
                if (--inDegree[neighbor] == 0) {
                    q.push(neighbor);
                }
            }
        }
    }

    return order.size() == nodes_.size();
}

bool AudioGraph::rebuildExecutionPlan(ExecutionPlan& plan, BufferArena& arena) {
    arena.reset();
    plan.steps.clear();
    plan.valid = false;

    std::vector<NodeId> order;
    if (!buildTopologicalOrder(order)) {
        return false;
    }

    // Phase 1: Allocate output port buffers for all nodes
    for (NodeId id : order) {
        auto node = nodes_[id];
        for (uint32_t p = 0; p < node->numOutputPorts(); ++p) {
            const auto& port = node->getOutputPort(p);
            for (uint32_t c = 0; c < port.numChannels; ++c) {
                float* buf = arena.allocateBuffer();
                if (!buf) return false;
                node->setOutputBufferPtr(p, c, buf);
            }
        }
    }

    // Phase 2: Route inputs & configure pre-summing steps if needed
    for (NodeId id : order) {
        auto dstNode = nodes_[id];
        ExecutionStep step;
        step.node = dstNode.get();

        for (uint32_t p = 0; p < dstNode->numInputPorts(); ++p) {
            // Find all incoming connections targeting (id, p)
            std::vector<GraphConnection> incoming;
            for (const auto& conn : connections_) {
                if (conn.dstNode == id && conn.dstPort == p) {
                    incoming.push_back(conn);
                }
            }

            if (incoming.empty()) {
                // Unconnected: feed silent zero buffer
                dstNode->setInputBufferPtr(p, 0, zeroBuffer_.data());
                dstNode->setInputBufferPtr(p, 1, zeroBuffer_.data());
            } else if (incoming.size() == 1) {
                // Direct cable (Zero Copy)
                const auto& conn = incoming[0];
                auto srcNode = nodes_[conn.srcNode];
                const float* bufL = srcNode->getOutputBuffer(conn.srcPort, 0);
                const float* bufR = srcNode->getOutputBuffer(conn.srcPort, 1);

                dstNode->setInputBufferPtr(p, 0, bufL ? bufL : zeroBuffer_.data());
                dstNode->setInputBufferPtr(p, 1, bufR ? bufR : (bufL ? bufL : zeroBuffer_.data()));
            } else {
                // Multiple sources connecting to the same port -> Summing Bus
                float* sumBufL = arena.allocateBuffer();
                float* sumBufR = arena.allocateBuffer();
                if (!sumBufL || !sumBufR) return false;

                dstNode->setInputBufferPtr(p, 0, sumBufL);
                dstNode->setInputBufferPtr(p, 1, sumBufR);

                step.sumBuffersToClear.push_back(sumBufL);
                step.sumBuffersToClear.push_back(sumBufR);

                for (const auto& conn : incoming) {
                    auto srcNode = nodes_[conn.srcNode];
                    const float* sL = srcNode->getOutputBuffer(conn.srcPort, 0);
                    const float* sR = srcNode->getOutputBuffer(conn.srcPort, 1);
                    if (sL) step.preSumSteps.push_back(SumStep{sL, sumBufL, 0});
                    if (sR) step.preSumSteps.push_back(SumStep{sR, sumBufR, 0});
                }
            }
        }

        plan.steps.push_back(step);
    }

    auto outIt = nodes_.find(outputNodeId_);
    if (outIt != nodes_.end()) {
        plan.outputNode = outIt->second.get();
        plan.outputPort = outputNodePort_;
    } else {
        plan.outputNode = nullptr;
        plan.outputPort = 0;
    }

    plan.valid = true;
    return true;
}

bool AudioGraph::compile() {
    int nextIdx = 1 - activePlanIndex_.load(std::memory_order_relaxed);
    if (!rebuildExecutionPlan(plans_[nextIdx], arenas_[nextIdx])) {
        return false;
    }
    // Atomic lock-free swap
    activePlanIndex_.store(nextIdx, std::memory_order_release);
    return true;
}

void AudioGraph::process(float* masterOutL, float* masterOutR, uint32_t numFrames) noexcept {
    int idx = activePlanIndex_.load(std::memory_order_acquire);
    const auto& plan = plans_[idx];

    if (!plan.valid || plan.steps.empty()) {
        if (masterOutL) std::fill_n(masterOutL, numFrames, 0.0f);
        if (masterOutR) std::fill_n(masterOutR, numFrames, 0.0f);
        return;
    }

    // Evaluate nodes in strictly sorted topological order
    for (const auto& step : plan.steps) {
        if (!step.node) continue;

        // Zero out summing buffers
        for (float* buf : step.sumBuffersToClear) {
            std::fill_n(buf, numFrames, 0.0f);
        }

        // Perform any bus summing into destination input buffers
        for (const auto& sum : step.preSumSteps) {
            for (uint32_t i = 0; i < numFrames; ++i) {
                sum.dst[i] += sum.src[i];
            }
        }

        if (!step.node->isEnabled()) {
            for (uint32_t p = 0; p < step.node->numOutputPorts(); ++p) {
                for (uint32_t c = 0; c < 2; ++c) {
                    float* outBuf = step.node->getOutputBuffer(p, c);
                    if (outBuf) {
                        std::fill_n(outBuf, numFrames, 0.0f);
                    }
                }
            }
            continue;
        }

        step.node->processBlock(numFrames);
    }

    // Write to master output buffer
    if (plan.outputNode && masterOutL && masterOutR) {
        const float* outL = plan.outputNode->getOutputBuffer(plan.outputPort, 0);
        const float* outR = plan.outputNode->getOutputBuffer(plan.outputPort, 1);

        if (outL) {
            std::copy_n(outL, numFrames, masterOutL);
        } else {
            std::fill_n(masterOutL, numFrames, 0.0f);
        }

        if (outR) {
            std::copy_n(outR, numFrames, masterOutR);
        } else if (outL) {
            std::copy_n(outL, numFrames, masterOutR); // Duplicate mono
        } else {
            std::fill_n(masterOutR, numFrames, 0.0f);
        }
    } else {
        if (masterOutL) std::fill_n(masterOutL, numFrames, 0.0f);
        if (masterOutR) std::fill_n(masterOutR, numFrames, 0.0f);
    }
}

void AudioGraph::broadcastEvent(const AudioEvent& event) noexcept {
    int idx = activePlanIndex_.load(std::memory_order_acquire);
    const auto& plan = plans_[idx];
    for (const auto& step : plan.steps) {
        if (step.node) {
            step.node->handleEvent(event);
        }
    }
}

void AudioGraph::sendNodeEvent(NodeId targetId, const AudioEvent& event) noexcept {
    int idx = activePlanIndex_.load(std::memory_order_acquire);
    const auto& plan = plans_[idx];
    for (const auto& step : plan.steps) {
        if (step.node && step.node->getId() == targetId) {
            step.node->handleEvent(event);
            break;
        }
    }
}

} // namespace eatsbits::audio
