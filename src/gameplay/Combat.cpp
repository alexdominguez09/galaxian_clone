#include "Combat.hpp"

#include "gameplay/Collision.hpp"

namespace galaxian {
namespace combat {

int resolvePlayerBullets(ProjectileManager& projectiles,
                         EnemyFormation& formation,
                         ScoreManager& score,
                         EffectManager& effects)
{
    int kills = 0;
    const Vector2 formationPosition = formation.position();

    int i = 0;
    while (i < projectiles.count()) {
        const Projectile& p = projectiles.projectile(i);
        if (p.owner == ProjectileOwner::Player) {
            // First living enemy (row-major) whose box intersects the
            // bullet box. Dead enemies are skipped entirely, so a bullet
            // flies through their holes (spec §6.3).
            const Rect bulletBox = p.bounds();
            int hitRow = -1;
            int hitCol = -1;
            for (int row = 0; row < EnemyFormation::kRows && hitRow < 0;
                 ++row) {
                for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                    const Enemy& enemy = formation.at(row, col);
                    if (enemy.alive() &&
                        intersects(bulletBox,
                                   enemy.bounds(formationPosition))) {
                        hitRow = row;
                        hitCol = col;
                        break;
                    }
                }
            }
            if (hitRow >= 0) {
                Enemy& enemy = formation.at(hitRow, hitCol);
                enemy.kill();
                score.addKill(enemy.type());
                // Placeholder destruction effect at the enemy's box
                // (top-left = world position + slot offset, spec §6.2).
                effects.add(formation.positionOf(hitRow, hitCol),
                            Enemy::kWidth, Enemy::kHeight);
                // The bullet is consumed on the first hit: it can never
                // continue on to destroy a second enemy.
                projectiles.removeAt(i);
                ++kills;
                // Swap-remove put the last bullet into slot i; check it
                // again (it may kill another enemy in the same step).
                continue;
            }
        }
        ++i;
    }
    return kills;
}

}  // namespace combat
}  // namespace galaxian
