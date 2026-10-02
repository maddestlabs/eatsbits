#ifndef EATS_CABLE_PATCH_PRESENTER_HPP
#define EATS_CABLE_PATCH_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include "eatsbits/presenter/drag_handler.hpp"
#include "eatsbits/audio/graph/audio_graph.hpp"
#include <functional>

namespace eatsbits::presenter {

/**
 * @brief Headless presenter managing patch cable dragging, floating probe positioning,
 * and connection validation on the modular rack graph.
 */
class CablePatchPresenter : public PresenterBase, public IDragHandler {
public:
    CablePatchPresenter() = default;

    /**
     * @brief Begin dragging a patch cable from an output jack.
     */
    void startPatch(audio::NodeId srcNode, uint32_t srcPort, float srcX, float srcY,
                    std::function<void(audio::NodeId, uint32_t, audio::NodeId, uint32_t)> onConnected = nullptr);

    // IDragHandler Implementation
    using IDragHandler::onPointerMove;
    using IDragHandler::onPointerUp;
    void onPointerMove(const ui::PointerEvent& ev) override;
    void onPointerUp(const ui::PointerEvent& ev) override;
    void cancelDrag() override;

    [[nodiscard]] bool isDragging() const noexcept override { return isDragging_; }
    [[nodiscard]] ui::DragMode getDragMode() const noexcept override {
        return isDragging_ ? ui::DragMode::PatchCable : ui::DragMode::None;
    }

    [[nodiscard]] audio::NodeId getSourceNode() const noexcept { return srcNode_; }
    [[nodiscard]] uint32_t getSourcePort() const noexcept { return srcPort_; }
    [[nodiscard]] float getSourceX() const noexcept { return srcX_; }
    [[nodiscard]] float getSourceY() const noexcept { return srcY_; }
    [[nodiscard]] float getCurrentX() const noexcept { return curX_; }
    [[nodiscard]] float getCurrentY() const noexcept { return curY_; }

    /**
     * @brief Validate if dropping onto destination is a legal connection.
     */
    [[nodiscard]] bool canConnect(audio::NodeId destNode, uint32_t /*destPort*/, bool isOutput) const noexcept {
        if (!isDragging_) return false;
        if (destNode == 0 || destNode == srcNode_) return false;
        if (isOutput) return false; // Must drop onto an input jack
        return true;
    }

    /**
     * @brief Commit connection to graph and notify listener.
     */
    bool commitConnection(audio::NodeId destNode, uint32_t destPort, bool isOutput, audio::AudioGraph& graph);

private:
    bool isDragging_{false};
    audio::NodeId srcNode_{0};
    uint32_t srcPort_{0};
    float srcX_{0.0f};
    float srcY_{0.0f};
    float curX_{0.0f};
    float curY_{0.0f};

    std::function<void(audio::NodeId, uint32_t, audio::NodeId, uint32_t)> onConnected_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_CABLE_PATCH_PRESENTER_HPP
