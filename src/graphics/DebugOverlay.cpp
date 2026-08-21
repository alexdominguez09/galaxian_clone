#include "DebugOverlay.hpp"

namespace galaxian {

void DebugOverlay::drawCollisionBoxes(Renderer& renderer,
                                      const std::vector<Rect>& boxes,
                                      const Color& color)
{
    // Pure drawing: one 1-pixel outline per box. No collision rules here —
    // the boxes arrive precomputed from gameplay code.
    for (const Rect& box : boxes) {
        renderer.drawRect(box, color);
    }
}

}  // namespace galaxian
