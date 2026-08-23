// Stage 21 data-driven configuration tests (docs/test_plan.md, Stage 21).
//
// GameConfig is SDL-free pure data plus a tolerant loader. Some tests
// mutate the GLOBAL config; each such test restores the pristine defaults
// in its guard's destructor so later cases are unaffected.
//
// The documented defaults equal the previously frozen constants
// (player 220 px/s, lives 3, cooldown 0.35 s, Scout/Guard/Commander
// 50/80/150 pts, dive speeds 140/100/70, spec §7 wave rows).

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <string>

#include "core/GameConfig.hpp"
#include "gameplay/AttackDirector.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Projectile.hpp"
#include "gameplay/ScoreManager.hpp"

using namespace galaxian;

namespace {

constexpr double kDt = kFixedDeltaSeconds;

struct ConfigGuard {
    ~ConfigGuard() { GameConfig::set(GameConfig{}); }
};

bool writeFile(const std::string& path, const std::string& content)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
    return true;
}

}  // namespace

TEST_CASE("config: documented defaults match the frozen constants",
          "[config]")
{
    const GameConfig def;
    CHECK(def.playerSpeed == 220.0f);
    CHECK(def.initialLives == 3);
    CHECK(def.playerBulletSpeed == 480.0f);
    CHECK(def.fireCooldownSeconds == Catch::Approx(0.35).margin(1e-12));
    CHECK(def.maxPlayerProjectiles == 2);

    CHECK(def.scoutPoints == 50);
    CHECK(def.guardPoints == 80);
    CHECK(def.commanderPoints == 150);
    CHECK(def.scoutDiveSpeed == 140.0f);
    CHECK(def.guardDiveSpeed == 100.0f);
    CHECK(def.commanderDiveSpeed == 70.0f);

    CHECK(def.enemyBulletBaseSpeed == 240.0f);
    CHECK(def.enemyBulletRampPerWave == 40.0f);
    CHECK(def.enemyBulletMaxSpeed == 360.0f);

    CHECK(def.oscillationSwingPx == 64.0f);
    CHECK(def.oscillationPeriodSeconds == Catch::Approx(4.0).margin(1e-12));
    CHECK(def.oscillationMaxMultiplier == Catch::Approx(2.5).margin(1e-12));

    // Spec §7 rows.
    CHECK(def.waves[0].maxAttackers == 1);
    CHECK(def.waves[0].intervalSeconds == 6.0);
    CHECK(def.waves[1].maxAttackers == 2);
    CHECK(def.waves[3].shotsPerAttack == 2);
    CHECK(def.waves[4].maxAttackers == 3);
    CHECK(def.waves[5].maxAttackers == 4);

    CHECK(def.interstitialSeconds == 2.0);
    CHECK(def.explosionSeconds == Catch::Approx(0.25).margin(1e-12));
    CHECK(def.respawnDelaySeconds == 1.5);
    CHECK(def.invulnerableSeconds == 2.0);
}

TEST_CASE("config: missing file falls back to defaults without crashing",
          "[config]")
{
    GameConfig cfg;  // pristine defaults
    const bool ok = cfg.loadFromFile("/nonexistent/path/game.json");
    CHECK_FALSE(ok);
    // Still exactly the defaults.
    CHECK(cfg.playerSpeed == 220.0f);
    CHECK(cfg.initialLives == 3);
}

TEST_CASE("config: corrupt/truncated JSON falls back to defaults",
          "[config]")
{
    const std::string path = "/tmp/galaxian_test_corrupt.json";
    ConfigGuard guard;

    REQUIRE(writeFile(path, "{ \"player_speed\": 300,,, "));
    GameConfig cfg;
    CHECK_FALSE(cfg.loadFromFile(path));
    CHECK(cfg.playerSpeed == 220.0f);  // untouched defaults

    // Truncated mid-key.
    REQUIRE(writeFile(path, "{ \"scout_points\": "));
    CHECK_FALSE(cfg.loadFromFile(path));
    CHECK(cfg.scoutPoints == 50);

    // Root not an object.
    REQUIRE(writeFile(path, "[1, 2, 3]"));
    CHECK_FALSE(cfg.loadFromFile(path));

    std::remove(path.c_str());
}

TEST_CASE("config: a valid override loads and DRIVES gameplay "
          "(no recompile)",
          "[config]")
{
    const std::string path = "/tmp/galaxian_test_valid.json";
    ConfigGuard guard;

    REQUIRE(writeFile(path,
                      "{\n"
                      "  \"player_speed\": 400,\n"
                      "  \"score_scout\": 500,\n"
                      "  \"wave_1\": { \"interval\": 2.5 },\n"
                      "  \"unknown_future_key\": 42\n"
                      "}\n"));
    GameConfig cfg;
    REQUIRE(cfg.loadFromFile(path));
    CHECK(cfg.playerSpeed == 400.0f);
    CHECK(cfg.scoutPoints == 500);
    CHECK(cfg.waves[0].intervalSeconds == Catch::Approx(2.5).margin(1e-12));
    CHECK(cfg.initialLives == 3);  // untouched keys keep defaults

    // ---- Behavior change through the global config ----
    GameConfig::set(cfg);

    // The player now moves at 400 px/s: clearly farther than before.
    Player fast;
    for (int i = 0; i < 30; ++i) {
        fast.update(kDt, +1.0f);
    }
    const float distFast = fast.position().x - Player::kStartPosition.x;
    CHECK(distFast > 150.0f);  // ~400 px/s x 0.5 s = 200 px

    // Score values follow the config too.
    ScoreManager score;
    score.addKill(EnemyType::Scout);
    CHECK(score.score() == 500);

    // Attack pacing follows the wave row.
    CHECK(waveParams(1).attackIntervalSeconds ==
          Catch::Approx(2.5).margin(1e-12));
}

TEST_CASE("config: structural clamps hold for out-of-range values",
          "[config]")
{
    const std::string path = "/tmp/galaxian_test_clamp.json";
    ConfigGuard guard;

    REQUIRE(writeFile(path,
                      "{\n"
                      "  \"initial_lives\": 99,\n"
                      "  \"wave_7_plus\": { \"max_attackers\": 9, "
                      "\"shots\": 7 },\n"
                      "  \"enemy_bullet_max_speed\": 99999\n"
                      "}\n"));
    GameConfig cfg;
    REQUIRE(cfg.loadFromFile(path));
    CHECK(cfg.initialLives == 9);              // clamped to the 1..9 range
    CHECK(cfg.waves[5].maxAttackers == 4);     // hard cap
    CHECK(cfg.waves[5].shotsPerAttack == 2);
    CHECK(cfg.enemyBulletMaxSpeed == 720.0f);  // loader sanity cap

    // Negative speeds are floored by clampToBounds on set().
    GameConfig wild;
    wild.playerSpeed = -5.0f;
    GameConfig::set(wild);
    CHECK(GameConfig::get().playerSpeed >= 10.0f);
}
