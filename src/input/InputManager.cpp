#include "InputManager.hpp"

#include <algorithm>

namespace galaxian {

InputManager::InputManager()
{
    // Default bindings (docs/game_spec.md §4). Set in the constructor so the
    // manager works out of the box (headless tests never call initialize()).
    // Braced lists are passed by value and consumed immediately, which is
    // safe (returning an initializer_list by value would not be).
    setBinding(Action::MoveLeft, {SDLK_LEFT, SDLK_a});
    setBinding(Action::MoveRight, {SDLK_RIGHT, SDLK_d});
    setBinding(Action::Fire, {SDLK_SPACE});
    setBinding(Action::Start, {SDLK_RETURN});
    setBinding(Action::Pause, {SDLK_ESCAPE});
    setBinding(Action::DebugCollision, {SDLK_F1});
    setBinding(Action::DebugOverlay, {SDLK_F2});
    setBinding(Action::DebugDive, {SDLK_F3});
}

void InputManager::initialize()
{
    // Idempotent: clear any stale transient state. Bindings are preserved so
    // a remap set before initialize() is not clobbered.
    downKeys_.clear();
    held_.reset();
    pressed_.reset();
    released_.reset();
    pending_.clear();

    // If the video subsystem (and thus the keyboard) is already up, sync the
    // initial keyboard state. Keys physically held at startup are reported
    // as held but produce no press edge (the original key-down was not
    // observed). The Renderer normally initializes SDL before this runs; if
    // it is not up yet we simply start with no keys down.
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        const Uint8* state = SDL_GetKeyboardState(nullptr);
        if (state != nullptr) {
            for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc) {
                if (state[sc] != 0) {
                    const SDL_Keycode key =
                        SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(sc));
                    if (key != SDLK_UNKNOWN) {
                        downKeys_.insert(key);
                    }
                }
            }
            refreshHeld();
        }
    }
}

bool InputManager::pollEvents()
{
    bool quit = false;

    // Injected transitions first (headless test hooks), then real SDL
    // events. Injected keys are applied in the order they were queued.
    for (const Transition& t : pending_) {
        if (t.down) {
            applyKeyDown(t.key);
        } else {
            applyKeyUp(t.key);
        }
    }
    pending_.clear();

    // SDL_PollEvent is only meaningful when the video subsystem is up;
    // headless state-machine tests run without it.
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                // Ignore auto-repeat: only the initial key-down is a press.
                if (event.key.repeat == 0) {
                    applyKeyDown(event.key.keysym.sym);
                }
                break;
            case SDL_KEYUP:
                applyKeyUp(event.key.keysym.sym);
                break;
            case SDL_WINDOWEVENT:
                // Losing focus releases all keys (SDL stops delivering
                // key-ups for keys held while the window is unfocused).
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    releaseAll();
                }
                break;
            default:
                break;
            }
        }
    }

    return quit;
}

void InputManager::endFrame()
{
    pressed_.reset();
    released_.reset();
}

bool InputManager::isHeld(Action action) const
{
    const int i = static_cast<int>(action);
    return i >= 0 && i < kActionCount && held_.test(i);
}

bool InputManager::wasPressed(Action action) const
{
    const int i = static_cast<int>(action);
    return i >= 0 && i < kActionCount && pressed_.test(i);
}

bool InputManager::wasReleased(Action action) const
{
    const int i = static_cast<int>(action);
    return i >= 0 && i < kActionCount && released_.test(i);
}

void InputManager::setBinding(Action action, std::initializer_list<SDL_Keycode> keys)
{
    const int i = static_cast<int>(action);
    if (i < 0 || i >= kActionCount) {
        return;
    }
    Binding& binding = bindings_[i];
    binding = Binding{};
    for (const SDL_Keycode key : keys) {
        if (binding.count < static_cast<int>(binding.keys.size())) {
            binding.keys[binding.count++] = key;
        }
    }
    // Remapping is a configuration change, not user input: update the held
    // state without generating press/release edges.
    refreshHeld();
}

const std::array<SDL_Keycode, 4>& InputManager::bindingKeys(Action action) const
{
    static const std::array<SDL_Keycode, 4> empty{};
    const int i = static_cast<int>(action);
    if (i < 0 || i >= kActionCount) {
        return empty;
    }
    return bindings_[i].keys;
}

int InputManager::bindingCount(Action action) const
{
    const int i = static_cast<int>(action);
    if (i < 0 || i >= kActionCount) {
        return 0;
    }
    return bindings_[i].count;
}

void InputManager::injectKeyDown(SDL_Keycode key)
{
    pending_.push_back(Transition{key, true});
}

void InputManager::injectKeyUp(SDL_Keycode key)
{
    pending_.push_back(Transition{key, false});
}

void InputManager::applyKeyDown(SDL_Keycode key)
{
    if (downKeys_.count(key) != 0) {
        return;  // Already down (repeat or duplicate): no new press edge.
    }
    downKeys_.insert(key);

    const std::bitset<kActionCount> heldBefore = held_;
    refreshHeld();
    for (int i = 0; i < kActionCount; ++i) {
        if (!heldBefore.test(i) && held_.test(i)) {
            pressed_.set(i);
        }
    }
}

void InputManager::applyKeyUp(SDL_Keycode key)
{
    if (downKeys_.erase(key) == 0) {
        return;  // Was not down: ignore.
    }

    const std::bitset<kActionCount> heldBefore = held_;
    refreshHeld();
    for (int i = 0; i < kActionCount; ++i) {
        if (heldBefore.test(i) && !held_.test(i)) {
            released_.set(i);
        }
    }
}

void InputManager::releaseAll()
{
    if (downKeys_.empty()) {
        return;
    }
    downKeys_.clear();

    const std::bitset<kActionCount> heldBefore = held_;
    refreshHeld();
    for (int i = 0; i < kActionCount; ++i) {
        if (heldBefore.test(i)) {
            released_.set(i);
        }
    }
}

void InputManager::refreshHeld()
{
    held_.reset();
    for (int i = 0; i < kActionCount; ++i) {
        const Binding& binding = bindings_[i];
        for (int k = 0; k < binding.count; ++k) {
            if (downKeys_.count(binding.keys[k]) != 0) {
                held_.set(i);
                break;
            }
        }
    }
}

}  // namespace galaxian
