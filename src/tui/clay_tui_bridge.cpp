#include "eatsbits/tui/clay_tui_bridge.hpp"
#include <string_view>
#include <cmath>

namespace eatsbits::tui {

ClayTuiBridge::ClayTuiBridge() = default;

void ClayTuiBridge::render(const Clay_RenderCommandArray& commands, CellSurface& surface, int offsetX, int offsetY) {
    for (int32_t i = 0; i < commands.length; ++i) {
        const Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(const_cast<Clay_RenderCommandArray*>(&commands), i);
        if (!cmd) continue;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                renderRectangle(*cmd, surface, offsetX, offsetY);
                break;

            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                renderBorder(*cmd, surface, offsetX, offsetY);
                break;

            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                renderText(*cmd, surface, offsetX, offsetY);
                break;

            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                if (customHandler_) {
                    Rect r = snapToCells(cmd->boundingBox);
                    r.x += offsetX;
                    r.y += offsetY;
                    customHandler_(*cmd, r, surface);
                }
                break;

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            case CLAY_RENDER_COMMAND_TYPE_NONE:
            default:
                break;
        }
    }
}

void ClayTuiBridge::renderRectangle(const Clay_RenderCommand& cmd, CellSurface& surface, int offsetX, int offsetY) {
    if (cmd.renderData.rectangle.backgroundColor.a == 0) return;

    Rect r = snapToCells(cmd.boundingBox);
    r.x += offsetX;
    r.y += offsetY;
    if (r.width <= 0 || r.height <= 0) return;

    Color bg = toTuiColor(cmd.renderData.rectangle.backgroundColor);
    surface.fillRect(r, bg, ' ');
}

void ClayTuiBridge::renderBorder(const Clay_RenderCommand& cmd, CellSurface& surface, int offsetX, int offsetY) {
    Rect r = snapToCells(cmd.boundingBox);
    r.x += offsetX;
    r.y += offsetY;
    if (r.width <= 0 || r.height <= 0) return;

    Color fg = toTuiColor(cmd.renderData.border.color);
    const auto& w = cmd.renderData.border.width;
    bool hasTop = (w.top > 0);
    bool hasBottom = (w.bottom > 0);
    bool hasLeft = (w.left > 0);
    bool hasRight = (w.right > 0);

    if (!hasTop && !hasBottom && !hasLeft && !hasRight) return;

    // Full box border
    if (hasTop && hasBottom && hasLeft && hasRight && r.width >= 2 && r.height >= 2) {
        bool rounded = (cmd.renderData.border.cornerRadius.topLeft > 0.0f);
        // Top and bottom borders
        for (int x = r.x + 1; x < r.x + r.width - 1; ++x) {
            surface.setCell(x, r.y, Cell{0x2500, fg, surface.getCell(x, r.y).bg, 0}); // ─
            surface.setCell(x, r.y + r.height - 1, Cell{0x2500, fg, surface.getCell(x, r.y + r.height - 1).bg, 0}); // ─
        }
        // Left and right borders
        for (int y = r.y + 1; y < r.y + r.height - 1; ++y) {
            surface.setCell(r.x, y, Cell{0x2502, fg, surface.getCell(r.x, y).bg, 0}); // │
            surface.setCell(r.x + r.width - 1, y, Cell{0x2502, fg, surface.getCell(r.x + r.width - 1, y).bg, 0}); // │
        }
        if (rounded) {
            surface.setCell(r.x, r.y, Cell{0x256D, fg, surface.getCell(r.x, r.y).bg, 0}); // ╭
            surface.setCell(r.x + r.width - 1, r.y, Cell{0x256E, fg, surface.getCell(r.x + r.width - 1, r.y).bg, 0}); // ╮
            surface.setCell(r.x, r.y + r.height - 1, Cell{0x2570, fg, surface.getCell(r.x, r.y + r.height - 1).bg, 0}); // ╰
            surface.setCell(r.x + r.width - 1, r.y + r.height - 1, Cell{0x256F, fg, surface.getCell(r.x + r.width - 1, r.y + r.height - 1).bg, 0}); // ╯
        } else {
            surface.setCell(r.x, r.y, Cell{0x250C, fg, surface.getCell(r.x, r.y).bg, 0}); // ┌
            surface.setCell(r.x + r.width - 1, r.y, Cell{0x2510, fg, surface.getCell(r.x + r.width - 1, r.y).bg, 0}); // ┐
            surface.setCell(r.x, r.y + r.height - 1, Cell{0x2514, fg, surface.getCell(r.x, r.y + r.height - 1).bg, 0}); // └
            surface.setCell(r.x + r.width - 1, r.y + r.height - 1, Cell{0x2518, fg, surface.getCell(r.x + r.width - 1, r.y + r.height - 1).bg, 0}); // ┘
        }
    } else {
        // Individual edges
        if (hasTop) {
            for (int x = r.x; x < r.x + r.width; ++x) {
                surface.setCell(x, r.y, Cell{0x2500, fg, surface.getCell(x, r.y).bg, 0});
            }
        }
        if (hasBottom && r.height > 1) {
            for (int x = r.x; x < r.x + r.width; ++x) {
                surface.setCell(x, r.y + r.height - 1, Cell{0x2500, fg, surface.getCell(x, r.y + r.height - 1).bg, 0});
            }
        }
        if (hasLeft) {
            for (int y = r.y; y < r.y + r.height; ++y) {
                surface.setCell(r.x, y, Cell{0x2502, fg, surface.getCell(r.x, y).bg, 0});
            }
        }
        if (hasRight && r.width > 1) {
            for (int y = r.y; y < r.y + r.height; ++y) {
                surface.setCell(r.x + r.width - 1, y, Cell{0x2502, fg, surface.getCell(r.x + r.width - 1, y).bg, 0});
            }
        }
    }
}

void ClayTuiBridge::renderText(const Clay_RenderCommand& cmd, CellSurface& surface, int offsetX, int offsetY) {
    if (cmd.renderData.text.stringContents.length <= 0 || !cmd.renderData.text.stringContents.chars) {
        return;
    }

    std::string_view text(cmd.renderData.text.stringContents.chars,
                          static_cast<size_t>(cmd.renderData.text.stringContents.length));

    Rect r = snapToCells(cmd.boundingBox);
    r.x += offsetX;
    r.y += offsetY;
    Color fg = toTuiColor(cmd.renderData.text.textColor);
    Color bg = surface.getCell(r.x, r.y).bg;

    surface.drawText(r.x, r.y, text, fg, bg, 0);
}

} // namespace eatsbits::tui
