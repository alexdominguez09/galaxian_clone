#include "Renderer.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include "core/Constants.hpp"

namespace galaxian {

namespace {

// Candidate TTF fonts (DejaVu ships on Ubuntu/Debian).
constexpr const char* kFontPaths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
};

SDL_Color toSdlColor(const Color& c)
{
    return SDL_Color{c.r, c.g, c.b, c.a};
}

std::string textCacheKey(const std::string& text, const Color& color, int fontSize)
{
    std::string key;
    key.reserve(text.size() + 16);
    key += text;
    key.push_back('\0');
    key += std::to_string(static_cast<int>(color.r));
    key.push_back(',');
    key += std::to_string(static_cast<int>(color.g));
    key.push_back(',');
    key += std::to_string(static_cast<int>(color.b));
    key.push_back(',');
    key += std::to_string(static_cast<int>(color.a));
    key.push_back(',');
    key += std::to_string(fontSize);
    return key;
}

}  // namespace

Renderer::~Renderer() { shutdown(); }

bool Renderer::initialize(int windowWidth, int windowHeight, bool vsync)
{
    if (initialized_) {
        return true;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "galaxian: SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow("Galaxian Clone",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               windowWidth,
                               windowHeight,
                               SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        std::fprintf(stderr, "galaxian: SDL_CreateWindow failed: %s\n",
                     SDL_GetError());
        shutdown();
        return false;
    }

    Uint32 flags = SDL_RENDERER_ACCELERATED;
    if (vsync) {
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

    logicalWidth_ = kLogicalWidth;
    logicalHeight_ = kLogicalHeight;
    // Integer scaling + letterboxing is applied manually in the draw calls
    // (updateTransform + toPhysical): SDL's own logical-size transform is
    // not honored correctly by the software renderer (top-left anchor,
    // transparent bars, blank output when the window is smaller than the
    // logical size). Window size never changes gameplay coordinates
    // (docs/game_spec.md §3).
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    updateTransform();

    // Font: degrade gracefully if none can be loaded.
    if (TTF_Init() == 0) {
        for (const char* path : kFontPaths) {
            font_ = TTF_OpenFont(path, kBaseFontSize);
            if (font_ != nullptr) {
                fontPath_ = path;
                break;
            }
        }
        if (font_ == nullptr) {
            std::fprintf(stderr,
                         "galaxian: warning: no TTF font found, text disabled\n");
        }
    } else {
        std::fprintf(stderr, "galaxian: TTF_Init failed: %s\n", TTF_GetError());
    }

    initialized_ = true;
    return true;
}

void Renderer::shutdown()
{
    textCache_.clear();
    textures_.clear();
    for (auto& [size, font] : fontsBySize_) {
        TTF_CloseFont(font);
    }
    fontsBySize_.clear();
    if (font_ != nullptr) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
    fontPath_.clear();
    if (TTF_WasInit()) {
        TTF_Quit();
    }
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
}

void Renderer::registerTexture(const std::string& id, Texture texture)
{
    textures_[id] = std::move(texture);
}

const Texture* Renderer::texture(const std::string& idOrPath)
{
    auto it = textures_.find(idOrPath);
    if (it != textures_.end()) {
        return &it->second;
    }

    if (!initialized_) {
        return nullptr;
    }

    SDL_Surface* surface = IMG_Load(idOrPath.c_str());
    if (surface == nullptr) {
        std::fprintf(stderr, "galaxian: failed to load texture '%s': %s\n",
                     idOrPath.c_str(), IMG_GetError());
        return nullptr;
    }
    // Normalize to a known format for predictable pixel behavior.
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    SDL_Surface* converted =
        format != nullptr ? SDL_ConvertSurface(surface, format, 0) : nullptr;
    if (format != nullptr) {
        SDL_FreeFormat(format);
    }
    SDL_FreeSurface(surface);
    if (converted == nullptr) {
        std::fprintf(stderr, "galaxian: texture conversion failed for '%s': %s\n",
                     idOrPath.c_str(), SDL_GetError());
        return nullptr;
    }

    auto [inserted, ok] =
        textures_.emplace(idOrPath, makeTexture(renderer_, converted));
    SDL_FreeSurface(converted);
    if (!inserted->second.valid()) {
        textures_.erase(inserted);
        std::fprintf(stderr, "galaxian: failed to create texture for '%s': %s\n",
                     idOrPath.c_str(), SDL_GetError());
        return nullptr;
    }
    return &inserted->second;
}

Texture Renderer::makeTexture(SDL_Renderer* renderer, SDL_Surface* surface)
{
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != nullptr) {
        // Pixel-perfect: nearest-neighbor when a sprite is drawn scaled.
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
    }
    return Texture(texture);
}

void Renderer::updateTransform()
{
    int winW = 0;
    int winH = 0;
    SDL_GetRendererOutputSize(renderer_, &winW, &winH);

    scale_ = 1;
    if (winW > 0 && winH > 0) {
        scale_ = std::min(winW / logicalWidth_, winH / logicalHeight_);
        if (scale_ < 1) {
            scale_ = 1;  // window smaller than logical: scale 1, centered clip
        }
    }
    const int contentW = logicalWidth_ * scale_;
    const int contentH = logicalHeight_ * scale_;
    offsetX_ = (winW - contentW) / 2;
    offsetY_ = (winH - contentH) / 2;
}

SDL_Rect Renderer::toPhysical(const Rect& logical) const
{
    return SDL_Rect{offsetX_ + static_cast<int>(logical.x) * scale_,
                    offsetY_ + static_cast<int>(logical.y) * scale_,
                    static_cast<int>(logical.width) * scale_,
                    static_cast<int>(logical.height) * scale_};
}

void Renderer::clear(const Color& color)
{
    if (!initialized_) {
        return;
    }
    // The window may have been resized since the last frame; recompute the
    // transform so the content stays centered at an integer scale.
    updateTransform();

    // No SDL viewport/scale is set, so this fills the entire window,
    // including the letterbox bars.
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderClear(renderer_);
}

void Renderer::drawRect(const Rect& rect, const Color& color)
{
    if (!initialized_) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    const SDL_Rect physical = toPhysical(rect);
    SDL_RenderDrawRect(renderer_, &physical);
}

void Renderer::drawFilledRect(const Rect& rect, const Color& color)
{
    if (!initialized_) {
        return;
    }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    const SDL_Rect physical = toPhysical(rect);
    SDL_RenderFillRect(renderer_, &physical);
}

void Renderer::drawSprite(const Texture& sprite, const Vector2& position,
                          bool flipH, bool flipV)
{
    if (!initialized_ || !sprite.valid()) {
        return;
    }
    SDL_Rect dst{offsetX_ + static_cast<int>(position.x) * scale_,
                 offsetY_ + static_cast<int>(position.y) * scale_,
                 sprite.width() * scale_, sprite.height() * scale_};
    SDL_RendererFlip flags = SDL_FLIP_NONE;
    if (flipH) {
        flags = static_cast<SDL_RendererFlip>(flags | SDL_FLIP_HORIZONTAL);
    }
    if (flipV) {
        flags = static_cast<SDL_RendererFlip>(flags | SDL_FLIP_VERTICAL);
    }
    SDL_RenderCopyEx(renderer_, sprite.handle(), nullptr, &dst, 0.0, nullptr, flags);
}

const Texture* Renderer::cachedText(const std::string& key, TTF_Font* font,
                                    const std::string& text, const Color& color)
{
    auto it = textCache_.find(key);
    if (it != textCache_.end()) {
        return &it->second;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), toSdlColor(color));
    if (surface == nullptr) {
        return nullptr;
    }
    Texture texture = makeTexture(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!texture.valid()) {
        return nullptr;
    }

    auto [inserted, ok] = textCache_.emplace(key, std::move(texture));
    (void)ok;
    return &inserted->second;
}

TTF_Font* Renderer::fontForSize(int fontSize)
{
    if (fontSize == kBaseFontSize) {
        return font_;
    }
    auto it = fontsBySize_.find(fontSize);
    if (it != fontsBySize_.end()) {
        return it->second;
    }
    TTF_Font* font = TTF_OpenFont(fontPath_.c_str(), fontSize);
    if (font == nullptr) {
        std::fprintf(stderr, "galaxian: TTF_OpenFont(%s, %d) failed: %s\n",
                     fontPath_.c_str(), fontSize, TTF_GetError());
        return nullptr;
    }
    fontsBySize_.emplace(fontSize, font);
    return font;
}

void Renderer::drawText(const std::string& text, const Vector2& position,
                        const Color& color, int fontSize)
{
    if (!initialized_ || font_ == nullptr || text.empty() || fontSize < 8) {
        return;
    }

    TTF_Font* font = fontForSize(fontSize);
    if (font == nullptr) {
        return;
    }

    const std::string key = textCacheKey(text, color, fontSize);
    const Texture* texture = cachedText(key, font, text, color);
    if (texture == nullptr) {
        return;
    }

    SDL_Rect dst{offsetX_ + static_cast<int>(position.x) * scale_,
                 offsetY_ + static_cast<int>(position.y) * scale_,
                 texture->width() * scale_, texture->height() * scale_};
    SDL_RenderCopy(renderer_, texture->handle(), nullptr, &dst);
}

void Renderer::present()
{
    if (initialized_) {
        SDL_RenderPresent(renderer_);
    }
}

}  // namespace galaxian
