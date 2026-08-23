#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"
#include "DevArt.hpp"

namespace galaxian {
namespace animation {

// An immutable frame-sequence definition (Stage 19, docs/architecture.md):
// a list of texture ids (static lifetime — DevArt's constexpr ids), the
// per-frame duration and whether the clip loops.
//
// Pure data: gameplay logic SELECTS a clip ("state = Diving" ->
// "animation = EnemyDive"), but the clip never drives physics — advancing
// it touches nothing but the animator's own clock.
class AnimationClip {
public:
    AnimationClip() = default;
    constexpr AnimationClip(const char* const* textureIds, int count,
                            double frameSeconds, bool loop)
        : textureIds_(textureIds), count_(count),
          frameSeconds_(frameSeconds), loop_(loop) {}

    int frameCount() const { return count_; }
    const char* textureId(int index) const { return textureIds_[index]; }
    double frameSeconds() const { return frameSeconds_; }
    bool loops() const { return loop_; }
    // Total playback time of one pass (frames * frameSeconds).
    double duration() const { return frameSeconds_ * count_; }

private:
    const char* const* textureIds_ = nullptr;
    int count_ = 0;
    double frameSeconds_ = 0.0;
    bool loop_ = false;
};

// Plays ONE clip at a time: a private clock advanced by the fixed
// simulation step, mapped to a frame index. SDL-free logic; drawing is a
// thin convenience on top.
//
// Timing rules (docs/test_plan.md Stage 19):
//   * frames advanced = elapsed sim time / frameSeconds (exact float math
//     for dyadic durations; the codebase-wide determinism applies),
//   * looping clips wrap modulo the clip duration (the clock stays
//     bounded — long sessions cannot degrade it),
//   * one-shot clips CLAMP to the last frame and report finished().
class Animator {
public:
    // Starts (or restarts) the clip from its first frame.
    void play(const AnimationClip& clip);

    // Advances the internal clock. dt <= 0 is a no-op.
    void update(double dt);

    bool valid() const { return clip_ != nullptr; }

    // Current frame index, always in [0, frameCount()-1] (looped clips
    // wrap; one-shot clips clamp to the last frame).
    int frame() const;

    // One-shot clips only: true once the end was reached (and kept).
    bool finished() const;

    double time() const { return time_; }

    // Draws the current frame's sprite with its top-left at `position`.
    // Safe no-op without a clip.
    void draw(Renderer& renderer, Vector2 position) const;

private:
    const AnimationClip* clip_ = nullptr;
    double time_ = 0.0;
};

// ---- The production clip set (Stage 19) ----
//
// Gameplay logic only ever REFERENCES these (state selects clip); the
// clips never drive physics. Durations: idle flickers are deliberately
// slow and readable; the explosion totals exactly the Stage 9 gameplay
// effect duration (4 x 0.0625 s = 0.25 s), so the placeholder white box
// is replaced one-to-one by the animated one-shot.
inline constexpr const char* kPlayerIdleIds[] = {DevArt::kPlayerIdleA,
                                                 DevArt::kPlayerIdleB};
inline constexpr AnimationClip kPlayerIdleClip{kPlayerIdleIds, 2, 0.20, true};

inline constexpr const char* kScoutIdleIds[] = {DevArt::kEnemyScoutA,
                                                DevArt::kEnemyScoutB};
inline constexpr AnimationClip kScoutIdleClip{kScoutIdleIds, 2, 0.30, true};
inline constexpr const char* kGuardIdleIds[] = {DevArt::kEnemyGuardA,
                                                DevArt::kEnemyGuardB};
inline constexpr AnimationClip kGuardIdleClip{kGuardIdleIds, 2, 0.30, true};
inline constexpr const char* kCommanderIdleIds[] = {DevArt::kEnemyCommanderA,
                                                    DevArt::kEnemyCommanderB};
inline constexpr AnimationClip kCommanderIdleClip{kCommanderIdleIds, 2, 0.30,
                                                  true};

inline constexpr const char* kExplosionIds[] = {DevArt::kExplosionA,
                                                DevArt::kExplosionB,
                                                DevArt::kExplosionC,
                                                DevArt::kExplosionD};
inline constexpr AnimationClip kExplosionClip{kExplosionIds, 4, 0.0625,
                                              false};

// The PLAYER death burst (Stage 24): same one-shot timing as the enemy
// explosion so Game can progress-map the effect's remaining time onto
// frames identically.
inline constexpr const char* kPlayerExplosionIds[] = {
    DevArt::kPlayerExplosionA, DevArt::kPlayerExplosionB,
    DevArt::kPlayerExplosionC, DevArt::kPlayerExplosionD};
inline constexpr AnimationClip kPlayerExplosionClip{kPlayerExplosionIds, 4,
                                                    0.0625, false};

}  // namespace animation
}  // namespace galaxian
