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
            Rect hitBox;
            for (int row = 0; row < EnemyFormation::kRows && hitRow < 0;
                 ++row) {
                for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                    const Enemy& enemy = formation.at(row, col);
                    if (!enemy.alive()) {
                        continue;
                    }
                    // State-aware bounds: a diving enemy is tested at its
                    // live dive position, not at its empty slot.
                    const Rect box = enemy.bounds(formationPosition);
                    if (intersects(bulletBox, box)) {
                        hitRow = row;
                        hitCol = col;
                        hitBox = box;
                        break;
                    }
                }
            }
            if (hitRow >= 0) {
                Enemy& enemy = formation.at(hitRow, hitCol);
                enemy.kill();
                const int awarded = score.addKill(enemy.type());
                // Placeholder destruction effect at where the enemy ACTUALLY
                // was (captured before the kill; a diver's true position,
                // not its empty slot). The awarded points ride along for
                // the Stage 24 score popup.
                effects.add(hitBox.position(), Enemy::kWidth, Enemy::kHeight,
                            awarded);
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

int resolveEnemyThreats(ProjectileManager& projectiles,
                        EnemyFormation& formation,
                        Player& player)
{
    if (!player.vulnerable()) {
        return 0;  // invulnerable blink, dying or gone: nothing can land
    }
    const Rect playerBox = player.bounds();

    // 1) Enemy bullets (spec §5: enemy projectile collision = death).
    int i = 0;
    while (i < projectiles.count()) {
        const Projectile& p = projectiles.projectile(i);
        if (p.owner == ProjectileOwner::Enemy &&
            intersects(p.bounds(), playerBox)) {
            projectiles.removeAt(i);  // the bullet is consumed by the hit
            player.hit();             // exactly one life per step
            return 1;
        }
        ++i;
    }

    // 2) Enemy bodies — state-aware bounds, so a mid-dive body counts.
    const Vector2 fp = formation.position();
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const Enemy& enemy = formation.at(row, col);
            if (!enemy.alive()) {
                continue;
            }
            if (intersects(enemy.bounds(fp), playerBox)) {
                player.hit();
                return 1;
            }
        }
    }
    return 0;
}

}  // namespace combat
}  // namespace galaxian
