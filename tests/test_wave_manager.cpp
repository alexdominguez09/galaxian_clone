// Stage 16 wave system tests (docs/test_plan.md, Stage 16).
//
// Pure logic: WaveManager is SDL-free (dependency rule); no window, no
// SDL init, no rendering.
//
// Exact-value notes (scratch-verified countdown drift, same 1 ns tolerance
// as every other simulation timer):
//   * The 2.0 s interstitial expires exactly on the 120th fixed step
//     (leftover ~2.1e-15 s <= epsilon); alive through 119 updates.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/Constants.hpp"
#include "gameplay/AttackDirector.hpp"
#include "gameplay/EnemyFormation.hpp"
#include "gameplay/Projectile.hpp"
#include "gameplay/WaveManager.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;

struct Rig {
    EnemyFormation formation;
    AttackDirector director{1};
    WaveManager waves{1};

    // One production-order tick.
    WaveManager::Event tick()
    {
        formation.update(kDt);
        director.update(kDt, formation);
        return waves.update(kDt, formation, director);
    }
};

}  // namespace

TEST_CASE("waves: starts at wave 1 with a live formation", "[wave]")
{
    Rig rig;
    CHECK(rig.waves.wave() == 1);
    CHECK_FALSE(rig.waves.interstitial());
    CHECK(rig.formation.aliveCount() == 40);
    // Nothing happens while enemies live.
    for (int i = 0; i < 300; ++i) {
        CHECK(rig.tick() == WaveManager::Event::None);
    }
    CHECK(rig.formation.aliveCount() == 40);
}

TEST_CASE("waves: all dead -> cleared once -> 2 s interstitial -> "
          "next formation",
          "[wave]")
{
    Rig rig;
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        rig.formation.at(index).kill();
    }

    // The clear is detected on the next tick (post-combat state), exactly
    // once.
    CHECK(rig.tick() == WaveManager::Event::WaveCleared);
    CHECK(rig.waves.interstitial());
    CHECK(rig.waves.remaining() ==
          Catch::Approx(WaveManager::kInterstitialSeconds).margin(1e-12));
    for (int i = 0; i < 50; ++i) {
        CHECK(rig.tick() == WaveManager::Event::None);  // fires only once
    }

    // The interstitial keeps running through exactly 119 more updates...
    for (int i = 0; i < 119 - 50; ++i) {
        REQUIRE(rig.tick() == WaveManager::Event::None);
        CHECK(rig.waves.interstitial());
    }
    // ...and the next tick (the 120th decrement) advances to wave 2.
    CHECK(rig.tick() == WaveManager::Event::WaveAdvanced);

    CHECK_FALSE(rig.waves.interstitial());
    CHECK(rig.waves.wave() == 2);
    CHECK(rig.formation.aliveCount() == 40);
    // The rebuilt grid sits at the spec anchor with exact geometry.
    CHECK(rig.formation.position() == EnemyFormation::kAnchor);
    CHECK(rig.formation.positionOf(4, 7) == Vector2{368.0f, 208.0f});
    // ...and the director received the wave-2 parameters (spec §7:
    // 2 attackers / 6 s / 1 shot), timer restarted.
    CHECK(rig.director.params().maxSimultaneousAttackers == 2);
    CHECK(rig.director.params().attackIntervalSeconds == 6.0);
    CHECK(rig.director.params().shotsPerAttack == 1);
    CHECK(rig.director.sinceLastLaunch() == 0.0);
}

TEST_CASE("waves: a lone surviving diver blocks the clear", "[wave]")
{
    Rig rig;
    // Kill everyone except one enemy that dives away (still ALIVE).
    for (int index = 1; index < EnemyFormation::kTotal; ++index) {
        rig.formation.at(index).kill();
    }
    REQUIRE(rig.formation.at(0, 0).beginDive(DivePattern::CenterAttack, 0));
    REQUIRE(rig.formation.aliveCount() == 1);

    // Long soak: never a clear while the diver lives.
    for (int step = 0; step < 1500; ++step) {
        CHECK(rig.tick() == WaveManager::Event::None);
    }
    CHECK(rig.formation.aliveCount() >= 1);

    // Only death counts as cleared (spec §9): kill it and the clear lands
    // on the very next tick.
    rig.formation.at(0, 0).kill();
    CHECK(rig.tick() == WaveManager::Event::WaveCleared);
}

TEST_CASE("waves: twelve consecutive waves without state corruption",
          "[wave][stress]")
{
    Rig rig;
    int score = 1234;  // stand-in: score/lives persist across waves

    for (int expected = 2; expected <= 13; ++expected) {
        // Clear the current wave.
        for (int index = 0; index < EnemyFormation::kTotal; ++index) {
            rig.formation.at(index).kill();
        }
        REQUIRE(rig.tick() == WaveManager::Event::WaveCleared);

        // Drain the interstitial: None ticks until the window's last one
        // advances the wave.
        for (;;) {
            const WaveManager::Event ev = rig.tick();
            if (ev == WaveManager::Event::WaveAdvanced) {
                break;
            }
            REQUIRE(ev == WaveManager::Event::None);
        }
        REQUIRE(rig.waves.wave() == expected);

        // Post-transition invariants — every single time.
        CHECK(rig.waves.wave() == expected);
        CHECK(rig.formation.aliveCount() == 40);
        CHECK(rig.formation.position() == EnemyFormation::kAnchor);
        for (int index = 0; index < EnemyFormation::kTotal; ++index) {
            CHECK(rig.formation.at(index).alive());
            CHECK(rig.formation.at(index).state() == EnemyState::Formation);
        }
        // Bounded difficulty handover each wave (spec §7 caps).
        const AttackWaveParams p = waveParams(expected);
        CHECK(rig.director.params().maxSimultaneousAttackers ==
              p.maxSimultaneousAttackers);
        CHECK(p.maxSimultaneousAttackers <= 4);
        CHECK(p.attackIntervalSeconds >= 3.0);
        CHECK(p.shotsPerAttack <= 2);
    }

    // The systems remain fully usable after 12 transitions.
    CHECK(rig.director.update(kDt, rig.formation) >= 0);
    rig.formation.reset();
    CHECK(rig.formation.aliveCount() == 40);
    (void)score;
}

TEST_CASE("waves: difficulty bounds hold across waves 1..20", "[wave]")
{
    double previousSpeed = 0.0;
    for (int wave = 1; wave <= 20; ++wave) {
        const AttackWaveParams p = waveParams(wave);
        CHECK(p.maxSimultaneousAttackers >= 1);
        CHECK(p.maxSimultaneousAttackers <= 4);
        CHECK(p.attackIntervalSeconds >= 3.0);
        CHECK(p.shotsPerAttack >= 1);
        CHECK(p.shotsPerAttack <= 2);

        // Spec §8: enemy bullet speed ramps but never exceeds 360 px/s.
        const double speed = ProjectileManager::speedForWave(wave);
        CHECK(speed >= ProjectileManager::kEnemySpeed);
        CHECK(speed <= ProjectileManager::kEnemyMaxSpeed);
        CHECK(speed >= previousSpeed);  // monotonic non-decreasing ramp
        previousSpeed = speed;
    }
    // Exact ramp values at the ends.
    CHECK(ProjectileManager::speedForWave(1) ==
          Catch::Approx(240.0).margin(1e-12));
    CHECK(ProjectileManager::speedForWave(4) ==
          Catch::Approx(360.0).margin(1e-12));   // capped from wave 4 on
    CHECK(ProjectileManager::speedForWave(20) ==
          Catch::Approx(360.0).margin(1e-12));

    // Nonsense waves clamp defensively everywhere.
    CHECK(ProjectileManager::speedForWave(0) ==
          Catch::Approx(240.0).margin(1e-12));
}

TEST_CASE("waves: dt <= 0 never advances anything", "[wave]")
{
    Rig rig;
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        rig.formation.at(index).kill();
    }
    // First tick detects the clear (positive dt), then zero/negative dt
    // are inert forever.
    REQUIRE(rig.tick() == WaveManager::Event::WaveCleared);
    for (int i = 0; i < 100; ++i) {
        CHECK(rig.waves.update(0.0, rig.formation, rig.director) ==
              WaveManager::Event::None);
        CHECK(rig.waves.update(-kDt, rig.formation, rig.director) ==
              WaveManager::Event::None);
    }
    CHECK(rig.waves.remaining() > 1.9);
}
