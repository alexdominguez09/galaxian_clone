# Galaxian Clone

A 2D fixed-resolution arcade shooter in the style of Galaxian, written in
C++20 with SDL2 for Linux. Original assets and names; the classic gameplay
feel is the target.

## Status

| Stage | Name                 | State |
|-------|----------------------|-------|
| 0     | Specification        | done  |
| 1     | Project skeleton     | done  |
| 2+    | Game loop, ...       | not started |

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
- Headless smoke run (no display needed):

  ```bash
  SDL_VIDEODRIVER=dummy ./build/galaxian --smoke 120
  ```

- Warning policy: `-Wall -Wextra -Wpedantic`, zero warnings.
