#include <catch2/catch_test_macros.hpp>

#include "gameplay/Collision.hpp"

using namespace galaxian;

// Stage 7 — AABB collision (docs/test_plan.md, Stage 7).
//
// The rule: two boxes collide iff their intersection has positive area.
// Edge/corner contact is NOT a collision; degenerate (zero-size) boxes never
// collide; the test is symmetric and origin-independent (negative coords OK).

// Compile-time sanity (the function is constexpr): these must hold before any
// test runs.
static_assert(intersects(Rect{0, 0, 10, 10}, Rect{5, 5, 10, 10}));
static_assert(!intersects(Rect{0, 0, 10, 10}, Rect{10, 0, 10, 10}));
static_assert(!intersects(Rect{5, 5, 0, 0}, Rect{0, 0, 10, 10}));

TEST_CASE("overlapping boxes collide", "[collision]")
{
    // Center-ish overlap: intersection (5..10, 5..10).
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{5, 5, 10, 10}));
    // Same pair, swapped (symmetry spot check).
    REQUIRE(intersects(Rect{5, 5, 10, 10}, Rect{0, 0, 10, 10}));
    // A box overlapping only in x but sharing the full y range.
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{5, 0, 10, 10}));
    // A box overlapping only in y but sharing the full x range.
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{0, 5, 10, 10}));
    // A box with itself.
    REQUIRE(intersects(Rect{3, 4, 5, 6}, Rect{3, 4, 5, 6}));
}

TEST_CASE("disjoint boxes do not collide", "[collision]")
{
    // Separated horizontally (gap of 10).
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{20, 0, 10, 10}));
    // Separated vertically.
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{0, 20, 10, 10}));
    // Separated diagonally.
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{20, 20, 10, 10}));
    // One axis overlaps, the other does not (the common false-positive case).
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{5, 15, 10, 10}));
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{15, 5, 10, 10}));
}

TEST_CASE("edge and corner contact is not a collision", "[collision]")
{
    // Right edge of a touches left edge of b (x=10).
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{10, 0, 10, 10}));
    // Left edge of a touches right edge of b.
    REQUIRE_FALSE(intersects(Rect{10, 0, 10, 10}, Rect{0, 0, 10, 10}));
    // Bottom edge of a touches top edge of b (y=10).
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{0, 10, 10, 10}));
    // Top edge of a touches bottom edge of b.
    REQUIRE_FALSE(intersects(Rect{0, 10, 10, 10}, Rect{0, 0, 10, 10}));
    // Corner-to-corner (a's bottom-right at b's top-left, (10,10)).
    REQUIRE_FALSE(intersects(Rect{0, 0, 10, 10}, Rect{10, 10, 10, 10}));
}

TEST_CASE("one pixel of overlap IS a collision", "[collision]")
{
    // x-overlap is exactly (9..10) = 1 px; y fully overlaps.
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{9, 0, 10, 10}));
    // y-overlap is exactly (9..10) = 1 px; x fully overlaps.
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{0, 9, 10, 10}));
    // Both overlaps are 1 px (a's corner region).
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{9, 9, 10, 10}));
}

TEST_CASE("full containment collides in both directions", "[collision]")
{
    // b strictly inside a.
    REQUIRE(intersects(Rect{0, 0, 20, 20}, Rect{5, 5, 5, 5}));
    // a strictly inside b (containment is not directional).
    REQUIRE(intersects(Rect{5, 5, 5, 5}, Rect{0, 0, 20, 20}));
    // b inside a but touching a's inner edges is still a collision
    // (positive-area intersection (5..15) x (5..15)).
    REQUIRE(intersects(Rect{0, 0, 20, 20}, Rect{5, 5, 10, 10}));
}

TEST_CASE("partial intersection collides", "[collision]")
{
    // b's left half inside a's right half.
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{5, 0, 10, 10}));
    // b's top half inside a's bottom half.
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{0, 5, 10, 10}));
    // Small corner overlap (5..10) x (5..10) already covered by the overlap
    // case; here a sliver: x (8..10), y (8..10).
    REQUIRE(intersects(Rect{0, 0, 10, 10}, Rect{8, 8, 10, 10}));
}

TEST_CASE("negative coordinates are handled", "[collision]")
{
    // a spans (-10..5), b spans (0..10): overlap (0..5) on both axes.
    REQUIRE(intersects(Rect{-10, -10, 15, 15}, Rect{0, 0, 10, 10}));
    // a spans (-10..0), b spans (0..10): touch at x=0 and y=0, no area.
    REQUIRE_FALSE(intersects(Rect{-10, -10, 10, 10}, Rect{0, 0, 10, 10}));
    // Both boxes fully in negative space, overlapping (-15..-10) on both.
    REQUIRE(intersects(Rect{-20, -20, 10, 10}, Rect{-15, -15, 10, 10}));
    // Both boxes fully in negative space, disjoint.
    REQUIRE_FALSE(intersects(Rect{-30, -30, 10, 10}, Rect{-15, -15, 10, 10}));
    // Mixed: a negative x-range, positive y-range, overlapping b.
    REQUIRE(intersects(Rect{-5, 0, 10, 10}, Rect{0, 0, 10, 10}));
    // Mixed: a negative x-range that only touches b at x=0.
    REQUIRE_FALSE(intersects(Rect{-10, 0, 10, 10}, Rect{0, 0, 10, 10}));
}

TEST_CASE("degenerate (zero-size) boxes never collide", "[collision]")
{
    // Zero width, vertically inside b.
    REQUIRE_FALSE(intersects(Rect{5, 0, 0, 10}, Rect{0, 0, 10, 10}));
    // Zero height, horizontally inside b.
    REQUIRE_FALSE(intersects(Rect{0, 5, 10, 0}, Rect{0, 0, 10, 10}));
    // A point strictly inside b.
    REQUIRE_FALSE(intersects(Rect{5, 5, 0, 0}, Rect{0, 0, 10, 10}));
    // A point on b's edge.
    REQUIRE_FALSE(intersects(Rect{0, 5, 0, 0}, Rect{0, 0, 10, 10}));
    // Two identical points.
    REQUIRE_FALSE(intersects(Rect{5, 5, 0, 0}, Rect{5, 5, 0, 0}));
    // A zero-width box against itself.
    REQUIRE_FALSE(intersects(Rect{5, 0, 0, 10}, Rect{5, 0, 0, 10}));
    // Degenerate box in negative space.
    REQUIRE_FALSE(intersects(Rect{-5, -5, 0, 0}, Rect{-10, -10, 10, 10}));
}

TEST_CASE("intersects is symmetric", "[collision]")
{
    const Rect a{0, 0, 10, 10};
    const Rect b{5, 5, 10, 10};
    const Rect c{20, 20, 10, 10};
    const Rect d{10, 0, 10, 10};  // edge contact with a
    const Rect e{5, 5, 0, 0};     // degenerate, inside a

    REQUIRE(intersects(a, b) == intersects(b, a));
    REQUIRE(intersects(a, c) == intersects(c, a));
    REQUIRE(intersects(a, d) == intersects(d, a));
    REQUIRE(intersects(a, e) == intersects(e, a));
}
