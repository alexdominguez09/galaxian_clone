#include "WaveManager.hpp"

#include "core/GameConfig.hpp"

namespace galaxian {

namespace {

// Same 1 ns tolerance as the other simulation timers.
constexpr double kTimeEpsilon = 1e-9;

}  // namespace

WaveManager::WaveManager(int wave)
{
    beginWave(wave);
}

void WaveManager::beginWave(int wave)
{
    wave_ = (wave < 1) ? 1 : wave;
    phase_ = Phase::Active;
    remaining_ = 0.0;
}

WaveManager::Event WaveManager::update(double dt,
                                       EnemyFormation& formation,
                                       AttackDirector& director)
{
    if (dt <= 0.0) {
        return Event::None;
    }

    if (phase_ == Phase::Active) {
        if (formation.aliveCount() == 0) {
            phase_ = Phase::Interstitial;
            remaining_ = GameConfig::get().interstitialSeconds;
            return Event::WaveCleared;
        }
        return Event::None;
    }

    // Interstitial countdown.
    remaining_ -= dt;
    if (remaining_ > kTimeEpsilon) {
        return Event::None;
    }

    // Advance to the next wave: rebuild the grid and hand over the new
    // spec §7 parameters.
    ++wave_;
    formation.reset();
    director.beginWave(wave_);
    phase_ = Phase::Active;
    remaining_ = 0.0;
    return Event::WaveAdvanced;
}

}  // namespace galaxian
