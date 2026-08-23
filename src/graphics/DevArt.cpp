#include "DevArt.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/Types.hpp"

namespace galaxian {
namespace DevArt {

namespace {

SDL_Surface* makeSurface(int w, int h)
{
    // 32-bit ARGB, fully transparent initially.
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (surface != nullptr) {
        SDL_FillRect(surface, nullptr, 0x00000000);
    }
    return surface;
}

void setPixel(SDL_Surface* surface, int x, int y, const Color& c)
{
    if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) {
        return;
    }
    const std::uint32_t argb =
        (static_cast<std::uint32_t>(c.a) << 24) |
        (static_cast<std::uint32_t>(c.r) << 16) |
        (static_cast<std::uint32_t>(c.g) << 8) |
        static_cast<std::uint32_t>(c.b);
    std::uint32_t* pixel =
        static_cast<std::uint32_t*>(surface->pixels) + y * (surface->pitch / 4) + x;
    *pixel = argb;
}

// Filled square with a 1px darker border.
void fillSquare(SDL_Surface* surface, const Color& fill, const Color& border)
{
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            const bool edge =
                (x == 0) || (y == 0) || (x == surface->w - 1) ||
                (y == surface->h - 1);
            setPixel(surface, x, y, edge ? border : fill);
        }
    }
}

// ---- Stage 24: ASCII-bitmap sprite forge -------------------------------
//
// Original pixel art authored as mirrored half-rows: each row string holds
// the LEFT half (ceil(w/2) chars); the right half mirrors it. Palette keys
// map to colors; '.'/' ' are transparent. Keeps the hand-drawn sprites
// compact and guarantees symmetry.
struct SpritePalette {
    Color operator()(char key) const
    {
        switch (key) {
            case 'C': return body;      // main hull colour
            case 'L': return light;     // light tone (wings/highlights)
            case 'X': return shade;     // dark outline / shade
            case 'W': return white;     // white accent
            case 'A': return accentA;   // hot accent (thruster/crest)
            case 'B': return accentB;   // second accent (tips/details)
            default:  return Color{0, 0, 0, 0};  // transparent
        }
    }
    Color body;
    Color light;
    Color shade;
    Color white;
    Color accentA;
    Color accentB;
};

void stampBitmap(SDL_Surface* surface,
                 const std::vector<std::string>& halfRows,
                 const SpritePalette& pal)
{
    const int w = surface->w;
    const int h = surface->h;
    const int half = (w + 1) / 2;  // left-half width incl. centre column
    for (int y = 0; y < h && y < static_cast<int>(halfRows.size()); ++y) {
        const std::string& row = halfRows[y];
        if (static_cast<int>(row.size()) < half) {
            // A short row would silently leave the sprite's centre
            // transparent (a split, "debuggy" sprite). The bitmaps below
            // are all full-width; this guard keeps it that way.
            std::fprintf(stderr,
                         "galaxian: dev art: bitmap row %d has %zu of %d "
                         "columns\n",
                         y, row.size(), half);
        }
        for (int hx = 0; hx < half; ++hx) {
            const char key =
                hx < static_cast<int>(row.size()) ? row[hx] : '.';
            if (key == '.' || key == ' ') {
                continue;
            }
            const Color color = pal(key);
            setPixel(surface, hx, y, color);
            setPixel(surface, w - 1 - hx, y, color);  // mirror
        }
    }
}

// ---- Stage 24 sprite bitmaps (original, Galaxian-style) ----------------
//
// All sprites are authored as MIRRORED HALF-ROWS: each string holds the
// left 12 columns of a 24px-wide sprite; the right side mirrors it (so
// every row is EXACTLY 12 chars and the centre seam x11/x12 is explicit).
// Keys:
//   C main body | L light tone (wings) | X dark accent | W white
//   A hot accent (spine/eyes/dome) | B second accent (wings/claws) | clear
//
// The designs follow the arcade reference shots in
// assets/sprites/examples_from_internet: a white fighter with cyan wings
// and a red spine (player), teal drone bugs with red eyes (scouts), red
// escort bugs with yellow eyes and claws (guards), and the flagship --
// yellow swept wings, orange dome, blue wing-tip bars (commanders).
//
// PLAYER FIGHTER 24x16 -- white hull, cyan wings, red spine + nose tip.
// Frame B lights the cyan thruster bar at the base.
const std::vector<std::string> kPlayerRowsA = {
    "...........A",
    "...........A",
    "..........AA",
    "..........AA",
    ".........CAA",
    ".........CAA",
    ".........CAA",
    "....B....CAA",
    "...BB...CCAA",
    "..BBB..CCCAA",
    ".BBBB.CCCCAA",
    ".BBBBCCCCCAA",
    "BBBBBCCCCCAA",
    ".BBBCCCCCCAA",
    "......CCCCAA",
    ".........BBB",
};
const std::vector<std::string> kPlayerRowsB = {
    "...........A",
    "...........A",
    "..........AA",
    "..........AA",
    ".........CAA",
    ".........CAA",
    ".........CAA",
    "....B....CAA",
    "...BB...CCAA",
    "..BBB..CCCAA",
    ".BBBB.CCCCAA",
    ".BBBBCCCCCAA",
    "BBBBBCCCCCAA",
    ".BBBCCCCCCAA",
    "......CCCCAA",
    ".......BBBBB",
};

// SCOUT DRONE 24x24 -- teal bug: magenta antennae, red eyes, blue
// segmented wings. Frame B raises the wings one notch (the signature
// flap); the body rows stay put so the flap reads as wings, not wobble.
const std::vector<std::string> kScoutRowsA = {
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "........B...",
    "........B...",
    ".........B..",
    "........CCCC",
    ".......CCCCC",
    ".......CCACC",
    ".......CCACC",
    "..LLLLCCCCCC",
    "LLLLL.CCCCCC",
    "LLLLL.CCCCCC",
    "..LLLLCCCCCC",
    "........CCCC",
    ".........CCC",
    "........CC..",
    ".......CC...",
    "............",
    "............",
    "............",
};
const std::vector<std::string> kScoutRowsB = {
    "............",
    "............",
    "............",
    "............",
    "............",
    "............",
    "........B...",
    "........B...",
    ".........B..",
    "........CCCC",
    ".......CCCCC",
    ".......CCACC",
    "..LLLL.CCACC",
    "LLLLL.CCCCCC",
    "LLLLL.CCCCCC",
    "..LLLLCCCCCC",
    "........CCCC",
    ".........CCC",
    "........CC..",
    ".......CC...",
    "............",
    "............",
    "............",
    "............",
};

// GUARD ESCORT 24x24 -- red bug: orange claws raised, yellow eyes, blue
// wings. Frame B pinches the claws and raises the wings one notch.
const std::vector<std::string> kGuardRowsA = {
    "............",
    "............",
    "............",
    "............",
    "....BB......",
    ".....BB.....",
    "......B.....",
    "......B.....",
    "......BCCCCC",
    ".....BCCCCCC",
    ".....BCCACCC",
    ".....BCCACCC",
    "......CCCCCC",
    ".LLLLCCCCCCC",
    "LLLLLCCCCCCC",
    "LLLLLCCCCCCC",
    ".LLLLCCCCCCC",
    ".....CCCCCCC",
    "......CCCC..",
    "......CC....",
    ".....C......",
    "............",
    "............",
    "............",
};
const std::vector<std::string> kGuardRowsB = {
    "............",
    "............",
    "............",
    "............",
    ".....BB.....",
    "......B.....",
    "......B.....",
    "......B.....",
    "......BCCCCC",
    ".....BCCCCCC",
    ".....BCCACCC",
    ".....BCCACCC",
    ".LLLLCCCCCCC",
    "LLLLLCCCCCCC",
    "LLLLLCCCCCCC",
    ".LLLLCCCCCCC",
    ".....CCCCCCC",
    "......CCCC..",
    "......CC....",
    ".....C......",
    "............",
    "............",
    "............",
    "............",
};

// COMMANDER FLAGSHIP 24x24 -- yellow swept wings, orange dome, red nose,
// blue wing-tip bars, centre tail. Frame B pings the dome with a white
// glint and extends the tail one pixel.
const std::vector<std::string> kCommanderRowsA = {
    "............",
    "............",
    "...........X",
    "...........X",
    "..........AA",
    "..........AA",
    ".........AAA",
    ".........AAA",
    "..L..CCCCCAA",
    "..LLCCCCCCAA",
    "..LLCCCCCCAA",
    ".LLCCCCCCCCC",
    ".LLCCCCCCCCC",
    "..LCCCCCCCCC",
    "...CCCCCCCCC",
    ".....CCCCCCC",
    ".......CCCCC",
    "..........CC",
    "..........CC",
    "...........C",
    "............",
    "............",
    "............",
    "............",
};
const std::vector<std::string> kCommanderRowsB = {
    "............",
    "............",
    "...........X",
    "...........X",
    "..........WA",
    "..........AA",
    ".........AAA",
    ".........AAA",
    "..L..CCCCCAA",
    "..LLCCCCCCAA",
    "..LLCCCCCCAA",
    ".LLCCCCCCCCC",
    ".LLCCCCCCCCC",
    "..LCCCCCCCCC",
    "...CCCCCCCCC",
    ".....CCCCCCC",
    ".......CCCCC",
    "..........CC",
    "..........CC",
    "...........C",
    "...........C",
    "............",
    "............",
    "............",
};

// ---- Stage 24 explosions (procedural, arcade-style) --------------------
//
// The old effect drew a filled core "blob" that read as a solid square on
// screen -- unnatural and debuggy. Both new effects are built from THIN
// RAYS and SCATTERED DOTS only (no filled squares), after the arcade
// reference screenshots:
//
//   ENEMY  = starburst: thin rays radiating from the centre, expanding and
//            breaking up over 4 frames, ending as drifting embers.
//   PLAYER = the life-lost fountain: a ragged magenta mound at the base
//            with yellow rays fanning upward, then embers drifting up.
//
// Both are 32x32 and are drawn CENTRED on the gameplay effect box (the
// blast reads bigger than the ship, like the reference).
constexpr int kExplosionSize = 32;

// Tiny deterministic generator (same LCG the old burst used) so the
// debris pattern is identical every run.
struct Lcg {
    unsigned state;
    explicit Lcg(unsigned seed) : state(seed) {}
    unsigned next()
    {
        state = state * 1664525u + 1013904223u;
        return state;
    }
    int range(int lo, int hi)
    {
        return lo + static_cast<int>(next() %
                                     static_cast<unsigned>(hi - lo + 1));
    }
};

void plot(SDL_Surface* s, float x, float y, const Color& c)
{
    setPixel(s, static_cast<int>(x + 0.5f), static_cast<int>(y + 0.5f), c);
}

// One thin ray from (cx,cy) along unit (dx,dy): 1px wide, 2px near the
// base for weight, with an optional broken gap and a contrasting tip.
void plotRay(SDL_Surface* s, float cx, float cy, float dx, float dy,
             int len, const Color& c, const Color& tip, int gapLo = 0,
             int gapHi = 0)
{
    for (int r = 1; r <= len; ++r) {
        if (gapHi > gapLo && r >= gapLo && r < gapHi) {
            continue;  // the ray breaks up as it dissipates
        }
        const float px = cx + dx * static_cast<float>(r);
        const float py = cy + dy * static_cast<float>(r);
        const Color& col = (r >= len - 1) ? tip : c;
        plot(s, px, py, col);
        if (r * 3 <= len) {
            plot(s, px, py + 1.0f, col);  // base weight
        }
    }
}

// The four ENEMY starburst frames.
void stampEnemyBurst(SDL_Surface* s, int frame)
{
    const float cx = 15.5f;
    const float cy = 15.5f;
    // 8 ray directions: cardinals first, then diagonals.
    const float dirs[8][2] = {
        {0.f, -1.f},  {1.f, 0.f},  {0.f, 1.f},  {-1.f, 0.f},
        {0.7071f, -0.7071f}, {0.7071f, 0.7071f},
        {-0.7071f, 0.7071f}, {-0.7071f, -0.7071f}};
    switch (frame) {
        case 0: {
            // Ignition: a small white star with cyan tips.
            for (int i = 0; i < 4; ++i) {
                plotRay(s, cx, cy, dirs[i][0], dirs[i][1], 5,
                        colors::kWhite, colors::kEnemyCyan);
            }
            for (int i = 4; i < 8; ++i) {
                plotRay(s, cx, cy, dirs[i][0], dirs[i][1], 3,
                        colors::kWhite, colors::kWhite);
            }
            plot(s, cx, cy, colors::kWhite);
            break;
        }
        case 1: {
            // Full burst: long cyan cardinals, white diagonals, yellow
            // mid-ray sparks, small white heart.
            for (int i = 0; i < 4; ++i) {
                plotRay(s, cx, cy, dirs[i][0], dirs[i][1], 13,
                        colors::kEnemyCyan, colors::kWhite);
                plot(s, cx + dirs[i][0] * 7.0f, cy + dirs[i][1] * 7.0f,
                     colors::kEnemyYellow);
            }
            for (int i = 4; i < 8; ++i) {
                plotRay(s, cx, cy, dirs[i][0], dirs[i][1], 9,
                        colors::kWhite, colors::kEnemyYellow);
            }
            plot(s, cx, cy, colors::kWhite);
            plot(s, cx + 1.f, cy, colors::kWhite);
            plot(s, cx - 1.f, cy, colors::kWhite);
            plot(s, cx, cy + 1.f, colors::kWhite);
            plot(s, cx, cy - 1.f, colors::kWhite);
            break;
        }
        case 2: {
            // Expansion: the rays stretch, break and turn hot.
            for (int i = 0; i < 4; ++i) {
                plotRay(s, cx, cy, dirs[i][0], dirs[i][1], 15,
                        colors::kEnemyMagenta, colors::kEnemyRed, 6, 9);
            }
            for (int i = 4; i < 8; ++i) {
                plotRay(s, cx, cy, dirs[i][0], dirs[i][1], 11,
                        colors::kEnemyYellow, colors::kEnemyOrange, 5, 7);
            }
            // First debris ring.
            Lcg lcg{1979u + 2u * 131u};
            for (int d = 0; d < 6; ++d) {
                const float a = (static_cast<float>(d) / 6.f) * 6.2831f +
                                lcg.range(0, 1) * 0.4f;
                const float r = 12.0f + lcg.range(0, 2);
                plot(s, cx + std::cos(a) * r, cy + std::sin(a) * r,
                     d % 2 == 0 ? colors::kEnemyRed : colors::kEnemyYellow);
            }
            break;
        }
        default: {
            // Embers only: the burst is gone, sparks drift outward.
            Lcg lcg{1979u + 3u * 131u};
            const Color ember[5] = {colors::kEnemyRed, colors::kEnemyOrange,
                                    colors::kStarMid, colors::kWhite,
                                    colors::kEnemyYellow};
            for (int d = 0; d < 18; ++d) {
                const float a =
                    (static_cast<float>(d) / 18.f) * 6.2831f +
                    lcg.range(0, 2) * 0.3f;
                const float r = 5.0f + lcg.range(0, 10);
                plot(s, cx + std::cos(a) * r, cy + std::sin(a) * r,
                     ember[lcg.range(0, 4)]);
            }
            break;
        }
    }
}

// The four PLAYER fountain frames (the arcade life-lost burst): a ragged
// magenta mound at the base with yellow rays fanning upward.
void stampPlayerBurst(SDL_Surface* s, int frame)
{
    const float cx = 15.5f;
    const float baseY = 25.0f;  // the burst erupts from the ship base
    // Upward fan directions (angles from straight up, in degrees).
    switch (frame) {
        case 0: {
            // Ignition flash where the ship was.
            plot(s, cx, 20.0f, colors::kWhite);
            plot(s, cx + 1.f, 20.0f, colors::kWhite);
            plot(s, cx - 1.f, 20.0f, colors::kEnemyCyan);
            plot(s, cx, 19.0f, colors::kWhite);
            plot(s, cx, 21.0f, colors::kWhite);
            plot(s, cx + 3.f, 19.0f, colors::kEnemyCyan);
            plot(s, cx - 3.f, 19.0f, colors::kEnemyCyan);
            break;
        }
        case 1:
        case 2: {
            const bool wide = (frame == 2);
            // The ragged magenta mound (red flecks mixed in).
            const int top = wide ? 22 : 23;
            for (int y = top; y <= 27; ++y) {
                const int half = (y - top) + (wide ? 3 : 2);
                for (int dx = -half; dx <= half; ++dx) {
                    const bool corner = (dx == -half || dx == half);
                    if (corner && (dx + y) % 2 == 0) {
                        continue;  // ragged edge
                    }
                    const Color c =
                        ((dx * 7 + y * 3) % 5 == 0) ? colors::kEnemyRed
                                                    : colors::kEnemyMagenta;
                    plot(s, cx + static_cast<float>(dx),
                         static_cast<float>(y), c);
                }
            }
            // Yellow rays fanning upward.
            const int angles = wide ? 9 : 7;
            for (int i = 0; i < angles; ++i) {
                const float deg = -70.0f +
                                  140.0f * static_cast<float>(i) /
                                      static_cast<float>(angles - 1);
                const float a = deg * 3.14159f / 180.0f;
                const float dx = std::sin(a);
                const float dy = -std::cos(a);
                const int len = 8 + (i % 3) * 3 + (wide ? 3 : 0);
                const int gapLo = wide ? 7 : 0;
                plotRay(s, cx, baseY - 1.0f, dx, dy, len,
                        colors::kEnemyYellow,
                        (i % 2 == 0) ? colors::kWhite : colors::kEnemyOrange,
                        gapLo, gapLo + 3);
            }
            if (wide) {
                // Colored debris thrown clear (like the reference).
                Lcg lcg{4242u};
                const Color bits[4] = {colors::kEnemyCyan, colors::kEnemyGreen,
                                       colors::kWhite, colors::kEnemyYellow};
                for (int d = 0; d < 10; ++d) {
                    const float a =
                        (static_cast<float>(lcg.range(0, 359)) / 359.f) *
                        3.14159f;  // upper half
                    const float r = 8.0f + lcg.range(0, 6);
                    plot(s, cx + std::sin(a) * r,
                         baseY - 4.0f - std::cos(a) * r, bits[d % 4]);
                }
            }
            break;
        }
        default: {
            // Embers drifting up and out; the mound gutters out.
            plot(s, cx - 2.f, 26.0f, Color{120, 20, 90});
            plot(s, cx + 2.f, 26.0f, Color{120, 20, 90});
            Lcg lcg{9753u};
            const Color ember[5] = {colors::kEnemyYellow,
                                    colors::kEnemyMagenta, colors::kEnemyRed,
                                    colors::kWhite, colors::kEnemyCyan};
            for (int d = 0; d < 14; ++d) {
                const float a =
                    (static_cast<float>(lcg.range(0, 359)) / 359.f) *
                    3.14159f;
                const float r = 5.0f + lcg.range(0, 11);
                plot(s, cx + std::sin(a) * r,
                     baseY - 2.0f - std::cos(a) * r, ember[lcg.range(0, 4)]);
            }
            break;
        }
    }
}

// ---- Stage 24b: "GALAXIAN CLONE" title logo ----------------------------
//
// A blocky 5x7 arcade pixel font composed into the full title, stamped at
// 4x scale with a vertical GREEN GRADIENT (bright at the top, darker at the
// bottom) — after the classic Galaxian title screen reference. The letters
// stay transparent-backed so they read over the black + starfield backdrop.
constexpr int kLogoScale = 4;    // each font pixel becomes a 4x4 block
constexpr int kGlyphW = 5;
constexpr int kGlyphH = 7;
constexpr int kGlyphPitch = 6;   // 5 wide + 1px spacing

// The 5x7 green gradient bands (top -> bottom).
const Color kTitleGradient[kGlyphH] = {
    {180, 255, 120},  // bright lime
    {160, 250, 115},
    {140, 245, 110},
    {120, 240, 100},
    {100, 235, 90},
    {80, 225, 80},
    {60, 210, 70},    // darker green
};

// 5x7 glyphs for the letters in "GALAXIAN CLONE". 'X' = lit pixel.
const std::map<char, std::vector<std::string>> kLogoFont = {
    {'A', {".XXX.", "X...X", "X...X", "XXXXX", "X...X", "X...X", "X...X"}},
    {'C', {".XXX.", "X...X", "X....", "X....", "X....", "X...X", ".XXX."}},
    {'E', {"XXXXX", "X....", "X....", "XXXX.", "X....", "X....", "XXXXX"}},
    {'G', {".XXX.", "X...X", "X....", "X.XXX", "X...X", "X...X", ".XXX."}},
    {'I', {"XXXXX", "..X..", "..X..", "..X..", "..X..", "..X..", "XXXXX"}},
    {'L', {"X....", "X....", "X....", "X....", "X....", "X....", "XXXXX"}},
    {'N', {"X...X", "XX..X", "X.X.X", "X..XX", "X...X", "X...X", "X...X"}},
    {'O', {".XXX.", "X...X", "X...X", "X...X", "X...X", "X...X", ".XXX."}},
    {'X', {"X...X", "X...X", ".X.X.", "..X..", ".X.X.", "X...X", "X...X"}},
    {' ', {".....", ".....", ".....", ".....", ".....", ".....", "....."}},
};

// Stamps "GALAXIAN CLONE" into `s`, which must be sized by the caller with
// stampTitleLogoWidth/Height.
void stampTitleLogo(SDL_Surface* s)
{
    const char* text = "GALAXIAN CLONE";
    const int len = static_cast<int>(std::strlen(text));
    for (int i = 0; i < len; ++i) {
        auto it = kLogoFont.find(text[i]);
        const std::vector<std::string>* glyph =
            it != kLogoFont.end() ? &it->second : nullptr;
        for (int fy = 0; fy < kGlyphH; ++fy) {
            const Color& col = kTitleGradient[fy];
            for (int fx = 0; fx < kGlyphW; ++fx) {
                const bool on =
                    glyph != nullptr && fy < static_cast<int>(glyph->size()) &&
                    fx < static_cast<int>((*glyph)[fy].size()) &&
                    (*glyph)[fy][fx] == 'X';
                if (!on) {
                    continue;
                }
                const int px = (i * kGlyphPitch + fx) * kLogoScale;
                const int py = fy * kLogoScale;
                for (int dy = 0; dy < kLogoScale; ++dy) {
                    for (int dx = 0; dx < kLogoScale; ++dx) {
                        setPixel(s, px + dx, py + dy, col);
                    }
                }
            }
        }
    }
}

int titleLogoWidth() { return kLogoScale * (14 * kGlyphPitch - 1); }  // 332
int titleLogoHeight() { return kLogoScale * kGlyphH; }                 // 28

Texture toTexture(SDL_Renderer* renderer, SDL_Surface* surface)
{
    Texture result;
    if (surface != nullptr) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture != nullptr) {
            // Pixel-perfect when drawn scaled (Renderer::drawSprite).
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
            result = Texture(texture);
        }
        SDL_FreeSurface(surface);
    } else {
        std::fprintf(stderr, "galaxian: dev art: surface creation failed: %s\n",
                     SDL_GetError());
    }
    return result;
}

}  // namespace

bool createAll(Renderer& renderer)
{
    if (!renderer.initialized()) {
        return false;
    }

    // Player: 24x16 pixel-art fighter. Frame A idles with a small pilot
    // flame; frame B lights the full thruster bar.
    {
        const SpritePalette pal{
            colors::kPlayerHull,               // C hull (white)
            Color{185, 195, 210},              // L light steel (unused)
            Color{110, 120, 140},              // X shade (unused)
            colors::kWhite,                    // W
            colors::kPlayerStripe,             // A red spine + nose
            colors::kPlayerCyan};              // B cyan wings + thruster
        SDL_Surface* s = makeSurface(24, 16);
        stampBitmap(s, kPlayerRowsA, pal);
        renderer.registerTexture(kPlayerIdleA, toTexture(renderer.sdlRenderer(), s));

        s = makeSurface(24, 16);
        stampBitmap(s, kPlayerRowsB, pal);
        renderer.registerTexture(kPlayerIdleB, toTexture(renderer.sdlRenderer(), s));

        // Compatibility id: same art as frame A (Game's texture check and
        // the rendering tests resolve through it).
        s = makeSurface(24, 16);
        stampBitmap(s, kPlayerRowsA, pal);
        renderer.registerTexture(kPlayer, toTexture(renderer.sdlRenderer(), s));
    }

    // Enemies: 24x24 pixel-art aliens, two frames each (wing flap / claw
    // pinch / dome glint). The bare kEnemy* ids resolve to the SAME art as
    // frame A so every consumer draws the production sprites.
    const struct {
        const char* legacyId;
        const char* idA;
        const char* idB;
        Color body;      // C
        Color light;     // L (wings)
        Color shade;     // X (dark accent)
        Color accentA;   // A (eyes / spine / dome)
        Color accentB;   // B (antennae / claws / glints)
        const std::vector<std::string>& rowsA;
        const std::vector<std::string>& rowsB;
    } enemyArt[3] = {
        {kEnemyScout,     kEnemyScoutA,     kEnemyScoutB,
         colors::kEnemyCyan,    colors::kEnemyBlue,
         colors::kEnemyNavy,    colors::kEnemyRed,
         colors::kEnemyMagenta,
         kScoutRowsA,          kScoutRowsB},
        {kEnemyGuard,     kEnemyGuardA,     kEnemyGuardB,
         colors::kEnemyRed,     colors::kEnemyBlue,
         Color{90, 15, 15},     colors::kEnemyYellow,
         colors::kEnemyOrange,
         kGuardRowsA,          kGuardRowsB},
        {kEnemyCommander, kEnemyCommanderA, kEnemyCommanderB,
         colors::kEnemyYellow,  colors::kEnemyBlue,
         colors::kPlayerStripe, colors::kEnemyOrange,
         colors::kEnemyCyan,
         kCommanderRowsA,      kCommanderRowsB},
    };
    for (const auto& e : enemyArt) {
        const SpritePalette pal{e.body, e.light, e.shade, colors::kWhite,
                                e.accentA, e.accentB};
        SDL_Surface* s = makeSurface(24, 24);
        stampBitmap(s, e.rowsA, pal);
        renderer.registerTexture(e.idA, toTexture(renderer.sdlRenderer(), s));

        s = makeSurface(24, 24);
        stampBitmap(s, e.rowsB, pal);
        renderer.registerTexture(e.idB, toTexture(renderer.sdlRenderer(), s));

        s = makeSurface(24, 24);
        stampBitmap(s, e.rowsA, pal);
        renderer.registerTexture(e.legacyId,
                                 toTexture(renderer.sdlRenderer(), s));
    }

    // Bullet: 4x10 rectangle.
    {
        SDL_Surface* s = makeSurface(4, 10);
        fillSquare(s, colors::kBullet, colors::kBullet);
        renderer.registerTexture(kBullet, toTexture(renderer.sdlRenderer(), s));
    }

    // Title logo: "GALAXIAN CLONE" green-gradient pixel art.
    {
        SDL_Surface* s = makeSurface(titleLogoWidth(), titleLogoHeight());
        stampTitleLogo(s);
        renderer.registerTexture(kTitleLogo, toTexture(renderer.sdlRenderer(), s));
    }

    // Explosions: 32x32 per frame, drawn CENTRED on the gameplay effect
    // box (the blast reads bigger than the ship, like the arcade
    // reference). Four ENEMY starburst frames + four PLAYER fountain
    // frames; both total exactly the Stage 9 effect duration (4 frames).
    {
        const char* const enemyIds[4] = {kExplosionA, kExplosionB,
                                         kExplosionC, kExplosionD};
        for (int f = 0; f < 4; ++f) {
            SDL_Surface* s = makeSurface(kExplosionSize, kExplosionSize);
            stampEnemyBurst(s, f);
            renderer.registerTexture(enemyIds[f],
                                     toTexture(renderer.sdlRenderer(), s));
        }
        const char* const playerIds[4] = {kPlayerExplosionA,
                                          kPlayerExplosionB,
                                          kPlayerExplosionC,
                                          kPlayerExplosionD};
        for (int f = 0; f < 4; ++f) {
            SDL_Surface* s = makeSurface(kExplosionSize, kExplosionSize);
            stampPlayerBurst(s, f);
            renderer.registerTexture(playerIds[f],
                                     toTexture(renderer.sdlRenderer(), s));
        }
    }

    return true;
}

}  // namespace DevArt
}  // namespace galaxian
