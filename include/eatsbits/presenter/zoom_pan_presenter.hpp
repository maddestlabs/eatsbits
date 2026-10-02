#ifndef EATS_ZOOM_PAN_PRESENTER_HPP
#define EATS_ZOOM_PAN_PRESENTER_HPP

#include "eatsbits/presenter/presenter_base.hpp"
#include <algorithm>
#include <cmath>

namespace eatsbits::presenter {

/**
 * @brief Headless 2D viewport transform state machine managing horizontal (time)
 * and vertical (pitch/track) zoom levels, pan offsets, and anchor-preserving zooming.
 * Guaranteed zero heap allocations.
 */
class ZoomPanPresenter : public PresenterBase {
public:
    struct Vec2D {
        float x{0.0f};
        float y{0.0f};

        constexpr Vec2D() noexcept = default;
        constexpr Vec2D(float x_, float y_) noexcept : x(x_), y(y_) {}
    };

    struct ViewportRect {
        float x{0.0f};
        float y{0.0f};
        float width{800.0f};
        float height{600.0f};
    };

    struct WorldBounds {
        float minX{0.0f};
        float minY{0.0f};
        float maxX{0.0f};
        float maxY{0.0f};
    };

    ZoomPanPresenter() = default;
    ~ZoomPanPresenter() override = default;

    // --- Viewport Configuration ---

    void setViewport(float x, float y, float width, float height) noexcept {
        viewport_.x = x;
        viewport_.y = y;
        viewport_.width = std::max(0.0f, width);
        viewport_.height = std::max(0.0f, height);
        markDirty();
    }

    [[nodiscard]] const ViewportRect& getViewport() const noexcept { return viewport_; }
    [[nodiscard]] float getViewportOriginX() const noexcept { return viewport_.x; }
    [[nodiscard]] float getViewportOriginY() const noexcept { return viewport_.y; }
    [[nodiscard]] float getViewportWidth() const noexcept { return viewport_.width; }
    [[nodiscard]] float getViewportHeight() const noexcept { return viewport_.height; }

    // --- Zoom Limits ---

    void setZoomLimits(float minZoomX, float maxZoomX, float minZoomY, float maxZoomY) noexcept {
        minZoomX_ = std::max(0.0001f, minZoomX);
        maxZoomX_ = std::max(minZoomX_, maxZoomX);
        minZoomY_ = std::max(0.0001f, minZoomY);
        maxZoomY_ = std::max(minZoomY_, maxZoomY);
        zoomX_ = std::clamp(zoomX_, minZoomX_, maxZoomX_);
        zoomY_ = std::clamp(zoomY_, minZoomY_, maxZoomY_);
        markDirty();
    }

    void setUniformZoomLimits(float minZoom, float maxZoom) noexcept {
        setZoomLimits(minZoom, maxZoom, minZoom, maxZoom);
    }

    [[nodiscard]] float getMinZoomX() const noexcept { return minZoomX_; }
    [[nodiscard]] float getMaxZoomX() const noexcept { return maxZoomX_; }
    [[nodiscard]] float getMinZoomY() const noexcept { return minZoomY_; }
    [[nodiscard]] float getMaxZoomY() const noexcept { return maxZoomY_; }

    // --- Pan Bounds (Optional Clamping) ---

    void setPanBounds(float minPanX, float maxPanX, float minPanY, float maxPanY) noexcept {
        minPanX_ = minPanX;
        maxPanX_ = std::max(minPanX, maxPanX);
        minPanY_ = minPanY;
        maxPanY_ = std::max(minPanY, maxPanY);
        panBoundsEnabled_ = true;
        clampPanToBounds();
        markDirty();
    }

    void setPanBoundsEnabled(bool enabled) noexcept {
        panBoundsEnabled_ = enabled;
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    [[nodiscard]] bool isPanBoundsEnabled() const noexcept { return panBoundsEnabled_; }
    [[nodiscard]] float getMinPanX() const noexcept { return minPanX_; }
    [[nodiscard]] float getMaxPanX() const noexcept { return maxPanX_; }
    [[nodiscard]] float getMinPanY() const noexcept { return minPanY_; }
    [[nodiscard]] float getMaxPanY() const noexcept { return maxPanY_; }

    // --- Transform State Accessors ---

    [[nodiscard]] float getZoomX() const noexcept { return zoomX_; }
    [[nodiscard]] float getZoomY() const noexcept { return zoomY_; }
    [[nodiscard]] float getPanX() const noexcept { return panX_; }
    [[nodiscard]] float getPanY() const noexcept { return panY_; }

    void setPan(float panX, float panY) noexcept {
        panX_ = panX;
        panY_ = panY;
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    void setPanX(float panX) noexcept {
        panX_ = panX;
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    void setPanY(float panY) noexcept {
        panY_ = panY;
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    void setZoom(float zoomX, float zoomY) noexcept {
        zoomX_ = std::clamp(zoomX, minZoomX_, maxZoomX_);
        zoomY_ = std::clamp(zoomY, minZoomY_, maxZoomY_);
        markDirty();
    }

    void setZoomX(float zoomX) noexcept {
        zoomX_ = std::clamp(zoomX, minZoomX_, maxZoomX_);
        markDirty();
    }

    void setZoomY(float zoomY) noexcept {
        zoomY_ = std::clamp(zoomY, minZoomY_, maxZoomY_);
        markDirty();
    }

    // --- Coordinate Transforms (Pure Math) ---

    void worldToScreen(float worldX, float worldY, float& outScreenX, float& outScreenY) const noexcept {
        outScreenX = viewport_.x + (worldX - panX_) * zoomX_;
        outScreenY = viewport_.y + (worldY - panY_) * zoomY_;
    }

    [[nodiscard]] float worldToScreenX(float worldX) const noexcept {
        return viewport_.x + (worldX - panX_) * zoomX_;
    }

    [[nodiscard]] float worldToScreenY(float worldY) const noexcept {
        return viewport_.y + (worldY - panY_) * zoomY_;
    }

    [[nodiscard]] Vec2D worldToScreen(const Vec2D& world) const noexcept {
        return {worldToScreenX(world.x), worldToScreenY(world.y)};
    }

    void screenToWorld(float screenX, float screenY, float& outWorldX, float& outWorldY) const noexcept {
        outWorldX = panX_ + (screenX - viewport_.x) / std::max(0.00001f, zoomX_);
        outWorldY = panY_ + (screenY - viewport_.y) / std::max(0.00001f, zoomY_);
    }

    [[nodiscard]] float screenToWorldX(float screenX) const noexcept {
        return panX_ + (screenX - viewport_.x) / std::max(0.00001f, zoomX_);
    }

    [[nodiscard]] float screenToWorldY(float screenY) const noexcept {
        return panY_ + (screenY - viewport_.y) / std::max(0.00001f, zoomY_);
    }

    [[nodiscard]] Vec2D screenToWorld(const Vec2D& screen) const noexcept {
        return {screenToWorldX(screen.x), screenToWorldY(screen.y)};
    }

    // --- Distance Helpers ---

    [[nodiscard]] float worldDistanceToScreenX(float worldDistX) const noexcept {
        return worldDistX * zoomX_;
    }

    [[nodiscard]] float worldDistanceToScreenY(float worldDistY) const noexcept {
        return worldDistY * zoomY_;
    }

    [[nodiscard]] float screenDistanceToWorldX(float screenDistX) const noexcept {
        return screenDistX / std::max(0.00001f, zoomX_);
    }

    [[nodiscard]] float screenDistanceToWorldY(float screenDistY) const noexcept {
        return screenDistY / std::max(0.00001f, zoomY_);
    }

    // --- Anchor-Point Zooming ---

    /**
     * @brief Zoom with target zoom factors relative to an anchor point in screen coordinates.
     * The world position currently under (screenAnchorX, screenAnchorY) remains stationary.
     */
    void zoomAt(float screenAnchorX, float screenAnchorY, float factorX, float factorY) noexcept {
        zoomTo(screenAnchorX, screenAnchorY, zoomX_ * factorX, zoomY_ * factorY);
    }

    /**
     * @brief Uniform zoom with a target zoom factor relative to an anchor point in screen coordinates.
     */
    void zoomAt(float screenAnchorX, float screenAnchorY, float factor) noexcept {
        zoomAt(screenAnchorX, screenAnchorY, factor, factor);
    }

    /**
     * @brief Zoom directly to specified zoom levels while keeping (screenAnchorX, screenAnchorY) stationary.
     */
    void zoomTo(float screenAnchorX, float screenAnchorY, float targetZoomX, float targetZoomY) noexcept;

    /**
     * @brief Uniform zoom to specified zoom level while keeping anchor stationary.
     */
    void zoomTo(float screenAnchorX, float screenAnchorY, float uniformTargetZoom) noexcept {
        zoomTo(screenAnchorX, screenAnchorY, uniformTargetZoom, uniformTargetZoom);
    }

    // --- Multi-touch Pinch & Drag Interaction ---

    /**
     * @brief Handle multi-touch pinch gesture with simultaneous pan translation.
     */
    void handlePinch(float screenCenterX, float screenCenterY, float pinchScaleFactor,
                     float deltaScreenX = 0.0f, float deltaScreenY = 0.0f) noexcept {
        handlePinch2D(screenCenterX, screenCenterY, pinchScaleFactor, pinchScaleFactor, deltaScreenX, deltaScreenY);
    }

    /**
     * @brief Handle 2D independent horizontal and vertical pinch scaling with pan translation.
     */
    void handlePinch2D(float screenCenterX, float screenCenterY, float scaleX, float scaleY,
                       float deltaScreenX = 0.0f, float deltaScreenY = 0.0f) noexcept;

    // --- Panning Operations ---

    /**
     * @brief Pan by screen delta pixels. Dragging right moves pan left so content follows cursor.
     */
    void panByScreen(float deltaScreenX, float deltaScreenY) noexcept {
        panX_ -= deltaScreenX / std::max(0.00001f, zoomX_);
        panY_ -= deltaScreenY / std::max(0.00001f, zoomY_);
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    /**
     * @brief Pan directly in world units.
     */
    void panByWorld(float deltaWorldX, float deltaWorldY) noexcept {
        panX_ += deltaWorldX;
        panY_ += deltaWorldY;
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    /**
     * @brief Center the viewport around a world coordinate (worldX, worldY).
     */
    void centerOn(float worldX, float worldY) noexcept {
        panX_ = worldX - (viewport_.width * 0.5f) / std::max(0.00001f, zoomX_);
        panY_ = worldY - (viewport_.height * 0.5f) / std::max(0.00001f, zoomY_);
        if (panBoundsEnabled_) {
            clampPanToBounds();
        }
        markDirty();
    }

    // --- Visible Bounds Query (View Culling) ---

    void getVisibleWorldBounds(float& outMinX, float& outMinY, float& outMaxX, float& outMaxY) const noexcept {
        screenToWorld(viewport_.x, viewport_.y, outMinX, outMinY);
        screenToWorld(viewport_.x + viewport_.width, viewport_.y + viewport_.height, outMaxX, outMaxY);
    }

    [[nodiscard]] WorldBounds getVisibleWorldBounds() const noexcept {
        WorldBounds b{};
        getVisibleWorldBounds(b.minX, b.minY, b.maxX, b.maxY);
        return b;
    }

private:
    void clampPanToBounds() noexcept {
        panX_ = std::clamp(panX_, minPanX_, maxPanX_);
        panY_ = std::clamp(panY_, minPanY_, maxPanY_);
    }

    ViewportRect viewport_{};

    float panX_{0.0f};
    float panY_{0.0f};
    float zoomX_{1.0f};
    float zoomY_{1.0f};

    float minZoomX_{0.01f};
    float maxZoomX_{100.0f};
    float minZoomY_{0.01f};
    float maxZoomY_{100.0f};

    bool panBoundsEnabled_{false};
    float minPanX_{0.0f};
    float maxPanX_{10000.0f};
    float minPanY_{0.0f};
    float maxPanY_{10000.0f};
};

} // namespace eatsbits::presenter

#endif // EATS_ZOOM_PAN_PRESENTER_HPP
