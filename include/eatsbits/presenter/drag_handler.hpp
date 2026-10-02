#ifndef EATS_PRESENTER_DRAG_HANDLER_HPP
#define EATS_PRESENTER_DRAG_HANDLER_HPP

#include "eatsbits/presenter/drag_types.hpp"
#include "eatsbits/input/pointer_event.hpp"

namespace eatsbits::presenter {

/**
 * @brief Abstract interface for all headless drag & interaction state machines in Eatsbits.
 * Enables polymorphic event routing from GuiWindow or TUI gestures with zero graphics coupling.
 */
class IDragHandler {
public:
    virtual ~IDragHandler() = default;

    [[nodiscard]] virtual bool isDragging() const noexcept = 0;

    virtual void onPointerDown(const ui::PointerEvent& ev) { (void)ev; }
    virtual void onPointerMove(const ui::PointerEvent& ev) = 0;
    virtual void onPointerUp(const ui::PointerEvent& ev) = 0;
    virtual void cancelDrag() = 0;

    // Backward-compatibility delegating overloads
    virtual void onPointerMove(float x, float y) {
        ui::PointerEvent ev{};
        ev.x = x;
        ev.y = y;
        ev.action = ui::PointerAction::Move;
        onPointerMove(ev);
    }

    virtual void onPointerUp(float x, float y, int button = 0) {
        ui::PointerEvent ev{};
        ev.x = x;
        ev.y = y;
        ev.button = static_cast<ui::PointerButton>(button == 0 ? 1 : button);
        ev.action = ui::PointerAction::Up;
        onPointerUp(ev);
    }

    [[nodiscard]] virtual ui::DragMode getDragMode() const noexcept = 0;
};

} // namespace eatsbits::presenter

#endif // EATS_PRESENTER_DRAG_HANDLER_HPP
