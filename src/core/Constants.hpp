#pragma once

namespace galaxian {

// Logical (gameplay) resolution. The window may be any size; the renderer
// scales this fixed coordinate space (docs/game_spec.md §3).
inline constexpr int kLogicalWidth = 448;
inline constexpr int kLogicalHeight = 576;

// Fixed simulation timestep (docs/game_spec.md §3, docs/architecture.md §3.1).
// All gameplay motion is `position += velocity * dt` with this dt; it is
// never scaled by the render frame rate.
inline constexpr double kFixedDeltaSeconds = 1.0 / 60.0;

// Maximum catch-up simulation steps per rendered frame. If the backlog
// exceeds this (e.g. after a long stall), the excess time is dropped to
// prevent a spiral of death (docs/architecture.md §3.1).
inline constexpr int kMaxCatchUpSteps = 5;

}  // namespace galaxian
