#include "Projectile.hpp"

namespace galaxian {

namespace {

// The cooldown is compared with a 1 ns tolerance: 0.35 s and the 1/60 s
// fixed step are not exactly representable in binary floating point, so the
// remaining time after exactly 21 steps (0.35 s of simulation) can be off by
// ~1e-16 s. 1 ns is 6e-5 of one simulation step — invisible to gameplay,
// but it keeps the "fire at exactly the cooldown" boundary deterministic
// across platforms and compilers.
constexpr double kCooldownEpsilon = 1e-9;

}  // namespace

bool ProjectileManager::tryFirePlayer(const Player& player)
{
    if (!player.alive() || !canFirePlayer()) {
        return false;
    }
    // Directly above the player: the bullet's center x matches the player's
    // center, and the bullet's bottom edge touches the player's top edge.
    const Rect pb = player.bounds();
    const Vector2 position{pb.x + pb.width * 0.5f - Projectile::kWidth * 0.5f,
                           pb.top() - Projectile::kHeight};
    if (!spawn(ProjectileOwner::Player, position, Vector2{0.0f, -kPlayerSpeed})) {
        return false;
    }
    // The cooldown starts when a projectile is actually spawned; a rejected
    // shot (max reached, pool full) does not extend it.
    cooldownRemaining_[static_cast<int>(ProjectileOwner::Player)] =
        kFireCooldownSeconds;
    return true;
}

bool ProjectileManager::spawn(ProjectileOwner owner, Vector2 position,
                              Vector2 velocity)
{
    if (count_ >= kMaxProjectiles) {
        return false;
    }
    Projectile& p = pool_[count_++];
    p.position = position;
    p.velocity = velocity;
    p.owner = owner;
    p.active = true;
    return true;
}

void ProjectileManager::update(double dt)
{
    if (dt <= 0.0) {
        return;
    }

    // Advance the per-owner cooldowns.
    for (int i = 0; i < kOwnerCount; ++i) {
        if (cooldownRemaining_[i] > 0.0) {
            cooldownRemaining_[i] -= dt;
            if (cooldownRemaining_[i] < 0.0) {
                cooldownRemaining_[i] = 0.0;
            }
        }
    }

    // Move, then cull. Swap-remove keeps the pool contiguous; it never grows
    // past kMaxProjectiles and allocates nothing (architecture §3.4).
    int i = 0;
    while (i < count_) {
        Projectile& p = pool_[i];
        p.position = p.position + p.velocity * static_cast<float>(dt);

        const Rect b = p.bounds();
        const bool offscreen =
            (p.owner == ProjectileOwner::Player)
                ? b.bottom() < 0.0f
                : b.top() > static_cast<float>(kLogicalHeight);
        if (offscreen) {
            --count_;
            p = pool_[count_];  // swap-remove (self-copy when it was last)
        } else {
            ++i;
        }
    }
}

void ProjectileManager::reset()
{
    count_ = 0;
    for (int i = 0; i < kOwnerCount; ++i) {
        cooldownRemaining_[i] = 0.0;
    }
}

int ProjectileManager::count(ProjectileOwner owner) const
{
    int n = 0;
    for (int i = 0; i < count_; ++i) {
        if (pool_[i].owner == owner) {
            ++n;
        }
    }
    return n;
}

bool ProjectileManager::canFirePlayer() const
{
    return count(ProjectileOwner::Player) < kMaxPlayerProjectiles &&
           cooldownRemaining_[static_cast<int>(ProjectileOwner::Player)] <=
               kCooldownEpsilon;
}

}  // namespace galaxian
