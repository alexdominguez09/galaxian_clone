// Stage 9 combat tests (docs/test_plan.md, Stage 9 — Player vs enemy
// combat).
//
// Pure logic: gameplay/Combat is SDL-free (dependency rule,
// docs/architecture.md §1), so no window, no SDL init, no rendering. The
// end-to-end pixel check that a bullet visibly destroys a formation enemy
// lives in test_rendering.cpp.
//
// Exact-value notes (arithmetic verified against the game's exact float32
// operations; a player bullet moves 480 * (1/60) = exactly 8 px per fixed
// step in float32):
//   * The formation is at the spec anchor: enemy(r,c) box top-left is
//     (32 + 48c, 64 + 36r), size 24x24. The gaps between boxes are 24 px
//     horizontally and 12 px vertically, so a 4x10 bullet box can overlap
//     at most ONE enemy box — the "one bullet, one kill" rule is structural
//     and the tests pin down its mechanism.
//   * A bullet centered in slot (r,c) (top-left (32+48c+10, 64+36r+10))
//     lies strictly inside that one box.
//   * A bullet spawned at (42, 238) (column 0, below the row-4 box) passes
//     through the dead row-4 slot at y = 230, 222, 214, 206 (steps 1-4),
//     sits between the rows at y = 198 (step 5), and overlaps the row-3
//     box [172,196] from y = 190 (step 6) on.
//   * A destruction effect (duration 0.25 s) expires on its 15th fixed-step
//     update (after 14 updates the remaining 0.016667 s is above the 1 ns
//     tolerance; the 15th update brings it to ~5e-17 s).

#include <catch2/catch_test_macros.hpp>

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

// Spawns a player bullet whose 4x10 box lies strictly inside the 24x24 box
// of the enemy at (row, col) of a formation at the spec anchor.
bool spawnCenteredBullet(ProjectileManager& mgr, int row, int col)
{
    const Vector2 tl =
        EnemyFormation::kAnchor + EnemyFormation::slotOffset(row, col);
    return mgr.spawn(ProjectileOwner::Player, Vector2{tl.x + 10.0f, tl.y + 10.0f},
                     Vector2{0.0f, -ProjectileManager::kPlayerSpeed});
}

// One fixed step of the Stage 9 combat chain, the same order
// Game::fixedUpdate() uses: move + cull the bullets, then resolve player
// bullets vs the formation. Returns the kills of the step.
int step(ProjectileManager& projectiles, EnemyFormation& formation,
         ScoreManager& score, EffectManager& effects)
{
    projectiles.update(kDt);
    return combat::resolvePlayerBullets(projectiles, formation, score,
                                        effects);
}

}  // namespace

TEST_CASE("combat: a bullet hit kills the enemy, consumes the bullet, awards "
          "the score",
          "[combat][enemy]")
{
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // Row 4 col 0 is a Scout (50 pts), box top-left (32, 208).
    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));
    const int kills =
        combat::resolvePlayerBullets(projectiles, formation, score, effects);

    CHECK(kills == 1);
    CHECK_FALSE(formation.at(4, 0).alive());
    CHECK(formation.at(4, 0).state() == EnemyState::Dead);
    // The bullet was consumed on the hit.
    CHECK(projectiles.count() == 0);
    // The type's base points were awarded through the ScoreManager.
    CHECK(score.score() == 50);
    CHECK(score.kills() == 1);
    // A placeholder destruction effect sits on the enemy's box.
    CHECK(effects.count() == 1);
    CHECK(effects.effect(0).bounds() == Rect{32.0f, 208.0f, 24.0f, 24.0f});
    // The other 39 enemies are untouched (the hole stays, spec §6.3).
    CHECK(formation.aliveCount() == 39);
    CHECK(formation.at(3, 0).alive());
    CHECK(formation.at(4, 1).alive());
    // The slot offset is unchanged (no re-pack; the formation is intact).
    CHECK(formation.at(4, 0).slotOffset() == EnemyFormation::slotOffset(4, 0));
    CHECK(formation.positionOf(4, 0) ==
          EnemyFormation::kAnchor + EnemyFormation::slotOffset(4, 0));
}

TEST_CASE("combat: score increases by the type's point value",
          "[combat][score][enemy]")
{
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // One kill per type, in different columns (independent boxes):
    // Scout (row 4) 50, Guard (row 2) 80, Commander (row 0) 150.
    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 1);
    CHECK(score.score() == 50);

    REQUIRE(spawnCenteredBullet(projectiles, 2, 1));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 1);
    CHECK(score.score() == 50 + 80);

    REQUIRE(spawnCenteredBullet(projectiles, 0, 2));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 1);
    CHECK(score.score() == 50 + 80 + 150);
    CHECK(score.kills() == 3);
    CHECK(formation.aliveCount() == 37);
}

TEST_CASE("combat: one bullet cannot kill two enemies (consumed on first hit)",
          "[combat][enemy]")
{
    // A 4x10 bullet can overlap at most one box of the spec grid (the gaps
    // are 24 px wide / 12 px tall); the rule to pin down is the mechanism:
    // the bullet that hits an enemy is CONSUMED, so it can never continue
    // on and destroy a second one.
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 1);

    CHECK_FALSE(formation.at(4, 0).alive());
    CHECK(projectiles.count() == 0);  // consumed: nothing left to fly on
    CHECK(score.score() == 50);       // exactly one kill scored
    // Nothing else died.
    CHECK(formation.at(4, 1).alive());
    CHECK(formation.at(3, 0).alive());
    CHECK(formation.aliveCount() == 39);
    CHECK(score.kills() == 1);
}

TEST_CASE("combat: a dead enemy cannot be scored twice (sequentially)",
          "[combat][enemy]")
{
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // First shot destroys the enemy...
    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 1);
    CHECK(score.score() == 50);

    // ...a later bullet that overlaps only the dead enemy's hole is NOT
    // consumed (it passes through) and awards no second score.
    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 0);
    CHECK(score.score() == 50);
    CHECK(score.kills() == 1);
    CHECK(projectiles.count() == 1);  // it flew through the hole
    CHECK(formation.aliveCount() == 39);
}

TEST_CASE("combat: a dead enemy cannot be scored twice (two bullets, one "
          "enemy, same step)",
          "[combat][enemy]")
{
    // Two bullets overlap the same living enemy in the same step: exactly
    // one kill, one score, and only the first bullet is consumed.
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // Both boxes lie strictly inside the (4,0) box [32,56]x[208,232].
    REQUIRE(projectiles.spawn(ProjectileOwner::Player, Vector2{42.0f, 218.0f},
                              Vector2{0.0f, -480.0f}));
    REQUIRE(projectiles.spawn(ProjectileOwner::Player, Vector2{43.0f, 219.0f},
                              Vector2{0.0f, -480.0f}));

    const int kills =
        combat::resolvePlayerBullets(projectiles, formation, score, effects);

    CHECK(kills == 1);
    CHECK_FALSE(formation.at(4, 0).alive());
    CHECK(score.score() == 50);
    CHECK(score.kills() == 1);
    CHECK(effects.count() == 1);  // one destruction, one placeholder
    // The first bullet (pool order) was consumed; the second, finding its
    // target already dead, passed through and is still alive.
    CHECK(projectiles.count() == 1);
    CHECK(projectiles.projectile(0).position == Vector2{43.0f, 219.0f});
    CHECK(formation.aliveCount() == 39);
}

TEST_CASE("combat: a bullet passes through dead holes and kills the next "
          "living enemy",
          "[combat][enemy]")
{
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // Row 4 col 0 is already destroyed...
    formation.at(4, 0).kill();
    // ...and a bullet flies up column 0 from below the row-4 box.
    REQUIRE(projectiles.spawn(ProjectileOwner::Player, Vector2{42.0f, 238.0f},
                              Vector2{0.0f, -480.0f}));

    // Steps 1-4 (y = 230, 222, 214, 206): the box overlaps the dead (4,0)
    // slot but nothing is consumed or scored.
    for (int i = 1; i <= 4; ++i) {
        CHECK(step(projectiles, formation, score, effects) == 0);
        CHECK(projectiles.count() == 1);
        CHECK(score.score() == 0);
    }
    // Step 5 (y = 198): the box touches neither row (zero-area contact is
    // not a collision).
    CHECK(step(projectiles, formation, score, effects) == 0);
    CHECK(projectiles.count() == 1);
    // Step 6 (y = 190, box [190,200] overlaps the row-3 box [172,196]):
    // the living (3,0) Scout is destroyed and the bullet is consumed.
    CHECK(step(projectiles, formation, score, effects) == 1);
    CHECK(projectiles.count() == 0);
    CHECK_FALSE(formation.at(3, 0).alive());
    CHECK(score.score() == 50);  // only the (3,0) kill is scored
    // 40 - the pre-killed (4,0) - the bullet's kill of (3,0).
    CHECK(formation.aliveCount() == 38);
}

TEST_CASE("combat: enemy-owned bullets never damage enemies (ownership)",
          "[combat][enemy]")
{
    // Spec §8 ownership: enemy bullets never damage enemies (Stage 14
    // resolves them against the player).
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    REQUIRE(projectiles.spawn(ProjectileOwner::Enemy, Vector2{42.0f, 218.0f},
                              Vector2{0.0f, 240.0f}));
    CHECK(combat::resolvePlayerBullets(projectiles, formation, score,
                                       effects) == 0);
    CHECK(formation.at(4, 0).alive());
    CHECK(projectiles.count() == 1);  // the enemy bullet is untouched
    CHECK(score.score() == 0);
    CHECK(effects.count() == 0);
}

TEST_CASE("combat: two bullets can kill two enemies in the same step",
          "[combat][enemy]")
{
    // The "one bullet, one kill" rule does not cap kills per STEP: two
    // bullets that reach two different enemies both score.
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));  // Scout
    REQUIRE(spawnCenteredBullet(projectiles, 4, 2));  // Scout
    CHECK(combat::resolvePlayerBullets(projectiles, formation, score,
                                       effects) == 2);
    CHECK_FALSE(formation.at(4, 0).alive());
    CHECK_FALSE(formation.at(4, 2).alive());
    CHECK(projectiles.count() == 0);
    CHECK(score.score() == 100);
    CHECK(score.kills() == 2);
    CHECK(effects.count() == 2);
    CHECK(formation.aliveCount() == 38);
}

TEST_CASE("combat: destroying all 40 enemies works without corruption",
          "[combat][enemy][stress]")
{
    // The acceptance case, scripted deterministically: every slot of the
    // 5x8 grid is destroyed exactly once. The score must be the spec total
    // 8*150 + 16*80 + 16*50 = 3280, and the systems must come out clean.
    // Ten rounds prove repeatability.
    for (int round = 0; round < 10; ++round) {
        EnemyFormation formation;
        ScoreManager score;
        EffectManager effects;
        ProjectileManager projectiles;

        // The 16-slot effect pool fills up partway through; the adds must
        // fail gracefully (no growth, no crash) — verified below.
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                REQUIRE(spawnCenteredBullet(projectiles, row, col));
                REQUIRE(combat::resolvePlayerBullets(projectiles, formation,
                                                     score, effects) == 1);
                REQUIRE_FALSE(formation.at(row, col).alive());
                // Each bullet is consumed by its own kill.
                CHECK(projectiles.count() == 0);
            }
        }

        CHECK(formation.aliveCount() == 0);
        CHECK(score.score() == 3280);
        CHECK(score.kills() == 40);
        CHECK(projectiles.count() == 0);
        // The pool held at most kMaxEffects effects; the rest were rejected.
        CHECK(effects.count() == EffectManager::kMaxEffects);

        // The formation data is uncorrupted: the grid geometry is exact,
        // the holes are in place, and the world position is at the anchor.
        CHECK(formation.position() == EnemyFormation::kAnchor);
        CHECK(formation.positionOf(4, 7) == Vector2{368.0f, 208.0f});
        CHECK(formation.boundsOf(0, 0) == Rect{32.0f, 64.0f, 24.0f, 24.0f});
        for (int index = 0; index < EnemyFormation::kTotal; ++index) {
            CHECK(formation.at(index).state() == EnemyState::Dead);
        }

        // All the surviving effects expire after exactly 15 fixed steps.
        for (int i = 0; i < 14; ++i) {
            effects.update(kDt);
        }
        CHECK(effects.count() == EffectManager::kMaxEffects);
        effects.update(kDt);
        CHECK(effects.count() == 0);

        // The systems are still fully usable (no stale state).
        CHECK(projectiles.spawn(ProjectileOwner::Player, Vector2{100.0f, 100.0f},
                                Vector2{0.0f, -480.0f}));
        CHECK(projectiles.count() == 1);
        CHECK(score.addKill(EnemyType::Scout) == 50);
        CHECK(score.score() == 3330);
    }
}

TEST_CASE("combat: a destruction effect spawns at the kill site and lasts "
          "15 fixed steps",
          "[combat][enemy]")
{
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    REQUIRE(spawnCenteredBullet(projectiles, 4, 0));
    REQUIRE(combat::resolvePlayerBullets(projectiles, formation, score,
                                         effects) == 1);

    CHECK(effects.count() == 1);
    CHECK(effects.effect(0).bounds() == Rect{32.0f, 208.0f, 24.0f, 24.0f});
    CHECK(effects.effect(0).timeRemaining > 0.0);

    // 0.25 s = 15 fixed steps: alive after 14 updates...
    for (int i = 0; i < 14; ++i) {
        effects.update(kDt);
    }
    CHECK(effects.count() == 1);
    // ...expired by the 15th.
    effects.update(kDt);
    CHECK(effects.count() == 0);
}

TEST_CASE("combat: no hit when the bullet does not overlap any enemy",
          "[combat][enemy]")
{
    // A bullet in the 24 px gap between columns 0 and 1 of row 4 overlaps
    // neither box; resolution is a no-op.
    EnemyFormation formation;
    ScoreManager score;
    EffectManager effects;
    ProjectileManager projectiles;

    // Column 0 box ends at x = 56, column 1 box starts at x = 80.
    REQUIRE(projectiles.spawn(ProjectileOwner::Player, Vector2{66.0f, 218.0f},
                              Vector2{0.0f, -480.0f}));
    CHECK(combat::resolvePlayerBullets(projectiles, formation, score,
                                       effects) == 0);
    CHECK(formation.aliveCount() == 40);
    CHECK(projectiles.count() == 1);
    CHECK(score.score() == 0);
    CHECK(effects.count() == 0);
}
