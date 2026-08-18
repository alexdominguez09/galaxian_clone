#include "DevArt.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>

#include "core/Types.hpp"

namespace galaxian {
namespace DevArt {

namespace {

SDL_Surface* makeSurface(int w, int h)
{
    // 32-bit ARGB, fully transparent initially.
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface != nullptr) {
        SDL_FillRect(surface, nullptr, 0x00000000);
    }
    return surface;
}

void setPixel(SDL_Surface* surface, int x, int y, const Color& c)
{
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
        return;
    }
    const std::uint32_t argb =
        (static_cast<std::uint32_t>(c.a) << 24) |
        (static_cast<std::uint32_t>(c.r) << 16) |
        (static_cast<std::uint32_t>(c.g) << 8) |
        static_cast<std::uint32_t>(c.b);
    std::uint32_t* pixel =
        static_cast<std::uint32_t*>(surface->pixels) + y * (surface->pitch / 4) + x;
    *pixel = argb;
}

// Filled triangle by scanline interpolation (apex on top).
void fillTriangle(SDL_Surface* surface,
                  const Vector2& apex,
                  const Vector2& bottomLeft,
                  const Vector2& bottomRight,
                  const Color& color)
{
    for (int y = 0; y < surface->h; ++y) {
        const float t = (static_cast<float>(y) + 0.5f) / static_cast<float>(surface->h);
        const float leftX = apex.x + (bottomLeft.x - apex.x) * t;
        const float rightX = apex.x + (bottomRight.x - apex.x) * t;
        for (int x = 0; x < surface->w; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            if (px >= leftX && px <= rightX) {
                setPixel(surface, x, y, color);
            }
        }
    }
}

// Filled square with a 1px darker border.
void fillSquare(SDL_Surface* surface, const Color& fill, const Color& border)
{
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            const bool edge =
                (x == 0) || (y == 0) || (x == surface->w - 1) || (y == surface->h - 1);
            setPixel(surface, x, y, edge ? border : fill);
        }
    }
}

Texture toTexture(SDL_Renderer* renderer, SDL_Surface* surface)
{
    Texture result;
    if (surface != nullptr) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture != nullptr) {
            // Pixel-perfect when drawn scaled (Renderer::drawSprite).
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
            result = Texture(texture);
        }
        SDL_FreeSurface(surface);
    } else {
        std::fprintf(stderr, "galaxian: dev art: surface creation failed: %s\n",
                     SDL_GetError());
    }
    return result;
}

}  // namespace

bool createAll(Renderer& renderer)
{
    if (!renderer.initialized()) {
        return false;
    }

    // Player: 24x16 triangle.
    {
        SDL_Surface* s = makeSurface(24, 16);
        fillTriangle(s, {12.0f, 0.5f}, {0.5f, 15.5f}, {23.5f, 15.5f},
                     colors::kPlayerCyan);
        renderer.registerTexture(kPlayer, toTexture(renderer.sdlRenderer(), s));
    }

    // Enemies: 24x24 squares, one color per type.
    {
        SDL_Surface* s = makeSurface(24, 24);
        fillSquare(s, colors::kEnemyRed, colors::kBlack);
        renderer.registerTexture(kEnemyCommander, toTexture(renderer.sdlRenderer(), s));
    }
    {
        SDL_Surface* s = makeSurface(24, 24);
        fillSquare(s, colors::kEnemyYellow, colors::kBlack);
        renderer.registerTexture(kEnemyGuard, toTexture(renderer.sdlRenderer(), s));
    }
    {
        SDL_Surface* s = makeSurface(24, 24);
        fillSquare(s, colors::kEnemyGreen, colors::kBlack);
        renderer.registerTexture(kEnemyScout, toTexture(renderer.sdlRenderer(), s));
    }

    // Bullet: 4x10 rectangle.
    {
        SDL_Surface* s = makeSurface(4, 10);
        fillSquare(s, colors::kBullet, colors::kBullet);
        renderer.registerTexture(kBullet, toTexture(renderer.sdlRenderer(), s));
    }

    return true;
}

}  // namespace DevArt
}  // namespace galaxian
