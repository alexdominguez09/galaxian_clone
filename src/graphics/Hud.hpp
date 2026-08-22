#pragma once

#include "Renderer.hpp"

namespace galaxian {
namespace hud {

// The arcade HUD (docs/game_spec.md §11, Stage 18). An isolated rendering
// module in the DebugOverlay style: it receives plain VALUES and draws
// them — no game logic, no state, no score rules (the score rules live in
// gameplay/ScoreManager; the HUD only displays).
//
// Layout (logical 448x576):
//
//   SCORE            HIGH
//   000000           000000      <- zero-padded to six digits
//   WAVE n                        <- under the top bar, left column
//
//   (life pips bottom-left: one small triangle per remaining life)
struct Values {
    int score = 0;
    int highScore = 0;
    int wave = 1;
};

// Draws the top bar (labels + values) and the WAVE line.
void drawTopBar(Renderer& renderer, const Values& values);

// Draws the life pips at the bottom-left: exactly `lives` small cyan
// triangles (font-free, so no glyph coverage surprises). Lives are clamped
// to [0, kLives] — a display can never show more ships than exist.
void drawLivesPips(Renderer& renderer, int lives);

}  // namespace hud
}  // namespace galaxian
