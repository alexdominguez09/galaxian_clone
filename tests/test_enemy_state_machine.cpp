// Stage 11 enemy state machine tests (docs/test_plan.md, Stage 11).
//
// Pure logic: the machine and the simple paths live in gameplay/Enemy
// (SDL-free, dependency rule); no window, no SDL init, no rendering.
//
// Exact-value notes (derived with a scratch program mirroring the game's
// exact float32 operations; Commander speed 70 px/s -> per fixed step
// exactly 1.166666746 px):
//   * Commander at slot (0,3) of an anchored formation: box top-left
//     (176, 64), bottom starts at 88.
//   * Prepare (0.5 s): expires on the 30th update (remaining ~1e-16 s,
//     within the 1 ns tolerance).
//   * Dive from bottom 88 until bottom >= 480: 337 updates (float
//     accumulation puts the crossing one step past the ideal 336);
//     final top y = 457.165344.
//   * Attack dash left (center x 188 <= 224 -> nearer edge is left):
//     fully off-screen after 172 updates; final x = -24.666767.
//   * Return homing to the live slot: snaps onto it on the 379th update,
//     bit-exact at (176, 64).
//   * Full automatic cycle: 30 + 337 + 172 + 379 = 918 updates = 15.3 s.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <utility>

#include "core/Constants.hpp"
#include "gameplay/Combat.hpp"
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
    CHECK_FALSE(dead.beginDive());
}

TEST_CASE("enemy state machine: full automatic cycle with simple paths "
          "(Commander at (0,3))",
          "[enemy][statemachine]")
{
    // Directly driven enemy against a FIXED anchor so every step count is
    // exact (scratch-verified; see the file header).
    Enemy enemy(EnemyType::Commander, EnemyFormation::slotOffset(0, 3));
    const Vector2 fp = EnemyFormation::kAnchor;  // (32, 64)

    auto stepN = [&](int n) {
        for (int i = 0; i < n; ++i) {
            enemy.update(kDt, fp);
        }
    };
    auto step1 = [&]() { enemy.update(kDt, fp); };

    // 1) Selection: leave the formation.
    REQUIRE(enemy.beginDive());
    CHECK(enemy.state() == EnemyState::PreparingDive);
    // While preparing, the enemy sits on its slot (rides the lattice).
    CHECK(enemy.bounds(fp) == Rect{176.0f, 64.0f, 24.0f, 24.0f});

    // 2) PreparingDive holds 0.5 s = 30 updates.
    stepN(29);
    CHECK(enemy.state() == EnemyState::PreparingDive);
    step1();  // the 30th fires the peel-off
    CHECK(enemy.state() == EnemyState::Diving);
    // The dive froze its start wherever the slot was.
    CHECK(enemy.divePosition() == Vector2{176.0f, 64.0f});

    // 3) Diving: straight down until the box bottom reaches the turn
    //    point. 336 updates leave it just under the line (scratch: top
    //    455.998688, bottom 479.998688 < 480); the 337th crosses it.
    stepN(336);
    CHECK(enemy.state() == EnemyState::Diving);
    CHECK(enemy.divePosition().y ==
          Catch::Approx(455.998687744).margin(1e-4));
    step1();  // the 337th crosses the turn point
    CHECK(enemy.state() == EnemyState::Attacking);
    // Scratch value: final top y = 457.165344, bottom = 481.165344 >= 480.
    CHECK(enemy.divePosition().y == Catch::Approx(457.165344).margin(1e-4));
    CHECK(enemy.bounds(fp).bottom() >= Enemy::kTurnPointY);
    // It travelled straight down: x untouched.
    CHECK(enemy.divePosition().x == 176.0f);

    // 4) Attacking: dashes left (center x 188 <= 224 -> nearer edge),
    //    exits fully off-screen after 172 updates.
    CHECK(enemy.divePosition().x + Enemy::kWidth * 0.5f <=
          static_cast<float>(kLogicalWidth) * 0.5f);  // left half -> LEFT
    stepN(171);
    CHECK(enemy.state() == EnemyState::Attacking);
    step1();  // the 172nd clears the screen edge
    CHECK(enemy.state() == EnemyState::Returning);
    // Scratch value: final x = -24.666767 (right edge -0.667 <= 0).
    CHECK(enemy.divePosition().x == Catch::Approx(-24.666767).margin(1e-4));
    CHECK(enemy.bounds(fp).right() <= 0.0f);

    // 5) Returning: homes to the live slot and snaps EXACTLY onto it.
    stepN(378);
    CHECK(enemy.state() == EnemyState::Returning);
    step1();  // the 379th closes the remaining distance
    CHECK(enemy.state() == EnemyState::Formation);
    CHECK(enemy.divePosition() == Vector2{176.0f, 64.0f});  // bit-exact
    CHECK(enemy.bounds(fp) == Rect{176.0f, 64.0f, 24.0f, 24.0f});

    // The slot offset never changed through the whole cycle.
    CHECK(enemy.slotOffset() == EnemyFormation::slotOffset(0, 3));
    CHECK(enemy.alive());

    // Total cycle time matches the scratch derivation.
    CHECK(30 + 337 + 172 + 379 == 918);  // 15.3 s of simulation
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
    REQUIRE(diver.beginDive());

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
    REQUIRE(diver.beginDive());
    // Drive the prepare phase directly (the enemy's own update; the
    // formation stays put here).
    for (int i = 0; i < 30; ++i) {
        diver.update(kDt, formation.position());
    }
    REQUIRE(diver.state() == EnemyState::Diving);
    // Descend visibly: 50 steps * 140 px/s * (1/60).
    for (int i = 0; i < 50; ++i) {
        diver.update(kDt, formation.position());
    }
    const Rect trueBox = diver.bounds(formation.position());
    // Scratch: top = 324.666870 after 50 dive steps of 2.333333492 px.
    CHECK(trueBox.top() == Catch::Approx(324.666870).margin(1e-3));
    CHECK(trueBox.top() > 300.0f);  // clearly away from the slot
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
    REQUIRE(enemy.beginDive());
    for (int i = 0; i < 60; ++i) {
        enemy.update(0.0, EnemyFormation::kAnchor);
        enemy.update(-kDt, EnemyFormation::kAnchor);
    }
    // Still preparing: no timer progress without positive dt.
    CHECK(enemy.state() == EnemyState::PreparingDive);
}
