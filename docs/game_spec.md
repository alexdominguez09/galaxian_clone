# Galaxian Clone — Game Specification (v1.0)

Status: **FROZEN for v1.0** (Stage 0)
Any change to this document after Stage 0 requires a new spec revision and re-approval.

---

## 1. Overview

A 2D fixed-resolution arcade shooter in the style of Galaxian. The player
controls a ship at the bottom of the screen and defends against waves of
enemies that form a grid formation, oscillate, and periodically dive at the
player. Original assets and names are used; the gameplay feel is the target.

## 2. Scope

### In scope for v1.0

- Player ship with horizontal movement and shooting
- Enemy formation (grid) with multiple enemy types
- Formation movement (horizontal oscillation)
- Enemy dives toward the player along parametric paths
- Enemy projectiles
- AABB collision detection
- Player lives, death, respawn, invulnerability
- Scoring (with central `ScoreManager`)
- Wave system with bounded difficulty progression
- Game-over state
- Title / start screen
- Pause (and restart from pause/game-over)
- HUD (score, high score, lives, wave)
- Simple sound effects + optional background music
- Local high score persistence
- Debug aids: FPS/timing overlay, collision-box overlay, enemy state labels

### Explicitly out of scope for v1.0

- Network multiplayer, online leaderboards
- Level editor, modding
- Sophisticated particle engine
- Controller remapping UI (gamepad support may exist but is not required)
- Mobile support

## 3. Coordinate System and Resolution

- Logical resolution: **448 × 576** (2× the classic 224 × 288).
- All gameplay coordinates are in logical pixels.
- The window may be resized; the renderer scales the logical framebuffer
  (integer scaling preferred, letterboxed if non-integer).
- Coordinate origin: top-left. +x right, +y down.
- Target frame rate: 60 FPS rendering; simulation runs at a fixed
  1/60 s timestep independent of render rate.

## 4. Controls

| Action  | Default keys            | Notes                          |
|---------|-------------------------|--------------------------------|
| Move left  | Left Arrow, A | continuous while held |
| Move right | Right Arrow, D  | continuous while held |
| Fire      | Space             | fires on press, respects cooldown |
| Start     | Enter             | title → playing, game over → title |
| Pause / Back | Escape        | playing → paused, paused → playing, title → quit |
| Toggle collision debug | F1 | developer only |
| Toggle debug overlay   | F2 | developer only |

Input is consumed through `InputManager` using named `Action`s; gameplay code
never references SDL key constants.

## 5. Player

- Movement: horizontal only, speed **220 logical px/s**.
- Start position: x = 224 (center), y = 528.
- Collision box: 24 × 16 logical px, centered on the sprite.
- Lives: **3** at start.
- Fire cooldown: **0.35 s**; maximum **2** simultaneous player projectiles.
- Player projectile speed: **480 px/s**, upward.
- Death: on collision with an enemy or enemy projectile.
- Respawn: after **1.5 s**, at start position, with **2.0 s** of
  invulnerability (blinking). Nearby enemy projectiles are cleared on respawn.
- Zero lives → Game Over.

## 6. Enemies

### 6.1 Types (data-driven via `EnemyDefinition`)

| Type      | Points | Sprite | Role |
|-----------|--------|--------|------|
| Scout     | 50     | 0      | fast, front rows |
| Guard     | 80     | 1      | mid rows |
| Commander | 150    | 2      | top row, may lead escorts |

### 6.2 Formation

Logical grid, **5 rows × 8 columns = 40 enemies**:

```text
row 0: Commander  × 8
row 1: Guard      × 8
row 2: Guard      × 8
row 3: Scout      × 8
row 4: Scout      × 8
```

- Column spacing: 48 px; row spacing: 36 px.
- Formation top-left anchor: (32, 64); formation center oscillates
  horizontally within the screen.
- Each enemy stores a **slot offset** (formation-local) plus the formation's
  **world position**; screen position = world + slot offset. This is what
  allows diving enemies to leave and rejoin the formation.

### 6.3 Formation movement

- Horizontal oscillation: amplitude 64 px, base period 4 s.
- As enemies die, oscillation speed increases (classic pressure mechanic),
  bounded: max 2.5× base speed.
- Destroyed enemies leave holes; the formation does not re-pack.

### 6.4 Enemy states

```text
Formation → PreparingDive → Diving → Attacking → Returning → Formation
Any living state → Dead
```

- Diving enemies follow parametric (cubic Bézier) paths: left dive, right
  dive, center attack, and a return path.
- A diving enemy may fire 1–2 projectiles during the attack phase.
- Diving enemies are worth **2×** their base points.

## 7. Attack Director

Central pacing authority. No enemy decides to attack on its own.

| Wave | Max simultaneous attackers | Attack interval | Enemy fire |
|------|---------------------------|-----------------|------------|
| 1    | 1                         | 6 s             | 1 shot     |
| 2    | 2                         | 6 s             | 1 shot     |
| 3    | 2                         | 4 s             | 1 shot     |
| 4    | 2                         | 4 s             | 2 shots    |
| 5+   | 3 (wave 7+: 4)            | 3 s (min)       | 2 shots    |

All values are bounded; difficulty never multiplies unboundedly.
The director never selects dead or already-diving enemies, and the game must
never deadlock when no eligible attacker exists (attack is simply skipped).

## 8. Projectiles

- Single shared `Projectile` system, distinguished by `ProjectileOwner`
  (`Player` / `Enemy`).
- Player bullets travel up; enemy bullets travel down (optionally aimed at
  the player's position at fire time).
- Bullets are removed when off-screen.
- Ownership is enforced: player bullets never damage the player, enemy
  bullets never damage enemies.
- Enemy bullet speed: 240 px/s (wave-dependent, bounded ≤ 360 px/s).

## 9. Waves

- Wave N complete when all 40 enemies are dead (or have left and returned —
  only dead enemies count as cleared).
- Sequence: formation cleared → "WAVE N" interstitial (2 s) → new formation
  with wave N+1 parameters from §7.
- Wave parameters are bounded as in §7.

## 10. Game States

```text
Title → Playing → Paused → Playing
Playing → GameOver → Title
```

- **Title**: logo, high score, "PRESS ENTER".
- **Playing**: full simulation.
- **Paused**: simulation halted, overlay shown; Escape resumes.
- **GameOver**: final score, high-score flag, "PRESS ENTER" to return to title.

State transitions must not leak or retain stale entities: entering Playing
always starts a fresh game; entering Title always resets to a clean state.

## 11. HUD

Top bar (logical px):

```text
SCORE            HIGH SCORE
000000           000000

WAVE 1

♥ ♥ ♥          (bottom-left, lives)
```

- Score values come from a single data table (§6.1 + dive bonus).
- HUD updates immediately on any score/lives/wave change.

## 12. Audio

Sound effects (via `AudioManager`, ids only — no file paths in gameplay code):

- Player fire, enemy fire
- Enemy destroyed, player destroyed
- Wave start, game start, game over
- Optional: background music (gameplay loop)

Requirements: rapid firing must not corrupt audio; overlapping SFX allowed;
the game must run with no audio device present (silent fallback); shutdown
releases all audio resources.

## 13. Persistence

- High score file: `~/.local/share/galaxian-clone/highscore.dat`
  (binary: magic + version + uint32 score; or plain text — implementation
  detail, Stage 22).
- Load at startup; save when surpassed and on clean shutdown.
- Missing or corrupt file → start from 0, never crash.

## 14. Configuration

Balance constants live in `assets/config/game.json` (Stage 21): player speed,
projectile speeds, cooldowns, lives, attack timing, score values, wave
parameters. Structural rules (state machines, collision rules) remain in C++.

## 15. Directory Structure

```text
galaxian/
├── CMakeLists.txt
├── README.md
├── assets/
│   ├── audio/
│   ├── fonts/
│   ├── sprites/
│   └── config/
├── cmake/
├── docs/
│   ├── game_spec.md
│   ├── architecture.md
│   └── test_plan.md
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── graphics/
│   ├── input/
│   ├── audio/
│   ├── gameplay/
│   ├── states/
│   └── persistence/
└── tests/
```

(The Stage 1 skeleton uses `src/main.cpp`, `src/Game.{hpp,cpp}` directly;
subdirectories are introduced as their subsystems land in later stages.)

## 16. Non-Goals / Quality Bar

- 0 known crashes, 0 sanitizer errors, no significant leaks (Stage 25).
- Simulation is deterministic at a fixed timestep; render rate never changes
  gameplay behavior.
- Every stage has automated unit tests for logic plus a manual acceptance
  checklist (see `test_plan.md`).

## Revision notes

- Stage 23 (balancing): §14 defaults reviewed against measured playtest
  sessions (see docs/playtest_log.md) and kept unchanged; all future
  tuning goes through assets/config/game.json only.
