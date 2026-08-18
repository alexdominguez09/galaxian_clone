// Stage 2: fixed-timestep tests (docs/test_plan.md §1, Stage 2).
//
// These run headlessly — no window, no display — and prove that the
// simulation step count is independent of the render frame rate.

#include <catch2/catch_test_macros.hpp>

#include "core/TimestepController.hpp"

using galaxian::TimestepController;

namespace {
constexpr double kDt = 1.0 / 60.0;
}  // namespace

TEST_CASE("Fixed timestep: exactly one step per frame at 60 Hz", "[timing]")
{
    TimestepController ts(kDt, 5);
    for (int i = 0; i < 120; ++i) {
        REQUIRE(ts.advance(kDt) == 1);
        REQUIRE_FALSE(ts.droppedTime());
    }
}

TEST_CASE("Fixed timestep: exactly two steps per frame at 30 Hz", "[timing]")
{
    TimestepController ts(kDt, 5);
    for (int i = 0; i < 60; ++i) {
        REQUIRE(ts.advance(2.0 * kDt) == 2);
    }
}

TEST_CASE("Fixed timestep: sub-frame remainder is preserved (120 Hz)", "[timing]")
{
    TimestepController ts(kDt, 5);
    // Each 120 Hz frame is half a simulation step: steps must alternate
    // 0, 1, 0, 1, ... with no drift.
    for (int i = 0; i < 60; ++i) {
        REQUIRE(ts.advance(0.5 * kDt) == 0);
        REQUIRE(ts.advance(0.5 * kDt) == 1);
    }
}

TEST_CASE("Equal render rates consume equal real time per 600 steps",
          "[timing][equivalence]")
{
    // The refresh-rate-independence property: running 600 simulation steps
    // (10 s of game time) must take the same real time whether the game is
    // rendered at 30, 60, or 120 Hz — within one frame of that rate.
    const int kSteps = 600;
    const double frameRates[] = {30.0, 60.0, 120.0};

    double referenceTime = -1.0;
    for (double hz : frameRates) {
        TimestepController ts(kDt, 5);
        int steps = 0;
        double t = 0.0;
        const double frame = 1.0 / hz;
        while (steps < kSteps) {
            steps += ts.advance(frame);
            t += frame;
        }
        if (referenceTime < 0.0) {
            referenceTime = t;
        }
        REQUIRE(std::abs(t - referenceTime) <= 1.0 / hz);
    }
    REQUIRE(std::abs(referenceTime - 10.0) < 0.05);
}

TEST_CASE("10 simulated seconds yields 600 steps at any render rate (±1)",
          "[timing][equivalence]")
{
    // 600 * (1/60 as a double) lies within ~3e-15 of 10.0, so feeding a
    // floating-point sum of frame times can land one simulation step short
    // in the worst case. The meaningful guarantee is "within one step".
    const double kSimSeconds = 10.0;
    const double frameRates[] = {30.0, 60.0, 120.0};
    const int expected = static_cast<int>(kSimSeconds / kDt);  // 600

    for (double hz : frameRates) {
        TimestepController ts(kDt, 5);
        int steps = 0;
        const double frame = 1.0 / hz;
        double t = 0.0;
        while (t < kSimSeconds - 1e-12) {
            double step = frame;
            if (t + step > kSimSeconds) {
                step = kSimSeconds - t;
            }
            steps += ts.advance(step);
            t += step;
        }
        REQUIRE(steps >= expected - 1);
        REQUIRE(steps <= expected);
    }
}

TEST_CASE("A long stall cannot cause a runaway update burst", "[timing][runaway]")
{
    TimestepController ts(kDt, 5);
    // 10 s of backlog = 600 steps worth of work.
    const int steps = ts.advance(10.0);
    REQUIRE(steps <= 5);
    REQUIRE(ts.droppedTime());

    // Backlog was dropped: the next normal frame runs exactly one step,
    // not a burst of catch-up work.
    REQUIRE(ts.advance(kDt) == 1);
    REQUIRE_FALSE(ts.droppedTime());
}

TEST_CASE("Backlog just under the cap is fully caught up, not dropped",
          "[timing][runaway]")
{
    TimestepController ts(kDt, 5);
    // Exactly 5 steps worth of backlog: all caught up, nothing dropped.
    REQUIRE(ts.advance(5.0 * kDt) == 5);
    REQUIRE_FALSE(ts.droppedTime());
    REQUIRE(ts.advance(kDt) == 1);
}

TEST_CASE("Zero and negative frame deltas are ignored", "[timing]")
{
    TimestepController ts(kDt, 5);
    REQUIRE(ts.advance(0.0) == 0);
    REQUIRE(ts.advance(-0.5) == 0);
    REQUIRE_FALSE(ts.droppedTime());
}
