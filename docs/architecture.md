# Galaxian Clone — Architecture

Companion to `game_spec.md`. Describes module layout, layering rules, and the
key subsystem designs. Updated when a stage lands; frozen parts are marked.

---

## 1. Layering and Dependency Rule

```text
Gameplay (Player, Enemy, Projectile, WaveManager, AttackDirector, ...)
        ↓ depends on
Core abstractions (Renderer, InputManager, AudioManager, GameClock, math)
        ↓ depends on
SDL2
```

Rules:

1. **Gameplay code never includes SDL headers.** It sees `Action` enums,
   `Renderer` calls, `AudioManager` ids — nothing else.
2. Only the leaf modules (`graphics/`, `input/`, `audio/`, `core/Game`)
   know that SDL exists.
3. Rendering never contains game logic (no score changes, no state changes
   inside draw code). Debug overlays are the only exception and are isolated
   in `graphics/`.
4. All timing goes through `GameClock`; no `SDL_GetTicks()` in gameplay.

This keeps the entire gameplay layer unit-testable headlessly (no window,
no display, no audio device).

## 2. Module Layout (target state, mid-development)

```text
src/
├── main.cpp                  # entry point: parse args, Game lifecycle
├── core/
│   ├── Game.{hpp,cpp}        # owns subsystems, main loop, state machine
│   ├── GameClock.{hpp,cpp}   # fixed timestep (1/60 s), accumulator
│   ├── Types.hpp             # Vector2, Rect, shared enums
│   └── Constants.hpp         # logical resolution, timestep
├── graphics/
│   ├── Renderer.{hpp,cpp}    # SDL_Renderer wrapper, logical size, scaling
│   ├── Texture.{hpp,cpp}
│   ├── Animation.{hpp,cpp}   # AnimationClip / Animator (Stage 19)
│   └── DebugOverlay.{hpp,cpp}# FPS, collision boxes, state labels
├── input/
│   └── InputManager.{hpp,cpp}# Action-based, pressed/held/released
├── audio/
│   └── AudioManager.{hpp,cpp}# SoundId/MusicId, silent fallback
├── gameplay/
│   ├── Player.{hpp,cpp}
│   ├── Enemy.{hpp,cpp}            # + EnemyDefinition, EnemyState
│   ├── EnemyFormation.{hpp,cpp}
│   ├── AttackDirector.{hpp,cpp}
│   ├── Projectile.{hpp,cpp}       # shared player/enemy system
│   ├── Collision.{hpp,cpp}        # pure AABB math (header-only OK)
│   ├── DivePath.{hpp,cpp}         # Bézier/waypoint paths
│   ├── ScoreManager.{hpp,cpp}
│   └── WaveManager.{hpp,cpp}
├── states/
│   ├── GameState.hpp
│   ├── TitleState.{hpp,cpp}
│   ├── PlayState.{hpp,cpp}
│   ├── PauseState.{hpp,cpp}
│   └── GameOverState.{hpp,cpp}
└── persistence/
    └── HighScore.{hpp,cpp}
```

Tests mirror this: `tests/test_collision.cpp`, `tests/test_player.cpp`, etc.

## 3. Core Subsystems

### 3.1 Game loop and timing (Stage 2)

```text
processEvents()
acc += frameDelta
while (acc >= dt) { fixedUpdate(dt); acc -= dt; }   # dt = 1/60 s
render()
```

- Fixed simulation step: **1/60 s**.
- Accumulator capped (e.g. 5 steps) to prevent a spiral of death after stalls.
- All movement is `position += velocity * dt` in fixed updates; nothing is
  scaled by render frame rate.
- Debug overlay (F2): FPS, frame time, update count, entity count.

### 3.2 Renderer (Stage 3)

- Wraps `SDL_Renderer`; sets logical size 448×576; integer scaling when the
  window allows, letterbox otherwise.
- API: `drawSprite`, `drawRect`, `drawText`, `clear`, `present`.
- Nearest-neighbor filtering for pixel-art fidelity.
- Textures cached by path; sprites positioned in logical coordinates.

### 3.3 InputManager (Stage 4)

```cpp
enum class Action { MoveLeft, MoveRight, Fire, Pause, Start,
                    DebugCollision, DebugOverlay };
input.isHeld(Action::MoveLeft);
input.wasPressed(Action::Fire);
input.wasReleased(Action::MoveRight);
```

- Built from SDL keyboard events each frame; exposes per-frame
  pressed/held/released sets. Bindings are data (a table), not code.

### 3.4 Projectiles (Stage 6)

```cpp
struct Projectile {
    Vector2 position;
    Vector2 velocity;
    ProjectileOwner owner;   // Player | Enemy
    bool active;
};
```

- Single manager for both owners; spawn/move/cull/cooldown.
- Pool or vector with swap-remove; no per-frame allocations in steady state.

### 3.5 Collision (Stage 7)

- Pure function: `bool intersects(const Rect& a, const Rect& b)`.
- AABB only. Edge-touching counts as **no** collision (strict inequality).
- Every gameplay object exposes `bounds()`; debug overlay (F1) draws them.

### 3.6 Enemies and formation (Stages 8–13)

- `EnemyDefinition` (data): points, speed, sprite index. Types: Scout,
  Guard, Commander — data-driven, no subclasses unless logic diverges.
- `EnemyFormation` owns the grid: slot offsets + a single world position
  that oscillates. Enemy screen position = formation world pos + slot offset
  (or the dive path position while diving).
- `EnemyState` machine per enemy:
  `Formation → PreparingDive → Diving → Attacking → Returning → Formation`,
  plus `Dead` reachable from any living state.
- `DivePath`: cubic Bézier / waypoint parameterization; `PathFollower`
  advances `t` by speed*dt. No hard-coded frame tables.
- `AttackDirector`: sole authority for who dives, when, and how many.
  Bounded by wave parameters; never selects dead/diving enemies; skips
  gracefully when nobody is eligible.

### 3.7 Player (Stages 5, 15)

- Horizontal movement, screen-clamped, velocity-based.
- State machine: `Alive → Dying → Respawning → Invulnerable → Alive`,
  `GameOver` terminal for the run.
- Death is idempotent per frame: multiple collisions in one frame remove
  exactly one life.

### 3.8 Waves and scoring (Stages 9, 16, 18)

- `WaveManager`: detects formation cleared, runs interstitial, spawns next
  formation with bounded difficulty parameters.
- `ScoreManager`: single source of truth for score, high score, and the
  score table (points per type, dive multiplier, wave bonus).

### 3.9 Game states (Stage 17)

```cpp
enum class GameState { Title, Playing, Paused, GameOver };
```

- `Game` holds the current state object; transitions are explicit and
  validated. Entering `Playing` constructs a fresh `PlayState` (no stale
  entities); `Paused` freezes the same `PlayState` (no re-simulation).

### 3.10 Audio (Stage 20)

- `AudioManager::playSound(SoundId)`, `playMusic(MusicId)`.
- Ids map to files in one place (audio module). Silent no-op fallback when
  `SDL_Init(SDL_INIT_AUDIO)` fails.

### 3.11 Persistence (Stage 22)

- `HighScore`: load/save `~/.local/share/galaxian-clone/highscore.dat`.
- Corrupt/missing file → 0. Atomic write (write temp + rename).

## 4. Testing Strategy

- **Catch2** (v3) for unit tests; tests are plain executables built with
  CTest (`ctest --test-dir build`).
- Logic under test (no window needed): collision math, player movement/clamp,
  projectile lifecycle, enemy state machine, dive paths, attack director,
  scoring, wave manager, high score file handling.
- Timing tests run the fixed-update loop headlessly at simulated 30/60/120 Hz
  and compare results.
- Manual acceptance: per-stage checklists in `test_plan.md`, executed on a
  real display (or `SDL_VIDEODRIVER=dummy` smoke runs in CI).
- Stage 25: ASan+UBSan build, valgrind, 30-minute soak, rapid-restart stress.

## 5. Build

- CMake ≥ 3.20, C++20, `-Wall -Wextra -Wpedantic`, zero warnings.
- `Debug` (default for dev) and `Release` builds; an `Sanitize` build type
  adds `-fsanitize=address,undefined`.
- Find SDL2 via its CMake config (Ubuntu `libsdl2-dev` ships it).

## 6. Milestone Discipline

- Every accepted stage is git-tagged (`stage-NN-<name>`).
- **A stage is never tagged while it has a known failing acceptance test.**
- Stage order is the dependency map in the project plan; no skipping ahead.
