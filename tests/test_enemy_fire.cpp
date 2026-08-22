// Stage 14 enemy projectile tests (docs/test_plan.md, Stage 14).
//
// Pure logic: ProjectileManager::tryFireEnemy and the Enemy shot-trigger
// machinery are SDL-free (dependency rule); no window, no rendering.
//
// Exact-value notes (scratch-derived by linking the real DivePath code;
// a "shot step" is the first Diving update whose path parameter reaches
// the trigger point):
//   * Scout CenterAttack from slot box (32, 208), 140 px/s: arc length
//     419.3125 -> t >= 0.35 on update 63, t >= 0.50 on update 90,
//     t >= 0.75 on update 135.
//   * Commander LeftDive from slot box (176, 64), 70 px/s: 480.442596 ->
//     t >= 0.50 on update 206.
//   * Guard CenterAttack from slot box (176, 100), 100 px/s: t >= 0.35 on
//     update 89.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

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

constexpr double kDt = kFixedDeltaSeconds;

// Drives `enemy` through its dive against a fixed anchor and collects the
// Diving-update indices at which fire events were raised. Preparation and
// pull-out produce no events.
std::vector<int> collectShotSteps(Enemy& enemy, DivePattern pattern,
                                  int shots)
{
    std::vector<int> steps;
    REQUIRE(enemy.beginDive(pattern, shots));
    // 30 prepare updates: never any fire event during PreparingDive.
    for (int i = 0; i < 30; ++i) {
        enemy.update(kDt, EnemyFormation::kAnchor);
        CHECK(enemy.drainPendingShots() == 0);
    }
    // The dive itself.
    for (int step = 1; step <= 2000 && enemy.state() == EnemyState::Diving;
         ++step) {
        enemy.update(kDt, EnemyFormation::kAnchor);
        if (enemy.state() != EnemyState::Diving) {
            break;  // pull-out this step; firing only happens while Diving
        }
        const int fired = enemy.drainPendingShots();
        for (int k = 0; k < fired; ++k) {
            steps.push_back(step);
        }
    }
    return steps;
}

}  // namespace

TEST_CASE("enemy fire: tryFireEnemy aims at the target at fire time",
          "[enemyfire]")
{
    ProjectileManager mgr;
    const Vector2 muzzle{44.0f, 232.0f};
    const Vector2 aim{224.0f, 528.0f};  // the player's start centre

    REQUIRE(mgr.tryFireEnemy(muzzle, aim));
    REQUIRE(mgr.count() == 1);
    const Projectile& p = mgr.projectile(0);
    CHECK(p.owner == ProjectileOwner::Enemy);
    // The 4x10 box is centred on the muzzle point.
    CHECK(p.position == Vector2{muzzle.x - Projectile::kWidth * 0.5f,
                                muzzle.y - Projectile::kHeight * 0.5f});
    // Velocity points at the aim with magnitude kEnemySpeed.
    const float dx = aim.x - muzzle.x;
    const float dy = aim.y - muzzle.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    const float speed = static_cast<float>(ProjectileManager::kEnemySpeed);
    CHECK(p.velocity.x == Catch::Approx(dx / len * speed).margin(1e-3));
    CHECK(p.velocity.y == Catch::Approx(dy / len * speed).margin(1e-3));
}

TEST_CASE("enemy fire: degenerate aim falls back straight down",
          "[enemyfire]")
{
    ProjectileManager mgr;
    REQUIRE(mgr.tryFireEnemy(Vector2{200.0f, 300.0f}, Vector2{200.0f, 300.0f}));
    const Projectile& p = mgr.projectile(0);
    CHECK(p.velocity ==
          Vector2{0.0f, static_cast<float>(ProjectileManager::kEnemySpeed)});
}

TEST_CASE("enemy fire: pool-full rejection is graceful", "[enemyfire]")
{
    ProjectileManager mgr;
    for (int i = 0; i < ProjectileManager::kMaxProjectiles; ++i) {
        REQUIRE(mgr.tryFireEnemy(Vector2{10.0f, 10.0f},
                                 Vector2{20.0f, 20.0f}));
    }
    CHECK_FALSE(
        mgr.tryFireEnemy(Vector2{10.0f, 10.0f}, Vector2{20.0f, 20.0f}));
    CHECK(mgr.count() == ProjectileManager::kMaxProjectiles);
}

TEST_CASE("enemy fire: a one-shot dive fires exactly once, at midpoint",
          "[enemyfire][enemy]")
{
    Enemy scout(EnemyType::Scout, EnemyFormation::slotOffset(4, 0));
    const auto shots =
        collectShotSteps(scout, DivePattern::CenterAttack, 1);

    REQUIRE(shots.size() == 1);
    CHECK(shots[0] == 90);  // scratch: t crosses 0.50 on the 90th update

    // After the dive ends (pull-out reached), nothing more fires.
    for (int i = 0; i < 200; ++i) {
        scout.update(kDt, EnemyFormation::kAnchor);
        CHECK(scout.drainPendingShots() == 0);
    }
}

TEST_CASE("enemy fire: a two-shot dive fires at both quartile triggers",
          "[enemyfire][enemy]")
{
    Enemy scout(EnemyType::Scout, EnemyFormation::slotOffset(4, 0));
    const auto shots =
        collectShotSteps(scout, DivePattern::CenterAttack, 2);

    REQUIRE(shots.size() == 2);
    CHECK(shots[0] == 63);   // t crosses 0.35
    CHECK(shots[1] == 135);  // t crosses 0.75
}

TEST_CASE("enemy fire: trigger timing scales with speed and arc",
          "[enemyfire][enemy]")
{
    // A slower type on a longer arc crosses the midpoint much later.
    Enemy commander(EnemyType::Commander, EnemyFormation::slotOffset(0, 3));
    const auto commanderShots =
        collectShotSteps(commander, DivePattern::LeftDive, 1);
    REQUIRE(commanderShots.size() == 1);
    CHECK(commanderShots[0] == 206);

    // A mid-speed type on the same center pattern: quartiles at 89/189.
    Enemy guard(EnemyType::Guard, EnemyFormation::slotOffset(1, 3));
    const auto guardShots =
        collectShotSteps(guard, DivePattern::CenterAttack, 2);
    REQUIRE(guardShots.size() == 2);
    CHECK(guardShots[0] == 89);   // t >= 0.35 (scratch)
    CHECK(guardShots[1] == 189);  // t >= 0.75 (scratch)
}

TEST_CASE("enemy fire: the shot budget clamps to the spec range",
          "[enemyfire][enemy]")
{
    // Spec §6.4: 1-2 projectiles. Larger budgets clamp to 2...
    Enemy scout(EnemyType::Scout, EnemyFormation::slotOffset(4, 0));
    auto shots = collectShotSteps(scout, DivePattern::CenterAttack, 7);
    CHECK(shots.size() == 2);
    // ...and a zero budget means silence.
    Enemy scout2(EnemyType::Scout, EnemyFormation::slotOffset(4, 0));
    shots = collectShotSteps(scout2, DivePattern::CenterAttack, 0);
    CHECK(shots.empty());
}

TEST_CASE("enemy fire: ownership — enemy bullets never damage enemies; "
          "player bullets only ever target enemies",
          "[enemyfire][combat]")
{
    // Spec §8 ownership, exercised with live enemy bullets in flight:
    // resolvePlayerBullets must ignore Enemy-owned rounds entirely.
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // An enemy bullet sitting right INSIDE slot (4,1)'s box.
    REQUIRE(projectiles.tryFireEnemy(Vector2{92.0f, 220.0f},
                                     Vector2{92.0f, 576.0f}));
    CHECK(projectiles.count(ProjectileOwner::Enemy) == 1);

    // ...plus a genuine player threat overlapping slot (4,2).
    REQUIRE(projectiles.spawn(ProjectileOwner::Player,
                              Vector2{138.0f, 218.0f},
                              Vector2{0.0f, -480.0f}));

    const int kills = combat::resolvePlayerBullets(
        projectiles, formation, score, effects);

    // Only the PLAYER bullet resolved; the enemy bullet is untouched and
    // its overlapping enemy is unharmed (friendly fire impossible).
    CHECK(kills == 1);
    CHECK_FALSE(formation.at(4, 2).alive());
    CHECK(formation.at(4, 1).alive());
    CHECK(score.score() == 50);  // one Scout kill, nothing extra
    CHECK(effects.count() == 1); // one destruction site
    CHECK(projectiles.count(ProjectileOwner::Enemy) == 1);
    CHECK(projectiles.count(ProjectileOwner::Player) == 0);  // consumed

    // And the enemy bullet keeps flying down towards the bottom cull
    // (240 px/s = 4 px per fixed step).
    projectiles.update(kDt);
    CHECK(projectiles.count(ProjectileOwner::Enemy) == 1);
    CHECK(projectiles.projectile(0).position.y ==
          Catch::Approx(215.0f + 4.0f).margin(1e-4));
}
