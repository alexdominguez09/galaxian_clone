#pragma once

#include <SDL2/SDL.h>

namespace galaxian {

// Monotonic high-resolution clock backed by the SDL performance counter.
//
// Lives in core/, which is allowed to use SDL (docs/architecture.md §1).
// Gameplay code must not read time anywhere else.
class GameClock {
public:
    // Seconds since start(). Returns 0 before start().
    double elapsed() const;

    // Seconds since the previous frameDelta() call (or since start),
    // clamped to kMaxFrameDeltaSeconds so a single stall (window drag,
    // debugger pause, system sleep) cannot produce a burst of catch-up
    // updates. The accumulator cap in TimestepController is the second
    // line of defense.
    double frameDelta();

    void start();
    void stop();

    static constexpr double kMaxFrameDeltaSeconds = 0.25;

private:
    Uint64 nowTicks() const;
    double ticksToSeconds(Uint64 ticks) const;

    Uint64 startTicks_ = 0;
    Uint64 lastTicks_ = 0;
    bool running_ = false;
};

}  // namespace galaxian
