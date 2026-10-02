#ifndef EATS_ELEMENT_2D_HPP
#define EATS_ELEMENT_2D_HPP

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include "eatsbits/ui/geometry.hpp"
#include "eatsbits/ui/input/pointer_event.hpp"

namespace eatsbits::ui {

struct ViewContext;

/**
 * @brief Foundational 2D scene graph node for Eatsbits UI hierarchy.
 * Encapsulates parent-child trees, local/global coordinate transformations,
 * hit testing, event bubbling, clipping bounds, dirty flags, and layout/render lifecycles.
 */
class Element2D : public std::enable_shared_from_this<Element2D> {
public:
    Element2D() = default;
    explicit Element2D(std::string id) : id_(std::move(id)) {}
    virtual ~Element2D();

    // --- Identification ---

    [[nodiscard]] const std::string& getId() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    [[nodiscard]] Element2D* findById(const std::string& targetId) noexcept;

    // --- Tree Hierarchy ---

    [[nodiscard]] Element2D* getParent() const noexcept { return parent_; }
    [[nodiscard]] const std::vector<std::shared_ptr<Element2D>>& getChildren() const noexcept { return children_; }
    [[nodiscard]] size_t getChildCount() const noexcept { return children_.size(); }
    [[nodiscard]] Element2D* getChildAt(size_t index) const noexcept {
        return (index < children_.size()) ? children_[index].get() : nullptr;
    }

    void addChild(std::shared_ptr<Element2D> child);
    void insertChild(size_t index, std::shared_ptr<Element2D> child);
    bool removeChild(Element2D* child);
    bool removeChild(const std::shared_ptr<Element2D>& child);
    void removeAllChildren();
    void removeFromParent();

    // --- Geometry & Transforms ---

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }
    void setBounds(const Rect2D& b) noexcept { bounds_ = b; markDirty(); }
    void setBounds(float x, float y, float w, float h) noexcept {
        bounds_ = Rect2D{x, y, w, h};
        markDirty();
    }

    [[nodiscard]] Point localToGlobal(const Point& localPt) const noexcept;
    [[nodiscard]] Point globalToLocal(const Point& globalPt) const noexcept;
    [[nodiscard]] Rect2D getGlobalBounds() const noexcept;

    // --- Clipping ---

    [[nodiscard]] bool isClippingEnabled() const noexcept { return clipping_; }
    void setClipping(bool clip) noexcept { clipping_ = clip; markDirty(); }
    [[nodiscard]] Rect2D getGlobalClipRect() const noexcept;

    // --- Visibility & State ---

    [[nodiscard]] bool isVisible() const noexcept { return visible_; }
    void setVisible(bool v) noexcept { visible_ = v; markDirty(); }
    [[nodiscard]] bool isEffectivelyVisible() const noexcept;

    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }
    void setEnabled(bool e) noexcept { enabled_ = e; markDirty(); }

    [[nodiscard]] bool isHitTestVisible() const noexcept { return hitTestVisible_; }
    void setHitTestVisible(bool v) noexcept { hitTestVisible_ = v; }

    // --- Hit Testing & Event Propagation ---

    [[nodiscard]] virtual Element2D* hitTest(float globalX, float globalY) noexcept;

    virtual bool handlePointer([[maybe_unused]] const PointerEvent& ev, [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    virtual bool handleGesture([[maybe_unused]] const GestureRecognizer::GestureEvent& g, [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    virtual bool handleKey([[maybe_unused]] int key, [[maybe_unused]] int scancode, [[maybe_unused]] int action,
                           [[maybe_unused]] int mods, [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    /**
     * @brief Dispatch pointer event to hit element with bubble-up propagation to root.
     */
    bool dispatchPointer(const PointerEvent& ev, const ViewContext& ctx);

    // --- Dirty Tracking & Layout Lifecycle ---

    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }
    void markDirty() noexcept;
    void clearDirty() noexcept;

    [[nodiscard]] bool isLayoutDirty() const noexcept { return layoutDirty_; }
    void markLayoutDirty() noexcept { layoutDirty_ = true; markDirty(); }

    virtual void onLayout([[maybe_unused]] const Rect2D& bounds, [[maybe_unused]] const ViewContext& ctx) {}
    void layout(const Rect2D& bounds, const ViewContext& ctx);

    // --- Rendering Lifecycle ---

    virtual void onRender([[maybe_unused]] const ViewContext& ctx) {}
    virtual void render(const ViewContext& ctx);

protected:
    std::string id_{};
    Element2D* parent_{nullptr};
    std::vector<std::shared_ptr<Element2D>> children_{};

    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    bool visible_{true};
    bool enabled_{true};
    bool hitTestVisible_{true};
    bool clipping_{false};
    bool dirty_{true};
    bool layoutDirty_{true};
};

using Node2D = Element2D;

} // namespace eatsbits::ui

#endif // EATS_ELEMENT_2D_HPP
