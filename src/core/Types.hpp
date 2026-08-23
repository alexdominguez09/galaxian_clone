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

    constexpr bool operator==(const Rect& o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    constexpr bool operator!=(const Rect& o) const { return !(*this == o); }
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
// Stage 24 arcade palette (limited, reference-inspired).
inline constexpr Color kPlayerHull{235, 235, 240};   // white fighter
inline constexpr Color kPlayerStripe{225, 45, 65};   // red wing stripes
inline constexpr Color kEnemyRed{235, 55, 55};
inline constexpr Color kEnemyGreen{80, 255, 120};
inline constexpr Color kEnemyYellow{255, 220, 60};
inline constexpr Color kEnemyBlue{70, 150, 255};     // drone wings/body
inline constexpr Color kEnemyCyan{90, 225, 235};     // drone light tone
inline constexpr Color kEnemyMagenta{235, 60, 220};  // drone accents
inline constexpr Color kEnemyOrange{255, 150, 40};
inline constexpr Color kEnemyNavy{25, 35, 110};      // drone outline
inline constexpr Color kBullet{255, 255, 200};
inline constexpr Color kBorder{96, 96, 140};
inline constexpr Color kGreen{120, 255, 120};
inline constexpr Color kStarDim{70, 70, 110};
inline constexpr Color kStarMid{140, 140, 170};
inline constexpr Color kStarNear{230, 230, 255};
// Stage 7: F1 debug collision-box outline (magenta, unused elsewhere).
inline constexpr Color kDebugBox{255, 0, 255};
// Stage 9: placeholder destruction-effect box (white; Stage 19 replaces the
// placeholder with the explosion animation and may change this color).
inline constexpr Color kEffect{255, 255, 255};
}  // namespace colors

}  // namespace galaxian
