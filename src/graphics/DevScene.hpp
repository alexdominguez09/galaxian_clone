#pragma once

#include "Renderer.hpp"
#include "core/Types.hpp"

namespace galaxian {

// The playfield backdrop (docs/test_plan.md §1, Stage 3): the screen
// border, the black background and the controls help line.
//
// History: this scene carried progressively more scaffolding — a demo
// player (Stage 4), dummy enemies + an action table (Stage 3/4) — all of
// which were replaced by real gameplay objects in Stages 5-8. The static
// test bullets and the per-stage title line went away in Stage 18 when
// the top bar became the real HUD (graphics/Hud).
class DevScene {
public:
    // Loads dev art into the renderer. Returns false on failure.
    bool initialize(Renderer& renderer);

    void draw(Renderer& renderer) const;
};

}  // namespace galaxian
