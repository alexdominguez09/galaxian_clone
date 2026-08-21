#pragma once

#include "core/Constants.hpp"
#include "core/Types.hpp"

namespace galaxian {

// Player state (docs/game_spec.md §5, docs/architecture.md §3.7).
//
// Stage 5 implements the minimal alive/dead pair; Stage 15 expands this to
// the full Alive/Dying/Respawning/Invulnerable/GameOver machine with lives,
// a respawn timer, and invulnerability.
enum class PlayerState {
    Alive,
    Dead,
};

// The player ship (docs/game_spec.md §5).
//
// Stage 5 prototype: horizontal-only, velocity-based movement clamped to
// the screen, a fire command (emits a fire event; no projectile yet —
// Stage 6), and an alive/dead state.
//
// SDL-free by design (dependency rule, docs/architecture.md §1): movement
// is driven by a `direction` in {-1, 0, +1} that the caller computes from
// the InputManager's named Actions (left/right cancel to 0), and firing is
// a plain command. The Player never sees SDL keys or the InputManager.
class Player {
public:
    // Spec §5 constants (logical px).
    static constexpr float kSpeed = 220.0f;   // horizontal speed, px/s
    static constexpr float kWidth = 24.0f;    // collision box width
    static constexpr float kHeight = 16.0f;   // collision box height
    static constexpr Vector2 kStartPosition{224.0f, 528.0f};  // center

    Player() = default;

    // Advances the player by one fixed simulation step (docs/architecture.md
    // §3.1: all motion is `position += velocity * dt` with the fixed dt).
    //
    // `direction` is the net horizontal direction in {-1, 0, +1}: -1 for
    // left, +1 for right, 0 for none or for the spec'd simultaneous
    // left+right cancel (docs/game_spec.md §4). Horizontal only: y is never
    // changed. The position is clamped so the collision box never leaves
    // the screen. A dead player does not move.
    void update(double dt, float direction);

    // Emits a fire event (Stage 5: the event is the fire count; the caller
    // logs it). No projectile is spawned yet (Stage 6). A dead player
    // cannot fire.
    void fire();

    // Collision bounds: the 24x16 box centered on the sprite (spec §5).
    // For the dev art this coincides with the sprite itself.
    Rect bounds() const;

    // Center position (spec §5 start: (224, 528)).
    Vector2 position() const { return position_; }

    PlayerState state() const { return state_; }
    bool alive() const { return state_ == PlayerState::Alive; }

    // Stage 5 alive/dead transitions. Stage 15 replaces these with the full
    // death/respawn lifecycle (lives, respawn timer, invulnerability).
    void kill();
    // Returns the player to the start position and the Alive state.
    void respawn();

    // Number of fire events emitted (for tests and diagnostics).
    int fireCount() const { return fireCount_; }

private:
    // Clamps the center x so the collision box stays within [0, kLogicalWidth].
    void clampToScreen();

    Vector2 position_ = kStartPosition;
    PlayerState state_ = PlayerState::Alive;
    int fireCount_ = 0;
};

}  // namespace galaxian
