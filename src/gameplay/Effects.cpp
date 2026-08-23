#include "Effects.hpp"

#include "core/GameConfig.hpp"

namespace galaxian {

namespace {

// Same 1 ns tolerance as the projectile fire cooldown (Projectile.cpp):
// 0.25 s and the 1/60 s fixed step are not exactly representable in binary
// floating point, so the remaining time after exactly 15 steps (0.25 s of
// simulation) can be off by ~5e-17 s. 1 ns is 6e-5 of one simulation step —
// invisible to gameplay, but it keeps the expiry boundary deterministic
// across platforms and compilers.
constexpr double kEffectEpsilon = 1e-9;

}  // namespace

bool EffectManager::add(Vector2 position, float width, float height,
                        int scoreValue, EffectKind kind)
{
    if (count_ >= kMaxEffects) {
        return false;
    }
    Effect& e = pool_[count_++];
    e.position = position;
    e.width = width;
    e.height = height;
    e.timeRemaining = GameConfig::get().explosionSeconds;
    e.scoreValue = scoreValue;
    e.kind = kind;
    e.active = true;
    return true;
}

void EffectManager::update(double dt)
{
    if (dt <= 0.0) {
        return;
    }

    int i = 0;
    while (i < count_) {
        Effect& e = pool_[i];
        e.timeRemaining -= dt;
        if (e.timeRemaining <= kEffectEpsilon) {
            --count_;
            pool_[i] = pool_[count_];  // swap-remove (self-copy when last)
        } else {
            ++i;
        }
    }
}

void EffectManager::reset()
{
    count_ = 0;
}

}  // namespace galaxian
