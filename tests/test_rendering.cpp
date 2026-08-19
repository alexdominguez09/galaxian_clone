// Stage 3 rendering tests (docs/test_plan.md §1).
//
// Runs headless (SDL dummy video driver) and verifies actual framebuffer
// pixels via SDL_RenderReadSurface, so "it rendered" is proven, not
// assumed.

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
    // Player triangle at (212, 520), 24x16: base row is filled.
    CHECK(isColor(pixelAt(frame, 212 + 12, 520 + 15), colors::kPlayerCyan));
    // First enemy (commander, red) at (80, 60): center of the 24x24 square.
    CHECK(isColor(pixelAt(frame, 80 + 12, 60 + 12), colors::kEnemyRed));
    // Bullet rectangle at (100, 400), 4x10.
    CHECK(isColor(pixelAt(frame, 101, 405), colors::kBullet));
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

TEST_CASE("dev scene: Stage 4 input demo moves the player and fires",
          "[input][rendering][sdl]")
{
    useDummyVideoDriver();
    Renderer renderer;
    REQUIRE(renderer.initialize(448, 576, false));
    DevScene scene;
    REQUIRE(scene.initialize(renderer));
    InputManager input;

    const float dt = 1.0f / 60.0f;

    // Initial: player centered at x=224; the base pixel (224, 535) is cyan.
    scene.draw(renderer);
    renderer.present();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        CHECK(isColor(pixelAt(frame, 224, 535), colors::kPlayerCyan));
        SDL_FreeSurface(frame);
    }

    // Hold MoveLeft for 10 frames: the player moves ~36.7 px left.
    drainSdlEvents();  // drop spurious window events from setup/present
    input.injectKeyDown(SDLK_LEFT);
    input.pollEvents();
    for (int i = 0; i < 10; ++i) {
        scene.update(dt, input);
        input.endFrame();
    }
    scene.draw(renderer);
    renderer.present();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // The original center is no longer the player (it moved left)...
        CHECK_FALSE(isColor(pixelAt(frame, 224, 535), colors::kPlayerCyan));
        // ...and the new position (~x=187) is.
        CHECK(isColor(pixelAt(frame, 187, 535), colors::kPlayerCyan));
        SDL_FreeSurface(frame);
    }

    // Fire: a single press lights the projectile flash above the player.
    drainSdlEvents();  // drop spurious window events from the present above
    input.injectKeyUp(SDLK_LEFT);
    input.injectKeyDown(SDLK_SPACE);
    input.pollEvents();
    scene.update(dt, input);
    input.endFrame();
    scene.draw(renderer);
    renderer.present();
    {
        SDL_Surface* frame = readback(renderer.sdlRenderer());
        REQUIRE(frame != nullptr);
        // Bullet (4x10) is drawn at (centerX - 2, 506); center is ~187 now.
        CHECK(isColor(pixelAt(frame, 187, 510), colors::kBullet));
        SDL_FreeSurface(frame);
    }

    renderer.shutdown();
}
