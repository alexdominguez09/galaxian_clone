#pragma once

#include <SDL2/SDL.h>

namespace galaxian {

// RAII wrapper around an SDL_Texture (move-only).
//
// Textures are normally obtained from the Renderer's cache
// (Renderer::texture) so each source is loaded/created once.
class Texture {
public:
    Texture() = default;
    explicit Texture(SDL_Texture* texture) : texture_(texture) {}
    ~Texture();

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    bool valid() const { return texture_ != nullptr; }
    int width() const;
    int height() const;

    // Raw SDL handle (for SDL_RenderCopy inside the Renderer).
    SDL_Texture* handle() const { return texture_; }

private:
    SDL_Texture* texture_ = nullptr;
};

}  // namespace galaxian
