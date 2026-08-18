#include "GameClock.hpp"

namespace galaxian {

Uint64 GameClock::nowTicks() const
{
    return SDL_GetPerformanceCounter();
}

double GameClock::ticksToSeconds(Uint64 ticks) const
{
    return static_cast<double>(ticks) /
           static_cast<double>(SDL_GetPerformanceFrequency());
}

void GameClock::start()
{
    startTicks_ = nowTicks();
    lastTicks_ = startTicks_;
    running_ = true;
}

void GameClock::stop()
{
    running_ = false;
}

double GameClock::elapsed() const
{
    if (!running_) {
        return 0.0;
    }
    return ticksToSeconds(nowTicks() - startTicks_);
}

double GameClock::frameDelta()
{
    if (!running_) {
        return 0.0;
    }
    const Uint64 now = nowTicks();
    double delta = ticksToSeconds(now - lastTicks_);
    lastTicks_ = now;
    if (delta > kMaxFrameDeltaSeconds) {
        delta = kMaxFrameDeltaSeconds;
    }
    return delta;
}

}  // namespace galaxian
