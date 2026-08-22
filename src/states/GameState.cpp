#include "GameState.hpp"

namespace galaxian {

const char* gameStateName(GameStateId id)
{
    switch (id) {
        case GameStateId::Title:    return "TITLE";
        case GameStateId::Playing:  return "PLAYING";
        case GameStateId::Paused:   return "PAUSED";
        case GameStateId::GameOver: return "GAME OVER";
    }
    return "?";
}

}  // namespace galaxian
