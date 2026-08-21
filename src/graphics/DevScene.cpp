#include "DevScene.hpp"

#include "DevArt.hpp"
#include "core/Constants.hpp"

namespace galaxian {

bool DevScene::initialize(Renderer& renderer)
{
    return DevArt::createAll(renderer);
}

void DevScene::draw(Renderer& renderer) const
{
    // Background + screen border.
    renderer.clear(colors::kBlack);
    renderer.drawRect(
        {0.0f, 0.0f, static_cast<float>(kLogicalWidth),
         static_cast<float>(kLogicalHeight)},
        colors::kBorder);

    // Projectile rectangles (Stage 3 static test shapes).
    renderer.drawFilledRect({100.0f, 400.0f, 4.0f, 10.0f}, colors::kBullet);
    renderer.drawFilledRect({222.0f, 300.0f, 4.0f, 10.0f}, colors::kBullet);
    renderer.drawFilledRect({340.0f, 450.0f, 4.0f, 10.0f}, colors::kBullet);

    // Text. (The enemy sprites are the real gameplay formation, drawn by
    // Game since Stage 8; the Stage 4 action table was removed in Stage 8.)
    renderer.drawText("GALAXIAN CLONE - STAGE 8 ENEMY FORMATION", {16.0f, 16.0f},
                      colors::kWhite);
    renderer.drawText("PLAYER=TRIANGLE  ENEMIES=SQUARES", {16.0f, 40.0f},
                      colors::kWhite);
    renderer.drawText(
        "LEFT/RIGHT=MOVE  SPACE=FIRE  F1=BOXES  F2=STATS  ESC=QUIT",
        {16.0f, 552.0f}, colors::kWhite);
}

}  // namespace galaxian
