#include "TimestepController.hpp"

namespace galaxian {

TimestepController::TimestepController(double dtSeconds, int maxCatchUpSteps)
    : dt_(dtSeconds), maxCatchUpSteps_(maxCatchUpSteps) {}

int TimestepController::advance(double frameDeltaSeconds)
{
    dropped_ = false;
    if (frameDeltaSeconds <= 0.0 || dt_ <= 0.0) {
        return 0;
    }

    accumulator_ += frameDeltaSeconds;

    int steps = 0;
    while (accumulator_ >= dt_ && steps < maxCatchUpSteps_) {
        accumulator_ -= dt_;
        ++steps;
    }

    if (steps == maxCatchUpSteps_ && accumulator_ >= dt_) {
        // Backlog exceeds the catch-up cap: drop the excess so a long stall
        // cannot produce a burst of updates on the next frame.
        accumulator_ = 0.0;
        dropped_ = true;
    }

    return steps;
}

}  // namespace galaxian
