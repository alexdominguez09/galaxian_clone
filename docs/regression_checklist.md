# Galaxian Clone — v1.0 Regression Checklist (Stage 26)

Formal final-regression checklist per `galaxian_plan.md` Stage 26 and
`test_plan.md` §2. Every v1.0 requirement from `docs/game_spec.md` §2 maps to
an automated test (Catch2/CTest) and/or a manual step. A requirement is
satisfied when its automated tests pass **and** its manual steps pass.

## How to use

- **Automated**: `ctest --test-dir build` (GCC Debug) and repeat on
  `build-clang`, `build-release`, `build-san` (see §6). The "Covered by"
  column names the test files.
- **Manual**: run the real binary (`./build/galaxian`) on a real display and
  step through the listed checks.
- **Edge cases**: runnable headlessly with the exact commands in §5.

---

## 1. Functional checklist

| # | Requirement (spec) | Automated coverage | Manual step |
|---|--------------------|--------------------|-------------|
| 1 | Startup → title screen renders, high score shown (spec §10) | test_game_states, test_rendering | Launch; GALAXIAN CLONE logo, honour guard, HIGH SCORE, PRESS ENTER visible |
| 2 | Enter starts game; Escape on title quits (spec §4/§10) | test_game_states, test_input | Enter → playfield; Escape on title closes window |
| 3 | Player moves left/right, clamped to screen (spec §5) | test_player, test_rendering | Arrows/A/D move; ship stops at edges |
| 4 | Player fires; cooldown + max-shots respected (spec §5/§8) | test_projectiles, test_player | Space fires; ≤2 bullets; 0.35 s cadence |
| 5 | Formation appears (40 enemies, correct layout) (spec §6.2) | test_enemy_formation | 5×8 grid: commander/guard/scout rows |
| 6 | Formation oscillates; kills leave holes (spec §6.3) | test_formation_motion | Sway; destroyed enemies stay gone; speeds up as they die |
| 7 | Bullets kill enemies; score correct per type (spec §6.1/§9) | test_combat, test_scoring | Scout 50 / Guard 80 / Commander 150; +N popup |
| 8 | Dives per director rules; diving enemies worth 2× (spec §6.4/§7) | test_attack_director, test_enemy_state_machine, test_dive_path | Enemies dive on their own; state labels (F2) |
| 9 | Enemy bullets fire, move, cull; ownership enforced (spec §8) | test_enemy_fire, test_projectiles | Divers shoot aimed bullets; enemy bullets can't hurt enemies |
| 10 | Player death → life lost → respawn with invulnerability (spec §5) | test_player_damage | Hit costs a life, 1.5 s pause, blinking invulnerable respawn |
| 11 | Zero lives → game over screen with final score (spec §10) | test_player_damage, test_game_states | Third death → GAME OVER + final score + wave |
| 12 | Waves progress with bounded difficulty (spec §9) | test_wave_manager | Clear 40 → WAVE CLEAR → fresh, harder formation |
| 13 | Pause freezes sim; resume continues (spec §10) | test_game_states | Escape pauses/freezes; Escape resumes seamlessly |
| 14 | Restart from game over is clean (spec §10) | test_game_states | Enter on game over → title → fresh run, no stale entities |
| 15 | High score persists across runs (spec §13) | test_high_score, test_scoring | Quit/relaunch keeps best |
| 16 | Audio plays; silent fallback without device (spec §12) | test_audio | Sounds on events; runs muted without a device |
| 17 | Window close exits cleanly from every state (spec §10) | test_input, test_game_states | Close from Title/Playing/Paused/GameOver → exit 0 |
| 18 | HUD: score, high score, lives, wave (spec §11) | test_rendering (HUD), test_scoring | Top bar + life pips track live state |
| 19 | Debug aids: F1 boxes, F2 overlay, F3 dive (spec §2) | test_rendering, test_animation | F1 outlines, F2 stats, F3 manual dive |

## 2. Matrix checklist (builds / environments)

| Scenario | Automated | Command / notes |
|----------|-----------|-----------------|
| Fresh install (no data dir) | test_high_score | `GALAXIAN_DATA_DIR=$(mktemp -d)` → boots, high score 0 |
| Missing config file → defaults | test_config | `--config /nonexistent.json` → defaults, no crash |
| Missing/corrupt save file → 0 | test_high_score | delete/scramble `highscore.dat` → loads 0 |
| No audio device | test_audio | `GALAXIAN_SILENT=1` (or dummy driver) → muted but functional |
| Resolutions 448×576, 896×1152, 1344×1728, 1920×1080 (letterbox) | test_rendering (scaling) | Manual: resize window, integer-scale/letterbox, logical coords stable |
| GCC Debug / Release, Clang, Sanitize | §6 | All 4 build variants, 0 warnings, all tests pass |
| 30+ min stability / no leaks | — | Stage 25 `[perf]` + ASan/valgrind (see test_plan Stage 25) |

## 3. Out of scope / not implemented (verified not regressed)

- Network multiplayer, leaderboards, level editor, modding, particles,
  controller remap UI, mobile — spec §2 explicitly out of scope.
- Fullscreen/windowed toggle — not implemented (checklist item marked
  "if implemented"; N/A).

---

## 4. Automated verification record (Stage 26)

_Fill in after running §6. Expected: all pass._

- [ ] GCC Debug (`build`): N/N tests pass, 0 warnings
- [ ] GCC Release (`build-release`): N/N tests pass, 0 warnings
- [ ] Clang Debug (`build-clang`): N/N tests pass, 0 warnings
- [ ] GCC ASan+UBSan (`build-san`): N/N tests pass, 0 warnings
- [ ] Both dependency guards (no_sdlk_in_gameplay, no_collision_in_graphics) pass

## 5. Headless edge-case smoke commands

```bash
cd /home/alex/Downloads/qwen3.8_27b_test/galaxian
export SDL_VIDEODRIVER=dummy GALAXIAN_SILENT=1

# Fresh install (no high-score record): boots, logs "High score loaded: 0"
GALAXIAN_DATA_DIR=$(mktemp -d) ./build/galaxian --smoke-time 1 --no-vsync

# Missing config: falls back to documented defaults, no crash
./build/galaxian --config /nonexistent.json --smoke-time 1 --no-vsync

# Missing/corrupt save: scramble then boot -> loads 0
printf 'garbage!!' > "$(mktemp -d)/highscore.dat"

# Long session with [run]/[perf] telemetry at exit
./build/galaxian --smoke-time 20 --no-vsync 2>&1 | grep -E '\[run\]|\[perf\]'
```

## 6. Build matrix

```bash
cmake -S . -B build        # GCC Debug (default)
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake -S . -B build-clang  -DCMAKE_CXX_COMPILER=clang++
cmake -S . -B build-san    -DCMAKE_BUILD_TYPE=Sanitize
# then for each: cmake --build <dir> && ctest --test-dir <dir>
```
