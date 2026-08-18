#include "Texture.hpp"

namespace galaxian {

Texture::~Texture()
{
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
    }
}

Texture::Texture(Texture&& other) noexcept : texture_(other.texture_)
{
    other.texture_ = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
        }
        texture_ = other.texture_;
        other.texture_ = nullptr;
    }
    return *this;
}

int Texture::width() const
{
    int w = 0;
    int h = 0;
    if (texture_ != nullptr) {
        SDL_QueryTexture(texture_, nullptr, nullptr, &w, &h);
    }
    return w;
}

int Texture::height() const
{
    int w = 0;
    int h = 0;
    if (texture_ != nullptr) {
        SDL_QueryTexture(texture_, nullptr, nullptr, &w, &h);
    }
    return h;
}

}  // namespace galaxian
