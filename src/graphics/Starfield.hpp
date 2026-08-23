#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"

namespace galaxian {
namespace graphics {

// Layered scrolling starfield (Stage 24, purely cosmetic). Three depth
// layers of deterministic pseudo-random stars drifting downward; the
// positions wrap, so it runs forever without state growth. Updated with
// the frame delta (visual-only clock) and drawn BEHIND gameplay.
class Starfield {
public:
    struct Star {
        float x = 0.0f;
        float y = 0.0f;
        int layer = 0;      // 0 far .. 2 near
    };

    Starfield();

    // Advances the drift by `dt` seconds (any positive value; visual only).
    void update(double dt);

    // Draws every star as a single logical pixel in its layer's shade.
    void draw(Renderer& renderer) const;

private:
    static constexpr int kStarsPerLayer = 26;
    static constexpr float kLayerSpeed[3] = {18.0f, 42.0f, 90.0f};
    static constexpr Color kLayerColor[3] = {
        Color{70, 70, 110},     // far: dim blue-gray
        Color{140, 140, 170},   // mid
        Color{230, 230, 255},   // near: bright white-blue
    };

    Star stars_[3 * kStarsPerLayer];
};

}  // namespace graphics
}  // namespace galaxian
