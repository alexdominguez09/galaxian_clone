#pragma once

#include "gameplay/DivePath.hpp"
#include "gameplay/EnemyFormation.hpp"

namespace galaxian {

// Per-wave pacing parameters for the attack director (docs/game_spec.md
// §7 table). All values are bounded; difficulty never multiplies
// unboundedly. `shotsPerAttack` is the enemy-fire budget per dive — the
// projectile side consumes it in Stage 14.
struct AttackWaveParams {
    int maxSimultaneousAttackers;
    double attackIntervalSeconds;
    int shotsPerAttack;
};

// Spec §7 table, indexed by wave (1-based). Wave 5+ keeps max 3 until wave
// 7 raises it to 4; the interval floors at 3 s from wave 5 on.
AttackWaveParams waveParams(int wave);

// The central pacing authority (docs/game_spec.md §7, docs/architecture.md
// §3.6): NO enemy ever decides to attack on its own — the director decides
// WHEN an attack begins, WHICH enemy attacks, and HOW MANY are out at once.
//
// Rules:
//   - At most `maxSimultaneousAttackers` enemies are away from the
//     formation (PreparingDive / Diving / Attacking / Returning) at any
//     time; the count never exceeds the wave maximum.
//   - Successive launches are spaced at least `attackIntervalSeconds`
//     apart ("attacks occur predictably"; verified within ±1 frame).
//   - Selection scans the grid deterministically: FRONT rows first (bottom
//     up), columns starting at a rotating cursor that advances after every
//     launch so attacks spread across the formation. Only Formation-state
//     enemies are eligible — dead and already-away enemies are never
//     selected, so nobody dives twice concurrently.
//   - No deadlock: when nobody is eligible (or capacity is full) the tick
//     simply skips; the timer keeps running and the moment both capacity
//     and an eligible enemy exist, the next attack fires.
//
// SDL-free (dependency rule, docs/architecture.md §1): pure logic on the
// fixed 1/60 s step, no I/O, no rendering.
class AttackDirector {
public:
    // Starts at `wave` with a fresh timer (the first attack fires exactly
    // one interval into the wave).
    explicit AttackDirector(int wave = 1);

    // Switches pacing parameters (Stage 16 waves); resets the timer and
    // cursor like a fresh wave.
    void beginWave(int wave);

    // Advances one fixed simulation step against `formation`. Launches at
    // most ONE attacker this step (staggered attacks read better than a
    // simultaneous volley and keep "predictable" trivially true). Returns
    // the number of attacks launched (0 or 1).
    int update(double dt, EnemyFormation& formation);

    // Enemies currently away from the formation (diagnostics/stats).
    static int activeAttacks(const EnemyFormation& formation);

    // True when `enemy` may be selected right now: alive and sitting in
    // its Formation slot.
    static bool eligible(const Enemy& enemy);

    const AttackWaveParams& params() const { return params_; }
    int wave() const { return wave_; }
    // Seconds since the last launch (diagnostics/tests).
    double sinceLastLaunch() const { return sinceLast_; }

private:
    // Deterministic scan order: rows bottom-up, columns rotating from
    // cursorCol_. Returns nullptr when nobody is eligible.
    Enemy* pickAttacker(EnemyFormation& formation);

    int wave_ = 1;
    AttackWaveParams params_{};
    double sinceLast_ = 0.0;
    int cursorCol_ = 0;
};

}  // namespace galaxian
