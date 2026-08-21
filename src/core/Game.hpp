#pragma once

#include <SDL2/SDL.h>

#include "Constants.hpp"
#include "GameClock.hpp"
#include "TimestepController.hpp"
#include "gameplay/EnemyFormation.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Projectile.hpp"
#include "graphics/DevScene.hpp"
#include "graphics/Renderer.hpp"
#include "input/InputManager.hpp"

namespace galaxian {

// Game — top-level application object.
//
// Owns: initialization, main loop, event processing, update, rendering,
// shutdown. Stage 2 provides deterministic timing: the loop feeds real
// frame time into a TimestepController and runs a fixed number of 1/60 s
// simulation steps per frame (docs/architecture.md §3.1). Stage 3 routes
// all drawing through the Renderer subsystem and shows the DevScene test
// scene until gameplay rendering exists (Stage 5+). Stage 4 routes all
// keyboard input through the InputManager using named Actions: the game
// loop no longer references SDL key constants (docs/architecture.md §3.3).
// Stage 5 adds the first gameplay object, the Player: it is updated in the
// fixed-timestep simulation from the named Actions (movement is the held
// level, fire is the pressed edge) and drawn each frame. Stage 6 adds the
// shared Projectile system: a Fire event asks the ProjectileManager to spawn
// a bullet above the player (cooldown 0.35 s, max 2 simultaneous, spec §5),
// bullets fly upward at 480 px/s and are culled off-screen, all in the
// fixed-timestep simulation. Stage 7 adds the collision system: the AABB
// rule lives in gameplay/Collision.hpp (SDL-free); this class only owns the
// F1 (Action::DebugCollision) toggle that makes graphics/DebugOverlay draw
// 1-px outlines around the live player/projectile boxes. Stage 8 adds the
// static enemy formation: the 40-enemy grid (gameplay/EnemyFormation,
// SDL-free) is drawn each frame from the dev-art enemy textures (the
// spriteIndex -> texture mapping lives here, the composition root) and its
// boxes join the F1 collision overlay.
class Game {
public:
    Game() = default;
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    // SDL init + window + renderer + font + dev scene. Returns false (with
    // a diagnostic on stderr) if any of them fail.
    bool initialize();

    // Main loop: processEvents -> fixed updates -> render, until quit.
    void run();

    // Releases all resources. Safe to call more than once, and safe to call
    // if initialize() failed.
    void shutdown();

    // Render with/without vertical sync (test hook for refresh-rate
    // independence runs; see docs/test_plan.md Stage 2).
    void setVsync(bool enabled) { vsync_ = enabled; }

    // Test hooks (docs/test_plan.md): exit automatically after `frames`
    // rendered frames, or after `seconds` of wall time. A summary line is
    // printed to stdout on exit when either is active.
    void setSmokeFrames(int frames) { smokeFrames_ = frames; }
    void setSmokeSeconds(double seconds) { smokeSeconds_ = seconds; }

    bool inSmokeMode() const { return smokeFrames_ > 0 || smokeSeconds_ > 0.0; }
    int smokeResultFrames() const { return frameCount_; }
    int smokeResultUpdates() const { return updateCount_; }
    double smokeResultSimTime() const { return simTime_; }
    double smokeResultWallTime() const { return wallTime_; }

private:
    void processEvents();
    // Reads this frame's input once (after pollEvents, before the fixed
    // updates): the held movement level becomes pendingDirection_ and a Fire
    // press edge sets fireRequested_. Done once per frame so the pressed
    // edge is consumed at most once even when several fixed steps run in the
    // same frame (docs/architecture.md §3.3).
    void updateInputState();
    void fixedUpdate(double dt);
    void render();
    void reportStats();

    bool initialized_ = false;
    bool running_ = false;

    Renderer renderer_;
    // Stage 4: all keyboard input flows through the InputManager as named
    // Actions. Initialized after the renderer (which brings up SDL) and used
    // by processEvents()/the demo; holds no SDL resources of its own.
    InputManager input_;
    DevScene devScene_;
    // Stage 5: the first gameplay object. Updated in fixedUpdate() from the
    // named Actions; drawn in render(). The dev-art player texture (24x16,
    // coinciding with the collision box) is looked up from the renderer's
    // cache after the dev scene registers it.
    Player player_;
    const Texture* playerTexture_ = nullptr;
    // Stage 6: the shared projectile system (player bullets now, enemy
    // bullets in Stage 14). Updated in fixedUpdate() and drawn in render();
    // the dev-art bullet texture (4x10, coinciding with the projectile box)
    // is looked up from the renderer's cache after the dev scene registers
    // it.
    ProjectileManager projectiles_;
    const Texture* bulletTexture_ = nullptr;
    // Stage 8: the static enemy formation (gameplay/EnemyFormation,
    // SDL-free). No update yet (Stage 8 has no movement; Stage 10 adds the
    // oscillation); drawn in render() and part of the F1 collision boxes.
    EnemyFormation formation_;
    // Dev-art enemy textures indexed by EnemyDefinition::spriteIndex
    // (0 Scout, 1 Guard, 2 Commander). The spriteIndex -> texture mapping
    // is a graphics concern kept in the composition root, so gameplay/
    // stays SDL-free.
    const Texture* enemyTextures_[kEnemyTypeCount] = {};
    // Per-frame input, read once in updateInputState() and consumed by the
    // fixed updates. pendingDirection_ is the net held movement in {-1,0,+1}
    // (left/right cancel); fireRequested_ is the Fire press edge for this
    // frame (consumed by the first fixed step that runs).
    float pendingDirection_ = 0.0f;
    bool fireRequested_ = false;
    bool vsync_ = true;

    GameClock clock_;
    TimestepController timestep_{kFixedDeltaSeconds, kMaxCatchUpSteps};

    // Debug stats. Toggled with F2: on-screen overlay (Stage 3 text
    // rendering) plus the console line used by smoke runs.
    bool statsEnabled_ = false;
    // Stage 7: collision-box debug overlay. Toggled with F1
    // (Action::DebugCollision); when on, render() asks graphics/DebugOverlay
    // to outline the live player/projectile boxes (docs/architecture.md §3.5).
    bool collisionDebug_ = false;
    double lastReportSeconds_ = 0.0;
    int framesSinceReport_ = 0;
    int updatesSinceReport_ = 0;
    double lastFrameSeconds_ = 0.0;
    double fps_ = 0.0;
    double updatesPerSecond_ = 0.0;

    // Frame/update counters (diagnostics + smoke summary).
    int frameCount_ = 0;
    int updateCount_ = 0;
    double simTime_ = 0.0;
    double wallTime_ = 0.0;

    int smokeFrames_ = 0;
    double smokeSeconds_ = 0.0;
};

}  // namespace galaxian
