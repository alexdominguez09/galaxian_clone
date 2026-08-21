// Stage 5 player tests (docs/test_plan.md, Stage 5).
//
// Pure logic tests: no window, no SDL init, no rendering. The Player is
// driven with explicit dt values and directions, so the math is verified
// directly (docs/architecture.md §4).

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "core/Constants.hpp"
#include "gameplay/Player.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;  // 1/60 s

// Moves the player `steps` fixed steps in `direction` and returns the final
// center x.
float runSteps(Player& player, int steps, float direction)
{
    for (int i = 0; i < steps; ++i) {
        player.update(kDt, direction);
    }
    return player.position().x;
}

}  // namespace

TEST_CASE("player: starts at the spec position (224, 528)", "[player]")
{
    Player player;
    REQUIRE(player.position() == Vector2(224.0f, 528.0f));
    REQUIRE(player.alive());
    REQUIRE(player.state() == PlayerState::Alive);
    REQUIRE(player.fireCount() == 0);
}

TEST_CASE("player: movement distance = speed * dt over N steps", "[player]")
{
    // Right for 30 steps (0.5 s): 224 + 30 * 220 * (1/60) = 224 + 110 = 334.
    Player right;
    REQUIRE(std::abs(runSteps(right, 30, +1.0f) - 334.0f) < 0.01f);

    // Left for 30 steps: 224 - 110 = 114.
    Player left;
    REQUIRE(std::abs(runSteps(left, 30, -1.0f) - 114.0f) < 0.01f);

    // No direction: no movement.
    Player still;
    REQUIRE(runSteps(still, 30, 0.0f) == 224.0f);

    // y is never changed (horizontal only).
    REQUIRE(right.position().y == 528.0f);
    REQUIRE(left.position().y == 528.0f);
}

TEST_CASE("player: cannot move left of the screen (clamped)", "[player]")
{
    Player player;
    // 600 steps left (10 s) is far more than the ~1 s it takes to reach the
    // left edge from the center.
    runSteps(player, 600, -1.0f);

    REQUIRE(player.position().x == Player::kWidth * 0.5f);  // 12
    // The collision box never leaves the screen.
    const Rect b = player.bounds();
    REQUIRE(b.left() == 0.0f);
    REQUIRE(b.right() == Player::kWidth);
}

TEST_CASE("player: cannot move right of the screen (clamped)", "[player]")
{
    Player player;
    runSteps(player, 600, +1.0f);

    REQUIRE(player.position().x ==
            static_cast<float>(kLogicalWidth) - Player::kWidth * 0.5f);  // 436
    const Rect b = player.bounds();
    REQUIRE(b.left() == static_cast<float>(kLogicalWidth) - Player::kWidth);
    REQUIRE(b.right() == static_cast<float>(kLogicalWidth));
}

TEST_CASE("player: clamping is stable when the input is held at the edge",
          "[player]")
{
    Player player;
    runSteps(player, 600, -1.0f);
    const float atEdge = player.position().x;
    // Holding left at the edge must not drift, oscillate, or go negative.
    runSteps(player, 600, -1.0f);
    REQUIRE(player.position().x == atEdge);
    REQUIRE(player.position().x >= 0.0f);
}

TEST_CASE("player: frame-rate independence (30 Hz vs 120 Hz, unclamped)",
          "[player]")
{
    // Simulate 0.5 s of rightward movement with two different step
    // granularities. 0.5 s from the center (224) travels 110 px and does not
    // reach the right edge (436), so the clamp cannot mask a difference.
    const double totalSeconds = 0.5;

    Player p30;
    const double dt30 = 1.0 / 30.0;
    for (int i = 0; i < static_cast<int>(totalSeconds * 30); ++i) {
        p30.update(dt30, +1.0f);
    }

    Player p120;
    const double dt120 = 1.0 / 120.0;
    for (int i = 0; i < static_cast<int>(totalSeconds * 120); ++i) {
        p120.update(dt120, +1.0f);
    }

    REQUIRE(std::abs(p30.position().x - p120.position().x) < 0.01f);
    // And both match the analytical distance: 224 + 220 * 0.5 = 334.
    REQUIRE(std::abs(p30.position().x - 334.0f) < 0.01f);
    REQUIRE(std::abs(p120.position().x - 334.0f) < 0.01f);
}

TEST_CASE("player: frame-rate independence (10 s sim, clamped)", "[player]")
{
    // 10 s of rightward movement at 220 px/s is 2200 px — far past the
    // right edge, so both granularities must converge on the same clamped
    // position.
    const double totalSeconds = 10.0;

    Player p30;
    const double dt30 = 1.0 / 30.0;
    for (int i = 0; i < static_cast<int>(totalSeconds * 30); ++i) {
        p30.update(dt30, +1.0f);
    }

    Player p120;
    const double dt120 = 1.0 / 120.0;
    for (int i = 0; i < static_cast<int>(totalSeconds * 120); ++i) {
        p120.update(dt120, +1.0f);
    }

    REQUIRE(p30.position().x == p120.position().x);
    REQUIRE(p30.position().x ==
            static_cast<float>(kLogicalWidth) - Player::kWidth * 0.5f);
}

TEST_CASE("player: fire emits a fire event; dead player cannot fire",
          "[player]")
{
    Player player;
    REQUIRE(player.fireCount() == 0);

    player.fire();
    player.fire();
    REQUIRE(player.fireCount() == 2);

    player.kill();
    player.fire();  // ignored while dead
    REQUIRE(player.fireCount() == 2);
}

TEST_CASE("player: alive/dead state and respawn", "[player]")
{
    Player player;
    REQUIRE(player.alive());

    player.kill();
    REQUIRE_FALSE(player.alive());
    REQUIRE(player.state() == PlayerState::Dead);

    // A dead player does not move.
    const float x = player.position().x;
    player.update(kDt, +1.0f);
    REQUIRE(player.position().x == x);

    // Respawn restores the start position and the Alive state.
    player.respawn();
    REQUIRE(player.alive());
    REQUIRE(player.state() == PlayerState::Alive);
    REQUIRE(player.position() == Player::kStartPosition);
}

TEST_CASE("player: bounds is the 24x16 box centered on the position",
          "[player]")
{
    Player player;
    Rect b = player.bounds();
    REQUIRE(b.width == 24.0f);
    REQUIRE(b.height == 16.0f);
    REQUIRE(b.x == 224.0f - 12.0f);
    REQUIRE(b.y == 528.0f - 8.0f);

    // After moving, the box stays centered on the new position.
    runSteps(player, 30, +1.0f);
    b = player.bounds();
    REQUIRE(std::abs(b.x - (player.position().x - 12.0f)) < 1e-6f);
    REQUIRE(b.y == player.position().y - 8.0f);
}

TEST_CASE("player: simultaneous left+right cancel (net direction zero)",
          "[player]")
{
    // The spec'd resolution (docs/game_spec.md §4): when both directions
    // are held, the net direction is zero and the player does not move.
    // The cancellation happens in the caller (Actions -> direction); here
    // we verify the Player honors a zero net direction.
    Player player;
    float direction = 0.0f;
    const bool leftHeld = true;
    const bool rightHeld = true;
    if (rightHeld) {
        direction += 1.0f;
    }
    if (leftHeld) {
        direction -= 1.0f;
    }
    REQUIRE(direction == 0.0f);
    REQUIRE(runSteps(player, 60, direction) == 224.0f);
}
