#include "RunStats.hpp"

#include <cstdio>

namespace galaxian {

std::string RunStats::summaryLine() const
{
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "[run] time=%.1fs shots=%d acc=%.0f%% kills=%d deaths=%d "
                  "waves=%d",
                  runTimeSeconds, shotsFired, accuracy() * 100.0,
                  enemiesKilled, playerDeaths, wavesReached);
    return buf;
}

}  // namespace galaxian
