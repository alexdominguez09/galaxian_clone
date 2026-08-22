// Stage 17 game state management tests (docs/test_plan.md, Stage 17).
//
// The StateMachine and the transition table are SDL-free pure logic, so
// the whole spec §10 graph is verified headlessly. A second section boots
// the REAL Game under the dummy video driver and drives it with synthetic
// key presses through the public injectKey() hook — including the
// pause-frozen-simulation check via the smoke update counter.

#include <catch2/catch_test_macros.hpp>

#include <SDL2/SDL.h>

#include <string>

#include "core/Game.hpp"
#include "states/GameState.hpp"
#include "states/StateMachine.hpp"

using namespace galaxian;

namespace {

void useDummyVideoDriver() { ::setenv("SDL_VIDEODRIVER", "dummy", 1); }

}  // namespace

TEST_CASE("states: every legal transition accepted, illegal rejected",
          "[states]")
{
    // The expected graph, stated independently from spec §10.
    struct Edge {
        GameStateId from, to;
    };
    constexpr Edge kLegalEdges[] = {
        {GameStateId::Title, GameStateId::Playing},
        {GameStateId::Playing, GameStateId::Paused},
        {GameStateId::Paused, GameStateId::Playing},
        {GameStateId::Playing, GameStateId::GameOver},
        {GameStateId::GameOver, GameStateId::Title},
    };
    auto expectedLegal = [&](GameStateId f, GameStateId t) {
        for (const Edge& e : kLegalEdges) {
            if (e.from == f && e.to == t) {
                return true;
            }
        }
        return false;
    };

    int legal = 0;
    int illegal = 0;
    for (int fi = 0; fi < kGameStateCount; ++fi) {
        for (int ti = 0; ti < kGameStateCount; ++ti) {
            const GameStateId from = static_cast<GameStateId>(fi);
            const GameStateId to = static_cast<GameStateId>(ti);
            if (expectedLegal(from, to)) {
                ++legal;
                INFO("legal pair " << fi << "->" << ti);
                CHECK(isLegalGameStateTransition(from, to));
            } else {
                ++illegal;
                INFO("illegal pair " << fi << "->" << ti);
                CHECK_FALSE(isLegalGameStateTransition(from, to));
            }
        }
    }
    CHECK(legal == 5);   // exactly the spec graph
    CHECK(illegal == 11);

    // Compile-time spot checks (the table is constexpr).
    STATIC_REQUIRE(isLegalGameStateTransition(GameStateId::Title,
                                              GameStateId::Playing));
    STATIC_REQUIRE_FALSE(isLegalGameStateTransition(
        GameStateId::Title, GameStateId::GameOver));
    STATIC_REQUIRE_FALSE(isLegalGameStateTransition(
        GameStateId::Paused, GameStateId::GameOver));
}

TEST_CASE("states: the machine walks the graph and rejects everything "
          "else",
          "[states]")
{
    struct Ctx {
        int changes = 0;
        std::string log;
    } ctx;
    struct Thunk {
        static void call(GameStateId from, GameStateId to, void* ud)
        {
            auto* c = static_cast<Ctx*>(ud);
            ++c->changes;
            c->log += std::to_string(static_cast<int>(from)) + ">" +
                      std::to_string(static_cast<int>(to)) + ";";
        }
    };

    StateMachine sm;
    sm.setCallback(&Thunk::call, &ctx);
    CHECK(sm.current() == GameStateId::Title);

    // Illegal from Title.
    CHECK_FALSE(sm.request(GameStateId::Paused));
    CHECK_FALSE(sm.request(GameStateId::GameOver));
    CHECK_FALSE(sm.request(GameStateId::Title));  // self
    CHECK(sm.current() == GameStateId::Title);
    CHECK(ctx.changes == 0);  // no callback fired

    // Full cycle: start -> pause/resume -> die -> game over -> restart.
    REQUIRE(sm.request(GameStateId::Playing));
    CHECK(sm.current() == GameStateId::Playing);
    REQUIRE(sm.request(GameStateId::Paused));
    REQUIRE(sm.request(GameStateId::Playing));
    REQUIRE(sm.request(GameStateId::GameOver));
    REQUIRE(sm.request(GameStateId::Title));
    CHECK(ctx.changes == 5);
    // Callback order: fired AFTER each change, with (from, to).
    CHECK(ctx.log == "0>1;1>2;2>1;1>3;3>0;");
}

TEST_CASE("states: 100 full restart cycles and 100 pause cycles without "
          "drift",
          "[states][stress]")
{
    struct Ctx {
        int enters[4] = {0, 0, 0, 0};
        int changes = 0;
    } ctx;
    struct Thunk {
        static void call(GameStateId, GameStateId to, void* ud)
        {
            auto* c = static_cast<Ctx*>(ud);
            ++c->enters[static_cast<int>(to)];
            ++c->changes;
        }
    };

    StateMachine sm;
    sm.setCallback(&Thunk::call, &ctx);

    // start -> die -> game over -> restart x100.
    for (int i = 0; i < 100; ++i) {
        REQUIRE(sm.request(GameStateId::Playing));
        REQUIRE(sm.request(GameStateId::GameOver));
        REQUIRE(sm.request(GameStateId::Title));
    }
    CHECK(ctx.changes == 300);
    CHECK(ctx.enters[0] == 100);  // Title re-entered every restart
    CHECK(ctx.enters[1] == 100);
    CHECK(ctx.enters[3] == 100);
    CHECK(ctx.enters[2] == 0);    // never paused in this loop
    CHECK(sm.current() == GameStateId::Title);

    // pause -> resume x100 (mid-game). The section first requests Playing
    // once to get there, then flips Paused/Playing a hundred times.
    REQUIRE(sm.request(GameStateId::Playing));
    for (int i = 0; i < 100; ++i) {
        REQUIRE(sm.request(GameStateId::Paused));
        REQUIRE(sm.request(GameStateId::Playing));
    }
    CHECK(ctx.changes == 300 + 1 + 200);
    CHECK(ctx.enters[2] == 100);
    CHECK(ctx.enters[1] == 201);  // 100 starts + the entry + 100 resumes
    CHECK(sm.current() == GameStateId::Playing);
}

TEST_CASE("game states: the real Game boots to TITLE and follows keys",
          "[states][game][sdl]")
{
    useDummyVideoDriver();
    Game game;
    REQUIRE(game.initialize());
    REQUIRE(game.state() == GameStateId::Title);
    // Time-based pump helper: run() exits after `seconds` of WALL time,
    // so while PLAYING roughly seconds*60 fixed steps accumulate
    // (frame-pumping would be too fast to yield any steps under the dummy
    // driver).
    auto pump = [&game](double seconds) {
        game.setSmokeSeconds(seconds);
        game.run();
        game.setSmokeSeconds(0.0);
    };

    pump(0.05);  // settle one title screen render
    CHECK(game.state() == GameStateId::Title);

    // Enter starts a fresh game...
    game.injectKey(SDLK_RETURN);
    pump(0.25);
    CHECK(game.state() == GameStateId::Playing);
    CHECK(game.player().lives() == Player::kLives);
    CHECK(game.score().score() == 0);
    CHECK(game.waves().wave() == 1);

    // ...Escape pauses, and the simulation FREEZES: half a second of wall
    // time adds zero updates while paused.
    game.injectKey(SDLK_ESCAPE);
    pump(0.05);
    CHECK(game.state() == GameStateId::Paused);
    const int frozenAtPause = game.smokeResultUpdates();
    CHECK(frozenAtPause > 0);  // the game really simulated before pausing
    pump(0.50);
    CHECK(game.state() == GameStateId::Paused);
    CHECK(game.smokeResultUpdates() == frozenAtPause);

    // Escape resumes the same game (no reset: updates continue growing).
    game.injectKey(SDLK_ESCAPE);
    pump(0.25);
    CHECK(game.state() == GameStateId::Playing);
    CHECK(game.smokeResultUpdates() > frozenAtPause);

    game.shutdown();
}
