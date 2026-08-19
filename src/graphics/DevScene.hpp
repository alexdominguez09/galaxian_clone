#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"
#include "input/Actions.hpp"

namespace galaxian {

class InputManager;

// Stage 3 test scene (docs/test_plan.md §1, Stage 3):
//
//   player sprite, 10 enemy sprites, text, projectile rectangles,
//   screen border
//
// Stage 4 extends it with a small input demo (dev aid, replaced by the real
// Player in Stage 5): the player sprite follows the MoveLeft/MoveRight
// Actions, Space (Fire) flashes a projectile, and an on-screen table shows
// the live held/pressed/released state of every Action. Rendered by Game
// until gameplay rendering exists (Stage 5+).
class DevScene {
public:
    // Loads dev art into the renderer. Returns false on failure.
    bool initialize(Renderer& renderer);

    // Advances the Stage 4 input demo by one rendered frame. Reads the
    // named Actions from `input` (never SDL keys) and moves the player /
    // triggers the fire flash. Frame-rate dependent by design: this is a
    // dev aid, not the fixed-timestep gameplay simulation.
    void update(double frameDeltaSeconds, const InputManager& input);

    void draw(Renderer& renderer) const;

private:
    struct EnemySprite {
        const Texture* texture = nullptr;
        Vector2 position{0.0f, 0.0f};
    };

    const Texture* player_ = nullptr;
    const Texture* bullet_ = nullptr;
    EnemySprite enemies_[10] = {};

    // Stage 4 input demo state.
    float demoPlayerCenterX_ = 224.0f;  // spec §5 start center
    double fireFlashSeconds_ = 0.0;

    // Snapshot of the input state, captured in update() and rendered by
    // draw() as an on-screen action table (dev aid for manual verification).
    struct ActionRow {
        bool held = false;
        bool pressed = false;
        bool released = false;
    };
    ActionRow actionRows_[kActionCount] = {};
};

}  // namespace galaxian
