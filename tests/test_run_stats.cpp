// Stage 23 telemetry struct tests (pure POD math).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "gameplay/RunStats.hpp"

using galaxian::RunStats;

TEST_CASE("run stats: accuracy handles zero-shot runs", "[runstats]")
{
    RunStats s;
    CHECK(s.accuracy() == 0.0);
    s.enemiesKilled = 5;  // kills without recorded shots cannot happen in
    CHECK(s.accuracy() == 0.0);  // gameplay; guard keeps it defined anyway
}

TEST_CASE("run stats: accuracy is kills over fired shots", "[runstats]")
{
    RunStats s;
    s.shotsFired = 10;
    s.enemiesKilled = 3;
    CHECK(s.accuracy() == Catch::Approx(0.3).margin(1e-12));
    s.shotsFired = 4;
    s.enemiesKilled = 4;
    CHECK(s.accuracy() == Catch::Approx(1.0).margin(1e-12));
}

TEST_CASE("run stats: summary line carries every metric", "[runstats]")
{
    RunStats s;
    s.runTimeSeconds = 42.7;
    s.shotsFired = 20;
    s.enemiesKilled = 9;
    s.playerDeaths = 2;
    s.wavesReached = 3;

    const std::string line = s.summaryLine();
    CHECK(line.find("time=42.7s") != std::string::npos);
    CHECK(line.find("shots=20") != std::string::npos);
    CHECK(line.find("acc=45%") != std::string::npos);   // 9/20 rounds to 45%
    CHECK(line.find("kills=9") != std::string::npos);
    CHECK(line.find("deaths=2") != std::string::npos);
    CHECK(line.find("waves=3") != std::string::npos);

    // reset() zeroes everything back.
    s.reset();
    const std::string fresh = s.summaryLine();
    CHECK(fresh.find("time=0.0s") != std::string::npos);
    CHECK(fresh.find("shots=0") != std::string::npos);
}
