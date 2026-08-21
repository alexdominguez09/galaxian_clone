#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"

namespace galaxian {

// Test scene (docs/test_plan.md §1, Stage 3):
//
//   enemy sprites, text, projectile rectangles, screen border
//
// Stage 4 added an input demo (a stand-in player) and an on-screen action
// table; Stage 5 replaced the stand-in with the real gameplay Player
// (owned and drawn by Game). Stage 8 removes the rest: the 10 dummy enemies
// are replaced by the real gameplay formation (gameplay/EnemyFormation,
// owned and drawn by Game) and the action table is gone because its area
// (y 140-280) overlaps the formation (y 64-232). What remains is the
// static test content: text, three projectile rectangles, and the border.
class DevScene {
public:
    // Loads dev art into the renderer. Returns false on failure.
    bool initialize(Renderer& renderer);

    void draw(Renderer& renderer) const;
};

}  // namespace galaxian
