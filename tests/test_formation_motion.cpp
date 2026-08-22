// Stage 10 formation motion tests (docs/test_plan.md, Stage 10).
//
// Pure logic: the oscillation is phase accumulation inside EnemyFormation
// (SDL-free, dependency rule), driven with explicit fixed-step dt values.
// The pixel-level check that a shifted formation actually renders lives in
// test_rendering.cpp.
//
// Expected values (derived with a scratch program mirroring the exact
// double/float operations; 8-16 significant digits):
//   * x(t) = kAnchor.x + kOscillationHalfSwing * sin(phase), with
//     phase advancing (2*pi / 4 s) * multiplier * dt per step.
//   * Full formation (multiplier exactly 1.0): after n fixed steps,
//     x(60) = 64, x(120) = 32, x(180) = 0, x(240) = 32 — one full
//     4 s period lands bit-exact on these waypoints in float32.
//   * A 10-minute soak stays within x ∈ [0, 64], so every enemy box stays
//     on screen ([0, 360] .. [64, 424] ⊂ [0, 448]).
//   * The §6.3 speed multiplier is 1.0 (full), 1.75 (20 of 40 dead),
//     2.5 (all dead — the spec bound); a formation with 20 dead advances
//     60 steps to exactly the same x as a full one advancing 105 steps
//     (60 * 1.75 == 105).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/Constants.hpp"
#include "gameplay/EnemyFormation.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;  // 1/60 s

// Advances the formation by `steps` fixed steps.
void runSteps(EnemyFormation& formation, int steps)
{
    for (int i = 0; i < steps; ++i) {
        formation.update(kDt);
    }
}

}  // namespace

TEST_CASE("motion: starts at rest at the anchor", "[enemy][formation][motion]")
{
    EnemyFormation formation;
    CHECK(formation.position() == EnemyFormation::kAnchor);
    CHECK(formation.phase() == 0.0);
    CHECK(formation.speedMultiplier() == Catch::Approx(1.0));
}

TEST_CASE("motion: update with dt <= 0 is a no-op", "[enemy][formation][motion]")
{
    EnemyFormation formation;
    formation.update(0.0);
    formation.update(-1.0 / 60.0);
    CHECK(formation.position() == EnemyFormation::kAnchor);
    CHECK(formation.phase() == 0.0);
}

TEST_CASE("motion: sinusoid hits the spec waypoints at full strength",
          "[enemy][formation][motion]")
{
    // Spec §6.3: amplitude 64 px peak-to-peak around the anchor, base
    // period 4 s (= 240 fixed steps). All waypoints are exact float32
    // values (verified by scratch program).
    EnemyFormation formation;
    runSteps(formation, 60);
    CHECK(formation.position().x == 64.0f);  // rightmost: 32 + 32
    CHECK(formation.phase() == Catch::Approx(std::numbers::pi / 2.0).margin(1e-12));

    runSteps(formation, 60);
    CHECK(formation.position().x == 32.0f);  // back through center

    runSteps(formation, 60);
    CHECK(formation.position().x == 0.0f);   // leftmost: 32 - 32

    runSteps(formation, 60);
    CHECK(formation.position().x == 32.0f);  // full period complete
    CHECK(formation.position().y == EnemyFormation::kAnchor.y);
}

TEST_CASE("motion: oscillates within screen bounds over a 10-minute soak",
          "[enemy][formation][motion][stress]")
{
    EnemyFormation formation;
    float minX = 1e9f;
    float maxX = -1e9f;
    for (int step = 1; step <= 36000; ++step) {
        formation.update(kDt);
        const Vector2 p = formation.position();
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);

        if (step % 10 == 0) {  // sampled: every box fully on screen
            for (int row = 0; row < EnemyFormation::kRows; ++row) {
                for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                    const Rect b = formation.boundsOf(row, col);
                    REQUIRE(b.left() >= 0.0f);
                    REQUIRE(b.right() <= static_cast<float>(kLogicalWidth));
                    REQUIRE(b.top() >= 0.0f);
                    REQUIRE(b.bottom() <= static_cast<float>(kLogicalHeight));
                }
            }
        }
    }
    // The swing covers exactly [anchor-32, anchor+32] = [0, 64].
    CHECK(minX == 0.0f);
    CHECK(maxX == 64.0f);
    // The phase never escapes [0, 2π).
    CHECK(formation.phase() >= 0.0);
    CHECK(formation.phase() < 2.0 * std::numbers::pi);
    CHECK(formation.aliveCount() == 40);
}

TEST_CASE("motion: spacing invariant while moving", "[enemy][formation][motion]")
{
    EnemyFormation formation;
    for (int step = 0; step <= 600; ++step) {
        if (step > 0) {
            formation.update(kDt);
        }
        if (step % 37 != 0) {  // sample irregularly across the period
            continue;
        }
        // Screen position = world position + slot offset (spec §6.2) for
        // every enemy, and the 48/36 spacing lattice is preserved (the
        // whole grid translates as one rigid body).
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                CHECK(formation.positionOf(row, col) ==
                      formation.position() + EnemyFormation::slotOffset(row, col));
                if (col + 1 < EnemyFormation::kColumns) {
                    CHECK(formation.positionOf(row, col + 1).x -
                              formation.positionOf(row, col).x ==
                          Catch::Approx(EnemyFormation::kColumnSpacing)
                              .margin(1e-4));
                }
                if (row + 1 < EnemyFormation::kRows) {
                    CHECK(formation.positionOf(row + 1, col).y -
                              formation.positionOf(row, col).y ==
                          EnemyFormation::kRowSpacing);
                    CHECK(formation.positionOf(row + 1, col).x -
                              formation.positionOf(row, col).x == 0.0f);
                }
            }
        }
        // y never changes.
        CHECK(formation.position().y == EnemyFormation::kAnchor.y);
    }
}

TEST_CASE("motion: killed enemies stay absent; state valid while moving",
          "[enemy][formation][motion]")
{
    EnemyFormation formation;
    // Kill row 0 entirely plus two scattered slots.
    for (int col = 0; col < EnemyFormation::kColumns; ++col) {
        formation.at(0, col).kill();
    }
    formation.at(3, 2).kill();
    formation.at(4, 6).kill();
    REQUIRE(formation.aliveCount() == 30);

    for (int step = 1; step <= 600; ++step) {
        formation.update(kDt);
        if (step % 100 != 0) {
            continue;
        }
        // The dead stay dead (no resurrection, no re-packing).
        CHECK(formation.aliveCount() == 30);
        for (int index = 0; index < EnemyFormation::kTotal; ++index) {
            const int row = index / EnemyFormation::kColumns;
            const int col = index % EnemyFormation::kColumns;
            const bool killed =
                row == 0 || (row == 3 && col == 2) || (row == 4 && col == 6);
            CHECK(formation.at(index).alive() == !killed);
        }
        // The survivors move rigidly with the formation...
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                if (formation.at(row, col).alive()) {
                    CHECK(formation.at(row, col).bounds(formation.position()) ==
                          formation.boundsOf(row, col));
                }
            }
        }
        // ...and everything stays on screen.
        CHECK(formation.position().x >= 0.0f);
        CHECK(formation.position().x + 7 * EnemyFormation::kColumnSpacing +
                  Enemy::kWidth <=
              static_cast<float>(kLogicalWidth));
    }

    // reset() revives everyone at the anchor with a zeroed phase.
    formation.reset();
    CHECK(formation.aliveCount() == 40);
    CHECK(formation.position() == EnemyFormation::kAnchor);
    CHECK(formation.phase() == 0.0);
}

TEST_CASE("motion: speed-up multiplier follows deaths, bounded at 2.5x",
          "[enemy][formation][motion]")
{
    EnemyFormation formation;
    CHECK(formation.speedMultiplier() == Catch::Approx(1.0));

    // Linear ramp: each death adds (maxMult - 1)/40 to the multiplier.
    formation.at(0, 0).kill();
    CHECK(formation.speedMultiplier() ==
          Catch::Approx(1.0 + 1.5 / 40.0).margin(1e-12));

    for (int i = 1; i < 20; ++i) {
        formation.at(i / EnemyFormation::kColumns,
                     i % EnemyFormation::kColumns)
            .kill();
    }
    CHECK(formation.aliveCount() == 20);
    CHECK(formation.speedMultiplier() == Catch::Approx(1.75));

    for (int i = 20; i < EnemyFormation::kTotal; ++i) {
        formation.at(i / EnemyFormation::kColumns,
                     i % EnemyFormation::kColumns)
            .kill();
    }
    CHECK(formation.aliveCount() == 0);
    // Exactly the spec §6.3 bound.
    CHECK(formation.speedMultiplier() == Catch::Approx(2.5));
    CHECK(formation.speedMultiplier() <= EnemyFormation::kMaxSpeedMultiplier);
}

TEST_CASE("motion: speed-up functionally advances the oscillation faster",
          "[enemy][formation][motion]")
{
    // A half-dead formation stepping 60 times accumulates the same phase
    // (60 * 1.75 == 105) as a full-strength one stepping 105 times, so both
    // land on exactly the same x (bit-exact — verified by scratch program).
    EnemyFormation full;
    EnemyFormation halfDead;
    for (int i = 0; i < 20; ++i) {
        halfDead.at(i / EnemyFormation::kColumns, i % EnemyFormation::kColumns)
            .kill();
    }

    runSteps(halfDead, 60);
    runSteps(full, 105);
    CHECK(halfDead.position().x == full.position().x);
    CHECK(full.position().x != 32.0f);  // sanity: it actually moved

    // And it moved further than a full-strength formation does in 60 steps.
    EnemyFormation full60;
    runSteps(full60, 60);
    CHECK(full60.position().x == 64.0f);
    CHECK(full.position().x != full60.position().x);
}

TEST_CASE("motion: frame-rate independence of the oscillation phase "
          "(30 vs 60 vs 120 Hz)",
          "[enemy][formation][motion]")
{
    // 4 seconds of simulation time from the anchor: identical final x
    // regardless of step granularity (tolerance mirrors earlier stages).
    auto run = [](double dt, int steps) {
        EnemyFormation formation;
        for (int i = 0; i < steps; ++i) {
            formation.update(dt);
        }
        return formation.position().x;
    };

    const float x30 = run(1.0 / 30.0, 120);
    const float x60 = run(kDt, 240);
    const float x120 = run(1.0 / 120.0, 480);
    CHECK(x60 == 32.0f);  // exact waypoint (full period)
    CHECK(std::abs(x30 - x60) < 0.01f);
    CHECK(std::abs(x120 - x60) < 0.01f);

    // Same at an arbitrary non-waypoint time: all three granularities cover
    // EXACTLY 80/60 s = 4/3 s of simulation time, so any difference is pure
    // accumulation rounding.
    const float a = run(kDt, 80);
    const float b = run(1.0 / 120.0, 160);
    const float c = run(1.0 / 30.0, 40);
    CHECK(std::abs(b - a) < 0.01f);
    CHECK(std::abs(c - a) < 0.01f);
}

TEST_CASE("motion: deterministic across independent spawns",
          "[enemy][formation][motion]")
{
    // Two formations fed the same step sequence produce bit-identical
    // trajectories (snapshot every 100 steps over 1000 steps).
    EnemyFormation a;
    EnemyFormation b;
    for (int step = 1; step <= 1000; ++step) {
        a.update(kDt);
        b.update(kDt);
        if (step % 100 == 0) {
            CHECK(a.position() == b.position());
            CHECK(a.phase() == b.phase());
        }
    }
}
