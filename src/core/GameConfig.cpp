#include "GameConfig.hpp"

#include <nlohmann/json.hpp>
#include <cstdio>
#include <fstream>

using json = nlohmann::json;

namespace galaxian {

namespace {

GameConfig gConfig;  // defaults until Game boots with the real file

// Reads `key` as a number into `out`; missing key = silently default;
// present-but-wrong-type = stderr note + default. Returns whether applied.
bool readNumber(const json& j, const char* key, double& out,
                double defaultValue)
{
    if (!j.contains(key)) {
        out = defaultValue;
        return false;
    }
    const auto& v = j.at(key);
    if (!v.is_number()) {
        std::fprintf(stderr,
                     "config: '%s' is not a number, using default %g\n",
                     key, defaultValue);
        out = defaultValue;
        return false;
    }
    out = v.get<double>();
    return true;
}

void applyNumber(const json& j, const char* key, double defaultValue,
                 float* target)
{
    double raw = 0.0;
    readNumber(j, key, raw, defaultValue);
    *target = static_cast<float>(raw);
}

void applyNumber(const json& j, const char* key, double defaultValue,
                 double* target)
{
    double raw = 0.0;
    readNumber(j, key, raw, defaultValue);
    *target = raw;
}

void applyNumber(const json& j, const char* key, double defaultValue,
                 int* target, int minValue, int maxValue)
{
    double raw = 0.0;
    readNumber(j, key, raw, defaultValue);
    int value = static_cast<int>(raw + (raw >= 0 ? 0.5 : -0.5));
    if (value < minValue) {
        std::fprintf(stderr, "config: '%s'=%d below minimum %d, clamped\n",
                     key, value, minValue);
        value = minValue;
    }
    if (value > maxValue) {
        std::fprintf(stderr, "config: '%s'=%d above maximum %d, clamped\n",
                     key, value, maxValue);
        value = maxValue;
    }
    *target = value;
}

void applyWaveRow(const json& j, const char* key,
                  GameConfig::WaveRow* row, const GameConfig::WaveRow& def)
{
    if (!j.contains(key)) {
        *row = def;
        return;
    }
    const auto& v = j.at(key);
    if (!v.is_object()) {
        std::fprintf(stderr, "config: '%s' is not an object, using default\n",
                     key);
        *row = def;
        return;
    }
    // Structural caps live HERE (docs/test_plan.md Stage 21): attackers
    // 1..4, shots 1..2, interval floored at 1 s.
    applyNumber(v, "max_attackers", def.maxAttackers, &row->maxAttackers, 1,
                4);
    applyNumber(v, "interval", def.intervalSeconds, &row->intervalSeconds);
    if (row->intervalSeconds < 1.0) {
        row->intervalSeconds = 1.0;
    }
    applyNumber(v, "shots", def.shotsPerAttack, &row->shotsPerAttack, 1, 2);
}

}  // namespace

const GameConfig& GameConfig::get()
{
    return gConfig;
}

void GameConfig::set(const GameConfig& config)
{
    gConfig = config;
    gConfig.clampToBounds();
}

bool GameConfig::loadFromFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in.good()) {
        std::fprintf(stderr,
                     "config: cannot open %s -- using documented defaults\n",
                     path.c_str());
        return false;
    }

    json parsed;
    try {
        in >> parsed;
    } catch (const json::exception&) {
        std::fprintf(stderr,
                     "config: %s is not valid JSON -- using documented "
                     "defaults\n",
                     path.c_str());
        return false;
    }
    if (!parsed.is_object()) {
        std::fprintf(stderr,
                     "config: %s root is not a JSON object -- using "
                     "documented defaults\n",
                     path.c_str());
        return false;
    }

    const GameConfig def;  // pristine documented defaults for fallbacks

    // Player.
    applyNumber(parsed, "player_speed", def.playerSpeed, &playerSpeed);
    applyNumber(parsed, "initial_lives", def.initialLives, &initialLives, 1,
                9);
    applyNumber(parsed, "player_bullet_speed", def.playerBulletSpeed,
                &playerBulletSpeed);
    applyNumber(parsed, "fire_cooldown_seconds", def.fireCooldownSeconds,
                &fireCooldownSeconds);
    applyNumber(parsed, "max_player_projectiles", def.maxPlayerProjectiles,
                &maxPlayerProjectiles, 1, 5);

    // Enemy types.
    applyNumber(parsed, "score_scout", def.scoutPoints, &scoutPoints, 1,
                100000);
    applyNumber(parsed, "score_guard", def.guardPoints, &guardPoints, 1,
                100000);
    applyNumber(parsed, "score_commander", def.commanderPoints,
                &commanderPoints, 1, 100000);
    applyNumber(parsed, "scout_dive_speed", def.scoutDiveSpeed,
                &scoutDiveSpeed);
    applyNumber(parsed, "guard_dive_speed", def.guardDiveSpeed,
                &guardDiveSpeed);
    applyNumber(parsed, "commander_dive_speed", def.commanderDiveSpeed,
                &commanderDiveSpeed);

    // Enemy bullets.
    applyNumber(parsed, "enemy_bullet_base_speed", def.enemyBulletBaseSpeed,
                &enemyBulletBaseSpeed);
    applyNumber(parsed, "enemy_bullet_ramp_per_wave",
                def.enemyBulletRampPerWave, &enemyBulletRampPerWave);
    applyNumber(parsed, "enemy_bullet_max_speed", def.enemyBulletMaxSpeed,
                &enemyBulletMaxSpeed);

    // Formation motion.
    applyNumber(parsed, "formation_swing_px", def.oscillationSwingPx,
                &oscillationSwingPx);
    applyNumber(parsed, "formation_period_seconds",
                def.oscillationPeriodSeconds, &oscillationPeriodSeconds);
    applyNumber(parsed, "formation_max_speed_multiplier",
                def.oscillationMaxMultiplier, &oscillationMaxMultiplier);

    // Attack pacing table.
    applyWaveRow(parsed, "wave_1", &waves[0], def.waves[0]);
    applyWaveRow(parsed, "wave_2", &waves[1], def.waves[1]);
    applyWaveRow(parsed, "wave_3", &waves[2], def.waves[2]);
    applyWaveRow(parsed, "wave_4", &waves[3], def.waves[3]);
    applyWaveRow(parsed, "wave_5_to_6", &waves[4], def.waves[4]);
    applyWaveRow(parsed, "wave_7_plus", &waves[5], def.waves[5]);

    // Wave system / effects / lifecycle.
    applyNumber(parsed, "interstitial_seconds", def.interstitialSeconds,
                &interstitialSeconds);
    applyNumber(parsed, "explosion_seconds", def.explosionSeconds,
                &explosionSeconds);
    applyNumber(parsed, "respawn_delay_seconds", def.respawnDelaySeconds,
                &respawnDelaySeconds);
    applyNumber(parsed, "invulnerable_seconds", def.invulnerableSeconds,
                &invulnerableSeconds);

    clampToBounds();
    return true;
}

void GameConfig::clampToBounds()
{
    // Speeds must be positive and sane; structural bounds mirror spec §7.
    auto floorSpeed = [](float& s) {
        if (s < 10.0f) s = 10.0f;
        if (s > 5000.0f) s = 5000.0f;
    };
    floorSpeed(playerSpeed);
    floorSpeed(playerBulletSpeed);
    floorSpeed(scoutDiveSpeed);
    floorSpeed(guardDiveSpeed);
    floorSpeed(commanderDiveSpeed);
    floorSpeed(enemyBulletBaseSpeed);
    floorSpeed(enemyBulletRampPerWave);
    floorSpeed(enemyBulletMaxSpeed);

    if (enemyBulletMaxSpeed > 720.0f) enemyBulletMaxSpeed = 720.0f;

    for (WaveRow& r : waves) {
        if (r.intervalSeconds < 1.0) r.intervalSeconds = 1.0;
    }

    if (oscillationSwingPx < 8.0f) oscillationSwingPx = 8.0f;
    if (oscillationPeriodSeconds < 1.0) oscillationPeriodSeconds = 1.0;
    if (oscillationMaxMultiplier < 1.0) oscillationMaxMultiplier = 1.0;
    if (oscillationMaxMultiplier > 5.0) oscillationMaxMultiplier = 5.0;

    if (fireCooldownSeconds < 0.05) fireCooldownSeconds = 0.05;
    if (explosionSeconds < 0.05) explosionSeconds = 0.05;
    if (interstitialSeconds < 0.25) interstitialSeconds = 0.25;
    if (respawnDelaySeconds < 0.25) respawnDelaySeconds = 0.25;
    if (invulnerableSeconds < 0.0) invulnerableSeconds = 0.0;
}

}  // namespace galaxian
