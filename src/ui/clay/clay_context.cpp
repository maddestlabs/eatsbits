#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "eatsbits/ui/clay/clay_context.hpp"
#include <iostream>
#include <algorithm>

namespace eatsbits::ui::clay {

ClayContext::ClayContext() = default;

ClayContext::~ClayContext() {
    shutdown();
}

ClayContext::ClayContext(ClayContext&& other) noexcept
    : arenaMemory_(std::move(other.arenaMemory_))
    , arena_(other.arena_)
    , context_(other.context_)
    , measureTextFn_(std::move(other.measureTextFn_)) {
    other.arena_ = {};
    other.context_ = nullptr;
}

ClayContext& ClayContext::operator=(ClayContext&& other) noexcept {
    if (this != &other) {
        shutdown();
        arenaMemory_ = std::move(other.arenaMemory_);
        arena_ = other.arena_;
        context_ = other.context_;
        measureTextFn_ = std::move(other.measureTextFn_);
        other.arena_ = {};
        other.context_ = nullptr;
    }
    return *this;
}

bool ClayContext::initialize(float initialWidth, float initialHeight, size_t arenaSizeBytes) {
    shutdown();

    uint32_t minMemory = Clay_MinMemorySize();
    size_t allocSize = std::max<size_t>(arenaSizeBytes, static_cast<size_t>(minMemory));

    arenaMemory_.resize(allocSize);
    arena_ = Clay_CreateArenaWithCapacityAndMemory(allocSize, arenaMemory_.data());

    Clay_Dimensions dims{ initialWidth, initialHeight };
    Clay_ErrorHandler errorHandler{ clayErrorHandler, this };

    context_ = Clay_Initialize(arena_, dims, errorHandler);
    if (!context_) {
        arenaMemory_.clear();
        return false;
    }

    Clay_SetCurrentContext(context_);

    // Set default fallback text measurement
    setDefaultMonospaceMeasure(8.0f, 16.0f);

    return true;
}

void ClayContext::shutdown() {
    if (s_currentContext_ == this) {
        s_currentContext_ = nullptr;
    }
    if (context_) {
        if (Clay_GetCurrentContext() == context_) {
            Clay_SetCurrentContext(nullptr);
        }
        context_ = nullptr;
    }
    arenaMemory_.clear();
    arena_ = {};
    measureTextFn_ = nullptr;
    idStack_.clear();
}

void ClayContext::setDimensions(float width, float height) {
    if (!context_) return;
    makeCurrent();
    Clay_SetLayoutDimensions(Clay_Dimensions{ width, height });
}

Clay_Dimensions ClayContext::getDimensions() const {
    if (!context_) return Clay_Dimensions{ 0.0f, 0.0f };
    return Clay_GetLayoutDimensions();
}

void ClayContext::setPointerState(float x, float y, bool isMouseDown) {
    if (!context_) return;
    makeCurrent();
    Clay_SetPointerState(Clay_Vector2{ x, y }, isMouseDown);
}

Clay_PointerData ClayContext::getPointerState() const {
    if (!context_) return Clay_PointerData{};
    return Clay_GetPointerState();
}

void ClayContext::updateScrollContainers(bool enableDragScrolling, float scrollDeltaX, float scrollDeltaY, float deltaTime) {
    if (!context_) return;
    makeCurrent();
    Clay_UpdateScrollContainers(enableDragScrolling, Clay_Vector2{ scrollDeltaX, scrollDeltaY }, deltaTime);
}

void ClayContext::beginLayout() {
    if (!context_) return;
    makeCurrent();
    idStack_.clear();
    Clay_BeginLayout();
}

Clay_RenderCommandArray ClayContext::endLayout(float deltaTime) {
    if (!context_) return Clay_RenderCommandArray{};
    makeCurrent();
    return Clay_EndLayout(deltaTime);
}

bool ClayContext::isPointerOver(Clay_ElementId elementId) const {
    if (!context_) return false;
    return Clay_PointerOver(elementId);
}

Clay_ElementData ClayContext::getElementData(Clay_ElementId elementId) const {
    if (!context_) return Clay_ElementData{ Clay_BoundingBox{ 0, 0, 0, 0 }, false };
    return Clay_GetElementData(elementId);
}

void ClayContext::setMeasureTextFunction(MeasureTextFn fn) {
    measureTextFn_ = std::move(fn);
    if (context_) {
        makeCurrent();
        Clay_SetMeasureTextFunction(clayMeasureTextCallback, this);
    }
}

void ClayContext::setDefaultMonospaceMeasure(float charWidth, float charHeight) {
    setMeasureTextFunction([charWidth, charHeight](std::string_view text, const Clay_TextElementConfig* config) -> Clay_Dimensions {
        float scale = 1.0f;
        if (config && config->fontSize > 0) {
            scale = static_cast<float>(config->fontSize) / 14.0f;
        }
        float w = static_cast<float>(text.length()) * charWidth * scale;
        float h = charHeight * scale;
        if (config && config->lineHeight > 0) {
            h = static_cast<float>(config->lineHeight);
        }
        return Clay_Dimensions{ w, h };
    });
}

thread_local ClayContext* ClayContext::s_currentContext_ = nullptr;

void ClayContext::makeCurrent() {
    s_currentContext_ = this;
    if (context_ && Clay_GetCurrentContext() != context_) {
        Clay_SetCurrentContext(context_);
    }
}

ClayContext* ClayContext::getCurrent() noexcept {
    return s_currentContext_;
}

void ClayContext::pushId(std::string_view id) {
    idStack_.emplace_back(id);
}

void ClayContext::popId() {
    if (!idStack_.empty()) {
        idStack_.pop_back();
    }
}

std::string ClayContext::getScopedId(std::string_view rawId) const {
    if (idStack_.empty()) return std::string(rawId);
    std::string res;
    for (const auto& s : idStack_) {
        res += s;
        res += "/";
    }
    res += rawId;
    return res;
}

Clay_ElementId ClayContext::makeId(std::string_view label, uint32_t index) const {
    std::string scoped = getScopedId(label);
    Clay_String cs{ false, static_cast<int32_t>(scoped.size()), scoped.data() };
    if (index > 0) {
        return Clay__HashStringWithOffset(cs, index, 0);
    }
    return Clay__HashString(cs, 0);
}

Clay_Dimensions ClayContext::clayMeasureTextCallback(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    auto* self = static_cast<ClayContext*>(userData);
    if (self && self->measureTextFn_) {
        std::string_view sv(text.chars, static_cast<size_t>(text.length));
        return self->measureTextFn_(sv, config);
    }
    // Minimal fallback
    float len = static_cast<float>(text.length);
    float fs = (config && config->fontSize > 0) ? static_cast<float>(config->fontSize) : 14.0f;
    return Clay_Dimensions{ len * (fs * 0.6f), fs * 1.25f };
}

void ClayContext::clayErrorHandler(Clay_ErrorData error) {
    std::string_view msg(error.errorText.chars, static_cast<size_t>(error.errorText.length));
    std::cerr << "[Clay Error " << static_cast<int>(error.errorType) << "]: " << msg << std::endl;
}

} // namespace eatsbits::ui::clay
