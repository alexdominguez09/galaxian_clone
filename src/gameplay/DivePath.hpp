#pragma once

#include <string_view>

#include "core/Types.hpp"

namespace galaxian {

// Cubic Bezier curve, P(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 +
// t^3 P3 (docs/game_spec.md §6.4, docs/architecture.md §3.6). Pure SDL-free
// math: no game state, no time, no I/O.
//
// Exactness: P(0) == p0 and P(1) == p3 bit-exactly (endpoint shortcuts);
// interior points use de Casteljau's algorithm, which is numerically stable
// for t in [0, 1].
struct CubicBezier {
    Vector2 p0{0.0f, 0.0f};
    Vector2 p1{0.0f, 0.0f};
    Vector2 p2{0.0f, 0.0f};
    Vector2 p3{0.0f, 0.0f};

    // Curve point at parameter t (clamped to [0, 1]).
    Vector2 evaluate(float t) const;

    // A copy of this curve whose end point is replaced. Used by return
    // paths, whose end is the LIVE formation slot (spec §6.2 indirection):
    // the tail of the curve tracks the slot while the shape stays put, and
    // P(1) is always exactly the live end.
    CubicBezier withEnd(Vector2 newP3) const;
};

// The four dive patterns (docs/game_spec.md §6.4): left dive, right dive,
// center attack, and the return path.
enum class DivePattern {
    LeftDive,
    RightDive,
    CenterAttack,
    ReturnPath,
};

// Short upper-case label for debug displays.
inline std::string_view divePatternName(DivePattern pattern)
{
    switch (pattern) {
        case DivePattern::LeftDive:     return "LEFT DIVE";
        case DivePattern::RightDive:    return "RIGHT DIVE";
        case DivePattern::CenterAttack: return "CENTER ATTACK";
        case DivePattern::ReturnPath:   return "RETURN PATH";
    }
    return "?";
}

// A complete dive trajectory: a cubic Bezier shaped by deterministic
// RELATIVE control-point offsets from the start position, plus a sampled
// arc length for speed pacing.
//
// Shape constants (dev-tuned, documented here on purpose; Stage 21 makes
// them configurable and Stage 23 tunes them):
//   Left dive:    P1 = S + (-72, -36), P2 = S + (-192, +240), P3 = S + (-96, +416)
//   Right dive:   mirrored x
//   Center attack: P1 = S + (0, -24),  P2 = S + (0, +200),   P3 = S + (0, +416)
//   Return path:  P0 = E (attack end), P3 = T (slot);
//                 P1 = E + (side * 48, +24), P2 = T + (side * 96, -96),
//                 side = sign(E.x - T.x) — the enemy swings outward-down a
//                 touch, then rises into the slot from its own side.
// All dives end 416 px below their start (rows 0-1 land near the old turn
// line, lower rows briefly leave the screen bottom before pulling out).
class DivePath {
public:
    // Builds `pattern` from `start` (the enemy's box top-left at peel-off).
    // For ReturnPath, `end` is the target captured at build time; the
    // caller re-evaluates against the live slot via endCurve().
    static DivePath make(DivePattern pattern, Vector2 start, Vector2 end = {});

    // The curve with the STORED end point (exact for dives).
    const CubicBezier& curve() const { return curve_; }

    // The curve re-targeted at `liveEnd` (return paths): identical shape,
    // P(1) == liveEnd exactly.
    CubicBezier endCurve(Vector2 liveEnd) const
    {
        return curve_.withEnd(liveEnd);
    }

    // Sampled arc length in logical px (16 chords, computed once at make()).
    float length() const { return length_; }

    DivePattern pattern() const { return pattern_; }

private:
    CubicBezier curve_{};
    DivePattern pattern_ = DivePattern::LeftDive;
    float length_ = 0.0f;
};

// Advances a parameter t along a path by PIXELS travelled (not raw dt), so
// the world-space speed is definition().speed regardless of curve length.
//
//   t += pixels / path.length(), clamped to [0, 1]
//
// t therefore ALWAYS stays in [0, 1] (docs/test_plan.md Stage 12), and the
// advance depends only on accumulated distance — frame-rate independent.
class PathFollower {
public:
    void begin() { t_ = 0.0f; }

    // Advances along `path` by `pixels`; returns true once t has reached 1
    // (the call that reaches it included).
    bool advance(const DivePath& path, double pixels);

    float t() const { return t_; }
    bool finished() const { return t_ >= 1.0f; }

private:
    float t_ = 0.0f;
};

}  // namespace galaxian
