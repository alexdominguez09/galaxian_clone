#pragma once

// Game — top-level application object (Stage 1).
//
// Owns: initialization, main loop, event processing, update, rendering,
// shutdown. Subsystems (renderer, input, audio, gameplay) are attached here
// as later stages land; for now the class drives an empty scene.

#include <SDL2/SDL.h>

namespace galaxian {

// Logical (gameplay) resolution. The window may be any size; the renderer
// scales this fixed coordinate space (see docs/game_spec.md §3).
inline constexpr int kLogicalWidth = 448;
inline constexpr int kLogicalHeight = 576;

class Game {
public:
    Game() = default;
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    // SDL init + window + renderer. Returns false (with a diagnostic on
    // stderr) if any of them fail.
    bool initialize();

    // Main loop: processEvents -> update -> render, until quit.
    void run();

    // Releases all resources. Safe to call more than once, and safe to call
    // if initialize() failed.
    void shutdown();

    // Test hook: exit automatically after `frames` rendered frames.
    // Used for headless smoke runs (see docs/test_plan.md, Stage 1).
    void setSmokeFrames(int frames) { smokeFrames_ = frames; }

private:
    void processEvents();
    void update();
    void render();

    bool initialized_ = false;
    bool running_ = false;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;

    int smokeFrames_ = 0;
    int frameCount_ = 0;
};

}  // namespace galaxian
