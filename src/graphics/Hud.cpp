#include "Hud.hpp"

#include <cstdio>

#include "core/Constants.hpp"
#include "gameplay/Player.hpp"  // kLives: the pip cap

namespace galaxian {
namespace hud {

namespace {

// One life pip: a tiny two-rect triangle (font-free), 10x8 logical px.
constexpr float kPipWidth = 10.0f;
constexpr float kPipSpacing = 14.0f;
constexpr float kPipBaseX = 16.0f;
constexpr float kPipY = 538.0f;  // bottom-left, clear of the gameplay

}  // namespace

void drawTopBar(Renderer& renderer, const Values& v)
{
    char line[32];

    // Labels.
    renderer.drawText("SCORE", {16.0f, 8.0f}, colors::kGreen);
    renderer.drawText("HIGH", {352.0f, 8.0f}, colors::kGreen);
    // Zero-padded values (spec §11 example: 012350 / 042000).
    std::snprintf(line, sizeof(line), "%06d", v.score);
    renderer.drawText(line, {16.0f, 24.0f}, colors::kWhite);
    std::snprintf(line, sizeof(line), "%06d", v.highScore);
    renderer.drawText(line, {352.0f, 24.0f}, colors::kWhite);
    // The wave line sits under the top bar's left column.
    std::snprintf(line, sizeof(line), "WAVE %d", v.wave);
    renderer.drawText(line, {16.0f, 44.0f}, colors::kWhite);
}

void drawLivesPips(Renderer& renderer, int lives)
{
    if (lives < 0) {
        lives = 0;
    }
    if (lives > Player::kLives) {  // never more pips than possible ships
        lives = Player::kLives;
    }
    for (int i = 0; i < lives; ++i) {
        const float x = kPipBaseX + static_cast<float>(i) * kPipSpacing;
        // Top tip + base bar -> reads as a little ship triangle.
        renderer.drawFilledRect({x + 3.0f, kPipY, 4.0f, 4.0f},
                                colors::kPlayerCyan);
        renderer.drawFilledRect({x, kPipY + 4.0f, kPipWidth, 4.0f},
                                colors::kPlayerCyan);
    }
}

}  // namespace hud
}  // namespace galaxian
