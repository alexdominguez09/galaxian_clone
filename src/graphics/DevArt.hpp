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

// Stage 19 animation frames (all procedural dev art).
inline constexpr const char* kPlayerIdleA = "dev:player_idle_a";
inline constexpr const char* kPlayerIdleB = "dev:player_idle_b";  // thruster
inline constexpr const char* kEnemyScoutA = "dev:enemy_scout_a";
inline constexpr const char* kEnemyScoutB = "dev:enemy_scout_b";  // core blink
inline constexpr const char* kEnemyGuardA = "dev:enemy_guard_a";
inline constexpr const char* kEnemyGuardB = "dev:enemy_guard_b";
inline constexpr const char* kEnemyCommanderA = "dev:enemy_commander_a";
inline constexpr const char* kEnemyCommanderB = "dev:enemy_commander_b";
// One-shot explosion, 4 frames (total 0.25 s == the gameplay effect
// duration from Stage 9).
inline constexpr const char* kExplosionA = "dev:explosion_a";
inline constexpr const char* kExplosionB = "dev:explosion_b";
inline constexpr const char* kExplosionC = "dev:explosion_c";
inline constexpr const char* kExplosionD = "dev:explosion_d";

// Creates all dev textures in the renderer's cache. Idempotent.
// Returns false if creation fails.
bool createAll(Renderer& renderer);

}  // namespace DevArt

}  // namespace galaxian
