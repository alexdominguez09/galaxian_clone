#pragma once

#include "gameplay/Effects.hpp"
#include "gameplay/EnemyFormation.hpp"
#include "gameplay/Projectile.hpp"
#include "gameplay/ScoreManager.hpp"

namespace galaxian {
namespace combat {

// Resolves player-bullet vs enemy collisions for one fixed simulation step
// (Stage 9: the plan's "Player Projectile -> Collision -> Enemy destroyed
// -> Score awarded" chain).
//
// Rules:
//   - Only Player-owned bullets are considered (docs/game_spec.md §8
//     ownership: enemy bullets never damage enemies — they are skipped;
//     Stage 14 resolves enemy bullets against the player).
//   - For each bullet (pool order), the first living enemy whose box
//     intersects the bullet box — row-major scan of the grid,
//     docs/game_spec.md §6.2 layout — is killed and the bullet is consumed:
//     one bullet can never destroy two enemies, and a dead enemy is never
//     scored twice.
//   - A bullet that overlaps only dead enemies passes through untouched
//     (dead enemies leave holes, spec §6.3).
//   - A kill awards the enemy type's base points (spec §6.1) through the
//     ScoreManager and spawns a placeholder destruction effect at the
//     enemy's box (gameplay/Effects, Stage 19 replaces it with the
//     explosion animation).
//
// Must run after ProjectileManager::update(dt) for the same step, so the
// test uses each bullet's new position. Uses the AABB rule from
// gameplay/Collision.hpp (the only place collision rules live,
// docs/architecture.md §3.5).
//
// SDL-free (dependency rule, docs/architecture.md §1): no SDL, no I/O, no
// rendering. Returns the number of enemies destroyed this step.
int resolvePlayerBullets(ProjectileManager& projectiles,
                         EnemyFormation& formation,
                         ScoreManager& score,
                         EffectManager& effects);

// Resolves enemy threats against the player for one fixed simulation step
// (Stage 15, docs/game_spec.md §5):
//   - an Enemy-owned bullet whose box intersects the player box, or
//   - a living enemy (state-aware bounds, so mid-dive bodies count) whose
//     box intersects the player box.
// Exactly ONE threat takes effect per step (docs/test_plan.md Stage 15:
// two collisions in the same frame remove exactly one life): the offending
// bullet is consumed, Player::hit() applies once, and every other threat
// simply passes through this step.
//
// Ownership rules (spec §8): Player-owned bullets are NEVER checked
// against the player, enemy bullets are NEVER checked against enemies.
// An invulnerable, dying or gone player ignores everything — bullets fly
// through untouched (the invulnerability window is not consumed).
// Note: the colliding enemy body itself is NOT destroyed here; only the
// player is affected (the frozen spec fixes the player side only).
//
// Must run after ProjectileManager::update(dt). SDL-free.
// Returns 1 when a hit landed this step, 0 otherwise.
int resolveEnemyThreats(ProjectileManager& projectiles,
                        EnemyFormation& formation,
                        Player& player);

}  // namespace combat
}  // namespace galaxian
