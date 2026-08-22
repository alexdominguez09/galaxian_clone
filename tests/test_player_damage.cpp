// Stage 15 player damage/lifecycle tests (docs/test_plan.md, Stage 15).
//
// Pure logic: combat::resolveEnemyThreats plus the Player lifecycle are
// SDL-free (dependency rule); no window, no SDL init, no rendering.
//
// Exact-value notes (scratch-verified countdown drift, same 1 ns tolerance
// as every other simulation timer):
//   * The 1.5 s Dying delay expires exactly on the 90th fixed step
//     (leftover ~3e-16 s <= epsilon); alive through 89 updates.
//   * The 2.0 s Invulnerable window expires exactly on the 120th step.
//   * The ship's start centre is (224, 528): box [212..236]x[520..536].
//     Scout (4,4)'s CenterAttack plunges straight down x=224 through that
//     band, which makes a deterministic body-collision scenario.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/Constants.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/DivePath.hpp"
#include "gameplay/Effects.hpp"
#include "gameplay/EnemyFormation.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Projectile.hpp"
#include "gameplay/ScoreManager.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;

}  // namespace

TEST_CASE("damage: an enemy bullet costs exactly one life", "[damage]")
{
    Player player;
    EnemyFormation formation;
    ProjectileManager projectiles;

    // An enemy bullet right at the ship's centre, heading straight down.
    REQUIRE(projectiles.tryFireEnemy(Vector2{224.0f, 528.0f},
                                     Vector2{224.0f, 600.0f}));
    REQUIRE(projectiles.count(ProjectileOwner::Enemy) == 1);

    CHECK(combat::resolveEnemyThreats(projectiles, formation, player) == 1);
    CHECK(player.lives() == Player::kLives - 1);  // EXACTLY one life
    CHECK(player.state() == PlayerState::Dying);
    CHECK(player.stateTimer() == Player::kRespawnDelaySeconds);
    CHECK_FALSE(player.vulnerable());
    // The offending bullet was consumed by the hit.
    CHECK(projectiles.count(ProjectileOwner::Enemy) == 0);
}

TEST_CASE("damage: a player bullet never damages the player",
          "[damage][combat]")
{
    // Spec §8 ownership: Player-owned rounds are never checked against
    // the player.
    Player player;
    EnemyFormation formation;
    ProjectileManager projectiles;

    REQUIRE(projectiles.spawn(ProjectileOwner::Player,
                              Vector2{222.0f, 524.0f},
                              Vector2{0.0f, -480.0f}));  // inside the ship

    CHECK(combat::resolveEnemyThreats(projectiles, formation, player) == 0);
    CHECK(player.lives() == Player::kLives);
    CHECK(player.vulnerable());
    CHECK(projectiles.count(ProjectileOwner::Player) == 1);  // untouched
}

TEST_CASE("damage: two collisions in the same frame remove exactly one "
          "life",
          "[damage][combat]")
{
    Player player;
    EnemyFormation formation;
    ProjectileManager projectiles;

    // Two enemy bullets overlapping the ship simultaneously.
    REQUIRE(projectiles.tryFireEnemy(Vector2{220.0f, 528.0f},
                                     Vector2{220.0f, 600.0f}));
    REQUIRE(projectiles.tryFireEnemy(Vector2{228.0f, 528.0f},
                                     Vector2{228.0f, 600.0f}));
    REQUIRE(projectiles.count(ProjectileOwner::Enemy) == 2);

    CHECK(combat::resolveEnemyThreats(projectiles, formation, player) == 1);
    CHECK(player.lives() == Player::kLives - 1);  // ONE life only

    // Only the first threat was consumed; the second flew on...
    CHECK(projectiles.count(ProjectileOwner::Enemy) == 1);
    // ...but the ship is Dying now, so later steps can never land another
    // hit even while the surviving bullet keeps crossing the hull.
    for (int i = 0; i < 10; ++i) {
        projectiles.update(kDt);
        CHECK(combat::resolveEnemyThreats(projectiles, formation, player) ==
              0);
    }
    CHECK(player.lives() == Player::kLives - 1);
}

TEST_CASE("damage: an enemy body costs exactly one life and cannot "
          "multi-hit",
          "[damage][combat][enemy]")
{
    Player player;
    EnemyFormation formation;
    ProjectileManager projectiles;

    // Scout (4,4)'s CenterAttack plunges straight down the x=224 column --
    // right through the ship's start band. The diver is driven DIRECTLY
    // against the fixed anchor so the path is deterministic (running the
    // full formation here would freeze an arbitrary swayed peel-off x).
    Enemy& diver = formation.at(4, 4);
    REQUIRE(diver.beginDive(DivePattern::CenterAttack, 0));
    const Vector2 fp = formation.position();  // static anchor (32, 64)

    int hits = 0;
    int steps = 0;
    while (hits == 0 && steps < 1000) {
        diver.update(kDt, fp);
        hits += combat::resolveEnemyThreats(projectiles, formation, player);
        ++steps;
    }
    REQUIRE(hits == 1);
    REQUIRE(steps > 30);  // genuinely the diving BODY, not spawn overlap
    CHECK(player.lives() == Player::kLives - 1);
    CHECK(player.state() == PlayerState::Dying);

    // The body keeps sweeping through the band for many updates, but a
    // Dying ship is immune: never a second life.
    int extra = 0;
    for (int i = 0; i < 60; ++i) {
        diver.update(kDt, fp);
        extra += combat::resolveEnemyThreats(projectiles, formation, player);
    }
    CHECK(extra == 0);
    CHECK(player.lives() == Player::kLives - 1);
}

TEST_CASE("lifecycle: respawn after exactly 1.5 s at start with 2 s of "
          "invulnerability",
          "[damage][lifecycle]")
{
    Player player;
    REQUIRE(player.hit());
    CHECK(player.lives() == Player::kLives - 1);

    for (int i = 0; i < 89; ++i) {
        player.update(kDt, 0.0f);
        CHECK_FALSE(player.awaitingRespawnConfirm());
    }
    player.update(kDt, 0.0f);  // the 90th update crosses 1.5 s
    CHECK(player.awaitingRespawnConfirm());

    // The caller clears nearby enemy bullets and confirms: back to start.
    player.confirmRespawn();
    CHECK(player.state() == PlayerState::Invulnerable);
    CHECK(player.alive());             // controllable again
    CHECK_FALSE(player.vulnerable());  // but immune
    CHECK(player.position() == Player::kStartPosition);
    CHECK(player.stateTimer() == Player::kInvulnerableSeconds);

    for (int i = 0; i < 119; ++i) {
        player.update(kDt, 0.0f);
        CHECK(player.state() == PlayerState::Invulnerable);
    }
    player.update(kDt, 0.0f);  // the 120th update crosses 2.0 s
    CHECK(player.state() == PlayerState::Alive);
    CHECK(player.vulnerable());
}

TEST_CASE("lifecycle: the invulnerable ship is controllable but ignores "
          "collisions",
          "[damage][lifecycle]")
{
    Player player;
    EnemyFormation formation;
    ProjectileManager projectiles;

    REQUIRE(player.hit());
    player.update(Player::kRespawnDelaySeconds + 1e-6, 0.0f);
    REQUIRE(player.awaitingRespawnConfirm());
    player.confirmRespawn();
    REQUIRE(player.state() == PlayerState::Invulnerable);

    // Controllable: moves left (~220 px/s) and fires during the blink.
    const float x0 = player.position().x;
    for (int i = 0; i < 60; ++i) {
        player.update(kDt, -1.0f);
        player.fire();
    }
    CHECK(player.position().x < x0 - 200.0f);
    CHECK(player.fireCount() == 60);

    // Immune: a bullet sitting INSIDE the ship's box does nothing at all
    // (it is not even consumed -- it flies straight through the blink).
    REQUIRE(projectiles.tryFireEnemy(
        player.position(),
        Vector2{player.position().x, static_cast<float>(kLogicalHeight)}));
    CHECK(combat::resolveEnemyThreats(projectiles, formation, player) == 0);
    CHECK(player.lives() == Player::kLives - 1);  // only the original hit
    CHECK(projectiles.count(ProjectileOwner::Enemy) == 1);

    // The window expires into plain vulnerable Alive.
    player.update(Player::kInvulnerableSeconds + 1e-6, 0.0f);
    CHECK(player.state() == PlayerState::Alive);
    CHECK(player.vulnerable());
}

TEST_CASE("lifecycle: respawn clears the enemy projectiles",
          "[damage][lifecycle]")
{
    Player player;
    ProjectileManager projectiles;

    // Scattered enemy rounds everywhere plus one friendly one.
    for (int i = 0; i < 4; ++i) {
        REQUIRE(projectiles.tryFireEnemy(Vector2{40.0f * i, 100.0f},
                                         Vector2{40.0f * i, 300.0f}));
    }
    REQUIRE(projectiles.spawn(ProjectileOwner::Player,
                              Vector2{224.0f, 500.0f},
                              Vector2{0.0f, -480.0f}));

    REQUIRE(player.hit());
    player.update(Player::kRespawnDelaySeconds + 1e-6, 0.0f);
    REQUIRE(player.awaitingRespawnConfirm());

    // The composition-root handoff (mirrors Game::fixedUpdate): wipe the
    // enemy rounds, confirm the respawn.
    CHECK(projectiles.removeAll(ProjectileOwner::Enemy) == 4);
    player.confirmRespawn();

    CHECK(projectiles.count(ProjectileOwner::Enemy) == 0);
    CHECK(projectiles.count(ProjectileOwner::Player) == 1);  // untouched
    CHECK(player.state() == PlayerState::Invulnerable);
}

TEST_CASE("lifecycle: zero lives means GameOver and no respawn",
          "[damage][lifecycle]")
{
    Player player;
    EnemyFormation formation;
    ProjectileManager projectiles;

    for (int life = 0; life < Player::kLives; ++life) {
        REQUIRE(player.hit());
        for (int i = 0; i < 90 && !player.awaitingRespawnConfirm(); ++i) {
            player.update(kDt, 0.0f);
        }
        if (life < Player::kLives - 1) {
            CHECK(player.awaitingRespawnConfirm());
            player.confirmRespawn();
            CHECK(player.state() == PlayerState::Invulnerable);
            player.update(Player::kInvulnerableSeconds + 1e-6, 0.0f);
            CHECK(player.vulnerable());
        }
    }

    // The third death had no lives left: straight to GameOver.
    CHECK(player.state() == PlayerState::GameOver);
    CHECK_FALSE(player.awaitingRespawnConfirm());
    CHECK_FALSE(player.alive());
    CHECK(player.lives() == 0);

    // Terminal: nothing revives it, nothing hurts it.
    for (int i = 0; i < 240; ++i) {
        player.update(kDt, +1.0f);
    }
    CHECK(player.state() == PlayerState::GameOver);
    CHECK_FALSE(player.alive());
    REQUIRE(projectiles.tryFireEnemy(Vector2{224.0f, 528.0f},
                                     Vector2{224.0f, 600.0f}));
    CHECK(combat::resolveEnemyThreats(projectiles, formation, player) == 0);
}
