// Stage 13 attack director tests (docs/test_plan.md, Stage 13).
//
// Pure logic: the director is SDL-free (dependency rule); no window, no
// SDL init, no rendering.
//
// Timing values (spec §7): wave 1 = 1 attacker / 6 s / 1 shot; wave 7+ =
// 4 attackers / 3 s. At the fixed 60 Hz step, 6 s = 360 updates and
// 3 s = 180 updates; the double-accumulated timer crosses the interval
// boundary deterministically on exactly that update (drift ~1e-16 s).
//
// The tick order below mirrors Game::fixedUpdate(): the formation moves
// first (advancing every enemy's machine), then the director sees the
// fresh states.

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "core/Constants.hpp"
#include "gameplay/AttackDirector.hpp"
#include "gameplay/DivePath.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;  // 1/60 s

struct Rig {
    EnemyFormation formation;
    AttackDirector director;

    explicit Rig(int wave = 1) : director(wave) {}

    // One production-order tick; returns attacks launched this tick.
    int tick()
    {
        formation.update(kDt);
        return director.update(kDt, formation);
    }
};

}  // namespace

TEST_CASE("director: wave parameter table matches spec §7", "[director]")
{
    const AttackWaveParams w1 = waveParams(1);
    CHECK(w1.maxSimultaneousAttackers == 1);
    CHECK(w1.attackIntervalSeconds == 6.0);
    CHECK(w1.shotsPerAttack == 1);

    const AttackWaveParams w2 = waveParams(2);
    CHECK(w2.maxSimultaneousAttackers == 2);
    CHECK(w2.attackIntervalSeconds == 6.0);

    const AttackWaveParams w3 = waveParams(3);
    CHECK(w3.maxSimultaneousAttackers == 2);
    CHECK(w3.attackIntervalSeconds == 4.0);

    const AttackWaveParams w4 = waveParams(4);
    CHECK(w4.maxSimultaneousAttackers == 2);
    CHECK(w4.shotsPerAttack == 2);

    const AttackWaveParams w5 = waveParams(5);
    CHECK(w5.maxSimultaneousAttackers == 3);
    CHECK(w5.attackIntervalSeconds == 3.0);   // the interval floor
    CHECK(w5.shotsPerAttack == 2);

    const AttackWaveParams w6 = waveParams(6);
    CHECK(w6.maxSimultaneousAttackers == 3);

    // Wave 7 raises the attacker cap one last time — nothing ever exceeds
    // these bounds.
    const AttackWaveParams w7 = waveParams(7);
    CHECK(w7.maxSimultaneousAttackers == 4);
    CHECK(w7.attackIntervalSeconds == 3.0);
    CHECK(waveParams(8).maxSimultaneousAttackers == 4);
    CHECK(waveParams(99).maxSimultaneousAttackers == 4);
    CHECK(waveParams(99).attackIntervalSeconds == 3.0);

    // Defensive clamp for nonsense waves (Stage 16 always passes >= 1).
    CHECK(waveParams(0).maxSimultaneousAttackers ==
          waveParams(1).maxSimultaneousAttackers);
    CHECK(waveParams(-3).attackIntervalSeconds == 6.0);
}

TEST_CASE("director: the first attack fires exactly one interval into "
          "the wave",
          "[director]")
{
    Rig rig{1};  // max 1 attacker, 6 s = 360 updates

    int launched = 0;
    for (int step = 0; step < 359; ++step) {
        launched += rig.tick();
        REQUIRE(rig.director.activeAttacks(rig.formation) <= 1);
    }
    CHECK(launched == 0);
    launched += rig.tick();  // the 360th update crosses the interval
    CHECK(launched == 1);

    // Exactly one enemy is away: the deterministic scan pick — front row,
    // rotating cursor start, i.e. slot (4, 0), sweeping LEFT (slot x=0).
    CHECK(rig.director.activeAttacks(rig.formation) == 1);
    Enemy& picked = rig.formation.at(4, 0);
    CHECK(picked.state() == EnemyState::PreparingDive);
    CHECK(picked.divePattern() == DivePattern::LeftDive);
    // Everyone else still sits in Formation.
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        if (index != 4 * EnemyFormation::kColumns + 0) {
            CHECK(rig.formation.at(index).state() == EnemyState::Formation);
        }
    }
    // The timer restarted with the launch (predictable spacing).
    CHECK(rig.director.sinceLastLaunch() == 0.0);
}

TEST_CASE("director: successive launches respect the interval and never "
          "exceed the wave cap",
          "[director]")
{
    Rig rig{7};  // max 4 attackers, 3 s = 180 updates

    std::vector<int> launchSteps;
    for (int step = 1; step <= 1200; ++step) {
        if (rig.tick() > 0) {
            launchSteps.push_back(step);
        }
        // THE bound: simultaneous away-count never exceeds the maximum.
        REQUIRE(rig.director.activeAttacks(rig.formation) <= 4);
    }

    REQUIRE(launchSteps.size() >= 5);
    // The first four launches fill the cap at exact 180-step spacing
    // (before any diver can rejoin and free capacity again).
    CHECK(launchSteps[0] == 180);
    CHECK(launchSteps[1] == 360);
    CHECK(launchSteps[2] == 540);
    CHECK(launchSteps[3] == 720);
    // The interval is a MINIMUM spacing (spec §7 "min"): later launches
    // (post-rejoin relaunches included) may only be delayed further by a
    // full-capacity wait, never fired early.
    for (size_t i = 4; i < launchSteps.size(); ++i) {
        const int gap = launchSteps[i] - launchSteps[i - 1];
        CHECK(gap >= 179);  // interval minus the ±1-frame tolerance
    }
}

TEST_CASE("director: dead enemies are never selected", "[director]")
{
    Rig rig{1};
    // Kill the entire front row: the would-be first picks.
    for (int col = 0; col < EnemyFormation::kColumns; ++col) {
        rig.formation.at(EnemyFormation::kRows - 1, col).kill();
    }

    // Drive until the first launch happens, bounded.
    int launched = 0;
    int steps = 0;
    while (launched == 0 && steps < 10000) {
        ++steps;
        launched = rig.tick();
    }
    REQUIRE(launched == 1);

    // The selected enemy is alive and NOT one of the dead ones...
    bool foundAway = false;
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            Enemy& e = rig.formation.at(row, col);
            if (!e.alive()) {
                // ...and the dead stay dead.
                CHECK(e.state() == EnemyState::Dead);
            } else if (e.state() != EnemyState::Formation) {
                foundAway = true;
                CHECK(row != EnemyFormation::kRows - 1);  // front is dead
                CHECK(e.state() == EnemyState::PreparingDive);
            }
        }
    }
    CHECK(foundAway);
}

TEST_CASE("director: no eligible attacker -> skipped without deadlock "
          "(1000-tick soak), resumes after reset",
          "[director]")
{
    Rig rig{5};  // cap 3, 3 s interval

    // Kill EVERYTHING.
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        rig.formation.at(index).kill();
    }
    REQUIRE(rig.formation.aliveCount() == 0);

    for (int step = 0; step < 1000; ++step) {
        CHECK(rig.tick() == 0);  // skipped every tick, forever if needed
    }
    CHECK(rig.director.activeAttacks(rig.formation) == 0);

    // Liveness: a rebuilt wave resumes launching normally.
    rig.formation.reset();
    int launched = 0;
    for (int step = 0; step < 1000 && launched == 0; ++step) {
        launched += rig.tick();
    }
    CHECK(launched > 0);
    CHECK(rig.director.activeAttacks(rig.formation) >= 1);
}

TEST_CASE("director: all enemies mid-dive -> capacity blocks safely",
          "[director]")
{
    // Nobody eligible AND capacity blown: send everyone out manually, then
    // run a wide-capacity director — it must simply idle.
    Rig rig{7};
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const float slotX =
                EnemyFormation::slotOffset(row, col).x;
            DivePattern pattern = DivePattern::CenterAttack;
            if (slotX < 144.0f) {
                pattern = DivePattern::LeftDive;
            } else if (slotX > 192.0f) {
                pattern = DivePattern::RightDive;
            }
            REQUIRE(rig.formation.at(row, col).beginDive(pattern));
        }
    }
    CHECK(AttackDirector::activeAttacks(rig.formation) == 40);

    for (int step = 0; step < 500; ++step) {
        CHECK(rig.tick() == 0);
    }
}

TEST_CASE("director: beginWave restarts pacing from zero", "[director]")
{
    Rig rig{1};
    for (int step = 0; step < 359; ++step) {
        CHECK(rig.tick() == 0);
    }
    // Just before the first launch of wave 1... switch to wave 3: the
    // timer resets, so the next launch waits a FULL new interval
    // (4 s = 240 updates), not an immediate fire.
    rig.director.beginWave(3);
    CHECK(rig.director.wave() == 3);
    CHECK(rig.director.params().maxSimultaneousAttackers == 2);
    CHECK(rig.director.sinceLastLaunch() == 0.0);

    int launched = 0;
    for (int step = 1; step <= 239; ++step) {
        launched += rig.tick();
    }
    CHECK(launched == 0);
    launched += rig.tick();  // the 240th
    CHECK(launched == 1);
}

TEST_CASE("director: dt <= 0 never advances or launches", "[director]")
{
    Rig rig{1};
    for (int i = 0; i < 4000; ++i) {
        CHECK(rig.director.update(0.0, rig.formation) == 0);
        CHECK(rig.director.update(-kDt, rig.formation) == 0);
    }
    CHECK(rig.director.sinceLastLaunch() == 0.0);
    CHECK(rig.director.activeAttacks(rig.formation) == 0);
}

TEST_CASE("director: selection is reproducible across identical rigs",
          "[director]")
{
    auto runSoak = [](int steps) {
        Rig rig{5};
        std::vector<int> launchSteps;
        for (int step = 1; step <= steps; ++step) {
            if (rig.tick() > 0) {
                launchSteps.push_back(step);
            }
        }
        return launchSteps;
    };
    const auto a = runSoak(900);
    const auto b = runSoak(900);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i] == b[i]);
    }
}
