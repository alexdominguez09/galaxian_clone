// Stage 11/12 enemy state machine tests (docs/test_plan.md).
//
// Pure logic: the machine and the trajectories live in gameplay/Enemy +
// gameplay/DivePath (SDL-free, dependency rule); no window, no SDL init,
// no rendering.
//
// Exact-value notes (scratch-derived by LINKING the real DivePath code —
// Commander speed 70 px/s = 1.166666746 px per fixed step):
//   * LeftDive from the Commander (0,3) slot box (176, 64): arc length
//     480.442596 px -> the path finishes on the 412th update, pull-out at
//     P3 = (80, 480) exactly. The first control point (-72, -36) makes the
//     enemy RISE out of its slot into the loop.
//   * Left dash from x = 80 until fully off-screen: 90 updates, ends at
//     x = -24.999996.
//   * ReturnPath from there to the slot (176, 64): arc length 515.623962
//     px -> completes on the 442nd update with a bit-exact rejoin.
//   * Full automatic cycle: 30 + 412 + 90 + 442 = 974 updates = 16.23 s.
//   * Scout CenterAttack from (32, 208): length 419.3125; after 80 updates
//     t = 0.445173 and top y = 300.807373.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <utility>

#include "core/Constants.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/DivePath.hpp"
#include "gameplay/Effects.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/EnemyFormation.hpp"
#include "gameplay/Projectile.hpp"
#include "gameplay/ScoreManager.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;  // 1/60 s

// The five chain edges of spec §6.4.
constexpr std::array<std::pair<EnemyState, EnemyState>, 5> kChainEdges{{
    {EnemyState::Formation, EnemyState::PreparingDive},
    {EnemyState::PreparingDive, EnemyState::Diving},
    {EnemyState::Diving, EnemyState::Attacking},
    {EnemyState::Attacking, EnemyState::Returning},
    {EnemyState::Returning, EnemyState::Formation},
}};

// The next chain state (wraps around the cycle).
EnemyState nextChainState(EnemyState s)
{
    switch (s) {
        case EnemyState::Formation:     return EnemyState::PreparingDive;
        case EnemyState::PreparingDive: return EnemyState::Diving;
        case EnemyState::Diving:        return EnemyState::Attacking;
        case EnemyState::Attacking:     return EnemyState::Returning;
        case EnemyState::Returning:     return EnemyState::Formation;
        case EnemyState::Dead:          return EnemyState::Dead;
    }
    return EnemyState::Dead;
}

// Drives `enemy` into `target` using ONLY legal chain transitions.
void driveTo(Enemy& enemy, EnemyState target)
{
    int guard = 0;
    while (enemy.state() != target && guard++ < 6) {
        REQUIRE(enemy.transitionTo(nextChainState(enemy.state())));
    }
    REQUIRE(enemy.state() == target);
}

}  // namespace

TEST_CASE("enemy state machine: the full dive cycle of transitions is legal",
          "[enemy][statemachine]")
{
    Enemy enemy(EnemyType::Commander, Vector2{144.0f, 0.0f});
    CHECK(enemy.state() == EnemyState::Formation);
    CHECK(enemy.alive());

    for (const auto& [from, to] : kChainEdges) {
        (void)from;  // the walk itself proves the order
        CHECK(enemy.transitionTo(to));
        CHECK(enemy.state() == to);
        CHECK(enemy.alive());
    }
    // The cycle closed: back at Formation.
    CHECK(enemy.state() == EnemyState::Formation);
    // And it can start over.
    CHECK(enemy.transitionTo(EnemyState::PreparingDive));
}

TEST_CASE("enemy state machine: every legal pair accepted, every illegal "
          "pair rejected",
          "[enemy][statemachine]")
{
    // Independent expectation table built from spec §6.4 (NOT from the
    // implementation): the five chain edges plus any living -> Dead.
    const auto expectedLegal = [](EnemyState from, EnemyState to) {
        if (to == EnemyState::Dead) {
            return from != EnemyState::Dead;
        }
        for (const auto& [f, t] : kChainEdges) {
            if (f == from && t == to) {
                return true;
            }
        }
        return false;
    };

    for (int fi = 0; fi < 6; ++fi) {
        for (int ti = 0; ti < 6; ++ti) {
            const EnemyState from = static_cast<EnemyState>(fi);
            const EnemyState to = static_cast<EnemyState>(ti);

            Enemy enemy(EnemyType::Guard, Vector2{48.0f, 36.0f});
            if (from == EnemyState::Dead) {
                enemy.kill();
            } else if (from != EnemyState::Formation) {
                driveTo(enemy, from);
            }
            REQUIRE(enemy.state() == from);

            const bool accepted = enemy.transitionTo(to);
            INFO("transition " << fi << " -> " << ti);
            CHECK(accepted == expectedLegal(from, to));
            if (!accepted) {
                CHECK(enemy.state() == from);  // unchanged on rejection
            } else {
                CHECK(enemy.state() == to);
                // Dead transitions are validated too: nothing leaves it.
                if (to == EnemyState::Dead) {
                    for (int ri = 0; ri < 6; ++ri) {
                        CHECK_FALSE(enemy.transitionTo(
                            static_cast<EnemyState>(ri)));
                        CHECK(enemy.state() == EnemyState::Dead);
                    }
                }
            }
        }
    }

    // Self-transitions are explicitly illegal.
    for (int si = 0; si < 6; ++si) {
        const EnemyState s = static_cast<EnemyState>(si);
        CHECK(Enemy::isLegalTransition(s, s) ==
              false);
    }
}

TEST_CASE("enemy state machine: Dead reachable from every living state, "
          "and terminal",
          "[enemy][statemachine]")
{
    const EnemyState living[] = {
        EnemyState::Formation,   EnemyState::PreparingDive,
        EnemyState::Diving,      EnemyState::Attacking,
        EnemyState::Returning,
    };
    for (const EnemyState from : living) {
        Enemy enemy(EnemyType::Scout, Vector2{96.0f, 144.0f});
        if (from != EnemyState::Formation) {
            driveTo(enemy, from);
        }
        REQUIRE(enemy.state() == from);
        REQUIRE(enemy.alive());

        enemy.kill();
        CHECK(enemy.state() == EnemyState::Dead);
        CHECK_FALSE(enemy.alive());
    }

    // Terminal: kill() again is a safe no-op...
    Enemy dead(EnemyType::Scout, Vector2{0.0f, 0.0f});
    dead.kill();
    dead.kill();
    CHECK(dead.state() == EnemyState::Dead);
    // ...update does nothing (no resurrection)...
    dead.update(kDt, EnemyFormation::kAnchor);
    CHECK(dead.state() == EnemyState::Dead);
    // ...and beginDive is rejected.
    CHECK_FALSE(dead.beginDive(DivePattern::CenterAttack));
}

TEST_CASE("enemy state machine: full automatic cycle with Bezier paths "
          "(Commander at (0,3))",
          "[enemy][statemachine]")
{
    // Directly driven enemy against a FIXED anchor so every step count is
    // exact. Scratch-derived against the real DivePath code (see header):
    // LeftDive from (176, 64) is 480.442596 px long -> 412 updates at the
    // Commander's 70 px/s, ending at the pull-out (80, 480); the left dash
    // exits after 90 more; the ReturnPath (515.623962 px) completes on the
    // 442nd update with a bit-exact rejoin.
    Enemy enemy(EnemyType::Commander, EnemyFormation::slotOffset(0, 3));
    const Vector2 fp = EnemyFormation::kAnchor;  // (32, 64)

    auto stepN = [&](int n) {
        for (int i = 0; i < n; ++i) {
            enemy.update(kDt, fp);
        }
    };
    auto step1 = [&]() { enemy.update(kDt, fp); };

    // 1) Selection: leave the formation with an explicit attack pattern.
    REQUIRE(enemy.beginDive(DivePattern::LeftDive));
    CHECK(enemy.state() == EnemyState::PreparingDive);
    CHECK(enemy.divePattern() == DivePattern::LeftDive);
    // While preparing, the enemy sits on its slot (rides the lattice).
    CHECK(enemy.bounds(fp) == Rect{176.0f, 64.0f, 24.0f, 24.0f});

    // 2) PreparingDive holds 0.5 s = 30 updates.
    stepN(29);
    CHECK(enemy.state() == EnemyState::PreparingDive);
    step1();  // the 30th fires the peel-off
    CHECK(enemy.state() == EnemyState::Diving);
    // The path froze its start wherever the slot was.
    CHECK(enemy.divePosition() == Vector2{176.0f, 64.0f});

    // 3) Diving follows the Bezier. The curve's first control point pulls
    //    UP and outward (-72, -36): the classic flip out of the formation.
    step1();
    CHECK(enemy.divePosition().y < 64.0f);  // rising into the loop
    stepN(410);                             // 411 diving updates so far
    CHECK(enemy.state() == EnemyState::Diving);
    step1();  // the 412th finishes the path (t = 1)
    CHECK(enemy.state() == EnemyState::Attacking);
    // Pull-out exactly at P3.
    CHECK(enemy.divePosition() == Vector2{80.0f, 480.0f});

    // 4) Attacking: dashes left (pull-out center x 92 <= 224), exits fully
    //    off-screen after 90 updates.
    stepN(89);
    CHECK(enemy.state() == EnemyState::Attacking);
    step1();  // the 90th clears the screen edge
    CHECK(enemy.state() == EnemyState::Returning);
    CHECK(enemy.bounds(fp).right() <= 0.0f);

    // 5) Returning: follows the return arc and snaps EXACTLY onto the slot.
    stepN(441);
    CHECK(enemy.state() == EnemyState::Returning);
    step1();  // the 442nd closes the path
    CHECK(enemy.state() == EnemyState::Formation);
    CHECK(enemy.divePosition() == Vector2{176.0f, 64.0f});  // bit-exact
    CHECK(enemy.bounds(fp) == Rect{176.0f, 64.0f, 24.0f, 24.0f});

    // The slot offset never changed through the whole cycle.
    CHECK(enemy.slotOffset() == EnemyFormation::slotOffset(0, 3));
    CHECK(enemy.alive());

    // Total cycle time matches the scratch derivation.
    CHECK(30 + 412 + 90 + 442 == 974);  // 16.23 s of simulation
}

TEST_CASE("enemy state machine: dive cycle leaves the formation uncorrupted",
          "[enemy][statemachine]")
{
    // The integrated version: EnemyFormation::update drives both the
    // oscillation AND the diver, so the returning target MOVES. The rejoin
    // must land exactly on the live slot.
    EnemyFormation formation;

    // Snapshot the other slots' offsets before the dive.
    std::array<Vector2, EnemyFormation::kTotal> offsetsBefore{};
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        offsetsBefore[index] =
            EnemyFormation::slotOffset(index / EnemyFormation::kColumns,
                                       index % EnemyFormation::kColumns);
    }

    Enemy& diver = formation.at(0, 3);
    REQUIRE(diver.beginDive(DivePattern::CenterAttack));

    bool leftFormation = false;
    bool sawDiving = false;
    bool sawAttacking = false;
    bool sawReturning = false;
    bool rejoined = false;
    for (int step = 0; step < 4000 && !rejoined; ++step) {
        formation.update(kDt);
        if (diver.state() != EnemyState::Formation) {
            leftFormation = true;
        } else if (leftFormation) {
            rejoined = true;  // back in Formation after having been away
        }
        switch (diver.state()) {
            case EnemyState::Diving:    sawDiving = true; break;
            case EnemyState::Attacking: sawAttacking = true; break;
            case EnemyState::Returning: sawReturning = true; break;
            default: break;
        }
    }
    // The full cycle ran: away through all three motion phases, then back.
    REQUIRE(leftFormation);
    REQUIRE(rejoined);
    REQUIRE(sawDiving);
    REQUIRE(sawAttacking);
    REQUIRE(sawReturning);

    // Rejoined EXACTLY onto the live slot (bit-exact lattice equality).
    const Vector2 slotPos =
        formation.position() + EnemyFormation::slotOffset(0, 3);
    CHECK(diver.bounds(formation.position()) ==
          Rect{slotPos.x, slotPos.y, Enemy::kWidth, Enemy::kHeight});

    // Slot offset unchanged; everyone alive; the grid intact.
    CHECK(diver.slotOffset() == EnemyFormation::slotOffset(0, 3));
    CHECK(formation.aliveCount() == 40);
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        CHECK(formation.at(index).slotOffset() == offsetsBefore[index]);
        CHECK(formation.at(index).alive());
    }
    // And the oscillation kept running throughout.
    CHECK(formation.phase() > 0.0);
}

TEST_CASE("enemy state machine: combat kills a diver at its actual position",
          "[enemy][statemachine][combat]")
{
    // A bullet must hit the diver where it IS, not where its empty slot is,
    // and the destruction effect must spawn at the hit site.
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    Enemy& diver = formation.at(4, 0);  // Scout, slot box (32, 208)
    REQUIRE(diver.beginDive(DivePattern::CenterAttack));
    // Drive the prepare phase directly (the enemy's own update; the
    // formation stays put here).
    for (int i = 0; i < 30; ++i) {
        diver.update(kDt, formation.position());
    }
    REQUIRE(diver.state() == EnemyState::Diving);
    // Follow the Center attack arc for 80 updates: scratch value for the
    // path parameter t = 0.445173, top y = 300.807373.
    for (int i = 0; i < 80; ++i) {
        diver.update(kDt, formation.position());
    }
    const Rect trueBox = diver.bounds(formation.position());
    CHECK(trueBox.top() == Catch::Approx(300.807373).margin(1e-3));
    CHECK(trueBox.top() > 280.0f);  // clearly away from the slot
    // The slot itself is empty space now.
    CHECK(formation.boundsOf(4, 0) == Rect{32.0f, 208.0f, 24.0f, 24.0f});
    CHECK(trueBox != formation.boundsOf(4, 0));

    // A player bullet right inside the diver's TRUE box.
    REQUIRE(projectiles.spawn(ProjectileOwner::Player,
                              Vector2{trueBox.x + 10.0f, trueBox.y + 8.0f},
                              Vector2{0.0f, -480.0f}));
    const int kills = combat::resolvePlayerBullets(
        projectiles, formation, score, effects);

    CHECK(kills == 1);
    CHECK(diver.state() == EnemyState::Dead);  // killed mid-dive
    CHECK(score.score() == 50);
    CHECK(projectiles.count() == 0);
    // The effect spawned at the HIT SITE, not at the slot.
    CHECK(effects.count() == 1);
    CHECK(effects.effect(0).position == trueBox.position());
}

TEST_CASE("enemy state machine: dt <= 0 never advances a diver",
          "[enemy][statemachine]")
{
    Enemy enemy(EnemyType::Scout, Vector2{0.0f, 0.0f});
    REQUIRE(enemy.beginDive(DivePattern::CenterAttack));
    for (int i = 0; i < 60; ++i) {
        enemy.update(0.0, EnemyFormation::kAnchor);
        enemy.update(-kDt, EnemyFormation::kAnchor);
    }
    // Still preparing: no timer progress without positive dt.
    CHECK(enemy.state() == EnemyState::PreparingDive);
}
