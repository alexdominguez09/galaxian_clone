#pragma once

#include "gameplay/AttackDirector.hpp"

namespace galaxian {

// The wave lifecycle (docs/game_spec.md §9):
//
//   ...playing... -> all 40 dead -> "WAVE N" interstitial (2 s) ->
//   next formation with wave N+1 parameters (spec §7).
//
// Every difficulty knob stays BOUNDED per the frozen spec: the director's
// waveParams() caps attackers/interval/shots; enemy bullet speed ramps via
// ProjectileManager::speedForWave capped at 360 px/s; formation speed is
// already self-adjusting through the death-pressure multiplier bounded at
// 2.5x (Stage 10). Nothing multiplies unboundedly.
class WaveManager {
public:
    // What happened during this step.
    enum class Event { None, WaveCleared, WaveAdvanced };

    explicit WaveManager(int wave = 1);

    // Fresh run / restart at `wave`.
    void beginWave(int wave);

    // One fixed simulation step. Detects a cleared formation and runs the
    // interstitial countdown; on expiry advances to the next wave —
    // rebuilding `formation` (reset()) and handing the new parameters to
    // `director` (beginWave()). Wave N counts as complete only when every
    // enemy is DEAD (spec §9): a lone diver still in flight blocks it.
    Event update(double dt, EnemyFormation& formation,
                 AttackDirector& director);

    int wave() const { return wave_; }
    bool interstitial() const { return phase_ == Phase::Interstitial; }
    double remaining() const { return remaining_; }

    static constexpr double kInterstitialSeconds = 2.0;

private:
    enum class Phase { Active, Interstitial };

    int wave_ = 1;
    Phase phase_ = Phase::Active;
    double remaining_ = 0.0;
};

}  // namespace galaxian
