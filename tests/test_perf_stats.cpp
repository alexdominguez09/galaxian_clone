// Stage 25 performance instrumentation tests (docs/test_plan.md, Stage 25).
//
// PerformanceStats is pure POD accumulation (no SDL, no Game), so the
// average/max math is verified headlessly. These are ordinary unit tests
// of the instrumentation — not a stress harness.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <string>

#include "core/PerformanceStats.hpp"

using namespace galaxian;

TEST_CASE("perf stats: empty stats report zeros", "[perf]")
{
    PerformanceStats p;
    CHECK(p.frames == 0);
    CHECK(p.updates == 0);
    CHECK(p.avgFrameTimeMs() == Catch::Approx(0.0));
    CHECK(p.avgStepCostMs() == Catch::Approx(0.0));
    CHECK(p.summaryLine().find("[perf]") == 0);
}

TEST_CASE("perf stats: averages and maxima accumulate correctly", "[perf]")
{
    PerformanceStats p;
    // 3 frames: (frame=0.016, 1 step, cost=0.0002), (0.020, 2, 0.0004),
    // (0.010, 0, 0.0).
    p.recordFrame(0.016, 1, 0.0002);
    p.recordFrame(0.020, 2, 0.0004);
    p.recordFrame(0.010, 0, 0.0);

    CHECK(p.frames == 3);
    CHECK(p.updates == 3);

    // Average frame time = (0.016+0.020+0.010)/3 = 0.015333... s.
    CHECK(p.avgFrameTimeMs() == Catch::Approx(15.333333).epsilon(1e-6));
    // Max frame time = 0.020 s = 20 ms.
    CHECK(p.frameTimeMaxSeconds == Catch::Approx(0.020));

    // Average step cost = total cost / total steps =
    // (0.0002+0.0004+0.0)/3 = 0.0002 s = 0.2 ms.
    CHECK(p.avgStepCostMs() == Catch::Approx(0.2).epsilon(1e-9));
    // Max step (estimated from its block: 0.0004 / 2) = 0.0002 s = 0.2 ms.
    CHECK(p.updateCostMaxSeconds == Catch::Approx(0.0004));
    CHECK(p.updateCostMaxSteps == 2);
    CHECK(p.updatesPerFrameMax == Catch::Approx(2.0));

    // The summary line carries the frame/step figures.
    const std::string line = p.summaryLine();
    CHECK(line.find("frames=3") != std::string::npos);
    CHECK(line.find("updates=3") != std::string::npos);
    CHECK(line.find("max_frame=20.00ms") != std::string::npos);
}

TEST_CASE("perf stats: reset clears everything", "[perf]")
{
    PerformanceStats p;
    p.recordFrame(0.016, 1, 0.0002);
    p.reset();
    CHECK(p.frames == 0);
    CHECK(p.updates == 0);
    CHECK(p.avgFrameTimeMs() == Catch::Approx(0.0));
    CHECK(p.avgStepCostMs() == Catch::Approx(0.0));
}
