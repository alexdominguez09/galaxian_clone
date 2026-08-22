#pragma once

#include "gameplay/Enemy.hpp"

namespace galaxian {

// The central scoring subsystem (Stage 9, the plan's "ScoreManager",
// docs/architecture.md §3.8): the single source of truth for the player's
// score and kill count.
//
// SDL-free (dependency rule, docs/architecture.md §1): pure logic, no
// rendering. The HUD (Stage 18) only displays what this class reports;
// score rules never live in drawing code.
//
// Points per enemy type come from the kEnemyDefinitions data table
// (docs/game_spec.md §6.1) — the score table is NOT duplicated here.
class ScoreManager {
public:
    // Awards points for destroying an enemy of `type`: the type's base
    // points (spec §6.1) times `multiplier`. Stage 9 always uses the
    // default multiplier of 1; Stage 11+ will pass 2 for diving enemies
    // (spec §6.4: a diving enemy is worth 2x its base points).
    // Returns the number of points added to the score.
    int addKill(EnemyType type, int multiplier = 1);

    // Awards a raw score amount (wave bonuses and other future sources,
    // spec §9/§11). Does not change the kill count. Returns the number of
    // points added.
    int addPoints(int points);

    // Current score.
    int score() const { return score_; }
    // Total enemies destroyed (diagnostics/tests).
    int kills() const { return kills_; }

    // Back to a fresh game (Stage 17 state transitions, tests).
    void reset();

private:
    int score_ = 0;
    int kills_ = 0;
};

}  // namespace galaxian
