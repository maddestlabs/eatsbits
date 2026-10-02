#include "eatsbits/ui/element2d.hpp"
#include "eatsbits/ui/views/view_base.hpp"

namespace eatsbits::ui {

Element2D::~Element2D() {
    for (auto& child : children_) {
        if (child) {
            child->parent_ = nullptr;
        }
    }
}

Element2D* Element2D::findById(const std::string& targetId) noexcept {
    if (id_ == targetId) {
        return this;
    }
    for (const auto& child : children_) {
        if (child) {
            if (auto* match = child->findById(targetId)) {
                return match;
            }
        }
    }
    return nullptr;
}

void Element2D::addChild(std::shared_ptr<Element2D> child) {
    if (!child) return;
    if (child->parent_) {
        child->removeFromParent();
    }
    child->parent_ = this;
    children_.push_back(std::move(child));
    markDirty();
}

void Element2D::insertChild(size_t index, std::shared_ptr<Element2D> child) {
    if (!child) return;
    if (child->parent_) {
        child->removeFromParent();
    }
    child->parent_ = this;
    size_t clampedIndex = std::min(index, children_.size());
    children_.insert(children_.begin() + clampedIndex, std::move(child));
    markDirty();
}

bool Element2D::removeChild(Element2D* child) {
    if (!child) return false;
    auto it = std::find_if(children_.begin(), children_.end(),
                           [child](const std::shared_ptr<Element2D>& p) { return p.get() == child; });
    if (it != children_.end()) {
        (*it)->parent_ = nullptr;
        children_.erase(it);
        markDirty();
        return true;
    }
    return false;
}

bool Element2D::removeChild(const std::shared_ptr<Element2D>& child) {
    return removeChild(child.get());
}

void Element2D::removeAllChildren() {
    for (auto& child : children_) {
        if (child) {
            child->parent_ = nullptr;
        }
    }
    children_.clear();
    markDirty();
}

void Element2D::removeFromParent() {
    if (parent_) {
        parent_->removeChild(this);
    }
}

Point Element2D::localToGlobal(const Point& localPt) const noexcept {
    Point pt = localPt;
    pt.x += bounds_.x;
    pt.y += bounds_.y;
    const Element2D* cur = parent_;
    while (cur) {
        pt.x += cur->bounds_.x;
        pt.y += cur->bounds_.y;
        cur = cur->parent_;
    }
    return pt;
}

Point Element2D::globalToLocal(const Point& globalPt) const noexcept {
    Point pt = globalPt;
    pt.x -= bounds_.x;
    pt.y -= bounds_.y;
    const Element2D* cur = parent_;
    while (cur) {
        pt.x -= cur->bounds_.x;
        pt.y -= cur->bounds_.y;
        cur = cur->parent_;
    }
    return pt;
}

Rect2D Element2D::getGlobalBounds() const noexcept {
    Point origin = localToGlobal(Point{0.0f, 0.0f});
    return Rect2D{origin.x, origin.y, bounds_.w, bounds_.h};
}

Rect2D Element2D::getGlobalClipRect() const noexcept {
    Rect2D clip = getGlobalBounds();
    const Element2D* cur = parent_;
    while (cur) {
        if (cur->clipping_) {
            Rect2D curGb = cur->getGlobalBounds();
            float nx = std::max(clip.x, curGb.x);
            float ny = std::max(clip.y, curGb.y);
            float nr = std::min(clip.right(), curGb.right());
            float nb = std::min(clip.bottom(), curGb.bottom());
            clip = Rect2D{nx, ny, std::max(0.0f, nr - nx), std::max(0.0f, nb - ny)};
        }
        cur = cur->parent_;
    }
    return clip;
}

bool Element2D::isEffectivelyVisible() const noexcept {
    if (!visible_) return false;
    const Element2D* cur = parent_;
    while (cur) {
        if (!cur->visible_) return false;
        cur = cur->parent_;
    }
    return true;
}

Element2D* Element2D::hitTest(float globalX, float globalY) noexcept {
    if (!visible_ || !enabled_) return nullptr;

    Rect2D gb = getGlobalBounds();
    if (clipping_ && !gb.contains(globalX, globalY)) {
        return nullptr;
    }

    // Traverse children in reverse order (topmost child first)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (*it) {
            auto* target = (*it)->hitTest(globalX, globalY);
            if (target) {
                return target;
            }
        }
    }

    if (hitTestVisible_ && gb.contains(globalX, globalY)) {
        return this;
    }

    return nullptr;
}

bool Element2D::dispatchPointer(const PointerEvent& ev, const ViewContext& ctx) {
    Element2D* target = hitTest(ev.x, ev.y);
    while (target) {
        if (target->handlePointer(ev, ctx)) {
            return true;
        }
        target = target->parent_;
    }
    return false;
}

void Element2D::markDirty() noexcept {
    dirty_ = true;
    if (parent_) {
        parent_->markDirty();
    }
}

void Element2D::clearDirty() noexcept {
    dirty_ = false;
    for (auto& child : children_) {
        if (child) {
            child->clearDirty();
        }
    }
}

void Element2D::layout(const Rect2D& bounds, const ViewContext& ctx) {
    bounds_ = bounds;
    layoutDirty_ = false;
    onLayout(bounds, ctx);
}

void Element2D::render(const ViewContext& ctx) {
    if (!visible_) return;
    onRender(ctx);
    for (auto& child : children_) {
        if (child) {
            child->render(ctx);
        }
    }
}

} // namespace eatsbits::ui
