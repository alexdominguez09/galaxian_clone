#pragma once

#include <string_view>

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
// Points and sprite index are frozen spec values. Speed drives the Stage 11
// simple-path motion (dive/attack/return); final values are tuned in
// Stage 23 and configurable in Stage 21 (spec §14).
inline constexpr EnemyDefinition kEnemyDefinitions[kEnemyTypeCount] = {
    {50, 140.0f, 0},    // Scout: front rows
    {80, 100.0f, 1},    // Guard: mid rows
    {150, 70.0f, 2},    // Commander: top row
};

// Per-enemy state machine (docs/game_spec.md §6.4):
//
//   Formation → PreparingDive → Diving → Attacking → Returning → Formation
//   any living state → Dead (terminal)
//
// Stage 11 proves the machine with SIMPLE paths (the plan's "do not
// implement complex trajectories yet"); Stage 12 replaces Diving/Attacking/
// Returning motion with the real Bézier trajectories without changing the
// machine:
//
//   - PreparingDive holds at its slot for kPrepareDurationSeconds (the
//     AttackDirector of Stage 13 decides WHEN an enemy is selected; the
//     short peel-off pause belongs to the enemy itself).
//   - Diving descends straight down at definition().speed until the box's
//     BOTTOM reaches kTurnPointY.
//   - Attacking dashes horizontally (towards the nearer screen edge) until
//     the box is fully off-screen.
//   - Returning homes to its LIVE slot position (formation world position +
//     slot offset, recomputed every step so a swaying formation is tracked)
//     and snaps exactly onto the slot when within one step's travel.
enum class EnemyState {
    Formation,
    PreparingDive,
    Diving,
    Attacking,
    Returning,
    Dead,
};

// Short upper-case label for debug displays (docs/test_plan.md Stage 11).
inline std::string_view enemyStateName(EnemyState state)
{
    switch (state) {
        case EnemyState::Formation:     return "FORMATION";
        case EnemyState::PreparingDive: return "PREPARING";
        case EnemyState::Diving:        return "DIVING";
        case EnemyState::Attacking:     return "ATTACKING";
        case EnemyState::Returning:     return "RETURNING";
        case EnemyState::Dead:          return "DEAD";
    }
    return "?";
}

// A single enemy (docs/game_spec.md §6, docs/architecture.md §3.6).
//
// The enemy stores its type (with the data-driven definition), its SLOT
// OFFSET (formation-local), and — while away from the formation — its live
// dive position. The screen position depends on the state: slot members sit
// on the lattice (formation world position + slot offset, spec §6.2);
// diving enemies report their dive position. That indirection is what lets
// enemies leave and rejoin the formation while it keeps oscillating.
//
// SDL-free by design (dependency rule, docs/architecture.md §1): no keys,
// no InputManager, no rendering. All motion is fixed-step arithmetic
// (docs/architecture.md §3.1): frame-rate independent, deterministic.
class Enemy {
public:
    // Collision box: 24x24 (the dev-art square; the final art keeps the
    // same box, Stage 24).
    static constexpr float kWidth = 24.0f;
    static constexpr float kHeight = 24.0f;

    // Stage 11 simple-path constants (Stage 12 replaces the paths, not the
    // machine). The diver pauses at its slot for kPrepareDurationSeconds,
    // dives until its box bottom reaches kTurnPointY (40 px above the
    // player's start top edge, spec §5), then dashes off-screen and homes
    // back to its slot.
    static constexpr double kPrepareDurationSeconds = 0.5;
    static constexpr float kTurnPointY = 480.0f;

    Enemy() = default;

    // Creates a living enemy of `type` at formation-local `slotOffset`
    // (top-left of the collision box, the Rect convention).
    Enemy(EnemyType type, Vector2 slotOffset);

    EnemyType type() const { return type_; }
    const EnemyDefinition& definition() const;
    Vector2 slotOffset() const { return slotOffset_; }

    EnemyState state() const { return state_; }
    bool alive() const { return state_ != EnemyState::Dead; }

    // Screen position (top-left of the box): the slot lattice for
    // Formation/PreparingDive/Dead members, the live dive position for
    // Diving/Attacking/Returning ones.
    Vector2 screenPosition(Vector2 formationPosition) const;

    // Collision bounds for the current state (see screenPosition). This is
    // what combat uses, so bullets hit divers where they actually are.
    Rect bounds(Vector2 formationPosition) const;

    // Live position while away from the slot (diagnostics/tests).
    Vector2 divePosition() const { return divePosition_; }

    // --- State machine (docs/test_plan.md Stage 11) ---

    // The legality table: the five chain edges plus "any living state ->
    // Dead". Dead is terminal; self-transitions are illegal.
    static bool isLegalTransition(EnemyState from, EnemyState to);

    // Attempts a validated transition. Returns false (leaving the state
    // unchanged) when the transition is illegal per isLegalTransition.
    bool transitionTo(EnemyState next);

    // Any living state -> Dead. Terminal: nothing leaves Dead.
    void kill();

    // Convenience for selection (the future AttackDirector, debug aids):
    // Formation -> PreparingDive. Returns false if not in Formation.
    bool beginDive();

    // Advances one fixed simulation step (docs/architecture.md §3.1).
    //
    // `formationPosition` is the formation anchor's CURRENT world position.
    // Formation members do not move themselves (they ride the lattice);
    // PreparingDive counts down; Diving/Attacking/Returning follow the
    // simple paths at definition().speed, with Returning homing onto the
    // live slot position. dt <= 0 is a no-op.
    void update(double dt, Vector2 formationPosition);

private:
    EnemyType type_ = EnemyType::Scout;
    Vector2 slotOffset_ = {0.0f, 0.0f};
    EnemyState state_ = EnemyState::Formation;
    // Live position while away from the slot (top-left of the box).
    Vector2 divePosition_ = {0.0f, 0.0f};
    // Horizontal dash direction, chosen when Attacking is entered:
    // -1 towards the left edge, +1 towards the right edge.
    float attackDirection_ = -1.0f;
    double prepareRemaining_ = 0.0;
};

}  // namespace galaxian
