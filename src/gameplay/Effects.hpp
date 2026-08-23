#pragma once

#include "core/Types.hpp"

namespace galaxian {

// Which art the renderer plays for this effect (Stage 24): enemy kills
// burst as a starburst, the player's death erupts as the arcade fountain
// (see the reference screenshot). Pure presentation data — gameplay never
// branches on it.
enum class EffectKind {
    Enemy,
    Player,
};

// A destruction effect (Stage 9 placeholder; animated since Stage 19/24):
// a fixed-size box at a screen position that stays visible for a fixed
// duration, then disappears.
//
// `position` is the box's top-left corner (the Rect convention,
// docs/game_spec.md §3). The renderer draws the explosion sprite CENTRED
// on this box (the 32x32 blast art is bigger than the box on purpose).
struct Effect {
    Vector2 position{0.0f, 0.0f};
    float width = 0.0f;
    float height = 0.0f;
    double timeRemaining = 0.0;
    bool active = false;
    // Stage 24: points carried by the kill that spawned this effect; the
    // renderer draws a rising "+N" score popup while the effect lives.
    int scoreValue = 0;
    // Stage 24: enemy starburst vs player fountain.
    EffectKind kind = EffectKind::Enemy;

    Rect bounds() const { return {position.x, position.y, width, height}; }
};

// The placeholder effect system (Stage 9, docs/architecture.md §3.8).
//
// SDL-free (dependency rule, docs/architecture.md §1): pure logic driven by
// the fixed 1/60 s step (docs/architecture.md §3.1).
//
// Storage is a fixed pool with swap-remove (like the projectile pool,
// docs/architecture.md §3.4): zero heap allocation in steady state; a full
// pool fails an add gracefully (returns false) instead of growing.
class EffectManager {
public:
    // How long a destruction placeholder stays visible (simulation seconds).
    static constexpr double kDurationSeconds = 0.25;
    // Pool capacity.
    static constexpr int kMaxEffects = 16;

    // Adds a kDurationSeconds effect with top-left `position` and the given
    // size. Returns false when the pool is full (no effect is added).
    bool add(Vector2 position, float width, float height,
             int scoreValue = 0, EffectKind kind = EffectKind::Enemy);

    // Advances all active effects by one fixed simulation step and removes
    // the ones that have expired.
    void update(double dt);

    // Total active effects.
    int count() const { return count_; }
    // Active effect `index` (0..count()-1).
    const Effect& effect(int index) const { return pool_[index]; }

    // Removes all effects (state transitions in Stage 17, tests).
    void reset();

private:
    Effect pool_[kMaxEffects] = {};
    int count_ = 0;
};

}  // namespace galaxian
