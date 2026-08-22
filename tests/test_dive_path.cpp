// Stage 12 dive trajectory tests (docs/test_plan.md, Stage 12).
//
// Pure logic: CubicBezier / DivePath / PathFollower are SDL-free
// (dependency rule); no window, no SDL init, no rendering.
//
// Exact-value notes (scratch-derived by linking the real DivePath code):
//   * Left/Right dives from (176, 64): arc length 480.442596 each; the
//     mirrored geometry makes the two lengths bit-equal.
//   * CenterAttack from (176, 64) and from (32, 208): 419.312500.
//   * A Commander LeftDive cycle runs 412 + 90 + 442 = 944 motion updates
//     (dive, dash off-screen, return) after the 30-update preparation.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>
#include <tuple>
#include <utility>

#include "core/Constants.hpp"
#include "gameplay/DivePath.hpp"
#include "gameplay/Enemy.hpp"
#include "gameplay/EnemyFormation.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;

// Advances `follower` along `path` in `chunks` steps of `chunkPx`.
void advanceBy(const DivePath& path, PathFollower& follower, double chunkPx,
               int chunks)
{
    for (int i = 0; i < chunks && !follower.finished(); ++i) {
        follower.advance(path, chunkPx);
    }
}

// Independent Bernstein-formula evaluation used as a cross-check of the
// implementation's de Casteljau recursion.
Vector2 bernstein(const CubicBezier& c, float t)
{
    const float u = 1.0f - t;
    const float b0 = u * u * u;
    const float b1 = 3.0f * u * u * t;
    const float b2 = 3.0f * u * t * t;
    const float b3 = t * t * t;
    return {b0 * c.p0.x + b1 * c.p1.x + b2 * c.p2.x + b3 * c.p3.x,
            b0 * c.p0.y + b1 * c.p1.y + b2 * c.p2.y + b3 * c.p3.y};
}

}  // namespace

TEST_CASE("bezier: endpoint evaluation is exact and clamped",
          "[dive][bezier]")
{
    const CubicBezier c{Vector2{10.0f, 20.0f}, Vector2{30.0f, -40.0f},
                        Vector2{-50.0f, 60.0f}, Vector2{70.0f, -80.0f}};
    // P(0) == P0 and P(1) == P3 BIT-EXACTLY (the return-path snap relies
    // on this).
    CHECK(c.evaluate(0.0f) == Vector2{10.0f, 20.0f});
    CHECK(c.evaluate(1.0f) == Vector2{70.0f, -80.0f});
    // Outside [0, 1] clamps to the endpoints.
    CHECK(c.evaluate(-0.5f) == Vector2{10.0f, 20.0f});
    CHECK(c.evaluate(2.0f) == Vector2{70.0f, -80.0f});

    // Degenerate curve: every control point equal -> constant.
    const CubicBezier flat{Vector2{5.0f, 5.0f}, Vector2{5.0f, 5.0f},
                           Vector2{5.0f, 5.0f}, Vector2{5.0f, 5.0f}};
    CHECK(flat.evaluate(0.37f) == Vector2{5.0f, 5.0f});
}

TEST_CASE("bezier: interior evaluation matches the Bernstein form",
          "[dive][bezier]")
{
    const CubicBezier c{Vector2{176.0f, 64.0f},
                        Vector2{104.0f, 28.0f},
                        Vector2{-16.0f, 304.0f},
                        Vector2{80.0f, 480.0f}};
    for (int i = 1; i < 20; ++i) {
        const float t = static_cast<float>(i) / 20.0f;
        const Vector2 p = c.evaluate(t);
        const Vector2 q = bernstein(c, t);
        CHECK(p.x == Catch::Approx(q.x).margin(1e-4));
        CHECK(p.y == Catch::Approx(q.y).margin(1e-4));
    }
}

TEST_CASE("bezier: left/right dive curves are exact mirror images",
          "[dive][bezier]")
{
    const Vector2 start{176.0f, 64.0f};
    const DivePath left =
        DivePath::make(DivePattern::LeftDive, start);
    const DivePath right =
        DivePath::make(DivePattern::RightDive, start);

    // Mirrored about the vertical line x = start.x at EVERY parameter.
    // The de Casteljau lerps are sign-symmetric mathematically but each
    // product rounds independently, so allow float noise.
    for (int i = 0; i <= 20; ++i) {
        const float t = static_cast<float>(i) / 20.0f;
        const Vector2 l = left.curve().evaluate(t);
        const Vector2 r = right.curve().evaluate(t);
        CHECK(r.x - start.x ==
              Catch::Approx(-(l.x - start.x)).margin(1e-3));
        CHECK(r.y == Catch::Approx(l.y).margin(1e-3));
    }
    // Mirror symmetry => identical arc lengths (bit-exact).
    CHECK(left.length() == right.length());
    CHECK(left.length() == Catch::Approx(480.442596).margin(1e-4));
}

TEST_CASE("bezier: symmetric control points evaluate symmetrically",
          "[dive][bezier]")
{
    // A curve mirror-symmetric about x = 224.
    const CubicBezier c{Vector2{248.0f, 100.0f}, Vector2{298.0f, 50.0f},
                        Vector2{150.0f, 50.0f}, Vector2{200.0f, 100.0f}};
    for (int i = 1; i < 20; ++i) {
        const float t = static_cast<float>(i) / 20.0f;
        const Vector2 a = c.evaluate(t);
        const Vector2 b = c.evaluate(1.0f - t);
        CHECK(a.x + b.x == Catch::Approx(448.0f).margin(1e-3));
        CHECK(a.y == Catch::Approx(b.y).margin(1e-3));
    }
}

TEST_CASE("divepath: factory geometry matches the documented offsets",
          "[dive][bezier]")
{
    const Vector2 s{176.0f, 64.0f};

    const DivePath left = DivePath::make(DivePattern::LeftDive, s);
    CHECK(left.curve().p0 == s);
    CHECK(left.curve().p1 == s + Vector2{-72.0f, -36.0f});
    CHECK(left.curve().p2 == s + Vector2{-192.0f, 240.0f});
    CHECK(left.curve().p3 == s + Vector2{-96.0f, 416.0f});

    const DivePath center =
        DivePath::make(DivePattern::CenterAttack, s);
    CHECK(center.curve().p1 == s + Vector2{0.0f, -24.0f});
    CHECK(center.curve().p2 == s + Vector2{0.0f, 200.0f});
    CHECK(center.curve().p3 == s + Vector2{0.0f, 416.0f});
    CHECK(center.length() == Catch::Approx(419.312500).margin(1e-4));

    // Deterministic: identical inputs give bit-identical paths.
    const DivePath again =
        DivePath::make(DivePattern::LeftDive, s);
    CHECK(again.curve().p0 == left.curve().p0);
    CHECK(again.curve().p1 == left.curve().p1);
    CHECK(again.curve().p2 == left.curve().p2);
    CHECK(again.curve().p3 == left.curve().p3);
    CHECK(again.length() == left.length());
}

TEST_CASE("divepath: arc length exceeds the straight-line distance",
          "[dive][bezier]")
{
    const Vector2 s{176.0f, 64.0f};
    const auto straight = [](const Vector2& a, const Vector2& b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    for (const DivePattern pattern :
         {DivePattern::LeftDive, DivePattern::RightDive,
          DivePattern::CenterAttack}) {
        const DivePath p = DivePath::make(pattern, s);
        CHECK(p.length() > 0.0f);
        CHECK(p.length() > straight(s, p.curve().p3));
        // The curve never exceeds its control polygon.
        const float polygon =
            straight(p.curve().p0, p.curve().p1) +
            straight(p.curve().p1, p.curve().p2) +
            straight(p.curve().p2, p.curve().p3);
        CHECK(p.length() < polygon);
    }

    // The return path from the Commander cycle's attack end to its slot.
    const DivePath ret =
        DivePath::make(DivePattern::ReturnPath,
                       Vector2{-25.0f, 480.0f}, Vector2{176.0f, 64.0f});
    CHECK(ret.length() == Catch::Approx(515.623962).margin(1e-3));
}

TEST_CASE("follower: t stays in [0, 1] under 10 000 headless updates",
          "[dive][follower][stress]")
{
    const DivePath path =
        DivePath::make(DivePattern::LeftDive, Vector2{176.0f, 64.0f});
    PathFollower follower;
    follower.begin();
    CHECK_FALSE(follower.finished());

    // Deliberately oversized advances: the clamp must hold forever, and
    // once t reaches 1 the follower must stay finished.
    int finishStep = -1;
    for (int i = 0; i < 10000; ++i) {
        follower.advance(path, 100.0);
        REQUIRE(follower.t() >= 0.0f);
        REQUIRE(follower.t() <= 1.0f);
        if (follower.finished()) {
            if (finishStep < 0) {
                finishStep = i;
            }
            CHECK(follower.finished());  // stays finished
        }
    }
    // The ~480 px arc is exhausted by the 5th 100 px advance...
    REQUIRE(finishStep == 4);
    CHECK(follower.t() == 1.0f);

    // Negative or zero advances never move a fresh follower below 0...
    PathFollower fresh;
    fresh.begin();
    fresh.advance(path, -100.0);
    CHECK(fresh.t() == 0.0f);
    fresh.advance(path, 0.0);
    CHECK(fresh.t() == 0.0f);
    // ...and half the arc lands at half the parameter.
    fresh.advance(path, static_cast<double>(path.length()) / 2.0);
    CHECK(fresh.t() == Catch::Approx(0.5f).margin(1e-6));
}

TEST_CASE("follower: identical total pixels land identically regardless "
          "of chunking (frame-rate independence)",
          "[dive][follower]")
{
    const DivePath path =
        DivePath::make(DivePattern::CenterAttack, Vector2{32.0f, 208.0f});
    const double total = 200.0;  // well short of the 419 px arc

    // One call vs many calls covering the same distance.
    PathFollower one;
    one.begin();
    advanceBy(path, one, total, 1);
    PathFollower many;
    many.begin();
    advanceBy(path, many, total / 60.0, 60);
    PathFollower fine;
    fine.begin();
    advanceBy(path, fine, total / 120.0, 120);

    // Same consumed distance -> same parameter within accumulation noise.
    CHECK(one.t() == Catch::Approx(total / path.length()).margin(1e-5));
    CHECK(many.t() == Catch::Approx(one.t()).margin(1e-4));
    CHECK(fine.t() == Catch::Approx(many.t()).margin(1e-4));
    const Vector2 pa = path.curve().evaluate(one.t());
    const Vector2 pb = path.curve().evaluate(many.t());
    CHECK(pa.x == Catch::Approx(pb.x).margin(1e-3));
    CHECK(pa.y == Catch::Approx(pb.y).margin(1e-3));

    // And at full speed granularity: 60 Hz vs 120 Hz stepping of a Scout's
    // CenterAttack both finish on exactly the same spot.
    auto runFull = [](double dt, int steps) {
        const DivePath d =
            DivePath::make(DivePattern::CenterAttack,
                           Vector2{32.0f, 208.0f});
        PathFollower f;
        f.begin();
        for (int i = 0; i < steps && !f.finished(); ++i) {
            f.advance(d, 140.0 * dt);
        }
        return d.curve().evaluate(f.t());
    };
    const Vector2 p60 = runFull(kDt, 400);
    const Vector2 p120 = runFull(kDt / 2.0, 800);
    CHECK(p60 == Vector2{32.0f, 624.0f});   // P(1) == P3, bit-exact
    CHECK(p120 == p60);                     // same endpoint either way
}

TEST_CASE("returnpath: dynamic end tracks the live target", "[dive][bezier]")
{
    // Built while the slot sat at T0; the formation sways to T1 before the
    // flight completes. The tail must follow, and P(1) must be EXACTLY the
    // live slot so the rejoin stays bit-exact.
    const Vector2 attackEnd{-25.0f, 480.0f};
    const Vector2 t0{176.0f, 64.0f};
    const Vector2 t1 = t0 + Vector2{16.0f, 0.0f};  // swayed right

    const DivePath ret =
        DivePath::make(DivePattern::ReturnPath, attackEnd, t0);
    const CubicBezier live = ret.endCurve(t1);

    CHECK(live.evaluate(1.0f) == t1);       // bit-exact new end
    CHECK(live.evaluate(0.0f) == attackEnd);
    CHECK(ret.curve().evaluate(1.0f) == t0);  // stored path untouched

    // The shape barely moved: midpoints stay close together.
    const Vector2 mOld = ret.curve().evaluate(0.5f);
    const Vector2 mNew = live.evaluate(0.5f);
    const float dx = mNew.x - mOld.x;
    const float dy = mNew.y - mOld.y;
    CHECK(std::sqrt(dx * dx + dy * dy) < 40.0f);
}

TEST_CASE("enemy: every type executes every pattern and returns safely "
          "(reproducible)",
          "[dive][enemy][statemachine]")
{
    // The Stage 12 acceptance: multiple enemy types can execute
    // reproducible attack patterns and return safely. Each combination
    // runs a FULL cycle against a fixed anchor; two runs must be
    // bit-identical.
    struct Case {
        EnemyType type;
        DivePattern pattern;
        Vector2 slot;      // representative row per type
        int speed;         // definition speed (px/s), for the guard bound
    };
    const Case cases[] = {
        {EnemyType::Scout, DivePattern::LeftDive, {48.0f, 208.0f}, 140},
        {EnemyType::Scout, DivePattern::CenterAttack, {48.0f, 208.0f}, 140},
        {EnemyType::Scout, DivePattern::RightDive, {48.0f, 208.0f}, 140},
        {EnemyType::Guard, DivePattern::LeftDive, {128.0f, 136.0f}, 100},
        {EnemyType::Guard, DivePattern::RightDive, {128.0f, 136.0f}, 100},
        {EnemyType::Guard, DivePattern::CenterAttack, {128.0f, 136.0f}, 100},
        {EnemyType::Commander, DivePattern::LeftDive, {144.0f, 64.0f}, 70},
        {EnemyType::Commander, DivePattern::CenterAttack, {144.0f, 64.0f},
         70},
        {EnemyType::Commander, DivePattern::RightDive, {144.0f, 64.0f}, 70},
    };

    const Vector2 fp = EnemyFormation::kAnchor;

    for (const Case& c : cases) {
        CAPTURE(static_cast<int>(c.type),
                std::string(divePatternName(c.pattern)));

        auto runCycle = [&]() {
            Enemy enemy(c.type, c.slot);
            REQUIRE(enemy.beginDive(c.pattern));
            bool sawDiving = false;
            bool sawAttacking = false;
            bool sawReturning = false;
            for (int step = 0; step < 6000; ++step) {
                enemy.update(kDt, fp);
                switch (enemy.state()) {
                    case EnemyState::Diving:    sawDiving = true; break;
                    case EnemyState::Attacking: sawAttacking = true; break;
                    case EnemyState::Returning: sawReturning = true; break;
                    default: break;
                }
                if (enemy.state() == EnemyState::Formation && step > 31) {
                    break;
                }
            }
            return std::pair{enemy.divePosition(),
                             std::tuple{sawDiving, sawAttacking,
                                        sawReturning}};
        };

        // Two independent cycles: reproducible means bit-identical.
        const auto [posA, phasesA] = runCycle();
        const auto [posB, phasesB] = runCycle();
        CHECK(posA == posB);
        CHECK(phasesA == phasesB);
        CHECK(std::get<0>(phasesA));  // dove...
        CHECK(std::get<1>(phasesA));  // ...attacked...
        CHECK(std::get<2>(phasesA));  // ...and returned.

        // Safe return: back on the original lattice slot, bit-exact.
        const Vector2 expected = fp + c.slot;
        CHECK(posA == expected);
    }
}
