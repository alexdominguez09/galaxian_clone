// Stage 3 rendering tests (docs/test_plan.md §1).
//
// Runs headless (SDL dummy video driver) and verifies actual framebuffer
// pixels via SDL_RenderReadSurface, so "it rendered" is proven, not
// assumed.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <SDL2/SDL.h>

#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/Types.hpp"
#include "gameplay/Combat.hpp"
#include "gameplay/Effects.hpp"
#include "gameplay/EnemyFormation.hpp"
#include "gameplay/Player.hpp"
#include "gameplay/Projectile.hpp"
#include "gameplay/ScoreManager.hpp"
#include "graphics/DebugOverlay.hpp"
#include "graphics/DevArt.hpp"
#include "graphics/DevScene.hpp"
#include "graphics/Renderer.hpp"
#include "input/Actions.hpp"
#include "input/InputManager.hpp"

using namespace galaxian;

namespace {

void useDummyVideoDriver() { ::setenv("SDL_VIDEODRIVER", "dummy", 1); }

// The dummy video driver emits spurious window events (including
// FOCUS_LOST) as the window is shown and frames are presented. InputManager
// correctly treats FOCUS_LOST as "release all keys", so tests that inject
// synthetic key events must first drain these stale window events, or the
// just-injected key would be immediately released.
void drainSdlEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev) != 0) {
        // discard
    }
}

// Reads the framebuffer back as ARGB8888 (SDL2 has no RenderReadSurface).
// Returns a SDL-managed surface: SDL_FreeSurface releases everything. The
// pixel buffer is copied into the surface because a surface created with
// SDL_CreateRGBSurfaceWithFormatFrom is pre-allocated and SDL will NOT free
// its pixel data (see the SDL2 docs), which would leak under ASan.
SDL_Surface* readback(SDL_Renderer* renderer)
{
    int w = 0;
    int h = 0;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    if (w <= 0 || h <= 0) {
        return nullptr;
    }

    const int pitch = w * 4;
    auto* data = static_cast<std::uint8_t*>(
        SDL_malloc(static_cast<std::size_t>(pitch) * h));
    if (data == nullptr) {
        return nullptr;
    }
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, data,
                             pitch) != 0) {
        SDL_free(data);
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface == nullptr) {
        SDL_free(data);
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        std::memcpy(static_cast<std::uint8_t*>(surface->pixels) +
                        static_cast<std::size_t>(y) * surface->pitch,
                    data + static_cast<std::size_t>(y) * pitch,
                    static_cast<std::size_t>(w) * 4);
    }
    SDL_free(data);
    return surface;
}

std::uint32_t pixelAt(SDL_Surface* surface, int x, int y)
{
    return static_cast<const std::uint32_t*>(surface->pixels)[
        y * (surface->pitch / 4) + x];
}

bool isColor(std::uint32_t argb, const Color& c)
{
    const std::uint32_t expected =
        (static_cast<std::uint32_t>(c.a) << 24) |
        (static_cast<std::uint32_t>(c.r) << 16) |
        (static_cast<std::uint32_t>(c.g) << 8) |
        static_cast<std::uint32_t>(c.b);
    return argb == expected;
}

// Minimal 24-bit BMP writer (BGR, bottom-up, rows padded to 4 bytes).
void writeBmp(const std::string& path, int w, int h,
              const std::vector<std::array<std::uint8_t, 3>>& bgr)
{
    const int rowSize = ((w * 3 + 3) / 4) * 4;
    const int dataSize = rowSize * h;
    const int fileSize = 54 + dataSize;

    std::ofstream out(path, std::ios::binary);
    auto put16 = [&](std::uint16_t v) {
        out.put(static_cast<char>(v & 0xFF));
        out.put(static_cast<char>(v >> 8));
    };
    auto put32 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            out.put(static_cast<char>((v >> (8 * i)) & 0xFF));
        }
    };

    out.put('B');
    out.put('M');
    put32(static_cast<std::uint32_t>(fileSize));
    put16(0);
    put16(0);
    put32(54);  // pixel data offset
    put32(40);  // BITMAPINFOHEADER size
    put32(static_cast<std::uint32_t>(w));
    put32(static_cast<std::uint32_t>(h));
    put16(1);   // planes
    put16(24);  // bpp
    put32(0);   // no compression
    put32(static_cast<std::uint32_t>(dataSize));
    put32(2835);
    put32(2835);  // ~72 dpi
    put32(0);
    put32(0);

    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            const std::array<std::uint8_t, 3>& p = bgr[static_cast<std::size_t>(y) * w + x];
            out.put(static_cast<char>(p[0]));
            out.put(static_cast<char>(p[1]));
            out.put(static_cast<char>(p[2]));
        }
        const int pad = rowSize - w * 3;
        for (int i = 0; i < pad; ++i) {
            out.put('\0');
        }
    }
}

std::string uniqueTempFile(const std::string& ext)
{
    static int counter = 0;
    const std::string name = "galaxian_test_" + std::to_string(::getpid()) +
                             "_" + std::to_string(counter++) + ext;
    return (std::filesystem::temp_directory_path() / name).string();
}

bool fontAvailable()
{
    const char* paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    };
    for (const char* path : paths) {
        if (std::filesystem::exists(path)) {
            return true;
        }
    }
    return false;
}

// Enemy textures indexed by EnemyDefinition::spriteIndex (0 Scout, 1 Guard,
// 2 Commander) — the same mapping Game uses (the composition root keeps it
// out of gameplay/).
using EnemyTextureTable = std::array<const Texture*, kEnemyTypeCount>;

// Resolves the three enemy textures from the renderer's cache.
EnemyTextureTable resolveEnemyTextures(Renderer& renderer)
{
    const char* const ids[kEnemyTypeCount] = {DevArt::kEnemyScout,
                                              DevArt::kEnemyGuard,
                                              DevArt::kEnemyCommander};
    EnemyTextureTable textures{};
    for (int sprite = 0; sprite < kEnemyTypeCount; ++sprite) {
        textures[sprite] = renderer.texture(ids[sprite]);
    }
    return textures;
}

// Draws the formation exactly like Game::render() does (Stage 8, state-
// aware since Stage 11): the 24x24 sprite coincides with the collision box;
// slot members draw at formation world position + slot offset, divers at
// their LIVE dive position (bounds() is state-aware).
void drawFormation(Renderer& renderer, const EnemyFormation& formation,
                   const EnemyTextureTable& textures)
{
    for (int row = 0; row < EnemyFormation::kRows; ++row) {
        for (int col = 0; col < EnemyFormation::kColumns; ++col) {
            const Enemy& enemy = formation.at(row, col);
            if (!enemy.alive()) {
                continue;
            }
            const Texture* texture =
                textures[enemy.definition().spriteIndex];
            if (texture != nullptr) {
                renderer.drawSprite(
                    *texture,
                    enemy.bounds(formation.position()).position());
            }
        }
    }
}

}  // namespace

TEST_CASE("renderer: initialize, clear, pixel readback", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(renderer.initialized());
    CHECK(renderer.logicalWidth() == 448);
    CHECK(renderer.logicalHeight() == 576);

    const Color bg{10, 20, 30, 255};
    renderer.clear(bg);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(frame->w == 448);
    CHECK(frame->h == 576);
    CHECK(isColor(pixelAt(frame, 0, 0), bg));
    CHECK(isColor(pixelAt(frame, 224, 288), bg));
    CHECK(isColor(pixelAt(frame, 447, 575), bg));
    SDL_FreeSurface(frame);

    renderer.shutdown();
    CHECK_FALSE(renderer.initialized());
}

TEST_CASE("renderer: filled and outlined rectangles", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));

    const Color bg{0, 0, 0};
    const Color fill{255, 0, 0};
    const Color line{0, 255, 0};

    renderer.clear(bg);
    renderer.drawFilledRect({100.0f, 100.0f, 50.0f, 40.0f}, fill);
    renderer.drawRect({200.0f, 200.0f, 60.0f, 30.0f}, line);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(isColor(pixelAt(frame, 100, 100), fill));  // filled top-left
    CHECK(isColor(pixelAt(frame, 125, 120), fill));  // filled interior
    CHECK(isColor(pixelAt(frame, 149, 139), fill));  // filled bottom-right
    CHECK(isColor(pixelAt(frame, 99, 120), bg));     // outside filled rect
    CHECK(isColor(pixelAt(frame, 200, 200), line));  // outline top-left
    CHECK(isColor(pixelAt(frame, 259, 229), line));  // outline bottom-right
    CHECK(isColor(pixelAt(frame, 230, 215), bg));    // outline interior empty
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("renderer: sprite draw and flip", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));

    // 4x2 texture: only the top-left pixel is colored.
    SDL_Surface* src =
        SDL_CreateRGBSurfaceWithFormat(0, 4, 2, 32, SDL_PIXELFORMAT_ARGB8888);
    REQUIRE(src != nullptr);
    SDL_FillRect(src, nullptr, 0x00000000);
    static_cast<std::uint32_t*>(src->pixels)[0] = 0xFF112233;
    Texture sprite(SDL_CreateTextureFromSurface(renderer.sdlRenderer(), src));
    SDL_FreeSurface(src);
    REQUIRE(sprite.valid());
    CHECK(sprite.width() == 4);
    CHECK(sprite.height() == 2);

    const Color bg{0, 0, 0};
    const Color mark{0x11, 0x22, 0x33};

    renderer.clear(bg);
    renderer.drawSprite(sprite, {10.0f, 10.0f});
    renderer.drawSprite(sprite, {40.0f, 10.0f}, /*flipH=*/true, /*flipV=*/false);
    renderer.drawSprite(sprite, {70.0f, 10.0f}, /*flipH=*/false, /*flipV=*/true);
    renderer.drawSprite(sprite, {100.0f, 10.0f}, /*flipH=*/true, /*flipV=*/true);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(isColor(pixelAt(frame, 10, 10), mark));  // normal
    CHECK(isColor(pixelAt(frame, 13, 10), bg));
    CHECK(isColor(pixelAt(frame, 10, 11), bg));
    CHECK(isColor(pixelAt(frame, 43, 10), mark));  // flipH
    CHECK(isColor(pixelAt(frame, 40, 10), bg));
    CHECK(isColor(pixelAt(frame, 70, 11), mark));  // flipV
    CHECK(isColor(pixelAt(frame, 70, 10), bg));
    CHECK(isColor(pixelAt(frame, 103, 11), mark)); // both
    CHECK(isColor(pixelAt(frame, 100, 10), bg));
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("renderer: text renders pixels", "[rendering]")
{
    if (!fontAvailable()) {
        SKIP("no TTF font available on this system");
    }
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));

    const Color bg{0, 0, 0};
    const Color fg{255, 255, 255};
    renderer.clear(bg);
    renderer.drawText("ABC", {10.0f, 10.0f}, fg, 24);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    int lit = 0;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 120; ++x) {
            if (!isColor(pixelAt(frame, x, y), bg)) {
                ++lit;
            }
        }
    }
    CHECK(lit > 20);  // glyphs actually drew something
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("renderer: texture() loads and caches BMP files", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));

    // 16x16: red body, blue top-left pixel. (BMP stores BGR.)
    const std::string path = uniqueTempFile(".bmp");
    std::vector<std::array<std::uint8_t, 3>> pixels(16 * 16,
                                                    {10, 10, 200});  // red
    pixels[0] = {200, 10, 10};  // blue
    writeBmp(path, 16, 16, pixels);

    const Texture* tex = renderer.texture(path);
    REQUIRE(tex != nullptr);
    CHECK(tex->width() == 16);
    CHECK(tex->height() == 16);
    CHECK(renderer.texture(path) == tex);  // cached: same pointer

    renderer.clear({0, 0, 0});
    renderer.drawSprite(*tex, {50.0f, 50.0f});
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(isColor(pixelAt(frame, 50, 50), Color{10, 10, 200}));   // blue corner
    CHECK(isColor(pixelAt(frame, 60, 60), Color{200, 10, 10}));   // red body
    SDL_FreeSurface(frame);

    std::filesystem::remove(path);
    renderer.shutdown();
}

TEST_CASE("renderer: texture() returns nullptr for missing files", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    CHECK(renderer.texture("/nonexistent/path/does_not_exist.bmp") == nullptr);
    renderer.shutdown();
}

TEST_CASE("renderer: integer scaling centers and letterboxes", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(renderer.window() != nullptr);

    // 700x900 has the same aspect ratio as 448x576 (7:9) but is not an
    // integer multiple, so integer scaling picks scale 1, centers the
    // content, and letterboxes on all four sides.
    SDL_SetWindowSize(renderer.window(), 700, 900);
    SDL_PumpEvents();

    const Color bg{5, 5, 5};
    const Color center{200, 200, 200};
    renderer.clear(bg);
    renderer.drawFilledRect({222.0f, 284.0f, 8.0f, 8.0f}, center);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(frame->w == 700);
    CHECK(frame->h == 900);

    const int ox = (700 - 448) / 2;  // 126
    const int oy = (900 - 576) / 2;  // 162
    // Letterbox bars are filled with the clear color.
    CHECK(isColor(pixelAt(frame, 0, 0), bg));
    CHECK(isColor(pixelAt(frame, 699, 899), bg));
    CHECK(isColor(pixelAt(frame, ox - 1, oy), bg));      // left bar
    CHECK(isColor(pixelAt(frame, ox + 448, oy), bg));    // right bar
    // Content: logical (222,284)-(229,291) maps to physical (ox+222, oy+284).
    CHECK(isColor(pixelAt(frame, ox + 222, oy + 284), center));
    CHECK(isColor(pixelAt(frame, ox + 229, oy + 291), center));
    // Just outside the content rect is the clear color.
    CHECK(isColor(pixelAt(frame, ox + 221, oy + 284), bg));
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("renderer: window smaller than logical size clips, not blanks",
          "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(renderer.window() != nullptr);

    // 400x500 is smaller than the 448x576 logical size: scale stays 1 and
    // the content is centered + clipped (SDL's logical size would blank it).
    SDL_SetWindowSize(renderer.window(), 400, 500);
    SDL_PumpEvents();

    const Color bg{5, 5, 5};
    const Color center{200, 200, 200};
    renderer.clear(bg);
    renderer.drawFilledRect({222.0f, 284.0f, 8.0f, 8.0f}, center);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(frame->w == 400);
    CHECK(frame->h == 500);

    // scale=1, offset = ((400-448)/2, (500-576)/2) = (-24, -38).
    // logical (222,284) -> physical (198, 246).
    CHECK(isColor(pixelAt(frame, 198, 246), center));
    // Frame is not blank: corners show the clear color.
    CHECK(isColor(pixelAt(frame, 0, 0), bg));
    CHECK(isColor(pixelAt(frame, 399, 499), bg));
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("renderer: 2x integer scaling is pixel-perfect", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(renderer.window() != nullptr);

    // 896x1152 is exactly 2x the logical size: scale 2, no letterbox.
    SDL_SetWindowSize(renderer.window(), 896, 1152);
    SDL_PumpEvents();

    const Color bg{5, 5, 5};
    const Color center{200, 200, 200};
    renderer.clear(bg);
    renderer.drawFilledRect({222.0f, 284.0f, 8.0f, 8.0f}, center);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    CHECK(frame->w == 896);
    CHECK(frame->h == 1152);
    CHECK(renderer.scale() == 2);

    // logical (222,284)-(229,291) maps to physical (444,568)-(459,583):
    // an 8x8 logical rect becomes a 16x16 physical block.
    CHECK(isColor(pixelAt(frame, 444, 568), center));
    CHECK(isColor(pixelAt(frame, 459, 583), center));
    CHECK(isColor(pixelAt(frame, 443, 568), bg));   // just outside (left)
    CHECK(isColor(pixelAt(frame, 460, 583), bg));   // just outside (right)
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("dev scene: renders all elements", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));

    DevScene scene;
    REQUIRE(scene.initialize(renderer));
    scene.draw(renderer);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    // Screen border (1px outline at the edges).
    CHECK(isColor(pixelAt(frame, 0, 0), colors::kBorder));
    CHECK(isColor(pixelAt(frame, 447, 0), colors::kBorder));
    CHECK(isColor(pixelAt(frame, 0, 575), colors::kBorder));
    // Interior background (a point away from all sprites/text/bullets).
    CHECK(isColor(pixelAt(frame, 224, 350), colors::kBlack));
    // Bullet rectangle at (100, 400), 4x10.
    CHECK(isColor(pixelAt(frame, 101, 405), colors::kBullet));
    // Stage 5: the player is no longer part of the dev scene (it is the real
    // gameplay Player, owned and drawn by Game). The spot where the Stage 4
    // demo player used to be is now plain background.
    CHECK(isColor(pixelAt(frame, 212 + 12, 520 + 15), colors::kBlack));
    // Stage 8: the 10 dummy enemies are gone (the real gameplay formation
    // is owned and drawn by Game — see the formation pixel test below). The
    // center of the first old dummy enemy is plain background again...
    CHECK(isColor(pixelAt(frame, 80 + 12, 60 + 12), colors::kBlack));
    // ...and so is the Stage 4 action table area (removed in Stage 8, it
    // overlapped the formation area).
    CHECK(isColor(pixelAt(frame, 20, 250), colors::kBlack));
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

// Stage 8 (docs/test_plan.md, Stage 8): the real gameplay formation renders
// as the 5x8 spec grid — Commander row red, Guard rows yellow, Scout rows
// green — at the exact spec coordinates, with the spec 48/36 px spacing
// (verified by the gaps between boxes). Pixel-verified headlessly.
TEST_CASE("enemy formation: renders the 40-enemy grid at spec coordinates",
          "[enemy][formation][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(DevArt::createAll(renderer));
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);
    for (const Texture* texture : enemyTextures) {
        REQUIRE(texture != nullptr);
    }

    EnemyFormation formation;  // default: 40 enemies at the spec anchor
    DevScene scene;
    REQUIRE(scene.initialize(renderer));

    scene.draw(renderer);
    drawFormation(renderer, formation, enemyTextures);
    renderer.present();

    SDL_Surface* frame = readback(renderer.sdlRenderer());
    REQUIRE(frame != nullptr);
    // Column-0 centers, one per row (box centers: x = 44, y = 76 + 36r).
    CHECK(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));     // row 0
    CHECK(isColor(pixelAt(frame, 44, 112), colors::kEnemyYellow)); // row 1
    CHECK(isColor(pixelAt(frame, 44, 148), colors::kEnemyYellow)); // row 2
    CHECK(isColor(pixelAt(frame, 44, 184), colors::kEnemyGreen));  // row 3
    CHECK(isColor(pixelAt(frame, 44, 220), colors::kEnemyGreen));  // row 4
    // Corners of the grid (center x of col 7 is 380).
    CHECK(isColor(pixelAt(frame, 380, 76), colors::kEnemyRed));    // row 0
    CHECK(isColor(pixelAt(frame, 380, 220), colors::kEnemyGreen)); // row 4
    // Middle of the grid (col 4 center x = 236).
    CHECK(isColor(pixelAt(frame, 236, 220), colors::kEnemyGreen)); // row 4
    // Spec spacing: the gaps between adjacent boxes are background.
    // Column gap: col 0 ends at x=56, col 1 starts at x=80 -> x=68 empty.
    CHECK(isColor(pixelAt(frame, 68, 76), colors::kBlack));
    // Row gap: row 0 ends at y=88, row 1 starts at y=100 -> y=94 empty.
    CHECK(isColor(pixelAt(frame, 44, 94), colors::kBlack));
    // The grid extent: just outside the top-left box (32, 64) is background.
    CHECK(isColor(pixelAt(frame, 31, 64), colors::kBlack));
    CHECK(isColor(pixelAt(frame, 32, 63), colors::kBlack));
    SDL_FreeSurface(frame);
    renderer.shutdown();
}

TEST_CASE("dev art: createAll is idempotent", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(DevArt::createAll(renderer));
    REQUIRE(DevArt::createAll(renderer));  // second call must not fail
    CHECK(renderer.texture(DevArt::kPlayer) != nullptr);
    CHECK(renderer.texture(DevArt::kBullet) != nullptr);
    renderer.shutdown();
}

TEST_CASE("renderer: shutdown is idempotent", "[rendering]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    renderer.shutdown();
    renderer.shutdown();  // must not crash
    CHECK_FALSE(renderer.initialized());
}

// Stage 5 end-to-end (replaces the Stage 4 input-demo test): the real
// gameplay Player, driven by the InputManager's named Actions the same way
// Game does, renders at the spec start position, moves when MoveLeft is
// held, and emits a fire event on a Fire press. Pixel-verified headlessly.
TEST_CASE("player: input drives the real Player (movement + fire, pixels)",
          "[player][input][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    DevScene scene;
    REQUIRE(scene.initialize(renderer));
    const Texture* playerTex = renderer.texture(DevArt::kPlayer);
    REQUIRE(playerTex != nullptr);
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);

    Player player;
    EnemyFormation formation;  // Stage 8: the static formation (drawn only)
    InputManager input;
    const double dt = 1.0 / 60.0;

    // One simulated Game frame, mirroring Game::run() (docs/architecture.md
    // §3.3): poll input, translate to the Player's net held direction and
    // the Fire press edge (updateInputState), step the Player and consume the
    // edge (fixedUpdate), then clear the edges (endFrame).
    auto stepFrame = [&]() {
        input.pollEvents();
        float dir = 0.0f;
        if (input.isHeld(Action::MoveRight)) {
            dir += 1.0f;
        }
        if (input.isHeld(Action::MoveLeft)) {
            dir -= 1.0f;
        }
        player.update(dt, dir);
        if (input.wasPressed(Action::Fire) && player.alive()) {
            player.fire();
        }
        input.endFrame();
    };
    // Mirrors Game::render(): the test scene, the formation, then the real
    // Player on top.
    auto drawFrame = [&]() {
        scene.draw(renderer);
        drawFormation(renderer, formation, enemyTextures);
        if (player.alive()) {
            renderer.drawSprite(*playerTex, player.bounds().position());
        }
        renderer.present();
    };

    // Initial: player centered at (224, 528); the triangle base pixel
    // (224, 535) is cyan.
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        CHECK(isColor(pixelAt(frame, 224, 535), colors::kPlayerCyan));
        SDL_FreeSurface(frame);
    }

    // Hold MoveLeft for 10 frames: the player moves ~36.7 px left.
    drainSdlEvents();  // drop spurious window events from setup/present
    input.injectKeyDown(SDLK_LEFT);
    for (int i = 0; i < 10; ++i) {
        stepFrame();
    }
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // The original center is no longer the player (it moved left)...
        CHECK_FALSE(isColor(pixelAt(frame, 224, 535), colors::kPlayerCyan));
        // ...and the new position (~x=187) is.
        CHECK(isColor(pixelAt(frame, 187, 535), colors::kPlayerCyan));
        SDL_FreeSurface(frame);
    }

    // Release left, press Fire: a single press emits exactly one fire event.
    drainSdlEvents();  // drop spurious window events from the present above
    input.injectKeyUp(SDLK_LEFT);
    input.injectKeyDown(SDLK_SPACE);
    stepFrame();
    CHECK(player.fireCount() == 1);

    // Keep holding Fire: no further fire events (press edge only).
    for (int i = 0; i < 5; ++i) {
        stepFrame();
    }
    CHECK(player.fireCount() == 1);

    renderer.shutdown();
}

// Stage 6 end-to-end (docs/test_plan.md, Stage 6): Space creates a projectile
// at the player position, the bullet is drawn moving upward, the 0.35 s
// cooldown and the 2-bullet cap hold while firing continuously, and the
// bullets disappear offscreen. Pixel-verified headlessly.
TEST_CASE("projectiles: fire spawns bullets that fly up and expire (pixels)",
          "[player][projectile][input][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    DevScene scene;
    REQUIRE(scene.initialize(renderer));
    const Texture* playerTex = renderer.texture(DevArt::kPlayer);
    const Texture* bulletTex = renderer.texture(DevArt::kBullet);
    REQUIRE(playerTex != nullptr);
    REQUIRE(bulletTex != nullptr);
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);

    Player player;
    EnemyFormation formation;  // Stage 8: the static formation (drawn only)
    ProjectileManager projectiles;
    InputManager input;
    const double dt = 1.0 / 60.0;

    // One simulated Game frame, mirroring Game::run() (docs/architecture.md
    // §3.3): poll input, fire on the press edge (the ProjectileManager
    // enforces the cooldown/max), step the bullets, clear the edges.
    auto stepFrame = [&]() {
        input.pollEvents();
        player.update(dt, 0.0f);
        if (input.wasPressed(Action::Fire) && player.alive()) {
            player.fire();
            projectiles.tryFirePlayer(player);
        }
        projectiles.update(dt);
        input.endFrame();
    };
    // Mirrors Game::render(): the dev scene, the formation, then the
    // gameplay objects.
    auto drawFrame = [&]() {
        scene.draw(renderer);
        drawFormation(renderer, formation, enemyTextures);
        if (player.alive()) {
            renderer.drawSprite(*playerTex, player.bounds().position());
        }
        for (int i = 0; i < projectiles.count(); ++i) {
            renderer.drawSprite(*bulletTex,
                                projectiles.projectile(i).position);
        }
        renderer.present();
    };

    // Press Space once: a bullet spawns directly above the player (box
    // 222..226 x 510..520) and moves 8 px up during the same step, so by
    // render time its box is 222..226 x 502..512; center pixel (224, 507)
    // is the bullet color.
    drainSdlEvents();  // drop spurious window events from setup/present
    input.injectKeyDown(SDLK_SPACE);
    stepFrame();
    CHECK(projectiles.count() == 1);
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        CHECK(isColor(pixelAt(frame, 224, 507), colors::kBullet));
        SDL_FreeSurface(frame);
    }

    // Mash fire for 60 more frames (a fresh press edge each frame): the
    // cooldown (0.35 s) and the 2-bullet cap keep at most 2 bullets alive
    // at any time — and 2 is actually reached, so the cap is not tighter
    // than the spec.
    int maxSimultaneous = projectiles.count();
    for (int i = 0; i < 60; ++i) {
        input.injectKeyUp(SDLK_SPACE);
        input.injectKeyDown(SDLK_SPACE);
        stepFrame();
        maxSimultaneous = std::max(maxSimultaneous, projectiles.count());
        CHECK(projectiles.count() <=
              ProjectileManager::kMaxPlayerProjectiles);
    }
    CHECK(maxSimultaneous == ProjectileManager::kMaxPlayerProjectiles);
    drawFrame();

    // Stop firing and let the remaining bullets fly off the top edge.
    input.injectKeyUp(SDLK_SPACE);
    for (int i = 0; i < 90 && projectiles.count() > 0; ++i) {
        stepFrame();
    }
    CHECK(projectiles.count() == 0);
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // The spot above the player is plain background again.
        CHECK(isColor(pixelAt(frame, 224, 515), colors::kBlack));
        SDL_FreeSurface(frame);
    }

    renderer.shutdown();
}

// Stage 7 end-to-end (docs/test_plan.md, Stage 7): F1 (Action::DebugCollision)
// toggles 1-px outlines around the live player/projectile collision boxes,
// and the outlines align exactly with the sprites (the boxes coincide with
// the dev-art sprites). Pixel-verified headlessly, mirroring Game::run() and
// Game::render() the same way the Stage 5/6 tests do.
TEST_CASE("debug overlay: F1 toggles collision boxes aligned with sprites",
          "[collision][input][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    DevScene scene;
    REQUIRE(scene.initialize(renderer));
    const Texture* playerTex = renderer.texture(DevArt::kPlayer);
    const Texture* bulletTex = renderer.texture(DevArt::kBullet);
    REQUIRE(playerTex != nullptr);
    REQUIRE(bulletTex != nullptr);
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);

    Player player;
    EnemyFormation formation;  // Stage 8: boxes join the F1 overlay
    ProjectileManager projectiles;
    InputManager input;
    bool collisionDebug = false;  // mirrors Game::collisionDebug_
    const double dt = 1.0 / 60.0;

    // One simulated Game frame, mirroring Game::run() (docs/architecture.md
    // §3.3): poll input, toggle the debug overlay on the DebugCollision press
    // edge (processEvents), fire on the Fire press edge, step the simulation,
    // clear the edges.
    auto stepFrame = [&]() {
        input.pollEvents();
        if (input.wasPressed(Action::DebugCollision)) {
            collisionDebug = !collisionDebug;
        }
        player.update(dt, 0.0f);
        if (input.wasPressed(Action::Fire) && player.alive()) {
            player.fire();
            projectiles.tryFirePlayer(player);
        }
        projectiles.update(dt);
        input.endFrame();
    };
    // Mirrors Game::render(): the dev scene, the formation, the gameplay
    // objects, then the F1 debug overlay (boxes collected by Game —
    // formation + player + projectiles — drawn by DebugOverlay).
    auto drawFrame = [&]() {
        scene.draw(renderer);
        drawFormation(renderer, formation, enemyTextures);
        if (player.alive()) {
            renderer.drawSprite(*playerTex, player.bounds().position());
        }
        for (int i = 0; i < projectiles.count(); ++i) {
            renderer.drawSprite(*bulletTex,
                                projectiles.projectile(i).position);
        }
        if (collisionDebug) {
            std::vector<Rect> boxes;
            for (int row = 0; row < EnemyFormation::kRows; ++row) {
                for (int col = 0; col < EnemyFormation::kColumns; ++col) {
                    if (formation.at(row, col).alive()) {
                        boxes.push_back(formation.boundsOf(row, col));
                    }
                }
            }
            if (player.alive()) {
                boxes.push_back(player.bounds());
            }
            for (int i = 0; i < projectiles.count(); ++i) {
                boxes.push_back(projectiles.projectile(i).bounds());
            }
            DebugOverlay::drawCollisionBoxes(renderer, boxes,
                                             colors::kDebugBox);
        }
        renderer.present();
    };

    // Player start: center (224, 528), box (212, 520, 24, 16).
    const Rect playerBox = player.bounds();
    REQUIRE(playerBox == Rect{212.0f, 520.0f, 24.0f, 16.0f});

    // Debug off: the player renders (triangle base pixel is cyan) and no
    // debug-box pixels exist at the box corners.
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        CHECK(isColor(pixelAt(frame, 224, 535), colors::kPlayerCyan));
        CHECK_FALSE(isColor(pixelAt(frame, 212, 520), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 235, 535), colors::kDebugBox));
        // Stage 8: the formation renders (row 0 col 0 center is the
        // Commander red) and its boxes are not outlined yet.
        CHECK(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));
        CHECK_FALSE(isColor(pixelAt(frame, 32, 64), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 55, 87), colors::kDebugBox));
        SDL_FreeSurface(frame);
    }

    // Press F1: the overlay turns on.
    drainSdlEvents();  // drop spurious window events from setup/present
    input.injectKeyDown(SDLK_F1);
    stepFrame();
    CHECK(collisionDebug);
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // Player box outline: all four corners...
        CHECK(isColor(pixelAt(frame, 212, 520), colors::kDebugBox));  // TL
        CHECK(isColor(pixelAt(frame, 235, 520), colors::kDebugBox));  // TR
        CHECK(isColor(pixelAt(frame, 212, 535), colors::kDebugBox));  // BL
        CHECK(isColor(pixelAt(frame, 235, 535), colors::kDebugBox));  // BR
        // ...and the edge midpoints (the outline overdraws the sprite).
        CHECK(isColor(pixelAt(frame, 224, 520), colors::kDebugBox));  // top
        CHECK(isColor(pixelAt(frame, 224, 535), colors::kDebugBox));  // bottom
        CHECK(isColor(pixelAt(frame, 212, 528), colors::kDebugBox));  // left
        CHECK(isColor(pixelAt(frame, 235, 528), colors::kDebugBox));  // right
        // The outline is 1 px: just outside the box is not the debug color.
        CHECK_FALSE(isColor(pixelAt(frame, 211, 528), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 236, 528), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 224, 519), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 224, 536), colors::kDebugBox));
        // Stage 8: the formation's boxes are outlined too (the whole 5x8
        // spec grid), aligned with the enemy sprites.
        CHECK(isColor(pixelAt(frame, 32, 64), colors::kDebugBox));   // r0c0 TL
        CHECK(isColor(pixelAt(frame, 55, 87), colors::kDebugBox));   // r0c0 BR
        CHECK(isColor(pixelAt(frame, 368, 208), colors::kDebugBox)); // r4c7 TL
        CHECK(isColor(pixelAt(frame, 391, 231), colors::kDebugBox)); // r4c7 BR
        // The box interior is still the sprite (outline only).
        CHECK(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));
        SDL_FreeSurface(frame);
    }

    // Fire once: a bullet spawns at (222, 510) and moves 8 px up during the
    // same step, so by render time its box is (222, 502, 4, 10).
    drainSdlEvents();  // drop spurious window events from the present above
    input.injectKeyDown(SDLK_SPACE);
    stepFrame();
    CHECK(projectiles.count() == 1);
    CHECK(projectiles.projectile(0).bounds() == Rect{222.0f, 502.0f, 4.0f, 10.0f});
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // Bullet box outline: corners...
        CHECK(isColor(pixelAt(frame, 222, 502), colors::kDebugBox));  // TL
        CHECK(isColor(pixelAt(frame, 225, 511), colors::kDebugBox));  // BR
        // ...but the interior is still the bullet sprite (outline only).
        CHECK(isColor(pixelAt(frame, 224, 507), colors::kBullet));
        // The player box is still outlined at the same time.
        CHECK(isColor(pixelAt(frame, 212, 520), colors::kDebugBox));
        SDL_FreeSurface(frame);
    }

    // Press F1 again: the overlay turns off (the bullet has moved 8 px up to
    // (222, 494) during this step).
    drainSdlEvents();  // drop spurious window events from the present above
    input.injectKeyUp(SDLK_F1);
    input.injectKeyDown(SDLK_F1);
    stepFrame();
    CHECK_FALSE(collisionDebug);
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // No debug color anywhere on the boxes anymore...
        CHECK_FALSE(isColor(pixelAt(frame, 212, 520), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 222, 494), colors::kDebugBox));
        CHECK_FALSE(isColor(pixelAt(frame, 32, 64), colors::kDebugBox));
        // ...and the sprites are back: player base, bullet body, and the
        // formation squares.
        CHECK(isColor(pixelAt(frame, 224, 535), colors::kPlayerCyan));
        CHECK(isColor(pixelAt(frame, 224, 499), colors::kBullet));
        CHECK(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));
        SDL_FreeSurface(frame);
    }

    renderer.shutdown();
}

// Stage 10 end-to-end (docs/test_plan.md, Stage 10): the formation sways
// horizontally around the anchor — after one simulated second it sits at
// its rightmost offset (+32 px), after three at its leftmost (-32 px) —
// with every enemy keeping its slot spacing and dead enemies staying gone.
// Pixel-verified headlessly, mirroring Game::run()/fixedUpdate().
TEST_CASE("enemy formation: oscillates smoothly (pixels)",
          "[enemy][formation][motion][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(DevArt::createAll(renderer));
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);
    DevScene scene;
    REQUIRE(scene.initialize(renderer));

    EnemyFormation formation;
    const double dt = 1.0 / 60.0;

    // Mirrors Game::fixedUpdate() for a no-input frame: only the formation
    // moves in this test.
    auto stepFrame = [&]() { formation.update(dt); };
    auto drawFrame = [&]() {
        scene.draw(renderer);
        drawFormation(renderer, formation, enemyTextures);
        renderer.present();
    };

    // Column centers are x = anchor.x + 48c + 12; row centers are y =
    // 64 + 36r + 12. Row 0 is Commander red, rows 3-4 Scout green.

    // t = 1 s (60 steps): x offset is exactly +32 px.
    for (int i = 0; i < 60; ++i) {
        stepFrame();
    }
    CHECK(formation.position().x == 64.0f);
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // Col 0 moved from center x=44 to x=76; col 7 from 380 to 412.
        CHECK(isColor(pixelAt(frame, 76, 76), colors::kEnemyRed));   // r0c0
        CHECK(isColor(pixelAt(frame, 412, 76), colors::kEnemyRed));  // r0c7
        CHECK(isColor(pixelAt(frame, 76, 220), colors::kEnemyGreen)); // r4c0
        // The old spots are background now.
        CHECK_FALSE(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));
        SDL_FreeSurface(frame);
    }

    // t = 3 s (180 steps total): x offset is exactly -32 px.
    for (int i = 0; i < 120; ++i) {
        stepFrame();
    }
    CHECK(formation.position().x == 0.0f);
    drawFrame();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        CHECK(isColor(pixelAt(frame, 12, 76), colors::kEnemyRed));   // r0c0
        CHECK(isColor(pixelAt(frame, 348, 76), colors::kEnemyRed));  // r0c7
        CHECK(isColor(pixelAt(frame, 12, 220), colors::kEnemyGreen)); // r4c0
        SDL_FreeSurface(frame);
    }

    renderer.shutdown();
}

// Stage 9 end-to-end (docs/test_plan.md, Stage 9): a player bullet that
// reaches the formation destroys the enemy it overlaps — the enemy's pixels
// become the white placeholder effect, the bullet is consumed, the
// ScoreManager holds the type's points, and after the effect expires
// (0.25 s = 15 fixed steps) a permanent hole remains. A second shot flies
// through the dead hole and destroys the next row. Pixel-verified
// headlessly, mirroring Game::run()/fixedUpdate()/render() the same way the
// Stage 5/6/7 tests do.
TEST_CASE("combat: player bullets destroy formation enemies (pixels)",
          "[combat][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    DevScene scene;
    REQUIRE(scene.initialize(renderer));
    const Texture* playerTex = renderer.texture(DevArt::kPlayer);
    const Texture* bulletTex = renderer.texture(DevArt::kBullet);
    REQUIRE(playerTex != nullptr);
    REQUIRE(bulletTex != nullptr);
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);

    Player player;  // stays at the start center (224, 528)
    EnemyFormation formation;
    ProjectileManager projectiles;
    ScoreManager score;
    EffectManager effects;
    InputManager input;
    const double dt = 1.0 / 60.0;

    // One simulated Game frame, mirroring Game::run()/fixedUpdate()
    // (docs/architecture.md §3.1/§3.3): poll input, fire on the press edge,
    // step the player and bullets, resolve combat, step the effects, clear
    // the edges. The player never moves (direction 0).
    auto stepFrame = [&]() {
        input.pollEvents();
        player.update(dt, 0.0f);
        if (input.wasPressed(Action::Fire) && player.alive()) {
            player.fire();
            projectiles.tryFirePlayer(player);
        }
        projectiles.update(dt);
        combat::resolvePlayerBullets(projectiles, formation, score, effects);
        effects.update(dt);
        input.endFrame();
    };
    // Mirrors Game::render(): the dev scene, the formation (dead enemies
    // leave holes), the placeholder effects, then the gameplay objects.
    auto drawFrame = [&]() {
        scene.draw(renderer);
        drawFormation(renderer, formation, enemyTextures);
        for (int i = 0; i < effects.count(); ++i) {
            renderer.drawFilledRect(effects.effect(i).bounds(),
                                    colors::kEffect);
        }
        if (player.alive()) {
            renderer.drawSprite(*playerTex, player.bounds().position());
        }
        for (int i = 0; i < projectiles.count(); ++i) {
            renderer.drawSprite(*bulletTex,
                                 projectiles.projectile(i).position);
        }
        renderer.present();
    };
    auto readPixels = [&]() {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        return frame;
    };

    // The player's bullets spawn at box x 222..226, which overlaps column 4
    // (boxes x 224..248) but no other column. Row 4 col 4 is a Scout (green,
    // center pixel (236, 220)); row 3 col 4 is a Scout at (236, 184).
    drawFrame();
    {
        SDL_Surface* frame = readPixels();
        CHECK(isColor(pixelAt(frame, 236, 220), colors::kEnemyGreen));  // r4c4
        CHECK(isColor(pixelAt(frame, 236, 184), colors::kEnemyGreen));  // r3c4
        SDL_FreeSurface(frame);
    }
    CHECK(formation.aliveCount() == 40);
    CHECK(score.score() == 0);

    // Shot 1 (frame 1): the bullet flies up 8 px per step (480 px/s) and
    // first overlaps the row-4 col-4 box on its 35th step (y = 230).
    drainSdlEvents();  // drop spurious window events from setup/present
    input.injectKeyDown(SDLK_SPACE);
    for (int frame = 1; frame <= 34; ++frame) {
        stepFrame();
    }
    CHECK(formation.at(4, 4).alive());  // not reached yet (y = 238)
    CHECK(score.score() == 0);
    stepFrame();  // frame 35
    CHECK_FALSE(formation.at(4, 4).alive());
    CHECK(score.score() == 50);  // Scout points
    CHECK(score.kills() == 1);
    CHECK(formation.aliveCount() == 39);
    CHECK(effects.count() == 1);
    CHECK(effects.effect(0).bounds() == Rect{224.0f, 208.0f, 24.0f, 24.0f});
    drawFrame();
    {
        SDL_Surface* frame = readPixels();
        // The kill site shows the white placeholder effect (over the hole).
        CHECK(isColor(pixelAt(frame, 236, 220), colors::kEffect));
        CHECK(isColor(pixelAt(frame, 225, 209), colors::kEffect));  // box TL
        // The rest of the formation is untouched.
        CHECK(isColor(pixelAt(frame, 236, 184), colors::kEnemyGreen));  // r3c4
        CHECK(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));      // r0c0
        SDL_FreeSurface(frame);
    }

    // The effect lasts 0.25 s = 15 fixed steps: added during frame 35, it
    // expires during frame 49. Idle until it is gone; the hole is plain
    // background afterwards.
    for (int frame = 36; frame <= 50; ++frame) {
        stepFrame();
    }
    CHECK(effects.count() == 0);
    drawFrame();
    {
        SDL_Surface* frame = readPixels();
        CHECK(isColor(pixelAt(frame, 236, 220), colors::kBlack));  // the hole
        CHECK(isColor(pixelAt(frame, 236, 184), colors::kEnemyGreen));
        SDL_FreeSurface(frame);
    }

    // Shot 2 (frame 51, a fresh press edge after releasing): the 0.35 s
    // cooldown has long elapsed. The bullet passes through the dead row-4
    // hole (steps 35-38 of its flight) and kills row 3 col 4 on its 40th
    // step (frame 51 + 39 = 90) for a second 50 points.
    drainSdlEvents();  // drop spurious window events from the present above
    input.injectKeyUp(SDLK_SPACE);
    input.injectKeyDown(SDLK_SPACE);
    for (int frame = 51; frame <= 90; ++frame) {
        stepFrame();
    }
    CHECK_FALSE(formation.at(3, 4).alive());
    CHECK_FALSE(formation.at(4, 4).alive());
    CHECK(score.score() == 100);  // 50 + 50: the dead hole was NOT re-scored
    CHECK(score.kills() == 2);
    CHECK(formation.aliveCount() == 38);
    CHECK(effects.count() == 1);
    CHECK(effects.effect(0).bounds() == Rect{224.0f, 172.0f, 24.0f, 24.0f});
    drawFrame();
    {
        SDL_Surface* frame = readPixels();
        // The row-3 hole shows the effect; the row-4 hole stays a hole.
        CHECK(isColor(pixelAt(frame, 236, 184), colors::kEffect));
        CHECK(isColor(pixelAt(frame, 236, 220), colors::kBlack));
        // Neighbors are intact.
        CHECK(isColor(pixelAt(frame, 188, 184), colors::kEnemyGreen));  // r3c3
        CHECK(isColor(pixelAt(frame, 44, 76), colors::kEnemyRed));      // r0c0
        SDL_FreeSurface(frame);
    }

    renderer.shutdown();
}

// Stage 11 end-to-end (docs/test_plan.md, Stage 11): a diving enemy draws
// at its LIVE dive position away from its empty slot, and the F2 debug
// state label renders above it. Pixel-verified headlessly, mirroring
// Game::render() (state-aware drawFormation + state labels).
TEST_CASE("enemy state machine: a diver renders away from its slot "
          "with its state label",
          "[enemy][statemachine][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    REQUIRE(DevArt::createAll(renderer));
    const EnemyTextureTable enemyTextures = resolveEnemyTextures(renderer);
    DevScene scene;
    REQUIRE(scene.initialize(renderer));

    EnemyFormation formation;  // static anchor in this test
    Enemy& scout = formation.at(4, 0);  // Scout green, slot box (32, 208)
    REQUIRE(scout.beginDive(DivePattern::CenterAttack));

    // 110 updates = 30 preparing + 80 along the CenterAttack arc:
    // scratch value t = 0.445173, top y = 300.807373 (box spans ~[300.8,
    // 324.8]).
    const double dt = 1.0 / 60.0;
    for (int i = 0; i < 110; ++i) {
        scout.update(dt, formation.position());
    }
    REQUIRE(scout.state() == EnemyState::Diving);
    const Vector2 pos = scout.screenPosition(formation.position());
    CHECK(pos.y == Catch::Approx(300.807373).margin(1e-3));
    const int boxTop = static_cast<int>(pos.y);          // 300
    const int centerX = static_cast<int>(pos.x) + 12;    // 44

    scene.draw(renderer);
    drawFormation(renderer, formation, enemyTextures);
    renderer.present();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // The diver is drawn at its live position (interior pixels green).
        CHECK(isColor(pixelAt(frame, centerX, boxTop + 12),
                      colors::kEnemyGreen));
        CHECK(isColor(pixelAt(frame, centerX - 5, boxTop + 18),
                      colors::kEnemyGreen));
        // Its old slot is plain background now.
        CHECK(isColor(pixelAt(frame, 44, 220), colors::kBlack));
        SDL_FreeSurface(frame);
    }

    // Mirror Game's F2 state-label rendering above each living enemy.
    scene.draw(renderer);
    drawFormation(renderer, formation, enemyTextures);
    renderer.drawText(std::string(enemyStateName(scout.state())),
                      {pos.x, pos.y - 18.0f}, colors::kGreen, 16);
    renderer.present();
    if (!fontAvailable()) {
        SKIP("no TTF font available on this system");
    } else {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // The DIVING label lit up the strip between the slot row above and
        // the diver's new position.
        int lit = 0;
        for (int y = boxTop - 17; y <= boxTop - 2; ++y) {
            for (int x = static_cast<int>(pos.x);
                 x < static_cast<int>(pos.x) + 40; ++x) {
                if (!isColor(pixelAt(frame, x, y), colors::kBlack)) {
                    ++lit;
                }
            }
        }
        CHECK(lit > 10);  // glyphs actually drew something
        SDL_FreeSurface(frame);
    }

    renderer.shutdown();
}
