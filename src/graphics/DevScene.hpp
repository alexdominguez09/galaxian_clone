#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"

namespace galaxian {

// Stage 3 test scene (docs/test_plan.md §1, Stage 3):
//
//   player sprite, 10 enemy sprites, text, projectile rectangles,
//   screen border
//
// Rendered by Game until gameplay rendering exists (Stage 5+).
class DevScene {
public:
    // Loads dev art into the renderer. Returns false on failure.
    bool initialize(Renderer& renderer);

    void draw(Renderer& renderer) const;

private:
    struct EnemySprite {
        const Texture* texture = nullptr;
        Vector2 position{0.0f, 0.0f};
    };

    const Texture* player_ = nullptr;
    const Texture* bullet_ = nullptr;
    EnemySprite enemies_[10] = {};
};

}  // namespace galaxian
