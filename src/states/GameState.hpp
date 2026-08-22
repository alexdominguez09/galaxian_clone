#pragma once

namespace galaxian {

// Top-level game states (docs/game_spec.md §10, docs/architecture.md §3.9,
// Stage 17):
//
//   Title -> Playing -> Paused -> Playing
//   Playing -> GameOver -> Title
//
// Responsibilities:
//   Title     - logo / high score / "PRESS ENTER"; Start begins a fresh
//               game, Escape quits the application.
//   Playing   - the full simulation (everything Stages 5-16 built).
//   Paused    - the SAME frozen scene, no simulation; Escape resumes.
//   GameOver  - final score + "PRESS ENTER"; returns to a clean Title.
enum class GameStateId {
    Title,
    Playing,
    Paused,
    GameOver,
};

inline constexpr int kGameStateCount = 4;

// Short upper-case label for logs/debug displays.
const char* gameStateName(GameStateId id);

// The validated transition table — EXACTLY the spec §10 graph, nothing
// else (skips, self-transitions and backwards jumps are illegal):
constexpr bool isLegalGameStateTransition(GameStateId from, GameStateId to)
{
    switch (from) {
        case GameStateId::Title:
            return to == GameStateId::Playing;
        case GameStateId::Playing:
            return to == GameStateId::Paused || to == GameStateId::GameOver;
        case GameStateId::Paused:
            return to == GameStateId::Playing;
        case GameStateId::GameOver:
            return to == GameStateId::Title;
    }
    return false;
}

}  // namespace galaxian
