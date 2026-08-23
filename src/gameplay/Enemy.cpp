#include "gameplay/Enemy.hpp"

#include <cmath>

#include "core/GameConfig.hpp"

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

EnemyDefinition Enemy::definition() const
{
    // Points + dive speed are BALANCE data (Stage 21 GameConfig); the
    // sprite index is the structural §6.1 mapping.
    const GameConfig& cfg = GameConfig::get();
    EnemyDefinition def = kEnemyDefinitions[static_cast<int>(type_)];
    switch (type_) {
        case EnemyType::Scout:
            def.points = cfg.scoutPoints;
            def.speed = cfg.scoutDiveSpeed;
            break;
        case EnemyType::Guard:
            def.points = cfg.guardPoints;
            def.speed = cfg.guardDiveSpeed;
            break;
        case EnemyType::Commander:
            def.points = cfg.commanderPoints;
            def.speed = cfg.commanderDiveSpeed;
            break;
    }
    return def;
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

bool Enemy::beginDive(DivePattern pattern, int shotsPerDive)
{
    if (!transitionTo(EnemyState::PreparingDive)) {
        return false;
    }
    divePattern_ = pattern;
    // Spec §6.4: 1-2 shots during the attack phase; clamp defensively.
    shotsTotal_ = (shotsPerDive < 0) ? 0
                  : (shotsPerDive > 2 ? 2 : shotsPerDive);
    shotsFired_ = 0;
    pendingShots_ = 0;
    return true;
}

int Enemy::drainPendingShots()
{
    const int pending = pendingShots_;
    pendingShots_ = 0;
    return pending;
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
            // Hold at the slot for the peel-off pause, then start the
            // selected attack pattern from wherever the slot currently is.
            prepareRemaining_ -= dt;
            if (prepareRemaining_ <= kPrepareEpsilon) {
                state_ = EnemyState::Diving;
                divePosition_ = formationPosition + slotOffset_;
                path_ = DivePath::make(divePattern_, divePosition_);
                follower_.begin();
            }
            return;

        case EnemyState::Diving:
            // Stage 12: follow the attack pattern's cubic Bézier at
            // definition().speed along its arc length; finishing the path
            // (t = 1) is the turn point.
            follower_.advance(path_, definition().speed *
                                         static_cast<float>(dt));
            divePosition_ = path_.curve().evaluate(follower_.t());

            // Stage 14: fire events at deterministic parametric trigger
            // points (midpoint for one-shot dives, quartiles 0.35/0.75 for
            // two-shot dives).
            while (shotsFired_ < shotsTotal_) {
                const float trigger =
                    (shotsTotal_ == 1)
                        ? kSingleShotTrigger
                        : kDoubleShotTriggers[shotsFired_];
                if (follower_.t() < trigger) {
                    break;
                }
                ++shotsFired_;
                ++pendingShots_;
            }

            if (follower_.finished()) {
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
                // Stage 12: build the ReturnPath from here back to the
                // slot as it sits RIGHT NOW; every following step
                // re-targets its end at the live slot, so a swaying
                // formation is tracked and P(1) is always exactly the slot.
                state_ = EnemyState::Returning;
                path_ = DivePath::make(DivePattern::ReturnPath, divePosition_,
                                       formationPosition + slotOffset_);
                follower_.begin();
            }
            return;
        }

        case EnemyState::Returning: {
            // Follow the return arc with its end re-targeted at the LIVE
            // slot position each step (spec §6.2 indirection). The pacing
            // uses the arc length captured at entry — the shape barely
            // changes as the formation sways.
            const Vector2 target = formationPosition + slotOffset_;
            const CubicBezier live = path_.endCurve(target);
            follower_.advance(path_, definition().speed *
                                         static_cast<float>(dt));
            divePosition_ = live.evaluate(follower_.t());
            if (follower_.finished()) {
                // P(1) == target bit-exactly; assign explicitly so the
                // snap is self-evident.
                divePosition_ = target;
                state_ = EnemyState::Formation;
            }
            return;
        }
    }
}

}  // namespace galaxian
