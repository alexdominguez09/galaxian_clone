#include "AttackDirector.hpp"

namespace galaxian {

namespace {

// The 1 ns tolerance used for the interval boundary, matching the other
// simulation timers (fire cooldown, effect lifetime, prepare countdown):
// 360 accumulated 1/60 s steps can land up to ~2e-14 s below 6.0 in
// binary floating point, which must not delay a launch past its frame.
constexpr double kIntervalEpsilon = 1e-9;

}  // namespace

AttackWaveParams waveParams(int wave)
{
    // Spec §7 table. Waves below 1 clamp to wave 1 (defensive; the wave
    // system lands in Stage 16 and always passes >= 1).
    if (wave <= 1) {
        return {1, 6.0, 1};
    }
    if (wave == 2) {
        return {2, 6.0, 1};
    }
    if (wave == 3) {
        return {2, 4.0, 1};
    }
    if (wave == 4) {
        return {2, 4.0, 2};
    }
    // Wave 5+: interval floors at 3 s; max attackers rises once more at
    // wave 7. Nothing ever exceeds these bounds.
    const int maxAttackers = (wave >= 7) ? 4 : 3;
    return {maxAttackers, 3.0, 2};
}

AttackDirector::AttackDirector(int wave)
{
    beginWave(wave);
}

void AttackDirector::beginWave(int wave)
{
    wave_ = wave;
    params_ = waveParams(wave);
    sinceLast_ = 0.0;
    cursorCol_ = 0;
}

bool AttackDirector::eligible(const Enemy& enemy)
{
    return enemy.alive() && enemy.state() == EnemyState::Formation;
}

int AttackDirector::activeAttacks(const EnemyFormation& formation)
{
    int active = 0;
    for (int index = 0; index < EnemyFormation::kTotal; ++index) {
        const Enemy& enemy = formation.at(index);
        if (enemy.alive() && enemy.state() != EnemyState::Formation) {
            ++active;
        }
    }
    return active;
}

Enemy* AttackDirector::pickAttacker(EnemyFormation& formation)
{
    // FRONT rows first (bottom up), columns rotating from cursorCol_: a
    // deterministic order that also spreads attacks across the grid.
    for (int row = EnemyFormation::kRows - 1; row >= 0; --row) {
        for (int i = 0; i < EnemyFormation::kColumns; ++i) {
            const int col =
                (cursorCol_ + i) % EnemyFormation::kColumns;
            Enemy& enemy = formation.at(row, col);
            if (eligible(enemy)) {
                cursorCol_ = (col + 1) % EnemyFormation::kColumns;
                return &enemy;
            }
        }
    }
    return nullptr;
}

int AttackDirector::update(double dt, EnemyFormation& formation)
{
    if (dt <= 0.0) {
        return 0;
    }
    sinceLast_ += dt;

    const int active = activeAttacks(formation);
    if (active >= params_.maxSimultaneousAttackers) {
        return 0;  // capacity full: wait (timer keeps running)
    }
    if (sinceLast_ < params_.attackIntervalSeconds - kIntervalEpsilon) {
        return 0;  // too soon since the last launch
    }

    // Launch ONE attacker per elapsed interval (staggered, predictable).
    Enemy* attacker = pickAttacker(formation);
    if (attacker == nullptr) {
        // Nobody eligible right now — skip without deadlock. The timer
        // keeps running so the next free+eligible moment fires.
        return 0;
    }

    // The pattern follows the slot's column band (sway-independent):
    // outer columns sweep outward, middle columns plunge through.
    const float slotX = attacker->slotOffset().x;
    DivePattern pattern = DivePattern::CenterAttack;
    if (slotX < 144.0f) {
        pattern = DivePattern::LeftDive;
    } else if (slotX > 192.0f) {
        pattern = DivePattern::RightDive;
    }
    attacker->beginDive(pattern);

    sinceLast_ = 0.0;
    return 1;
}

}  // namespace galaxian
