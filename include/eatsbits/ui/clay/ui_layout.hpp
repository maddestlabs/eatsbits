#pragma once

#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <functional>

// Clay Layout Engine & Bridges (Unified Core with Eatsbits & Eatscode)
#include "clay.h"
#include "eatsbits/ui/clay/clay_context.hpp"

namespace eatsbits::ui::clay {

enum class LayoutDirection : uint8_t {
    Column = 0, // Vertical: children flow top-to-bottom
    Row,        // Horizontal: children flow left-to-right
    Stack       // Overlaid: children overlay the exact same viewport in z-order
};

enum class SizingType : uint8_t {
    Fixed = 0,  // Exact pixel size
    Flex,       // Proportional weight of remaining space (e.g. 1.0, 2.0)
    Fill        // Fills parent container
};

struct LayoutConstraint {
    SizingType type{SizingType::Fill};
    float value{1.0f}; // pixels for Fixed, weight for Flex (default 1.0)

    static LayoutConstraint fixed(float px) noexcept {
        return {SizingType::Fixed, px};
    }
    static LayoutConstraint flex(float weight = 1.0f) noexcept {
        return {SizingType::Flex, weight};
    }
    static LayoutConstraint fill() noexcept {
        return {SizingType::Fill, 1.0f};
    }
};

struct LayoutRect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};

    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }
};

enum class WidgetType : uint8_t {
    Custom = 0,
    Panel = 1,
    Knob = 2,
    Slider = 3,
    Button = 4,
    Label = 5,
    Badge = 6,
    Meter = 7,
    TextEditor = 8,
    Spacer = 9
};

/**
 * @brief High-performance immediate-mode Flexbox layout node.
 * Evaluates in sub-microseconds without dynamic heap thrashing.
 */
struct LayoutNode {
    LayoutDirection direction{LayoutDirection::Column};
    LayoutConstraint mainAxis{LayoutConstraint::fill()};
    LayoutConstraint crossAxis{LayoutConstraint::fill()};
    float gap{0.0f};
    float padding{0.0f};

    LayoutRect resolvedRect{};
    bool isExplicitRect{false};

    bool isWidget{false};
    uint8_t widgetType{0};
    uint32_t colorRgba{0xFFFFFFFF};
    std::string id;
    std::string text;
    std::string callbackFn;

    // Interactive widget properties
    float value{0.0f};
    float minValue{0.0f};
    float maxValue{1.0f};
    uint32_t textureHandle{0};
    int totalFrames{0};
    bool isHorizontalFilmstrip{false};
    std::string styleName;
    uint32_t accentColor{0xFF21F4E8};

    uint32_t clayId{0};

    std::vector<std::shared_ptr<LayoutNode>> children;
    std::weak_ptr<LayoutNode> parent;
};

/**
 * @brief Immediate Mode Layout Engine for Eatsbits / Eatscode.
 * Powered by Nic Barker's Clay engine linear arena under the hood.
 * Bridges declarative nested script calls (ui_begin_column, ui_begin_row, ui_end)
 * with the underlying graphics (WebGPU / Dawn) and terminal (TUI) renderers.
 */
class UiLayoutEngine {
public:
    UiLayoutEngine() = default;

    void reset() {
        rootNode_.reset();
        activeNode_.reset();
        idStack_.clear();
        nextClayId_ = 0;
    }

    void beginRoot(float x, float y, float w, float h, LayoutDirection dir = LayoutDirection::Column) {
        rootNode_ = std::make_shared<LayoutNode>();
        rootNode_->direction = dir;
        rootNode_->resolvedRect = {x, y, w, h};
        rootNode_->mainAxis = LayoutConstraint::fixed(dir == LayoutDirection::Column ? h : w);
        rootNode_->crossAxis = LayoutConstraint::fixed(dir == LayoutDirection::Column ? w : h);
        activeNode_ = rootNode_;
    }

    void beginContainer(LayoutDirection dir,
                        LayoutConstraint mainAxis,
                        LayoutConstraint crossAxis = LayoutConstraint::fill(),
                        float gap = 0.0f,
                        float padding = 0.0f) {
        if (!activeNode_) {
            beginRoot(0.0f, 0.0f, 1440.0f, 900.0f, dir);
            return;
        }

        auto node = std::make_shared<LayoutNode>();
        node->direction = dir;
        node->mainAxis = mainAxis;
        node->crossAxis = crossAxis;
        node->gap = gap;
        node->padding = padding;
        node->parent = activeNode_;

        activeNode_->children.push_back(node);
        activeNode_ = node;
    }

    bool endContainer() {
        if (!activeNode_) return false;
        auto p = activeNode_->parent.lock();
        if (p) {
            activeNode_ = p;
            return true;
        }
        // At root container
        activeNode_ = nullptr;
        return true;
    }

    [[nodiscard]] bool isInsideContainer() const noexcept {
        return activeNode_ != nullptr;
    }

    [[nodiscard]] LayoutDirection getActiveDirection() const noexcept {
        return activeNode_ ? activeNode_->direction : LayoutDirection::Column;
    }

    void addWidget(uint8_t widgetType,
                   LayoutConstraint mainAxis,
                   LayoutConstraint crossAxis = LayoutConstraint::fill(),
                   uint32_t color = 0xFFFFFFFF,
                   std::string id = "",
                   std::string text = "",
                   std::string callback = "") {
        if (!activeNode_) return;

        auto node = std::make_shared<LayoutNode>();
        node->isWidget = true;
        node->widgetType = widgetType;
        node->mainAxis = mainAxis;
        node->crossAxis = crossAxis;
        node->colorRgba = color;
        node->id = getScopedId(id);
        node->text = std::move(text);
        node->callbackFn = std::move(callback);
        node->parent = activeNode_;

        activeNode_->children.push_back(std::move(node));
    }

    void addExplicitWidget(uint8_t widgetType,
                           float x, float y, float w, float h,
                           uint32_t color = 0xFFFFFFFF,
                           std::string id = "",
                           std::string text = "",
                           std::string callback = "") {
        if (!activeNode_) return;

        auto node = std::make_shared<LayoutNode>();
        node->isWidget = true;
        node->isExplicitRect = true;
        node->resolvedRect = {x, y, w, h};
        node->widgetType = widgetType;
        node->colorRgba = color;
        node->id = getScopedId(id);
        node->text = std::move(text);
        node->callbackFn = std::move(callback);
        node->parent = activeNode_;

        activeNode_->children.push_back(std::move(node));
    }

    void addSpacer(float size, bool isFlex = false) {
        if (!activeNode_) return;
        auto node = std::make_shared<LayoutNode>();
        node->isWidget = false;
        node->mainAxis = isFlex ? LayoutConstraint::flex(size) : LayoutConstraint::fixed(size);
        node->crossAxis = LayoutConstraint::fill();
        node->parent = activeNode_;
        activeNode_->children.push_back(std::move(node));
    }

    // --- ID Stack Management ---
    void pushId(const std::string& id) {
        idStack_.push_back(id);
    }

    void popId() {
        if (!idStack_.empty()) {
            idStack_.pop_back();
        }
    }

    [[nodiscard]] std::string getScopedId(const std::string& rawId) const {
        if (idStack_.empty()) return rawId;
        std::string res;
        for (const auto& s : idStack_) {
            res += s;
            res += "/";
        }
        res += rawId;
        return res;
    }

    // --- Clay Engine Integration & Multi-pass Solver ---
    [[nodiscard]] ClayContext& getClayContext() noexcept { return clayContext_; }
    [[nodiscard]] const ClayContext& getClayContext() const noexcept { return clayContext_; }
    [[nodiscard]] bool isClayBackendActive() const noexcept { return useClayBackend_; }
    void setClayBackendActive(bool enable) noexcept { useClayBackend_ = enable; }
    [[nodiscard]] const Clay_RenderCommandArray& getLatestClayCommands() const noexcept { return latestClayCommands_; }
    [[nodiscard]] std::shared_ptr<LayoutNode> getRootNode() const noexcept { return rootNode_; }

    void solve(const LayoutRect& rootBounds) {
        if (!rootNode_) return;

        if (useClayBackend_) {
            if (!clayContext_.isInitialized()) {
                clayContext_.initialize(rootBounds.w > 0.0f ? rootBounds.w : 1440.0f,
                                        rootBounds.h > 0.0f ? rootBounds.h : 900.0f,
                                        1024 * 1024);
            }
            clayContext_.setDimensions(rootBounds.w > 0.0f ? rootBounds.w : 1440.0f,
                                       rootBounds.h > 0.0f ? rootBounds.h : 900.0f);
            clayContext_.makeCurrent();
            clayContext_.beginLayout();

            uint32_t idCounter = 1;
            declareClayTree(rootNode_, idCounter, rootBounds);

            latestClayCommands_ = clayContext_.endLayout();

            syncClaySolvedBounds(rootNode_);
        }

        // Run hierarchical flex solver to handle any explicit overrides, custom weights, or fallback
        solveNode(rootNode_, rootBounds);
    }

    template <typename EmitFn>
    void emitSolvedCommands(EmitFn&& emitFn) {
        if (!rootNode_) return;
        collectCommands(rootNode_, emitFn);
    }

private:
    static void declareClayTree(const std::shared_ptr<LayoutNode>& node, uint32_t& idCounter, const LayoutRect& rootBounds) {
        // Critical lesson 2.1: skip explicit rect nodes so Clay flexbox does not displace them!
        if (node->isExplicitRect) {
            return;
        }

        node->clayId = idCounter++;
        Clay_ElementId clayId = CLAY_IDI("EatsNode", node->clayId);
        Clay__OpenElementWithId(clayId);

        Clay_ElementDeclaration decl{};
        decl.layout.layoutDirection = (node->direction == LayoutDirection::Row) ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
        decl.layout.childGap = static_cast<uint16_t>(node->gap);
        decl.layout.padding = {
            static_cast<uint16_t>(node->padding),
            static_cast<uint16_t>(node->padding),
            static_cast<uint16_t>(node->padding),
            static_cast<uint16_t>(node->padding)
        };

        auto p = node->parent.lock();
        if (!p) {
            decl.layout.sizing.width = CLAY_SIZING_FIXED(rootBounds.w > 0.0f ? rootBounds.w : 1440.0f);
            decl.layout.sizing.height = CLAY_SIZING_FIXED(rootBounds.h > 0.0f ? rootBounds.h : 900.0f);
        } else {
            bool isParentCol = (p->direction == LayoutDirection::Column);
            if (node->mainAxis.type == SizingType::Fixed) {
                if (isParentCol) {
                    decl.layout.sizing.height = CLAY_SIZING_FIXED(node->mainAxis.value);
                } else {
                    decl.layout.sizing.width = CLAY_SIZING_FIXED(node->mainAxis.value);
                }
            } else {
                if (isParentCol) {
                    decl.layout.sizing.height = CLAY_SIZING_GROW(0);
                } else {
                    decl.layout.sizing.width = CLAY_SIZING_GROW(0);
                }
            }

            if (node->crossAxis.type == SizingType::Fixed) {
                if (isParentCol) {
                    decl.layout.sizing.width = CLAY_SIZING_FIXED(node->crossAxis.value);
                } else {
                    decl.layout.sizing.height = CLAY_SIZING_FIXED(node->crossAxis.value);
                }
            } else {
                if (isParentCol) {
                    decl.layout.sizing.width = CLAY_SIZING_GROW(0);
                } else {
                    decl.layout.sizing.height = CLAY_SIZING_GROW(0);
                }
            }
        }

        if (node->isWidget) {
            // Lesson 2.2: Ensure visual property is set so render commands are generated
            decl.backgroundColor = {
                static_cast<float>((node->colorRgba >> 24) & 0xFF),
                static_cast<float>((node->colorRgba >> 16) & 0xFF),
                static_cast<float>((node->colorRgba >> 8) & 0xFF),
                static_cast<float>(node->colorRgba & 0xFF)
            };
        }

        Clay__ConfigureOpenElement(decl);

        for (const auto& child : node->children) {
            declareClayTree(child, idCounter, rootBounds);
        }

        Clay__CloseElement();
    }

    static void syncClaySolvedBounds(const std::shared_ptr<LayoutNode>& node) {
        if (!node->isExplicitRect && node->clayId > 0) {
            Clay_ElementData data = Clay_GetElementData(CLAY_IDI("EatsNode", node->clayId));
            if (data.found && data.boundingBox.width > 0.0f && data.boundingBox.height > 0.0f) {
                node->resolvedRect = {
                    data.boundingBox.x,
                    data.boundingBox.y,
                    data.boundingBox.width,
                    data.boundingBox.height
                };
            }
        }
        for (const auto& child : node->children) {
            syncClaySolvedBounds(child);
        }
    }

    static void solveNode(const std::shared_ptr<LayoutNode>& node, const LayoutRect& available) {
        node->resolvedRect = available;
        if (node->children.empty()) return;

        const float pad = node->padding;
        const float innerX = available.x + pad;
        const float innerY = available.y + pad;
        const float innerW = std::max(0.0f, available.w - 2.0f * pad);
        const float innerH = std::max(0.0f, available.h - 2.0f * pad);

        if (node->direction == LayoutDirection::Stack) {
            for (auto& child : node->children) {
                solveNode(child, {innerX, innerY, innerW, innerH});
            }
            return;
        }

        const bool isCol = (node->direction == LayoutDirection::Column);
        const float mainAvailable = isCol ? innerH : innerW;
        const float crossAvailable = isCol ? innerW : innerH;

        // 1. Calculate total fixed space and sum flex weights
        float totalFixed = 0.0f;
        float totalFlex = 0.0f;
        const size_t n = node->children.size();
        const float totalGaps = (n > 1) ? static_cast<float>(n - 1) * node->gap : 0.0f;

        for (const auto& child : node->children) {
            if (child->isExplicitRect) continue;
            if (child->mainAxis.type == SizingType::Fixed) {
                totalFixed += child->mainAxis.value;
            } else if (child->mainAxis.type == SizingType::Flex) {
                totalFlex += std::max(0.0001f, child->mainAxis.value);
            } else { // Fill
                totalFlex += 1.0f;
            }
        }

        const float remainingFlexSpace = std::max(0.0f, mainAvailable - totalFixed - totalGaps);

        // 2. Position children sequentially along main axis
        float curOffset = 0.0f;
        for (auto& child : node->children) {
            if (child->isExplicitRect) {
                solveNode(child, child->resolvedRect);
                continue;
            }

            float childMain = 0.0f;
            if (child->mainAxis.type == SizingType::Fixed) {
                childMain = child->mainAxis.value;
            } else {
                float weight = (child->mainAxis.type == SizingType::Flex) ? child->mainAxis.value : 1.0f;
                childMain = (totalFlex > 0.0f) ? (remainingFlexSpace * (weight / totalFlex)) : 0.0f;
            }

            float childCross = crossAvailable;
            if (child->crossAxis.type == SizingType::Fixed) {
                childCross = std::min(child->crossAxis.value, crossAvailable);
            }

            LayoutRect childRect;
            if (isCol) {
                childRect = {innerX, innerY + curOffset, childCross, childMain};
            } else {
                childRect = {innerX + curOffset, innerY, childMain, childCross};
            }

            solveNode(child, childRect);
            curOffset += childMain + node->gap;
        }
    }

    template <typename EmitFn>
    static void collectCommands(const std::shared_ptr<LayoutNode>& node, EmitFn& emitFn) {
        if (node->isWidget) {
            emitFn(node->widgetType,
                   node->resolvedRect.x,
                   node->resolvedRect.y,
                   node->resolvedRect.w,
                   node->resolvedRect.h,
                   node->colorRgba,
                   node->id,
                   node->text,
                   node->callbackFn);
        }
        for (const auto& child : node->children) {
            collectCommands(child, emitFn);
        }
    }

    std::shared_ptr<LayoutNode> rootNode_;
    std::shared_ptr<LayoutNode> activeNode_;
    std::vector<std::string> idStack_;
    uint32_t nextClayId_{0};

    ClayContext clayContext_;
    bool useClayBackend_{true};
    Clay_RenderCommandArray latestClayCommands_{};
};

// ==============================================================================
// Fluent Declarative C++ RAII Builders targeting UiLayoutEngine
// ==============================================================================

class EngineIdScope {
public:
    EngineIdScope(UiLayoutEngine& engine, const std::string& id)
        : engine_(engine) {
        engine_.pushId(id);
    }
    ~EngineIdScope() {
        engine_.popId();
    }
    EngineIdScope(const EngineIdScope&) = delete;
    EngineIdScope& operator=(const EngineIdScope&) = delete;
private:
    UiLayoutEngine& engine_;
};

class EngineContainerScope {
public:
    explicit EngineContainerScope(UiLayoutEngine& engine)
        : engine_(engine) {}
    ~EngineContainerScope() {
        engine_.endContainer();
    }
    EngineContainerScope(const EngineContainerScope&) = delete;
    EngineContainerScope& operator=(const EngineContainerScope&) = delete;
private:
    UiLayoutEngine& engine_;
};

struct EngineRow {
    static void Fixed(UiLayoutEngine& engine, float height, float gap = 0.0f, float padding = 0.0f) {
        engine.beginContainer(LayoutDirection::Row, LayoutConstraint::fixed(height), LayoutConstraint::fill(), gap, padding);
    }
    static void Flex(UiLayoutEngine& engine, float weight = 1.0f, float gap = 0.0f, float padding = 0.0f) {
        engine.beginContainer(LayoutDirection::Row, LayoutConstraint::flex(weight), LayoutConstraint::fill(), gap, padding);
    }
};

struct EngineColumn {
    static void Fixed(UiLayoutEngine& engine, float width, float gap = 0.0f, float padding = 0.0f) {
        engine.beginContainer(LayoutDirection::Column, LayoutConstraint::fixed(width), LayoutConstraint::fill(), gap, padding);
    }
    static void Flex(UiLayoutEngine& engine, float weight = 1.0f, float gap = 0.0f, float padding = 0.0f) {
        engine.beginContainer(LayoutDirection::Column, LayoutConstraint::flex(weight), LayoutConstraint::fill(), gap, padding);
    }
};

struct EngineStack {
    static void Begin(UiLayoutEngine& engine) {
        engine.beginContainer(LayoutDirection::Stack, LayoutConstraint::fill(), LayoutConstraint::fill());
    }
};

} // namespace eatsbits::ui::clay
