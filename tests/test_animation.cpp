// Stage 19 animation system tests (docs/test_plan.md, Stage 19).
//
// Pure logic: AnimationClip/Animator are SDL-free (the draw() convenience
// is exercised in test_rendering.cpp). No window, no SDL init here.
//
// Exact-value notes: the test clips use DYADIC frame durations (0.125 s,
// 0.0625 s) so every boundary is exact in binary floating point:
//   * a 4-frame 0.125 s clip loops after exactly 0.5 s (4 frames),
//   * the production explosion clip (4 x 0.0625) totals exactly 0.25 s —
//     the same duration as the gameplay Effect, which is what lets Game
//     progress-map effect time onto explosion frames one-to-one.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

#include "gameplay/EnemyFormation.hpp"
#include "graphics/Animation.hpp"

using namespace galaxian;
using namespace galaxian::animation;

namespace {

constexpr double kDt = 1.0 / 60.0;

// Two-frame test clip at 8 fps (dyadic 0.125 s frames).
const char* const kTwoIds[] = {"a", "b"};
constexpr AnimationClip kTwoFrame{kTwoIds, 2, 0.125, true};

// The production explosion clip (one-shot, dyadic 0.0625 s frames).
constexpr AnimationClip kBoom = kExplosionClip;

}  // namespace

TEST_CASE("animation: frames advance at exactly sim time x fps",
          "[animation]")
{
    Animator animator;
    CHECK_FALSE(animator.valid());
    CHECK(animator.frame() == 0);

    animator.play(kTwoFrame);
    CHECK(animator.valid());
    CHECK(animator.frame() == 0);
    CHECK_FALSE(animator.finished());

    // Dyadic boundaries are exact: each 0.125 s flips the frame.
    animator.update(0.1249);
    CHECK(animator.frame() == 0);
    animator.update(0.0001 + 1e-12);  // cross 0.125
    CHECK(animator.time() == Catch::Approx(0.125).margin(1e-12));
    CHECK(animator.frame() == 1);

    // Half a second of playback advances 4 frames (fps x time): wrapped
    // back to frame 0. The steps are dyadic (1/32 s) so the accumulated
    // clock hits the 0.125 s boundaries EXACTLY -- non-dyadic steps drift
    // a few ulps below/above and would flip either way.
    Animator looped;
    looped.play(kTwoFrame);
    for (int i = 0; i < 16; ++i) {  // 16 x 0.03125 = exactly 0.5 s
        looped.update(0.03125);
    }
    CHECK(looped.frame() == 0);     // 4 frames advanced -> wrapped (index 4)
    // The clock wrapped exactly onto the clip start (fmod of an exact
    // multiple): bounded forever.
    CHECK(looped.time() == 0.0);

    // An odd frame is reachable with a matching offset (first section
    // covers index 1 directly).
    Animator odd;
    odd.play(kTwoFrame);
    odd.update(0.125);              // exactly one frame duration
    CHECK(odd.frame() == 1);
}

TEST_CASE("animation: looping clips stay bounded forever", "[animation]")
{
    Animator animator;
    animator.play(kTwoFrame);

    for (int i = 0; i < 10000; ++i) {
        animator.update(0.03125);  // quarter-frame chunks
        REQUIRE(animator.frame() >= 0);
        REQUIRE(animator.frame() < kTwoFrame.frameCount());
        // The clock wraps modulo the clip duration.
        REQUIRE(animator.time() < kTwoFrame.duration() + 1e-9);
    }
}

TEST_CASE("animation: one-shot clips stop on the last frame",
          "[animation]")
{
    Animator animator;
    animator.play(kBoom);
    CHECK(kBoom.frameCount() == 4);
    CHECK(kBoom.duration() == Catch::Approx(0.25).margin(1e-12));

    // Not finished before the end; clamps exactly at the total duration.
    animator.update(0.10);
    CHECK_FALSE(animator.finished());
    CHECK(animator.frame() == 1);  // 0.10/0.0625 -> index 1

    animator.update(1.0);  // way past the end
    CHECK(animator.finished());
    CHECK(animator.frame() == 3);  // clamped to the LAST frame
    const double frozen = animator.time();
    animator.update(1.0);
    CHECK(animator.time() == frozen);   // no further movement
    CHECK(animator.frame() == 3);

    // play() restarts cleanly.
    animator.play(kBoom);
    CHECK_FALSE(animator.finished());
    CHECK(animator.frame() == 0);
    CHECK(animator.time() == 0.0);
}

TEST_CASE("animation: animators never alter physics", "[animation][enemy]")
{
    // Two identical formations stepped identically; one gets arbitrary
    // animator updates interleaved. Positions must be BIT-identical.
    EnemyFormation plain;
    EnemyFormation withAnim;
    std::array<Animator, EnemyFormation::kTotal> animators{};

    for (auto& a : animators) {
        a.play(kTwoFrame);
    }

    for (int step = 1; step <= 600; ++step) {
        plain.update(kDt);
        withAnim.update(kDt);
        for (auto& a : animators) {
            a.update(kDt * 3.0);  // deliberately different rate
            a.update(-kDt);       // and junk input: still irrelevant
        }
        if (step % 100 == 0) {
            CHECK(withAnim.position() == plain.position());
            CHECK(withAnim.phase() == plain.phase());
        }
    }
}

TEST_CASE("animation: destroyed entities leave nothing dangling",
          "[animation]")
{
    // The animator array outlives entities: killing an enemy only stops
    // its DRAWING (Game iterates alive enemies), and the slot is safely
    // re-playable after a reset/rebuild.
    std::array<Animator, EnemyFormation::kTotal> animators{};
    for (auto& a : animators) {
        a.play(kBoom);
    }
    for (auto& a : animators) {
        a.update(1.0);  // all finished (clamped at last frame)
        CHECK(a.finished());
    }

    // "Rebuild": replaying is always safe; pointers point at static clips.
    for (auto& a : animators) {
        a.play(kTwoFrame);
        CHECK_FALSE(a.finished());
        CHECK(a.frame() == 0);
    }

    // A default (clip-less) animator draws as a safe no-op and reports
    // frame 0.
    Animator empty;
    CHECK_FALSE(empty.valid());
    CHECK(empty.frame() == 0);
    CHECK_FALSE(empty.finished());
    empty.update(1.0);  // inert
    CHECK(empty.time() == 0.0);
}
