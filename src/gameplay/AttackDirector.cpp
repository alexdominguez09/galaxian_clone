#include "AttackDirector.hpp"

#include "core/GameConfig.hpp"

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
    // Spec §7 pacing table — now DATA-DRIVEN from the GameConfig rows
    // ([0]=w1 .. [3]=w4 [4]=w5-6 [5]=w7+), with the structural caps
    // re-asserted here so a bypassed loader can never break bounds.
    const GameConfig& cfg = GameConfig::get();
    AttackWaveParams p{};
    if (wave <= 1) {
        p = {cfg.waves[0].maxAttackers, cfg.waves[0].intervalSeconds,
             cfg.waves[0].shotsPerAttack};
    } else if (wave == 2) {
        p = {cfg.waves[1].maxAttackers, cfg.waves[1].intervalSeconds,
             cfg.waves[1].shotsPerAttack};
    } else if (wave == 3) {
        p = {cfg.waves[2].maxAttackers, cfg.waves[2].intervalSeconds,
             cfg.waves[2].shotsPerAttack};
    } else if (wave == 4) {
        p = {cfg.waves[3].maxAttackers, cfg.waves[3].intervalSeconds,
             cfg.waves[3].shotsPerAttack};
    } else {
        p = {cfg.waves[4].maxAttackers, cfg.waves[4].intervalSeconds,
             cfg.waves[4].shotsPerAttack};
        if (wave >= 7) {
            p.maxSimultaneousAttackers = cfg.waves[5].maxAttackers;
        }
    }
    if (p.maxSimultaneousAttackers < 1) p.maxSimultaneousAttackers = 1;
    if (p.maxSimultaneousAttackers > 4) p.maxSimultaneousAttackers = 4;
    if (p.shotsPerAttack < 1) p.shotsPerAttack = 1;
    if (p.shotsPerAttack > 2) p.shotsPerAttack = 2;
    if (p.attackIntervalSeconds < 1.0) p.attackIntervalSeconds = 1.0;
    return p;
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
    attacker->beginDive(pattern, params_.shotsPerAttack);

    sinceLast_ = 0.0;
    return 1;
}

}  // namespace galaxian
