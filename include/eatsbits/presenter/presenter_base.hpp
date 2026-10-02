#ifndef EATS_PRESENTER_BASE_HPP
#define EATS_PRESENTER_BASE_HPP

#include <cstdint>
#include <functional>

namespace eatsbits::presenter {

/**
 * @brief Foundational base class for all Model-View-Presenter (MVP) presenters in Eatsbits.
 * Provides lightweight, zero-allocation dirty tracking and revision management
 * across desktop GUI, terminal TUI, and offline headless rendering.
 */
class PresenterBase {
public:
    virtual ~PresenterBase() = default;

    [[nodiscard]] bool isDirty() const noexcept { return isDirty_; }
    void clearDirty() noexcept { isDirty_ = false; }
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    void setOnDirtyChanged(std::function<void()> cb) {
        onDirtyChanged_ = std::move(cb);
    }

protected:
    void markDirty() noexcept {
        isDirty_ = true;
        ++revision_;
        if (onDirtyChanged_) {
            onDirtyChanged_();
        }
    }

    uint64_t revision_{1};
    bool isDirty_{true};
    std::function<void()> onDirtyChanged_{nullptr};
};

} // namespace eatsbits::presenter

#endif // EATS_PRESENTER_BASE_HPP
