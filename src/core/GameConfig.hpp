#pragma once

#include <string>

namespace galaxian {

// Data-driven gameplay balance (docs/game_spec.md §14, Stage 21).
//
// Every field here is a BALANCE knob loaded from
// assets/config/game.json; the values below are the documented defaults,
// identical to the previously frozen constants. STRUCTURAL RULES stay in
// C++: state machines, collision semantics, the spec §7 progression SHAPE
// and its hard caps, pool sizes, grid geometry.
//
// Loading rules:
//   * loadFromFile() on a missing/unreadable file returns false and keeps
//     the defaults — never a crash.
//   * Invalid JSON or an invalid value for a known key falls back to that
//     key's default (a note goes to stderr); unknown keys are ignored.
//   * Post-load structural clamps are applied (e.g., attackers 1..4,
//     shots 1..2, speeds > 0).
struct GameConfig {
    // ---- Player (spec §5) ----
    float playerSpeed = 220.0f;            // px/s, horizontal
    int initialLives = 3;
    float playerBulletSpeed = 480.0f;      // px/s, upward
    double fireCooldownSeconds = 0.35;
    int maxPlayerProjectiles = 2;

    // ---- Enemy types (spec §6.1): points + dive speeds ----
    int scoutPoints = 50;
    int guardPoints = 80;
    int commanderPoints = 150;
    float scoutDiveSpeed = 140.0f;         // px/s along the dive arc
    float guardDiveSpeed = 100.0f;
    float commanderDiveSpeed = 70.0f;

    // ---- Enemy bullets (spec §8) ----
    float enemyBulletBaseSpeed = 240.0f;   // wave-1 speed
    float enemyBulletRampPerWave = 40.0f;  // + per completed wave
    float enemyBulletMaxSpeed = 360.0f;    // hard cap

    // ---- Formation motion (spec §6.3) ----
    float oscillationSwingPx = 64.0f;      // peak-to-peak
    double oscillationPeriodSeconds = 4.0; // at full strength
    double oscillationMaxMultiplier = 2.5; // death-pressure bound

    // ---- Attack pacing table (spec §7). Row per wave bracket:
    //        [0]=wave1 [1]=wave2 [2]=wave3 [3]=wave4 [4]=waves5-6
    //        [5]=waves7+ (the final cap rise)
    struct WaveRow {
        int maxAttackers;
        double intervalSeconds;
        int shotsPerAttack;
    };
    WaveRow waves[6] = {
        {1, 6.0, 1},
        {2, 6.0, 1},
        {2, 4.0, 1},
        {2, 4.0, 2},
        {3, 3.0, 2},
        {4, 3.0, 2},
    };

    // ---- Wave system (spec §9) ----
    double interstitialSeconds = 2.0;

    // ---- Explosions / effects (Stage 9/19 placeholder duration) ----
    double explosionSeconds = 0.25;

    // ---- Player lifecycle (spec §5) ----
    double respawnDelaySeconds = 1.5;
    double invulnerableSeconds = 2.0;

    // Loads `path` over the current defaults. Returns false if the file is
    // missing/unreadable/unparseable (defaults remain); true when parsed.
    bool loadFromFile(const std::string& path);

    // The process-wide configuration used by all gameplay systems. Game
    // sets it once at boot (defaults apply before that, so unit tests are
    // unaffected).
    static const GameConfig& get();
    static void set(const GameConfig& config);

private:
    void clampToBounds();
};

}  // namespace galaxian
