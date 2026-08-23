#pragma once

#include <string>

namespace galaxian {

// Per-session performance instrumentation (Stage 25, docs/test_plan.md).
//
// A POD accumulator owned by Game and fed every rendered frame in run().
// It measures the REAL loop's cost — wall frame time, the number of fixed
// updates per frame, and the measured CPU time spent inside the fixed-
// update block — purely as telemetry. It NEVER influences simulation.
//
// The plan's "CPU profile: update cost within budget (logged)" and
// "stable frame time" criteria are evidenced by the `[perf]` summary line
// emitted once at shutdown.
struct PerformanceStats {
    // Totals (frames/updates seen, accumulated wall + measured CPU time).
    long frames = 0;
    long updates = 0;
    double frameTimeSumSeconds = 0.0;   // for the average frame time
    double updateCostSumSeconds = 0.0;  // for the average per-step cost
    // Worst observed values (the "stable frame time" check).
    double frameTimeMaxSeconds = 0.0;
    double updateCostMaxSeconds = 0.0;  // worst fixed-update block
    long updateCostMaxSteps = 0;        // steps in that worst block
    double updatesPerFrameMax = 0.0;

    void reset();

    // Feed one rendered frame: `frameSeconds` is the wall frame time,
    // `steps` the number of fixed updates it ran, `updateCostSeconds` the
    // measured CPU time spent inside that fixed-update block (0 if no
    // steps ran, e.g. while paused/title).
    void recordFrame(double frameSeconds, long steps,
                     double updateCostSeconds);

    double avgFrameTimeMs() const;
    // Average measured CPU time per fixed step (the "update cost within
    // budget" figure).
    double avgStepCostMs() const;

    // One-line `[perf]` summary (no trailing newline).
    std::string summaryLine() const;
};

}  // namespace galaxian
