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

    renderer_ =
        SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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

    running_ = true;
    while (running_) {
        processEvents();
        update();
        render();
    }
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
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running_ = false;
            }
            break;
        default:
            break;
        }
    }
}

void Game::update()
{
    // No gameplay yet (Stage 1). Timing/fixed-step logic arrives in Stage 2.
    if (smokeFrames_ > 0) {
        if (++frameCount_ >= smokeFrames_) {
            running_ = false;
        }
    }
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
