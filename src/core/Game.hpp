#pragma once

#include <SDL2/SDL.h>

#include "Constants.hpp"
#include "GameClock.hpp"
#include "TimestepController.hpp"
#include "graphics/DevScene.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputManager.hpp"

namespace galaxian {

// Game — top-level application object.
//
// Owns: initialization, main loop, event processing, update, rendering,
// shutdown. Stage 2 provides deterministic timing: the loop feeds real
// frame time into a TimestepController and runs a fixed number of 1/60 s
// simulation steps per frame (docs/architecture.md §3.1). Stage 3 routes
// all drawing through the Renderer subsystem and shows the DevScene test
// scene until gameplay rendering exists (Stage 5+). Stage 4 routes all
// keyboard input through the InputManager using named Actions: the game
// loop no longer references SDL key constants (docs/architecture.md §3.3).
class Game {
public:
    Game() = default;
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    // SDL init + window + renderer + font + dev scene. Returns false (with
    // a diagnostic on stderr) if any of them fail.
    bool initialize();

    // Main loop: processEvents -> fixed updates -> render, until quit.
    void run();

    // Releases all resources. Safe to call more than once, and safe to call
    // if initialize() failed.
    void shutdown();

    // Render with/without vertical sync (test hook for refresh-rate
    // independence runs; see docs/test_plan.md Stage 2).
    void setVsync(bool enabled) { vsync_ = enabled; }

    // Test hooks (docs/test_plan.md): exit automatically after `frames`
    // rendered frames, or after `seconds` of wall time. A summary line is
    // printed to stdout on exit when either is active.
    void setSmokeFrames(int frames) { smokeFrames_ = frames; }
    void setSmokeSeconds(double seconds) { smokeSeconds_ = seconds; }

    bool inSmokeMode() const { return smokeFrames_ > 0 || smokeSeconds_ > 0.0; }
    int smokeResultFrames() const { return frameCount_; }
    int smokeResultUpdates() const { return updateCount_; }
    double smokeResultSimTime() const { return simTime_; }
    double smokeResultWallTime() const { return wallTime_; }

private:
    void processEvents();
    void fixedUpdate(double dt);
    void render();
    void reportStats();

    bool initialized_ = false;
    bool running_ = false;

    Renderer renderer_;
    // Stage 4: all keyboard input flows through the InputManager as named
    // Actions. Initialized after the renderer (which brings up SDL) and used
    // by processEvents()/the demo; holds no SDL resources of its own.
    InputManager input_;
    DevScene devScene_;
    bool vsync_ = true;

    GameClock clock_;
    TimestepController timestep_{kFixedDeltaSeconds, kMaxCatchUpSteps};

    // Debug stats. Toggled with F2: on-screen overlay (Stage 3 text
    // rendering) plus the console line used by smoke runs.
    bool statsEnabled_ = false;
    double lastReportSeconds_ = 0.0;
    int framesSinceReport_ = 0;
    int updatesSinceReport_ = 0;
    double lastFrameSeconds_ = 0.0;
    double fps_ = 0.0;
    double updatesPerSecond_ = 0.0;

    // Frame/update counters (diagnostics + smoke summary).
    int frameCount_ = 0;
    int updateCount_ = 0;
    double simTime_ = 0.0;
    double wallTime_ = 0.0;

    int smokeFrames_ = 0;
    double smokeSeconds_ = 0.0;
};

}  // namespace galaxian
