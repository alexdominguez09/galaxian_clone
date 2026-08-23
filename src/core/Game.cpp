#include "Game.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/GameConfig.hpp"
#include "gameplay/Combat.hpp"
#include "persistence/HighScore.hpp"
#include "graphics/DebugOverlay.hpp"
#include "graphics/DevArt.hpp"
#include "graphics/Hud.hpp"
#include "states/GameState.hpp"

#include <cstdlib>  // getenv (GALAXIAN_SILENT)

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

// Stage 15: invulnerability blink period (~4 Hz on/off cycle).
constexpr double kInvulnerableBlinkPeriodSeconds = 0.25;

}  // namespace

Game::~Game() { shutdown(); }

bool Game::changeState(GameStateId to)
{
    return states_.request(to);
}

void Game::onStateChanged(GameStateId from, GameStateId to, void* self)
{
    auto* game = static_cast<Game*>(self);
    std::printf("State: %s -> %s\n", gameStateName(from),
                gameStateName(to));
    // Stage 20: state-driven audio cues.
    if (to == GameStateId::Playing && from == GameStateId::Title) {
        game->audio_.playSound(SoundId::GameStart);
        game->audio_.playMusic(MusicId::Gameplay);  // loops for the run
    } else if (to == GameStateId::GameOver) {
        std::fprintf(stderr, "%s\n",
                     game->stats_.summaryLine().c_str());
        game->audio_.stopMusic();
        game->audio_.playSound(SoundId::GameOver);
        // Stage 22: the run just ended — persist immediately so even a
        // hard power-off right now cannot lose it.
        game->highScoreStore_.save(game->score_.highScore());
    } else if (to == GameStateId::Title) {
        game->audio_.stopMusic();
    }
    switch (to) {
        case GameStateId::Playing:
            // Spec §10: entering Playing always starts a fresh game — but
            // a resume from Paused keeps the very same one running.
            if (from == GameStateId::Title) {
                game->startNewGame();
            }
            break;
        case GameStateId::Title:
            // Entering Title always resets to a clean state.
            game->startNewGame();
            break;
        case GameStateId::Paused:
        case GameStateId::GameOver:
            break;  // the scene freezes as-is / final score stays up
    }
}

void Game::startNewGame()
{
    stats_.reset();
    score_.reset();
    player_.resetGame();
    formation_.reset();
    projectiles_.reset();
    effects_.reset();
    attacks_.beginWave(1);
    waves_.beginWave(1);
}

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
    // Stage 20: bring up audio (silent fallback if no device / env-forced).
    // GALAXIAN_SILENT=1 forces the muted path for headless determinism.
    if (std::getenv("GALAXIAN_SILENT") != nullptr) {
        audio_.setSilent(true);
    }
    audio_.initialize();

    // Stage 22: load the persisted high score BEFORE the first render so
    // the title screen shows the all-time best immediately. Missing or
    // corrupt records simply start at 0 (docs/test_plan.md Stage 22).
    {
        int persisted = 0;
        if (highScoreStore_.load(&persisted)) {
            score_.seedHighScore(persisted);
            std::printf("High score loaded: %d\n", persisted);
        }
    }
    // Stage 17: the top-level state machine starts on the Title screen;
    // every accepted transition runs the enter bookkeeping below.
    states_.setCallback(&Game::onStateChanged, this);
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

    // Stage 19: start every entity's idle animation (graphics-side state
    // only — the simulation never reads it).
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const EnemyType type = EnemyFormation::typeForRow(row);
            const animation::AnimationClip* clip =
                (type == EnemyType::Scout)
                    ? &animation::kScoutIdleClip
                    : (type == EnemyType::Guard)
                          ? &animation::kGuardIdleClip
                          : &animation::kCommanderIdleClip;
            enemyAnimators_[row * EnemyFormation::kColumns + col]
                .play(*clip);
        }
    }
    playerAnimator_.play(animation::kPlayerIdleClip);

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
        // Stage 17: ONLY PLAYING simulates. Paused freezes the scene
        // entirely (the timestep keeps draining so a resume never
        // fast-forwards); Title/GameOver have no simulation at all.
        if (states_.current() == GameStateId::Playing) {
            for (int i = 0; i < steps; ++i) {
                fixedUpdate(timestep_.dt());
            }
            updatesSinceReport_ += steps;
        }

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
    if (input_.wasPressed(Action::DebugDive)) {
        // Stage 11 debug aid (docs/test_plan.md Stage 11): send the first
        // idle formation enemy into a full dive cycle so the state machine
        // is observable on screen. The attack pattern is picked from where
        // the enemy sits (left half sweeps left, right half sweeps right).
        // Stage 13's AttackDirector owns real selection; this stays a
        // developer aid.
        bool launched = false;
        for (int row = 0; row < EnemyFormation::kRows && !launched; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns && !launched;
                 ++col) {
                Enemy& enemy = formation_.at(row, col);
                if (enemy.state() != EnemyState::Formation) {
                    continue;
                }
                const Rect box = enemy.bounds(formation_.position());
                const DivePattern pattern =
                    (box.x + box.width * 0.5f <=
                     static_cast<float>(kLogicalWidth) * 0.5f)
                        ? DivePattern::LeftDive
                        : DivePattern::RightDive;
                if (enemy.beginDive(pattern,
                                    attacks_.params().shotsPerAttack)) {
                    std::printf("Enemy (%d,%d) beginning dive (%s)\n", row,
                                col, divePatternName(pattern).data());
                    launched = true;
                }
            }
        }
    }
    // Stage 17: state-driven menu keys (docs/game_spec.md §4/§10).
    //   Title:    Enter starts, Escape quits the application.
    //   Playing:  Escape pauses.
    //   Paused:   Escape resumes.
    //   GameOver: Enter returns to a clean Title.
    switch (states_.current()) {
        case GameStateId::Title:
            if (input_.wasPressed(Action::Start)) {
                changeState(GameStateId::Playing);
            } else if (input_.wasPressed(Action::Pause)) {
                running_ = false;  // title -> quit (spec §4, Stage 1)
            }
            break;
        case GameStateId::Playing:
            if (input_.wasPressed(Action::Pause)) {
                changeState(GameStateId::Paused);
            }
            break;
        case GameStateId::Paused:
            if (input_.wasPressed(Action::Pause)) {
                changeState(GameStateId::Playing);
            }
            break;
        case GameStateId::GameOver:
            if (input_.wasPressed(Action::Start)) {
                changeState(GameStateId::Title);
            }
            break;
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
                audio_.playSound(SoundId::PlayerFire);
                ++stats_.shotsFired;
            }
        }
    }
    projectiles_.update(dt);

    // Stage 10: the formation oscillates horizontally (spec §6.3, 64 px
    // peak-to-peak around the anchor, base period 4 s, speeding up as
    // enemies die up to the 2.5x bound) — gameplay/EnemyFormation, SDL-free,
    // phase accumulation on the fixed step. This also advances every
    // enemy's own state machine.
    formation_.update(dt);

    // Stage 14: divers' fire events -> aimed enemy bullets (spec §6.4/§8).
    // The enemy raises deterministic parametric triggers; this composition
    // root turns each event into a bullet spawned from the diver's
    // bottom-center, aimed at the player's position AT FIRE TIME (straight
    // down when the player is dead). Enemy bullets reuse the shared
    // Projectile system (downward motion + bottom cull since Stage 6);
    // they can never damage enemies, and player damage itself is
    // Stage 15.
    {
        const Vector2 fp = formation_.position();
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                Enemy& enemy = formation_.at(row, col);
                int pending = enemy.drainPendingShots();
                if (pending == 0 || !enemy.alive()) {
                    continue;
                }
                const Rect box = enemy.bounds(fp);
                const Vector2 muzzle{box.x + box.width * 0.5f, box.bottom()};
                while (pending-- > 0) {
                    if (projectiles_.tryFireEnemy(
                            muzzle,
                            player_.alive()
                                ? player_.position()
                                : Vector2{muzzle.x,
                                          static_cast<float>(kLogicalHeight)},
                            ProjectileManager::speedForWave(waves_.wave()))) {
                        audio_.playSound(SoundId::EnemyFire);
                    }
                }
            }
        }
    }

    // Stage 13: the AttackDirector — the central pacing authority (spec
    // §7). It sees this step's post-move states and launches at most one
    // attacker per elapsed interval, never exceeding the wave's
    // simultaneous-attacker cap. Wave 1 defaults until the wave system
    // lands in Stage 16.
    const int launched = attacks_.update(dt, formation_);
    if (launched > 0) {
        std::printf("AttackDirector launched an attack\n");
    }

    // Stage 9: player bullets vs the formation (gameplay/Combat, SDL-free,
    // runs after both move so it sees their new positions): a hit
    // kills the enemy, consumes the bullet, awards the type's base points
    // through the ScoreManager, and spawns a placeholder effect.
    if (const int killed =
            combat::resolvePlayerBullets(projectiles_, formation_, score_,
                                         effects_);
        killed > 0) {
        stats_.enemiesKilled += killed;
        audio_.playSound(SoundId::EnemyDestroyed);
    }

    // Stage 15: enemy threats vs the player — bullets and diver bodies
    // (gameplay/Combat). Exactly one life per step; a hit starts the
    // 1.5 s Dying phase with a placeholder destruction effect.
    if (combat::resolveEnemyThreats(projectiles_, formation_, player_) != 0) {
        ++stats_.playerDeaths;
        std::printf("Player destroyed (%d lives left)\n", player_.lives());
        audio_.playSound(SoundId::PlayerDestroyed);
        effects_.add(player_.bounds().position(), Player::kWidth,
                     Player::kHeight);
    }

    // Stage 15: the respawn handoff (spec §5) — clear nearby enemy
    // projectiles so the fresh ship never spawns into instant death, then
    // confirm: back at start with the 2 s invulnerability window.
    if (player_.awaitingRespawnConfirm()) {
        const int cleared = projectiles_.removeAll(ProjectileOwner::Enemy);
        player_.confirmRespawn();
        std::printf("Player respawning at start (%d bullets cleared)\n",
                    cleared);
    }

    effects_.update(dt);

    // Stage 19: advance the entity animations on the same fixed step.
    // Purely visual: nothing here can move a gameplay object.
    for (animation::Animator& animator : enemyAnimators_) {
        animator.update(dt);
    }
    playerAnimator_.update(dt);

    // Stage 16: the wave lifecycle (spec §9) — runs last so the clear
    // detection sees this step's post-combat state. On WaveAdvanced the
    // manager has already rebuilt the formation and handed the new
    // bounded parameters to the director.
    const WaveManager::Event waveEvent =
        waves_.update(dt, formation_, attacks_);
    if (waveEvent == WaveManager::Event::WaveCleared) {
        std::printf("Wave %d cleared\n", waves_.wave());
    } else if (waveEvent == WaveManager::Event::WaveAdvanced) {
        std::printf("Wave %d begins\n", waves_.wave());
        stats_.wavesReached = waves_.wave();
        audio_.playSound(SoundId::WaveStart);
    }

    // Stage 17: the last life gone ends the run (Playing -> GameOver).
    if (player_.state() == PlayerState::GameOver) {
        changeState(GameStateId::GameOver);
    }

    // These counters prove the loop runs a deterministic number of steps:
    // simTime_ must equal updateCount_ * dt at all times.
    ++updateCount_;
    simTime_ += dt;
    // Stage 23 telemetry: simulated run seconds.
    stats_.runTimeSeconds += dt;
}

void Game::render()
{
    // Stage 17: each top-level state owns its screen. Title and GameOver
    // are pure text screens; Playing and Paused share the frozen playfield
    // (Paused adds the overlay and never advances).
    switch (states_.current()) {
        case GameStateId::Title:
            renderTitle();
            return;
        case GameStateId::GameOver:
            renderGameOver();
            return;
        case GameStateId::Playing:
        case GameStateId::Paused:
        default:
            renderPlayfield(states_.current() == GameStateId::Paused);
            return;
    }
}

void Game::renderTitle()
{
    devScene_.draw(renderer_);
    renderer_.drawText("GALAXIAN CLONE", {118.0f, 176.0f}, colors::kWhite,
                       24);
    char hudLine[32];
    std::snprintf(hudLine, sizeof(hudLine), "HIGH SCORE %06d",
                  score_.highScore());
    renderer_.drawText(hudLine, {138.0f, 236.0f}, colors::kGreen);
    renderer_.drawText("PRESS ENTER TO START", {122.0f, 320.0f},
                       colors::kWhite, 24);
    renderer_.drawText("ESC QUITS", {192.0f, 380.0f}, colors::kBorder);
    renderer_.present();
}

void Game::renderGameOver()
{
    devScene_.draw(renderer_);
    char line[64];
    renderer_.drawText("GAME OVER", {158.0f, 200.0f}, colors::kEnemyRed, 24);
    std::snprintf(line, sizeof(line), "FINAL SCORE %d", score_.score());
    renderer_.drawText(line, {150.0f, 260.0f}, colors::kGreen);
    std::snprintf(line, sizeof(line), "REACHED WAVE %d", waves_.wave());
    renderer_.drawText(line, {158.0f, 284.0f}, colors::kGreen);
    renderer_.drawText("PRESS ENTER", {166.0f, 340.0f}, colors::kWhite, 24);
    renderer_.present();
}

void Game::renderPlayfield(bool paused)
{
    // Stage 3 test scene: text, static projectile rectangles, screen border.
    // (The dummy enemies and the Stage 4 action table were removed in
    // Stage 8; see DevScene.)
    devScene_.draw(renderer_);

    // Stage 18: the arcade HUD (graphics/Hud) — top bar with SCORE / HIGH
    // / WAVE and the life pips bottom-left. Drawn from live values every
    // frame, so it can never lag the simulation. The interstitial keeps
    // its center notice.
    hud::drawTopBar(renderer_, {score_.score(), score_.highScore(),
                                waves_.wave()});
    hud::drawLivesPips(renderer_, player_.lives());
    if (waves_.interstitial()) {
        renderer_.drawText("WAVE CLEAR", {168.0f, 272.0f},
                           colors::kGreen);
    }

    // Stage 8 (state-aware since Stage 11): the enemy formation. The 24x24
    // enemy sprite coincides with the collision box; slot members draw at
    // formation world position + slot offset, divers at their LIVE dive
    // position (bounds() is state-aware), so a diver visibly leaves its
    // empty slot.
    const Vector2 formationPosition = formation_.position();
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const Enemy& enemy = formation_.at(row, col);
            if (!enemy.alive()) {
                continue;  // holes stay empty (spec §6.3)
            }
            // Stage 19: the current idle frame (the animator is graphics-
            // side only; dead slots are simply not drawn, so destruction
            // can never leave a dangling draw).
            enemyAnimators_[row * EnemyFormation::kColumns + col].draw(
                renderer_, enemy.bounds(formationPosition).position());
        }
    }

    // Stage 9/19: the destruction effects — now the real explosion
    // animation. Each gameplay effect carries its remaining time; the
    // frame is progress-mapped onto the 4-frame one-shot clip (whose total
    // duration equals the effect duration exactly).
    for (int i = 0; i < effects_.count(); ++i) {
        const Effect& e = effects_.effect(i);
        double progress =
            1.0 - e.timeRemaining / GameConfig::get().explosionSeconds;
        if (progress < 0.0) {
            progress = 0.0;
        }
        if (progress > 1.0) {
            progress = 1.0;
        }
        int frame = static_cast<int>(progress * animation::kExplosionClip.frameCount());
        if (frame >= animation::kExplosionClip.frameCount()) {
            frame = animation::kExplosionClip.frameCount() - 1;
        }
        const Texture* tex = renderer_.texture(
            animation::kExplosionClip.textureId(frame));
        if (tex != nullptr) {
            renderer_.drawSprite(*tex, e.bounds().position());
        }
    }

    // Stage 5/15/19: the real Player with its idle animation. The sprite
    // is 24x16 and coincides with the collision box. Hidden while
    // Dying/Respawning/GameOver; blinking at ~4 Hz during the
    // Invulnerable window.
    const bool shipVisible =
        player_.alive() &&
        (player_.state() != PlayerState::Invulnerable ||
         std::fmod(simTime_, kInvulnerableBlinkPeriodSeconds) <
             kInvulnerableBlinkPeriodSeconds * 0.5);
    if (shipVisible) {
        playerAnimator_.draw(renderer_, player_.bounds().position());
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
        std::snprintf(line, sizeof(line), "SIM: %.3f s  STEP: %.1f ms  WAVE %d",
                      simTime_, timestep_.dt() * 1000.0, waves_.wave());
        renderer_.drawText(line, {16.0f, 496.0f}, colors::kGreen);
        const int entities = formation_.aliveCount() +
                             (player_.alive() ? 1 : 0) + projectiles_.count();
        std::snprintf(line, sizeof(line),
                      "ENT: %d (enemy %d, proj %d, atk %d)", entities,
                      formation_.aliveCount(), projectiles_.count(),
                      AttackDirector::activeAttacks(formation_));
        renderer_.drawText(line, {16.0f, 512.0f}, colors::kGreen);
        // Stage 9/15: the ScoreManager's value plus the lives counter (the
        // full HUD lands in Stage 18).
        std::snprintf(line, sizeof(line), "SCORE: %d  KILLS: %d  LIVES: %d",
                      score_.score(), score_.kills(), player_.lives());
        renderer_.drawText(line, {16.0f, 528.0f}, colors::kGreen);

        // Stage 11: the enemy state labels (debug aid, docs/test_plan.md
        // Stage 11): FORMATION / PREPARING / DIVING / ATTACKING / RETURNING
        // above every living enemy.
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                const Enemy& enemy = formation_.at(row, col);
                if (!enemy.alive()) {
                    continue;
                }
                const Rect box = enemy.bounds(formationPosition);
                const Vector2 labelPos{box.x, box.y - 18.0f};
                renderer_.drawText(std::string(enemyStateName(enemy.state())),
                                   labelPos, colors::kGreen, 16);
            }
        }
    }

    // Stage 7 (extended in Stage 8, state-aware since Stage 11): F1
    // collision-box debug overlay. Game collects the live boxes (formation
    // at their true positions + player + projectiles) and hands them to the
    // isolated graphics module; the AABB rule itself is
    // gameplay/Collision.hpp, not here.
    if (collisionDebug_) {
        std::vector<Rect> boxes;
        boxes.reserve(formation_.aliveCount() + 1 + projectiles_.count());
        for (int row = 0; row < EnemyFormation::kRows; ++row) {
            for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                const Enemy& enemy = formation_.at(row, col);
                if (enemy.alive()) {
                    boxes.push_back(enemy.bounds(formationPosition));
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

    // Stage 17: the pause overlay on the frozen scene.
    if (paused) {
        renderer_.drawText("PAUSED", {186.0f, 264.0f}, colors::kEffect, 24);
        renderer_.drawText("ESC TO RESUME", {162.0f, 300.0f},
                           colors::kWhite);
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
                      "sim_time=%.1fs entities=%d score=%d atk=%d lives=%d "
                      "wave=%d\n",
                      fps_, lastFrameSeconds_ * 1000.0, updatesPerSecond_,
                      simTime_, entities, score_.score(),
                      AttackDirector::activeAttacks(formation_),
                      player_.lives(), waves_.wave());
    }

    lastReportSeconds_ = now;
    framesSinceReport_ = 0;
    updatesSinceReport_ = 0;
}

void Game::shutdown()
{
    if (initialized_) {
        // Stage 23: emit the run telemetry on any exit while a run was up.
        if (states_.current() == GameStateId::Playing) {
            std::fprintf(stderr, "%s\n", stats_.summaryLine().c_str());
        }
        // Stage 22: persist the session best on every clean exit (the
        // GameOver transition already saved; this covers quitting mid-run).
        highScoreStore_.save(score_.highScore());
    }
    audio_.shutdown();
    renderer_.shutdown();
    initialized_ = false;
    running_ = false;
}

}  // namespace galaxian
