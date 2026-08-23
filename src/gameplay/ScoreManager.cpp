#include "ScoreManager.hpp"

#include "core/GameConfig.hpp"

namespace galaxian {

int ScoreManager::addKill(EnemyType type, int multiplier)
{
    const GameConfig& cfg = GameConfig::get();
    int base = cfg.scoutPoints;
    switch (type) {
        case EnemyType::Guard:     base = cfg.guardPoints; break;
        case EnemyType::Commander: base = cfg.commanderPoints; break;
        case EnemyType::Scout: break;
    }
    const int points = base * multiplier;
    ++kills_;
    score_ += points;
    if (score_ > highScore_) {
        highScore_ = score_;
    }
    return points;
}

int ScoreManager::addPoints(int points)
{
    score_ += points;
    if (score_ > highScore_) {
        highScore_ = score_;
    }
    return points;
}

void ScoreManager::reset()
{
    score_ = 0;
    kills_ = 0;
    // highScore_ intentionally survives (session best, spec §11).
}

}  // namespace galaxian
