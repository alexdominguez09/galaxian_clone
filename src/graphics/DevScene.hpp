#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"
#include "input/Actions.hpp"

namespace galaxian {

class InputManager;

// Stage 3 test scene (docs/test_plan.md §1, Stage 3):
//
//   enemy sprites, text, projectile rectangles, screen border
//
// Stage 4 added a small input demo (a stand-in player that followed the
// MoveLeft/MoveRight Actions and a fire flash). Stage 5 replaces that
// stand-in with the real gameplay Player (owned and drawn by Game), so the
// demo player and fire flash are gone. What remains from Stage 4 is the
// on-screen action table: a dev aid showing the live held/pressed/released
// state of every Action, so input can still be verified by eye.
class DevScene {
public:
    // Loads dev art into the renderer. Returns false on failure.
    bool initialize(Renderer& renderer);

    // Snapshots the input state for the on-screen action table. Called once
    // per frame by Game. (The Stage 4 demo player movement lived here and is
    // gone: the real Player is simulated by Game in the fixed-timestep loop.)
    void update(const InputManager& input);

    void draw(Renderer& renderer) const;

private:
    struct EnemySprite {
        const Texture* texture = nullptr;
        Vector2 position{0.0f, 0.0f};
    };

    EnemySprite enemies_[10] = {};

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
