#pragma once

#include "core/Constants.hpp"
#include "core/Types.hpp"

namespace galaxian {

// The player lifecycle (docs/game_spec.md §5, docs/architecture.md §3.7):
//
//   Alive --hit--> Dying --1.5 s--> Respawning --confirm--> Invulnerable
//     ^                                                            |
//     +------------------------- 2.0 s ----------------------------+
//
//   A hit with zero lives remaining sends Dying straight to GameOver
//   (terminal until an external new-game reset, Stage 17).
enum class PlayerState {
    Alive,         // controllable, vulnerable
    Dying,         // hit: ship gone, the spec's 1.5 s respawn delay ticks
    Respawning,    // transient: awaiting the caller's clear+confirm (the
                   // caller wipes nearby enemy projectiles first so the
                   // fresh ship never spawns into instant death)
    Invulnerable,  // at the start position, blinking, controllable, immune
    GameOver,      // out of lives; no respawn
};

// The player ship (docs/game_spec.md §5).
//
// SDL-free by design (dependency rule, docs/architecture.md §1): movement
// is driven by a net `direction` in {-1, 0, +1} computed by the caller
// from named Actions, firing is a plain command, damage is a plain
// `hit()` request. All timing runs on the fixed 1/60 s step
// (docs/architecture.md §3.1).
class Player {
public:
    // Spec §5 constants (logical px / seconds).
    static constexpr float kSpeed = 220.0f;   // horizontal speed, px/s
    static constexpr float kWidth = 24.0f;    // collision box width
    static constexpr float kHeight = 16.0f;   // collision box height
    static constexpr Vector2 kStartPosition{224.0f, 528.0f};  // center

    static constexpr int kLives = 3;                          // at start
    static constexpr double kRespawnDelaySeconds = 1.5;       // hit->respawn
    static constexpr double kInvulnerableSeconds = 2.0;       // blink window

    Player() = default;

    // Advances the player by one fixed simulation step. Movement happens
    // only while controllable (Alive or Invulnerable); the lifecycle
    // timers tick in every state that owns one.
    void update(double dt, float direction);

    // Emits a fire event. Only while controllable.
    void fire();

    // Collision bounds: the 24x16 box centred on the sprite (spec §5).
    Rect bounds() const;

    // Center position (spec §5 start: (224, 528)).
    Vector2 position() const { return position_; }

    PlayerState state() const { return state_; }
    int lives() const { return lives_; }

    // Controllable = can move & fire right now (Alive or Invulnerable).
    bool alive() const
    {
        return state_ == PlayerState::Alive ||
               state_ == PlayerState::Invulnerable;
    }
    // Vulnerable = a hit would take a life (plain Alive only; the
    // Invulnerable blink ignores everything).
    bool vulnerable() const { return state_ == PlayerState::Alive; }

    // Applies one hit. Lands only while vulnerable(); returns whether a
    // life was consumed. Multiple same-frame hits are absorbed naturally:
    // after the first, the player is no longer vulnerable.
    bool hit();

    // True once the machine reached Respawning and waits for the caller
    // to clear nearby enemy projectiles and confirm.
    bool awaitingRespawnConfirm() const
    {
        return state_ == PlayerState::Respawning;
    }

    // Caller-confirmed respawn: back to the start position with the full
    // invulnerability window. No-op unless currently Respawning.
    void confirmRespawn();

    // Fresh game (Stage 17 state transitions): full lives, Alive, at the
    // start position, fire count zeroed.
    void resetGame();

    // Seconds left on the current state's timer (diagnostics/tests).
    double stateTimer() const { return stateTimer_; }

    // Number of fire events emitted (for tests and diagnostics).
    int fireCount() const { return fireCount_; }

private:
    // Clamps the centre x so the collision box stays within [0,
    // kLogicalWidth].
    void clampToScreen();
    void move(double dt, float direction);

    Vector2 position_ = kStartPosition;
    PlayerState state_ = PlayerState::Alive;
    int lives_ = kLives;
    double stateTimer_ = 0.0;
    int fireCount_ = 0;
};

}  // namespace galaxian
