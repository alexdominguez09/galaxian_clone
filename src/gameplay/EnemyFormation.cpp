#include "gameplay/EnemyFormation.hpp"

#include <numbers>

namespace galaxian {

void EnemyFormation::reset()
{
    position_ = kAnchor;
    phase_ = 0.0;
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kColumns; ++col) {
            enemies_[row * kColumns + col] =
                Enemy(typeForRow(row), slotOffset(row, col));
        }
    }
}

double EnemyFormation::speedMultiplier() const
{
    // Spec §6.3 pressure mechanic: the oscillation speeds up linearly as
    // enemies die, hitting exactly kMaxSpeedMultiplier (2.5x) when the
    // formation is empty.
    return 1.0 + (kMaxSpeedMultiplier - 1.0) *
                     (1.0 - static_cast<double>(aliveCount()) /
                                static_cast<double>(kTotal));
}

void EnemyFormation::update(double dt)
{
    if (dt <= 0.0) {
        return;
    }

    // Phase accumulation on the fixed step (docs/architecture.md §3.1):
    // frame-rate independent by construction, deterministic for identical
    // step sequences. The phase wraps modulo 2π so it cannot grow without
    // bound over long sessions; increments are always far below one turn,
    // so a single floor-based wrap is exact.
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    phase_ += (kTwoPi / kOscillationPeriodSeconds) * speedMultiplier() * dt;
    if (phase_ >= kTwoPi) {
        phase_ -= kTwoPi * std::floor(phase_ / kTwoPi);
    }
    if (phase_ >= kTwoPi) {  // defensive: float rounding at the boundary
        phase_ = 0.0;
    }

    // Sinusoidal sway around the anchor (spec §6.3). x is recomputed from
    // the phase every step (not incremented), y never changes.
    position_.x =
        static_cast<float>(static_cast<double>(kAnchor.x) +
                           kOscillationHalfSwing * std::sin(phase_));
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
