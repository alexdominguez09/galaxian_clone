#include "gameplay/EnemyFormation.hpp"

namespace galaxian {

void EnemyFormation::reset()
{
    position_ = kAnchor;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kColumns; ++col) {
            enemies_[row * kColumns + col] =
                Enemy(typeForRow(row), slotOffset(row, col));
        }
    }
}

int EnemyFormation::aliveCount() const
{
    int alive = 0;
    for (const Enemy& enemy : enemies_) {
        if (enemy.alive()) {
            ++alive;
        }
    }
    return alive;
}

Enemy& EnemyFormation::at(int row, int col)
{
    return enemies_[row * kColumns + col];
}

const Enemy& EnemyFormation::at(int row, int col) const
{
    return enemies_[row * kColumns + col];
}

Enemy& EnemyFormation::at(int index)
{
    return enemies_[index];
}

const Enemy& EnemyFormation::at(int index) const
{
    return enemies_[index];
}

EnemyType EnemyFormation::typeForRow(int row)
{
    // Spec §6.2: row 0 Commander, rows 1-2 Guard, rows 3-4 Scout.
    switch (row) {
        case 0:
            return EnemyType::Commander;
        case 1:
        case 2:
            return EnemyType::Guard;
        default:
            return EnemyType::Scout;
    }
}

Vector2 EnemyFormation::slotOffset(int row, int col)
{
    return {col * kColumnSpacing, row * kRowSpacing};
}

void EnemyFormation::setPosition(Vector2 position)
{
    position_ = position;
}

Vector2 EnemyFormation::positionOf(int row, int col) const
{
    return position_ + slotOffset(row, col);
}

Rect EnemyFormation::boundsOf(int row, int col) const
{
    const Vector2 position = positionOf(row, col);
    return {position.x, position.y, Enemy::kWidth, Enemy::kHeight};
}

}  // namespace galaxian
