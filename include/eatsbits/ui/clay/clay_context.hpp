#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <string_view>
#include <functional>
#include "clay.h"

namespace eatsbits::ui::clay {

/**
 * @brief Manages a dedicated Clay memory arena, lifecycle, and layout pipeline.
 * Suitable for both Graphical desktop/web apps (pixels) and Terminal TUIs (character cells).
 */
class ClayContext {
public:
    ClayContext();
    ~ClayContext();

    ClayContext(const ClayContext&) = delete;
    ClayContext& operator=(const ClayContext&) = delete;
    ClayContext(ClayContext&&) noexcept;
    ClayContext& operator=(ClayContext&&) noexcept;

    /**
     * @brief Allocates the arena and initializes Clay.
     * @param initialWidth Screen width (pixels for GUI, columns for TUI).
     * @param initialHeight Screen height (pixels for GUI, rows for TUI).
     * @param arenaSizeBytes Size of the linear arena in bytes (default: 4 MB).
     */
    bool initialize(float initialWidth, float initialHeight, size_t arenaSizeBytes = 4 * 1024 * 1024);

    /**
     * @brief Destroys and cleans up the arena.
     */
    void shutdown();

    [[nodiscard]] bool isInitialized() const noexcept { return context_ != nullptr; }

    /**
     * @brief Updates layout dimensions in response to window / terminal resize.
     */
    void setDimensions(float width, float height);
    [[nodiscard]] Clay_Dimensions getDimensions() const;

    /**
     * @brief Updates pointer / mouse coordinates and button state.
     */
    void setPointerState(float x, float y, bool isMouseDown);
    [[nodiscard]] Clay_PointerData getPointerState() const;

    /**
     * @brief Updates scroll container positions and momentum scrolling.
     */
    void updateScrollContainers(bool enableDragScrolling, float scrollDeltaX, float scrollDeltaY, float deltaTime);

    /**
     * @brief Starts a new frame layout definition pass.
     */
    void beginLayout();

    /**
     * @brief Finishes layout declarations and returns computed render commands.
     */
    Clay_RenderCommandArray endLayout(float deltaTime = 0.01666f);

    /**
     * @brief Queries if the pointer is currently within the bounding box of an element.
     */
    [[nodiscard]] bool isPointerOver(Clay_ElementId elementId) const;

    /**
     * @brief Queries calculated layout data (bounding box) for an element by ID.
     */
    [[nodiscard]] Clay_ElementData getElementData(Clay_ElementId elementId) const;

    /**
     * @brief Custom text measurement function signature.
     */
    using MeasureTextFn = std::function<Clay_Dimensions(std::string_view text, const Clay_TextElementConfig* config)>;
    void setMeasureTextFunction(MeasureTextFn fn);

    /**
     * @brief Sets a default monospace text measurement function (width = chars * charWidth, height = charHeight).
     */
    void setDefaultMonospaceMeasure(float charWidth = 1.0f, float charHeight = 1.0f);

    [[nodiscard]] Clay_Context* getRawContext() noexcept { return context_; }

    /**
     * @brief Makes this context the active thread-local Clay context.
     */
    void makeCurrent();

    /**
     * @brief Gets the active thread-local ClayContext, or nullptr if none is currently active.
     */
    [[nodiscard]] static ClayContext* getCurrent() noexcept;

    /**
     * @brief Hierarchical ID Stack management for namespaced element IDs.
     */
    void pushId(std::string_view id);
    void popId();
    [[nodiscard]] std::string getScopedId(std::string_view rawId) const;
    [[nodiscard]] Clay_ElementId makeId(std::string_view label, uint32_t index = 0) const;

private:
    std::vector<uint8_t> arenaMemory_;
    Clay_Arena arena_{};
    Clay_Context* context_{nullptr};
    MeasureTextFn measureTextFn_{nullptr};
    std::vector<std::string> idStack_;

    static thread_local ClayContext* s_currentContext_;

    static Clay_Dimensions clayMeasureTextCallback(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);
    static void clayErrorHandler(Clay_ErrorData error);
};

} // namespace eatsbits::ui::clay
