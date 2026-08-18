#pragma once

#include <cstdint>

namespace galaxian {

// 2D vector in logical pixels (docs/game_spec.md §3).
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2() = default;
    constexpr Vector2(float x, float y) : x(x), y(y) {}

    constexpr Vector2 operator+(const Vector2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vector2 operator-(const Vector2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vector2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vector2 operator/(float s) const { return {x / s, y / s}; }
    constexpr bool operator==(const Vector2& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vector2& o) const { return !(*this == o); }
};

// Axis-aligned rectangle in logical pixels (docs/game_spec.md §7).
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr Rect() = default;
    constexpr Rect(float x, float y, float width, float height)
        : x(x), y(y), width(width), height(height) {}

    constexpr float left() const { return x; }
    constexpr float top() const { return y; }
    constexpr float right() const { return x + width; }
    constexpr float bottom() const { return y + height; }
    constexpr Vector2 position() const { return {x, y}; }
    constexpr Vector2 size() const { return {width, height}; }
};

// 8-bit-per-channel color.
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    constexpr Color() = default;
    constexpr Color(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
        : r(r), g(g), b(b), a(a) {}

    constexpr bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
};

// Palette used by the Stage 3 dev scene (final palette lands in Stage 24).
namespace colors {
inline constexpr Color kBlack{0, 0, 0};
inline constexpr Color kWhite{255, 255, 255};
inline constexpr Color kPlayerCyan{0, 220, 255};
inline constexpr Color kEnemyRed{255, 64, 64};
inline constexpr Color kEnemyGreen{80, 255, 120};
inline constexpr Color kEnemyYellow{255, 220, 60};
inline constexpr Color kBullet{255, 255, 200};
inline constexpr Color kBorder{96, 96, 140};
inline constexpr Color kGreen{120, 255, 120};
}  // namespace colors

}  // namespace galaxian
