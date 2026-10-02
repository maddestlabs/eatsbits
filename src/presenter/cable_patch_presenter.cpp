#include "eatsbits/presenter/cable_patch_presenter.hpp"

namespace eatsbits::presenter {

void CablePatchPresenter::startPatch(audio::NodeId srcNode, uint32_t srcPort, float srcX, float srcY,
                                     std::function<void(audio::NodeId, uint32_t, audio::NodeId, uint32_t)> onConnected) {
    srcNode_ = srcNode;
    srcPort_ = srcPort;
    srcX_ = srcX;
    srcY_ = srcY;
    curX_ = srcX;
    curY_ = srcY;
    onConnected_ = std::move(onConnected);
    isDragging_ = true;
    markDirty();
}

void CablePatchPresenter::onPointerMove(const ui::PointerEvent& ev) {
    if (!isDragging_) return;

    curX_ = ev.x;
    curY_ = ev.y;
    markDirty();
}

void CablePatchPresenter::onPointerUp(const ui::PointerEvent& /*ev*/) {
    if (!isDragging_) return;

    isDragging_ = false;
    markDirty();
}

void CablePatchPresenter::cancelDrag() {
    if (!isDragging_) return;

    isDragging_ = false;
    markDirty();
}

bool CablePatchPresenter::commitConnection(audio::NodeId destNode, uint32_t destPort, bool isOutput, audio::AudioGraph& graph) {
    if (!canConnect(destNode, destPort, isOutput)) {
        cancelDrag();
        return false;
    }

    graph.connect(srcNode_, srcPort_, destNode, destPort);
    graph.compile();

    if (onConnected_) {
        onConnected_(srcNode_, srcPort_, destNode, destPort);
    }

    isDragging_ = false;
    markDirty();
    return true;
}

} // namespace eatsbits::presenter
