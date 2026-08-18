#include "core/Game.hpp"

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
    // Headless smoke run: SDL_VIDEODRIVER=dummy ./build/galaxian --smoke 120
    bool vsync = true;
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
        }
    }

    game.setVsync(vsync);

    if (!game.initialize()) {
        return 1;
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
