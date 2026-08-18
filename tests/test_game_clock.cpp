// Stage 2: GameClock sanity tests (docs/test_plan.md §1, Stage 2).
//
// The SDL performance counter works without SDL_Init, so these run
// headlessly. They check monotonicity and the stall clamp.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "core/GameClock.hpp"

using galaxian::GameClock;

TEST_CASE("GameClock: elapsed is monotonic, frameDelta is positive", "[clock]")
{
    GameClock clock;
    clock.start();
    const double e0 = clock.elapsed();
    REQUIRE(e0 >= 0.0);

    const double d0 = clock.frameDelta();
    REQUIRE(d0 >= 0.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const double e1 = clock.elapsed();
    REQUIRE(e1 > e0);

    const double d1 = clock.frameDelta();
    REQUIRE(d1 > 0.0);
    REQUIRE(d1 < GameClock::kMaxFrameDeltaSeconds);
}

TEST_CASE("GameClock: frameDelta is clamped after a long stall", "[clock]")
{
    GameClock clock;
    clock.start();
    clock.frameDelta();  // establish reference

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    const double d = clock.frameDelta();
    REQUIRE(d == GameClock::kMaxFrameDeltaSeconds);
}

TEST_CASE("GameClock: stopped clock reports zero", "[clock]")
{
    GameClock clock;
    REQUIRE(clock.elapsed() == 0.0);
    REQUIRE(clock.frameDelta() == 0.0);

    clock.start();
    clock.stop();
    REQUIRE(clock.frameDelta() == 0.0);
}
