#include "eatsbits/presenter/zoom_pan_presenter.hpp"

namespace eatsbits::presenter {

void ZoomPanPresenter::zoomTo(float screenAnchorX, float screenAnchorY, float targetZoomX, float targetZoomY) noexcept {
    float newZoomX = std::clamp(targetZoomX, minZoomX_, maxZoomX_);
    float newZoomY = std::clamp(targetZoomY, minZoomY_, maxZoomY_);

    // 1. Calculate world anchor position under the screen anchor before zoom
    float worldAnchorX = panX_ + (screenAnchorX - viewport_.x) / std::max(0.00001f, zoomX_);
    float worldAnchorY = panY_ + (screenAnchorY - viewport_.y) / std::max(0.00001f, zoomY_);

    // 2. Compute new pan such that screenToWorld(screenAnchor) remains exactly worldAnchor
    panX_ = worldAnchorX - (screenAnchorX - viewport_.x) / std::max(0.00001f, newZoomX);
    panY_ = worldAnchorY - (screenAnchorY - viewport_.y) / std::max(0.00001f, newZoomY);

    zoomX_ = newZoomX;
    zoomY_ = newZoomY;

    if (panBoundsEnabled_) {
        clampPanToBounds();
    }
    markDirty();
}

void ZoomPanPresenter::handlePinch2D(float screenCenterX, float screenCenterY, float scaleX, float scaleY,
                                     float deltaScreenX, float deltaScreenY) noexcept {
    zoomTo(screenCenterX, screenCenterY, zoomX_ * scaleX, zoomY_ * scaleY);
    if (std::abs(deltaScreenX) > 0.0001f || std::abs(deltaScreenY) > 0.0001f) {
        panByScreen(deltaScreenX, deltaScreenY);
    }
}

} // namespace eatsbits::presenter
