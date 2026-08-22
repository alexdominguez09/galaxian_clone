#include "ScoreManager.hpp"

namespace galaxian {

int ScoreManager::addKill(EnemyType type, int multiplier)
{
    const int points =
        kEnemyDefinitions[static_cast<int>(type)].points * multiplier;
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
