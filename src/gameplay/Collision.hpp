#pragma once

#include <algorithm>

#include "core/Types.hpp"

namespace galaxian {

// Stage 7 — collision detection (docs/game_spec.md §3, docs/architecture.md
// §3.5). Pure AABB math: no SDL, no game state, no side effects. This is the
// only place in the codebase where collision rules live; rendering code
// (graphics/) must never call it (enforced by the no_collision_in_graphics
// test).
//
// Semantics (fixed in docs/test_plan.md, Stage 7):
//   - Two boxes collide iff their intersection has positive area.
//   - Edge or corner contact (zero-area intersection) is NOT a collision.
//   - A degenerate box (zero width or zero height) never collides.
//   - The test is symmetric: intersects(a, b) == intersects(b, a).
//   - Negative coordinates are handled (the test is origin-independent).
constexpr bool intersects(const Rect& a, const Rect& b)
{
    // Overlap on each axis must be strictly positive.
    const float overlapX =
        std::min(a.right(), b.right()) - std::max(a.left(), b.left());
    const float overlapY =
        std::min(a.bottom(), b.bottom()) - std::max(a.top(), b.top());
    return overlapX > 0.0f && overlapY > 0.0f;
}

}  // namespace galaxian
