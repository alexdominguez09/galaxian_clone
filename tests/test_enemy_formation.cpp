// Stage 8 logic tests (docs/test_plan.md, Stage 8 — Enemy formation).
//
// Headless, SDL-free: the formation is pure gameplay data. The pixel-level
// check that the formation renders at these coordinates lives in
// test_rendering.cpp.
//
// All expected values are the exact spec integers (anchor 32/64, column
// spacing 48, row spacing 36, box 24x24), so exact equality is the correct
// assertion: every expected coordinate is an integer <= 392, exactly
// representable as float32.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include "gameplay/Enemy.hpp"
#include "gameplay/EnemyFormation.hpp"

using namespace galaxian;

namespace {

// Expected screen position (top-left of the 24x24 box) of the enemy at
// row/col for a formation whose anchor is at the spec anchor.
Vector2 expectedPosition(int row, int col)
{
    return {EnemyFormation::kAnchor.x + col * EnemyFormation::kColumnSpacing,
            EnemyFormation::kAnchor.y + row * EnemyFormation::kRowSpacing};
}

}  // namespace

TEST_CASE("formation: exactly 40 enemies, all alive at construction",
          "[enemy][formation]")
{
    EnemyFormation formation;
    CHECK(formation.count() == 40);
    CHECK(formation.count() == EnemyFormation::kRows * EnemyFormation::kColumns);
    CHECK(formation.aliveCount() == 40);
    for (int index = 0; index < formation.count(); ++index) {
        CHECK(formation.at(index).alive());
        CHECK(formation.at(index).state() == EnemyState::Formation);
    }
}

TEST_CASE("formation: row/type layout matches spec", "[enemy][formation]")
{
    // Spec §6.2: row 0 Commander, rows 1-2 Guard, rows 3-4 Scout, 8 each.
    CHECK(EnemyFormation::typeForRow(0) == EnemyType::Commander);
    CHECK(EnemyFormation::typeForRow(1) == EnemyType::Guard);
    CHECK(EnemyFormation::typeForRow(2) == EnemyType::Guard);
    CHECK(EnemyFormation::typeForRow(3) == EnemyType::Scout);
    CHECK(EnemyFormation::typeForRow(4) == EnemyType::Scout);

    EnemyFormation formation;
    const EnemyType expectedRows[] = {EnemyType::Commander, EnemyType::Guard,
                                      EnemyType::Guard, EnemyType::Scout,
                                      EnemyType::Scout};
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            CHECK(formation.at(row, col).type() == expectedRows[row]);
        }
    }
    // Per-type totals: 8/8/8/8/8 by row -> 8 Commanders, 16 Guards, 16 Scouts.
    int command = 0;
    int guard = 0;
    int scout = 0;
    for (int index = 0; index < formation.count(); ++index) {
        const EnemyType type = formation.at(index).type();
        if (type == EnemyType::Commander) {
            ++command;
        } else if (type == EnemyType::Guard) {
            ++guard;
        } else {
            ++scout;
        }
    }
    CHECK(command == 8);
    CHECK(guard == 16);
    CHECK(scout == 16);
}

TEST_CASE("formation: spacing is 48 px columns and 36 px rows",
          "[enemy][formation]")
{
    EnemyFormation formation;
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col + 1 < EnemyFormation::kColumns; ++col) {
            const Vector2 a = formation.positionOf(row, col);
            const Vector2 b = formation.positionOf(row, col + 1);
            CHECK(b.x - a.x == EnemyFormation::kColumnSpacing);
            CHECK(b.y == a.y);  // same row
        }
    }
    for (int col = 0; col < EnemyFormation::kColumns; ++col) {
        for (int row = 0; row + 1 < EnemyFormation::kRows; ++row) {
            const Vector2 a = formation.positionOf(row, col);
            const Vector2 b = formation.positionOf(row + 1, col);
            CHECK(b.y - a.y == EnemyFormation::kRowSpacing);
            CHECK(b.x == a.x);  // same column
        }
    }
    // Slot offsets are the pure spacing lattice (anchor-independent).
    CHECK(EnemyFormation::slotOffset(0, 0) == Vector2{0.0f, 0.0f});
    CHECK(EnemyFormation::slotOffset(0, 1) == Vector2{48.0f, 0.0f});
    CHECK(EnemyFormation::slotOffset(1, 0) == Vector2{0.0f, 36.0f});
    CHECK(EnemyFormation::slotOffset(4, 7) == Vector2{336.0f, 144.0f});
}

TEST_CASE("formation: initial coordinates match spec exactly",
          "[enemy][formation]")
{
    // Spec §6.2: top-left anchor (32, 64); enemy(r,c) top-left =
    // (32 + 48c, 64 + 36r).
    CHECK(EnemyFormation::kAnchor == Vector2{32.0f, 64.0f});

    EnemyFormation formation;
    CHECK(formation.position() == EnemyFormation::kAnchor);
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            CHECK(formation.positionOf(row, col) == expectedPosition(row, col));
        }
    }
    // Explicit corners (top-left / top-right / bottom-left / bottom-right).
    CHECK(formation.positionOf(0, 0) == Vector2{32.0f, 64.0f});
    CHECK(formation.positionOf(0, 7) == Vector2{368.0f, 64.0f});
    CHECK(formation.positionOf(4, 0) == Vector2{32.0f, 208.0f});
    CHECK(formation.positionOf(4, 7) == Vector2{368.0f, 208.0f});
    // The whole grid fits on screen: x in [32, 392], y in [64, 232].
    CHECK(formation.boundsOf(0, 0) == Rect{32.0f, 64.0f, 24.0f, 24.0f});
    CHECK(formation.boundsOf(4, 7).right() == 392.0f);
    CHECK(formation.boundsOf(4, 7).bottom() == 232.0f);
    // Enemy::bounds agrees with the formation's computation (spec §6.2:
    // screen position = formation world pos + slot offset).
    CHECK(formation.at(2, 3).bounds(formation.position()) ==
          formation.boundsOf(2, 3));
}

TEST_CASE("formation: identical layout across 100 spawns", "[enemy][formation]")
{
    // Determinism: construction is a pure function of the constants, so 100
    // fresh formations must be bit-identical.
    struct Snapshot {
        std::array<Vector2, EnemyFormation::kTotal> positions{};
        std::array<EnemyType, EnemyFormation::kTotal> types{};
        std::array<EnemyState, EnemyFormation::kTotal> states{};
        Vector2 anchor;
    };
    auto snapshot = [](const EnemyFormation& f) {
        Snapshot s;
        s.anchor = f.position();
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                const int index = row * EnemyFormation::kColumns + col;
                s.positions[index] = f.positionOf(row, col);
                s.types[index] = f.at(index).type();
                s.states[index] = f.at(index).state();
            }
        }
        return s;
    };

    EnemyFormation reference;
    const Snapshot ref = snapshot(reference);
    for (int spawn = 0; spawn < 100; ++spawn) {
        EnemyFormation fresh;
        const Snapshot s = snapshot(fresh);
        CHECK(s.anchor == ref.anchor);
        for (int index = 0; index < EnemyFormation::kTotal; ++index) {
            CHECK(s.positions[index] == ref.positions[index]);
            CHECK(s.types[index] == ref.types[index]);
            CHECK(s.states[index] == ref.states[index]);
        }
    }
}

TEST_CASE("formation: world position shift moves every enemy (spec §6.2)",
          "[enemy][formation]")
{
    // The formation's world position is the single value Stage 10 will
    // oscillate; the screen position of every enemy must follow it.
    EnemyFormation formation;
    const Vector2 shifted{96.0f, 200.0f};
    formation.setPosition(shifted);
    CHECK(formation.position() == shifted);
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const Vector2 expected =
                shifted + EnemyFormation::slotOffset(row, col);
            CHECK(formation.positionOf(row, col) == expected);
            CHECK(formation.at(row, col).bounds(shifted) ==
                  formation.boundsOf(row, col));
        }
    }
    // Slot offsets never change (that is what lets enemies rejoin, §6.2).
    CHECK(formation.at(3, 5).slotOffset() == EnemyFormation::slotOffset(3, 5));

    // reset() rebuilds the grid at the anchor with all enemies alive.
    formation.at(0, 0).kill();
    formation.setPosition(shifted);
    formation.reset();
    CHECK(formation.position() == EnemyFormation::kAnchor);
    CHECK(formation.aliveCount() == 40);
    CHECK(formation.positionOf(0, 0) == EnemyFormation::kAnchor);
}

TEST_CASE("enemy: definition table matches spec and kill transitions to Dead",
          "[enemy][formation]")
{
    // Spec §6.1: Scout 50 pts sprite 0, Guard 80 pts sprite 1, Commander
    // 150 pts sprite 2.
    CHECK(kEnemyDefinitions[static_cast<int>(EnemyType::Scout)].points == 50);
    CHECK(kEnemyDefinitions[static_cast<int>(EnemyType::Scout)].spriteIndex == 0);
    CHECK(kEnemyDefinitions[static_cast<int>(EnemyType::Guard)].points == 80);
    CHECK(kEnemyDefinitions[static_cast<int>(EnemyType::Guard)].spriteIndex == 1);
    CHECK(kEnemyDefinitions[static_cast<int>(
        EnemyType::Commander)].points == 150);
    CHECK(kEnemyDefinitions[static_cast<int>(EnemyType::Commander)].spriteIndex == 2);

    // definition() routes the EnemyType through the same table.
    CHECK(Enemy(EnemyType::Guard, {0.0f, 0.0f}).definition().points == 80);
    CHECK(Enemy(EnemyType::Commander, {0.0f, 0.0f}).definition().spriteIndex == 2);

    // Stage 8 alive/dead pair (mirrors the Stage 5 Player).
    Enemy enemy(EnemyType::Scout, {48.0f, 72.0f});
    CHECK(enemy.alive());
    CHECK(enemy.state() == EnemyState::Formation);
    CHECK(enemy.bounds({32.0f, 64.0f}) == Rect{80.0f, 136.0f, 24.0f, 24.0f});
    enemy.kill();
    CHECK_FALSE(enemy.alive());
    CHECK(enemy.state() == EnemyState::Dead);
    // Dead is recorded but the slot stays put (no re-pack, spec §6.3).
    CHECK(enemy.slotOffset() == Vector2{48.0f, 72.0f});
    CHECK(enemy.bounds({32.0f, 64.0f}) == Rect{80.0f, 136.0f, 24.0f, 24.0f});

    // A kill reduces the formation's alive count; the hole stays.
    EnemyFormation formation;
    formation.at(1, 2).kill();
    CHECK(formation.aliveCount() == 39);
    CHECK_FALSE(formation.at(1, 2).alive());
    CHECK(formation.positionOf(1, 2) == expectedPosition(1, 2));
}
