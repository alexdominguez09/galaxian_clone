#include "DivePath.hpp"

#include <algorithm>
#include <cmath>

namespace galaxian {

namespace {

// Arc-length sampling resolution: chords of the curve at uniform t steps.
constexpr int kLengthSamples = 16;

float clamp01(float t)
{
    if (t < 0.0f) {
        return 0.0f;
    }
    if (t > 1.0f) {
        return 1.0f;
    }
    return t;
}

}  // namespace

Vector2 CubicBezier::evaluate(float rawT) const
{
    const float t = clamp01(rawT);
    // Endpoint shortcuts: P(0) == p0 and P(1) == p3 BIT-EXACTLY, which the
    // rejoin snap (P(1) == live slot) relies on.
    if (t <= 0.0f) {
        return p0;
    }
    if (t >= 1.0f) {
        return p3;
    }

    // de Casteljau: stable, symmetric in its lerp structure.
    const float u = 1.0f - t;
    // First level.
    const Vector2 p01 = p0 * u + p1 * t;
    const Vector2 p12 = p1 * u + p2 * t;
    const Vector2 p23 = p2 * u + p3 * t;
    // Second level.
    const Vector2 q0 = p01 * u + p12 * t;
    const Vector2 q1 = p12 * u + p23 * t;
    // Third level.
    return q0 * u + q1 * t;
}

CubicBezier CubicBezier::withEnd(Vector2 newP3) const
{
    CubicBezier result{*this};
    result.p3 = newP3;
    return result;
}

DivePath DivePath::make(DivePattern pattern, Vector2 start, Vector2 end)
{
    DivePath path;
    path.pattern_ = pattern;
    switch (pattern) {
        case DivePattern::LeftDive:
            path.curve_ = CubicBezier{start,
                                      start + Vector2{-72.0f, -36.0f},
                                      start + Vector2{-192.0f, 240.0f},
                                      start + Vector2{-96.0f, 416.0f}};
            break;
        case DivePattern::RightDive:
            path.curve_ = CubicBezier{start,
                                      start + Vector2{72.0f, -36.0f},
                                      start + Vector2{192.0f, 240.0f},
                                      start + Vector2{96.0f, 416.0f}};
            break;
        case DivePattern::CenterAttack:
            path.curve_ = CubicBezier{start,
                                      start + Vector2{0.0f, -24.0f},
                                      start + Vector2{0.0f, 200.0f},
                                      start + Vector2{0.0f, 416.0f}};
            break;
        case DivePattern::ReturnPath: {
            // side: which side of the target the attack ended on; the
            // return rises back into the slot from that same side.
            const float side =
                (end.x >= start.x) ? -1.0f : +1.0f;
            path.curve_ = CubicBezier{start,
                                      start + Vector2{side * 48.0f, 24.0f},
                                      end + Vector2{side * 96.0f, -96.0f},
                                      end};
            break;
        }
    }

    // Sampled arc length (uniform t chords — an approximation that is
    // deterministic and more than good enough for speed pacing).
    float length = 0.0f;
    Vector2 previous = path.curve_.p0;
    for (int i = 1; i <= kLengthSamples; ++i) {
        const float t = static_cast<float>(i) /
                        static_cast<float>(kLengthSamples);
        const Vector2 point = path.curve_.evaluate(t);
        const float dx = point.x - previous.x;
        const float dy = point.y - previous.y;
        length += std::sqrt(dx * dx + dy * dy);
        previous = point;
    }
    path.length_ = length;
    return path;
}

bool PathFollower::advance(const DivePath& path, double pixels)
{
    if (!finished() && path.length() > 0.0f) {
        t_ = clamp01(t_ +
                     static_cast<float>(pixels / path.length()));
    }
    return finished();
}

}  // namespace galaxian
