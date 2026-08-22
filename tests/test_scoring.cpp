// Stage 9 scoring tests (docs/test_plan.md, Stage 9).
//
// Pure logic: ScoreManager is SDL-free (dependency rule,
// docs/architecture.md §1), so no window, no SDL init, no rendering.
// The points come from the kEnemyDefinitions data table (spec §6.1) — the
// manager must not carry a duplicated score table.

#include <catch2/catch_test_macros.hpp>

#include "gameplay/Enemy.hpp"
#include "gameplay/ScoreManager.hpp"

using namespace galaxian;

TEST_CASE("score: addKill awards the spec points per type", "[score]")
{
    ScoreManager score;
    CHECK(score.score() == 0);
    CHECK(score.kills() == 0);

    // Spec §6.1: Scout 50, Guard 80, Commander 150.
    CHECK(score.addKill(EnemyType::Scout) == 50);
    CHECK(score.addKill(EnemyType::Guard) == 80);
    CHECK(score.addKill(EnemyType::Commander) == 150);

    CHECK(score.score() == 280);
    CHECK(score.kills() == 3);
}

TEST_CASE("score: addKill points come from the EnemyDefinition table",
          "[score]")
{
    // Single source of truth: whatever the kEnemyDefinitions table says is
    // what a kill is worth (no duplicated score table in the manager).
    ScoreManager score;
    for (int type = 0; type < kEnemyTypeCount; ++type) {
        const EnemyType t = static_cast<EnemyType>(type);
        CHECK(score.addKill(t) == kEnemyDefinitions[type].points);
    }
    CHECK(score.score() == 50 + 80 + 150);
    CHECK(score.kills() == kEnemyTypeCount);
}

TEST_CASE("score: the multiplier hook scales the base points", "[score]")
{
    // Spec §6.4: a diving enemy is worth 2x its base points. Stage 9 combat
    // always uses the default multiplier of 1; the manager already supports
    // the Stage 11+ case.
    ScoreManager score;
    CHECK(score.addKill(EnemyType::Commander, 2) == 300);
    CHECK(score.addKill(EnemyType::Scout, 2) == 100);
    CHECK(score.addKill(EnemyType::Guard) == 80);  // default multiplier is 1
    CHECK(score.score() == 300 + 100 + 80);
    CHECK(score.kills() == 3);
}

TEST_CASE("score: addPoints adds a raw amount without a kill", "[score]")
{
    // Raw amounts are for wave bonuses and other future sources
    // (spec §9/§11); they do not change the kill count.
    ScoreManager score;
    score.addKill(EnemyType::Guard);
    CHECK(score.addPoints(1234) == 1234);
    CHECK(score.score() == 80 + 1234);
    CHECK(score.kills() == 1);
}

TEST_CASE("score: kills accumulate over a full formation", "[score]")
{
    // The spec §6.2 grid holds 8 Commanders, 16 Guards, 16 Scouts:
    // 8*150 + 16*80 + 16*50 = 3280.
    ScoreManager score;
    for (int i = 0; i < 8; ++i) {
        score.addKill(EnemyType::Commander);
    }
    for (int i = 0; i < 16; ++i) {
        score.addKill(EnemyType::Guard);
    }
    for (int i = 0; i < 16; ++i) {
        score.addKill(EnemyType::Scout);
    }
    CHECK(score.score() == 3280);
    CHECK(score.kills() == 40);
}

TEST_CASE("score: reset returns a fresh game", "[score]")
{
    ScoreManager score;
    for (int i = 0; i < 40; ++i) {
        score.addKill(EnemyType::Scout);
    }
    CHECK(score.score() == 2000);
    CHECK(score.kills() == 40);

    score.reset();
    CHECK(score.score() == 0);
    CHECK(score.kills() == 0);

    // The manager is fully usable after a reset.
    CHECK(score.addKill(EnemyType::Guard) == 80);
    CHECK(score.score() == 80);
    CHECK(score.kills() == 1);
}

TEST_CASE("score: the session high score tracks the peak", "[score]")
{
    ScoreManager score;
    CHECK(score.highScore() == 0);

    score.addKill(EnemyType::Commander);            // 150
    CHECK(score.highScore() == 150);
    score.addKill(EnemyType::Scout);                // 200
    CHECK(score.highScore() == 200);

    // Raw points track the peak too...
    CHECK(score.addPoints(1000) == 1000);
    CHECK(score.highScore() == 1200);

    // ...and a fresh game starts back at zero WITHOUT losing the session
    // best (spec §11: the high score belongs to the session, not the run).
    score.reset();
    CHECK(score.score() == 0);
    CHECK(score.highScore() == 1200);

    // A weaker run never lowers it.
    score.addKill(EnemyType::Guard);
    CHECK(score.highScore() == 1200);

    // Surpassing it takes over again.
    for (int i = 0; i < 25; ++i) {
        score.addKill(EnemyType::Guard);            // 80 * 25 = 2000
    }
    CHECK(score.score() == 2000 + 80);
    CHECK(score.highScore() == 2080);

    // Only an explicit reset clears the session best (the running score is
    // untouched); the very next scoring event re-seeds it.
    score.resetHighScore();
    CHECK(score.highScore() == 0);
    CHECK(score.score() == 2080);
    score.addKill(EnemyType::Scout);
    CHECK(score.highScore() == 2130);
}
