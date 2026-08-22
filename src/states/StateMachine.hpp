#pragma once

#include "GameState.hpp"

namespace galaxian {

// The validated top-level state machine (docs/architecture.md §3.9,
// Stage 17): holds the current GameStateId and accepts ONLY transitions
// from the spec §10 graph (isLegalGameStateTransition). Every accepted
// change fires one callback so the composition root can run its
// enter/exit bookkeeping (fresh game on ->Playing, clean reset on
// ->Title, pause/resume bookkeeping).
//
// SDL-free: pure logic, fully unit-testable headlessly.
class StateMachine {
public:
    // Fired AFTER the state changed: (from, to).
    using ChangeCallback = void (*)(GameStateId from, GameStateId to,
                                    void* userData);

    explicit StateMachine(GameStateId initial = GameStateId::Title);

    // Requests a transition. Illegal requests are REJECTED: false is
    // returned and the current state stays unchanged.
    bool request(GameStateId to);

    GameStateId current() const { return current_; }

    void setCallback(ChangeCallback callback, void* userData);

private:
    GameStateId current_;
    ChangeCallback callback_ = nullptr;
    void* userData_ = nullptr;
};

}  // namespace galaxian
