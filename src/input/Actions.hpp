#pragma once

#include <string_view>

namespace galaxian {

// Named game actions (docs/game_spec.md §4, docs/architecture.md §3.3).
//
// This is the only input vocabulary gameplay code uses: it queries
// Actions through the InputManager and never sees SDL key constants
// (dependency rule, docs/architecture.md §1).
enum class Action : int {
    MoveLeft,        // Left Arrow / A
    MoveRight,       // Right Arrow / D
    Fire,            // Space
    Start,           // Enter
    Pause,           // Escape (pause/back; title → quit)
    DebugCollision,  // F1 (developer only)
    DebugOverlay,    // F2 (developer only)
    DebugDive,       // F3 (developer only): send one formation enemy into a
                     // dive cycle so the Stage 11 state machine is visible;
                     // Stage 13's AttackDirector owns real selection.
    Count            // sentinel: number of actions, not an action itself
};

inline constexpr int kActionCount = static_cast<int>(Action::Count);

// Short label for debug displays.
inline std::string_view actionName(Action action)
{
    switch (action) {
    case Action::MoveLeft:
        return "MoveLeft";
    case Action::MoveRight:
        return "MoveRight";
    case Action::Fire:
        return "Fire";
    case Action::Start:
        return "Start";
    case Action::Pause:
        return "Pause";
    case Action::DebugCollision:
        return "DebugCollision";
    case Action::DebugOverlay:
        return "DebugOverlay";
    case Action::DebugDive:
        return "DebugDive";
    default:
        return "?";
    }
}

}  // namespace galaxian
