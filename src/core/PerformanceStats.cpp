#include "PerformanceStats.hpp"

#include <cstdio>

namespace galaxian {

void PerformanceStats::reset()
{
    frames = 0;
    updates = 0;
    frameTimeSumSeconds = 0.0;
    updateCostSumSeconds = 0.0;
    frameTimeMaxSeconds = 0.0;
    updateCostMaxSeconds = 0.0;
    updateCostMaxSteps = 0;
    updatesPerFrameMax = 0.0;
}

void PerformanceStats::recordFrame(double frameSeconds, long steps,
                                   double updateCostSeconds)
{
    ++frames;
    updates += steps;
    frameTimeSumSeconds += frameSeconds;
    updateCostSumSeconds += updateCostSeconds;
    if (frameSeconds > frameTimeMaxSeconds) {
        frameTimeMaxSeconds = frameSeconds;
    }
    if (steps > 0 && updateCostSeconds > updateCostMaxSeconds) {
        updateCostMaxSeconds = updateCostSeconds;
        updateCostMaxSteps = steps;
    }
    const double upf = static_cast<double>(steps);
    if (upf > updatesPerFrameMax) {
        updatesPerFrameMax = upf;
    }
}

double PerformanceStats::avgFrameTimeMs() const
{
    return frames > 0 ? (frameTimeSumSeconds / frames) * 1000.0 : 0.0;
}

double PerformanceStats::avgStepCostMs() const
{
    return updates > 0 ? (updateCostSumSeconds / updates) * 1000.0 : 0.0;
}

std::string PerformanceStats::summaryLine() const
{
    // max_step estimates the worst single step from its containing block
    // (uniform split); the true worst step could only be measured per-step,
    // which this rolling instrument does not do. Good enough for the
    // "within budget" check.
    const double worstStepMs =
        (updateCostMaxSteps > 0)
            ? (updateCostMaxSeconds /
               static_cast<double>(updateCostMaxSteps)) *
                  1000.0
            : 0.0;
    char buf[200];
    std::snprintf(buf, sizeof(buf),
                  "[perf] frames=%ld updates=%ld avg_frame=%.2fms "
                  "max_frame=%.2fms avg_step=%.3fms max_step=%.3fms "
                  "max_upf=%.1f",
                  frames, updates, avgFrameTimeMs(),
                  frameTimeMaxSeconds * 1000.0, avgStepCostMs(), worstStepMs,
                  updatesPerFrameMax);
    return buf;
}

}  // namespace galaxian
