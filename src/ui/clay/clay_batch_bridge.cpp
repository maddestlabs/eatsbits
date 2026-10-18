#include "eatsbits/ui/clay/clay_batch_bridge.hpp"
#include "eatsbits/ui/draw_utils.hpp"
#include <string_view>
#include <algorithm>

namespace eatsbits::ui::clay {

ClayBatchBridge::ClayBatchBridge() = default;

void ClayBatchBridge::render(const Clay_RenderCommandArray& commands, BatchRenderer2D& r) {
    for (int32_t i = 0; i < commands.length; ++i) {
        const Clay_RenderCommand* cmd = Clay_RenderCommandArray_Get(const_cast<Clay_RenderCommandArray*>(&commands), i);
        if (!cmd) continue;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                renderRectangle(*cmd, r);
                break;

            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                renderBorder(*cmd, r);
                break;

            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                renderText(*cmd, r);
                break;

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                r.pushScissor(cmd->boundingBox.x, cmd->boundingBox.y,
                              cmd->boundingBox.width, cmd->boundingBox.height);
                break;

            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                r.popScissor();
                break;

            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                if (customHandler_) {
                    customHandler_(*cmd, r);
                }
                break;

            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                if (cmd->renderData.image.imageData) {
                    // Supported via custom texture render or r.drawRgbaBitmap
                }
                break;

            case CLAY_RENDER_COMMAND_TYPE_NONE:
            default:
                break;
        }
    }
}

void ClayBatchBridge::renderRectangle(const Clay_RenderCommand& cmd, BatchRenderer2D& r) {
    Color col = toEatsColor(cmd.renderData.rectangle.backgroundColor);
    if (col.a <= 0.001f) return;

    float radius = cmd.renderData.rectangle.cornerRadius.topLeft;
    if (radius > 0.5f) {
        r.drawRoundedRect(cmd.boundingBox.x, cmd.boundingBox.y,
                          cmd.boundingBox.width, cmd.boundingBox.height,
                          radius, col.r, col.g, col.b, col.a);
    } else {
        r.drawRect(cmd.boundingBox.x, cmd.boundingBox.y,
                   cmd.boundingBox.width, cmd.boundingBox.height,
                   col.r, col.g, col.b, col.a);
    }
}

void ClayBatchBridge::renderBorder(const Clay_RenderCommand& cmd, BatchRenderer2D& r) {
    Color col = toEatsColor(cmd.renderData.border.color);
    if (col.a <= 0.001f) return;

    const auto& w = cmd.renderData.border.width;
    float radius = cmd.renderData.border.cornerRadius.topLeft;

    // Check if border is uniform around all 4 sides
    if (w.top == w.bottom && w.left == w.right && w.top == w.left && w.top > 0) {
        float lineWidth = static_cast<float>(w.top);
        if (radius > 0.5f) {
            r.drawRoundedRectOutline(cmd.boundingBox.x, cmd.boundingBox.y,
                                     cmd.boundingBox.width, cmd.boundingBox.height,
                                     radius, col.r, col.g, col.b, col.a, lineWidth);
        } else {
            r.drawRectOutline(cmd.boundingBox.x, cmd.boundingBox.y,
                              cmd.boundingBox.width, cmd.boundingBox.height,
                              col.r, col.g, col.b, col.a, lineWidth);
        }
    } else {
        // Individual non-uniform border segments
        float bx = cmd.boundingBox.x;
        float by = cmd.boundingBox.y;
        float bw = cmd.boundingBox.width;
        float bh = cmd.boundingBox.height;

        if (w.top > 0) {
            r.drawRect(bx, by, bw, static_cast<float>(w.top), col.r, col.g, col.b, col.a);
        }
        if (w.bottom > 0) {
            r.drawRect(bx, by + bh - static_cast<float>(w.bottom), bw, static_cast<float>(w.bottom), col.r, col.g, col.b, col.a);
        }
        if (w.left > 0) {
            r.drawRect(bx, by, static_cast<float>(w.left), bh, col.r, col.g, col.b, col.a);
        }
        if (w.right > 0) {
            r.drawRect(bx + bw - static_cast<float>(w.right), by, static_cast<float>(w.right), bh, col.r, col.g, col.b, col.a);
        }
    }
}

void ClayBatchBridge::renderText(const Clay_RenderCommand& cmd, BatchRenderer2D& r) {
    if (cmd.renderData.text.stringContents.length <= 0 || !cmd.renderData.text.stringContents.chars) {
        return;
    }

    std::string_view text(cmd.renderData.text.stringContents.chars,
                          static_cast<size_t>(cmd.renderData.text.stringContents.length));
    Color col = toEatsColor(cmd.renderData.text.textColor);
    float fontSize = cmd.renderData.text.fontSize > 0 ? static_cast<float>(cmd.renderData.text.fontSize) : 13.0f;

    drawText(r, text, cmd.boundingBox.x, cmd.boundingBox.y, fontSize, col);
}

} // namespace eatsbits::ui::clay
