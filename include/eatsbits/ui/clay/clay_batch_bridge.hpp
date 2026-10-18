#pragma once

#include "clay.h"
#include "eatsbits/ui/batch_renderer_2d.hpp"
#include "eatsbits/ui/theme.hpp"
#include <functional>

namespace eatsbits::ui::clay {

/**
 * @brief Translates Clay render command arrays into BatchRenderer2D GPU draw calls.
 */
class ClayBatchBridge {
public:
    using CustomRenderHandler = std::function<void(const Clay_RenderCommand& cmd, BatchRenderer2D& r)>;

    ClayBatchBridge();
    ~ClayBatchBridge() = default;

    /**
     * @brief Renders the entire command array using BatchRenderer2D.
     */
    void render(const Clay_RenderCommandArray& commands, BatchRenderer2D& r);

    /**
     * @brief Registers a handler for CLAY_RENDER_COMMAND_TYPE_CUSTOM.
     */
    void setCustomRenderHandler(CustomRenderHandler handler) {
        customHandler_ = std::move(handler);
    }

    /**
     * @brief Helper to convert Clay_Color (0-255) to eatsbits::ui::Color (0.0f-1.0f).
     */
    static inline Color toEatsColor(const Clay_Color& c) noexcept {
        return Color{
            static_cast<float>(c.r) / 255.0f,
            static_cast<float>(c.g) / 255.0f,
            static_cast<float>(c.b) / 255.0f,
            static_cast<float>(c.a) / 255.0f
        };
    }

private:
    CustomRenderHandler customHandler_{nullptr};

    void renderRectangle(const Clay_RenderCommand& cmd, BatchRenderer2D& r);
    void renderBorder(const Clay_RenderCommand& cmd, BatchRenderer2D& r);
    void renderText(const Clay_RenderCommand& cmd, BatchRenderer2D& r);
};

} // namespace eatsbits::ui::clay
