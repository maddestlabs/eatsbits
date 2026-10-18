#pragma once

#include "clay.h"
#include "eatsbits/ui/clay/clay_context.hpp"
#include "eatsbits/ui/theme.hpp"
#include <string_view>
#include <functional>

namespace eatsbits::ui::clay {

/**
 * @brief Helper to convert eatsbits::ui::Color into Clay_Color.
 */
inline Clay_Color toClayColor(const Color& c) noexcept {
    return Clay_Color{
        std::clamp(c.r * 255.0f, 0.0f, 255.0f),
        std::clamp(c.g * 255.0f, 0.0f, 255.0f),
        std::clamp(c.b * 255.0f, 0.0f, 255.0f),
        std::clamp(c.a * 255.0f, 0.0f, 255.0f)
    };
}

/**
 * @brief Visual styling configuration for UI_Button.
 */
struct ButtonStyle {
    Clay_Color normalBg{ 45, 48, 58, 255 };
    Clay_Color hoverBg{ 60, 65, 80, 255 };
    Clay_Color activeBg{ 30, 32, 40, 255 };
    Clay_Color textColor{ 240, 240, 240, 255 };
    Clay_CornerRadius cornerRadius{ 4.0f, 4.0f, 4.0f, 4.0f };
    Clay_Padding padding{ 12, 12, 6, 6 };
    float fontSize{ 13.0f };
};

/**
 * @brief Visual styling configuration for UI_Pill.
 */
struct PillStyle {
    Clay_Color selectedBg{ 255, 179, 0, 255 }; // Acid Amber
    Clay_Color selectedText{ 10, 12, 16, 255 };
    Clay_Color normalBg{ 32, 35, 44, 255 };
    Clay_Color normalText{ 180, 185, 195, 255 };
    Clay_CornerRadius cornerRadius{ 12.0f, 12.0f, 12.0f, 12.0f };
    Clay_Padding padding{ 10, 10, 4, 4 };
    float fontSize{ 11.0f };
};

/**
 * @brief Visual styling configuration for UI_Card.
 */
struct CardStyle {
    Clay_Color backgroundColor{ 22, 24, 30, 255 };
    Clay_Color headerBg{ 28, 31, 40, 255 };
    Clay_Color borderColor{ 50, 54, 66, 255 };
    Clay_Color titleColor{ 230, 235, 245, 255 };
    Clay_CornerRadius cornerRadius{ 6.0f, 6.0f, 6.0f, 6.0f };
    Clay_Padding padding{ 8, 8, 8, 8 };
    float headerHeight{ 26.0f };
};

/**
 * @brief Reusable declarative button element.
 */
inline void UI_Button(Clay_ElementId elementId,
                      std::string_view label,
                      bool isHovered = false,
                      bool isActive = false,
                      const ButtonStyle& style = ButtonStyle{}) {
    Clay_Color bg = isActive ? style.activeBg : (isHovered ? style.hoverBg : style.normalBg);

    CLAY(elementId, {
        .layout = {
            .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
            .padding = style.padding,
            .childGap = 6,
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = bg,
        .cornerRadius = style.cornerRadius,
    }) {
        Clay_String labelStr{ false, static_cast<int32_t>(label.size()), label.data() };
        CLAY_TEXT(labelStr, CLAY_TEXT_CONFIG({
            .textColor = style.textColor,
            .fontSize = static_cast<uint16_t>(style.fontSize)
        }));
    }
}

/**
 * @brief Reusable declarative pill / tab element.
 */
inline void UI_Pill(Clay_ElementId elementId,
                    std::string_view label,
                    bool isSelected,
                    bool isHovered = false,
                    const PillStyle& style = PillStyle{}) {
    Clay_Color bg = isSelected ? style.selectedBg : (isHovered ? Clay_Color{ 45, 50, 62, 255 } : style.normalBg);
    Clay_Color textCol = isSelected ? style.selectedText : style.normalText;

    CLAY(elementId, {
        .layout = {
            .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
            .padding = style.padding,
            .childGap = 4,
            .childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = bg,
        .cornerRadius = style.cornerRadius,
    }) {
        Clay_String labelStr{ false, static_cast<int32_t>(label.size()), label.data() };
        CLAY_TEXT(labelStr, CLAY_TEXT_CONFIG({
            .textColor = textCol,
            .fontSize = static_cast<uint16_t>(style.fontSize)
        }));
    }
}

/**
 * @brief Reusable collapsible card container with header and content.
 */
template <typename ContentFn>
inline void UI_Card(Clay_ElementId cardId,
                    std::string_view title,
                    bool isCollapsible,
                    bool isCollapsed,
                    ContentFn&& content,
                    const CardStyle& style = CardStyle{}) {
    CLAY(cardId, {
        .layout = {
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .padding = { 0, 0, 0, 0 },
            .childGap = 0,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = style.backgroundColor,
        .cornerRadius = style.cornerRadius,
        .border = { .color = style.borderColor, .width = { 1, 1, 1, 1, 0 } },
    }) {
        // Card Header
        CLAY(CLAY_IDI_LOCAL("Header", 0), {
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(style.headerHeight) },
                .padding = { 8, 8, 4, 4 },
                .childGap = 6,
                .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER },
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
            },
            .backgroundColor = style.headerBg,
            .cornerRadius = { style.cornerRadius.topLeft, style.cornerRadius.topRight, 0.0f, 0.0f },
        }) {
            if (isCollapsible) {
                Clay_String chevron = isCollapsed ? CLAY_STRING("▶") : CLAY_STRING("▼");
                CLAY_TEXT(chevron, CLAY_TEXT_CONFIG({
                    .textColor = { 160, 165, 175, 255 },
                    .fontSize = 10
                }));
            }

            Clay_String titleStr{ false, static_cast<int32_t>(title.size()), title.data() };
            CLAY_TEXT(titleStr, CLAY_TEXT_CONFIG({
                .textColor = style.titleColor,
                .fontSize = 12
            }));
        }

        // Card Content (only if expanded)
        if (!isCollapsed) {
            CLAY(CLAY_IDI_LOCAL("Body", 1), {
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                    .padding = style.padding,
                    .childGap = 4,
                    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                }
            }) {
                content();
            }
        }
    }
}

/**
 * @brief Reusable scrollable container.
 */
template <typename ItemsFn>
inline void UI_ScrollList(Clay_ElementId listId,
                          Clay_Sizing sizing,
                          ItemsFn&& items) {
    CLAY(listId, {
        .layout = {
            .sizing = sizing,
            .padding = { 0, 0, 0, 0 },
            .childGap = 2,
            .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .clip = { .horizontal = false, .vertical = true }
    }) {
        items();
    }
}

/**
 * @brief Declarative leaf element for custom GPU rendering (knobs, nixies, meters, scopes).
 */
inline void UI_CustomWidget(Clay_ElementId elementId,
                            Clay_Sizing sizing,
                            void* customData) {
    CLAY(elementId, {
        .layout = {
            .sizing = sizing,
        },
        .custom = { .customData = customData }
    }) {}
}

// ==============================================================================
// Fluent Declarative C++ RAII Builders & Layout Primitives (Unified Core)
// ==============================================================================

namespace detail {
    inline thread_local std::vector<std::string> s_threadLocalIdStack;
}

/**
 * @brief Resolves a hierarchical scoped string ID based on active ID scopes.
 */
inline std::string getActiveScopedId(std::string_view rawId) {
    auto* ctx = ClayContext::getCurrent();
    if (ctx) {
        return ctx->getScopedId(rawId);
    }
    if (detail::s_threadLocalIdStack.empty()) {
        return std::string(rawId);
    }
    std::string res;
    for (const auto& s : detail::s_threadLocalIdStack) {
        res += s;
        res += "/";
    }
    res += rawId;
    return res;
}

/**
 * @brief Constructs a Clay_ElementId taking the current hierarchical ID scope into account.
 */
inline Clay_ElementId UI_ID(std::string_view label) {
    auto* ctx = ClayContext::getCurrent();
    if (ctx) {
        return ctx->makeId(label, 0);
    }
    std::string scoped = getActiveScopedId(label);
    Clay_String cs{ false, static_cast<int32_t>(scoped.size()), scoped.data() };
    return Clay__HashString(cs, 0);
}

/**
 * @brief Constructs an indexed Clay_ElementId taking the current hierarchical ID scope into account.
 */
inline Clay_ElementId UI_IDI(std::string_view label, uint32_t index) {
    auto* ctx = ClayContext::getCurrent();
    if (ctx) {
        return ctx->makeId(label, index);
    }
    std::string scoped = getActiveScopedId(label);
    Clay_String cs{ false, static_cast<int32_t>(scoped.size()), scoped.data() };
    return Clay__HashStringWithOffset(cs, index, 0);
}

/**
 * @brief RAII ID Scoping helper. Automatically pushes and pops hierarchical namespaces.
 */
class IdScope {
public:
    explicit IdScope(std::string_view scopeName) {
        auto* ctx = ClayContext::getCurrent();
        if (ctx) {
            ctx->pushId(scopeName);
        } else {
            detail::s_threadLocalIdStack.emplace_back(scopeName);
        }
    }

    ~IdScope() {
        auto* ctx = ClayContext::getCurrent();
        if (ctx) {
            ctx->popId();
        } else if (!detail::s_threadLocalIdStack.empty()) {
            detail::s_threadLocalIdStack.pop_back();
        }
    }

    IdScope(const IdScope&) = delete;
    IdScope& operator=(const IdScope&) = delete;
    IdScope(IdScope&&) noexcept = default;
    IdScope& operator=(IdScope&&) noexcept = default;
};

/**
 * @brief RAII container scope that automatically invokes Clay__CloseElement() upon destruction.
 */
class ContainerScope {
public:
    ContainerScope() noexcept : active_(true) {}
    ~ContainerScope() {
        close();
    }

    void close() noexcept {
        if (active_) {
            Clay__CloseElement();
            active_ = false;
        }
    }

    ContainerScope(const ContainerScope&) = delete;
    ContainerScope& operator=(const ContainerScope&) = delete;

    ContainerScope(ContainerScope&& other) noexcept : active_(other.active_) {
        other.active_ = false;
    }

    ContainerScope& operator=(ContainerScope&& other) noexcept {
        if (this != &other) {
            close();
            active_ = other.active_;
            other.active_ = false;
        }
        return *this;
    }

private:
    bool active_{false};
};

/**
 * @brief Fluent Row container primitives (horizontal flow).
 */
struct Row {
    static void Fixed(float height, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        if (id.id != 0) Clay__OpenElementWithId(id);
        else Clay__OpenElement();
        Clay_ElementDeclaration decl{};
        decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        decl.layout.childGap = static_cast<uint16_t>(gap);
        decl.layout.padding = pad;
        decl.layout.sizing.width = CLAY_SIZING_GROW(0);
        decl.layout.sizing.height = CLAY_SIZING_FIXED(height);
        Clay__ConfigureOpenElement(decl);
    }

    [[nodiscard]] static ContainerScope ScopedFixed(float height, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        Fixed(height, gap, pad, id);
        return ContainerScope{};
    }

    static void Flex(float weight = 1.0f, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        (void)weight;
        if (id.id != 0) Clay__OpenElementWithId(id);
        else Clay__OpenElement();
        Clay_ElementDeclaration decl{};
        decl.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        decl.layout.childGap = static_cast<uint16_t>(gap);
        decl.layout.padding = pad;
        decl.layout.sizing.width = CLAY_SIZING_GROW(0);
        decl.layout.sizing.height = CLAY_SIZING_GROW(0);
        Clay__ConfigureOpenElement(decl);
    }

    [[nodiscard]] static ContainerScope ScopedFlex(float weight = 1.0f, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        Flex(weight, gap, pad, id);
        return ContainerScope{};
    }
};

/**
 * @brief Fluent Column container primitives (vertical flow).
 */
struct Column {
    static void Fixed(float width, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        if (id.id != 0) Clay__OpenElementWithId(id);
        else Clay__OpenElement();
        Clay_ElementDeclaration decl{};
        decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        decl.layout.childGap = static_cast<uint16_t>(gap);
        decl.layout.padding = pad;
        decl.layout.sizing.width = CLAY_SIZING_FIXED(width);
        decl.layout.sizing.height = CLAY_SIZING_GROW(0);
        Clay__ConfigureOpenElement(decl);
    }

    [[nodiscard]] static ContainerScope ScopedFixed(float width, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        Fixed(width, gap, pad, id);
        return ContainerScope{};
    }

    static void Flex(float weight = 1.0f, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        (void)weight;
        if (id.id != 0) Clay__OpenElementWithId(id);
        else Clay__OpenElement();
        Clay_ElementDeclaration decl{};
        decl.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        decl.layout.childGap = static_cast<uint16_t>(gap);
        decl.layout.padding = pad;
        decl.layout.sizing.width = CLAY_SIZING_GROW(0);
        decl.layout.sizing.height = CLAY_SIZING_GROW(0);
        Clay__ConfigureOpenElement(decl);
    }

    [[nodiscard]] static ContainerScope ScopedFlex(float weight = 1.0f, float gap = 0.0f, Clay_Padding pad = {}, Clay_ElementId id = {0}) {
        Flex(weight, gap, pad, id);
        return ContainerScope{};
    }
};

/**
 * @brief Fluent Stack container primitives (overlaid elements sharing bounds).
 */
struct Stack {
    static void Begin(Clay_ElementId id = {0}) {
        if (id.id != 0) Clay__OpenElementWithId(id);
        else Clay__OpenElement();
        Clay_ElementDeclaration decl{};
        decl.layout.sizing.width = CLAY_SIZING_GROW(0);
        decl.layout.sizing.height = CLAY_SIZING_GROW(0);
        Clay__ConfigureOpenElement(decl);
    }

    [[nodiscard]] static ContainerScope ScopedBegin(Clay_ElementId id = {0}) {
        Begin(id);
        return ContainerScope{};
    }
};

/**
 * @brief Declarative spacer element for inserting fixed or flex spacing gaps.
 */
inline void UI_Spacer(float size, bool isFlex = false) {
    Clay__OpenElement();
    Clay_ElementDeclaration decl{};
    if (isFlex) {
        decl.layout.sizing.width = CLAY_SIZING_GROW(0);
        decl.layout.sizing.height = CLAY_SIZING_GROW(0);
    } else {
        decl.layout.sizing.width = CLAY_SIZING_FIXED(size);
        decl.layout.sizing.height = CLAY_SIZING_FIXED(size);
    }
    Clay__ConfigureOpenElement(decl);
    Clay__CloseElement();
}

} // namespace eatsbits::ui::clay
