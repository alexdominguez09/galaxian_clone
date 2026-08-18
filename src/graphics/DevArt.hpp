#pragma once

#include "Renderer.hpp"

namespace galaxian {

// Temporary developer artwork (Stage 3). Procedural shapes stand in for the
// final pixel art (Stage 24):
//
//   player  = triangle (24x16, matches the spec's player collision box)
//   enemies = squares (24x24, one color per type)
//   bullet  = small rectangle (4x10)
//
// Textures are registered in the Renderer's cache under "dev:*" ids.
namespace DevArt {

// Texture ids.
inline constexpr const char* kPlayer = "dev:player";
inline constexpr const char* kEnemyScout = "dev:enemy_scout";
inline constexpr const char* kEnemyGuard = "dev:enemy_guard";
inline constexpr const char* kEnemyCommander = "dev:enemy_commander";
inline constexpr const char* kBullet = "dev:bullet";

// Creates all dev textures in the renderer's cache. Idempotent.
// Returns false if creation fails.
bool createAll(Renderer& renderer);

}  // namespace DevArt

}  // namespace galaxian
