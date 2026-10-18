#pragma once

#include "clay.h"
#include "eatsbits/tui/cell_surface.hpp"
#include "eatsbits/tui/types.hpp"
#include <functional>

namespace eatsbits::tui {

/**
 * @brief Translates Clay layout commands into CellSurface terminal operations.
 * Snaps floating-point coordinates into discrete character cells.
 */
class ClayTuiBridge {
public:
    using CustomTuiRenderHandler = std::function<void(const Clay_RenderCommand& cmd, const Rect& cellRect, CellSurface& surface)>;

    ClayTuiBridge();
    ~ClayTuiBridge() = default;

    /**
     * @brief Renders the entire Clay command array onto the CellSurface.
     */
    void render(const Clay_RenderCommandArray& commands, CellSurface& surface, int offsetX = 0, int offsetY = 0);

    /**
     * @brief Registers a handler for CLAY_RENDER_COMMAND_TYPE_CUSTOM in terminal mode.
     */
    void setCustomRenderHandler(CustomTuiRenderHandler handler) {
        customHandler_ = std::move(handler);
    }

    /**
     * @brief Helper to convert Clay_Color to eatsbits::tui::Color.
     */
    static inline Color toTuiColor(const Clay_Color& c) noexcept {
        return Color::FromRgb(static_cast<uint8_t>(c.r),
                              static_cast<uint8_t>(c.g),
                              static_cast<uint8_t>(c.b));
    }

    /**
     * @brief Helper to convert eatsbits::tui::Color to Clay_Color.
     */
    static inline Clay_Color toClayColor(const Color& c) noexcept {
        return Clay_Color{ static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b), 255.0f };
    }

    /**
     * @brief Converts a std::string_view into a non-owning Clay_String.
     */
    static inline Clay_String toClayString(std::string_view sv) noexcept {
        return Clay_String{ false, static_cast<int32_t>(sv.size()), sv.data() };
    }

    /**
     * @brief Snaps floating point Clay_BoundingBox to integer Rect character cells.
     */
    static inline Rect snapToCells(const Clay_BoundingBox& box) noexcept {
        int x = static_cast<int>(std::round(box.x));
        int y = static_cast<int>(std::round(box.y));
        int w = std::max(0, static_cast<int>(std::round(box.width)));
        int h = std::max(0, static_cast<int>(std::round(box.height)));
        return Rect{ x, y, w, h };
    }

private:
    CustomTuiRenderHandler customHandler_{nullptr};

    void renderRectangle(const Clay_RenderCommand& cmd, CellSurface& surface, int offsetX, int offsetY);
    void renderBorder(const Clay_RenderCommand& cmd, CellSurface& surface, int offsetX, int offsetY);
    void renderText(const Clay_RenderCommand& cmd, CellSurface& surface, int offsetX, int offsetY);
};

} // namespace eatsbits::tui
