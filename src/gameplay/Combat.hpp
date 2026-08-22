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

}  // namespace combat
}  // namespace galaxian
