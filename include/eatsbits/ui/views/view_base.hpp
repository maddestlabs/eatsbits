#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include "../geometry.hpp"
#include "../theme.hpp"
#include "../batch_renderer_2d.hpp"
#include "../draw_utils.hpp"
#include "../input/pointer_event.hpp"
#include "../widgets/value_edit_dialog.hpp"
#include "eatsbits/presenter/frame_time_context.hpp"

namespace eatsbits::audio {
    class AudioEngine;
}

namespace eatsbits::ui {

enum class WorkspaceView;
enum class EditSubView;
enum class DesignSubView;

struct ViewContext {
    BatchRenderer2D* renderer{nullptr};
    const ThemeTokens* theme{nullptr};
    audio::AudioEngine* audioEngine{nullptr};

    float screenWidth{1280.0f};
    float screenHeight{800.0f};
    float logicalWidth{1280.0f};
    float logicalHeight{800.0f};
    float uiScale{1.0f};
    float dpiScale{1.0f};
    FrameTimeContext time{};
    float dt{0.0166f};
    bool isMobile{false};
    float mouseX{-1.0f};
    float mouseY{-1.0f};

    // Global navigation callbacks matching Eatsbeats
    std::function<void(WorkspaceView)> onNavigateTab;
    std::function<void(uint32_t trackIdx, int clipIdx)> onJumpToClipEdit;
    std::function<void(EditSubView)> onSwitchEditSubView;
    std::function<void(DesignSubView)> onSwitchDesignSubView;
    std::function<void(bool)> onToggleBrowser;
    std::function<void(const std::string& msg)> onShowNotification;
    std::function<void(const ValueEditRequest&)> onOpenValueEdit;
};

class ViewBase {
public:
    virtual ~ViewBase() = default;

    virtual void layout(const Rect2D& bounds, [[maybe_unused]] const ViewContext& ctx) {
        bounds_ = bounds;
    }

    virtual void render(const ViewContext& ctx) = 0;

    virtual bool handlePointer([[maybe_unused]] const PointerEvent& ev, [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    virtual bool handleGesture([[maybe_unused]] const GestureRecognizer::GestureEvent& g, [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    virtual bool handleKey([[maybe_unused]] int key, [[maybe_unused]] int scancode, [[maybe_unused]] int action, [[maybe_unused]] int mods, [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    virtual bool handleFileDrop([[maybe_unused]] const std::vector<std::string>& filePaths,
                                [[maybe_unused]] float x, [[maybe_unused]] float y,
                                [[maybe_unused]] const ViewContext& ctx) {
        return false;
    }

    [[nodiscard]] const Rect2D& getBounds() const noexcept { return bounds_; }
    void setBounds(const Rect2D& b) noexcept { bounds_ = b; }

    [[nodiscard]] bool isVisible() const noexcept { return visible_; }
    void setVisible(bool v) noexcept { visible_ = v; }

protected:
    Rect2D bounds_{0.0f, 0.0f, 0.0f, 0.0f};
    bool visible_{true};
};

} // namespace eatsbits::ui
