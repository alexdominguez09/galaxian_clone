#pragma once

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
// Stage 8: STATIC. The world position sits at the anchor and nothing moves;
// Stage 10 adds the horizontal oscillation (amplitude 64 px, base period
// 4 s, bounded speed-up as enemies die), Stage 11+ the dives.
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
    // Formation top-left anchor (spec §6.2). The formation's world position
    // starts here; Stage 10 oscillates it horizontally from this base.
    static constexpr Vector2 kAnchor{32.0f, 64.0f};

    // Builds the full 40-enemy grid at the anchor, all alive.
    EnemyFormation() { reset(); }

    // Rebuilds the grid from scratch: 40 living enemies at their slot
    // offsets, world position back at the anchor. Used at wave starts
    // (Stage 16) and by tests.
    void reset();

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

    // World position of the formation anchor (Stage 8: always kAnchor).
    Vector2 position() const { return position_; }
    // Stage 10 sets this from the oscillation; the spec §6.2 invariant
    // position = world + slot offset holds for any value.
    void setPosition(Vector2 position);

    // Screen position (top-left of the box) of the enemy at row/col.
    Vector2 positionOf(int row, int col) const;
    // Collision box of the enemy at row/col.
    Rect boundsOf(int row, int col) const;

private:
    Enemy enemies_[kTotal] = {};
    Vector2 position_ = kAnchor;
};

}  // namespace galaxian
