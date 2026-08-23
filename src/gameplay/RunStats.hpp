#pragma once

#include <string>

namespace galaxian {

// Per-run gameplay telemetry (Stage 23 balancing, docs/test_plan.md).
//
// A POD accumulator owned by Game and updated at the existing composition-
// root event sites; it NEVER influences simulation behavior. Measured
// exactly what the plan asks for:
//
//   runTime      - simulated seconds since the run started (Playing)
//   shotsFired   - player bullets actually spawned
//   enemiesKilled- enemies destroyed this run
//   playerDeaths - lives consumed this run
//   wavesReached - highest wave number entered this run
//
// Accuracy derives from shots/kills: every spawned bullet can kill at
// most one enemy, so kills/shots is a strict upper bound of true aim
// efficiency -- good enough for balancing comparisons.
struct RunStats {
    double runTimeSeconds = 0.0;
    int shotsFired = 0;
    int enemiesKilled = 0;
    int playerDeaths = 0;
    int wavesReached = 1;

    void reset()
    {
        runTimeSeconds = 0.0;
        shotsFired = 0;
        enemiesKilled = 0;
        playerDeaths = 0;
        wavesReached = 1;
    }

    // Kill fraction of spawned shots, in [0,1]; 0 when nothing was fired.
    double accuracy() const
    {
        return shotsFired > 0
                   ? static_cast<double>(enemiesKilled) /
                         static_cast<double>(shotsFired)
                   : 0.0;
    }

    // One-line summary for logs (no trailing newline).
    std::string summaryLine() const;
};

}  // namespace galaxian
