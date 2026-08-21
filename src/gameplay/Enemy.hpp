#pragma once

#include "core/Constants.hpp"
#include "core/Types.hpp"

namespace galaxian {

// Enemy type (docs/game_spec.md §6.1). Data-driven: behavior comes from the
// EnemyDefinition table, not from subclasses (the plan's "prefer
// data-driven" rule; subclasses only if logic diverges, which it does not).
enum class EnemyType {
    Scout,
    Guard,
    Commander,
};

// Number of EnemyType values (Scout, Guard, Commander).
inline constexpr int kEnemyTypeCount = 3;

// Per-type data (docs/game_spec.md §6.1, docs/architecture.md §3.6):
// points, speed, sprite index.
struct EnemyDefinition {
    int points;
    float speed;
    int spriteIndex;
};

// Spec §6.1 table, indexed by EnemyType.
//
// Points and sprite index are frozen spec values. Speed is a nominal
// placeholder: the frozen spec does not define per-type speeds (they are
// used by the Stage 11+ dive/return motion). Final values are tuned in
// Stage 23 and configurable in Stage 21 (spec §14).
inline constexpr EnemyDefinition kEnemyDefinitions[kEnemyTypeCount] = {
    {50, 140.0f, 0},    // Scout: front rows
    {80, 100.0f, 1},    // Guard: mid rows
    {150, 70.0f, 2},    // Commander: top row
};

// Per-enemy state machine (docs/game_spec.md §6.4).
//
// Stage 8 implements the minimal Formation/Dead pair (mirroring the Stage 5
// Player's minimal pair); Stage 11 expands this to
// Formation → PreparingDive → Diving → Attacking → Returning → Formation,
// with Dead reachable from any living state and terminal.
enum class EnemyState {
    Formation,
    Dead,
};

// A single enemy (docs/game_spec.md §6, docs/architecture.md §3.6).
//
// Stage 8: a static formation member. The enemy stores its type (with the
// data-driven definition) and its SLOT OFFSET (formation-local); the screen
// position is always computed as formation world position + slot offset
// (spec §6.2). That indirection is what lets diving enemies leave and rejoin
// the formation in Stages 11–13. No movement of any kind yet (the
// formation's world position is static until Stage 10).
//
// SDL-free by design (dependency rule, docs/architecture.md §1): no keys,
// no InputManager, no rendering. The spriteIndex → texture mapping lives in
// the composition root (Game), which hands graphics/ the textures to draw.
class Enemy {
public:
    // Collision box: 24x24 (the dev-art square; the final art keeps the
    // same box, Stage 24).
    static constexpr float kWidth = 24.0f;
    static constexpr float kHeight = 24.0f;

    Enemy() = default;

    // Creates a living enemy of `type` at formation-local `slotOffset`
    // (top-left of the collision box, the Rect convention).
    Enemy(EnemyType type, Vector2 slotOffset);

    EnemyType type() const { return type_; }
    const EnemyDefinition& definition() const;
    Vector2 slotOffset() const { return slotOffset_; }

    // Collision bounds when the formation's anchor is at `formationPosition`
    // (spec §6.2: screen position = formation world pos + slot offset).
    Rect bounds(Vector2 formationPosition) const;

    EnemyState state() const { return state_; }
    // Any living state (Stage 11: everything except Dead).
    bool alive() const { return state_ != EnemyState::Dead; }

    // Stage 8 alive/dead pair. Stage 9 wires this into combat (a hit kills
    // the enemy); Stage 11 replaces the pair with the full machine.
    void kill();

private:
    EnemyType type_ = EnemyType::Scout;
    Vector2 slotOffset_ = {0.0f, 0.0f};
    EnemyState state_ = EnemyState::Formation;
};

}  // namespace galaxian
