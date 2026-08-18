#include "Game.hpp"

#include <cstdio>

namespace galaxian {

namespace {
constexpr const char* kWindowTitle = "Galaxian Clone";
}  // namespace

Game::~Game() { shutdown(); }

bool Game::initialize()
{
    if (initialized_) {
        return true;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "galaxian: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(kWindowTitle,
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               kLogicalWidth,
                               kLogicalHeight,
                               SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        std::fprintf(stderr, "galaxian: SDL_CreateWindow failed: %s\n",
                     SDL_GetError());
        shutdown();
        return false;
    }

    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if (vsync_) {
        flags |= SDL_RENDERER_PRESENTVSYNC;
    }
    renderer_ = SDL_CreateRenderer(window_, -1, flags);
    if (renderer_ == nullptr) {
        // Fall back to software rendering (e.g. headless / dummy driver).
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer_ == nullptr) {
        std::fprintf(stderr, "galaxian: SDL_CreateRenderer failed: %s\n",
                     SDL_GetError());
        shutdown();
        return false;
    }

    // Fixed logical coordinate space; the window scales it.
    if (SDL_RenderSetLogicalSize(renderer_, kLogicalWidth, kLogicalHeight) != 0) {
        std::fprintf(stderr, "galaxian: SDL_RenderSetLogicalSize failed: %s\n",
                     SDL_GetError());
        shutdown();
        return false;
    }

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);

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
    SDL_RenderClear(renderer_);

    // Placeholder scene: a border rectangle so there is something to see.
    // Replaced by the real Renderer subsystem in Stage 3.
    SDL_SetRenderDrawColor(renderer_, 96, 96, 140, 255);
    const SDL_Rect border{0, 0, kLogicalWidth, kLogicalHeight};
    SDL_RenderDrawRect(renderer_, &border);

    SDL_RenderPresent(renderer_);
}

void Game::reportStats()
{
    // Temporary debug overlay (Stage 2): console line every second while
    // enabled. Becomes an on-screen overlay in Stage 3 (text rendering).
    // Enabled automatically in smoke mode so headless runs are observable.
    if (!statsEnabled_ && !inSmokeMode()) {
        return;
    }

    const double now = clock_.elapsed();
    if (now - lastReportSeconds_ < 1.0) {
        return;
    }

    const double window = now - lastReportSeconds_;
    const double fps = static_cast<double>(framesSinceReport_) / window;
    const double updatesPerSecond =
        static_cast<double>(updatesSinceReport_) / window;

    std::fprintf(stderr,
                 "[stats] fps=%.1f frame=%.2fms updates/s=%.0f "
                 "sim_time=%.1fs entities=%d\n",
                 fps, lastFrameSeconds_ * 1000.0, updatesPerSecond, simTime_,
                 0 /* entity count; no gameplay objects yet */);

    lastReportSeconds_ = now;
    framesSinceReport_ = 0;
    updatesSinceReport_ = 0;
}

void Game::shutdown()
{
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        SDL_Quit();
    }
    initialized_ = false;
    running_ = false;
}

}  // namespace galaxian
