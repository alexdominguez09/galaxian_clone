#include "gameplay/Enemy.hpp"

namespace galaxian {

Enemy::Enemy(EnemyType type, Vector2 slotOffset)
    : type_(type), slotOffset_(slotOffset) {}

const EnemyDefinition& Enemy::definition() const
{
    return kEnemyDefinitions[static_cast<int>(type_)];
}

Rect Enemy::bounds(Vector2 formationPosition) const
{
    const Vector2 position = formationPosition + slotOffset_;
    return {position.x, position.y, kWidth, kHeight};
}

void Enemy::kill()
{
    state_ = EnemyState::Dead;
}

}  // namespace galaxian
