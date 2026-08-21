#pragma once

#include <vector>

#include "Renderer.hpp"
#include "core/Types.hpp"

namespace galaxian {

// Stage 7 — debug collision-box overlay (docs/architecture.md §3.5).
//
// This is the isolated rendering hook for developer debug aids: it draws
// 1-pixel outlines around the boxes it is given. It contains NO game logic
// and NO collision rules — the boxes are computed by gameplay code (Player /
// Projectile bounds, gameplay/Collision.hpp) and passed in here. The F1
// (Action::DebugCollision) toggle lives in Game, which decides which boxes
// to hand to this overlay.
class DebugOverlay {
public:
    // Draws a 1-pixel outline around every box in `boxes` using `color`.
    // `boxes` are logical-space rectangles (the same space the sprites are
    // drawn in), so the outlines align exactly with the sprites.
    static void drawCollisionBoxes(Renderer& renderer,
                                   const std::vector<Rect>& boxes,
                                   const Color& color);
};

}  // namespace galaxian
