#include "Player.hpp"

namespace galaxian {

namespace {

// Same 1 ns tolerance as the other simulation timers (fire cooldown,
// effect lifetime, prepare countdown): the fixed step and the lifecycle
// durations are not exactly representable in binary floating point.
constexpr double kTimeEpsilon = 1e-9;

}  // namespace

void Player::update(double dt, float direction)
{
    if (dt <= 0.0) {
        return;
    }
    switch (state_) {
        case PlayerState::Alive:
            move(dt, direction);
            return;

        case PlayerState::Invulnerable:
            // Controllable while blinking; the window counts down to a
            // plain vulnerable Alive.
            move(dt, direction);
            stateTimer_ -= dt;
            if (stateTimer_ <= kTimeEpsilon) {
                stateTimer_ = 0.0;
                state_ = PlayerState::Alive;
            }
            return;

        case PlayerState::Dying:
            // Spec §5: respawn after 1.5 s — or game over with no lives
            // left. The handoff itself (projectile clearing + placement)
            // is the caller's confirmRespawn().
            stateTimer_ -= dt;
            if (stateTimer_ <= kTimeEpsilon) {
                state_ = (lives_ > 0) ? PlayerState::Respawning
                                      : PlayerState::GameOver;
            }
            return;

        case PlayerState::Respawning:
        case PlayerState::GameOver:
            return;
    }
}

void Player::move(double dt, float direction)
{
    if (direction == 0.0f) {
        return;
    }
    position_.x += direction * kSpeed * static_cast<float>(dt);
    clampToScreen();
}

void Player::fire()
{
    if (!alive()) {
        return;
    }
    ++fireCount_;
}

bool Player::hit()
{
    if (!vulnerable()) {
        return false;
    }
    --lives_;
    state_ = PlayerState::Dying;
    stateTimer_ = kRespawnDelaySeconds;
    return true;
}

void Player::confirmRespawn()
{
    if (state_ != PlayerState::Respawning) {
        return;
    }
    position_ = kStartPosition;
    state_ = PlayerState::Invulnerable;
    stateTimer_ = kInvulnerableSeconds;
}

Rect Player::bounds() const
{
    return {position_.x - kWidth * 0.5f,
            position_.y - kHeight * 0.5f,
            kWidth,
            kHeight};
}

void Player::clampToScreen()
{
    // The collision box (kWidth wide, centred on position_) must stay
    // inside the screen: centre x in [kWidth/2, kLogicalWidth - kWidth/2].
    const float halfWidth = kWidth * 0.5f;
    const float minX = halfWidth;
    const float maxX = static_cast<float>(kLogicalWidth) - halfWidth;
    if (position_.x < minX) {
        position_.x = minX;
    }
    if (position_.x > maxX) {
        position_.x = maxX;
    }
}

}  // namespace galaxian
