#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <map>
#include <string>

#include "Texture.hpp"
#include "core/Types.hpp"

namespace galaxian {

// 2D rendering on top of SDL (docs/architecture.md §3.2).
//
// Owns the window, the SDL renderer, and the font. All drawing happens in
// logical coordinates (448x576); the window is scaled with integer scaling
// and letterboxing, so gameplay coordinates never depend on window size.
//
// This is one of the few modules allowed to know about SDL
// (docs/architecture.md §1).
class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // SDL init + window + renderer + font. Returns false (with diagnostics
    // on stderr) on failure. Font failure degrades gracefully: text is
    // disabled but everything else works.
    bool initialize(int windowWidth, int windowHeight, bool vsync);
    void shutdown();
    bool initialized() const { return initialized_; }

    // Texture lookup by cache id. Pre-registered ids (dev art) resolve
    // immediately; anything else is loaded from disk (BMP/PNG/... via
    // SDL_image) and cached. Returns nullptr on failure.
    const Texture* texture(const std::string& idOrPath);

    // Pre-register a texture under an id (used by DevArt).
    void registerTexture(const std::string& id, Texture texture);

    void clear(const Color& color);
    void drawRect(const Rect& rect, const Color& color);
    void drawFilledRect(const Rect& rect, const Color& color);
    void drawSprite(const Texture& sprite, const Vector2& position,
                    bool flipH = false, bool flipV = false);

    // Bitmap-free text via SDL_ttf. `fontSize` is in logical pixels.
    // No-op if no font could be loaded.
    void drawText(const std::string& text, const Vector2& position,
                  const Color& color, int fontSize = 16);

    // Width of `text` at `fontSize` in logical pixels (for centering text,
    // e.g. title prompts). Returns 0 if no font is loaded or measurement
    // fails. `fontSize` defaults to the base 16 px size.
    int textWidth(const std::string& text, int fontSize = 16);

    void present();

    int logicalWidth() const { return logicalWidth_; }
    int logicalHeight() const { return logicalHeight_; }
    SDL_Window* window() const { return window_; }
    SDL_Renderer* sdlRenderer() const { return renderer_; }

    // Current integer scale factor and content offset (pixels). Recomputed
    // from the live window size on every clear(), so resizing "just works".
    int scale() const { return scale_; }
    int offsetX() const { return offsetX_; }
    int offsetY() const { return offsetY_; }

private:
    // Recomputes scale_/offsetX_/offsetY_ from the current window size:
    // the largest integer scale that fits the logical resolution, centered.
    void updateTransform();

    // Maps a logical rect to physical window pixels (scale + offset).
    SDL_Rect toPhysical(const Rect& logical) const;

    const Texture* cachedText(const std::string& key, TTF_Font* font,
                              const std::string& text, const Color& color);
    TTF_Font* fontForSize(int fontSize);
    static Texture makeTexture(SDL_Renderer* renderer, SDL_Surface* surface);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;  // base font (kBaseFontSize)
    std::string fontPath_;
    std::map<int, TTF_Font*> fontsBySize_;
    bool initialized_ = false;
    int logicalWidth_ = 0;
    int logicalHeight_ = 0;

    // Manual centered integer scaling (SDL's logical size is not honored
    // correctly by the software renderer, so we transform in draw calls).
    int scale_ = 1;
    int offsetX_ = 0;
    int offsetY_ = 0;

    std::map<std::string, Texture> textures_;
    std::map<std::string, Texture> textCache_;

    static constexpr int kBaseFontSize = 16;
};

}  // namespace galaxian
