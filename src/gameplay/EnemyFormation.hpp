#pragma once

#include <cmath>

#include "core/Constants.hpp"
#include "core/Types.hpp"

#include "gameplay/Enemy.hpp"

namespace galaxian {

// The enemy formation (docs/game_spec.md §6.2, docs/architecture.md §3.6).
//
// A logical grid, 5 rows x 8 columns = 40 enemies (spec §6.2):
//
//   row 0: Commander x 8
//   row 1: Guard     x 8
//   row 2: Guard     x 8
//   row 3: Scout     x 8
//   row 4: Scout     x 8
//
// Column spacing 48 px, row spacing 36 px, top-left anchor (32, 64). The
// formation owns a single world position; each enemy's screen position is
// world position + slot offset (spec §6.2). That decomposition is what lets
// diving enemies leave and rejoin the formation in Stages 11-13.
//
// Stage 10 adds the horizontal oscillation (spec §6.3): the world position
// sways sinusoidally around the anchor with a 64 px peak-to-peak swing
// (±32 px — the interpretation that keeps the whole 360 px-wide grid inside
// the 448 px screen, per the "oscillates within screen bounds" criterion)
// and a base period of 4 s. As enemies die the oscillation speeds up,
// bounded at 2.5x base speed (spec §6.3 pressure mechanic):
//
//   multiplier = 1 + 1.5 * (1 - alive / total)   ∈ [1.0, 2.5]
//
// All motion is phase accumulation on the fixed 1/60 s step
// (docs/architecture.md §3.1), so it is frame-rate independent and
// deterministic; the phase wraps modulo 2π so long sessions cannot grow it
// without bound. Vertical variation was explicitly optional in the plan and
// is not implemented: y never changes.
//
// Storage is a fixed 40-slot array (row-major: index = row * kColumns +
// col): zero heap allocations, no leaks by construction.
//
// SDL-free (dependency rule, docs/architecture.md §1).
class EnemyFormation {
public:
    // Spec §6.2 grid constants (logical px).
    static constexpr int kRows = 5;
    static constexpr int kColumns = 8;
    static constexpr int kTotal = kRows * kColumns;  // 40
    static constexpr float kColumnSpacing = 48.0f;
    static constexpr float kRowSpacing = 36.0f;
    // Formation top-left anchor (spec §6.2): the oscillation midpoint.
    static constexpr Vector2 kAnchor{32.0f, 64.0f};

    // Spec §6.3 oscillation constants.
    static constexpr float kOscillationSwing = 64.0f;  // peak-to-peak, ±32
    static constexpr float kOscillationHalfSwing = kOscillationSwing * 0.5f;
    static constexpr double kOscillationPeriodSeconds = 4.0;
    static constexpr double kMaxSpeedMultiplier = 2.5;

    // Builds the full 40-enemy grid at the anchor, all alive.
    EnemyFormation() { reset(); }

    // Rebuilds the grid from scratch: 40 living enemies at their slot
    // offsets, world position back at the anchor, oscillation phase back to
    // zero. Used at wave starts (Stage 16) and by tests.
    void reset();

    // Advances the oscillation by one fixed simulation step
    // (docs/architecture.md §3.1). A no-op for dt <= 0. Dead enemies do not
    // move (they are holes), but they still count towards the §6.3
    // speed-up.
    void update(double dt);

    // Total enemies (always kTotal; dead ones leave holes, spec §6.3).
    int count() const { return kTotal; }
    // Living enemies.
    int aliveCount() const;

    // Enemy at row `row` (0..kRows-1), column `col` (0..kColumns-1).
    Enemy& at(int row, int col);
    const Enemy& at(int row, int col) const;
    // Enemy at row-major index `index` (0..kTotal-1):
    // index = row * kColumns + col.
    Enemy& at(int index);
    const Enemy& at(int index) const;

    // Row -> type mapping (spec §6.2 layout).
    static EnemyType typeForRow(int row);
    // Slot offset for row/col: formation-local, top-left of the 24x24 box.
    static Vector2 slotOffset(int row, int col);

    // World position of the formation anchor: x oscillates around kAnchor.x
    // (Stage 10), y is always kAnchor.y.
    Vector2 position() const { return position_; }
    // Directly places the anchor (tests / future systems). The next
    // update() overwrites x from the oscillation phase; y persists until
    // reset(). The spec §6.2 invariant position = world + slot offset holds
    // for any value.
    void setPosition(Vector2 position);

    // Screen position (top-left of the box) of the enemy at row/col.
    Vector2 positionOf(int row, int col) const;
    // Collision box of the enemy at row/col.
    Rect boundsOf(int row, int col) const;

    // Oscillation angle in radians, kept in [0, 2π). Diagnostics/tests.
    double phase() const { return phase_; }
    // Current §6.3 speed multiplier in [1.0, 2.5]: 1.0 for a full
    // formation, 2.5 when everything is dead.
    double speedMultiplier() const;

private:
    Enemy enemies_[kTotal] = {};
    Vector2 position_ = kAnchor;
    double phase_ = 0.0;
};

}  // namespace galaxian
