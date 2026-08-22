#include "StateMachine.hpp"

namespace galaxian {

StateMachine::StateMachine(GameStateId initial) : current_(initial) {}

bool StateMachine::request(GameStateId to)
{
    if (!isLegalGameStateTransition(current_, to)) {
        return false;
    }
    const GameStateId from = current_;
    current_ = to;
    if (callback_ != nullptr) {
        callback_(from, to, userData_);
    }
    return true;
}

void StateMachine::setCallback(ChangeCallback callback, void* userData)
{
    callback_ = callback;
    userData_ = userData;
}

}  // namespace galaxian
