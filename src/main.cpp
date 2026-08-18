#include "Game.hpp"

#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    galaxian::Game game;

    // Test hook (see docs/test_plan.md): `--smoke <frames>` makes the game
    // exit on its own after the given number of frames. Used for headless
    // smoke runs: SDL_VIDEODRIVER=dummy ./build/galaxian --smoke 120
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke" && i + 1 < argc) {
            const int frames = std::atoi(argv[++i]);
            if (frames > 0) {
                game.setSmokeFrames(frames);
            }
        }
    }

    if (!game.initialize()) {
        return 1;
    }

    game.run();
    game.shutdown();

    return 0;
}
