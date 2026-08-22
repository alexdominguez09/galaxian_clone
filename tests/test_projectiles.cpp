// Stage 6 projectile tests (docs/test_plan.md, Stage 6).
//
// Pure logic tests: no window, no SDL init, no rendering. The
// ProjectileManager is driven with explicit fixed-step dt values, so the
// lifecycle (spawn, move, cull, cooldown, max shots) is verified directly.
//
// Exact-value notes (all arithmetic below is exact in float32):
//   * A player bullet spawns with top-left (222, 510) and velocity
//     (0, -480). Per fixed step it moves 480 * (1/60) = 8 px up, so after n
//     steps its top-left y is 510 - 8n.
//   * It is culled once its box (height 10) is fully above the top edge:
//     bottom = 520 - 8n < 0, i.e. n >= 66 (at n = 65, bottom = 0 exactly,
//     still on screen).
//   * The fire cooldown is 0.35 s = 21 fixed steps.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "core/Constants.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Projectile.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;  // 1/60 s

// Advances the manager by `steps` fixed steps.
void runSteps(ProjectileManager& mgr, int steps)
{
    for (int i = 0; i < steps; ++i) {
        mgr.update(kDt);
    }
}

}  // namespace

TEST_CASE("projectile: fire spawns a bullet above the player moving upward",
          "[projectile]")
{
    Player player;  // start center (224, 528), box top edge y = 520
    ProjectileManager mgr;

    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 1);
    REQUIRE(mgr.count(ProjectileOwner::Player) == 1);

    const Projectile& p = mgr.projectile(0);
    REQUIRE(p.active);
    REQUIRE(p.owner == ProjectileOwner::Player);
    // Top-left: bullet center x = player center x (224), bullet bottom edge
    // touches the player's top edge (520).
    REQUIRE(p.position == Vector2(222.0f, 510.0f));
    REQUIRE(p.velocity == Vector2(0.0f, -ProjectileManager::kPlayerSpeed));
    REQUIRE(p.bounds() ==
            Rect(222.0f, 510.0f, Projectile::kWidth, Projectile::kHeight));

    // After 60 steps (1 s) the bullet has traveled exactly 480 px up.
    runSteps(mgr, 60);
    REQUIRE(mgr.count() == 1);
    REQUIRE(mgr.projectile(0).position == Vector2(222.0f, 30.0f));
}

TEST_CASE("projectile: bullet follows a moved player's position",
          "[projectile]")
{
    Player player;
    for (int i = 30; i > 0; --i) {  // 0.5 s right: x ~ 224 + 110 = 334
        player.update(kDt, +1.0f);
    }
    ProjectileManager mgr;
    REQUIRE(mgr.tryFirePlayer(player));
    // x accumulates 30 float steps of 220 * (1/60): ~333.9998, so the
    // bullet center aligns within float tolerance; y is exact.
    REQUIRE(mgr.projectile(0).position.x ==
            Catch::Approx(player.position().x - Projectile::kWidth / 2.0f));
    REQUIRE(mgr.projectile(0).position.y == 510.0f);
}

TEST_CASE("projectile: removed only when fully above the top edge",
          "[projectile]")
{
    Player player;
    ProjectileManager mgr;
    REQUIRE(mgr.tryFirePlayer(player));

    // n = 65: top-left y = 510 - 520 = -10, bottom = 0. The box is (just)
    // fully above the screen? No — its bottom edge is exactly at y = 0, so
    // it is still on screen and must survive.
    runSteps(mgr, 65);
    REQUIRE(mgr.count() == 1);
    REQUIRE(mgr.projectile(0).bounds().bottom() == 0.0f);

    // n = 66: bottom = -1 < 0: fully off-screen, removed.
    mgr.update(kDt);
    REQUIRE(mgr.count() == 0);
}

TEST_CASE("projectile: a partially visible bullet is never culled",
          "[projectile]")
{
    // Spawn a player bullet already straddling the top edge (top-left
    // y = -4, box spans -4..6): it must survive until it is fully above.
    ProjectileManager mgr;
    REQUIRE(mgr.spawn(ProjectileOwner::Player, Vector2(100.0f, -4.0f),
                      Vector2(0.0f, -480.0f)));
    REQUIRE(mgr.count() == 1);
    REQUIRE(mgr.projectile(0).bounds().bottom() > 0.0f);

    // One step moves it to y = -12 (box -12..-2): fully above, culled.
    mgr.update(kDt);
    REQUIRE(mgr.count() == 0);
}

TEST_CASE("projectile: fire cooldown — a second shot within 0.35 s is rejected",
          "[projectile]")
{
    Player player;
    ProjectileManager mgr;
    REQUIRE(mgr.tryFirePlayer(player));

    // 20 steps = 0.3333 s < 0.35 s: still cooling down.
    runSteps(mgr, 20);
    REQUIRE_FALSE(mgr.canFirePlayer());
    REQUIRE_FALSE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 1);

    // One more step = 21 steps = exactly 0.35 s: the cooldown has elapsed.
    mgr.update(kDt);
    REQUIRE(mgr.canFirePlayer());
    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 2);  // both bullets are still on screen
}

TEST_CASE("projectile: max 2 simultaneous player projectiles enforced",
          "[projectile]")
{
    Player player;
    ProjectileManager mgr;

    REQUIRE(mgr.tryFirePlayer(player));  // bullet A (t = 0)
    runSteps(mgr, 21);                   // t = 0.35 s
    REQUIRE(mgr.tryFirePlayer(player));  // bullet B
    REQUIRE(mgr.count() == 2);

    // t = 0.7 s: the cooldown has elapsed again, but both bullets are still
    // on screen, so the max-simultaneous rule rejects the shot.
    runSteps(mgr, 21);
    REQUIRE(mgr.canFirePlayer() == false);
    REQUIRE(mgr.tryFirePlayer(player) == false);
    REQUIRE(mgr.count() == 2);

    // Let bullet A fly off the top edge; the freed slot allows firing again.
    while (mgr.count() == 2) {
        mgr.update(kDt);
    }
    REQUIRE(mgr.count() == 1);
    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 2);
}

TEST_CASE("projectile: a ship that cannot act cannot fire", "[projectile]")
{
    Player player;
    // Drive the Stage 15 lifecycle to GameOver: every hit costs one life,
    // each Dying phase runs its full 1.5 s delay, respawns are confirmed
    // and the invulnerability window elapses.
    for (int life = 0; life < Player::kLives; ++life) {
        REQUIRE(player.hit());
        player.update(Player::kRespawnDelaySeconds + 0.01, 0.0f);
        if (player.awaitingRespawnConfirm()) {
            player.confirmRespawn();
            player.update(Player::kInvulnerableSeconds + 0.01, 0.0f);
            REQUIRE(player.vulnerable());  // back to plain Alive
        }
    }
    REQUIRE(player.state() == PlayerState::GameOver);

    ProjectileManager mgr;
    REQUIRE_FALSE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 0);
}

TEST_CASE("projectile: enemy bullets move down and are culled at the bottom",
          "[projectile]")
{
    // The shared system (docs/game_spec.md §8) already knows about the Enemy
    // owner; enemy fire itself lands in Stage 14.
    ProjectileManager mgr;
    REQUIRE(mgr.spawn(ProjectileOwner::Enemy, Vector2(100.0f, 100.0f),
                      Vector2(0.0f, 240.0f)));
    REQUIRE(mgr.count() == 1);
    REQUIRE(mgr.count(ProjectileOwner::Player) == 0);
    REQUIRE(mgr.count(ProjectileOwner::Enemy) == 1);

    // 240 px/s = 4 px per fixed step, downward.
    mgr.update(kDt);
    REQUIRE(mgr.projectile(0).position == Vector2(100.0f, 104.0f));

    // An enemy bullet is culled only once its box is fully below the bottom
    // edge (top > kLogicalHeight). Top = 574 < 576: partially visible,
    // survives; after one step top = 578 > 576: culled.
    ProjectileManager mgr2;
    REQUIRE(mgr2.spawn(ProjectileOwner::Enemy, Vector2(100.0f, 574.0f),
                       Vector2(0.0f, 240.0f)));
    mgr2.update(kDt);
    REQUIRE(mgr2.count() == 0);

    // Strict inequality at the edge: top = 576 exactly is still on screen.
    // 360 px/s moves exactly 6 px per fixed step (all integer, exact in
    // float32): after one step top = 576 (survives), after two top = 582.
    ProjectileManager mgr3;
    REQUIRE(mgr3.spawn(ProjectileOwner::Enemy, Vector2(100.0f, 570.0f),
                       Vector2(0.0f, 360.0f)));
    mgr3.update(kDt);  // top = 576.0: NOT > 576, survives
    REQUIRE(mgr3.count() == 1);
    REQUIRE(mgr3.projectile(0).position.y == 576.0f);
    mgr3.update(kDt);  // top = 582.0 > 576: culled
    REQUIRE(mgr3.count() == 0);
}

TEST_CASE("projectile: enemy bullets do not consume the player's cooldown or max",
          "[projectile]")
{
    Player player;
    ProjectileManager mgr;
    REQUIRE(mgr.tryFirePlayer(player));

    // Enemy bullets never count against the player's max of 2.
    REQUIRE(mgr.spawn(ProjectileOwner::Enemy, Vector2(50.0f, 100.0f),
                      Vector2(0.0f, 240.0f)));
    REQUIRE(mgr.spawn(ProjectileOwner::Enemy, Vector2(150.0f, 100.0f),
                      Vector2(0.0f, 240.0f)));
    REQUIRE(mgr.count() == 3);
    REQUIRE(mgr.count(ProjectileOwner::Player) == 1);

    // The player's own cooldown is still the binding constraint.
    runSteps(mgr, 21);
    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count(ProjectileOwner::Player) == 2);
    REQUIRE(mgr.count() == 4);
}

TEST_CASE("projectile: frame-rate independence (30 Hz vs 120 Hz)",
          "[projectile]")
{
    // 1 s of flight from the spawn position: y = 510 - 480 = 30, regardless
    // of the step granularity (the bullet does not reach the top edge).
    auto run = [](double dt, int steps) {
        Player player;
        ProjectileManager mgr;
        REQUIRE(mgr.tryFirePlayer(player));
        for (int i = 0; i < steps; ++i) {
            mgr.update(dt);
        }
        REQUIRE(mgr.count() == 1);
        return mgr.projectile(0).position;
    };

    const Vector2 p30 = run(1.0 / 30.0, 30);
    const Vector2 p120 = run(1.0 / 120.0, 120);

    REQUIRE(std::abs(p30.y - 30.0f) < 0.01f);
    REQUIRE(std::abs(p120.y - 30.0f) < 0.01f);
    REQUIRE(std::abs(p30.y - p120.y) < 0.01f);
    REQUIRE(p30.x == 222.0f);
    REQUIRE(p120.x == 222.0f);
}

TEST_CASE("projectile: reset clears projectiles and cooldowns", "[projectile]")
{
    Player player;
    ProjectileManager mgr;
    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 1);
    REQUIRE_FALSE(mgr.canFirePlayer());  // mid-cooldown

    mgr.reset();
    REQUIRE(mgr.count() == 0);
    REQUIRE(mgr.canFirePlayer());  // the cooldown was reset too
    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 1);
}

TEST_CASE("projectile: generic spawn fails gracefully when the pool is full",
          "[projectile]")
{
    ProjectileManager mgr;
    for (int i = 0; i < ProjectileManager::kMaxProjectiles; ++i) {
        REQUIRE(mgr.spawn(ProjectileOwner::Enemy, Vector2(0.0f, 0.0f),
                          Vector2(0.0f, 1.0f)));
    }
    REQUIRE(mgr.count() == ProjectileManager::kMaxProjectiles);
    REQUIRE_FALSE(mgr.spawn(ProjectileOwner::Enemy, Vector2(0.0f, 0.0f),
                            Vector2(0.0f, 1.0f)));
    REQUIRE(mgr.count() == ProjectileManager::kMaxProjectiles);
}

TEST_CASE("projectile: 10 000 shots headlessly, active count returns to 0",
          "[projectile][stress]")
{
    // The lifecycle must be completely stable: fire 10 000 shots (paced by
    // the cooldown and the 2-bullet cap), let everything fly off, and the
    // manager must be back to empty and fully usable. The fixed pool
    // allocates nothing, so this also proves there is no per-shot leak.
    Player player;
    ProjectileManager mgr;

    int spawned = 0;
    while (spawned < 10000) {
        if (mgr.tryFirePlayer(player)) {
            ++spawned;
        }
        REQUIRE(mgr.count() <= ProjectileManager::kMaxProjectiles);
        mgr.update(kDt);
    }
    // Drain the last bullets.
    while (mgr.count() > 0) {
        mgr.update(kDt);
    }
    REQUIRE(mgr.count() == 0);

    // The pool is fully recycled: the manager still works normally.
    REQUIRE(mgr.tryFirePlayer(player));
    REQUIRE(mgr.count() == 1);
}

TEST_CASE("projectile: stress — continuous fire for 5 simulated minutes",
          "[projectile][stress]")
{
    // Attempt to fire every fixed step for 5 minutes of simulation time.
    // The arcade limits (0.35 s cooldown, max 2 simultaneous) must hold at
    // every step, and the system must end clean with no growth or
    // corruption (stable memory: the fixed pool never allocates).
    Player player;
    ProjectileManager mgr;

    const int steps = 300 * 60;  // 5 minutes at the fixed 60 Hz step
    int spawned = 0;
    for (int i = 0; i < steps; ++i) {
        if (mgr.tryFirePlayer(player)) {
            ++spawned;
        }
        REQUIRE(mgr.count(ProjectileOwner::Player) <=
                ProjectileManager::kMaxPlayerProjectiles);
        mgr.update(kDt);
    }
    while (mgr.count() > 0) {
        mgr.update(kDt);
    }
    REQUIRE(mgr.count() == 0);

    // Deterministic simulation: the exact spawn count is 546. Derivation:
    // a bullet spawned at step s is alive for tries at steps s..s+65
    // (culled during update s+65). With a 21-step cooldown and the 2-bullet
    // cap, shots land at steps 0, 21, then 66+66k and 87+66k (k >= 0) while
    // <= 17999: 2 + 272 + 272 = 546.
    REQUIRE(spawned == 546);
}
