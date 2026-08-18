#include "Game.hpp"

#include <cstdio>

namespace galaxian {

Game::~Game() { shutdown(); }

bool Game::initialize()
{
    if (initialized_) {
        return true;
    }

    if (!renderer_.initialize(kLogicalWidth, kLogicalHeight, vsync_)) {
        return false;
    }
    if (!devScene_.initialize(renderer_)) {
        std::fprintf(stderr, "galaxian: dev scene initialization failed\n");
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void Game::run()
{
    if (!initialized_) {
        return;
    }

    clock_.start();
    running_ = true;
    while (running_) {
        processEvents();

        // Fixed-timestep loop (docs/architecture.md §3.1):
        //   acc += frameDelta; while (acc >= dt) fixedUpdate(dt);
        const double frameDelta = clock_.frameDelta();
        lastFrameSeconds_ = frameDelta;

        const int steps = timestep_.advance(frameDelta);
        for (int i = 0; i < steps; ++i) {
            fixedUpdate(timestep_.dt());
        }
        updatesSinceReport_ += steps;

        render();
        ++frameCount_;
        ++framesSinceReport_;

        if (smokeFrames_ > 0 && frameCount_ >= smokeFrames_) {
            running_ = false;
        }
        if (smokeSeconds_ > 0.0 && clock_.elapsed() >= smokeSeconds_) {
            running_ = false;
        }

        reportStats();
    }
    wallTime_ = clock_.elapsed();
    clock_.stop();
}

void Game::processEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
        case SDL_QUIT:
            running_ = false;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                running_ = false;
                break;
            case SDLK_F2:
                statsEnabled_ = !statsEnabled_;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

void Game::fixedUpdate(double dt)
{
    // No gameplay yet (Stage 2). Simulation logic lands in Stage 5+.
    // These counters prove the loop runs a deterministic number of steps:
    // simTime_ must equal updateCount_ * dt at all times.
    (void)dt;
    ++updateCount_;
    simTime_ += dt;
}

void Game::render()
{
    // Stage 3 test scene (docs/test_plan.md §1): player sprite, 10 enemy
    // sprites, text, projectile rectangles, screen border.
    devScene_.draw(renderer_);

    if (statsEnabled_) {
        char line[64];
        std::snprintf(line, sizeof(line), "FPS: %.1f  UPD/S: %.1f", fps_,
                      updatesPerSecond_);
        renderer_.drawText(line, {16.0f, 480.0f}, colors::kGreen);
        std::snprintf(line, sizeof(line), "SIM: %.3f s  STEP: %.1f ms",
                      simTime_, timestep_.dt() * 1000.0);
        renderer_.drawText(line, {16.0f, 496.0f}, colors::kGreen);
    }

    renderer_.present();
}

void Game::reportStats()
{
    // Debug overlay. On-screen text when F2 is toggled (Stage 3); the
    // console line is kept for smoke runs, where it is enabled
    // automatically so headless runs are observable.
    const double now = clock_.elapsed();
    if (now - lastReportSeconds_ < 1.0) {
        return;
    }

    const double window = now - lastReportSeconds_;
    fps_ = static_cast<double>(framesSinceReport_) / window;
    updatesPerSecond_ = static_cast<double>(updatesSinceReport_) / window;

    if (statsEnabled_ || inSmokeMode()) {
        std::fprintf(stderr,
                     "[stats] fps=%.1f frame=%.2fms updates/s=%.0f "
                     "sim_time=%.1fs entities=%d\n",
                     fps_, lastFrameSeconds_ * 1000.0, updatesPerSecond_,
                     simTime_, 0 /* entity count; no gameplay objects yet */);
    }

    lastReportSeconds_ = now;
    framesSinceReport_ = 0;
    updatesSinceReport_ = 0;
}

void Game::shutdown()
{
    renderer_.shutdown();
    initialized_ = false;
    running_ = false;
}

}  // namespace galaxian
