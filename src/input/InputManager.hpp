#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <bitset>
#include <cstddef>
#include <initializer_list>
#include <unordered_set>
#include <vector>

#include "input/Actions.hpp"

namespace galaxian {

// Input layer (docs/architecture.md §3.3, docs/game_spec.md §4).
//
// Translates SDL keyboard events into named Actions so that gameplay code
// never references SDL key constants (dependency rule,
// docs/architecture.md §1). Exposes per-frame held/pressed/released state:
//
//   isHeld(action)      true while any bound key is physically down
//   wasPressed(action)  true only in the frame a bound key went down
//   wasReleased(action) true only in the frame the last bound key went up
//
// Frame protocol (driven by Game, docs/architecture.md §3.3):
//   pollEvents()  once per frame, before the simulation (sets edges)
//   ... read isHeld / wasPressed / wasReleased ...
//   endFrame()    once per frame, after the simulation (clears edges)
//
// This is one of the leaf modules allowed to know about SDL.
class InputManager {
public:
    // Constructs with the default key bindings (docs/game_spec.md §4), so
    // the manager is usable immediately (e.g. headless tests) without
    // calling initialize().
    InputManager();

    // Resets all transient state (held/pressed/released, down keys, pending
    // transitions). Existing key bindings are preserved. Safe to call more
    // than once. If the video subsystem is already up, the initial keyboard
    // state is synced (keys held at startup are reported as held, but
    // produce no press edge).
    void initialize();

    // Drains pending input and updates the action state. Called once per
    // frame, before the simulation. Returns true if a window-close
    // (SDL_QUIT) event was seen this frame.
    bool pollEvents();

    // Clears the per-frame pressed/released edge state. Called once per
    // frame, after the simulation has read the input.
    void endFrame();

    // True while any key bound to `action` is physically down.
    bool isHeld(Action action) const;
    // True only in the frame in which a key bound to `action` went down
    // (i.e. the action's held state transitioned false -> true).
    bool wasPressed(Action action) const;
    // True only in the frame in which the last key bound to `action` went
    // up (i.e. the action's held state transitioned true -> false).
    bool wasReleased(Action action) const;

    // Remapping: bindings are data, not code (docs/architecture.md §3.3).
    // Replaces all keys for `action`. Does not generate press/release edges.
    void setBinding(Action action, std::initializer_list<SDL_Keycode> keys);
    // Current keys bound to `action` (for tests / debug displays).
    const std::array<SDL_Keycode, 4>& bindingKeys(Action action) const;
    int bindingCount(Action action) const;

    // Test hooks (headless, no display required): queue a key transition to
    // be applied on the next pollEvents(). Mirrors the Game test-hook
    // convention (docs/test_plan.md). Bypasses SDL event polling entirely.
    void injectKeyDown(SDL_Keycode key);
    void injectKeyUp(SDL_Keycode key);

private:
    struct Binding {
        std::array<SDL_Keycode, 4> keys{};
        int count = 0;
    };
    struct Transition {
        SDL_Keycode key = SDLK_UNKNOWN;
        bool down = false;
    };

    void applyKeyDown(SDL_Keycode key);
    void applyKeyUp(SDL_Keycode key);
    void releaseAll();
    void refreshHeld();

    std::array<Binding, kActionCount> bindings_{};
    std::bitset<kActionCount> held_{};
    std::bitset<kActionCount> pressed_{};
    std::bitset<kActionCount> released_{};
    std::unordered_set<SDL_Keycode> downKeys_;
    std::vector<Transition> pending_;
};

}  // namespace galaxian
