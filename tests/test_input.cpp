// Stage 4 input tests (docs/test_plan.md, Stage 4).
//
// Two groups:
//
//  1. Headless state-machine tests: drive the InputManager with injected key
//     transitions (no display, no SDL init) and verify the held/pressed/
//     released semantics, multi-key bindings, remapping, and the spec'd
//     simultaneous-left+right resolution.
//
//  2. [sdl] integration tests: run under the SDL dummy video driver and push
//     synthetic SDL events to verify the SDL-event -> state-machine wiring
//     (key auto-repeat, SDL_QUIT, window focus loss).

#include <catch2/catch_test_macros.hpp>

#include <SDL2/SDL.h>

#include <string>

#include "input/Actions.hpp"
#include "input/InputManager.hpp"

using namespace galaxian;

namespace {

void useDummyVideoDriver() { ::setenv("SDL_VIDEODRIVER", "dummy", 1); }

// RAII helper for headless state-machine tests: applies any queued key
// transitions on construction (pollEvents) and clears the per-frame edges on
// destruction (endFrame). The body of the scope represents "one frame".
struct Frame {
    InputManager& input;
    explicit Frame(InputManager& in) : input(in) { (void)in.pollEvents(); }
    ~Frame() { input.endFrame(); }
};

// RAII guard for [sdl] tests: brings up the dummy video driver so the SDL
// event queue works, and tears SDL down on scope exit.
struct SdlVideoGuard {
    SdlVideoGuard() {
        useDummyVideoDriver();
        (void)SDL_Init(SDL_INIT_VIDEO);
    }
    ~SdlVideoGuard() { SDL_Quit(); }
};

// Pushes a synthetic keyboard event into the SDL queue.
void pushKey(SDL_Keycode key, bool down, int repeat = 0)
{
    SDL_Event ev{};
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.type = ev.type;
    ev.key.windowID = 1;
    ev.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    ev.key.repeat = repeat;
    ev.key.keysym.sym = key;
    ev.key.keysym.scancode = SDL_SCANCODE_UNKNOWN;
    SDL_PushEvent(&ev);
}

// Drains any events already in the queue (e.g. spurious init events).
void drain(InputManager& input)
{
    (void)input.pollEvents();
    input.endFrame();
}

}  // namespace

// ---------------------------------------------------------------------------
// Headless state-machine tests (no SDL).
// ---------------------------------------------------------------------------

TEST_CASE("input: wasPressed is true for exactly one frame on keydown",
          "[input]")
{
    InputManager input;

    // Frame 1: nothing pressed.
    {
        Frame frame(input);
        CHECK_FALSE(input.wasPressed(Action::Fire));
        CHECK_FALSE(input.isHeld(Action::Fire));
    }

    // Inject a key-down; frame 2 sees the press.
    input.injectKeyDown(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
        CHECK(input.wasPressed(Action::Fire));
    }

    // Frame 3: key still down, no new event -> held but NOT pressed again.
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
        CHECK_FALSE(input.wasPressed(Action::Fire));
    }
}

TEST_CASE("input: isHeld is true while held and false after release", "[input]")
{
    InputManager input;

    input.injectKeyDown(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
    }
    // Still held across frames with no new events.
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
    }

    input.injectKeyUp(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK_FALSE(input.isHeld(Action::Fire));
    }
    {
        Frame frame(input);
        CHECK_FALSE(input.isHeld(Action::Fire));
    }
}

TEST_CASE("input: wasReleased is true for exactly one frame on keyup",
          "[input]")
{
    InputManager input;
    input.injectKeyDown(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
        CHECK_FALSE(input.wasReleased(Action::Fire));
    }

    input.injectKeyUp(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK(input.wasReleased(Action::Fire));
        CHECK_FALSE(input.isHeld(Action::Fire));
        CHECK_FALSE(input.wasPressed(Action::Fire));
    }

    // Frame after the release: edge is gone.
    {
        Frame frame(input);
        CHECK_FALSE(input.wasReleased(Action::Fire));
    }
}

TEST_CASE("input: simultaneous left+right are both held and cancel", "[input]")
{
    InputManager input;
    input.injectKeyDown(SDLK_LEFT);
    input.injectKeyDown(SDLK_RIGHT);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::MoveLeft));
        CHECK(input.isHeld(Action::MoveRight));
        // Spec'd resolution (docs/game_spec.md §4): net direction cancels to
        // zero when both are held.
        float dir = 0.0f;
        if (input.isHeld(Action::MoveRight)) {
            dir += 1.0f;
        }
        if (input.isHeld(Action::MoveLeft)) {
            dir -= 1.0f;
        }
        CHECK(dir == 0.0f);
    }

    // Releasing one leaves the other held (no spurious release of the rest).
    input.injectKeyUp(SDLK_LEFT);
    {
        Frame frame(input);
        CHECK_FALSE(input.isHeld(Action::MoveLeft));
        CHECK(input.isHeld(Action::MoveRight));
        CHECK(input.wasReleased(Action::MoveLeft));
        CHECK_FALSE(input.wasReleased(Action::MoveRight));
    }
}

TEST_CASE("input: a repeated key-down does not retrigger a press", "[input]")
{
    InputManager input;
    // Two key-downs for the same key (simulates auto-repeat at the
    // state-machine level): only the first is a press.
    input.injectKeyDown(SDLK_SPACE);
    input.injectKeyDown(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
        CHECK(input.wasPressed(Action::Fire));
    }
    // The duplicate did not queue a second press for the next frame.
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
        CHECK_FALSE(input.wasPressed(Action::Fire));
    }
}

TEST_CASE("input: multi-key binding release semantics", "[input]")
{
    InputManager input;
    // MoveLeft is bound to Left Arrow and A.

    // Press Left: pressed + held.
    input.injectKeyDown(SDLK_LEFT);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::MoveLeft));
        CHECK(input.wasPressed(Action::MoveLeft));
    }

    // Press A (second key of the same action): still held, NO new press.
    input.injectKeyDown(SDLK_a);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::MoveLeft));
        CHECK_FALSE(input.wasPressed(Action::MoveLeft));
    }

    // Release Left: A is still down, so still held and NOT released.
    input.injectKeyUp(SDLK_LEFT);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::MoveLeft));
        CHECK_FALSE(input.wasReleased(Action::MoveLeft));
    }

    // Release A (the last key): now released.
    input.injectKeyUp(SDLK_a);
    {
        Frame frame(input);
        CHECK_FALSE(input.isHeld(Action::MoveLeft));
        CHECK(input.wasReleased(Action::MoveLeft));
    }
}

TEST_CASE("input: default bindings match the spec", "[input]")
{
    InputManager input;

    CHECK(input.bindingCount(Action::MoveLeft) == 2);
    CHECK(input.bindingKeys(Action::MoveLeft)[0] == SDLK_LEFT);
    CHECK(input.bindingKeys(Action::MoveLeft)[1] == SDLK_a);

    CHECK(input.bindingCount(Action::MoveRight) == 2);
    CHECK(input.bindingKeys(Action::MoveRight)[0] == SDLK_RIGHT);
    CHECK(input.bindingKeys(Action::MoveRight)[1] == SDLK_d);

    CHECK(input.bindingCount(Action::Fire) == 1);
    CHECK(input.bindingKeys(Action::Fire)[0] == SDLK_SPACE);

    CHECK(input.bindingCount(Action::Start) == 1);
    CHECK(input.bindingKeys(Action::Start)[0] == SDLK_RETURN);

    CHECK(input.bindingCount(Action::Pause) == 1);
    CHECK(input.bindingKeys(Action::Pause)[0] == SDLK_ESCAPE);

    CHECK(input.bindingCount(Action::DebugCollision) == 1);
    CHECK(input.bindingKeys(Action::DebugCollision)[0] == SDLK_F1);

    CHECK(input.bindingCount(Action::DebugOverlay) == 1);
    CHECK(input.bindingKeys(Action::DebugOverlay)[0] == SDLK_F2);
}

TEST_CASE("input: setBinding remaps an action", "[input]")
{
    InputManager input;
    input.setBinding(Action::Fire, {SDLK_x});
    CHECK(input.bindingCount(Action::Fire) == 1);
    CHECK(input.bindingKeys(Action::Fire)[0] == SDLK_x);

    // Space no longer fires.
    input.injectKeyDown(SDLK_SPACE);
    {
        Frame frame(input);
        CHECK_FALSE(input.isHeld(Action::Fire));
    }

    // 'x' now fires.
    input.injectKeyDown(SDLK_x);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
        CHECK(input.wasPressed(Action::Fire));
    }
}

TEST_CASE("input: unbound keys have no effect", "[input]")
{
    InputManager input;
    // SDLK_z is not bound to any action by default.
    input.injectKeyDown(SDLK_z);
    {
        Frame frame(input);
        for (int i = 0; i < kActionCount; ++i) {
            CHECK_FALSE(input.isHeld(static_cast<Action>(i)));
            CHECK_FALSE(input.wasPressed(static_cast<Action>(i)));
        }
    }
}

TEST_CASE("input: initialize resets state but preserves bindings", "[input]")
{
    InputManager input;
    input.setBinding(Action::Fire, {SDLK_x});

    input.injectKeyDown(SDLK_x);
    {
        Frame frame(input);
        CHECK(input.isHeld(Action::Fire));
    }

    // Re-initialize: transient state resets, the remap is preserved.
    input.initialize();
    {
        Frame frame(input);
        CHECK_FALSE(input.isHeld(Action::Fire));
        CHECK(input.bindingCount(Action::Fire) == 1);
        CHECK(input.bindingKeys(Action::Fire)[0] == SDLK_x);
    }
}

TEST_CASE("input: actionName returns stable labels", "[input]")
{
    CHECK(std::string(actionName(Action::MoveLeft)) == "MoveLeft");
    CHECK(std::string(actionName(Action::Fire)) == "Fire");
    CHECK(std::string(actionName(Action::DebugOverlay)) == "DebugOverlay");
}

// ---------------------------------------------------------------------------
// [sdl] integration tests (dummy video driver + synthetic SDL events).
// ---------------------------------------------------------------------------

TEST_CASE("input: SDL key events drive the state machine", "[input][sdl]")
{
    SdlVideoGuard guard;
    InputManager input;
    drain(input);

    // KEYDOWN Space -> Fire pressed + held.
    pushKey(SDLK_SPACE, /*down=*/true);
    REQUIRE_FALSE(input.pollEvents());
    CHECK(input.isHeld(Action::Fire));
    CHECK(input.wasPressed(Action::Fire));
    input.endFrame();

    // KEYUP Space -> Fire released, no longer held.
    pushKey(SDLK_SPACE, /*down=*/false);
    REQUIRE_FALSE(input.pollEvents());
    CHECK_FALSE(input.isHeld(Action::Fire));
    CHECK(input.wasReleased(Action::Fire));
    CHECK_FALSE(input.wasPressed(Action::Fire));
    input.endFrame();
}

TEST_CASE("input: SDL_QUIT is reported by pollEvents", "[input][sdl]")
{
    SdlVideoGuard guard;
    InputManager input;
    drain(input);

    // No quit yet.
    CHECK_FALSE(input.pollEvents());
    input.endFrame();

    SDL_Event quit{};
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);

    CHECK(input.pollEvents());  // true: quit requested this frame
    input.endFrame();
}

TEST_CASE("input: window focus loss releases all keys", "[input][sdl]")
{
    SdlVideoGuard guard;
    InputManager input;
    drain(input);

    // Hold two keys.
    pushKey(SDLK_LEFT, /*down=*/true);
    pushKey(SDLK_SPACE, /*down=*/true);
    (void)input.pollEvents();
    CHECK(input.isHeld(Action::MoveLeft));
    CHECK(input.isHeld(Action::Fire));
    input.endFrame();

    // Focus lost -> every held action is released.
    SDL_Event focus{};
    focus.type = SDL_WINDOWEVENT;
    focus.window.type = SDL_WINDOWEVENT;
    focus.window.windowID = 1;
    focus.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    SDL_PushEvent(&focus);

    (void)input.pollEvents();
    CHECK_FALSE(input.isHeld(Action::MoveLeft));
    CHECK_FALSE(input.isHeld(Action::Fire));
    CHECK(input.wasReleased(Action::MoveLeft));
    CHECK(input.wasReleased(Action::Fire));
    input.endFrame();
}

TEST_CASE("input: SDL key auto-repeat does not retrigger a press",
          "[input][sdl]")
{
    SdlVideoGuard guard;
    InputManager input;
    drain(input);

    // Initial key-down (repeat=0): a press.
    pushKey(SDLK_SPACE, /*down=*/true, /*repeat=*/0);
    (void)input.pollEvents();
    CHECK(input.wasPressed(Action::Fire));
    CHECK(input.isHeld(Action::Fire));
    input.endFrame();

    // Auto-repeat (repeat=1): still held, but no new press.
    pushKey(SDLK_SPACE, /*down=*/true, /*repeat=*/1);
    (void)input.pollEvents();
    CHECK_FALSE(input.wasPressed(Action::Fire));
    CHECK(input.isHeld(Action::Fire));
    input.endFrame();
}
