#pragma once

namespace galaxian {

// Fixed-timestep accumulator (docs/architecture.md §3.1).
//
// Pure logic: no SDL, no allocation, no I/O. Feed it a frame delta (seconds
// of real time) and it tells you how many fixed updates to run this frame.
// This is what makes simulation speed independent of the render rate.
class TimestepController {
public:
    TimestepController(double dtSeconds, int maxCatchUpSteps);

    // Advance by a real-time frame delta; returns the number of fixed
    // updates to run this frame (0..maxCatchUpSteps). If the backlog
    // exceeds the catch-up cap, the excess time is dropped (no spiral of
    // death) and droppedTime() reports it.
    int advance(double frameDeltaSeconds);

    double dt() const { return dt_; }
    int maxCatchUpSteps() const { return maxCatchUpSteps_; }

    // True if the most recent advance() dropped backlog time.
    bool droppedTime() const { return dropped_; }

private:
    double dt_;
    int maxCatchUpSteps_;
    double accumulator_ = 0.0;
    bool dropped_ = false;
};

}  // namespace galaxian
