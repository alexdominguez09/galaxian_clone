#include "gameplay/Enemy.hpp"

#include <cmath>

namespace galaxian {

namespace {

// The 1 ns tolerance used for the prepare countdown, matching the fire
// cooldown (Projectile.cpp) and the effect lifetime (Effects.cpp): the
// fixed step and kPrepareDurationSeconds are not exactly representable in
// binary floating point.
constexpr double kPrepareEpsilon = 1e-9;

}  // namespace

Enemy::Enemy(EnemyType type, Vector2 slotOffset)
    : type_(type), slotOffset_(slotOffset) {}

const EnemyDefinition& Enemy::definition() const
{
    return kEnemyDefinitions[static_cast<int>(type_)];
}

Vector2 Enemy::screenPosition(Vector2 formationPosition) const
{
    const bool away = state_ == EnemyState::Diving ||
                      state_ == EnemyState::Attacking ||
                      state_ == EnemyState::Returning;
    return away ? divePosition_ : formationPosition + slotOffset_;
}

Rect Enemy::bounds(Vector2 formationPosition) const
{
    const Vector2 position = screenPosition(formationPosition);
    return {position.x, position.y, kWidth, kHeight};
}

bool Enemy::isLegalTransition(EnemyState from, EnemyState to)
{
    if (from == EnemyState::Dead) {
        return false;  // Dead is terminal (spec §6.4)
    }
    if (to == EnemyState::Dead) {
        return true;   // any living state -> Dead
    }
    // Exactly the chain edges of spec §6.4; skips and self-transitions are
    // illegal.
    switch (from) {
        case EnemyState::Formation:     return to == EnemyState::PreparingDive;
        case EnemyState::PreparingDive: return to == EnemyState::Diving;
        case EnemyState::Diving:        return to == EnemyState::Attacking;
        case EnemyState::Attacking:     return to == EnemyState::Returning;
        case EnemyState::Returning:     return to == EnemyState::Formation;
        default:                        return false;
    }
}

bool Enemy::transitionTo(EnemyState next)
{
    if (!isLegalTransition(state_, next)) {
        return false;
    }
    if (next == EnemyState::PreparingDive) {
        prepareRemaining_ = kPrepareDurationSeconds;
    }
    state_ = next;
    return true;
}

void Enemy::kill()
{
    // Always legal from a living state; a second kill is rejected and
    // leaves Dead unchanged.
    transitionTo(EnemyState::Dead);
}

bool Enemy::beginDive()
{
    return transitionTo(EnemyState::PreparingDive);
}

void Enemy::update(double dt, Vector2 formationPosition)
{
    if (!alive() || dt <= 0.0) {
        return;
    }

    // All motion is `position += velocity * dt` on the fixed step
    // (docs/architecture.md §3.1); internal transitions use exactly the
    // legal edges of isLegalTransition.
    switch (state_) {
        case EnemyState::Formation:
        case EnemyState::Dead:
            return;

        case EnemyState::PreparingDive:
            // Hold at the slot for the peel-off pause, then freeze the
            // dive start at wherever the slot currently sits.
            prepareRemaining_ -= dt;
            if (prepareRemaining_ <= kPrepareEpsilon) {
                state_ = EnemyState::Diving;
                divePosition_ = formationPosition + slotOffset_;
            }
            return;

        case EnemyState::Diving:
            // Simple path (Stage 11): straight down until the box's bottom
            // reaches the turn point.
            divePosition_.y +=
                definition().speed * static_cast<float>(dt);
            if (divePosition_.y + kHeight >= kTurnPointY) {
                state_ = EnemyState::Attacking;
                // Dash towards the nearer vertical edge.
                attackDirection_ =
                    (divePosition_.x + kWidth * 0.5f <=
                     static_cast<float>(kLogicalWidth) * 0.5f)
                        ? -1.0f
                        : +1.0f;
            }
            return;

        case EnemyState::Attacking: {
            divePosition_.x +=
                attackDirection_ * definition().speed * static_cast<float>(dt);
            // Off-screen means FULLY off-screen (strict, mirroring the
            // projectile culling convention).
            const bool offLeft =
                divePosition_.x + kWidth <= 0.0f;
            const bool offRight =
                divePosition_.x >= static_cast<float>(kLogicalWidth);
            if (offLeft || offRight) {
                state_ = EnemyState::Returning;
            }
            return;
        }

        case EnemyState::Returning: {
            // Home to the LIVE slot position: recomputed every step, so a
            // swaying formation is tracked (spec §6.2 indirection).
            const Vector2 target = formationPosition + slotOffset_;
            const Vector2 delta = target - divePosition_;
            const float dist =
                std::sqrt(delta.x * delta.x + delta.y * delta.y);
            const float travel =
                definition().speed * static_cast<float>(dt);
            if (dist <= travel) {
                divePosition_ = target;  // snap exactly onto the slot
                state_ = EnemyState::Formation;
            } else {
                divePosition_ = divePosition_ +
                                delta * (travel / dist);
            }
            return;
        }
    }
}

}  // namespace galaxian
