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

    // The controls help line. Everything else that used to live here
    // (Stage 3 static bullets, Stage 4 action table, per-stage titles) was
    // scaffolding and is gone: the top bar is the real HUD (Stage 18,
    // graphics/Hud), the enemies/player/bullets are the gameplay objects.
    renderer.drawText(
        "LEFT/RIGHT=MOVE  SPACE=FIRE  F1=BOXES  F2=STATS+LABELS  F3=DIVE  ESC=PAUSE",
        {16.0f, 552.0f}, colors::kWhite);
}

}  // namespace galaxian
