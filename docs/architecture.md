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
│   ├── Combat.{hpp,cpp}           # bullet vs enemy resolution (Stage 9)
│   ├── Effects.{hpp,cpp}          # placeholder FX (Stage 9; Stage 19 art)
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
  Stage 2 implements this as a console line (one per second); Stage 3
  replaces it with an on-screen overlay once text rendering exists.

### 3.2 Renderer (Stage 3)

- Wraps `SDL_Renderer`, the window, and the font. All drawing is in logical
  coordinates (448×576); the window is scaled and letterboxed so gameplay
  coordinates never depend on window size.
- Scaling is applied **manually** in the draw calls (recompute an integer
  scale + centered offset from the live window size on every `clear`, then
  transform each rect/position). SDL's own `SDL_RenderSetLogicalSize` is not
  used: under the software renderer it top-left-anchors the content, leaves
  the letterbox bars transparent, and produces a blank frame when the window
  is smaller than the logical size.
- API: `drawSprite`, `drawRect`, `drawFilledRect`, `drawText`, `clear`,
  `present`, `texture` (cached load), `registerTexture`.
- Nearest-neighbor filtering for pixel-art fidelity (`SDL_ScaleModeNearest`).
- Textures cached by id/path; sprites positioned in logical coordinates.
- Text via SDL_ttf with a per-size font cache and a rendered-text cache;
  degrades gracefully (text disabled) if no TTF font is found.

### 3.3 InputManager (Stage 4)

```cpp
enum class Action { MoveLeft, MoveRight, Fire, Start, Pause,
                    DebugCollision, DebugOverlay, Count };
input.isHeld(Action::MoveLeft);     // level: true while any bound key is down
input.wasPressed(Action::Fire);     // edge: true for exactly one frame
input.wasReleased(Action::MoveRight); // edge: true for exactly one frame
```

- **Actions, not keys.** Gameplay sees `Action` only; the `SDLK_*` → `Action`
  mapping is a data table (default bindings from spec §4, overridable with
  `setBinding`). `src/input/Actions.hpp` is SDL-free so it can be included
  anywhere; `InputManager` is the only place that touches SDL key events.
- **Frame protocol** (driven by `Game::run`):
  ```text
  input.pollEvents()   // drain SDL events → update held/pressed/released
  ... read input (isHeld / wasPressed / wasReleased) during the frame ...
  input.endFrame()     // clear the pressed/released edge sets (once/frame)
  ```
  `pollEvents()` returns `true` on `SDL_QUIT`. `endFrame()` is called exactly
  once per frame, **after** all input has been read, so an edge is visible for
  exactly one frame.
- **Transition semantics.** `pressed_` is set when an action's held state goes
  false→true; `released_` when it goes true→false. A key-down for an already
  held action is ignored (no re-trigger). This is what makes `wasPressed` a
  clean single-frame edge even under OS key auto-repeat (repeat events are
  dropped).
- **Multi-key bindings.** An action with several bound keys is held while any
  one is down; it is released only when the last one is let go. Pressing a
  second key of the same action does not re-press.
- **Simultaneous left+right.** Both actions report held; the gameplay layer
  resolves the tie as **cancel** (net-zero movement) — the input layer reports
  the raw state and does not pick a winner.
- **Focus loss** (`SDL_WINDOWEVENT_FOCUS_LOST`) releases all keys so nothing
  stays "held" while the window is unfocused.
- **Headless-safe.** `SDL_PollEvent` is only called when `SDL_INIT_VIDEO` is
  up; otherwise injected events (`injectKeyDown`/`injectKeyUp`, test hooks)
  drive the same state machine, so the whole layer is unit-testable with no
  display.

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

Stage 6 implements the player side (`gameplay/Projectile.{hpp,cpp}`):

- **SDL-free** (dependency rule, §1): `ProjectileManager` is pure logic,
  driven by the fixed `dt` (§3.1). `Game` calls `tryFirePlayer(player)` on
  the consumed fire edge and `update(dt)` once per fixed step.
- **`position` is the box's top-left corner** (the `Rect` convention), so
  `position` is also the sprite draw position. The dev bullet is `4×10`
  (`Projectile::kWidth/kHeight`), coinciding with the box.
- **Player fire (`tryFirePlayer`):** spawns a bullet directly above the
  player (center-x aligned, bullet bottom edge touching the player's top
  edge) with velocity `(0, -480)` (`kPlayerSpeed`, spec §5). Rejected (and
  nothing spawned) when the player is dead, the cooldown is active, the
  player already has `kMaxPlayerProjectiles = 2` bullets on screen, or the
  pool is full. The `0.35 s` cooldown (`kFireCooldownSeconds`) is set only
  on a successful spawn and is a per-owner timer decremented in `update`.
- **Culling:** a player bullet is removed once its box is fully above the
  top edge (`bottom() < 0`); an enemy bullet once fully below the bottom
  edge (`top() > kLogicalHeight`). Strict inequalities: a partially visible
  bullet is never culled. Enemy fire itself lands in Stage 14, but the
  `Enemy` owner and its cull edge are already supported.
- **Pool:** fixed `kMaxProjectiles = 16` array with swap-remove; zero heap
  allocation, so firing 10 000 shots returns the count to 0 with stable
  memory (verified under ASan/UBSan).

### 3.5 Collision (Stage 7)

- Pure function: `bool intersects(const Rect& a, const Rect& b)`.
- AABB only. Edge-touching counts as **no** collision (strict inequality).
- Every gameplay object exposes `bounds()`; debug overlay (F1) draws them.

Stage 7 implements the rule and the debug overlay:

- **`gameplay/Collision.hpp`** (header-only, SDL-free): `constexpr bool
  intersects(const Rect& a, const Rect& b)` — positive-area intersection
  (`min(right) - max(left) > 0` on both axes). Symmetric, origin-independent
  (negative coordinates fine); a degenerate (zero-width/zero-height) box
  never collides. This is the only place collision rules live.
- **`graphics/DebugOverlay.{hpp,cpp}`**: the isolated rendering hook.
  `DebugOverlay::drawCollisionBoxes(renderer, boxes, color)` draws 1-px
  outlines around the boxes it is given — no game logic, no collision rules.
  `Game` owns the F1 (`Action::DebugCollision`) toggle and collects the live
  boxes (player + projectiles) to hand over.
- **Enforcement:** the `no_collision_in_graphics` CTest greps `src/graphics`
  for the collision function name, so the dependency rule is checked on
  every test run (alongside `no_sdlk_in_gameplay`).

### 3.6 Enemies and formation (Stages 8–13)

Stage 8 implements the static formation (`gameplay/Enemy.{hpp,cpp}`,
`gameplay/EnemyFormation.{hpp,cpp}`):

- **SDL-free** (dependency rule, §1): the formation is pure data updated in
  the fixed-timestep simulation; the spriteIndex → texture mapping lives in
  `Game` (the composition root), which hands graphics/ the textures to draw.
- **Data-driven types** (spec §6.1): `EnemyType` + the
  `kEnemyDefinitions` table (points, speed, sprite index); no subclasses.
  Speeds are nominal placeholders until Stage 23 tuning (spec-frozen values
  are points and sprite index only).
- **Grid** (spec §6.2): 5 rows x 8 columns = 40 enemies in a fixed 40-slot
  row-major array (no heap); slot offsets on a 48 px column / 36 px row
  lattice, top-left anchor (32, 64). Screen position = formation world
  position + slot offset; the world position is static in Stage 8 and is
  what Stage 10 oscillates.
- **Minimal state**: `EnemyState` is the `Formation`/`Dead` pair in Stage 8
  (mirroring the Stage 5 Player); Stage 11 expands it to the full machine.

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

Stage 5 implements the prototype (`gameplay/Player.{hpp,cpp}`):

- **SDL-free** (dependency rule, §1): the Player never sees SDL keys or the
  `InputManager`. Movement is driven by a `direction` in `{-1, 0, +1}` that
  `Game` computes from the named Actions (left/right cancel to 0, spec §4);
  firing is a plain `fire()` command.
- **Horizontal-only, velocity-based, screen-clamped** (spec §5):
  `position += direction * kSpeed * dt` with the fixed `dt`
  (§3.1), so motion is frame-rate independent. `kSpeed = 220 px/s`,
  start center `(224, 528)`, collision box `24×16` centered on the sprite
  (`bounds()`); the center x is clamped to `[12, 436]` so the box never
  leaves the screen. y is never changed.
- **Fire event:** `fire()` increments a fire count (the event); `Game` logs
  `"Player fired"`. No projectile yet (Stage 6). A dead player neither moves
  nor fires.
- **State:** minimal `PlayerState { Alive, Dead }` with `kill()`/`respawn()`.

Stage 15 expands this to the full state machine:
`Alive → Dying → Respawning → Invulnerable → Alive`, `GameOver` terminal for
the run, with lives, a respawn timer, and invulnerability. Death is idempotent
per frame: multiple collisions in one frame remove exactly one life.

### 3.8 Waves and scoring (Stages 9, 16, 18)

Stage 9 implements the combat chain and the scoring core
(`gameplay/Combat.{hpp,cpp}`, `gameplay/ScoreManager.{hpp,cpp}`,
`gameplay/Effects.{hpp,cpp}`):

- **SDL-free** (dependency rule, §1): `combat::resolvePlayerBullets` is pure
  logic that `Game` calls once per fixed step, **after**
  `ProjectileManager::update(dt)`, so it sees each bullet's new position.
  The F2 stats overlay shows the score; the full HUD (Stage 18) and high
  score (Stage 22) build on `ScoreManager`.
- **One bullet, one kill:** for each Player-owned bullet (pool order), the
  first living enemy whose box intersects the bullet box — a row-major scan
  of the grid (spec §6.2) — is killed and the bullet is **consumed**
  (`ProjectileManager::removeAt`, swap-remove). A 4×10 bullet can overlap at
  most one 24×24 box of the spec grid, so the rule is also structural. A
  bullet that overlaps only dead enemies passes through untouched (holes,
  spec §6.3), and a dead enemy is never scored twice.
- **Ownership:** enemy-owned bullets are skipped (spec §8); Stage 14
  resolves them against the player.
- **`ScoreManager`:** score + kill count. `addKill(type, multiplier = 1)`
  reads the type's base points from the `kEnemyDefinitions` table (spec
  §6.1) — no duplicated score table. The multiplier is 1 in Stage 9; Stage
  11+ passes 2 for diving enemies (spec §6.4). `addPoints` covers wave
  bonuses; `reset` starts a fresh game.
- **`EffectManager`:** the placeholder destruction effect — a `0.25 s` box
  at the kill site, fixed 16-slot pool (a full pool fails an add
  gracefully), swap-remove on expiry. Stage 19 replaces it with the
  explosion animation; the gameplay code that spawns it stays the same.

The deferred pieces land later:

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
