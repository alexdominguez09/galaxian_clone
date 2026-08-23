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

Stage 10 sets the formation in motion (`EnemyFormation::update(dt)`, called
by `Game` inside the fixed-timestep simulation after the projectiles move
and before combat resolution):

- **Oscillation** (spec §6.3): the world position sways sinusoidally around
  the anchor, `x = kAnchor.x + 32·sin(phase)` — the spec's "amplitude 64 px"
  read as a 64 px peak-to-peak swing (±32), the only interpretation that
  keeps the whole 360 px-wide grid inside the 448 px screen at all times.
  Base period 4 s. Vertical variation (optional in the plan) is not
  implemented: y never changes.
- **Phase accumulation** on the fixed step (§3.1): frame-rate independent by
  construction and deterministic for identical step sequences; the phase
  wraps modulo 2π so long sessions cannot grow it without bound. x is
  recomputed from the phase every step (never incremented).
- **Pressure speed-up** (spec §6.3):
  `multiplier = 1 + 1.5 · (1 − alive/40) ∈ [1.0, 2.5]` — linear in deaths,
  hitting exactly the spec bound of 2.5× base speed when everything is
  dead; dead enemies do not move but still count towards the multiplier.

Stage 11 gives every enemy the full spec §6.4 state machine
(`EnemyState {Formation, PreparingDive, Diving, Attacking, Returning,
Dead}`):

- **Validated transitions** (`Enemy::transitionTo`, table-driven via
  `isLegalTransition`): exactly the five chain edges
  `Formation → PreparingDive → Diving → Attacking → Returning → Formation`
  plus "any living state → Dead". Dead is terminal; skips and
  self-transitions are rejected (the transition leaves the state unchanged).
- **State-aware bounds**: `Enemy::bounds(formationPosition)` reports the
  slot lattice box for Formation/PreparingDive/Dead members and the LIVE
  dive position for Diving/Attacking/Returning ones. Combat therefore hits
  divers where they actually are, `Game` draws them away from their empty
  slots, and the F1 overlay tracks them.
- **Simple paths** (the plan's "prove the machine first"; Stage 12 replaces
  the motion, not the machine): PreparingDive holds its slot for 0.5 s;
  Diving descends straight down at `definition().speed` (that is what the
  §6.1 speed column is for) until the box bottom reaches y = 480; Attacking
  dashes horizontally towards the nearer screen edge until fully
  off-screen; Returning homes to the LIVE slot position each step (so a
  swaying formation is tracked) and snaps bit-exact onto it when within one
  step's travel.
- **Ownership of motion**: `EnemyFormation::update(dt)` advances both the
  oscillation and every enemy's own machine against the freshly computed
  anchor position.
- **Debug aid** (`Action::DebugDive`, F3): sends the first idle formation
  enemy through a full dive cycle so the machine is observable on screen;
  F2 shows the state label (FORMATION/PREPARING/DIVING/ATTACKING/
  RETURNING) above every living enemy. Real selection authority is
  Stage 13's AttackDirector.

Stage 12 replaces the placeholder motion with real trajectories
(`gameplay/DivePath.{hpp,cpp}`):

- **`CubicBezier`**: the spec §6.4 parametric curve,
  `P(t) = (1−t)³P0 + 3(1−t)²tP1 + 3(1−t)t²P2 + t³P3`. Pure math; endpoint
  shortcuts make `P(0)` and `P(1)` bit-exact (the rejoin snap relies on
  this); interior points use de Casteljau. No hard-coded frame tables
  anywhere.
- **Four patterns** (`DivePattern`, deterministic relative offsets from the
  peel-off point, documented in DivePath.hpp): LeftDive / RightDive are
  exact mirrors that flip up-out of the slot then sweep down-outward;
  CenterAttack plunges through the middle; ReturnPath arcs from the attack
  end back up into the slot from its own side. All dives end 416 px below
  their start.
- **`PathFollower`**: advances t by PIXELS travelled —
  `t += pixels / arcLength` (16-chord sampled length, computed once),
  clamped to [0, 1] forever. World-space speed is therefore always
  `definition().speed` regardless of curve shape, and the advance depends
  only on accumulated distance: frame-rate independent by construction.
- **Dynamic return end**: a returning enemy evaluates its curve with the
  end re-targeted at the LIVE slot every step (`CubicBezier::withEnd`),
  so the tail tracks the oscillating formation while the shape stays put —
  and completion still snaps bit-exact onto the slot.
- **Selection for now**: `Enemy::beginDive(pattern)` takes the pattern
  explicitly (the F3 debug aid picks left/right from the enemy's screen
  half). Stage 13's AttackDirector owns the real choice.

Stage 13 adds the pacing authority (`gameplay/AttackDirector.{hpp,cpp}`,
spec §7):

- **Data-driven waves**: `waveParams(wave)` implements the spec §7 table —
  max simultaneous attackers 1→2→3→4 (the last rise at wave 7), attack
  interval 6 s → 4 s → floored at 3 s, shots per attack 1→2 (Stage 14
  consumes the fire budget). Nothing ever exceeds these bounds.
- **One launch per elapsed interval** (staggered attacks stay predictable):
  the timer accumulates dt; a launch fires on the first tick where BOTH
  the interval elapsed AND capacity remains below the cap, then resets.
  A 1 ns tolerance on the boundary keeps launches deterministic across
  platforms (same convention as the other simulation timers). Capacity or
  eligibility may delay a launch further — the interval is a minimum
  spacing, never violated early.
- **Deterministic selection**: front rows first (bottom-up scan) with a
  rotating column cursor that advances after every launch, so attacks
  spread across the grid. Only Formation-state enemies are eligible —
  dead and already-away enemies are never selected, so nobody dives twice
  concurrently. The dive pattern follows the slot's column band
  (outer columns sweep outward via LeftDive/RightDive, middle columns
  plunge with CenterAttack).
- **No deadlock**: when nobody is eligible (e.g. everything is dead or
  mid-dive) ticks simply skip; the timer keeps running and the next
  free+eligible moment fires. An empty formation idles safely forever.
- **Integration**: `Game::fixedUpdate` runs the director right after
  `formation_.update(dt)` (post-move states) with wave 1 defaults until
  Stage 16's wave system; F2/console stats gained an active-attack count;
  the F3 debug aid remains a manual bypass for development.

Stage 14 adds enemy fire on top of the shared projectile system
(spec §6.4/§8):

- **Budget flow**: the director passes its wave's `shotsPerAttack` (1–2)
  into `Enemy::beginDive(pattern, shots)`. The enemy raises deterministic
  fire EVENTS when its dive path crosses parametric trigger points — the
  midpoint for one-shot dives, t = 0.35/0.75 for two-shot dives — and the
  composition root drains them (`drainPendingShots()`) right after
  `formation_.update(dt)`. Enemies stay decoupled from Player and
  ProjectileManager.
- **`ProjectileManager::tryFireEnemy(muzzle, aimAt, speed)`**: spawns an
  Enemy-owned bullet centred on the muzzle (the diver's box bottom-centre)
  aimed at the player's position AT FIRE TIME; degenerate aims fall back
  straight down. Base speed 240 px/s (`kEnemySpeed`); wave scaling
  (bounded ≤ 360) lands with Stage 16. No cooldown — the per-dive budget
  is the cadence.
- **Ownership unchanged** (spec §8): enemy bullets move down and are
  culled below the screen (Stage 6 machinery); they can never damage
  enemies (`resolvePlayerBullets` skips Enemy-owned rounds). Player damage
  from enemy bullets is Stage 15's resolution step.
- The F3 debug aid grants the current wave's shot budget so manual dives
  behave like directed ones.

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

Stage 15 implements it (`gameplay/Player.{hpp,cpp}` +
`combat::resolveEnemyThreats`):

- **Lifecycle** (spec §5): `hit()` lands only while `vulnerable()`
  (= plain Alive) — it consumes one life and starts **Dying** with the
  full 1.5 s delay; multiple same-frame threats are absorbed because the
  ship stops being vulnerable after the first. When the delay expires,
  lives > 0 → transient **Respawning** (the composition root clears ALL
  enemy projectiles — the "nearby" interpretation — and calls
  `confirmRespawn()`), else **GameOver**, terminal until an external
  new-game reset (Stage 17).
- **Invulnerable** = at the start position for exactly 2.0 s: fully
  controllable (moves AND fires) but immune; bullets fly through
  unconsumed. Rendered blinking at ~4 Hz by `Game` (`simTime_`-driven);
  hidden during Dying/Respawning/GameOver.
- **`resolveEnemyThreats(projectiles, formation, player)`**: per fixed
  step, the FIRST overlapping threat — Enemy-owned bullet first, then any
  living enemy's state-aware body (divers count) — costs one life and is
  consumed; everything else passes through. Ownership rules hold both ways:
  Player bullets are never checked against the player, enemy bodies/bullets
  never damage enemies. The colliding enemy body itself survives (the
  frozen spec fixes only the player side).
- Boundary determinism: the 1.5 s / 2.0 s countdowns use the codebase-wide
  1 ns tolerance — expiry lands exactly on updates 90 / 120.

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

- `WaveManager` (Stage 16): detects formation cleared, runs the
  interstitial, spawns the next formation with bounded difficulty
  parameters — **implemented in Stage 16**, see below.
- High score display/persistence lands in Stages 18/22.

Stage 16 adds the wave lifecycle (`gameplay/WaveManager.{hpp,cpp}`,
spec §9):

- **Clear detection**: a wave counts as complete only when every enemy is
  DEAD (spec §9) — a lone diver still in flight blocks it. Detection runs
  as the LAST step of `Game::fixedUpdate`, seeing post-combat state.
- **Interstitial**: exactly 2.0 s ("WAVE CLEAR" center notice; expiry on
  the 120th fixed step via the codebase-wide 1 ns tolerance), then the
  manager itself rebuilds the formation (`reset()`: 40 living enemies at
  the anchor) and hands the new parameters to the AttackDirector
  (`beginWave(n+1)`). Score and player lives intentionally persist across
  waves.
- **Bounded difficulty** (spec §7/§8 caps): attackers ≤ 4, interval ≥ 3 s,
  shots per dive ≤ 2 via `waveParams`; enemy bullet speed ramps +40 px/s
  per completed wave from the 240 base, hard-capped at 360 px/s
  (`ProjectileManager::speedForWave`, used by the fire-drain site);
  formation speed stays self-adjusting via the Stage 10 death-pressure
  multiplier bounded at 2.5x. Nothing multiplies unboundedly.
- **HUD bits** (full HUD in Stage 18): a persistent WAVE counter top-right,
  the F2/console stats lines carry `wave=`, and the interstitial shows a
  center notice.

Stage 18 replaces those bits with the real HUD (`graphics/Hud.{hpp,cpp}`,
spec §11):

- **Isolated rendering module** (DebugOverlay style): `hud::drawTopBar` /
  `hud::drawLivesPips` take plain VALUES and draw them — no game logic, no
  score rules. `Game::renderPlayfield` feeds it live values every frame,
  so the display can never lag the simulation.
- **Layout** (spec §11): top bar SCORE / HIGH labels with zero-padded
  six-digit values (`%06d`), WAVE line under the left column, and life
  pips bottom-left — small font-free cyan triangles clamped to
  `[0, kLives]` (a display can never show more ships than exist). The
  interstitial keeps its center notice; the title screen shows the live
  session high.
- **Session high score**: ScoreManager tracks the peak automatically on
  every scoring event (`highScore()`); `reset()` intentionally KEEPS it —
  a new game starts at 0 while the session best stays up — and only
  `resetHighScore()` clears it. Disk persistence is Stage 22.
- **Scaffolding retired**: DevScene slimmed to the pure backdrop (border +
  help line); the static test bullets and per-stage title line are gone —
  their space is the HUD's now.

### 3.9 Game states (Stage 17)

```cpp
enum class GameState { Title, Playing, Paused, GameOver };
```

Stage 17 implements it (`src/states/GameState.{hpp,cpp}`,
`src/states/StateMachine.{hpp,cpp}`):

- **Validated graph** (`isLegalGameStateTransition`, constexpr): EXACTLY
  `Title→Playing`, `Playing→Paused`, `Paused→Playing`,
  `Playing→GameOver`, `GameOver→Title`. Skips, self-transitions and
  backwards jumps are rejected; the table is exhaustively unit-tested
  (16 pairs) with compile-time spot checks.
- **`StateMachine`**: holds the current id, accepts only legal requests,
  fires one callback AFTER each accepted change. `Game::onStateChanged`
  runs the enter bookkeeping: entering **Playing from Title** calls
  `startNewGame()` — score reset, player `resetGame()`, formation
  rebuilt, projectile/effect pools wiped, director + waves restarted at
  wave 1 (spec §10: no stale entities); **Paused→Playing** resumes the
  very same game untouched; entering **Title** resets to a clean state.
- **State-driven keys** (spec §4): Title = Enter starts / Escape quits;
  Playing = Escape pauses; Paused = Escape resumes; GameOver = Enter
  returns to Title. Window-close quits from anywhere.
- **Simulation gating**: only PLAYING runs `fixedUpdate`; Paused freezes
  everything while the timestep keeps draining so a resume never
  fast-forwards (verified via frozen update counters); Title/GameOver
  have no simulation. The run ends into GameOver when the player's last
  life is gone.
- **Per-state screens**: Title = logo/high-score placeholder/PRESS ENTER;
  GameOver = final score + wave reached + PRESS ENTER; Playing/Paused
  share the playfield render, Paused adds the overlay.
- Headless smoke runs skip the title (a composition-root decision in
  main.cpp — the Game class itself always boots to Title).

- `Game` holds the current state object; transitions are explicit and
  validated. Entering `Playing` constructs a fresh `PlayState` (no stale
  entities); `Paused` freezes the same `PlayState` (no re-simulation).

**Stage 19 animation** (`graphics/Animation.{hpp,cpp}`):

- **`AnimationClip`**: immutable frame data — texture ids (static
  DevArt ids), per-frame duration, loop flag. Pure data.
- **`Animator`**: plays one clip; a private clock advanced on the fixed
  simulation step maps time to a frame index. Looping clips wrap modulo
  the clip duration (the clock stays bounded forever); one-shot clips
  clamp to the last frame and report `finished()`. SDL-free logic with a
  thin draw() convenience.
- **Independence rule** (the plan's core requirement): gameplay logic
  SELECTS clips ("enemy state = Diving" -> "animation = EnemyDive"), but
  animation never drives physics — animators live on the graphics side
  (Game-owned arrays), are advanced inside the Playing-gated fixed step,
  and nothing in gameplay/ reads them. Bit-exact position equality is
  unit-tested with and without animator updates interleaved.
- **Production set**: player idle flicker, three enemy idles (type colour
  with a blinking white core), and a 4-frame explosion one-shot whose
  total duration equals the Stage 9 effect duration exactly (4 x 0.0625 =
  0.25 s) — Game progress-maps each effect's remaining time onto the
  clip, replacing the placeholder white box one-to-one. Dead entities are
  simply never drawn, so destruction cannot leave a dangling frame.

### 3.10 Audio (Stage 20)

Stage 20 implements it (`src/audio/AudioManager.{hpp,cpp}`):

- **Ids only** (the plan's hard rule): gameplay never sees file paths or
  devices — `Game` calls `audio_.playSound(SoundId::PlayerFire)` etc. at
  the composition-root event sites (player/enemy fire, enemy/player
  destroyed, wave start, game start via the state callback, game over).
- **Procedural dev audio** (DevArt precedent): the clips are tiny
  synthesized chiptune-style buffers (square sweeps / noise bursts /
  tone sequences) generated at initialize() into memory — no binary
  assets, no SDL_mixer dependency; plain SDL2 audio only. Stage 24 can
  swap real files in behind the same ids (the architecture's "ids map to
  files" point).
- **Silent fallback**: if opening a device fails, or `GALAXIAN_SILENT=1`
  is set (headless determinism), the manager runs muted but fully
  functional — play requests are still counted, the game never crashes.
  NOTE: the manager initializes `SDL_INIT_AUDIO` ITSELF — the renderer
  only brings up video, and SDL does not lazy-init audio (missing this
  muted the whole game until the Stage 20 fix; a regression test opens a
  real dummy-driver device to guard it).
- **Music**: `playMusic(Gameplay)` starts the looping track whenever a run
  begins (Title -> Playing); it stops at GameOver and on returning to
  Title.
- **Bounded voices**: a fixed pool of 8 mixing slots with round-robin
  reuse plus one dedicated looping music channel; overlapping effects are
  allowed by construction and rapid firing cannot grow memory. The mix is
  summed in int32 and clamped back to S16.
- **Thread safety & determinism**: the SDL callback mutates voices under
  `SDL_LockAudioDevice`; clip synthesis is seeded per position, so two
  identically-driven managers mix bit-identically (unit-tested without
  any device via synchronous mixer test hooks).

### 3.11 Persistence (Stage 22)

Stage 22 implements it (`src/persistence/HighScore.{hpp,cpp}`):

- **`persistence/HighScoreStore`**: pure stdio, SDL-free. Default path
  `$GALAXIAN_DATA_DIR/highscore.dat` (hermetic override) else
  `${XDG_DATA_HOME:-$HOME/.local/share}/galaxian-clone/highscore.dat`.
- **Format**: one plain-text integer line — human-inspectable and strictly
  validated on load (digits only, plausible range). Missing/empty/garbage/
  negative/huge records all load as **0 without crashing**; a bare-digit
  file without trailing newline is accepted deliberately (harmless
  truncation case).
- **Atomic save**: write `<path>.tmp`, flush, `rename()` over the target
  (POSIX atomic) after creating missing parent directories. Readers always
  see a complete record.
- **Lifecycle wiring** (`Game`): loaded into `ScoreManager::seedHighScore`
  before the first render (the title shows the all-time best immediately);
  saved at the Playing→GameOver transition AND on every clean shutdown.

### 3.12 Data-driven configuration (Stage 21)

- **`core/GameConfig.{hpp,cpp}`**: one struct of BALANCE knobs (player
  speed/lives/cooldown/bullet speed, enemy points + dive speeds, enemy
  bullet base/ramp/max, formation swing/period/multiplier, the spec §7
  wave table as six bracket rows, interstitial/explosion/respawn/
  invulnerability durations). The in-header defaults equal the previously
  frozen constants.
- **Loading** (`assets/config/game.json`, `--config <path>` override):
  missing/unreadable file or invalid JSON -> documented defaults, never a
  crash; a present-but-invalid value falls back PER KEY with a stderr
  note; unknown keys are ignored; structural clamps applied post-load
  (attackers 1..4, shots 1..2, interval >= 1 s, speeds floored, lives
  1..9, max enemy bullet speed <= 720).
- **Consumption**: gameplay reads the process-wide config
  (`GameConfig::get()`) that main() installs before initialization —
  balance is adjustable WITHOUT recompiling (verified end-to-end: a JSON
  tweak changes attack timing and player speed in the running binary).
  Structural rules stay in C++: state machines, collision semantics, pool
  sizes, grid geometry, the §7 progression shape and its caps.
- **Tests** mutate and restore the global config safely (guard pattern).

### 3.13 Visual polish (Stage 24)

- **Reference-matched pixel-art sprites** (`graphics/DevArt`): the
  prototype triangle/squares were replaced by hand-authored bitmaps
  (mirrored half-row ASCII maps rasterized through a palette forge),
  designed after the arcade screenshots in
  `assets/sprites/examples_from_internet`: a 24x16 white fighter with
  cyan wings and a red spine (player; frame B lights the thruster bar),
  teal drone bugs with red eyes and segmented blue wings (scouts; frame B
  raises the wings one notch), red escort bugs with yellow eyes and
  orange claws (guards; frame B pinches the claws), and the yellow
  flagship with orange dome and blue wing-tip bars (commanders). Every
  bitmap row is exactly half the sprite width so the mirrored halves
  always connect at the centre seam (stampBitmap warns otherwise). All
  art fits the EXISTING collision boxes; combat/gameplay code is
  untouched (regression: full suite green).
- **Limited retro palette**: hull colour per type, wing blue, dark
  accent, white, hot accent (eyes/spine/dome), plus the starfield grays —
  no gradients.
- **Explosions** (Stage 24 rework): thin RAYS and SCATTERED DOTS only —
  the earlier filled-core "blob" read as a solid square on screen
  (unnatural, debuggy) and was removed. Enemy kills burst as a 4-frame
  32x32 starburst (white flash → cyan/white rays → hot broken rays +
  debris → embers); the player's death erupts as the arcade life-lost
  fountain (ragged magenta mound, yellow rays fanning upward, coloured
  debris, drifting embers) per the reference screenshot. Both clips keep
  the Stage 19 timing (4 × 0.0625 s = the Stage 9 effect duration);
  `Effect::kind` (`EffectKind::Enemy|Player`) selects the clip, and Game
  draws the 32x32 sprite CENTRED on the effect box so the blast reads
  bigger than the ship.
- **Starfield** (`graphics/Starfield`): three deterministic parallax
  layers drifting downward; cosmetic-only clock (wall delta) so it stays
  alive on Title/Paused; drawn behind gameplay and on the title screen.
- **Presentation extras**: rising "+N" score popups carried by the Stage 9
  effects (`scoreValue`), a one-frame PLUS-shaped muzzle flash at the
  ship nose, a row-by-row formation reveal as the wave transition, and
  the incoming wave number shown during the interstitial. Title screen
  gained an alien honour-guard composition.
- **Scaling**: nearest-neighbour textures plus the manual integer-scale +
  letterbox transform already present in Renderer (unchanged).

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
