#ifndef EATS_CORE_GEOMETRY_HPP
#define EATS_CORE_GEOMETRY_HPP

#include <algorithm>
#include <cmath>

namespace eatsbits::core {

struct Point2D {
    float x{0.0f};
    float y{0.0f};

    constexpr Point2D() noexcept = default;
    constexpr Point2D(float x_, float y_) noexcept : x(x_), y(y_) {}

    [[nodiscard]] constexpr Point2D operator+(const Point2D& o) const noexcept { return {x + o.x, y + o.y}; }
    [[nodiscard]] constexpr Point2D operator-(const Point2D& o) const noexcept { return {x - o.x, y - o.y}; }
    [[nodiscard]] constexpr Point2D operator*(float s) const noexcept { return {x * s, y * s}; }
    [[nodiscard]] float distanceTo(const Point2D& o) const noexcept {
        float dx = x - o.x, dy = y - o.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

struct Rect2D {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};

    constexpr Rect2D() noexcept = default;
    constexpr Rect2D(float x_, float y_, float w_, float h_) noexcept
        : x(x_), y(y_), w(w_), h(h_) {}

    [[nodiscard]] constexpr float left() const noexcept { return x; }
    [[nodiscard]] constexpr float top() const noexcept { return y; }
    [[nodiscard]] constexpr float right() const noexcept { return x + w; }
    [[nodiscard]] constexpr float bottom() const noexcept { return y + h; }
    [[nodiscard]] constexpr Point2D center() const noexcept { return {x + w * 0.5f, y + h * 0.5f}; }

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return w <= 0.0f || h <= 0.0f; }
    [[nodiscard]] constexpr bool contains(float px, float py) const noexcept {
        return px >= x && px <= (x + w) && py >= y && py <= (y + h);
    }
    [[nodiscard]] constexpr bool contains(const Point2D& p) const noexcept {
        return contains(p.x, p.y);
    }

    [[nodiscard]] constexpr bool intersects(const Rect2D& o) const noexcept {
        return !(x + w <= o.x || o.x + o.w <= x || y + h <= o.y || o.y + o.h <= y);
    }

    [[nodiscard]] Rect2D intersection(const Rect2D& o) const noexcept {
        float nx = std::max(x, o.x);
        float ny = std::max(y, o.y);
        float nw = std::max(0.0f, std::min(x + w, o.x + o.w) - nx);
        float nh = std::max(0.0f, std::min(y + h, o.y + o.h) - ny);
        return {nx, ny, nw, nh};
    }
};

} // namespace eatsbits::core

#endif // EATS_CORE_GEOMETRY_HPP
