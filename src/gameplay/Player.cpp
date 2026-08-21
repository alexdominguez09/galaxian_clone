#include "Player.hpp"

namespace galaxian {

void Player::update(double dt, float direction)
{
    if (!alive()) {
        return;  // A dead player does not move (Stage 15 adds respawn).
    }
    if (direction == 0.0f || dt <= 0.0) {
        return;
    }
    // Fixed-timestep motion (docs/architecture.md §3.1): the render frame
    // rate never enters this calculation.
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

Rect Player::bounds() const
{
    return {position_.x - kWidth * 0.5f,
            position_.y - kHeight * 0.5f,
            kWidth,
            kHeight};
}

void Player::kill()
{
    state_ = PlayerState::Dead;
}

void Player::respawn()
{
    position_ = kStartPosition;
    state_ = PlayerState::Alive;
}

void Player::clampToScreen()
{
    // The collision box (kWidth wide, centered on position_) must stay
    // inside the screen: center x in [kWidth/2, kLogicalWidth - kWidth/2].
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
