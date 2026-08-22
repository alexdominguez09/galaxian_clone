#include "Game.hpp"

#include <cstdio>
#include <vector>

#include "gameplay/Combat.hpp"
#include "graphics/DebugOverlay.hpp"
#include "graphics/DevArt.hpp"

namespace galaxian {

namespace {

// Stage 8: EnemyDefinition::spriteIndex -> dev-art texture id
// (docs/game_spec.md §6.1: Scout sprite 0, Guard 1, Commander 2). Kept in
// the composition root so gameplay/ stays SDL-free.
const char* const kEnemyTextureIds[kEnemyTypeCount] = {
    DevArt::kEnemyScout,
    DevArt::kEnemyGuard,
    DevArt::kEnemyCommander,
};

}  // namespace

Game::~Game() { shutdown(); }

bool Game::initialize()
{
    if (initialized_) {
        return true;
    }

    if (!renderer_.initialize(kLogicalWidth, kLogicalHeight, vsync_)) {
        return false;
    }
    // Stage 4: bring up the input layer after SDL (the renderer) is ready.
    input_.initialize();
    if (!devScene_.initialize(renderer_)) {
        std::fprintf(stderr, "galaxian: dev scene initialization failed\n");
        shutdown();
        return false;
    }
    // Stage 5/6/8: the dev scene's DevArt::createAll() registered the dev
    // textures; grab the player sprite (24x16), the bullet sprite (4x10,
    // coinciding with the projectile box), and the three enemy sprites
    // (24x24, coinciding with the enemy box) for drawing the gameplay
    // objects.
    playerTexture_ = renderer_.texture(DevArt::kPlayer);
    bulletTexture_ = renderer_.texture(DevArt::kBullet);
    for (int sprite = 0; sprite < kEnemyTypeCount; ++sprite) {
        enemyTextures_[sprite] = renderer_.texture(kEnemyTextureIds[sprite]);
    }
    bool allTextures = playerTexture_ != nullptr && bulletTexture_ != nullptr;
    if (allTextures) {
        for (int sprite = 0; sprite < kEnemyTypeCount; ++sprite) {
            allTextures = allTextures && enemyTextures_[sprite] != nullptr;
        }
    }
    if (!allTextures) {
        std::fprintf(stderr, "galaxian: dev texture unavailable\n");
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void Game::run()
{
    if (!initialized_) {
        return;
    }

    clock_.start();
    running_ = true;
    while (running_) {
        processEvents();

        // Fixed-timestep loop (docs/architecture.md §3.1):
        //   acc += frameDelta; while (acc >= dt) fixedUpdate(dt);
        const double frameDelta = clock_.frameDelta();
        lastFrameSeconds_ = frameDelta;

        // Stage 5: read this frame's input once (before the fixed updates)
        // so the Fire press edge is consumed at most once per frame.
        updateInputState();

        const int steps = timestep_.advance(frameDelta);
        for (int i = 0; i < steps; ++i) {
            fixedUpdate(timestep_.dt());
        }
        updatesSinceReport_ += steps;

        // Dev scene (text, static bullet rectangles, border). The Stage 4
        // input demo that moved a stand-in player is gone: the real Player
        // is simulated in fixedUpdate() and drawn in render(). The dummy
        // enemies and the action table were removed in Stage 8: the real
        // formation is drawn by render(), and the table overlapped the
        // formation area.
        render();

        // Clear per-frame input edges (once per frame;
        // docs/architecture.md §3.3). Done after render so the on-screen
        // action table reflects this frame's presses/releases.
        input_.endFrame();

        ++frameCount_;
        ++framesSinceReport_;

        if (smokeFrames_ > 0 && frameCount_ >= smokeFrames_) {
            running_ = false;
        }
        if (smokeSeconds_ > 0.0 && clock_.elapsed() >= smokeSeconds_) {
            running_ = false;
        }

        reportStats();
    }
    wallTime_ = clock_.elapsed();
    clock_.stop();
}

void Game::processEvents()
{
    // Stage 4: all keyboard input is routed through the InputManager as
    // named Actions (docs/architecture.md §3.3); the game loop no longer
    // references SDL key constants.
    if (input_.pollEvents()) {
        running_ = false;  // SDL_QUIT (window close)
    }
    if (input_.wasPressed(Action::DebugOverlay)) {
        statsEnabled_ = !statsEnabled_;
    }
    if (input_.wasPressed(Action::DebugCollision)) {
        // Stage 7: F1 toggles the collision-box debug overlay (the drawing
        // itself is isolated in graphics/DebugOverlay, architecture §3.5).
        collisionDebug_ = !collisionDebug_;
    }
    if (input_.wasPressed(Action::Pause)) {
        // Escape. The Stage 17 state machine turns this into pause/resume;
        // until then the single dev scene acts as the title, so Escape quits
        // (preserving the Stage 1 "Escape exits with code 0" acceptance).
        running_ = false;
    }
}

void Game::updateInputState()
{
    // Net horizontal movement from the held level (docs/game_spec.md §4):
    // right +1, left -1, both held cancel to 0, neither is 0. This is the
    // spec'd simultaneous-left+right resolution and keeps the Player free of
    // the InputManager (dependency rule, docs/architecture.md §1).
    pendingDirection_ = 0.0f;
    if (input_.isHeld(Action::MoveRight)) {
        pendingDirection_ += 1.0f;
    }
    if (input_.isHeld(Action::MoveLeft)) {
        pendingDirection_ -= 1.0f;
    }

    // Fire is a press edge: set once per frame, consumed by the first fixed
    // step below so a single press fires exactly once even when several
    // fixed steps run in one frame.
    if (input_.wasPressed(Action::Fire)) {
        fireRequested_ = true;
    }
}

void Game::fixedUpdate(double dt)
{
    // Stage 5/6: the gameplay simulation. The Player moves with the fixed
    // timestep (frame-rate independent) and fires on the consumed press
    // edge; the ProjectileManager moves and culls the live bullets.
    player_.update(dt, pendingDirection_);
    if (fireRequested_) {
        fireRequested_ = false;  // consume: one press == one fire event
        if (player_.alive()) {
            player_.fire();
            // Stage 6: the fire event asks the projectile system for a
            // bullet. The manager enforces the spec §5 rules (0.35 s
            // cooldown, max 2 simultaneous); a rejected shot spawns nothing.
            if (projectiles_.tryFirePlayer(player_)) {
                std::printf("Player fired\n");
            }
        }
    }
    projectiles_.update(dt);

    // Stage 9: player bullets vs the formation (gameplay/Combat, SDL-free,
    // runs after the move so it sees each bullet's new position): a hit
    // kills the enemy, consumes the bullet, awards the type's base points
    // through the ScoreManager, and spawns a placeholder effect.
    combat::resolvePlayerBullets(projectiles_, formation_, score_, effects_);
    effects_.update(dt);

    // These counters prove the loop runs a deterministic number of steps:
    // simTime_ must equal updateCount_ * dt at all times.
    ++updateCount_;
    simTime_ += dt;
}

void Game::render()
{
    // Stage 3 test scene: text, static projectile rectangles, screen border.
    // (The dummy enemies and the Stage 4 action table were removed in
    // Stage 8; see DevScene.)
    devScene_.draw(renderer_);

    // Stage 8: the static enemy formation (40 enemies, spec §6.2). The 24x24
    // enemy sprite coincides with the collision box, so the box's top-left
    // (formation world position + slot offset) is the draw position.
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const Enemy& enemy = formation_.at(row, col);
            if (!enemy.alive()) {
                continue;  // holes stay empty (spec §6.3)
            }
            const int sprite = enemy.definition().spriteIndex;
            if (enemyTextures_[sprite] != nullptr) {
                renderer_.drawSprite(*enemyTextures_[sprite],
                                     formation_.positionOf(row, col));
            }
        }
    }

    // Stage 9: the placeholder destruction effects — a box at each recent
    // kill site (over the hole the dead enemy left). Stage 19 replaces
    // these with the explosion animation.
    for (int i = 0; i < effects_.count(); ++i) {
        renderer_.drawFilledRect(effects_.effect(i).bounds(), colors::kEffect);
    }

    // Stage 5: the real Player (drawn on top of the test scene). The sprite
    // is 24x16 and coincides with the collision box, so the box's top-left
    // is the sprite's draw position.
    if (player_.alive() && playerTexture_ != nullptr) {
        renderer_.drawSprite(*playerTexture_, player_.bounds().position());
    }

    // Stage 6: the live projectiles. The 4x10 bullet sprite coincides with
    // the projectile box, and the box's top-left is the draw position.
    if (bulletTexture_ != nullptr) {
        for (int i = 0; i < projectiles_.count(); ++i) {
            renderer_.drawSprite(*bulletTexture_,
                                 projectiles_.projectile(i).position);
        }
    }

    if (statsEnabled_) {
        char line[64];
        std::snprintf(line, sizeof(line), "FPS: %.1f  UPD/S: %.1f", fps_,
                      updatesPerSecond_);
        renderer_.drawText(line, {16.0f, 480.0f}, colors::kGreen);
        std::snprintf(line, sizeof(line), "SIM: %.3f s  STEP: %.1f ms",
                      simTime_, timestep_.dt() * 1000.0);
        renderer_.drawText(line, {16.0f, 496.0f}, colors::kGreen);
        const int entities = formation_.aliveCount() +
                             (player_.alive() ? 1 : 0) + projectiles_.count();
        std::snprintf(line, sizeof(line), "ENT: %d (enemy %d, proj %d)",
                      entities, formation_.aliveCount(), projectiles_.count());
        renderer_.drawText(line, {16.0f, 512.0f}, colors::kGreen);
        // Stage 9: the ScoreManager's value (the full HUD lands in
        // Stage 18).
        std::snprintf(line, sizeof(line), "SCORE: %d  KILLS: %d",
                      score_.score(), score_.kills());
        renderer_.drawText(line, {16.0f, 528.0f}, colors::kGreen);
    }

    // Stage 7 (extended in Stage 8): F1 collision-box debug overlay. Game
    // collects the live boxes (formation + player + projectiles) and hands
    // them to the isolated graphics module; the AABB rule itself is
    // gameplay/Collision.hpp, not here.
    if (collisionDebug_) {
        std::vector<Rect> boxes;
        boxes.reserve(formation_.aliveCount() + 1 + projectiles_.count());
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                if (formation_.at(row, col).alive()) {
                    boxes.push_back(formation_.boundsOf(row, col));
                }
            }
        }
        if (player_.alive()) {
            boxes.push_back(player_.bounds());
        }
        for (int i = 0; i < projectiles_.count(); ++i) {
            boxes.push_back(projectiles_.projectile(i).bounds());
        }
        DebugOverlay::drawCollisionBoxes(renderer_, boxes, colors::kDebugBox);
    }

    renderer_.present();
}

void Game::reportStats()
{
    // Debug overlay. On-screen text when F2 is toggled (Stage 3); the
    // console line is kept for smoke runs, where it is enabled
    // automatically so headless runs are observable.
    const double now = clock_.elapsed();
    if (now - lastReportSeconds_ < 1.0) {
        return;
    }

    const double window = now - lastReportSeconds_;
    fps_ = static_cast<double>(framesSinceReport_) / window;
    updatesPerSecond_ = static_cast<double>(updatesSinceReport_) / window;

    if (statsEnabled_ || inSmokeMode()) {
        const int entities = formation_.aliveCount() +
                             (player_.alive() ? 1 : 0) + projectiles_.count();
        std::fprintf(stderr,
                      "[stats] fps=%.1f frame=%.2fms updates/s=%.0f "
                      "sim_time=%.1fs entities=%d score=%d\n",
                      fps_, lastFrameSeconds_ * 1000.0, updatesPerSecond_,
                      simTime_, entities, score_.score());
    }

    lastReportSeconds_ = now;
    framesSinceReport_ = 0;
    updatesSinceReport_ = 0;
}

void Game::shutdown()
{
    renderer_.shutdown();
    initialized_ = false;
    running_ = false;
}

}  // namespace galaxian
