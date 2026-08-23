#include "core/Game.hpp"
#include "core/GameConfig.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    galaxian::Game game;

    // Test hooks (see docs/test_plan.md):
    //   --smoke <frames>     exit after N rendered frames
    //   --smoke-time <secs>  exit after N seconds of wall time
    //   --no-vsync           render without vertical sync
    //   --config <path>      balance config (default assets/config/game.json)
    bool vsync = true;
    std::string configPath = "assets/config/game.json";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke" && i + 1 < argc) {
            const int frames = std::atoi(argv[++i]);
            if (frames > 0) {
                game.setSmokeFrames(frames);
            }
        } else if (arg == "--smoke-time" && i + 1 < argc) {
            const double seconds = std::atof(argv[++i]);
            if (seconds > 0.0) {
                game.setSmokeSeconds(seconds);
            }
        } else if (arg == "--no-vsync") {
            vsync = false;
        } else if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    game.setVsync(vsync);

    // Stage 21: load the balance configuration BEFORE anything initializes,
    // so every system reads the configured values from the first step.
    galaxian::GameConfig config;
    if (!config.loadFromFile(configPath)) {
        std::printf("config: using documented defaults\n");
    }
    galaxian::GameConfig::set(config);

    if (!game.initialize()) {
        return 1;
    }

    // Headless smoke runs skip the title screen so the simulation is
    // observable from the first frame (a composition-root decision; the
    // Game class itself always boots to Title).
    if (game.inSmokeMode() &&
        game.state() == galaxian::GameStateId::Title) {
        game.changeState(galaxian::GameStateId::Playing);
    }

    game.run();
    game.shutdown();

    if (game.inSmokeMode()) {
        std::printf("smoke: frames=%d updates=%d sim_time=%.3fs "
                    "wall_time=%.3fs\n",
                    game.smokeResultFrames(), game.smokeResultUpdates(),
                    game.smokeResultSimTime(), game.smokeResultWallTime());
    }

    return 0;
}
