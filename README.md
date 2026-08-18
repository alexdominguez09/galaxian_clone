# Galaxian Clone

A 2D fixed-resolution arcade shooter in the style of Galaxian, written in
C++20 with SDL2 for Linux. Original assets and names; the classic gameplay
feel is the target.

## Status

| Stage | Name                 | State |
|-------|----------------------|-------|
| 0     | Specification        | done  |
| 1     | Project skeleton     | done  |
| 2     | Game loop & timing   | done  |
| 3+    | Rendering, ...       | not started |

Development proceeds stage by stage; see `docs/game_spec.md`,
`docs/architecture.md`, and `docs/test_plan.md`. Every accepted stage is
git-tagged (`stage-NN-*`).

## Requirements

- Linux
- CMake ≥ 3.20
- GCC or Clang with C++20 support
- SDL2 development package (`libsdl2-dev` on Debian/Ubuntu)

## Build and run

```bash
cmake -S . -B build
cmake --build build
./build/galaxian
```

Useful build types:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release     # optimized
cmake -S . -B build -DCMAKE_BUILD_TYPE=Sanitize    # ASan + UBSan
```

## Controls (planned, not all implemented yet)

| Key              | Action            |
|------------------|-------------------|
| Left / A         | Move left         |
| Right / D        | Move right        |
| Space            | Fire              |
| Enter            | Start             |
| Escape           | Pause / quit      |
| F1               | Collision debug   |
| F2               | Debug overlay     |

## Developer notes

- Logical resolution is 448×576; the window scales it.
- The simulation runs at a fixed 1/60 s timestep; render rate never changes
  gameplay speed (Stage 2).
- Headless smoke runs (no display needed):

  ```bash
  SDL_VIDEODRIVER=dummy ./build/galaxian --smoke 120          # N frames
  SDL_VIDEODRIVER=dummy ./build/galaxian --smoke-time 5       # N seconds
  SDL_VIDEODRIVER=dummy ./build/galaxian --smoke-time 5 --no-vsync
  ```

  Smoke runs print a summary (`frames`, `updates`, `sim_time`, `wall_time`)
  and a per-second stats line on stderr.
- F2 toggles the (temporary, console-based) debug stats: fps, frame time,
  updates/s, entity count. Becomes an on-screen overlay in Stage 3.
- Unit tests: `ctest --test-dir build` (Catch2, fetched at configure time).
- Warning policy: `-Wall -Wextra -Wpedantic`, zero warnings.
