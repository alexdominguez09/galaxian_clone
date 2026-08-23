#pragma once

#include "Renderer.hpp"

namespace galaxian {

// Original pixel-art sprites (Stage 24). Hand-authored ASCII bitmaps
// rasterized through a palette forge (graphics/DevArt.cpp), designed after
// the arcade reference shots in assets/sprites/examples_from_internet:
//
//   player    = white fighter with cyan wings + red spine (24x16)
//   scout     = teal drone bug, red eyes, blue segmented wings (24x24)
//   guard     = red escort bug, yellow eyes, blue wings (24x24)
//   commander = flagship: yellow swept wings, orange dome, blue tips (24x24)
//   bullet    = small rectangle (4x10)
//   explosions = procedural starburst (enemies) / fountain burst (player),
//                32x32 per frame, drawn centered on the effect box
//
// All art fits the EXISTING collision boxes (no gameplay regression).
// Textures are registered in the Renderer's cache under "dev:*" ids.
namespace DevArt {

// Texture ids. The bare kPlayer/kEnemy* ids are kept for compatibility
// (Game's texture checks + the rendering tests) and resolve to the SAME
// pixel art as the matching frame-A id.
inline constexpr const char* kPlayer = "dev:player";
inline constexpr const char* kEnemyScout = "dev:enemy_scout";
inline constexpr const char* kEnemyGuard = "dev:enemy_guard";
inline constexpr const char* kEnemyCommander = "dev:enemy_commander";
inline constexpr const char* kBullet = "dev:bullet";

// Stage 19 animation frames (pixel art since Stage 24).
inline constexpr const char* kPlayerIdleA = "dev:player_idle_a";
inline constexpr const char* kPlayerIdleB = "dev:player_idle_b";  // thruster
inline constexpr const char* kEnemyScoutA = "dev:enemy_scout_a";
inline constexpr const char* kEnemyScoutB = "dev:enemy_scout_b";  // wing flap
inline constexpr const char* kEnemyGuardA = "dev:enemy_guard_a";
inline constexpr const char* kEnemyGuardB = "dev:enemy_guard_b";
inline constexpr const char* kEnemyCommanderA = "dev:enemy_commander_a";
inline constexpr const char* kEnemyCommanderB = "dev:enemy_commander_b";
// One-shot ENEMY explosion (starburst), 4 frames of 32x32 (total 0.25 s ==
// the gameplay effect duration from Stage 9). Drawn CENTERED on the effect
// box (the sprite is larger than the 24x24 box on purpose: the blast should
// read bigger than the ship, like the arcade reference).
inline constexpr const char* kExplosionA = "dev:explosion_a";
inline constexpr const char* kExplosionB = "dev:explosion_b";
inline constexpr const char* kExplosionC = "dev:explosion_c";
inline constexpr const char* kExplosionD = "dev:explosion_d";
// One-shot PLAYER explosion (fountain burst: magenta core + yellow rays
// fanning upward, like the arcade life-lost reference), 4 frames of 32x32.
inline constexpr const char* kPlayerExplosionA = "dev:player_explosion_a";
inline constexpr const char* kPlayerExplosionB = "dev:player_explosion_b";
inline constexpr const char* kPlayerExplosionC = "dev:player_explosion_c";
inline constexpr const char* kPlayerExplosionD = "dev:player_explosion_d";

// Creates all dev textures in the renderer's cache. Idempotent.
// Returns false if creation fails.
bool createAll(Renderer& renderer);

}  // namespace DevArt

}  // namespace galaxian
