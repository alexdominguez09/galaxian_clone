# Galaxian Clone — Test Plan

Covers automated (Catch2/CTest) and manual acceptance testing per stage, plus
the final regression suite. A stage is only complete when its automated tests
pass **and** its manual checklist passes.

---

## 0. Test Infrastructure

- Framework: Catch2 v3; runner: `ctest --test-dir build` (or `./build/tests/galaxian_tests`).
- Builds: `Debug` (default), `Release`, `Sanitize` (ASan+UBSan).
- Headless: logic tests never open a window. Smoke runs of the full binary
  use `SDL_VIDEODRIVER=dummy` and the `--smoke <frames>` flag.
- Environments tested: GCC and Clang (both must build warning-free).

## 1. Per-Stage Test Matrix

### Stage 1 — Project skeleton
- [ ] Clean clone builds with GCC (and Clang) at `-Wall -Wextra -Wpedantic`, 0 warnings
- [ ] `SDL_Init` + window + renderer succeed (real display and dummy driver)
- [ ] Window close (SDL_QUIT) exits with code 0
- [ ] Escape exits with code 0
- [ ] Shutdown path runs without warnings/crashes
- [ ] `--smoke 120` under `SDL_VIDEODRIVER=dummy` runs 120 frames and exits 0

### Stage 2 — Game loop and timing
Automated (headless, no window):
- [ ] Fixed timestep: N simulated seconds produce exactly N×60 updates
- [ ] Simulation at simulated 30/60/120 Hz render rates yields identical
      object positions after a fixed simulated time
- [ ] Accumulator cap: a 1 s stall produces ≤ 5 catch-up updates, no spiral
Manual:
- [ ] Debug overlay (F2) shows FPS/frame time/update/entity counts
- [ ] 15-minute run: no timing drift, no memory growth, stable FPS

### Stage 3 — Rendering
- [x] Texture loads from disk (valid + missing file case)
- [x] Test scene renders: player sprite, 10 enemies, text, projectiles, border
- [x] Window resize does not change logical coordinates (verified via
      rendered pixel positions at 2+ window sizes)
- [x] Sprite flipping works
- [x] Text renders at expected positions

### Stage 4 — Input
- [x] `wasPressed` true for exactly one frame on keydown
- [x] `isHeld` true while held, false after release
- [x] `wasReleased` true for exactly one frame on keyup
- [x] Simultaneous left+right handled (last-pressed-wins or cancel — spec'd)
- [x] No `SDLK_*` in gameplay/ (grep check in CI)
- [x] SDL key events drive the state machine; SDL_QUIT reported; focus loss
      releases all keys; key auto-repeat does not retrigger a press
- [x] Default bindings match spec §4; `setBinding` remaps; unbound keys inert
- [x] End-to-end: the Stage 4 dev-scene demo moves the player and fires in
      response to input (pixel-verified headlessly)

### Stage 5 — Player
- [x] Player cannot move left of x=0 (clamped)
- [x] Player cannot move right of right edge (clamped)
- [x] Movement distance = speed × dt over N steps (exact)
- [x] Initial position matches spec (224, 528)
- [x] Fire command emits a fire event (log) — no projectile yet
- [x] Frame-rate independence: same position after 10 s sim at 30 vs 120 Hz
- [x] End-to-end: input drives the real Player (movement + fire, pixel-verified
      headlessly)
Manual:
- [x] Smooth movement, correct feel, stays in bounds

### Stage 6 — Projectiles
- [x] Fire spawns a projectile at the player position moving upward
- [x] Projectile removed when y < -height
- [x] Cooldown: second shot within 0.35 s is rejected
- [x] Max 2 simultaneous player projectiles enforced
- [x] No leaks: fire 10 000 shots headlessly, active count returns to 0
- [x] Stress: continuous fire for 5 simulated minutes — stable memory
Manual:
- [x] Firing feel: bullets spawn above the player, cooldown and 2-bullet
      cap are perceivable, bullets vanish off the top edge

### Stage 7 — Collision
- [x] Overlap detected; no overlap not detected
- [x] Edge-touching = no collision (strict)
- [x] Full containment detected
- [x] Partial intersection detected
- [x] Negative coordinates handled
- [x] Degenerate (zero-size) rects handled
Manual:
- [x] F1 draws boxes aligned with sprites
- [x] No collision code in graphics/

### Stage 8 — Enemy formation
- [x] Exactly 40 enemies spawned
- [x] Row/type layout matches spec (8/8/8/8/8, types per row)
- [x] Spacing: 48 px columns, 36 px rows
- [x] Initial coordinates match spec exactly
- [x] Deterministic: identical layout across 100 spawns

### Stage 9 — Player vs enemy combat
- [x] Bullet-enemy overlap → enemy dead, bullet consumed
- [x] Score increases by the type's point value
- [x] One bullet cannot kill two enemies (bullet consumed on first hit)
- [x] Dead enemy cannot be scored twice
- [x] Destroying all 40 enemies works without corruption
- [x] Placeholder destruction effect at the kill site (appears, then expires)
Manual:
- [x] Entire stationary formation destroyable reliably

### Stage 10 — Formation movement
- [x] Formation oscillates within screen bounds
- [x] Enemy spacing invariant while moving
- [x] Killed enemies stay absent; formation state valid after deaths
- [x] Frame-rate independence of oscillation phase at fixed sim time

### Stage 11 — Enemy state machine
- [x] All legal transitions accepted; illegal transitions rejected
- [x] Dead reachable from any living state; Dead is terminal
- [x] Selected enemy: leaves formation → descends → turn point → returns →
      occupies original slot (slot offset unchanged, formation intact)
- [x] Formation state uncorrupted after a full dive cycle
Manual:
- [x] F2/debug shows state labels (FORMATION/DIVING/RETURNING) above enemies

### Stage 12 — Dive trajectories
- [x] Bézier evaluation: P(0)=P0, P(1)=P3, symmetry checks
- [x] t stays in [0,1] under 10 000 headless updates
- [x] Enemy follows full path and finishes (t=1 → state transition)
- [x] Left/right/center/return paths all complete and terminate in valid state
- [x] Path speed is frame-rate independent (arc-length or parametric check)

### Stage 13 — Attack director
- [x] Attack count never exceeds wave maximum
- [x] Attack interval respected (±1 frame)
- [x] Dead enemies never selected
- [x] Diving enemies never re-selected
- [x] No eligible attacker → attack skipped, no deadlock (1000-tick soak)
- [x] All 40 enemies dead → director idles safely

### Stage 14 — Enemy projectiles
- [x] Enemy fires during attack (1–2 shots per wave rules)
- [x] Enemy bullets move downward, culled off-screen
- [x] Enemy bullet never damages an enemy (ownership)
- [x] Player bullet never damages player (ownership)
- [x] Aimed shots point at player position at fire time

### Stage 15 — Player death/lives
- [x] Enemy bullet collision → exactly one life removed
- [x] Enemy body collision → exactly one life removed
- [x] Two collisions in the same frame → exactly one life removed
- [x] Respawn after 1.5 s at start position with 2.0 s invulnerability
- [x] Invulnerable player ignores collisions
- [x] Respawn clears nearby enemy projectiles
- [x] Zero lives → GameOver, no respawn

### Stage 16 — Waves
- [x] Wave N clears → interstitial → wave N+1 formation with correct params
- [x] Difficulty values stay within spec bounds for waves 1..20
- [x] 10 consecutive waves: no state corruption (headless scripted run)
- [x] Wave counter in HUD matches WaveManager state

### Stage 17 — Game states
- [x] Every transition in the state graph works; illegal ones rejected
- [x] start → die → game over → restart ×100: no leaked entities
- [x] pause → resume ×100: simulation time frozen while paused
- [x] Quit from any state exits cleanly

### Stage 18 — HUD
- [x] Score display equals ScoreManager value after each event
- [x] Lives display matches lives (3 → 2 → 1 → 0)
- [x] Wave display matches current wave
- [x] High score updates within session when surpassed
- [x] Score popups/flash do not affect score value

### Stage 19 — Animation
- [x] Clip advances at correct rate (frames advanced = sim time × fps)
- [x] Looping clips loop; one-shot clips stop at last frame
- [x] Animation state does not alter physics (position checks)
- [x] Destroyed entity's animation resources released (no dangling draw)

### Stage 20 — Audio
- [x] Each SoundId triggers its callback (mock device)
- [x] 100 rapid fires: no corruption, bounded voice count
- [x] Overlapping SFX allowed
- [x] No audio device → game runs silent, no crash
- [x] Shutdown releases audio (no leak reports)

### Stage 21 — Configuration
- [x] All §14 spec values load from game.json and are used by gameplay
- [x] Changing a value changes behavior without recompiling
- [x] Missing/invalid config → documented defaults, no crash

### Stage 22 — High score persistence
- [x] Score survives quit/relaunch
- [x] Missing file → 0, no crash
- [x] Corrupt file (garbage bytes, truncated) → 0, no crash
- [ ] Save is atomic (no partial file after simulated crash)
- [ ] Directory auto-created if absent

### Stage 23 — Balancing
- [x] 3+ structured playtest sessions logged (survival time, accuracy,
      deaths, waves reached)
- [x] No fundamental mechanic changes required (only numeric tuning)
- [x] Values written back into game.json + spec revision note

### Stage 24 — Visual polish
- [x] All prototype sprites replaced; palette consistent
- [x] No collision box changed vs Stage 23 (regression: combat tests pass)
- [x] Nearest-neighbor scaling, integer scaling at common window sizes
- [x] REWORK after human review: the filled-core explosion "blob" (read as
      a red/solid square — unnatural, debuggy) was removed. Enemy kills now
      burst as a thin-ray starburst and the player's death as the arcade
      fountain (magenta mound + yellow rays), both 32x32 drawn centred on
      the effect box; sprites redesigned after the reference screenshots in
      assets/sprites/examples_from_internet (connected mirrored halves,
      reference palettes). Pixel tests updated to the new art; full suite
      green on GCC Debug/Release, Clang, ASan+UBSan.

### Stage 25 — Performance and stability
- [ ] ASan+UBSan build: 30-minute gameplay session, 0 errors
- [ ] Valgrind: short session, 0 definite leaks
- [ ] Rapid restart ×100, pause/resume ×100, wave transition ×50: clean
- [ ] Continuous firing 10 min: stable memory
- [ ] Max simultaneous enemies/projectiles scenario: stable frame time
- [ ] CPU profile: update cost within budget (logged)

### Stage 26 — Final regression
Full checklist (see §2 below) on GCC + Clang, Debug + Release,
windowed + fullscreen, with and without audio device, fresh install
(no config/save files), multiple resolutions.

## 2. v1.0 Regression Checklist (Stage 26)

Functional:
- [ ] Startup → title screen renders, high score shown
- [ ] Enter starts game; Escape on title quits
- [ ] Player moves left/right, clamped to screen
- [ ] Player fires; cooldown and max-shots respected
- [ ] Enemy formation appears correctly (40 enemies, right layout)
- [ ] Formation oscillates; killed enemies leave holes
- [ ] Player bullets kill enemies; score correct per type
- [ ] Dives occur per director rules; diving enemies worth double
- [ ] Enemy bullets fire, move, cull; ownership enforced
- [ ] Player death → life lost → respawn with invulnerability
- [ ] Zero lives → game over screen with final score
- [ ] Waves progress with bounded difficulty
- [ ] Pause freezes simulation; resume continues seamlessly
- [ ] Restart from game over is clean (no stale entities)
- [ ] High score persists across runs
- [ ] Audio plays; silent fallback works without device
- [ ] Window close exits cleanly from every state
- [ ] Fullscreen/windowed toggle (if implemented) works

Matrix:
- [ ] Fresh install (no ~/.local/share/galaxian-clone)
- [ ] Missing config file → defaults
- [ ] Missing/corrupt save file → 0
- [ ] No audio device
- [ ] Resolutions: 448×576, 896×1152, 1344×1728, wide 1920×1080 (letterbox)
- [ ] GCC Debug, GCC Release, Clang Debug, Clang Release, Sanitize build

## 3. Definition of Done (per stage)

1. All automated tests for the stage pass on GCC (and Clang when available).
2. Manual acceptance checklist for the stage passes.
3. Zero compiler warnings at `-Wall -Wextra -Wpedantic`.
4. Code committed; stage tagged only after 1–3 are green.
