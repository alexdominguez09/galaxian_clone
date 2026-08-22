#pragma once

#include "core/Constants.hpp"
#include "core/Types.hpp"

#include "gameplay/Player.hpp"

namespace galaxian {

// Which side owns a projectile (docs/game_spec.md §8). Player bullets travel
// up, enemy bullets travel down. Ownership is enforced from Stage 7 on:
// player bullets never damage the player, enemy bullets never damage
// enemies. Stage 6 spawns player bullets only; enemy fire lands in Stage 14
// on this same system.
enum class ProjectileOwner {
    Player,
    Enemy,
};

// A single projectile (docs/game_spec.md §8, docs/architecture.md §3.4).
//
// `position` is the TOP-LEFT corner of the projectile box (the Rect
// convention, docs/game_spec.md §3); `bounds()` is the box itself. The dev-
// art bullet is a 4x10 rectangle, so the box coincides with the sprite and
// `position` is also the draw position.
struct Projectile {
    // Dev-art bullet size (logical px); the final art keeps the same box
    // (Stage 24).
    static constexpr float kWidth = 4.0f;
    static constexpr float kHeight = 10.0f;

    Vector2 position{0.0f, 0.0f};
    Vector2 velocity{0.0f, 0.0f};
    ProjectileOwner owner = ProjectileOwner::Player;
    bool active = false;

    // The projectile box (top-left + size).
    Rect bounds() const { return {position.x, position.y, kWidth, kHeight}; }
};

// The shared projectile system for both owners (docs/game_spec.md §8):
// spawn, movement, off-screen removal, and the player's fire cooldown /
// max-simultaneous-shots rules (docs/game_spec.md §5).
//
// SDL-free (dependency rule, docs/architecture.md §1): all motion is
// `position += velocity * dt` on the fixed 1/60 s step (architecture §3.1),
// so it is frame-rate independent and unit-testable headlessly.
//
// Storage is a fixed-size pool with swap-remove (architecture §3.4): zero
// heap allocations in steady state, so the projectile lifecycle cannot leak
// and the active count is bounded by construction.
class ProjectileManager {
public:
    // docs/game_spec.md §5/§8 constants.
    static constexpr float kPlayerSpeed = 480.0f;  // px/s, upward
    static constexpr double kFireCooldownSeconds = 0.35;
    static constexpr int kMaxPlayerProjectiles = 2;
    // Enemy bullet speed (spec §8): 240 px/s base; wave-dependent scaling
    // (bounded <= 360) lands with the Stage 16 wave system.
    static constexpr double kEnemySpeed = 240.0;
    // Pool capacity: the player max (2) plus headroom for the enemy fire
    // that lands in Stage 14. A spawn into a full pool fails gracefully
    // (returns false) instead of growing.
    static constexpr int kMaxProjectiles = 16;

    // Player fire (docs/game_spec.md §5): spawns a bullet directly above the
    // player, moving up at kPlayerSpeed. Returns false — spawning no
    // projectile — when the player is dead, the fire cooldown has not
    // elapsed, the max simultaneous player projectiles is reached, or the
    // pool is full.
    bool tryFirePlayer(const Player& player);

    // Enemy fire (docs/game_spec.md §8, Stage 14): spawns a bullet whose
    // CENTER starts at `muzzle` and whose velocity points from `muzzle`
    // towards `aimAt` (the player's position AT FIRE TIME) with magnitude
    // `speed`. A degenerate aim (target == muzzle) falls back straight
    // down. Returns false when the pool is full. No cooldown: the firing
    // cadence is owned by the diver's per-dive shot budget.
    bool tryFireEnemy(Vector2 muzzle, Vector2 aimAt,
                      double speed = kEnemySpeed);

    // Generic spawn (used by tests now and by enemy fire in Stage 14).
    // `position` is the box's top-left corner. Returns false when the pool
    // is full.
    bool spawn(ProjectileOwner owner, Vector2 position, Vector2 velocity);

    // Advances all live projectiles by one fixed simulation step and removes
    // the ones that are fully off-screen: a player bullet once its box is
    // above the top edge (position.y < -kHeight), an enemy bullet once its
    // box is below the bottom edge (position.y > kLogicalHeight). A
    // partially visible box is never culled.
    void update(double dt);

    // Removes all projectiles and resets the per-owner cooldowns (state
    // transitions in Stage 17, tests).
    void reset();

    // Removes the live projectile at pool `index` (swap-remove, the same
    // scheme update() uses for culling). Returns false when `index` is out
    // of range. Stage 9 uses this to consume a bullet that hit an enemy;
    // it does not touch the per-owner fire cooldowns.
    bool removeAt(int index);

    // Total live projectiles.
    int count() const { return count_; }
    // Live projectiles owned by `owner`.
    int count(ProjectileOwner owner) const;
    // Live projectile `index` (0..count()-1).
    const Projectile& projectile(int index) const { return pool_[index]; }

    // True when a player shot would be accepted right now (cooldown elapsed
    // AND below the max simultaneous player projectiles). Diagnostics/tests.
    bool canFirePlayer() const;

private:
    // Number of ProjectileOwner values (Player, Enemy).
    static constexpr int kOwnerCount = 2;

    // Per-owner fire cooldown, in seconds of simulation time. Only the owner
    // that just spawned has a nonzero value; enemy fire (Stage 14) reuses
    // the same mechanism.
    double cooldownRemaining_[kOwnerCount] = {0.0, 0.0};

    Projectile pool_[kMaxProjectiles] = {};
    int count_ = 0;
};

}  // namespace galaxian
