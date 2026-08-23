#include "Starfield.hpp"

#include <cstdlib>

namespace galaxian {
namespace graphics {

namespace {

float randFloat()
{
    return static_cast<float>(std::rand() % 10000) / 10000.0f;
}

}  // namespace

Starfield::Starfield()
{
    // Fixed seed for a deterministic sky.
    std::srand(1979);
    int i = 0;
    for (int layer = 0; layer < 3; ++layer) {
        for (int s = 0; s < kStarsPerLayer; ++s, ++i) {
            stars_[i].x = randFloat() * 448.0f;
            stars_[i].y = randFloat() * 576.0f;
            stars_[i].layer = layer;
        }
    }
}

void Starfield::update(double dt)
{
    if (dt <= 0.0) {
        return;
    }
    const float d = static_cast<float>(dt);
    for (Star& star : stars_) {
        star.y += kLayerSpeed[star.layer] * d;
        if (star.y >= 576.0f) {
            star.y -= 576.0f;
            star.x = randFloat() * 448.0f;  // re-roll the column on wrap
        }
    }
}

void Starfield::draw(Renderer& renderer) const
{
    for (const Star& star : stars_) {
        renderer.drawFilledRect({star.x, star.y, 1.0f, 1.0f},
                                kLayerColor[star.layer]);
    }
}

}  // namespace graphics
}  // namespace galaxian
